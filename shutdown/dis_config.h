/*
 * @Author: Cai Deng
 * @Date: 2023-04-11 04:22:21
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-12-01 10:11:54
 * @Description: 
 */
#ifndef _INCLUDE_DIS_CONFIG_H_
#define _INCLUDE_DIS_CONFIG_H_


#define ON 1
#define OFF 0





#define EXP_SEGMENT_SIZE 1024
#define MIN_SEGMENT_SIZE (EXP_SEGMENT_SIZE/2)
#define MAX_SEGMENT_SIZE (EXP_SEGMENT_SIZE*2)   // number of chunks.

#define FP_LEN 6
#define FEATURE_NUM 8


#define FINESSE_FEATURE 0
#define MINWISE_FEATURE 1
#define FEATURE_METHOD FINESSE_FEATURE


#ifndef NODE_NUM
    #define NODE_NUM 64
#endif


// #define DEBUG_MOTI


#endif