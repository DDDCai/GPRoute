/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-26 05:18:16
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-10-19 07:32:29
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include "message.h"
#include "dis_dedup.h"
#include "dis_config.h"
#include "glib.h"
#include <stdio.h>
#include "concurrent_hashtable.h"

extern cHashTable *fea_table, *fp_table;
extern struct counter {
    uint64_t num;
    GRWLock lock;
} re_gp;

#ifdef DEBUG_MOTI
extern uint64_t total_fea, hit_fea;
extern gpointer con_backup[FEATURE_NUM*2], con_tmp[FEATURE_NUM];
extern int backup_pos;
extern struct lru_item {gpointer data; gpointer next, pre;} con_lru[FEATURE_NUM];
extern struct lru_item * lru_head, *lru_tail;
#endif

#if(ROUTE_METHOD==GUIDEPOST)

extern struct htable_queue {
    cHashTable *table;
    GQueue *queue;
    GRWLock lock;
} open_fea_con;

#define ONE_PIECE
#ifdef ONE_PIECE

uint16_t fp_hit_times(fingerprint *fp, uint32_t fp_num, cHashTable *index, fingerprint *val[])
{
    uint16_t hit = 0;
    for(int i=0; i<fp_num; i++)
    {
        if(val[i] = c_hash_table_lookup(index, fp[i])) hit++;
    }
    #ifdef DEBUG_MOTI
    for(int i=0; i<FEATURE_NUM; i++)
    {
        // if(con_tmp[i]) {
        //     con_backup[backup_pos] = con_tmp[i];
        //     con_tmp[i] = NULL;
        //     backup_pos = (backup_pos+1)%(FEATURE_NUM*1);
        // }

        // if(con_tmp[i])
        // {
        //     int j;
        //     for(j=0; j<FEATURE_NUM; j++)
        //     {
        //         if(con_backup[j] == con_tmp[i]) break;
        //     }
        //     if(j!=FEATURE_NUM)
        //     {
        //         for(int k=j; k<FEATURE_NUM-1; k++)
        //         {
        //             con_backup[k] = con_backup[k+1];
        //         }
        //     }
        //     else
        //     {
        //         for(int k=0; k<FEATURE_NUM-1; k++)
        //         {
        //             con_backup[k] = con_backup[k+1];
        //         }
        //     }
        //     con_backup[FEATURE_NUM-1] = con_tmp[i];
        // }

        // con_backup[i] = con_tmp[i];
        // con_tmp[i] = NULL;

    //     if(con_tmp[i]) 
    //     {
    //         struct lru_item *p = lru_head;
    //         int j;
    //         for(j=0; j<FEATURE_NUM; j++,p=p->next) 
    //         {
    //             if(p->data == con_tmp[i]) 
    //             {
    //                 if(p == lru_head) 
    //                 {
    //                     lru_head = p->next;
    //                 }
    //                 else if(p == lru_tail) break;
    //                 else 
    //                 {
    //                     ((struct lru_item *)p->pre)->next = ((struct lru_item *)p->next);
    //                     ((struct lru_item *)p->next)->pre = ((struct lru_item *)p->pre);
    //                 }
    //                 lru_tail->next = p;
    //                 p->pre = lru_tail;
    //                 lru_tail = p;
    //                 break;
    //             }
    //         }
    //         if(j == FEATURE_NUM) 
    //         {
    //             p = lru_tail;
    //             int k;
    //             for(k=FEATURE_NUM-1; k>=0; k--,p=p->pre)
    //             {
    //                 if(p->data == NULL)
    //                 {
    //                     p->data = con_tmp[i];
    //                     if(p == lru_tail)
    //                     {
    //                         break;
    //                     }
    //                     else if(p == lru_head)
    //                     {
    //                         lru_head = lru_head->next;
    //                     }
    //                     else
    //                     {
    //                         ((struct lru_item *)p->pre)->next = ((struct lru_item *)p->next);
    //                         ((struct lru_item *)p->next)->pre = ((struct lru_item *)p->pre);
    //                     }
    //                     lru_tail->next = p;
    //                     p->pre = lru_tail;
    //                     lru_tail = p;
    //                 }
    //             }
    //             if(k == FEATURE_NUM)
    //             {
    //                 lru_head->data = con_tmp[i];
    //                 lru_head->pre = lru_tail;
    //                 lru_tail->next = lru_head;
    //                 lru_tail = lru_head;
    //                 lru_head = lru_head->next;
    //             }
    //         }
    //         con_tmp[i] = NULL;
    //     }


    }
    #endif
    return hit;
}

