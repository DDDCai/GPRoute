/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-24 03:41:08
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-06-21 09:23:31
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include "dis_config.h"
#include "dis_dedup.h"
#include "communicate.h"
#include "message.h"

boolean recv_and_combine(int fd, uint8_t *buf, int recv_szie)
{
    ssize_t part, total = 0;
    while((part = recv(fd, buf+total, recv_szie, MSG_WAITALL))!=recv_szie-total)
    {
        if(part <= 0) 
        {
            if(part == 0) printf("Opposite closed\n");
            return FALSE;
        }
        total += part;
    }
    return TRUE;
}

void send_message(int fd, void *data, int size)
{
    if(send(fd, data, size, MSG_WAITALL)!=size)
    {
        printf("Fail to send message\n");
    }
}

void recv_message(int fd, void *data, int size)
{
    if(!recv_and_combine(fd, (uint8_t*)data, size))
    {
        printf("Fail to receive message\n");
    }
}