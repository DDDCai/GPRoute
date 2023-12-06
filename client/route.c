/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-24 08:57:12
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-10-19 23:59:35
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include <netinet/in.h>
#include "message.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "communicate.h"
#include "glib.h"
#include <sys/epoll.h>
#include "tools.h"

#ifdef DEBUG_MOTI
extern uint64_t hit_fea, total_fea;
#endif

#ifdef PREDICTION_EVA
extern uint64_t gp_tp, gp_tn, gp_fp, gp_fn, ss_tp, ss_tn, ss_fn, ss_fp;
#endif

extern int epoll_fd;
extern int fea_fd[NODE_NUM];
extern double total_fea_recv_time, average_fea_recv_time;
extern GTimer *total_fea_recv_timer, *average_fea_recv_timer;
extern uint64_t gp_fea_hit_num, lc_fea_hit_num, force_fea_hit_num;
extern uint64_t lc_hit_times, gp_hit_times;
extern uint64_t hit_part_num, first_part_hit_num, second_part_hit_num;

static int find_all_matched(uint16_t score[], int item_num, int max_hit)
{
    int tmp, id = rand() % item_num;
    for(int i=id,j=0; j<item_num; i++,j++)
    {
        tmp = i%item_num;
        if(score[tmp]==max_hit) return tmp;
    }
    return -1;
}

static int find_load_weighted(uint16_t score[], uint64_t weight[], int item_num)
{
    uint64_t total_weight = 0;
    double load[NODE_NUM], weighted_score[NODE_NUM];
    for(int i=0; i<item_num; i++) total_weight += weight[i];
    for(int i=0; i<item_num; i++)
    {
        load[i] = weight[i]/(total_weight/(double)item_num);
        weighted_score[i] = (load[i]>1)?(score[i]/load[i]):score[i];
    }
    int tmp, id = rand()%item_num; 
    double best = 0;
    for(int i=id,j=0; j<item_num; i++,j++)
    {
        tmp = i%item_num;
        if((weighted_score[tmp] > best) && (load[i] < SKEW_THRES))
        {
            best = weighted_score[tmp];
            id = tmp;
        }
    }
    if(best > 0) return id;
    else return -1;
}

static int find_min_loaded(uint64_t weight[], int item_num)
{
    int tmp, id = rand()%item_num; 
    uint64_t best = weight[id];
    for(int i=id+1,j=1; j<item_num; i++,j++)
    {
        tmp = i%item_num;
        if(weight[tmp] < best)
        {
            best = weight[tmp];
            id = tmp;
        }
    }
    return id;
}

/**
 * @description: Get the best id. It has the biggest (score/weight) value, or its score is equal to max_hit.
 * @param {uint16_t} score[]
 * @param {uint64_t} weight[]
 * @param {int} item_num
 * @param {int} max_hit
 * @return {int} the best id
 */
static int find_weighted_best(uint16_t score[], uint64_t weight[], int item_num, int max_hit)
{
    // decision_time ++;
    int id;
    if((id = find_all_matched(score, item_num, max_hit)) >= 0) return id;
    if((id = find_load_weighted(score, weight, item_num)) >= 0) return id;
    // decision_time --;
    return find_min_loaded(weight, item_num); 
}

#if(SSDEDUP==ON)
extern struct {
    GList *list;
    GHashTable *table;
    uint64_t storage, size;
} lcache[NODE_NUM];

int use_lcache(message_feature *feature
    #ifdef DEBUG_MOTI
    , int force_target
    #endif
)
{
    uint16_t hit[NODE_NUM] = {0};
    uint64_t storage[NODE_NUM];
    for(int i=0; i<NODE_NUM; i++)
    {
        storage[i] = lcache[i].storage;
        for(int j=0; j<feature->fea_num; j++)
        {
            if(g_hash_table_contains(lcache[i].table, feature->fea[j])) hit[i]++;
        }
    }
    #ifdef DEBUG_MOTI
    hit_fea += hit[force_target];
    #endif
    boolean all_lower = TRUE;
    for(int i=0; i<NODE_NUM; i++)
    {
        if(hit[i] > LC_HIT_THRES)
        {
            all_lower = FALSE;
            break;
        }
    }
    if(all_lower) return -1;
    int target = find_weighted_best(hit, storage, NODE_NUM, feature->fea_num);
    lcache[target].storage += EXP_SEGMENT_SIZE*(feature->fea_num-hit[target])/FEATURE_NUM;
    lc_fea_hit_num += hit[target];
    return target;
}

