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

int utl_fs2oratype(char **fstype) {
    char *type = *fstype;

    if (strcmp(type, "PACKAGE_SPEC") == 0) {
        free(type);
        type = strdup("PACKAGE");
    } else if (strcmp(type, "PACKAGE_BODY") == 0) {
        free(type);
        type = strdup("PACKAGE BODY");
    } else if (strcmp(type, "TYPE_BODY") == 0) {
        free(type);
        type = strdup("TYPE BODY");
    } else if (strcmp(type, "JAVA_SOURCE") == 0) {
        free(type);
        type = strdup("JAVA SOURCE");
    } else if (strcmp(type, "JAVA_CLASS") == 0) {
        free(type);
        type = strdup("JAVA CLASS");
    } else if (strcmp(type, "MATERIALIZED_VIEW") == 0) {
        free(type);
        type = strdup("MATERIALIZED VIEW");
    }

    if (type == NULL) {
        logmsg(LOG_ERROR, "str_fs2oratype() - unable to malloc for normalized type.");
        return EXIT_FAILURE;
    }

    *fstype = type;
    return EXIT_SUCCESS;
}

int utl_ora2fstype(char **oratype) {
    char *type = *oratype;
    if (strcmp(type, "PACKAGE") == 0) {
        free(type);
        type = strdup("PACKAGE_SPEC");
    } else if (strcmp(type, "PACKAGE BODY") == 0) {
        free(type);
        type = strdup("PACKAGE_BODY");
    } else if (strcmp(type, "TYPE BODY") == 0) {
        free(type);
        type = strdup("TYPE_BODY");
    } else if (strcmp(type, "JAVA SOURCE") == 0) {
        free(type);
        type = strdup("JAVA_SOURCE");
    } else if (strcmp(type, "JAVA CLASS") == 0) {
        free(type);
        type = strdup("JAVA_CLASS");
    } else if (strcmp(type, "MATERIALIZED VIEW") == 0) {
        free(type);
        type = strdup("MATERIALIZED_VIEW");
    }

    if (type == NULL) {
        logmsg(LOG_ERROR, "str_ora2fstype() - unable to malloc for normalized type.");
        return EXIT_FAILURE;
    }

    *oratype = type;
    return EXIT_SUCCESS;
}
