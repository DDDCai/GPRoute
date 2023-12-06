/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-26 02:42:18
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-10-18 09:44:03
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include "dis_dedup.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include "message.h"
#include "glib.h"
#include "communicate.h"
#include <unistd.h>
#include "feature.h"
#include "dedup.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include "tools.h"
#include <errno.h>
#include <pthread.h>
#include "concurrent_hashtable.h"
#include <sys/time.h>

boolean alive_flag;
GTimer *total_timer;
struct counter {
    uint64_t num;
    GRWLock lock;
} logic_block, route_connec, fp_connec, route_volume, segment_volume, re_route_volume, re_segment_volume, re_gp;
struct f_counter {
    double num;
    GRWLock lock;
} fea_pro_time, fp_pro_time, fea_upd_time;
cHashTable *fea_table, *fp_table;

#ifdef DEBUG_MOTI
uint64_t total_fea, hit_fea;
gpointer con_backup[FEATURE_NUM*2], con_tmp[FEATURE_NUM];
int backup_pos;
struct lru_item {gpointer data; gpointer next, pre;} con_lru[FEATURE_NUM];
struct lru_item * lru_head, *lru_tail;
#endif

#if(ROUTE_METHOD==GUIDEPOST)

struct htable_queue {
    cHashTable *table;
    GQueue *queue;
    GRWLock lock;
} open_fea_con;

static void init_htable_queue(struct htable_queue *htq, GHashFunc hash_func, GEqualFunc key_equal_func, GDestroyNotify key_destroy_func, GDestroyNotify  value_destroy_func)
{
    htq->table = c_hash_table_new(hash_func, key_equal_func, key_destroy_func, value_destroy_func, 16);
    htq->queue = g_queue_new();
    g_rw_lock_init(&htq->lock);
}

static void free_htable_queue(struct htable_queue *htq)
{
    c_hash_table_destroy(htq->table);
    g_queue_free(htq->queue);
    g_rw_lock_clear(&htq->lock);
}

void handle_eof(message_eof *message, int fd)
{
    fea_container *con; 
    gint64 key = (gint64)(fd*16), *ori_key;
    gboolean exist = c_hash_table_lookup_extended(open_fea_con.table, &key, (gpointer*)&ori_key, (gpointer*)&con);
    if(exist)
    {
        c_hash_table_remove(open_fea_con.table, &key);
        _writelock_execution(&open_fea_con.lock,
            g_queue_push_tail(open_fea_con.queue, con);
        );
        free(ori_key);
    }
}

#endif

void fill_re_shutdown_msg(message_re_shutdown *msg)
{
    msg->type = M_RE_SHUTDOWN_TYPE;
    msg->phy_block = c_hash_table_size(fp_table);
    msg->logic_block = logic_block.num;
    msg->route_connec = route_connec.num;
    msg->route_volume = route_volume.num;
    msg->re_route_volume = re_route_volume.num;
    msg->fp_connec = fp_connec.num;
    msg->segment_volume = segment_volume.num;
    msg->re_segment_volume = re_segment_volume.num;
    msg->fea_tab_size = c_hash_table_size(fea_table);
    msg->re_gp = re_gp.num;
  _writelock_execution(&route_connec.lock,
    if(total_timer)
    {
        msg->total_time = g_timer_elapsed(total_timer, NULL);
        g_timer_destroy(total_timer);
        total_timer = NULL;
    }
    else msg->total_time = 0;
  );
    msg->fea_pro_time = fea_pro_time.num;
    msg->fp_pro_time = fp_pro_time.num;
    msg->fea_upd_time = fea_upd_time.num;
    #ifdef DEBUG_MOTI
    msg->total_fea = total_fea;
    msg->hit_fea = hit_fea;
    for(int i=0; i<FEATURE_NUM; i++){
        con_backup[i] = NULL;
        con_tmp[i] = NULL;
    }
    for(int i=FEATURE_NUM; i<FEATURE_NUM*2; i++) {
        con_backup[i] = NULL;
    }
    backup_pos = 0;
    memset(con_lru, 0, sizeof(con_lru));
    for(int i=1; i<FEATURE_NUM; i++) {
        con_lru[i].next = &con_lru[(i+1)%FEATURE_NUM];
        con_lru[i].pre = &con_lru[(i-1)];
    }
    con_lru[0].next = &con_lru[1];
    con_lru[0].pre = &con_lru[FEATURE_NUM-1];
    lru_head = &con_lru[0];
    lru_tail = &con_lru[FEATURE_NUM-1];
    #endif
}

