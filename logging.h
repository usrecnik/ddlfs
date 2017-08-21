#pragma once

#include <stdio.h>

#define LOG_ERROR 1
#define LOG_INFO  2
#define LOG_DEBUG 3

#define DDL_LOG_SIZE 1000

char *g_ddl_log[DDL_LOG_SIZE];
int g_ddl_log_idx; // index of most recently written msg

// log message to stdout/stderr
void logmsg(int level, const char *msg, ...);

// log message to g_ddl_log circular log buffer
void logddl(const char *msg, ...);

