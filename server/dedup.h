/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-27 02:22:18
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-07-30 11:53:10
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_DEDUP_H_
#define _INCLUDE_DEDUP_H_

#include "message.h"
#include "glib.h"
#include "dis_dedup.h"

int dedup_process(message_segment *segment, uint8_t *re_buf);

#endif