void update_lcache(int target, message_feature *feature)
{
    for(int i=0; i<feature->fea_num; i++)
    {
        gpointer value;
        if(value = g_hash_table_lookup(lcache[target].table, feature->fea[i]))
        {
            lcache[target].list = g_list_remove(lcache[target].list, value);
            lcache[target].list = g_list_append(lcache[target].list, value);
        }
        else {
            gpointer data = malloc(FP_LEN);
            fpcpy(data, feature->fea[i], 1);
            g_hash_table_insert(lcache[target].table, data, data);
            if(lcache[target].size == LCACHE_SIZE)
            {
                g_hash_table_remove(lcache[target].table, g_list_nth_data(lcache[target].list, 0));
                lcache[target].list = g_list_remove_link(lcache[target].list, g_list_nth(lcache[target].list, 0));
            }
            else lcache[target].size ++;
            lcache[target].list = g_list_append(lcache[target].list, data);
        }
    }
}
#endif  //  #if(SSDEDUP==ON)

#if(ROUTE_METHOD==GUIDEPOST)

extern struct guidepost {
    gp_buf part[GP_PART];
    GHashTable *table[GP_PART];
    int hit_pos[FEATURE_NUM], hit_map[FEATURE_NUM];
    uint16_t hit;
} gp[NODE_NUM];
extern uint64_t storage[NODE_NUM];

static void gp_part_statistic(struct guidepost *p)
{
    int hit[GP_PART] = {0};
    for(int i=0; i<p->hit; i++)
    {
        hit[p->hit_pos[i]] ++;
    }
    int hit_part = 0;
    for(int i=0; i<GP_PART; i++)
    {
        if(hit[i]) hit_part ++;
    }
    int first = hit[0], second = 0;
    for(int i=1; i<GP_PART; i++)
    {
        if(hit[i] > first)
        {
            second = first;
            first = hit[i];
        }
        else if(hit[i] > second)
        {
            second = hit[i];
        }
    }
    hit_part_num += hit_part;
    first_part_hit_num += first;
    second_part_hit_num += second;
}

static uint16_t match_in_gp(struct guidepost *p, message_feature *feature)
{
    p->hit = 0;
    for(int i=0; i<feature->fea_num; i++)
    {
        #ifndef DEBUG_MOTI
        for(int j=0; j<GP_PART; j++)
        {
            if(g_hash_table_contains(p->table[j], feature->fea[i]))
            {
                p->hit_pos[p->hit] = j;
                p->hit_map[i] = 1;
                p->hit ++;
                break;
            }
            else {
                p->hit_map[i] = 0;
            }
        }
        #else
        p->hit_map[i] = 0;
        #endif
    }
    return p->hit;
}

#if(FEATURE_METHOD!=SIGMA_FEATURE)
static int use_gp(message_feature *feature)
{
    uint16_t hit[NODE_NUM];
    for(int i=0; i<NODE_NUM; i++)
    {
        hit[i] = match_in_gp(&gp[i], feature);
    }
    boolean all_lower = TRUE;
    for(int i=0; i<NODE_NUM; i++)
    {
        if(hit[i] > HIT_THRES)
        {
            all_lower = FALSE;
            break;
        }
    }
    if(all_lower) return -1;
    int target = find_weighted_best(hit, storage, NODE_NUM, feature->fea_num);
    storage[target] += EXP_SEGMENT_SIZE*(feature->fea_num-hit[target])/FEATURE_NUM;
    gp_fea_hit_num += hit[target];
    return target;
}

