/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-21 10:10:09
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-04-24 06:42:11
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_SEGMENT_H_
#define _INCLUDE_SEGMENT_H_

#include "dis_dedup.h"
#include "message.h"

boolean new_message_segment(trace_meta *trace, message_segment *segment, message_feature *feature);

#endif