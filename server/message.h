/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-21 08:47:48
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-09-29 17:11:17
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_MESSAGE_H_
#define _INCLUDE_MESSAGE_H_

#include <stdint.h>
#include "dis_config.h"
#include "dis_dedup.h"
#include <netinet/in.h>
#include <sys/time.h>

#define M_SEGMENT_TYPE 0
#define M_RE_SEGMENT_TYPE 1
#define M_FEATURE_TYPE 2
#define M_RE_FEATURE_TYPE 3
#define M_EOF_TYPE 4
#define M_RE_EOF_TYPE 5
#define M_SHUTDOWN_TYPE 6
#define M_RE_SHUTDOWN_TYPE 7
#define M_RESEND_TYPE 8

typedef struct {
    uint8_t type;
} message_shutdown;

typedef struct {
    uint8_t type;
    boolean resend;
} message_resend;

typedef struct {
    uint8_t type;
    uint64_t phy_block, logic_block;
    uint64_t route_connec, fp_connec, re_gp;
    uint64_t route_volume, segment_volume, re_route_volume, re_segment_volume;
    uint64_t fea_tab_size;
    double total_time, fea_pro_time, fp_pro_time, fea_upd_time;
    #ifdef DEBUG_MOTI
    uint64_t total_fea, hit_fea;
    #endif
} message_re_shutdown;

typedef struct {
    uint8_t type;
    uint32_t seg_size;
    uint32_t fea_num;
    fingerprint fea[FEATURE_NUM];
    fingerprint fp[MAX_SEGMENT_SIZE];
} message_segment;

typedef struct {
    uint8_t type;
    uint32_t fp_num;
    uint64_t storage;
    fingerprint fp[MAX_SEGMENT_SIZE];
} message_re_segment;

#if(ROUTE_METHOD==GUIDEPOST)

typedef struct {
    uint8_t type;
    uint16_t hit;
    uint32_t fea_num;
    fingerprint fea[FEATURE_NUM];
} message_feature;

typedef struct {
    uint8_t type;
    uint16_t hit;
    uint64_t storage;
    #ifdef DEBUG_MOTI
    uint64_t fea_hit;
    #endif
    fingerprint gp[GP_SIZE];
} message_re_feature;

typedef struct {
    uint8_t type;
} message_eof;

#else   //  #if(ROUTE_METHOD==GUIDEPOST)

typedef struct {
    uint8_t type;
    uint32_t fea_num;
    fingerprint fea[FEATURE_NUM];
} message_feature;

typedef struct {
    uint8_t type;
    uint16_t hit;
    uint64_t storage;
} message_re_feature;

#endif  //  #if(ROUTE_METHOD==GUIDEPOST)

#define MAX_MESSAGE_SIZE (sizeof(message_segment)>sizeof(message_re_feature)?sizeof(message_segment):sizeof(message_re_feature))
#define MESSAGE_TYPE(m) (*((uint8_t*)m))

#endif  //  #ifndef _INCLUDE_MESSAGE_H_