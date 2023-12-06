/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-21 10:05:46
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-06-26 08:03:09
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include "message.h"
#include "glib.h"
#include "feature.h"

#define CHECK_BOUNDARY(ptr, mask) (!(*((guint32*)(ptr))&mask))


boolean new_message_segment(trace_meta *trace, message_segment *segment, message_feature *feature)
{
    memset(segment, 0, sizeof(message_segment));
    uint32_t seg_size = 0;
    int ret;
    guint32 chunking_mask = MIN_SEGMENT_SIZE - 1;

    while(1)
    {
        while(!trace->ci)
        {
            if((ret = hashfile_next_file(trace->handle)) <= 0) goto _finish_segmenting;
            trace->ci = hashfile_next_chunk(trace->handle);
        }
        fpcpy(segment->fp[seg_size], trace->ci->hash, 1);
        seg_size ++;
        trace->ci = hashfile_next_chunk(trace->handle);
        if(seg_size < MIN_SEGMENT_SIZE) continue;
        else if(seg_size < MAX_SEGMENT_SIZE)
        {
            if(CHECK_BOUNDARY(segment->fp[seg_size-1], chunking_mask)) break;
        }
        else break;
    }

 _finish_segmenting:
    if(seg_size == 0)
    {
        return FALSE;
    }
    segment->type = M_SEGMENT_TYPE;
    segment->seg_size = seg_size;
    new_message_feature(segment, feature);
    fpcpy(segment->fea, feature->fea, feature->fea_num);
    segment->fea_num = feature->fea_num;
    return TRUE;
}