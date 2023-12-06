/*
 * @Author: DDDCai dengcaidengcai@163.com
 * @Date: 2023-06-17 12:24:51
 * @LastEditors: DDDCai dengcaidengcai@163.com
 * @LastEditTime: 2023-07-27 09:05:50
 * @Description: 
 * Copyright (c) 2023 by Cai Deng (dengcaidengcai@163.com), All Rights Reserved. 
 */

#include "glib.h"

guint my_fplen_hash(gconstpointer p);
gboolean my_fplen_equal(gconstpointer v1, gconstpointer v2);
guint my_str_hash(gconstpointer p);
gboolean my_str_equal(gconstpointer v1, gconstpointer v2);
boolean load_server_addr(char *addr_str, struct sockaddr_in *server_addr);
guint my_int64_hash(gconstpointer p);
gboolean my_int64_equal(gconstpointer v1, gconstpointer v2);
double get_time(struct timeval *start, struct timeval *end);