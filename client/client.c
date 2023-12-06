/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-21 05:39:52
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-07-31 05:45:00
 * @Description: 
 * Copyright (c) 2023 by DDDCai dengcaidengcai@163.com, All Rights Reserved. 
 */

#include <stdio.h>
#include <stdlib.h>
#include "libhashfile.h"
#include "dis_dedup.h"
#include "message.h"
#include <netinet/in.h>
#include "segment.h"
#include "route.h"
#include "dedup.h"
#include "communicate.h"
#include <pthread.h>

extern int fea_fd[NODE_NUM];
extern double msg_total_time, fea_total_time, seg_total_time;

static boolean new_trace_meta(char *file_path, trace_meta *fm)
{
    fm->handle = hashfile_open(file_path);
    if(!fm->handle) {
        printf("Fail to open file : %s\n", file_path);
        return FALSE;
    }
    fm->ci = NULL;
    return TRUE;
}

static void close_trace_meta(trace_meta *fm)
{
    hashfile_close(fm->handle);
}

// #define USE_PIPELINE
#ifdef USE_PIPELINE

#define PIPE_NUM 4
#define MSG_PIPE 0
#define FEA_PIPE 1
#define SEG_PIPE 2
#define DAT_PIPE 3
message_feature fea_m_buf[PIPE_NUM+2];
message_segment seg_m_buf[PIPE_NUM+2];
int route_id[PIPE_NUM+2];
struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    boolean full, end;
    int data;
} pipe_channel[PIPE_NUM-1];

static void put_data_into_pipe_channel(int channel_id, int slot_id)
{
    _exclusive_execution(&pipe_channel[channel_id].mutex,
        if(pipe_channel[channel_id].full)
            pthread_cond_wait(&pipe_channel[channel_id].cond, &pipe_channel[channel_id].mutex);
        pipe_channel[channel_id].full = TRUE;
        pipe_channel[channel_id].data = slot_id;
    );
    pthread_cond_signal(&pipe_channel[channel_id].cond);
}

static int get_data_from_pipe_channel(int channel_id)
{
    _exclusive_execution(&pipe_channel[channel_id].mutex,
        while(!pipe_channel[channel_id].full)
        {
            if(pipe_channel[channel_id].end) return -1;
            pthread_cond_wait(&pipe_channel[channel_id].cond, &pipe_channel[channel_id].mutex);
        }
        pipe_channel[channel_id].full = FALSE;
        int slot_id = pipe_channel[channel_id].data;
    );
    pthread_cond_signal(&pipe_channel[channel_id].cond);
    return slot_id;
}

static void end_pipe_channel(int channel_id)
{
    _exclusive_execution(&pipe_channel[channel_id].mutex,
        pipe_channel[channel_id].end = TRUE;
    );
    pthread_cond_signal(&pipe_channel[channel_id].cond);
}

void* msg_thread(void *parameter)
{
    trace_meta *tm = (trace_meta*)parameter;
    int slot_id = 0;
    while(1)
    {
        g_timer_start(msg_timer);
        if(!new_message_segment(tm, &seg_m_buf[slot_id], &fea_m_buf[slot_id])) break;
        msg_total_time += g_timer_elapsed(msg_timer, NULL);
        put_data_into_pipe_channel(MSG_PIPE, slot_id);
        slot_id = (slot_id + 1) % (PIPE_NUM + 2);
    }
    end_pipe_channel(MSG_PIPE);
    return 0;
}

void* fea_thread(void *parameter)
{
    int slot_id;
    while(1)
    {
        slot_id = get_data_from_pipe_channel(MSG_PIPE);
        if(slot_id < 0) {
            end_pipe_channel(FEA_PIPE);
            return 0;
        }
        g_timer_start(fea_timer);
        route_id[slot_id] = route(&fea_m_buf[slot_id]);
        fea_total_time += g_timer_elapsed(fea_timer, NULL);
        put_data_into_pipe_channel(FEA_PIPE, slot_id);
    }
}

void* seg_thread(void *parameter)
{
    int slot_id;
    while(1)
    {
        slot_id = get_data_from_pipe_channel(FEA_PIPE);
        if(slot_id < 0) {
            end_pipe_channel(SEG_PIPE);
            return 0;
        }
        g_timer_start(seg_timer);
        remote_dedup(&seg_m_buf[slot_id], route_id[slot_id]);
        seg_total_time += g_timer_elapsed(seg_timer, NULL);
    }
}

static void init_pipe_channel()
{
    for(int i=0; i<PIPE_NUM-1; i++)
    {
        pthread_mutex_init(&pipe_channel[i].mutex, NULL);
        pthread_cond_init(&pipe_channel[i].cond, NULL);
        pipe_channel[i].full = FALSE;
        pipe_channel[i].end = FALSE;
        pipe_channel[i].data = -1;
    }
}

static void destroy_pipe_channel()
{
    for(int i=0; i<PIPE_NUM-1; i++)
    {
        pthread_mutex_destroy(&pipe_channel[i].mutex);
        pthread_cond_destroy(&pipe_channel[i].cond);
    }
}

boolean client_dedup_a_file(char *file_path)
{
    trace_meta tm;
    if(!new_trace_meta(file_path, &tm)) return FALSE;
    init_pipe_channel();

    pthread_t tid[PIPE_NUM];
    pthread_create(&tid[MSG_PIPE], NULL, msg_thread, &tm);
    pthread_create(&tid[FEA_PIPE], NULL, fea_thread, NULL);
    pthread_create(&tid[SEG_PIPE], NULL, seg_thread, NULL);
    
    pthread_join(tid[MSG_PIPE], NULL);
    pthread_join(tid[FEA_PIPE], NULL);
    pthread_join(tid[SEG_PIPE], NULL);

    #if(ROUTE_METHOD==GUIDEPOST)
    send_eof();
    #endif

    destroy_pipe_channel();
    close_trace_meta(&tm);

    return TRUE;
}

#else   //  #ifdef USE_PIPELINE

boolean client_dedup_a_file(char *file_path)
{
    trace_meta tm;
    message_segment segment;
    message_feature feature;
    if(!new_trace_meta(file_path, &tm)) return FALSE;
    GTimer  *msg_timer = g_timer_new(), 
            *fea_timer = g_timer_new(), 
            *seg_timer = g_timer_new();
    while(1)
    {
        _g_timer_count(msg_timer, msg_total_time,
            if(!new_message_segment(&tm, &segment, &feature)) break;
        );
        _g_timer_count(fea_timer, fea_total_time,
            int route_res = route(&feature);
        );
        _g_timer_count(seg_timer, seg_total_time,
            remote_dedup(&segment, route_res);
        );
    }
    g_timer_destroy(msg_timer);
    g_timer_destroy(fea_timer);
    g_timer_destroy(seg_timer);
    close_trace_meta(&tm);
    return TRUE;
}

#endif  //  #ifdef USE_PIPELINE 