/**
 * @description: According to the results of fp_hit_times(), get the container having the most matched features. *** Note that the input cur[] has been changed here! ***
 * @param {fingerprint} *cur[]: pointers of matched feature-containers
 * @param {int} num: num of features
 * @return {fingerprint*}: the most matched container
 */
fingerprint *get_best_con(fingerprint *cur[], int num)
{
    fingerprint *tmp, *best = NULL;
    int count, max = 0;
    for(int j=0; j<num; j++)
    {
        tmp = cur[j];
        if(tmp == NULL) continue;
        count = 1;
        for(int i=j+1; i<num; i++)
        {
            if(tmp==cur[i])
            {
                cur[i] = NULL;
                count++;
            }
        }
        if(count > max)
        {
            max = count;
            best = tmp;
        }
    }
    return best;
}

int feature_process(message_feature *feature, uint8_t *re_buf)
{
    message_re_feature *message = (message_re_feature*)re_buf;
    message->type = M_RE_FEATURE_TYPE;
    message->storage = c_hash_table_size(fp_table);
    fingerprint *cur[FEATURE_NUM] = {NULL};
    message->hit = fp_hit_times(feature->fea, feature->fea_num, fea_table, cur) + feature->hit;
    #ifdef DEBUG_MOTI
    message->fea_hit = hit_fea;
    hit_fea = 0;
    #endif
    if(message->hit > HIT_THRES)
    {
        _writelock_execution(&re_gp.lock, re_gp.num += 1);
        fpcpy(message->gp, get_best_con(cur, feature->fea_num), GP_SIZE);
        return sizeof(message_re_feature);
    }
    return sizeof(message_re_feature)-sizeof_xfp(GP_SIZE);
}

#else // #ifdef ONE_PIECE

uint16_t fp_hit_times(fingerprint *fp, uint32_t fp_num, cHashTable *index, fingerprint *key[], fingerprint *val[])
{
    uint16_t hit = 0;
    for(int i=0; i<fp_num; i++)
    {
        if(c_hash_table_lookup_extended(index, fp[i], (gpointer*)&key[hit], (gpointer*)&val[hit]))
        {
            hit ++;
        }
    }
    return hit;
}

/**
 * @description: Generate a new Guidepost from feature containers. Instead of choosing the most matched one container as the gp data, it extracts gp data from each matched container proportionally according to the number of matched features in each container. *** Note that the input head[] has been changed here! ***
 * @param {fingerprint} *gp: to save the generated gp data
 * @param {fingerprint} *cur[]: matched feature pointers
 * @param {fingerprint} *head[]: matched container pointers
 * @param {int} num: number of matched features
 * @return {}
 */
static void generate_gp(fingerprint *gp, fingerprint *cur[], fingerprint *head[], int num)
{
    fingerprint *hp, *cp;
    int hit, pos, needed;
    int ready_num = 0, piece_size = GP_SIZE/num, gap_size = GP_SIZE-num*piece_size;
    for(int i=0; i<num; i++)
    {
        if(!(hp = head[i])) continue;
        cp = cur[i];
        hit = 1;
        for(int j=i+1; j<num; j++)
        {
            if(head[j]!=hp) continue;
            hit ++;
            if((uint64_t)cur[j] < (uint64_t)cp) {
                cp = cur[j];
            }
            head[j] = NULL;
        }
        pos = ((uint64_t)cp - (uint64_t)hp)/FP_LEN;
        needed = hit*piece_size;
        if(GP_SIZE-pos >= needed) {
            fpcpy(gp+ready_num, cp, needed);
        }
        else {
            fpcpy(gp+ready_num, hp+GP_SIZE-needed, needed);
        }
        ready_num += needed;
    }
    if(gap_size)
    {
        if(GP_SIZE-pos >= needed) {
            int remain = GP_SIZE-pos-needed;
            if(remain >= gap_size) {
                fpcpy(gp+ready_num, cp+needed, gap_size);
            }
            else {
                fpcpy(gp+ready_num, cp+needed, remain);
                fpcpy(gp+ready_num+remain, cp-gap_size+remain, gap_size-remain);
            }
        }
        else {
            fpcpy(gp+ready_num, cp-pos+GP_SIZE-needed-gap_size, gap_size);
        }
    }
}

