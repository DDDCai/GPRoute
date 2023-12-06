/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-24 10:12:08
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-06-21 09:24:09
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_COMMUNICATE_H_
#define _INCLUDE_COMMUNICATE_H_

#include "dis_dedup.h"
#include <netinet/in.h>
#include <pthread.h>
#include <glib.h>
#include "message.h"

#define MAX_TCP_MESSAGE_SIZE 1460//1024//1460

void send_message(int fd, void *data, int size);
void recv_message(int fd, void *data, int size);

#endif