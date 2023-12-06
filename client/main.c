/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-11 04:15:59
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-10-20 09:12:55
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include "client.h"
#include <stdio.h>
#include <netinet/in.h>
#include "dis_dedup.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <unistd.h>
#include "glib.h"
#include "communicate.h"
#include <sys/epoll.h>
#include "tools.h"
#include <errno.h>

struct sockaddr_in server_addr[NODE_NUM];
int fea_fd[NODE_NUM], seg_fd[NODE_NUM], data_fd[NODE_NUM], epoll_fd;
double msg_total_time, fea_total_time, seg_total_time;
double total_fea_recv_time, average_fea_recv_time;
GTimer *total_fea_recv_timer, *average_fea_recv_timer;
uint64_t gp_fea_hit_num, lc_fea_hit_num, force_fea_hit_num;
uint64_t lc_hit_times, gp_hit_times;
uint64_t hit_part_num, first_part_hit_num, second_part_hit_num;

#ifdef DEBUG_MOTI
uint64_t hit_fea, total_fea;
#endif

#ifdef PREDICTION_EVA
uint64_t gp_tp, gp_tn, gp_fp, gp_fn, ss_tp, ss_tn, ss_fn, ss_fp;
#endif

#if(SSDEDUP==ON)
struct {
    GList *list;
    GHashTable *table;
    uint64_t storage, size;
} lcache[NODE_NUM];

void init_lcache()
{
    memset(lcache, 0, sizeof(lcache));
    for(int i=0; i<NODE_NUM; i++) lcache[i].table = g_hash_table_new_full(my_fplen_hash, my_fplen_equal, free, NULL);
}

void free_lcache()
{
    for(int i=0; i<NODE_NUM; i++) {
        g_list_free(lcache[i].list);
        g_hash_table_destroy(lcache[i].table);
    }
}
#endif

#if(ROUTE_METHOD==GUIDEPOST)

struct guidepost {
    gp_buf part[GP_PART];
    GHashTable *table[GP_PART];
    int hit_pos[FEATURE_NUM], hit_map[FEATURE_NUM];
    uint16_t hit;
} gp[NODE_NUM];
uint64_t storage[NODE_NUM];

static void send_eof()
{
    message_eof m;
    m.type = M_EOF_TYPE;
    for(int i=0; i<NODE_NUM; i++) send_message(fea_fd[i], &m, sizeof(message_eof));
}

static void init_gp_v()
{
    memset(gp, 0, sizeof(gp));
    memset(storage, 0, sizeof(storage));
    for(int i=0; i<NODE_NUM; i++) {
        for(int j=0; j<GP_PART; j++)
            gp[i].table[j] = g_hash_table_new_full(my_fplen_hash, my_fplen_equal, NULL, NULL);
    }
}

static void free_gp_v()
{
    for(int i=0; i<NODE_NUM; i++) {
        for(int j=0; j<GP_PART; j++)
            g_hash_table_destroy(gp[i].table[j]);
    }
}

#endif  //  #if(ROUTE_METHOD==GUIDEPOST)

