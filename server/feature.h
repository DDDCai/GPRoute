/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-26 05:46:54
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-06-26 13:36:55
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_FEATURE_H_
#define _INCLUDE_FEATURE_H_

#include "message.h"
#include "glib.h"
#include "dis_dedup.h"

int feature_process(message_feature *feature, uint8_t *re_buf);
void update_feature_index(message_segment *segment, int fd);

#endif