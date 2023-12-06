/*
 * @Author: Cai Deng
 * @Date: 2023-04-27 03:11:42
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-09-29 12:02:16
 * @Description: 
 */

#include "message.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "communicate.h"

boolean parse_addr(char *buf, int buf_size, char *ip, int ip_size, char *port, int port_size)
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

boolean load_server_addr(char *addr_file_path, struct sockaddr_in *server_addr, int addr_num)
{
    FILE *addr_file;
    int addr_count = 0;
    char buf[1024], ip[512], port[512];
    
    if( !(addr_file = fopen(addr_file_path, "rb")) )
    {
        printf("Fail to load remote addresses.\n");
        fclose(addr_file);
        return FALSE;
    }
    while(fgets(buf, 1024, addr_file))
    {
        if(!parse_addr(buf, 1024, ip, 512, port, 512))
        {
            printf("Fail to parse: %s\n", buf);
            fclose(addr_file);
            return FALSE;
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
        return FALSE;
    }
    fclose(addr_file);
    return TRUE;
}

void main(int argc, char *argv[])
{
    struct sockaddr_in server_addr[NODE_NUM];
    memset(server_addr, 0, sizeof(server_addr));
    if(!load_server_addr(argv[1], server_addr, NODE_NUM)) return;

    uint64_t phy_block = 0, logic_block = 0, route_connec = 0, route_volume = 0, fea_tab_size = 0, re_route_volume = 0, segment_volume = 0, re_segment_volume = 0, fp_connec = 0, re_gp = 0;
    uint64_t max_storage = 0;
    double total_time = 0, fea_pro_time = 0, fp_pro_time = 0, fea_upd_time = 0;
    #ifdef DEBUG_MOTI
    uint64_t total_fea = 0, hit_fea = 0;
    #endif

    message_shutdown send_buf = {M_SHUTDOWN_TYPE};
    message_re_shutdown recv_buf[NODE_NUM];
    for(int i=0; i<NODE_NUM; i++)
    {
        socket_send_recv(&server_addr[i], &send_buf, sizeof(message_shutdown), &recv_buf[i], sizeof(message_re_shutdown));
        phy_block += recv_buf[i].phy_block;
        logic_block += recv_buf[i].logic_block;
        route_connec += recv_buf[i].route_connec;
        route_volume += recv_buf[i].route_volume;
        fea_tab_size += recv_buf[i].fea_tab_size;
        total_time += recv_buf[i].total_time;
        re_route_volume += recv_buf[i].re_route_volume;
        segment_volume += recv_buf[i].segment_volume;
        re_segment_volume += recv_buf[i].re_segment_volume;
        fp_connec += recv_buf[i].fp_connec;
        fea_pro_time += recv_buf[i].fea_pro_time;
        fp_pro_time += recv_buf[i].fp_pro_time;
        fea_upd_time += recv_buf[i].fea_upd_time;
        re_gp += recv_buf[i].re_gp;
        if(recv_buf[i].phy_block > max_storage) max_storage = recv_buf[i].phy_block;
        #ifdef DEBUG_MOTI
        total_fea += recv_buf[i].total_fea;
        hit_fea += recv_buf[i].hit_fea;
        #endif
        sleep(1);
    }
    for(int i=0; i<NODE_NUM; i++)
    {
        socket_send_recv(&server_addr[i], &send_buf, sizeof(message_shutdown), NULL, 0);
    }

    FILE *fp = fopen(argv[2], "ab");
    fprintf(fp, "==============================================\n");
    fprintf(fp, "\
        total block: %lu\n\
        dedup ratio: %.2f\n\
        route conne: %lu\n\
        segmt conne: %lu\n\
        route volum: %lu MB = %lu fea + %lu re\n\
        segmt volum: %lu MB = %lu seg + %lu re\n\
        featab size: %lu\n"
        ,logic_block
        ,logic_block/(double)phy_block
        ,route_connec
        ,fp_connec
        ,(route_volume+re_route_volume)/1024/1024,route_volume/1024/1024,re_route_volume/1024/1024
        ,(segment_volume+re_segment_volume)/1024/1024,segment_volume/1024/1024,re_segment_volume/1024/1024
        ,fea_tab_size
    );
    fprintf(fp, "\
        physic block: %lu\n"
        ,phy_block
    );
    fprintf(fp, "\
        Data Skew: %.2f\n\
        Effect DR: %.2f\n\
        Time(min): %.2f\n\
        Fea_pro_T: %.2f\n\
        Fp_pro_T : %.2f\n\
        Fea_upd_T: %.2f\n"
        ,max_storage/(phy_block/(double)NODE_NUM)
        ,logic_block/(double)phy_block/(max_storage/(phy_block/(double)NODE_NUM))
        ,total_time/60/NODE_NUM
        ,fea_pro_time/60/NODE_NUM
        ,fp_pro_time/60
        ,fea_upd_time/60
    );
    fprintf(fp, "\
        RE_GP conec: %lu\n\
            RE_GP Rate(\%): %.2f\n"
        ,re_gp
        ,(double)re_gp/route_connec*100.0
    );
    #ifdef DEBUG_MOTI
    fprintf(fp, "\
        Total fea: %lu\n\
        Hit fea  : %lu\n\
            Hit Rate(\%): %.2f\n"
        ,total_fea
        ,hit_fea
        ,(double)hit_fea/total_fea*100.0
    );
    #endif
    fprintf(fp, "\n\n\n");
    fclose(fp);
}