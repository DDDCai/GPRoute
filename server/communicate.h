/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-26 04:31:09
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-06-20 08:22:52
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#ifndef _INCLUDE_COMMUNICATE_H_
#define _INCLUDE_COMMUNICATE_h_

#include <netinet/in.h>
#include "dis_dedup.h"
#include <sys/epoll.h>

#define MAX_CONNECTION_NUM 1024
#define MAX_TCP_MESSAGE_SIZE 1460

boolean init_communication(struct sockaddr_in *server_addr, int *socket_fd, int *epoll_fd);
int receive_message(int connect_fd, uint8_t *buf);
boolean send_message(int connect_fd, uint8_t *buf, int size);


#endif