static void feature_request(message_feature *feature, message_re_feature re[])
{
    message_feature new_feature[NODE_NUM];
    for(int i=0; i<NODE_NUM; i++) {
        new_feature[i].type = M_FEATURE_TYPE;
        new_feature[i].hit = gp[i].hit;
        new_feature[i].fea_num = 0;
        for(int j=0; j<feature->fea_num; j++) {
            if(!gp[i].hit_map[j]) {
                fpcpy(new_feature[i].fea[new_feature[i].fea_num], feature->fea[j], 1);
                new_feature[i].fea_num ++;
            }
        }
    }
    _g_timer_count(total_fea_recv_timer, total_fea_recv_time,
        g_timer_start(average_fea_recv_timer);
        for(int i=0; i<NODE_NUM; i++) {
            send_message(fea_fd[i], &new_feature[i], sizeof(message_feature));
        }
        int count = 0; int nfds;
        struct epoll_event events[NODE_NUM];
        while(count < NODE_NUM)
        {
            nfds = epoll_wait(epoll_fd, events, NODE_NUM, -1);
            average_fea_recv_time += nfds*g_timer_elapsed(average_fea_recv_timer, NULL);
            for(int i=0; i<nfds; i++)
            {
                recv_message(fea_fd[events[i].data.fd], &re[events[i].data.fd], sizeof(message_re_feature)-sizeof_xfp(GP_SIZE));
                if(re[events[i].data.fd].hit > HIT_THRES)
                {
                    recv_message(fea_fd[events[i].data.fd], (void*)&re[events[i].data.fd]+sizeof(message_re_feature)-sizeof_xfp(GP_SIZE), sizeof_xfp(GP_SIZE));
                }
            }
            count += nfds;
        }
    );
}


static int lru_id(struct guidepost *p)
{
    int hit_count[GP_PART] = {0};
    for(int i=0; i<p->hit; i++)
    {
        hit_count[p->hit_pos[i]] ++;
    }
    int min = hit_count[0], index = 0;
    for(int i=1; i<GP_PART; i++)
    {
        if(hit_count[i] < min) {
            min = hit_count[i];
            index = i;
        }
    }
    for(int i=0; i<GP_PART; i++)
    {
        if(hit_count[i]==min && i!=index)
        {
            boolean later = FALSE;
            for(int j=0; j<p->hit; j++)
            {
                if(p->hit_pos[j] == i) {
                    later = TRUE;
                    break;
                }
                else if(p->hit_pos[j] == index) {
                    break;
                }
            }
            if(!later) index = i;
        }
    }
    return index;
}

static void update_gp(message_re_feature re[])
{
    for(int i=0; i<NODE_NUM; i++)
    {
        storage[i] = re[i].storage;
        #if(SSDEDUP == ON)
        lcache[i].storage = re[i].storage;
        #endif
        #ifndef DEBUG_MOTI
        if(re[i].hit > HIT_THRES)
        {
            int replaced_id = lru_id(&gp[i]);
            fpcpy(gp[i].part[replaced_id], re[i].gp, GP_SIZE);
            g_hash_table_destroy(gp[i].table[replaced_id]);
            gp[i].table[replaced_id] = g_hash_table_new_full(my_fplen_hash, my_fplen_equal, NULL, NULL);
            for(int j=0; j<GP_SIZE; j++)
            {
                g_hash_table_insert(gp[i].table[replaced_id], gp[i].part[replaced_id][j], NULL);
            }
        }
        #endif
    }
}

static int use_force(message_feature *feature)
{
    message_re_feature re[NODE_NUM];
    uint16_t hit[NODE_NUM];
    feature_request(feature, re);
    for(int i=0; i<NODE_NUM; i++) hit[i] = re[i].hit;
    update_gp(re);
    int target = find_weighted_best(hit, storage, NODE_NUM, feature->fea_num);
    #ifdef PREDICTION_EVA
        if(hit[target] == 0) gp_tn++;
        else gp_fn++;
        #if(SSDEDUP==ON)
        if(hit[target] == 0) ss_tn++;
        else ss_fn++;
        #endif
    #endif
    #ifdef DEBUG_MOTI
    total_fea += feature->fea_num;
    hit_fea += re[target].fea_hit;
    #endif
    storage[target] += EXP_SEGMENT_SIZE*(feature->fea_num-hit[target])/FEATURE_NUM;
    force_fea_hit_num += hit[target];
    return target;
}

