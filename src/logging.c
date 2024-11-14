#ifdef __linux__
	__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
#endif

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#ifndef  _MSC_VER
	#include <syslog.h>
#else
	#pragma warning(disable:4996) // Disable warnings like: "This function or variable may be unsafe. Consider using sscanf_s instead."
#endif


#include "logging.h"
#include "config.h"

static void get_datestr(char* datestr, unsigned long bufsize) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(datestr, bufsize, "%Y-%m-%d %H:%M:%S", t);
}

static const char* get_levelstr(int level) {
    switch(level) {
        case LOG_ERROR: return "ERROR";
        case LOG_INFO:  return "INFO ";
        case LOG_DEBUG: return "DEBUG";
        default:        return "?????";
    }
}

static int get_levelint(char* level) {
    if (strcmp(level, "ERROR") == 0)
        return LOG_ERROR;
    if (strcmp(level, "INFO") == 0)
        return LOG_INFO;
    if (strcmp(level, "DEBUG") == 0)
        return LOG_DEBUG;

    fprintf(stderr, "Invalid log level [%s].", level);
    return LOG_DEBUG;
}

void logmsg(int level, const char *msg, ...) {
    char datestr[100];
    get_datestr(datestr, sizeof(datestr)-1);
    
    va_list args;
    va_start(args, msg);
    
    char *format = calloc(200, sizeof(char));
    if (format == NULL) {
        fprintf(stderr, "logmsg() - unable to calloc!");
        fprintf(stderr, "last message: [%s]", msg);
        return;
    }
    strcpy(format, datestr);
    strcat(format, " ");
    strcat(format, get_levelstr(level)); // "2017-07-20 16:58:39 INFO "
    strcat(format, " ");
    strncat(format, msg, 150);    
    strcat(format, "\n");
    
    if (level == LOG_ERROR || level <= get_levelint(g_conf.loglevel)) {
        vfprintf((level == 1 ? stderr : stdout), format, args);
        fflush((level == 1 ? stderr : stdout));
    }

    // error messages are logged to syslog, regardless of any ddlfs settings
    if (level == LOG_ERROR) {
        va_end(args);
        va_start(args, msg);
        
#ifndef _MSC_VER // @todo - log to Windows Event log on Windows
        openlog("ddlfs", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
        vsyslog(LOG_ERR, msg, args);
        closelog();
#endif
    }

    free(format);
    
    va_end(args);
}

void logddl(const char *msg, ...) { 
    
    if (g_ddl_log_buf == NULL) {
        g_ddl_log_buf = calloc(DDL_LOG_SIZE, sizeof(char));
        g_ddl_log_len = 0;
        if (g_ddl_log_buf == NULL) {
            logmsg(LOG_ERROR, "logddl() - Unable to allocate memory for in-memory contents of ddlfs.log, size=[%d]", DDL_LOG_SIZE);
            logmsg(LOG_ERROR, msg);
            return;
        }
    }

    va_list args;
    va_start(args, msg);

    // "realloc"
    if (DDL_LOG_SIZE - g_ddl_log_len < 500) {
        char *temp = calloc(DDL_LOG_SIZE, sizeof(char));
        if (temp == NULL) {
            logmsg(LOG_ERROR, "logddl() - Unable to re-allocate memory for in-memory contents of ddlfs.log, size=[%d]", DDL_LOG_SIZE);
            logmsg(LOG_ERROR, msg);
            return;
        }
        int half = DDL_LOG_SIZE / 2;
        memcpy(temp, g_ddl_log_buf + half, g_ddl_log_len-half);
        free(g_ddl_log_buf);
        g_ddl_log_buf = temp;
    }
    
    char *line = calloc(250, sizeof(char));
    vsnprintf(line, 249, msg, args);    

    strncat(g_ddl_log_buf, line, 250);
    strcat(g_ddl_log_buf, "\n");

    g_ddl_log_len = strlen(g_ddl_log_buf);
    //logmsg(LOG_DEBUG, "ddlfs.log len=[%d]", g_ddl_log_len);
    free(line);
    va_end(args);
}

