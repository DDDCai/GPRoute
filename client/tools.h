/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-06-14 04:32:54
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-07-26 09:00:11
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include <glib.h>

void load_server_addr(char *addr_file_path, struct sockaddr_in *server_addr, int addr_num);
guint my_fplen_hash(gconstpointer p);
gboolean my_fplen_equal(gconstpointer v1, gconstpointer v2);
double get_time(struct timeval *start, struct timeval *end);