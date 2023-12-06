/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-25 04:44:13
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-09-14 02:52:53
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include <netinet/in.h>
#include "message.h"
#include "communicate.h"

extern int seg_fd[NODE_NUM];
extern uint64_t storage[NODE_NUM];

void remote_dedup(message_segment *segment, int id)
{
    send_message(seg_fd[id], segment, sizeof(message_segment)-sizeof_xfp(MAX_SEGMENT_SIZE)+sizeof_xfp(segment->seg_size));

    message_re_segment buf;
    recv_message(seg_fd[id], &buf, sizeof(message_re_segment)-sizeof_xfp(MAX_SEGMENT_SIZE));
    if(buf.fp_num > 0) {
        recv_message(seg_fd[id], (void*)&buf+sizeof(message_re_segment)-sizeof_xfp(MAX_SEGMENT_SIZE), sizeof_xfp(buf.fp_num));
    }
    #if(ROUTE_METHOD==GUIDEPOST)
    storage[id] = buf.storage;
    #endif
}