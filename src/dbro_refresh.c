#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>           /* Definition of AT_* constants */
#ifndef _MSC_VER
    #include <windows.h>
	#include <unistd.h>
	#include <dirent.h>
#else
	#pragma warning(disable:4996)
#endif

#include "query.h"
#include "oracle.h"
#include "logging.h"
#include "config.h"
#include "tempfs.h"
#include "util.h"


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
        printf("tfs_validat2 IS OK SUCCESS.\n");
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

static int dbr_delete_obsolete_entry(const char* fn) {
    size_t name_len = strlen(fn);
    if (name_len < 5) // ignored files are succeess because failure breaks the loop
        return EXIT_SUCCESS;

    const char* suffix = fn + name_len - 4;
    if (strcmp(suffix, ".tmp") != 0)
        return EXIT_SUCCESS;

    char cache_fn[4096];
    snprintf(cache_fn, 4095, "%s%s%s", g_conf._temppath, PATH_SEP, fn);

    time_t last_ddl_time = 0;
    time_t mount_stamp = 0;
    pid_t mount_pid = 0;
    if (tfs_getldt(cache_fn, &last_ddl_time, &mount_pid, &mount_stamp) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "dbr_delete_obsolete() - tfs_getldt returned error");        
        return EXIT_FAILURE;
    }

    if ((mount_pid != g_conf._mount_pid) || (mount_stamp != g_conf._mount_stamp)) {
        if (tfs_rmfile(cache_fn) != EXIT_SUCCESS)
            return EXIT_FAILURE;
        logmsg(LOG_DEBUG, "dbr_delete_obsolete() - removed obsolete cache file [%s]", cache_fn);
    }
    
    return EXIT_SUCCESS;
}

#ifdef _MSC_VER
static int dbr_delete_obsolete() {

    char abs_path[4096];
    snprintf(abs_path, 4096, "%s%s*", g_conf._temppath, PATH_SEP);
    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFileA(abs_path, &find_data);

    if (find_handle == INVALID_HANDLE_VALUE) {
        logmsg(LOG_ERROR, "dbr_delete_obsolete() - FindFirstFile(%s) has failed: %d", abs_path, GetLastError());
        return EXIT_FAILURE;
    }

    do {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue; // skip directories, this includes "." and ".."

        const char* fn = find_data.cFileName;
        if (dbr_delete_obsolete_entry(fn) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "dbr_delete_obsolete() - entry function returned error, aborting.");
            break;
        }
        
    } while (FindNextFileA(find_handle, &find_data) != 0);

    // Check if the loop ended because there are no more files
    if (GetLastError() != ERROR_NO_MORE_FILES) {
        logmsg(LOG_ERROR, "dbr_delete_obsolete() - FindNextFileA failed. Error: %lu", GetLastError());
        FindClose(find_handle); // ignore result, we've alredy failed.
        return EXIT_FAILURE;
    }

    if (!FindClose(find_handle)) {
        logmsg(LOG_ERROR, "dbr_delete_obsolete() - FindClose failed. Error: %lu", GetLastError());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
#else
static int dbr_delete_obsolete() {   
    DIR *dir = opendir(g_conf._temppath);

    if (dir == NULL) {
        logmsg(LOG_ERROR, "dbr_delete_obsolete() - unable to open directory: %d - %s", errno, strerror(errno));
        return EXIT_FAILURE;
    }

    struct dirent *dir_entry = NULL;
    while ((dir_entry = readdir(dir)) != NULL) {
        if (dir_entry->d_type != DT_REG)
            continue;

        const char* fn = dir_entry->d_name;
        if (dbr_delete_obsolete_entry(fn) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "dbr_delete_obsolete() - entry function returned error, aborting.");
            closedir(dir);
            break;
        }

    }
    closedir(dir);

    return EXIT_SUCCESS;
}
#endif

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