static void reset_oneshot_fd_in_epoll(int connect_fd, int epoll_fd)
{
    struct epoll_event ev;
    ev.data.fd = connect_fd;
    ev.events = EPOLLIN|EPOLLONESHOT;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, connect_fd, &ev);
}

static void add_oneshot_fd_to_epoll(int connect_fd, int epoll_fd)
{
    struct epoll_event ev;
    fcntl(connect_fd, F_SETFL, fcntl(connect_fd, F_GETFL)|O_NONBLOCK);
    ev.events = EPOLLIN|EPOLLONESHOT;
    ev.data.fd = connect_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connect_fd, &ev) == -1)
    {
        printf("Fail to add epoll.\n");
        exit(1);
    }
}

void message_handler(gpointer data, gpointer user_data)
{
    struct epoll_event *ev = (struct epoll_event*)data;
    int connect_fd = ev->data.fd;
    int epoll_fd = (int)(gint64)user_data;
    uint8_t recv_buf[MAX_MESSAGE_SIZE+MAX_TCP_MESSAGE_SIZE], send_buf[MAX_MESSAGE_SIZE];
    message_resend succ = {M_RESEND_TYPE, FALSE}, fail = {M_RESEND_TYPE, TRUE};

    int ret, re_size;
    GTimer *timer = g_timer_new();
    while((ret = receive_message(connect_fd, recv_buf))==1)
    {
        switch (MESSAGE_TYPE(recv_buf))
        {
            case M_FEATURE_TYPE:
                g_timer_start(timer);
                re_size = feature_process((message_feature*)recv_buf, send_buf);
                _writelock_execution(&fea_pro_time.lock, fea_pro_time.num += g_timer_elapsed(timer, NULL));
                send_message(connect_fd, send_buf, re_size);
                _writelock_execution(&route_connec.lock, route_connec.num += 1);
                _writelock_execution(&route_volume.lock, route_volume.num += sizeof(message_feature));
                _writelock_execution(&re_route_volume.lock, re_route_volume.num += re_size);
                break;

            case M_SEGMENT_TYPE:
                g_timer_start(timer);
                re_size = dedup_process((message_segment*)recv_buf, send_buf);
                _writelock_execution(&fp_pro_time.lock, fp_pro_time.num += g_timer_elapsed(timer, NULL));
                send_message(connect_fd, send_buf, re_size);
                g_timer_start(timer);
                update_feature_index((message_segment*)recv_buf, connect_fd);
                _writelock_execution(&fea_upd_time.lock, fea_upd_time.num += g_timer_elapsed(timer, NULL));
                _writelock_execution(&logic_block.lock, logic_block.num += ((message_segment*)recv_buf)->seg_size);
                _writelock_execution(&fp_connec.lock, fp_connec.num += 1);
                _writelock_execution(&segment_volume.lock, segment_volume.num += sizeof(message_segment)-sizeof_xfp(MAX_SEGMENT_SIZE)+sizeof_xfp(((message_segment*)recv_buf)->seg_size));
                _writelock_execution(&re_segment_volume.lock, re_segment_volume.num += re_size);
                break;

            case M_SHUTDOWN_TYPE:
                if(alive_flag)
                {
                    printf("Shutdown (Request)\n");
                    alive_flag = FALSE;
                    fill_re_shutdown_msg((message_re_shutdown*)send_buf);
                    send_message(connect_fd, send_buf, sizeof(message_re_shutdown));
                }
                break;
            
            #if(ROUTE_METHOD==GUIDEPOST)
            case M_EOF_TYPE:
                handle_eof((message_eof*)recv_buf, connect_fd);
                break;
            #endif
            
            default:
                printf("Unknown message type: %d\n", MESSAGE_TYPE(recv_buf));
                break;
        }
    }
    g_timer_destroy(timer);
    if(ret == -1) reset_oneshot_fd_in_epoll(connect_fd, epoll_fd);
    else epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connect_fd, NULL);
    free(data);
}

