/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-06-14 04:32:59
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-07-28 09:17:39
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include "dis_dedup.h"
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "glib.h"
#include "xxhash.h"
#include <sys/time.h>

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

void load_server_addr(char *addr_file_path, struct sockaddr_in *server_addr, int addr_num)
{
    FILE *addr_file;
    int addr_count = 0;
    char buf[1024], ip[512], port[512];
    
    memset(server_addr, 0, sizeof(struct sockaddr_in)*addr_num);
    if( !(addr_file = fopen(addr_file_path, "rb")) )
    {
        printf("Fail to load remote addresses.\n");
        fclose(addr_file);
        exit(1);
    }
    while(fgets(buf, 1024, addr_file))
    {
        if(!parse_addr(buf, 1024, ip, 512, port, 512))
        {
            printf("Fail to parse: %s\n", buf);
            fclose(addr_file);
            exit(1);
        }
        server_addr[addr_count].sin_family = AF_INET;
        server_addr[addr_count].sin_port = htons((uint16_t)atoi(port));
        server_addr[addr_count].sin_addr.s_addr = inet_addr(ip);
        addr_count ++;
        if(addr_count == addr_num) break;
    }
    if(addr_count < addr_num)
    {
        printf("Remote address not enough\n");
        fclose(addr_file);
        exit(1);
    }
    fclose(addr_file);
}

guint my_fplen_hash(gconstpointer p)
{
    return XXH32(p, FP_LEN, 5831);
}

gboolean my_fplen_equal(gconstpointer v1, gconstpointer v2)
{
    if(!memcmp(v1,v2,FP_LEN))   return TRUE;
    else return FALSE;
}

double get_time(struct timeval *start, struct timeval *end)
{
    time_t sub_sec = end->tv_sec - start->tv_sec;
    double sub_usec = (double)end->tv_usec - (double)start->tv_usec;
    double sub = (double)sub_sec + sub_usec/1000000.0;
    if(sub < 0) return 0;
    return sub;
}