#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <openssl/evp.h>

#include "config.h"
#include "logging.h"
/*
@todo - mkRelease.sh should add following to requirements: 
$ sudo apt-get install libssl-dev
$ sudo yum install openssl-devel
*/

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

// return 1 if haystack ends with suffix and 0 otherwise.
static int tfs_strend(const char *haystack, const char *suffix) {
    if (strlen(haystack) <= strlen(suffix))
        return 0;
    
    if (strcmp(haystack + strlen(haystack) + strlen(haystack) - strlen(suffix), suffix))
        return 1;
    
    return 0;
}

int tfs_rmdir(int ignoreNoDir) {
    
    // remove files in directory
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (g_conf._temppath)) == NULL) {
        if (ignoreNoDir != 1 || errno != ENOENT)
            logmsg(LOG_ERROR, "tfs_rmdir - unable to open directory (%d): %d - %s", g_conf._temppath, errno, strerror(errno));
        return EXIT_FAILURE;
    }
    
    char temppath[8192];
    while ((ent = readdir (dir)) != NULL) {
        if (strlen(g_conf._temppath) + strlen(ent->d_name) > 8190) {
            logmsg(LOG_ERROR, "tfs_rmdir - unable to remove file because name too long (8k) [%s]", ent->d_name);
            continue;
        }
        strcpy(temppath, g_conf._temppath);
        strcat(temppath, "/");
        strcat(temppath, ent->d_name);
 
        if ( (tfs_strend(ent->d_name, ".tmp")) || (tfs_strend(ent->d_name, ".dfs")) ) {
            if (unlink(temppath) != 0) {
                logmsg(LOG_ERROR, "tfs_rmdir - unable to delete file [%s]: %d - %s", temppath, errno, strerror(errno));
                continue;
            }
        }
    }

    closedir(dir);

    // remove empty directory
    if (rmdir(g_conf._temppath) != 0) {
        logmsg(LOG_ERROR, "tfs_rmdir - unable to delete directory (%s): %d - %s", g_conf._temppath, errno, strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int tfs_mkdir() {
    
    g_conf._temppath = calloc(2048, sizeof(char));
    if (g_conf._temppath == NULL) {
        logmsg(LOG_ERROR, "tfs_mkdir - unable to calloc memory for temppath");
        return EXIT_FAILURE;
    }
   
    snprintf(g_conf._temppath, 2047, "%s/ddlfs-%s.%s.%s.%s.%d",
        g_conf.temppath,
        g_conf.database,
        (getenv("ORACLE_SID") == NULL ? "tns" : getenv("ORACLE_SID")),
        g_conf.username,
        g_conf.schemas,
        g_conf.lowercase);
    
    // replace "special" characters
    for (int i = strlen(g_conf.temppath)+1; i < strlen(g_conf._temppath); i++) {
        char c = g_conf._temppath[i];
        if (c >= '0' && c <= '9')
            continue;

        if (c >= 'A' && c <= 'Z')
            continue;

        if (c >= 'a' && c <= 'z')
            continue;

        if (c == '.' || c == '_')
            continue;

        g_conf._temppath[i] = '_';
    }
    
    // (optionally) delete existing directory
    if (g_conf.keepcache == 0)
        tfs_rmdir(1);

    // create or reuse directory
    struct stat st;
    if (stat(g_conf._temppath, &st) == -1) {
        if (errno == ENOENT) {
            if (mkdir(g_conf._temppath, 0700) != 0) {
                logmsg(LOG_ERROR, "tfs_mkdir - unable to create temporary directory [%s]: %d - %s", g_conf._temppath, errno, strerror(errno));
                return EXIT_FAILURE;
            }
            logmsg(LOG_DEBUG, "tfs_mkdir - created temporary directory: [%s]", g_conf._temppath);
        } else {
            logmsg(LOG_ERROR, "tfs_mkdir - unable to check if temporary directory [%s] exists. %d - %s", g_conf._temppath, errno, strerror(errno));
            return EXIT_FAILURE;
        }
    } else {
        logmsg(LOG_DEBUG, "tfs_mkdir - reused temporary directory: [%s]", g_conf._temppath); 
    }
     
    return EXIT_SUCCESS;
}

