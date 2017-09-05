#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>


#include "logging.h"
#include "config.h"


static void get_datestr(char* datestr, unsigned long bufsize) {
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	strftime(datestr, bufsize, "%Y-%m-%d %H:%M:%S", t);
}

static char* get_levelstr(int level) {
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
	
	if (level == 1 || level <= get_levelint(g_conf.loglevel)) {
		vfprintf((level == 1 ? stderr : stdout), format, args);
		fflush((level == 1 ? stderr : stdout));
	}

	free(format);
	
	va_end(args);
}

void logddl(const char *msg, ...) { 
    
    if (g_ddl_log_buf == NULL) {
        g_ddl_log_buf = calloc(DDL_LOG_SIZE, sizeof(char));
        if (g_ddl_log_buf == NULL) {
            logmsg(LOG_ERROR, "Unable to allocate memory for in-memory contents of ddlfs.log, size=[%d]", DDL_LOG_SIZE);
            logmsg(LOG_ERROR, msg);
            return;
        }
    }

    va_list args;
    va_start(args, msg);

    if (DDL_LOG_SIZE - 250 < 0) {
        logmsg(LOG_ERROR, "ABOUT TO REACH BUFFER OVERFLOW!");
    }

    char *line = calloc(250, sizeof(char));
    vsnprintf(line, 249, msg, args);    

    strncat(g_ddl_log_buf, line, 250);
    strcat(g_ddl_log_buf, "\n");
    
    free(line);
    va_end(args);
}

