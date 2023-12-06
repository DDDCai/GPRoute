/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-26 03:14:08
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-06-27 09:33:39
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include <netinet/in.h>
#include "dis_dedup.h"
#include "dis_config.h"
#include "message.h"
#include "communicate.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

boolean init_communication(struct sockaddr_in *server_addr, int *socket_fd, int *epoll_fd)
{
    *socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    setsockopt(*socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(*socket_fd, F_SETFL, fcntl(*socket_fd, F_GETFL)|O_NONBLOCK);
    if(bind(*socket_fd, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in)) == -1)
    {
        printf("Fail to bind address.\n");
        return FALSE;
    }
    if(listen(*socket_fd, MAX_CONNECTION_NUM) == -1)
    {
        printf("Fail to listen socket.\n");
        return FALSE;
    }
    *epoll_fd = epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = *socket_fd;
    if(epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, *socket_fd, &ev) == -1)
    {
        printf("Fail to add epoll.\n");
        return FALSE;
    }
    return TRUE;
}

#define loop_recieve()                                                      \
    total += part;                                                          \
    while(1)                                                                \
    {                                                                       \
        part = recv(connect_fd, buf+total, m_size, 0);                      \
        if(part == m_size) break;                                           \
        if(part == 0 || (part < 0 && errno != EAGAIN)) goto _escape_loop;   \
        if(part > 0) {                                                      \
            total += part;                                                  \
            m_size -= part;                                                 \
        }                                                                   \
    }

int receive_message(int connect_fd, uint8_t *buf)
{
    ssize_t total = 0, m_size;
    ssize_t part = recv(connect_fd, buf, 1, 0);
    if(part == 1)
    {
        switch(MESSAGE_TYPE(buf))
        {
          #if(ROUTE_METHOD==GUIDEPOST)
            case M_EOF_TYPE:
          #endif
            case M_SHUTDOWN_TYPE:
                return 1;
            case M_FEATURE_TYPE:
                m_size = sizeof(message_feature) - part;
                loop_recieve();
                break;
            case M_SEGMENT_TYPE:
                m_size = sizeof(message_segment) - part - sizeof_xfp(MAX_SEGMENT_SIZE);
                loop_recieve();
                m_size = sizeof_xfp(((message_segment*)buf)->seg_size);
                loop_recieve();
                break;
            default:
                printf("err type: %d\n",MESSAGE_TYPE(buf));
                return -2;
        }
        return 1;
    }
  _escape_loop:
    if(part == 0)
    {
        struct sockaddr_in client_addr;
        char addr_str[INET_ADDRSTRLEN];
        int addr_len = sizeof(client_addr);
        getpeername(connect_fd, (struct sockaddr*)&client_addr, &addr_len);
        printf("Opposite closed (%s:%d)\n", inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str)), ntohs(client_addr.sin_port));
        return 0;
    }
    if(errno == EAGAIN) return -1;
    else {
        printf("Fail to recieve message due to err %d\n", errno);
        return -3;
    }
}

boolean send_message(int connect_fd, uint8_t *buf, int size)
{
    if(send(connect_fd, buf, size, 0)!=size)
    {
        printf("Fail to send message\n");
        return FALSE;
    }
    return TRUE;
}