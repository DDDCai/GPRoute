/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-24 05:32:56
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-07-22 14:34:25
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include "dis_config.h"
#include "dis_dedup.h"
#include "message.h"
#include <glib.h>
#include <string.h>
#include <stdlib.h>

static boolean if_fp_bigger(fingerprint a, fingerprint b)
{
    for(int i=0; i<FP_LEN; i++)
    {
        if(a[i] > b[i]) return TRUE;
        else if(a[i] < b[i]) return FALSE;
    }
    return FALSE;
}

static void quick_min_k_fp(fingerprint *buf, int fp_num, int k)
{
    int position = 0;
    fingerprint tmp;
    fpcpy(tmp, buf[0], 1);

    for(int i=1; i<fp_num; i++)
    {
        if(if_fp_bigger(tmp, buf[i]))
        {
            fpcpy(buf[position], buf[i], 1);
            fpcpy(buf[i], buf[position+1], 1);
            position ++;
        }
    }
    fpcpy(buf[position], tmp, 1);

    if((position+1==k) || (position==k)) 
        return;
    else if(position+1>k) 
        quick_min_k_fp(buf, position, k);
    else 
        quick_min_k_fp(buf+position+1, fp_num-position-1, k-position-1);
}

static void get_min_k_fp(fingerprint *src, int fp_num, fingerprint *dst, int k)
{
    if(fp_num < k) return;
    fingerprint fp_buf[MAX_SEGMENT_SIZE];
    fpcpy(fp_buf, src, fp_num);
    quick_min_k_fp(fp_buf, fp_num, k);
    fpcpy(dst, fp_buf, k);
}

#if(FEATURE_METHOD == FINESSE_FEATURE)
static void finesse_style(message_feature *feature, message_segment *segment)
{
    if(segment->seg_size <= FEATURE_NUM)
    {
        fpcpy(feature->fea, segment->fp, segment->seg_size);
        feature->fea_num = segment->seg_size;
    }
    else
    {
        int sub_seg_size = segment->seg_size / FEATURE_NUM;
        int i;
        for(i=0; i<FEATURE_NUM-1; i++)
        {
            get_min_k_fp(segment->fp+i*sub_seg_size, sub_seg_size, feature->fea+i, 1);
        }
        get_min_k_fp(segment->fp+i*sub_seg_size, segment->seg_size-i*sub_seg_size, feature->fea+i, 1);
        feature->fea_num = FEATURE_NUM;
    }
}
#elif(FEATURE_METHOD == MINK_FEATURE || FEATURE_METHOD == SIGMA_FEATURE)
static void mink_style(message_feature *feature, message_segment *segment)
{
    if(segment->seg_size <= FEATURE_NUM)
    {
        fpcpy(feature->fea, segment->fp, segment->seg_size);
        feature->fea_num = segment->seg_size;
    }
    else
    {
        get_min_k_fp(segment->fp, segment->seg_size, feature->fea, FEATURE_NUM);
        feature->fea_num = FEATURE_NUM;
    }
}
#elif(FEATURE_METHOD == NTRANS_FEATURE)
guint64 K[] = {
    0x76931fac9dab2b36, 0xc248b87d6ae33f9a, 0x62d7183a5d5789e4, 0xb2d6b441e2411dc7, 
    0x09e111c7e1e7acb6, 0xf8cac0bb2fc4c8bc, 0x2ae3baaab9165cc4, 0x58e199cb89f51b13, 
    0x5f7091a5abb0874d, 0xf3e8cb4543a5eb93, 0xb0441e9ca4c2b0fb, 0x3d30875cbf29abd5, 
    0xb1acf38984b35ae8, 0x82809dd4cfe7abc5, 0xc61baa52e053b4c3, 0x643f204ef259d2e9, 
    0x8042a948aac5e884, 0xcb3ec7db925643fd, 0x34fdd467e2cca406, 0x035cb2744cb90a63, 
    0xe51c973790334394, 0x7e02086541e4c48a, 0x99630aa9aece1538, 0x43a4b190274ebc95, 
    0x5f8592e30a2205a4, 0x85846248987550aa, 0xf2094ec59e7931dc, 0x650c7451cc61c0cb, 
    0x2c46a1b3f2c349fa, 0xff763c7f8d14ddff, 0x946351744378d62c, 0x59285a8d7915614f, 
    0x5a2ac9e0d68aca62, 0x48a9227ab8f1930e, 0xe38ac7a9d239c9b0, 0x26a481e49d53161f, 
    0x9a9513fe5271c32e, 0x9c21d156eb9f1bea, 0x57f6ae4f1b1de3b7, 0xfd9cee2d9cca7b4c, 
    0x242d26c31d000b7f, 0x90b7fe48a131c7de, 0xbfbe58165266de56, 0xe1edf26939af07ec, 
    0x69ab1b17d8db6214, 0x3f2228b51551c3d2, 0xc7de3f5072bd4d18, 0xc3aeb64cb9e8cba8,
    0x1a0f3783ef9012db, 0x00a903566bce3501, 0xd2223908bccfe509, 0x5903acde8fd7ab31,
    0x935db607ea31258f, 0xe90788fdac21bd00, 0x235ad90b73c1e502, 0xe547f90ac56b73a2,
    0xa9073451a897d342, 0xc1d23f55690bb5a1, 0x3392b830b514a6f5, 0x6aaa890d35f0ff59,
    0x763fcba8bd62469f, 0x4fdb4529602ad675, 0x8f8263b034fadbc7, 0xf83bd098236ac562
};