#ifdef PREDICTION_EVA
static void feature_request_for_pre_eva(message_feature *feature, message_re_feature re[])
{
    message_feature new_feature[NODE_NUM];
    for(int i=0; i<NODE_NUM; i++) {
        new_feature[i].type = M_FEATURE_TYPE;
        new_feature[i].hit = 0;
        new_feature[i].fea_num = feature->fea_num;
        for(int j=0; j<feature->fea_num; j++) {
            fpcpy(new_feature[i].fea[j], feature->fea[j], 1);
        }
    }
    _g_timer_count(total_fea_recv_timer, total_fea_recv_time,
        g_timer_start(average_fea_recv_timer);
        for(int i=0; i<NODE_NUM; i++) {
            send_message(fea_fd[i], &new_feature[i], sizeof(message_feature));
        }
        int count = 0; int nfds;
        struct epoll_event events[NODE_NUM];
        while(count < NODE_NUM)
        {
            nfds = epoll_wait(epoll_fd, events, NODE_NUM, -1);
            average_fea_recv_time += nfds*g_timer_elapsed(average_fea_recv_timer, NULL);
            for(int i=0; i<nfds; i++)
            {
                recv_message(fea_fd[events[i].data.fd], &re[events[i].data.fd], sizeof(message_re_feature)-sizeof_xfp(GP_SIZE));
                if(re[events[i].data.fd].hit > HIT_THRES)
                {
                    recv_message(fea_fd[events[i].data.fd], (void*)&re[events[i].data.fd]+sizeof(message_re_feature)-sizeof_xfp(GP_SIZE), sizeof_xfp(GP_SIZE));
                }
            }
            count += nfds;
        }
    );
}

static int use_force_for_pre_eva(message_feature *feature)
{
    message_re_feature re[NODE_NUM];
    uint16_t hit[NODE_NUM];
    feature_request_for_pre_eva(feature, re);
    for(int i=0; i<NODE_NUM; i++) hit[i] = re[i].hit;
    int target = find_weighted_best(hit, storage, NODE_NUM, feature->fea_num);
    storage[target] += EXP_SEGMENT_SIZE*(feature->fea_num-hit[target])/FEATURE_NUM;
    force_fea_hit_num += hit[target];
    return target;
}
#endif

#if(SSDEDUP==OFF)
int gp_route(message_feature *feature)
{
    #ifndef DEBUG_MOTI
        #ifndef PREDICTION_EVA
        int target = use_gp(feature);
        if(target < 0) target = use_force(feature);
        else {
            gp_hit_times ++;
            gp_part_statistic(&gp[target]);
        }
        #else
        int target = use_gp(feature);
        if(target < 0)
        {
            target = use_force(feature);
        }
        else
        {
            gp_hit_times ++;
            gp_part_statistic(&gp[target]);
            int tmp_target = use_force_for_pre_eva(feature);
            if(tmp_target == target) gp_tp++;
            else gp_fp++;
        }
        #endif
    #else
    int target = use_gp(feature);
    target = use_force(feature);
    #endif
    return target;
}
#else   //  #if(SSDEDUP==OFF)
int gp_route(message_feature *feature)
{
    int target;
    #ifndef PREDICTION_EVA
    target = use_lcache(feature);
    if(target < 0) {
        target = use_gp(feature);
        if(target < 0) target = use_force(feature);
        else {
            gp_hit_times ++;
            gp_part_statistic(&gp[target]);
        }
    }
    else lc_hit_times ++;
    #else
    target = use_lcache(feature);
    if(target < 0) {
        target = use_gp(feature);
        if(target < 0) target = use_force(feature);
        else {
            gp_hit_times ++;
            gp_part_statistic(&gp[target]);
            int tmp_target = use_force_for_pre_eva(feature);
            if(tmp_target == target) gp_tp++;
            else gp_fp++;
            ss_fn++;
        }
    }
    else {
        lc_hit_times ++;
        int tmp_target = use_force_for_pre_eva(feature);
        if(tmp_target == target) ss_tp++;
        else ss_fp++;
    }
    #endif
    #if(SSDEDUP==ON)
    update_lcache(target, feature);
    #endif
    return target;
}
#endif  //  #if(SSDEDUP==OFF)
#else   //  #if(FEATURE_METHOD!=SIGMA_FEATURE)
static int use_k_gp(message_feature *feature, int dst_id[], int addr_num)
{
    uint16_t hit[NODE_NUM];
    for(int i=0; i<addr_num; i++)
    {
        hit[i] = match_in_gp(&gp[dst_id[i]], feature);
    }
    boolean all_lower = TRUE;
    for(int i=0; i<addr_num; i++)
    {
        if(hit[i] > HIT_THRES)
        {
            all_lower = FALSE;
            break;
        }
    }
    if(all_lower) return -1;
    uint64_t s_tmp[NODE_NUM];
    for(int i=0; i>addr_num; i++) s_tmp[i] = storage[dst_id[i]];
    int target = find_weighted_best(hit, s_tmp, addr_num, feature->fea_num);
    storage[dst_id[target]] += EXP_SEGMENT_SIZE*(feature->fea_num-hit[target])/FEATURE_NUM;
    gp_fea_hit_num += hit[target];
    return dst_id[target];
}

