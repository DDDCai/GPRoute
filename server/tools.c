/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-06-17 12:24:46
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-07-28 13:17:47
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include "glib.h"
#include "dis_dedup.h"
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "xxhash.h"
#include <sys/time.h>

guint my_fplen_hash(gconstpointer p)
{
    return XXH32(p, FP_LEN, 5831);
}

gboolean my_fplen_equal(gconstpointer v1, gconstpointer v2)
{
    if(!memcmp(v1,v2,FP_LEN))   return TRUE;
    else return FALSE;
}

guint my_str_hash(gconstpointer p)
{
    guint hash = 5831;
    char *ptr = (char*)p;
    while(*ptr != '\0')
    {
        hash = (hash << 5) + hash + *ptr;
        ptr++;
    }

    return hash;
}

gboolean my_str_equal(gconstpointer v1, gconstpointer v2)
{
    if(!strcmp(v1, v2))   return TRUE;
    else return FALSE;
}

guint my_int64_hash(gconstpointer p)
{
    guint hash = 5831;
    char *ptr = (char*)p;
    for(int i=0; i<4; i++) hash = (hash << 5) + hash + ptr[i];
    return hash;
}

gboolean my_int64_equal(gconstpointer v1, gconstpointer v2)
{
    if(*((gint64*)v1) == *((gint64*)v2)) return TRUE;
    else return FALSE;
}

static boolean parse_addr(char *buf, int buf_size, char *ip, int ip_size, char *port, int port_size)
{
    char *ptr = buf;
    int i = 0;

    while((i<buf_size) && (*ptr==' '))
    {
        ptr ++;
        i ++;
    }
    if(i == buf_size) return FALSE;

    char *ip_ptr = ptr;
    int ip_len = 0;
    while((i<buf_size) && (*ptr!=':') && (*ptr!=' '))
    {
        ptr ++;
        i ++;
        ip_len ++;
    }
    if((i==buf_size) || (ip_len==ip_size-1)) return FALSE;
    memcpy(ip, ip_ptr, ip_len);
    ip[ip_len] = '\0';

    while((i<buf_size) && ((*ptr==':') || (*ptr==' ')))
    {
        ptr ++;
        i ++;
    }
    if(i == buf_size) return FALSE;

    char *port_ptr = ptr;
    int port_len = 0;
    while((i<buf_size) && (*ptr!='\n') && (*ptr!=' ') && (*ptr!='\0'))
    {
        ptr ++;
        i ++;
        port_len ++;
    }
    if((i==buf_size) || (port_len==port_size-1)) return FALSE;
    memcpy(port, port_ptr, port_len);
    port[port_len] = '\0';

    return TRUE;
}

boolean load_server_addr(char *addr_str, struct sockaddr_in *server_addr)
{
    memset(server_addr, 0, sizeof(struct sockaddr_in));
    char ip[512], port[512];
    if(!parse_addr(addr_str, strlen(addr_str)+1, ip, 512, port, 512))
    {
        printf("Fail to parse: %s\n", addr_str);
        return FALSE;
    }
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = inet_addr(ip);
    server_addr->sin_port = htons((uint16_t)atoi(port));
    return TRUE;
}

double get_time(struct timeval *start, struct timeval *end)
{
    time_t sub_sec = end->tv_sec - start->tv_sec;
    double sub_usec = (double)end->tv_usec - (double)start->tv_usec;
    double sub = (double)sub_sec + sub_usec/1000000.0;
    if(sub < 0) return 0;
    return sub;
}