guint64 B[] = {
    0x38667b6ed2b2fcab, 0x04abae8676e318b4, 0x02a7d15b30d2d7dd, 0xb78650cc6af82bc3, 
    0xd7aa805b02dd9aa5, 0x23b7374a1323ee6b, 0x516d1b81e5f709c2, 0xc790edaf1c3fa9b0, 
    0xa1dbc6dabc2b5ed2, 0x67244c458752002b, 0x106d6381fad58a7e, 0x193657bde0fe0291, 
    0x20f8379316891f82, 0x8b8d24a049e5b86d, 0x855bcfed56765f9d, 0xa1ac54caeaf9257a, 
    0xbc67b451bc70b0e5, 0x2817dd1b704a6b41, 0x8a83fd4a9ca4c89e, 0x1a6e779f8d9e9df1, 
    0x8747591e5b314c05, 0x763edcd59632423c, 0xa83f14d6f073d784, 0xdb2b7001643a6760, 
    0xf9f0dd6ddd0a59e2, 0x41dc1ed720287896, 0x286f5cc3addf6c1a, 0xdf6ed35f477b0022, 
    0x981e5e1fbfe1bfb8, 0xe26b5ba93253275b, 0xf6a44b3fa1051cdf, 0xe3b3f5d2725a9a58, 
    0x0fd5b04525b3182f, 0xcd2b3fda124aca3c, 0x901406a2b55cd8b9, 0x5d48d13e379f1ccb, 
    0xcdfc39fee4acc552, 0x3aa0bdef57e63a1f, 0x81cbaba9f45caaed, 0x48d06bfb3d168360, 
    0x42bed57cac84761b, 0xfeb59a0c81304908, 0xbb781e4bbdf230d2, 0xe977374b97bd0b6b, 
    0x7d38b736428826a0, 0xf2729be2290256dc, 0x304e875c9d4b3fb2, 0x125ae3d0cd3130d6,
    0x3764bdca939cad56, 0x290bfd3ea9c74cbe, 0xcb32a05648982795, 0xb2083afde0219374,
    0x09389bfad721f43d, 0x458475badc30a38d, 0xbad72854902bd01a, 0xcf81993a3acb4302,
    0xf4b8eac294a96d54, 0x18321da9c9410111, 0x00df012104bc0103, 0x110018201acdf900,
    0xcc490ab371f1138f, 0x9327ad39875abef4, 0xabbb29843297f091, 0x0932998100000ac0
};

