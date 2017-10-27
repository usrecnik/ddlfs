#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include "logging.h"

/**
 * determine meta file name (meta_fn) for specified cache file name (cache_fn).
 * basically, return the same string, except replace suffix with ".dfs" 
 * */
static int tfs_getldt_fn(const char *cache_fn, char **meta_fn) {
    int len=strlen(cache_fn);
    if (len < 5) {
        logmsg(LOG_ERROR, "tfs_getldt_fn - cache file name [%s] is too short.", cache_fn);
        return EXIT_FAILURE;
    }
    
    *meta_fn = strdup(cache_fn);
    if (*meta_fn == NULL) {
        logmsg(LOG_ERROR, "tfs_getldt_fn - unable to allocate memory for meta_fn name based on [%s]", cache_fn);
        return EXIT_FAILURE;
    }

    (*meta_fn)[len-3] = 'd';
    (*meta_fn)[len-2] = 'f';
    (*meta_fn)[len-1] = 's';

    return EXIT_SUCCESS;
}

int tfs_setldt(const char *path, time_t last_ddl_time) {
    // I have considered saving last_ddl_time as user extended attribute of file on filesystems which supports this
    // (most do, but not all of them). Problem with this approach is that even if fs supports this feature, it may
    // be disabled at kernel level or at mount time (fs opts). So.. I decided not to rely on such feature.

    char *meta_fn = NULL;
    if (tfs_getldt_fn(path, &meta_fn) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_setldf - unable to determine meta file for cache file [%s]", path);
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(meta_fn, "w");
    if (fp == NULL) {
        logmsg(LOG_ERROR, "tfs_setldf - unable to open meta file [%s]: %d - %s", meta_fn, errno, strerror(errno));
        return EXIT_FAILURE;
    }

    int len = fwrite(&last_ddl_time, 1 , sizeof(time_t), fp);
    if (len != sizeof(time_t)) {
        logmsg(LOG_ERROR, "tfs_setldf - unable to write to meta file [%s]", meta_fn);
        fclose(fp);
        return EXIT_FAILURE;
    }

    if (fclose(fp) != 0) {
        logmsg(LOG_ERROR, "tfs_setldf - unable to close meta file [%s]", meta_fn);
        return EXIT_FAILURE;
    }
   
    free(meta_fn);
   
    return EXIT_SUCCESS;
}

int tfs_getldt(const char *path, time_t *last_ddl_time) {
    
    char *meta_fn = NULL;
    if (tfs_getldt_fn(path, &meta_fn) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_getldf - unable to determine meta file for cache file [%s]", path);
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(meta_fn, "r");
    if (fp == NULL) {
        logmsg(LOG_ERROR, "tfs_getldf - unable to opem meta file [%s]: %d - %s", meta_fn, errno, strerror(errno));
        return EXIT_FAILURE;
    }

    int len = fread(last_ddl_time, 1, sizeof(time_t), fp);
    if (len != sizeof(time_t)) {
        logmsg(LOG_ERROR, "tfs_getldf - unable to read meta file [%s], got [%d] bytes, expected [%d]", meta_fn, len, sizeof(time_t));
        if (ferror(fp) != 0) {
            logmsg(LOG_ERROR, "tfs_getldf - .. ERROR");
        }  else {
            logmsg(LOG_ERROR, "tfs_getldf - .. EOF");    
        }
        fclose(fp);
        return EXIT_FAILURE;
    }
    
    if (fclose(fp) != 0) {
        logmsg(LOG_ERROR, "tfs_getldf - unable to close meta file [%s]", meta_fn);
        return EXIT_FAILURE;
    }

    free(meta_fn);    

    return EXIT_SUCCESS;
}

int tfs_rmfile(const char *cache_fn) {

    int retval = EXIT_SUCCESS;
    if (unlink(cache_fn) != 0) {
        logmsg(LOG_ERROR, "tfs_rmfile - unable to remove [%s]: %d - %s", cache_fn, errno, strerror(errno));
        retval = EXIT_FAILURE;
    }

    char *meta_fn = NULL;
    
    if (tfs_getldt_fn(cache_fn, &meta_fn) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_rmfile - unable to determine metafile name for [%s]", cache_fn);
        return EXIT_FAILURE;
    }
    
    if (unlink(meta_fn) != 0) {
        logmsg(LOG_ERROR, "tfs_rmfile - unable to remove [%s]: %d - %s", meta_fn, errno, strerror(errno));
        retval = EXIT_FAILURE;
    }

    free(meta_fn);

    return retval;
}

int tfs_validate(const char *cache_fn, const char *last_ddl_time, time_t *actual_time) {
    
    struct tm *temptime = malloc(sizeof(struct tm));
    if (temptime == NULL) {
        logmsg(LOG_ERROR, "tfs_validate - unable to allocate memory for temptime");
        return EXIT_FAILURE;
    }
    
    // convert last_ddl_time to binary format time_t
    memset(temptime, 0, sizeof(struct tm));
    char* xx = strptime((char*) last_ddl_time, "%Y-%m-%d %H:%M:%S", temptime);
    if (*xx != '\0') {
        logmsg(LOG_ERROR, "tfs_validate - Unable to parse date (%s)", last_ddl_time);
        free(temptime);
        return EXIT_FAILURE;
    }
    
    *actual_time = timegm(temptime); // last_ddl_time as we got it by querying the datbase (@param last_ddl_time)
    time_t cached_time = 0;           // last_ddl_time as specified in metadata about cached file (@param cache_fn)
    free(temptime);
    
    // check if tempfile already exist and has atime equal to last_modified_time.
    // there is no need to recreate tempfile if times (atime & last_modified_time) match.
    if (access(cache_fn, F_OK) == -1 ) {
        logmsg(LOG_DEBUG, "tfs_validate - cache file [%s] does not (yet) exist.", cache_fn);
        return EXIT_FAILURE;
    }

    if (tfs_getldt(cache_fn, &cached_time) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_validate - unable to read cached last_ddl_time for [%s]!", cache_fn);
        return EXIT_FAILURE;
    }

    if (cached_time == *actual_time) {
        logmsg(LOG_DEBUG, "tfs_validate - cache file [%s] is already up2date.", cache_fn);
        return EXIT_SUCCESS;
    }
    
    logmsg(LOG_DEBUG, "tfs_validate - cache file [%s] is outdated.", cache_fn);
    return EXIT_FAILURE;
}