static void feature_request(message_feature *feature, message_re_feature re[], int dst_id[], int addr_num)
{
    message_feature new_feature[NODE_NUM];
    for(int i=0; i<addr_num; i++) {
        new_feature[i].type = M_FEATURE_TYPE;
        new_feature[i].hit = gp[dst_id[i]].hit;
        new_feature[i].fea_num = 0;
        for(int j=0; j<feature->fea_num; j++) {
            if(!gp[dst_id[i]].hit_map[j]) {
                fpcpy(new_feature[i].fea[new_feature[i].fea_num], feature->fea[j], 1);
                new_feature[i].fea_num ++;
            }
        }
    }
    _g_timer_count(total_fea_recv_timer, total_fea_recv_time,
        g_timer_start(average_fea_recv_timer);
        for(int i=0; i<addr_num; i++) {
            send_message(fea_fd[dst_id[i]], &new_feature[i], sizeof(message_feature));
        }
        int count = 0; int nfds;
        struct epoll_event events[NODE_NUM];
        while(count < addr_num)
        {
            nfds = epoll_wait(epoll_fd, events, addr_num, -1);
            for(int i=0; i<nfds; i++)
            {
                recv_message(fea_fd[events[i].data.fd], &re[events[i].data.fd], sizeof(message_re_feature)-sizeof_xfp(GP_SIZE));
                if(re[events[i].data.fd].hit > HIT_THRES)
                {
                    recv_message(fea_fd[events[i].data.fd], (void*)&re[events[i].data.fd]+sizeof(message_re_feature)-sizeof_xfp(GP_SIZE), sizeof_xfp(GP_SIZE));
                }
                average_fea_recv_time += g_timer_elapsed(average_fea_recv_timer, NULL);
            }
            count += nfds;
        }
    );
}

static int lru_id(struct guidepost *p)
{
    int hit_count[GP_PART] = {0};
    for(int i=0; i<p->hit; i++)
    {
        hit_count[p->hit_pos[i]] ++;
    }
    int min = hit_count[0], index = 0;
    for(int i=1; i<GP_PART; i++)
    {
        if(hit_count[i] < min) {
            min = hit_count[i];
            index = i;
        }
    }
    for(int i=0; i<GP_PART; i++)
    {
        if(hit_count[i]==min && i!=index)
        {
            boolean later = FALSE;
            for(int j=0; j<p->hit; j++)
            {
                if(p->hit_pos[j] == i) {
                    later = TRUE;
                    break;
                }
                else if(p->hit_pos[j] == index) {
                    break;
                }
            }
            if(!later) index = i;
        }
    }
    return index;
}

static void update_gp(message_re_feature re[], int dst_id[], int addr_num)
{
    for(int i=0; i<addr_num; i++)
    {
        storage[dst_id[i]] = re[dst_id[i]].storage;
        if(re[dst_id[i]].hit > HIT_THRES)
        {
            int replaced_id = lru_id(&gp[dst_id[i]]);
            fpcpy(gp[dst_id[i]].part[replaced_id], re[dst_id[i]].gp, GP_SIZE);
            g_hash_table_destroy(gp[dst_id[i]].table[replaced_id]);
            gp[dst_id[i]].table[replaced_id] = g_hash_table_new_full(my_fplen_hash, my_fplen_equal, NULL, NULL);
            for(int j=0; j<GP_SIZE; j++)
            {
                g_hash_table_insert(gp[dst_id[i]].table[replaced_id], gp[dst_id[i]].part[replaced_id][j], NULL);
            }
        }
    }
}

