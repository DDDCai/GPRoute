/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-06-25 11:56:40
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-10-19 09:51:23
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */
#include "concurrent_hashtable.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dis_config.h"
#ifdef DEBUG_MOTI
extern uint64_t total_fea, hit_fea;
extern gpointer con_backup[FEATURE_NUM*2], con_tmp[FEATURE_NUM];
extern struct lru_item {gpointer data; gpointer next, pre;} con_lru[FEATURE_NUM];
extern struct lru_item * lru_head, *lru_tail;
#endif

#define MAX_SUB_HT_SIZE 1024

struct _cHashTable {
    int sub_ht_num;
    GRWLock *lock;
    GHashTable **ht;
};

/**
 * @description: Create a new cHashTable.
 * @param {GHashFunc} hash_func : A function to create a hash value from a key
 * @param {GEqualFunc} key_equal_func : A function to check two keys for equality
 * @param {GDestroyNotify} key_destroy_func : A function to free the memory allocated for the key used when removing the entry from the cHashTable
 * @param {GDestroyNotify} value_destroy_func : A function to free the memory allocated for the value used when removing the entry from the cHashTable
 * @param {int} sub_table_num : The number of concurrently running sub-HashTables in cHashTable which can not exceed MAX_SUB_HT_SIZE (1024)
 * @return { cHashTable* } : The created cHashTable
 */
cHashTable *c_hash_table_new(GHashFunc hash_func, GEqualFunc key_equal_func, GDestroyNotify key_destroy_func, GDestroyNotify  value_destroy_func, int sub_table_num)
{
    if(sub_table_num <= 0 || sub_table_num > MAX_SUB_HT_SIZE) {
        printf("Wrong sub_table_num\n");
        return NULL;
    }
    cHashTable *cht = (cHashTable*)malloc(sizeof(cHashTable));
    cht->sub_ht_num = sub_table_num;
    cht->ht = malloc(sizeof(GHashTable*)*cht->sub_ht_num);
    cht->lock = malloc(sizeof(GRWLock)*cht->sub_ht_num);
    for(int i=0; i<cht->sub_ht_num; i++)
    {
        cht->ht[i] = g_hash_table_new_full(hash_func, key_equal_func, key_destroy_func, value_destroy_func);
        g_rw_lock_init(&cht->lock[i]);
    }
    return cht;
}

/**
 * @description: Destroy the cHashTable and free its memory.
 */
void c_hash_table_destroy(cHashTable *cht)
{
    for(int i=0; i<cht->sub_ht_num; i++)
    {
        g_hash_table_destroy(cht->ht[i]);
        g_rw_lock_clear(&cht->lock[i]);
    }
    free(cht->ht);
    free(cht->lock);
    free(cht);
}

/**
 * @description: Returns the number of elements contained in the cHashTable.
 * @return { uint32_t } : the number of elements contained in the cHashTable
 */
uint32_t c_hash_table_size(cHashTable *cht)
{
    uint32_t size = 0;
    for(int i=0; i<cht->sub_ht_num; i++)
    {
        size += g_hash_table_size(cht->ht[i]);
    }
    return size;
}

/**
 * @description: Inserts a new key and value into a cHashTable. If the key already exists in the cHashTable its current value is freed by the value_destroy_func() supplied when creating the cHashTable and replaced with the new value.
 * @return { gboolean } : whether the input key is already in this cHashTable or not
 */
gboolean c_hash_table_insert(cHashTable *cht, void *key, void *value)
{
    uint32_t ht_id = (*((uint32_t*)key)) % cht->sub_ht_num;
    g_rw_lock_writer_lock(&cht->lock[ht_id]);
    gboolean exist = g_hash_table_insert(cht->ht[ht_id], key, value);
    g_rw_lock_writer_unlock(&cht->lock[ht_id]);
    return exist;
}

/**
 * @description: Similar to c_hash_table_insert(). But this function only inserts the input key when the cHashTable does not contain it.
 * @return { gboolean } : whether the input key is already in this cHashTable or not
 */
gboolean c_hash_table_try_insert(cHashTable *cht, void *key, void *value)
{
    uint32_t ht_id = (*((uint32_t*)key)) % cht->sub_ht_num;
    g_rw_lock_writer_lock(&cht->lock[ht_id]);
    gboolean exist = g_hash_table_contains(cht->ht[ht_id], key);
    if(!exist) {
        g_hash_table_insert(cht->ht[ht_id], key, value);
    }
    g_rw_lock_writer_unlock(&cht->lock[ht_id]);
    return exist;
}

/**
 * @description: Similar to c_hash_table_try_insert(). Additionally, if a positive key length is given, copy the content of the input key into a newly allocated key and insert the copied key instead. Similar as the value.
 * @return { gboolean } : whether the input key is already in this cHashTable or not
 */
