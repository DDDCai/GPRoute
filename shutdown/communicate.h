/*
 * @Author: Cai Deng
 * @Date: 2023-04-24 10:12:08
 * @LastEditors: Cai Deng
 * @LastEditTime: 2023-05-07 08:05:22
 * @Description: 
 */

#ifndef _INCLUDE_COMMUNICATE_H_
#define _INCLUDE_COMMUNICATE_H_

#include "dis_dedup.h"
#include <netinet/in.h>

#define MAX_TCP_MESSAGE_SIZE 1460

boolean socket_send_recv(struct sockaddr_in *dest_addr, void *send_data, int send_size, void *recv_data, int recv_size);

#endif