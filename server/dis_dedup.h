/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-21 09:40:14
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-07-30 12:01:50
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_DIS_DEDUP_H_
#define _INCLUDE_DIS_DEDUP_H_

#include <stdint.h>
#include <string.h>
#include "dis_config.h"
#include <pthread.h>


typedef uint8_t fingerprint[FP_LEN];

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif
typedef int boolean;
typedef pthread_mutex_t p_mutex;


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

#define _anti_exclusive_exec(mutex, action) \
    pthread_mutex_unlock(mutex);            \
    action;                                 \
    pthread_mutex_lock(mutex)               

#define _readlock_execution(lock, action) \
    g_rw_lock_reader_lock(lock);          \
    action;                               \
    g_rw_lock_reader_unlock(lock)      

#define _writelock_execution(lock, action)  \
    g_rw_lock_writer_lock(lock);            \
    action;                                 \
    g_rw_lock_writer_unlock(lock)

#define _anti_writelock_exec(lock, action)  \
    g_rw_lock_writer_unlock(lock);          \
    action;                                 \
    g_rw_lock_writer_lock(lock);

#define _g_timer_count(timer, time, action) \
    g_timer_start(timer);                   \
    action;                                 \
    time += g_timer_elapsed(timer, NULL)

#if(ROUTE_METHOD==GUIDEPOST)
typedef struct {
    uint32_t used_slot;
    fingerprint *gp_data;
} fea_container;
#endif

#endif