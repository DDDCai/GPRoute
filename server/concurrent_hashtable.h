/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-06-25 11:56:58
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-06-26 12:53:59
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */
#include <glib.h>
#include <stdint.h>

typedef struct _cHashTable cHashTable;

cHashTable *c_hash_table_new(GHashFunc hash_func, GEqualFunc key_equal_func, GDestroyNotify key_destroy_func, GDestroyNotify  value_destroy_func, int sub_table_num);
void c_hash_table_destroy(cHashTable *cht);
uint32_t c_hash_table_size(cHashTable *cht);
gboolean c_hash_table_insert(cHashTable *cht, void *key, void *value);
gboolean c_hash_table_try_insert(cHashTable *cht, void *key, void *value);
gboolean c_hash_table_try_copy_insert(cHashTable *cht, void *key, int key_len, void *value, int value_len);
gboolean c_hash_table_try_steal_insert(cHashTable *cht, void *lookup_key, void *insert_key, int key_len, void *value);
gboolean c_hash_table_lookup_extended(cHashTable *cht, void *lookup_key, void **ori_key, void **value);
gboolean c_hash_table_contains(cHashTable *cht, void *key);
gpointer c_hash_table_lookup(cHashTable *cht, void *key);
gboolean c_hash_table_remove(cHashTable *cht, void *key);