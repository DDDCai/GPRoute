/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-21 09:40:14
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-07-31 06:23:46
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_DIS_DEDUP_H_
#define _INCLUDE_DIS_DEDUP_H_

#include <stdint.h>
#include <string.h>
#include "dis_config.h"
#include "libhashfile.h"

typedef struct {
    struct hashfile_handle *handle;
    const struct chunk_info *ci;
} trace_meta;

typedef uint8_t fingerprint[FP_LEN];

#define FALSE 0
#define TRUE (!FALSE)
typedef int boolean;


#define MAX_FILE_NAME_LEN 256


#define sizeof_xfp(x) ((x)*FP_LEN)
#define fpcpy(dest, src, fp_num) memcpy(dest, src, sizeof_xfp(fp_num))

#define PUT_3_STRS_TOGETHER(des,src1,src2,src3) \
    strcpy(des,src1);                           \
    strcat(des,src2);                           \
    strcat(des,src3)                           

#define _exclusive_execution(mutex, action) \
    pthread_mutex_lock(mutex);              \
    action;                                 \
    pthread_mutex_unlock(mutex)

#define _g_timer_count(timer, time, action) \
    g_timer_start(timer);                   \
    action                                  \
    time += g_timer_elapsed(timer, NULL)

#if(ROUTE_METHOD==GUIDEPOST)
typedef fingerprint gp_buf[GP_SIZE];
#endif

#endif