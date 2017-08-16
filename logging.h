#pragma once

#include <stdio.h>

#define LOG_ERROR 1
#define LOG_INFO  2
#define LOG_DEBUG 3

void logmsg(int level, const char* msg, ...);