static int use_k_force(message_feature *feature, int dst_id[], int addr_num)
{
    message_re_feature re[NODE_NUM];
    uint16_t hit[NODE_NUM];
    feature_request(feature, re, dst_id, addr_num);
    for(int i=0; i<addr_num; i++) hit[i] = re[dst_id[i]].hit;
    update_gp(re, dst_id, addr_num);
    uint64_t s_tmp[NODE_NUM];
    for(int i=0; i<addr_num; i++) s_tmp[i] = storage[dst_id[i]];
    int target = find_weighted_best(hit, s_tmp, addr_num, feature->fea_num);
    storage[dst_id[target]] += EXP_SEGMENT_SIZE*(feature->fea_num-hit[target])/FEATURE_NUM;
    force_fea_hit_num += hit[target];
    return dst_id[target];
}

int gp_route(message_feature *feature, int dst_id[], int addr_num)
{
    int target = use_k_gp(feature, dst_id, addr_num);
    if(target < 0) target = use_k_force(feature, dst_id, addr_num);
    else gp_hit_times ++;
    return target;
}

int k_gp_route(message_feature *feature)
{
    int dst_id[NODE_NUM] = {NODE_NUM}, addr_num = 0;
    int tmp, j;
    for(int i=0; i<feature->fea_num; i++)
    {
        tmp = ((uint8_t)(feature->fea[i][FP_LEN-1]))%NODE_NUM;
        for(j=0; j<addr_num; j++)
        {
            if(dst_id[j] == tmp) break;
        }
        if(j == addr_num)
        {
            dst_id[addr_num] = tmp;
            addr_num ++;
        }
    }
    return gp_route(feature, dst_id, addr_num);
}
#endif  //  #if(FEATURE_METHOD!=SIGMA_FEATURE)

#elif(ROUTE_METHOD==BOAFFT)

static void feature_request(message_feature *feature, message_re_feature re[])
{
    _g_timer_count(total_fea_recv_timer, total_fea_recv_time,
        g_timer_start(average_fea_recv_timer);
        for(int i=0; i<NODE_NUM; i++) send_message(fea_fd[i], feature, sizeof(message_feature));
        int count = 0; int nfds;
        struct epoll_event events[NODE_NUM];
        while(count < NODE_NUM)
        {
            nfds = epoll_wait(epoll_fd, events, NODE_NUM, -1);
            for(int i=0; i<nfds; i++)
            {
                recv_message(fea_fd[events[i].data.fd], &re[events[i].data.fd], sizeof(message_re_feature));
                average_fea_recv_time += g_timer_elapsed(average_fea_recv_timer, NULL);
            }
            count += nfds;
        }
    );
}

#if(SSDEDUP==OFF)
static int use_force(message_feature *feature)
{
    message_re_feature re[NODE_NUM];
    feature_request(feature, re);
    uint16_t hit[NODE_NUM];
    uint64_t storage[NODE_NUM];
    for(int i=0; i<NODE_NUM; i++)
    {
        hit[i] = re[i].hit;
        storage[i] = re[i].storage;
    }
    int target = find_weighted_best(hit, storage, NODE_NUM, feature->fea_num);
    force_fea_hit_num += hit[target];
    return target;
}

int force_route(message_feature *feature)
{
    return use_force(feature);
}
#else   //  #if(SSDEDUP==OFF)
static int use_force(message_feature *feature)
{
    message_re_feature re[NODE_NUM];
    feature_request(feature, re);
    uint16_t hit[NODE_NUM];
    uint64_t storage[NODE_NUM];
    for(int i=0; i<NODE_NUM; i++)
    {
        hit[i] = re[i].hit;
        storage[i] = re[i].storage;
        lcache[i].storage = re[i].storage;
    }
    int target = find_weighted_best(hit, storage, NODE_NUM, feature->fea_num);
    #ifdef PREDICTION_EVA
    if(hit[target] == 0) ss_tn++;
    else ss_fn++;
    #endif
    force_fea_hit_num += hit[target];
    return target;
}

int force_route(message_feature *feature)
{
    #ifndef DEBUG_MOTI
    int target = use_lcache(feature);
    if(target < 0) target = use_force(feature);
    else {
        lc_hit_times ++;
        #ifdef PREDICTION_EVA
        int tmp_target = use_force(feature);
        if(tmp_target == target) ss_tp ++;
        else ss_fp ++;
        #endif
    }
    #else
    total_fea += feature->fea_num;
    int target = use_force(feature);
    use_lcache(feature, target);
    #endif
    update_lcache(target, feature);
    return target;
}
#endif  //  #if(SSDEDUP==OFF)

