/*
 * @Author: Cai Deng
 * @Date: 2023-04-24 03:41:08
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-08-15 08:38:14
 * @Description: 
 */

#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include "dis_config.h"
#include "dis_dedup.h"
#include "communicate.h"
#include "message.h"
#include "errno.h"

ssize_t recv_and_combine(int fd, uint8_t *buf, int recv_size)
{
    ssize_t part, total = 0;
    while((part = recv(fd, buf+total, recv_size, MSG_WAITALL))!=recv_size-total)
    {
        if(part <= 0)
        {
            if(part == 0) printf("Opposite closed\n");
            return total;
        }
        total += part;
    }
    total += part;
    return total;
}

boolean socket_send_recv(struct sockaddr_in *dest_addr, void *send_data, int send_size, void *recv_data, int recv_size)
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    int connect_times = 0;
    while(connect(sock_fd, (struct sockaddr*)dest_addr, sizeof(struct sockaddr_in)) < 0)
    {
        connect_times ++;
        if(connect_times == 1)
        {
            printf("Fail to connect %u:%u due to %d\n", dest_addr->sin_addr.s_addr, ntohs(dest_addr->sin_port), errno);
            close(sock_fd);
            return FALSE;
        }
    }
    if(send(sock_fd, send_data, send_size, MSG_WAITALL) < 0)
    {
        printf("Fail to send features to %u:%u\n", dest_addr->sin_addr.s_addr, dest_addr->sin_port);
        close(sock_fd);
        return FALSE;
    }
    if(recv_data && (recv_and_combine(sock_fd, recv_data, recv_size)!=recv_size))
    {
        printf("Fail to receive feature result from %u:%u\n", dest_addr->sin_addr.s_addr, dest_addr->sin_port);
        close(sock_fd);
        return FALSE;
    }
    close(sock_fd);
    return TRUE;
}