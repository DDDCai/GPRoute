/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-25 04:52:35
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-06-20 04:17:34
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_DEDUP_H_
#define _INCLUDE_DEDUP_H_

#include "message.h"
#include <netinet/in.h>

void remote_dedup(message_segment *segment, int id);

#endif