#elif(ROUTE_METHOD==SIGMA)

static void feature_request(message_feature *feature, message_re_feature re[], int dst_id[], int addr_num)
{
    _g_timer_count(total_fea_recv_timer, total_fea_recv_time,
        g_timer_start(average_fea_recv_timer);
        for(int i=0; i<addr_num; i++) send_message(fea_fd[dst_id[i]], feature, sizeof(message_feature));
        int count = 0; int nfds;
        struct epoll_event events[NODE_NUM];
        while(count < addr_num)
        {
            nfds = epoll_wait(epoll_fd, events, addr_num, -1);
            for(int i=0; i<nfds; i++)
            {
                recv_message(fea_fd[events[i].data.fd], &re[events[i].data.fd], sizeof(message_re_feature));
                average_fea_recv_time += g_timer_elapsed(average_fea_recv_timer, NULL);
            }
            count += nfds;
        }
    );
}

static int use_force(message_feature *feature, int dst_id[], int addr_num)
{
    message_re_feature re[NODE_NUM];
    feature_request(feature, re, dst_id, addr_num);
    uint16_t hit[NODE_NUM];
    uint64_t storage[NODE_NUM];
    for(int i=0; i<addr_num; i++)
    {
        hit[i] = re[dst_id[i]].hit;
        storage[i] = re[dst_id[i]].storage;
    }
    int target = find_weighted_best(hit, storage, addr_num, feature->fea_num);
    force_fea_hit_num += hit[target];
    return dst_id[target];
}

int k_force_route(message_feature *feature)
{
    int dst_id[NODE_NUM] = {NODE_NUM}, addr_num = 0;
    int j, tmp;
    for(int i=0; i<feature->fea_num; i++)
    {
        tmp = ((uint8_t)(feature->fea[i][FP_LEN-1]))%NODE_NUM;
        for(j=0; j<addr_num; j++)
        {
            if(dst_id[j] == tmp) break;
        }
        if(j == addr_num)
        {
            dst_id[addr_num] = tmp;
            addr_num ++;
        }
    }
    return use_force(feature, dst_id, addr_num);
}

#elif(ROUTE_METHOD==STATELESS)

static void feature_request(message_feature *feature, message_re_feature re[])
{
    _g_timer_count(total_fea_recv_timer, total_fea_recv_time,
        g_timer_start(average_fea_recv_timer);
        for(int i=0; i<NODE_NUM; i++) send_message(fea_fd[i], feature, sizeof(message_feature));
        int count = 0; int nfds;
        struct epoll_event events[NODE_NUM];
        while(count < NODE_NUM)
        {
            nfds = epoll_wait(epoll_fd, events, NODE_NUM, -1);
            for(int i=0; i<nfds; i++)
            {
                recv_message(fea_fd[events[i].data.fd], &re[events[i].data.fd], sizeof(message_re_feature));
                average_fea_recv_time += g_timer_elapsed(average_fea_recv_timer, NULL);
            }
            count += nfds;
        }
    );
}

int stateless_route(message_feature *feature)
{
    message_re_feature re_m[NODE_NUM];    
    feature_request(feature, re_m);

    uint64_t storage[NODE_NUM], total = 0;
    for(int i=0; i<NODE_NUM; i++)
    {
        storage[i] = re_m[i].storage;
        total += storage[i];
    }

    int id = feature->fea[0][0]%NODE_NUM;
    while(storage[id]/(total/(double)NODE_NUM) >= 1.05)
    {
        id = (id+1)%NODE_NUM;
    }
    return id;
}

#endif

int route(message_feature *feature)
{
    int target;

    #if(ROUTE_METHOD==GUIDEPOST)
        #if(FEATURE_METHOD==SIGMA_FEATURE)
            target = k_gp_route(feature);
        #else
            target = gp_route(feature);
        #endif
    #elif(ROUTE_METHOD==BOAFFT)
        target = force_route(feature);
    #elif(ROUTE_METHOD==SIGMA)
        target = k_force_route(feature);
    #elif(ROUTE_METHOD==STATELESS)
        target = feature->fea[0][0]%NODE_NUM;
        // target = stateless_route(feature);
    #endif

    return target;
}