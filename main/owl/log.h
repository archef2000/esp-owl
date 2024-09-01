/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#define LOG_VERSION "0.1.0"

typedef void (*log_LockFn)(void *udata, int lock);

enum {
    LOG_ERR = 1,  /* error conditions */
    LOG_WARNING,  /* warning conditions */
    LOG_INFO,     /* informational */
    LOG_DEBUG,    /* debug-level messages */
    LOG_TRACE     /* trace-level messages */
};

#define log_error(...) log_log(LOG_ERR, __FILE__, __LINE__,__func__, __VA_ARGS__)
#define log_warn(...) log_log(LOG_WARNING, __FILE__, __LINE__,__func__, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __FILE__, __LINE__,__func__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__,__func__, __VA_ARGS__)
#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__,__func__, __VA_ARGS__)

void log_set_quiet(int enable);

int log_log(int level, const char *file, int line, const char *func, const char *fmt, ...);

#endif
