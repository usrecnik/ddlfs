#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>           /* Definition of AT_* constants */
#ifndef _MSC_VER
	#include <unistd.h>
	#include <dirent.h>
#else
	#pragma warning(disable:4996)
#endif

#include "logging.h"
#include "config.h"
#include "oracle.h"
#include "tempfs.h"
#include "util.h"
#include "query.h"

static int dbr_refresh_object(const char *schema,
                              const char *ora_type,
                              const char *object,
                              time_t last_ddl_time) {

    char object_with_suffix[300];

    // convert oracle type to filesystem type
    char *fs_type = strdup(ora_type);
    if (fs_type == NULL) {
        logmsg(LOG_ERROR, "dbr_refresh_object(): unable to allocate memory for ora_type");
        return EXIT_FAILURE;
    }
    utl_ora2fstype(&fs_type);

    // get suffix based on type
    char *suffix = NULL;
    if (str_suffix(&suffix, ora_type) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "dbr_refresh_object(): unable to determine file suffix");
        if (suffix != NULL)
            free(suffix);
        if (fs_type != NULL)
            free(fs_type);
        return EXIT_FAILURE;
    }
    snprintf(object_with_suffix, 299, "%s%s", object, suffix);

    // get cache filename
    char *fname = NULL;
    if (qry_object_fname(schema, fs_type, object_with_suffix, &fname) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "dbr_refresh_object(): unable to determine cache filename for [%s] [%s].[%s]", ora_type, schema, object_with_suffix);
        if (fname != NULL)
            free(fname);
        if (suffix != NULL)
            free(suffix);
        if (fs_type != NULL)
            free(fs_type);
        return EXIT_FAILURE;
    }

    // if cache file is already up2date
    if (tfs_validate2(fname, last_ddl_time) == EXIT_SUCCESS) {
        // then mark it as verified by this mount
        if (tfs_setldt(fname, last_ddl_time) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "dbr_refresh_object(): unable to mark [%s] [%s].[%s] as verified by this mount.", ora_type, schema, object);
            if (fname != NULL)
                free(fname);
            if (suffix != NULL)
                free(suffix);
            if (fs_type != NULL)
                free(fs_type);
            return EXIT_FAILURE;
        }
    }

    free(fname);
    free(suffix);
    free(fs_type);
    return EXIT_SUCCESS;
}

static int dbr_delete_obsolete() {
#ifdef _MSC_VER
	logmsg(LOG_ERROR, "dbr_delete_obsolete() - this function is not yet implemented for Windows platform!");
	return EXIT_FAILURE;
#else
    char cache_fn[4096];
    DIR *dir = opendir(g_conf._temppath);

    if (dir == NULL) {
        logmsg(LOG_ERROR, "dbr_delete_obsolete() - unable to open directory: %d - %s", errno, strerror(errno));
        return EXIT_FAILURE;
    }

    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)) != NULL) {
        if (dir_entry->d_type != DT_REG)
            continue;

        size_t name_len = strlen(dir_entry->d_name);
        if (name_len < 5)
            continue;

        char *suffix = dir_entry->d_name + name_len - 4;
        if (strcmp(suffix, ".tmp") != 0)
            continue;

        snprintf(cache_fn, 4095, "%s/%s", g_conf._temppath, dir_entry->d_name);

        time_t last_ddl_time = 0;
        time_t mount_stamp = 0;
        pid_t mount_pid = 0;
        if (tfs_getldt(cache_fn, &last_ddl_time, &mount_pid, &mount_stamp) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "dbr_delete_obsolete() - tfs_getldt returned error");
            closedir(dir);
            return EXIT_FAILURE;
        }

        if ((mount_pid != g_conf._mount_pid) || (mount_stamp != g_conf._mount_stamp)) {
            tfs_rmfile(cache_fn);
            logmsg(LOG_DEBUG, "dbr_delete_obsolete() - removed obsolete cache file [%s]", cache_fn);
        }
    }
    closedir(dir);

    return EXIT_SUCCESS;
#endif
}

int dbr_refresh_cache() {
    int retval = EXIT_SUCCESS;
    const char *query =
"select o.owner, o.object_type, o.object_name, \
 to_char(o.last_ddl_time, 'yyyy-mm-dd hh24:mi:ss') as last_ddl_time\
 from all_objects o\
 where generated='N'\
 and (o.object_type != 'TYPE' or o.subobject_name IS NULL)\
 and object_type IN (\
 'TABLE',\
 'VIEW',\
 'PROCEDURE',\
 'FUNCTION',\
 'PACKAGE',\
 'PACKAGE BODY',\
 'TRIGGER',\
 'TYPE',\
 'TYPE BODY',\
 'JAVA SOURCE')";

    ORA_STMT_PREPARE(dbr_refresh_state);
    ORA_STMT_DEFINE_STR_I(dbr_refresh_state, 1, schema, 300);
    ORA_STMT_DEFINE_STR_I(dbr_refresh_state, 2, type, 300);
    ORA_STMT_DEFINE_STR_I(dbr_refresh_state, 3, object, 300);
    ORA_STMT_DEFINE_STR_I(dbr_refresh_state, 4, last_ddl_time, 25);
    ORA_STMT_EXECUTE(dbr_refresh_state, 0);
    while (ORA_STMT_FETCH) {
        dbr_refresh_object(
            ORA_NVL(schema, "_UNKNOWN_SCHEMA_"),
            ORA_NVL(type, "_UNKNOWN_TYPE_"),
            ORA_NVL(object, "_UNKNOWN_OBJECT_"),
            utl_str2time(ORA_NVL(last_ddl_time, "1990-01-01 03:00:01")));
    }

    dbr_delete_obsolete();

dbr_refresh_state_cleanup:
    ORA_STMT_FREE;
    return retval;
}
