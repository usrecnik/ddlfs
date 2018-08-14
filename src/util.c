#define _XOPEN_SOURCE
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "logging.h"

time_t utl_str2time(char *time) {
    struct tm *temptime = malloc(sizeof(struct tm));
    if (temptime == NULL) {
            logmsg(LOG_ERROR, "utl_str2time(): unable to allocate memory for temptime");
            return -1;
    }
    memset(temptime, 0, sizeof(struct tm));
    char* xx = strptime(time, "%Y-%m-%d %H:%M:%S", temptime);
    if (xx == NULL || *xx != '\0') {
        logmsg(LOG_ERROR, "utl_str2time(): unable to parse date [%s]", time);
        free(temptime);
        return -1;
    }
    time_t retval = timegm(temptime);
    free(temptime);
    return retval;
}
