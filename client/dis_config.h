/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-11 04:22:21
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-12-01 10:12:04
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_DIS_CONFIG_H_
#define _INCLUDE_DIS_CONFIG_H_


#define ON 1
#define OFF 0

#define FINESSE_FEATURE 0
#define MINK_FEATURE 1
#define FIRST_FEATURE 2
#define NTRANS_FEATURE 3
#define CDMINK_FEATURE 4
#define SIGMA_FEATURE 5

#define BOAFFT 0
#define GUIDEPOST 1
#define SIGMA 2
#define STATELESS 3


/* General */
#ifndef EXP_SEGMENT_SIZE
    #define EXP_SEGMENT_SIZE 1024
#endif
#define MIN_SEGMENT_SIZE (EXP_SEGMENT_SIZE/2)
#define MAX_SEGMENT_SIZE (EXP_SEGMENT_SIZE*2)   // number of chunks.
#define FP_LEN 6
#ifndef FEATURE_NUM
    #define FEATURE_NUM 8
#endif
#ifndef NODE_NUM
    #define NODE_NUM 32
#endif

#ifndef SKEW_THRES
    #define SKEW_THRES 1.05
#endif
#define USE_EPOLL ON
#ifndef SSDEDUP
    #define SSDEDUP OFF
#endif



// #define DEBUG_MOTI
// #define PREDICTION_EVA





/* Route */
#ifndef ROUTE_METHOD
    #define ROUTE_METHOD 1
#endif

#if(ROUTE_METHOD==GUIDEPOST)
    #ifndef GP_SIZE
        #define GP_SIZE 512
    #endif
    #ifndef GP_PART
        #define GP_PART 4
    #endif
    #ifndef HIT_THRES
        #define HIT_THRES 1
    #endif
    #ifndef FEATURE_METHOD
        #define FEATURE_METHOD FINESSE_FEATURE
    #endif
#elif(ROUTE_METHOD==BOAFFT)
    #ifndef FEATURE_METHOD
        #define FEATURE_METHOD FINESSE_FEATURE
    #endif
#elif(ROUTE_METHOD==SIGMA)
    #ifndef FEATURE_METHOD
        #define FEATURE_METHOD MINK_FEATURE
    #endif
#elif(ROUTE_METHOD==STATELESS)
    #ifndef FEATURE_METHOD
        #define FEATURE_METHOD FIRST_FEATURE
    #endif
#endif
#if(SSDEDUP==ON)
    #ifndef FEATURE_METHOD
        #define FEATURE_METHOD FINESSE_FEATURE
    #endif
    #ifndef LCACHE_SIZE
        #define LCACHE_SIZE (2048)
    #endif
    #ifndef LC_HIT_THRES
        #define LC_HIT_THRES 1
    #endif
#endif


#endif