int feature_process(message_feature *feature, uint8_t *re_buf)
{
    message_re_feature *message = (message_re_feature*)re_buf;
    message->type = M_RE_FEATURE_TYPE;
    message->storage = c_hash_table_size(fp_table);
    fingerprint *cur[FEATURE_NUM] = {NULL}, *head[FEATURE_NUM] = {NULL};
    message->hit = fp_hit_times(feature->fea, feature->fea_num, fea_table, cur, head) + feature->hit;
    if(message->hit > HIT_THRES)
    {
        _writelock_execution(&re_gp.lock, re_gp.num += 1);
        generate_gp(message->gp, cur, head, message->hit);
        return sizeof(message_re_feature);
    }
    return sizeof(message_re_feature)-sizeof_xfp(GP_SIZE);
}

#endif // #ifdef ONE_PIECE

fea_container *new_fea_container(gint64 *fd_key, boolean exist)
{
    _writelock_execution(&open_fea_con.lock,
      fea_container *con = g_queue_pop_head(open_fea_con.queue);
    );
    if(con == NULL)
    {
        con = (fea_container*)g_malloc0(sizeof(fea_container));
        con->gp_data = g_malloc0(sizeof_xfp(GP_SIZE));
    }
    gint64 *key;
    if(exist) key = fd_key;
    else
    {
        key = (gint64*)malloc(sizeof(gint64));
        *key = *fd_key;
    }
    c_hash_table_insert(open_fea_con.table, key, con);
    return con;
}

static void free_fea_container(fea_container *con)
{
    free(con);
}

void update_feature_index(message_segment *segment, int fd)
{
    fea_container *con;
    gint64 key = (gint64)(fd*16), *ori_key;
    gboolean exist = c_hash_table_lookup_extended(open_fea_con.table, &key, (gpointer*)&ori_key, (gpointer*)&con);
    if(!exist) con = new_fea_container(&key, FALSE);
    for(int i=0; i<segment->fea_num; i++)
    {
        if(!c_hash_table_try_steal_insert(fea_table, segment->fea[i], con->gp_data[con->used_slot], FP_LEN, con->gp_data))
        {
            con->used_slot ++;
            if(con->used_slot == GP_SIZE)
            {
                free_fea_container(con);
                con = new_fea_container(&key, TRUE);
            }
        }
    }
}

#else // #if(ROUTE_METHOD==GUIDEPOST)

static uint16_t fp_hit_times(fingerprint *fp, uint32_t fp_num, cHashTable *index)
{
    uint16_t hit = 0;
    for(int i=0; i<fp_num; i++)
    {
        if(c_hash_table_contains(index, fp[i])) hit++;
    }
    return hit;
}

int feature_process(message_feature *feature, uint8_t *re_buf)
{
    message_re_feature *message = (message_re_feature*)re_buf;
    message->type = M_RE_FEATURE_TYPE;
    message->storage = c_hash_table_size(fp_table);
    message->hit = fp_hit_times(feature->fea, feature->fea_num, fea_table);
    return sizeof(message_re_feature);
}

void update_feature_index(message_segment *segment)
{
    for(int i=0; i<segment->fea_num; i++)
    {
        c_hash_table_try_copy_insert(fea_table, segment->fea[i], FP_LEN, NULL, 0);
    }
}

#endif  // #if(ROUTE_METHOD==GUIDEPOST)