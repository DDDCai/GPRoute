/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-04-25 05:35:48
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-06-20 04:13:55
 * @Description: 
 * Copyright (c) 2023 by DDDCai dengcaidengcai@163.com, All Rights Reserved. 
 */

#ifndef _INCLUDE_CLIENT_H_
#define _INCLUDE_CLIENT_H_

#include <netinet/in.h>
#include "dis_dedup.h"

boolean client_dedup_a_file(char *file_path, struct sockaddr_in *server_addr);

#endif