gboolean c_hash_table_try_copy_insert(cHashTable *cht, void *key, int key_len, void *value, int value_len)
{
    uint32_t ht_id = (*((uint32_t*)key)) % cht->sub_ht_num;
    g_rw_lock_writer_lock(&cht->lock[ht_id]);
    gboolean exist = g_hash_table_contains(cht->ht[ht_id], key);
    if(!exist) 
    {
        void *real_key = key, *real_value = value;
        if(key_len > 0) {
            real_key = malloc(key_len);
            memcpy(real_key, key, key_len);
        }
        if(value_len > 0) {
            real_value = malloc(value_len);
            memcpy(real_value, value, value_len);
        }
        g_hash_table_insert(cht->ht[ht_id], real_key, real_value);
    }
    g_rw_lock_writer_unlock(&cht->lock[ht_id]);
    return exist;
}

/**
 * @description: Similar to c_hash_table_try_insert(). The difference is that it copies the content of lookup_key into insert_key and insert the insert_key instead.
 * @return { gboolean } : whether the lookup key is already in this cHashTable or not
 */
gboolean c_hash_table_try_steal_insert(cHashTable *cht, void *lookup_key, void *insert_key, int key_len, void *value)
{
    uint32_t ht_id = (*((uint32_t*)lookup_key)) % cht->sub_ht_num;
    g_rw_lock_writer_lock(&cht->lock[ht_id]);
    gboolean exist = g_hash_table_contains(cht->ht[ht_id], lookup_key);
    if(!exist)
    {
        memcpy(insert_key, lookup_key, key_len);
        g_hash_table_insert(cht->ht[ht_id], insert_key, value);
    }
    g_rw_lock_writer_unlock(&cht->lock[ht_id]);
    return exist;
}

/**
 * @description: Looks up a key in the cHashTable, returning the original key and the associated value and a #gboolean which is TRUE if the key was found.
 * @return { gboolean } : TRUE if the key was found in the cHashTable
 */
gboolean c_hash_table_lookup_extended(cHashTable *cht, void *lookup_key, void **ori_key, void **value)
{
    uint32_t ht_id = (*((uint32_t*)lookup_key)) % cht->sub_ht_num;
    g_rw_lock_reader_lock(&cht->lock[ht_id]);
    gboolean exist = g_hash_table_lookup_extended(cht->ht[ht_id], lookup_key, ori_key, value);
    g_rw_lock_reader_unlock(&cht->lock[ht_id]);
    return exist;
}

gboolean c_hash_table_contains(cHashTable *cht, void *key)
{
    uint32_t ht_id = (*((uint32_t*)key)) % cht->sub_ht_num;
    g_rw_lock_reader_lock(&cht->lock[ht_id]);
    gboolean exist = g_hash_table_contains(cht->ht[ht_id], key);
    g_rw_lock_reader_unlock(&cht->lock[ht_id]);
    return exist;
}

gpointer c_hash_table_lookup(cHashTable *cht, void *key)
{
    uint32_t ht_id = (*((uint32_t*)key)) % cht->sub_ht_num;
    g_rw_lock_reader_lock(&cht->lock[ht_id]);
    gpointer value = g_hash_table_lookup(cht->ht[ht_id], key);
    g_rw_lock_reader_unlock(&cht->lock[ht_id]);
    #ifdef DEBUG_MOTI
    if(value) {
        for(int i=0; i<FEATURE_NUM; i++)
        {
            if(con_tmp[i] == NULL)
            {
                con_tmp[i] = value;
                break;
            }
            else if(con_tmp[i] == value) break;
        }

        // for(int i=0; i<FEATURE_NUM*2; i++)
        // for(int i=0; i<FEATURE_NUM; i++)
        // {
        //     if(con_backup[i] == value) {
        //         hit_fea++;
        //         break;
        //     }
        // }

        // if(value == con_backup[0]) hit_fea++;
        // else con_backup[0] = value;

        int j;
        for(j=0; j<FEATURE_NUM; j++)
        {
            if(con_backup[j] == value) {
                hit_fea ++;
                break;
            }
        }
        if(j!=FEATURE_NUM)
        {
            for(int k=j; k<FEATURE_NUM-1; k++)
            {
                con_backup[k] = con_backup[k+1];
            }
        }
        else
        {
            for(int k=0; k<FEATURE_NUM-1; k++)
            {
                con_backup[k] = con_backup[k+1];
            }
        }
        con_backup[FEATURE_NUM-1] = value;

        // struct lru_item *p = lru_head;
        // for(int i=0; i<FEATURE_NUM; i++,p=p->next)
        // {
        //     if(value == p->data) {
        //         hit_fea ++;
        //         break;
        //     }
        // }
    }
    total_fea ++;
    #endif
    return value;
}

gboolean c_hash_table_remove(cHashTable *cht, void *key)
{
    uint32_t ht_id = (*((uint32_t*)key)) % cht->sub_ht_num;
    g_rw_lock_writer_lock(&cht->lock[ht_id]);
    gboolean exist = g_hash_table_remove(cht->ht[ht_id], key);
    g_rw_lock_writer_unlock(&cht->lock[ht_id]);
    return exist;
}