static void init_local_v(char *ip, int *socket_fd, int *epoll_fd)
{
    struct sockaddr_in server_addr;
    if(!load_server_addr(ip, &server_addr)) exit(1);
    if(!init_communication(&server_addr, socket_fd, epoll_fd)) exit(1);
}

static void init_counter(struct counter *c)
{
    c->num = 0;
    g_rw_lock_init(&c->lock);
}

static void free_counter(struct counter *c)
{
    g_rw_lock_clear(&c->lock);
}

static void init_f_counter(struct f_counter *c)
{
    c->num = 0;
    g_rw_lock_init(&c->lock);
}

static void free_f_counter(struct f_counter *c)
{
    g_rw_lock_clear(&c->lock);
}

static void init_global_v()
{
    total_timer = g_timer_new(); g_timer_start(total_timer);
    alive_flag = TRUE;
  #if(ROUTE_METHOD==GUIDEPOST)
    init_htable_queue(&open_fea_con, my_int64_hash, my_int64_equal, NULL, NULL);
    fea_table = c_hash_table_new(my_fplen_hash, my_fplen_equal, NULL, NULL, 256);
  #else
    fea_table = c_hash_table_new(my_fplen_hash, my_fplen_equal, g_free, NULL, 256);
  #endif
    fp_table = c_hash_table_new(my_fplen_hash, my_fplen_equal, g_free, NULL, 256);
    init_counter(&logic_block);
    init_counter(&route_connec); init_counter(&fp_connec);
    init_counter(&route_volume); init_counter(&segment_volume);
    init_counter(&re_route_volume); init_counter(&re_segment_volume);
    init_f_counter(&fea_pro_time); init_f_counter(&fp_pro_time);
    init_f_counter(&fea_upd_time);
    init_counter(&re_gp);
    #ifdef DEBUG_MOTI
    total_fea = 0;
    hit_fea = 0;
    for(int i=0; i<FEATURE_NUM; i++) {
        con_backup[i] = NULL;
        con_tmp[i] = NULL;
    }
    #endif
}

static void free_global_v()
{
  #if(ROUTE_METHOD==GUIDEPOST)
    free_htable_queue(&open_fea_con);
  #endif
    c_hash_table_destroy(fea_table);
    c_hash_table_destroy(fp_table);
    free_counter(&logic_block);
    free_counter(&route_connec); free_counter(&fp_connec);
    free_counter(&route_volume); free_counter(&segment_volume);
    free_counter(&re_route_volume); free_counter(&re_segment_volume);
    free_f_counter(&fea_pro_time); free_f_counter(&fp_pro_time);
    free_f_counter(&fea_upd_time);
    free_counter(&re_gp);
}

void main(int argc, char *argv[])
{
    int socket_fd, epoll_fd;
    init_local_v(argv[1], &socket_fd, &epoll_fd);
    init_global_v();
    GThreadPool *server_pool = g_thread_pool_new(message_handler, (gpointer)(gint64)epoll_fd, MAX_THREAD_POOL_SIZE, TRUE, NULL);

    int nfds, connec_fd;
    struct epoll_event events[MAX_THREAD_POOL_SIZE];
    while(alive_flag)
    {
        nfds = epoll_wait(epoll_fd, events, MAX_THREAD_POOL_SIZE, -1);
        for(int i=0; i<nfds; i++)
        {
            if(events[i].data.fd == socket_fd)
            {
                if((connec_fd = accept(socket_fd, NULL, NULL)) == -1)
                {
                    printf("Fail to accept connection.\n");
                    exit(1);
                }
                add_oneshot_fd_to_epoll(connec_fd, epoll_fd);
            }
            else
            {
                void *arg = malloc(sizeof(struct epoll_event));
                memcpy(arg, &events[i], sizeof(struct epoll_event));
                g_thread_pool_push(server_pool, arg, NULL);
            }
        }
    }

    g_thread_pool_free(server_pool, FALSE, TRUE);
    free_global_v();
    close(socket_fd); close(epoll_fd);

    printf("Server closed (%s)\n", argv[1]);
}