static void init_connection(int x_fd[])
{
    struct epoll_event ev;
    for(int i=0; i<NODE_NUM; i++)
    {
        x_fd[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        int opt = 1;
        setsockopt(x_fd[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        int connect_times = 0;
        while(connect(x_fd[i], (struct sockaddr*)&server_addr[i], sizeof(struct sockaddr_in)) < 0)
        {
            connect_times ++;
            if(connect_times == 10)
            {
                printf("Skip to connect %u:%u due to %d\n", server_addr[i].sin_addr.s_addr, ntohs(server_addr[i].sin_port), errno);
                exit(1);
            }
        }
        ev.events = EPOLLIN;
        ev.data.fd = i;
        if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, x_fd[i], &ev) == -1)
        {
            printf("Fail to add epoll.\n");
            exit(1);
        }
        g_usleep(1000000);   //  Too frequent connections will overflow "some" queue and make some connections lost.
    }
}

static void init_global_v(char *ip_file)
{
    load_server_addr(ip_file, server_addr, NODE_NUM);
    epoll_fd = epoll_create1(0);
    init_connection(fea_fd);
    init_connection(seg_fd);
    init_connection(data_fd);
  #if(ROUTE_METHOD==GUIDEPOST)
    init_gp_v();
  #endif
    msg_total_time = 0;
    fea_total_time = 0;
    seg_total_time = 0;
    total_fea_recv_time = 0;
    average_fea_recv_time = 0;
    total_fea_recv_timer = g_timer_new();
    average_fea_recv_timer = g_timer_new();
  #if(SSDEDUP==ON)
    init_lcache();
  #endif
    gp_fea_hit_num = 0; 
    lc_fea_hit_num = 0; 
    force_fea_hit_num = 0;
    lc_hit_times = 0; 
    gp_hit_times = 0;
    hit_part_num = 0; 
    first_part_hit_num = 0; 
    second_part_hit_num = 0;
    #ifdef DEBUG_MOTI
    total_fea = 0;
    hit_fea = 0;
    #endif
    #ifdef PREDICTION_EVA
    gp_tp = 0; gp_tn = 0; gp_fp = 0; gp_fn = 0;
    ss_tp = 0; ss_tn = 0; ss_fp = 0; ss_fn = 0;
    #endif
}

static void destroy_connection(int x_fd[])
{
    for(int i=0; i<NODE_NUM; i++) close(x_fd[i]);
}

static void free_global_v()
{
    g_timer_destroy(total_fea_recv_timer);
    g_timer_destroy(average_fea_recv_timer);
  #if(ROUTE_METHOD==GUIDEPOST)
    free_gp_v();
  #endif
    destroy_connection(fea_fd);
    destroy_connection(seg_fd);
    destroy_connection(data_fd);
    close(epoll_fd);
  #if(SSDEDUP==ON)
    free_lcache();
  #endif
}

void main(int argc, char *argv[])
{
    GTimer *timer = g_timer_new();
    g_timer_start(timer);
   
    init_global_v(argv[1]);

    DIR *backup_folder = opendir(argv[2]);
    if(!backup_folder)
    {
        printf("Fail to open folder: %s\n", argv[2]);
        exit(1);
    }
    struct dirent *backup_entry;
    char backup_path[MAX_FILE_NAME_LEN];
    while(backup_entry = readdir(backup_folder))
    {
        if(!strcmp(backup_entry->d_name,".") || !strcmp(backup_entry->d_name,"..")) continue;
        PUT_3_STRS_TOGETHER(backup_path, argv[2], "/", backup_entry->d_name);
        if(!client_dedup_a_file(backup_path, server_addr)) exit(1);
    }
    closedir(backup_folder);

    #if(ROUTE_METHOD==GUIDEPOST)
    send_eof();
    #endif

    free_global_v();

    double total_time = g_timer_elapsed(timer, NULL);
    g_timer_destroy(timer);

    FILE *res_fp = fopen(argv[3], "ab");
    fprintf(res_fp, "=================================\n");
    fprintf(res_fp, "%.2f min = %.2f msg + %.2f fea + %.2f seg\n", total_time/60, msg_total_time/60, fea_total_time/60, seg_total_time/60);
    fprintf(res_fp, "client_total_fea_recv_time:    %.2f min\n", total_fea_recv_time/60);
    fprintf(res_fp, "client_average_fea_recv_time:  %.2f min\n", average_fea_recv_time/60/NODE_NUM);
    fprintf(res_fp, "\
        fea_hit_num_of_force: %lu\n", force_fea_hit_num);
    #if(SSDEDUP==ON)
    fprintf(res_fp, 
        "fea_hit_num_with_lcache: %lu\n\
            fea_hit_times_on_lcache: %lu\n\
            average_fea_hit_on_lcache: %.2f\n", lc_fea_hit_num, lc_hit_times, lc_fea_hit_num/(double)lc_hit_times);
    #endif
    #if(ROUTE_METHOD==GUIDEPOST)
    fprintf(res_fp, "\
        fea_hit_num_with_gp: %lu\n\
            fea_hit_times_on_gp: %lu\n\
            average_fea_hit_on_gp: %.2f\n", 
        gp_fea_hit_num, gp_hit_times, gp_fea_hit_num/(double)gp_hit_times
    );
    fprintf(res_fp,"\
        gp_part_hit_num: %lu\n\
            average_hit_part: %.2f\n", 
        hit_part_num, hit_part_num/(double)gp_hit_times
    );
    fprintf(res_fp,"\
        first_part_hit_num: %lu\n\
            first_part_hit_rate: %.2f\n\
        second_part_hit_num: %lu\n\
            second_part_hit_rate: %.2f\n", 
        first_part_hit_num, (double)first_part_hit_num/gp_fea_hit_num*100.0, second_part_hit_num, (double)second_part_hit_num/gp_fea_hit_num*100.0
    );
    #endif
    #ifdef DEBUG_MOTI
    fprintf(res_fp, "\
        total fea: %lu\n\
        hit fea: %lu\n\
            hit rate: %.2f\n", 
        total_fea, hit_fea, hit_fea/(double)total_fea*100
    );
    #endif
    #ifdef PREDICTION_EVA
    #if(ROUTE_METHOD==GUIDEPOST)
    fprintf(res_fp, "\
        GP_TP: %lu\n\
        GP_FP: %lu\n\
        GP_TN: %lu\n\
        GP_FN: %lu\n\
            GP_Acc: %.2f\n\ 
            GP_Pre: %.2f\n\
            GP_Rec: %.2f\n\
            GP_Pos: %.2f\n",
        gp_tp, gp_fp, gp_tn, gp_fn, (gp_tp+gp_tn)/(double)(gp_tp+gp_tn+gp_fp+gp_fn),gp_tp/(double)(gp_tp+gp_fp), gp_tp/(double)(gp_tp+gp_fn), (gp_tp+gp_fp)/(double)(gp_tp+gp_fp+gp_tn+gp_fn)
    );
    #endif
    #if(SSDEDUP==ON)
    fprintf(res_fp, "\
        SS_TP: %lu\n\
        SS_FP: %lu\n\
        SS_TN: %lu\n\
        SS_FN: %lu\n\
            SS_Acc: %.2f\n\ 
            SS_Pre: %.2f\n\
            SS_Rec: %.2f\n\
            SS_Pos: %.2f\n",
        ss_tp, ss_fp, ss_tn, ss_fn, (ss_tp+ss_tn)/(double)(ss_tp+ss_tn+ss_fp+ss_fn),ss_tp/(double)(ss_tp+ss_fp), ss_tp/(double)(ss_tp+ss_fn), (ss_tp+ss_fp)/(double)(ss_tp+ss_fp+ss_tn+ss_fn)
    );
    #endif
    #endif
    fprintf(res_fp, "=================================\n");
    fclose(res_fp);
}