static void ntrans_style(message_feature *feature, message_segment *segment)
{
    if(segment->seg_size <= FEATURE_NUM)
    {
        fpcpy(feature->fea, segment->fp, segment->seg_size);
        feature->fea_num = segment->seg_size;
    }
    else
    {
        guint64 tmp;
        guint64 lt_res[FEATURE_NUM], max[FEATURE_NUM] = {0}, pos[FEATURE_NUM];
        for(int i=0; i<segment->seg_size; i++)
        {
            tmp = 0;
            for(int j=0; j<FP_LEN; j++)
            {
                tmp += segment->fp[i][j] << (j*8);
            }
            for(int j=0; j<FEATURE_NUM; j++)
            {
                lt_res[j] = (K[j]*tmp+B[j])%((guint64)1<<(FP_LEN*8));
            }
            for(int j=0; j<FEATURE_NUM; j++)
            {
                if(lt_res[j] > max[j])
                {
                    max[j] = lt_res[j];
                    pos[j] = i;
                }
            }
        }
        for(int i=0; i<FEATURE_NUM; i++) fpcpy(feature->fea[i], segment->fp[pos[i]], 1);
        feature->fea_num = FEATURE_NUM;
    }
}
#elif(FEATURE_METHOD == CDMINK_FEATURE)
#define CHECK_BOUNDARY_(ptr, mask) (!(*((guint32*)(ptr))&(mask)))

static void cdmink_style(message_feature *feature, message_segment *segment)
{
    if(segment->seg_size <= FEATURE_NUM)
    {
        fpcpy(feature->fea, segment->fp, segment->seg_size);
        feature->fea_num = segment->seg_size;
    }
    else
    {
        fingerprint candidates[MIN_SEGMENT_SIZE];
        int cand_num = 0, start_pos = 0, par_len = 0, used_num = 0;
        while(used_num < segment->seg_size)
        {
            par_len ++; used_num ++;
            if(CHECK_BOUNDARY_(segment->fp[start_pos+par_len], EXP_SEGMENT_SIZE/FEATURE_NUM-1))
            {
                if(cand_num < MIN_SEGMENT_SIZE - 1)
                {
                    get_min_k_fp(segment->fp+start_pos, par_len, candidates+cand_num, 1);
                    cand_num ++; start_pos = used_num; par_len = 0;
                }
                else {
                    get_min_k_fp(segment->fp+start_pos, segment->seg_size-start_pos, candidates+cand_num, 1);
                    cand_num ++;
                    break;
                }
            }
        }
        if(cand_num <= FEATURE_NUM)
        {
            fpcpy(feature->fea, candidates, cand_num);
            feature->fea_num = cand_num;
        }
        else
        {
            get_min_k_fp(candidates, cand_num, feature->fea, FEATURE_NUM);
            feature->fea_num = FEATURE_NUM;
        }
    }
}
#else
static void stateless_style(message_feature *feature, message_segment *segment)
{
    // get_min_k_fp(segment->fp, segment->seg_size, feature->fea, 1);
    fpcpy(feature->fea, segment->fp, 1);
    feature->fea_num = 0;
}
#endif

static void generate_feature(message_feature *feature, message_segment *segment)
{
    #if(FEATURE_METHOD == FINESSE_FEATURE)
        finesse_style(feature, segment);
    #elif(FEATURE_METHOD == MINK_FEATURE || FEATURE_METHOD == SIGMA_FEATURE)
        mink_style(feature, segment);
    #elif(FEATURE_METHOD == NTRANS_FEATURE)
        ntrans_style(feature, segment);
    #elif(FEATURE_METHOD == CDMINK_FEATURE)
        cdmink_style(feature, segment);
    #else
        stateless_style(feature, segment);
    #endif
}

void new_message_feature(message_segment *segment, message_feature *feature)
{
    memset(feature, 0, sizeof(message_feature));
    generate_feature(feature, segment);
    feature->type = M_FEATURE_TYPE;
}