/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-27 02:11:37
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-09-14 06:48:52
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include "glib.h"
#include <stdlib.h>
#include "message.h"
#include "dis_dedup.h"
#include <stdio.h>
#include "concurrent_hashtable.h"

extern cHashTable *fp_table;

int dedup_process(message_segment *segment, uint8_t *re_buf)
{
    message_re_segment *re = (message_re_segment*)re_buf;
    re->fp_num = 0;
    re->type = M_RE_SEGMENT_TYPE;
    for(int i=0; i<segment->seg_size; i++)
    {
        if(!c_hash_table_try_copy_insert(fp_table, segment->fp[i], FP_LEN, NULL, 0)) {
            fpcpy(re->fp[re->fp_num], segment->fp[i], 1);
            re->fp_num ++;
        }
    }
    re->storage = c_hash_table_size(fp_table);
    return sizeof(message_re_segment)-sizeof_xfp(MAX_SEGMENT_SIZE)+sizeof_xfp(re->fp_num);
}