#pragma once

#include <stdio.h>
#include <time.h>

#define LOG_ERROR 1
#ifndef LOG_INFO
    #define LOG_INFO 6
#endif
#ifndef LOG_DEBUG
    #define LOG_DEBUG 7
#endif

#define DDL_LOG_SIZE 10000

char *g_ddl_log_buf;   // contents of in-memory "ddlfs.log"
time_t g_ddl_log_time; // time when the contents last changed
long g_ddl_log_len;


// log message to stdout/stderr
void logmsg(int level, const char *msg, ...);

// log message to g_ddl_log circular log buffer
void logddl(const char *msg, ...);

