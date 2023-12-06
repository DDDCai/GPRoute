/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-11 04:22:21
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-12-01 15:12:03
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_DIS_CONFIG_H_
#define _INCLUDE_DIS_CONFIG_H_


#define ON 1
#define OFF 0

#define BOAFFT 0
#define GUIDEPOST 1
#define SIGMA BOAFFT
#define STATELESS BOAFFT


/* General */
#define EXP_SEGMENT_SIZE 1024
#define MIN_SEGMENT_SIZE (EXP_SEGMENT_SIZE/2)
#define MAX_SEGMENT_SIZE (EXP_SEGMENT_SIZE*2)   // number of chunks.
#define FP_LEN 6
#ifndef FEATURE_NUM
    #define FEATURE_NUM 8
#endif

#ifndef MAX_THREAD_POOL_SIZE
    #define MAX_THREAD_POOL_SIZE 16
#endif


/* Route */
#ifndef ROUTE_METHOD
    #define ROUTE_METHOD GUIDEPOST
#endif

#define USE_EPOLL ON



#if(ROUTE_METHOD==GUIDEPOST)
    #ifndef GP_SIZE
        #define GP_SIZE 512
    #endif
    #ifndef HIT_THRES
        #define HIT_THRES 1
    #endif
#endif


// #define DEBUG_MOTI

#endif