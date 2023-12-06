/*
 * @Author: Cai Deng
 * @Date: 2023-04-21 09:40:14
 * @LastEditors: Cai Deng
 * @LastEditTime: 2023-04-27 04:02:18
 * @Description: 
 */

#ifndef _INCLUDE_DIS_DEDUP_H_
#define _INCLUDE_DIS_DEDUP_H_

#include <stdint.h>
#include <string.h>
#include "dis_config.h"


typedef uint8_t fingerprint[FP_LEN];

#define FALSE 0
#define TRUE (!FALSE)
typedef int boolean;


#define MAX_FILE_NAME_LEN 256


#define sizeof_xfp(x) ((x)*FP_LEN)
#define fpcpy(dest, src, fp_num) memcpy(dest, src, sizeof_xfp(fp_num))

#define PUT_3_STRS_TOGETHER(des,src1,src2,src3) \
{                                               \
    strcpy(des,src1);                           \
    strcat(des,src2);                           \
    strcat(des,src3);                           \
}

#endif