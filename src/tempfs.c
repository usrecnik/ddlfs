#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _MSC_VER
	#include <unistd.h>
	#include <sys/xattr.h>
	#include <dirent.h>
#else
	#include <windows.h>
	#include <io.h>
	#pragma warning(disable:4996)
	#define strdup _strdup
	#define pid_t int
	#define F_OK 0
#endif

#include "config.h"
#include "logging.h"
#include "util.h"


/**
 * determine meta file name (meta_fn) for specified cache file name (cache_fn).
 * basically, return the same string, except replace suffix with ".dfs"
 * */
static int tfs_getldt_fn(const char *cache_fn, char **meta_fn) {
    size_t len = strlen(cache_fn);
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


static inline int tfs_fwrite(const void *ptr, size_t size, FILE *stream) {
    size_t len = fwrite(ptr, 1, size, stream);
    if (len != size) {
        logmsg(LOG_ERROR, "tfs_fwrite() - Unable to write to meta file");
        fclose(stream);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int tfs_setldt(const char *path, time_t last_ddl_time) {
    // I have considered saving last_ddl_time as user extended attribute of file on filesystems which supports this
    // (most do, but not all of them). Problem with this approach is that even if fs supports this feature, it may
    // be disabled at kernel level or at mount time (fs opts). So.. I decided not to rely on such feature.

    char *meta_fn = NULL;
    if (tfs_getldt_fn(path, &meta_fn) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_setldf - unable to determine meta file for cache file [%s]", path);
        if (meta_fn != NULL)
            free(meta_fn);
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(meta_fn, "w");
    if (fp == NULL) {
        logmsg(LOG_ERROR, "tfs_setldf - unable to open meta file [%s]: %d - %s", meta_fn, errno, strerror(errno));
        free(meta_fn);
        return EXIT_FAILURE;
    }

    if (tfs_fwrite(&last_ddl_time, sizeof(time_t), fp) != EXIT_SUCCESS) {
        free(meta_fn);
        return EXIT_FAILURE;
    }

    if (tfs_fwrite(&g_conf._mount_pid, sizeof(pid_t), fp) != EXIT_SUCCESS) {
        free(meta_fn);
        return EXIT_FAILURE;
    }

    if (tfs_fwrite(&g_conf._mount_stamp, sizeof(time_t), fp) != EXIT_SUCCESS) {
        free(meta_fn);
        return EXIT_FAILURE;
    }

    if (fclose(fp) != 0) {
        logmsg(LOG_ERROR, "tfs_setldf - unable to close meta file [%s]", meta_fn);
        free(meta_fn);
        return EXIT_FAILURE;
    }

    free(meta_fn);
    return EXIT_SUCCESS;
}

int tfs_getldt(const char *path, time_t *last_ddl_time, pid_t *mount_pid, time_t *mount_stamp) {

    char *meta_fn = NULL;
    if (tfs_getldt_fn(path, &meta_fn) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_getldf - unable to determine meta file for cache file [%s]", path);
        if (meta_fn != NULL)
            free(meta_fn);
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(meta_fn, "r");
    if (fp == NULL) {
        logmsg(LOG_ERROR, "tfs_getldf - unable to open meta file [%s]: %d - %s", meta_fn, errno, strerror(errno));
        free(meta_fn);
        return EXIT_FAILURE;
    }

    size_t len = fread(last_ddl_time, 1, sizeof(time_t), fp);
    if (len != sizeof(time_t)) {
        logmsg(LOG_ERROR, "tfs_getldf - unable to read meta file [%s], field 1,  got [%d] bytes, expected [%d]", meta_fn, len, sizeof(time_t));
        if (ferror(fp) != 0) {
            logmsg(LOG_ERROR, "tfs_getldf - .. ERROR");
        }  else {
            logmsg(LOG_ERROR, "tfs_getldf - .. EOF");
        }
        fclose(fp);
        free(meta_fn);
        return EXIT_FAILURE;
    }

    if (mount_pid != NULL && mount_stamp != NULL) {
        len = fread(mount_pid, 1, sizeof(pid_t), fp);
        if (len != sizeof(pid_t)) {
            logmsg(LOG_ERROR, "tfs_getldf - unable to read meta file [%s], field 2, got [%d] bytes, expected [%d]", meta_fn, len, sizeof(pid_t));
            if (ferror(fp) != 0) {
                logmsg(LOG_ERROR, "tfs_getldf - .. ERROR");
            }  else {
                logmsg(LOG_ERROR, "tfs_getldf - .. EOF");
            }
            fclose(fp);
            free(meta_fn);
            return EXIT_FAILURE;
        }

        len = fread(mount_stamp, 1, sizeof(time_t), fp);
        if (len != sizeof(time_t)) {
            logmsg(LOG_ERROR, "tfs_getldf - unable to read meta file [%s], field 3, got [%d] bytes, expected [%d]", meta_fn, len, sizeof(time_t));
            if (ferror(fp) != 0) {
                logmsg(LOG_ERROR, "tfs_getldf - .. ERROR");
            }  else {
                logmsg(LOG_ERROR, "tfs_getldf - .. EOF");
            }
            fclose(fp);
            free(meta_fn);
            return EXIT_FAILURE;
        }
    }

    if (fclose(fp) != 0) {
        logmsg(LOG_ERROR, "tfs_getldf - unable to close meta file [%s]", meta_fn);
        free(meta_fn);
        return EXIT_FAILURE;
    }

    free(meta_fn);
    return EXIT_SUCCESS;
}

int tfs_rmfile(const char *cache_fn) {

    int retval = EXIT_SUCCESS;
    if (unlink(cache_fn) != 0) {
        logmsg(LOG_ERROR, "tfs_rmfile - unable to remove cache file [%s]: %d - %s", cache_fn, errno, strerror(errno));
        retval = EXIT_FAILURE;
    }

    char *meta_fn = NULL;

    if (tfs_getldt_fn(cache_fn, &meta_fn) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_rmfile - unable to determine metafile name for [%s]", cache_fn);
        if (meta_fn != NULL)
            free(meta_fn);
        return EXIT_FAILURE;
    }

    if (unlink(meta_fn) != 0) {
        logmsg(LOG_ERROR, "tfs_rmfile - unable to remove meta file [%s]: %d - %s", meta_fn, errno, strerror(errno));
        retval = EXIT_FAILURE;
    }

    free(meta_fn);

    return retval;
}

/**
 * It only makes sense to check this when dbro=1
 * @return EXIT_SUCCESS: file is up2date, EXIT_FAILURE: file is outdated
 * */
int tfs_quick_validate(const char *path) {

    char *meta_fn = NULL;
    if (tfs_getldt_fn(path, &meta_fn) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_setldf - unable to determine meta file for cache file [%s]", path);
        if (meta_fn != NULL)
            free(meta_fn);
        return EXIT_FAILURE;
    }
	
    if (access(meta_fn, F_OK) == -1) {
        logmsg(LOG_DEBUG, "tfs_quick_validate - cache file [%s] does not yet exist.", meta_fn);
        free(meta_fn);
        return EXIT_FAILURE;
    }

    time_t last_ddl_time = 0;
    pid_t mount_pid = 0;
    time_t mount_stamp = 0;

    if (tfs_getldt(meta_fn, &last_ddl_time, &mount_pid, &mount_stamp) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_quick_validate() - unable to obtain last mount pid & stamp from [%s]", meta_fn);
        free(meta_fn);
        return EXIT_FAILURE;
    }

    if (mount_pid == g_conf._mount_pid && mount_stamp == g_conf._mount_stamp) {
        logmsg(LOG_DEBUG, "tfs_quick_validate() - validated [%s]", meta_fn);
        free(meta_fn);
        return EXIT_SUCCESS;
    }

    free(meta_fn);
    return EXIT_FAILURE;
}

int tfs_validate2(const char *cache_fn, time_t last_ddl_time) {
    time_t cached_time = 0;

    // check if tempfile already exist and has atime equal to last_modified_time.
    // there is no need to recreate tempfile if times (atime & last_modified_time) match.
    if (access(cache_fn, F_OK) == -1 ) {
        logmsg(LOG_DEBUG, "tfs_validate - cache file [%s] does not (yet) exist.", cache_fn);
        return EXIT_FAILURE;
    }

    if (tfs_getldt(cache_fn, &cached_time, NULL, NULL) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "tfs_validate - unable to read cached last_ddl_time for [%s]!", cache_fn);
        return EXIT_FAILURE;
    }

    if (cached_time == last_ddl_time) {
        logmsg(LOG_DEBUG, "tfs_validate - cache file [%s] is already up2date.", cache_fn);
        return EXIT_SUCCESS;
    }

    logmsg(LOG_DEBUG, "tfs_validate - cache file [%s] is outdated.", cache_fn);
    return EXIT_FAILURE;
}

int tfs_validate(const char *cache_fn, char *last_ddl_time, time_t *actual_time) {
    *actual_time = utl_str2time(last_ddl_time);
    return tfs_validate2(cache_fn, *actual_time);
}

// return 1 if haystack ends with suffix and 0 otherwise.
static int tfs_strend(const char *haystack, const char *suffix) {
    if (strlen(haystack) <= strlen(suffix))
        return 0;

    if (strcmp(haystack + strlen(haystack) + strlen(haystack) - strlen(suffix), suffix))
        return 1;

    return 0;
}

#ifndef _MSC_VER
int tfs_rmdir(int ignoreNoDir) {

    // remove files in directory
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (g_conf._temppath)) == NULL) {
        if (ignoreNoDir != 1 || errno != ENOENT)
            logmsg(LOG_ERROR, "tfs_rmdir - unable to open directory [%s]: %d %s", g_conf._temppath, errno, strerror(errno));
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
#else
int tfs_rmdir(int ignoreNoDir) {
	WIN32_FIND_DATA file;
	HANDLE hFind = NULL;
	char abs_path[8192];

	if ((hFind = FindFirstFile(g_conf._temppath, &file)) == INVALID_HANDLE_VALUE) {
		if (ignoreNoDir != 1)
			printf("tfs_rmdir - unable to open directory because it probably doesn't exist (%s)\n", g_conf._temppath);
		return EXIT_FAILURE;
	}

	do {
		if (strcmp(file.cFileName, ".") == 0)
			continue;

		if (strcmp(file.cFileName, "..") == 0)
			continue;

		sprintf(abs_path, "%s\\%s", g_conf._temppath, file.cFileName);
		if (!(file.dwFileAttributes &FILE_ATTRIBUTE_DIRECTORY)) {
			if (!DeleteFileA(abs_path))
				printf("tfs_rmdir - unable to delete file [%s]!", abs_path);
		}

	} while (FindNextFile(hFind, &file)); //Find the next file.

	FindClose(hFind); //Always, Always, clean things up!
	return EXIT_SUCCESS;
}
#endif

int tfs_mkdir() {

    g_conf._temppath = calloc(2048, sizeof(char));
    if (g_conf._temppath == NULL) {
        logmsg(LOG_ERROR, "tfs_mkdir - unable to calloc memory for temppath");
        return EXIT_FAILURE;
    }

    snprintf(g_conf._temppath, 2047, "%s/ddlfs-%s.%s.%s.%s",
        g_conf.temppath,
        g_conf.database,
        (getenv("ORACLE_SID") == NULL ? "tns" : getenv("ORACLE_SID")),
        g_conf.username,
        g_conf.schemas);

    // replace "special" characters
    for (size_t i = strlen(g_conf.temppath)+1; i < strlen(g_conf._temppath); i++) {
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
    g_conf._temppath_reused = 0;
    struct stat st;
    if (stat(g_conf._temppath, &st) == -1) {
        if (errno == ENOENT) {
#ifndef _MSC_VER
            if (mkdir(g_conf._temppath, 0700) != 0) {
                logmsg(LOG_ERROR, "tfs_mkdir - unable to create temporary directory [%s]: %d - %s", g_conf._temppath, errno, strerror(errno));
                return EXIT_FAILURE;
            }
#else
			if (CreateDirectory(g_conf._temppath, NULL) == 0) {
				logmsg(LOG_ERROR, "tfs_mkdir - unable to create temporary directory [%s]!", g_conf._temppath);
				return EXIT_FAILURE;
			}
#endif
            logmsg(LOG_DEBUG, "tfs_mkdir - created temporary directory: [%s]", g_conf._temppath);
        } else {
            logmsg(LOG_ERROR, "tfs_mkdir - unable to check if temporary directory [%s] exists. %d - %s", g_conf._temppath, errno, strerror(errno));
            return EXIT_FAILURE;
        }
    } else {
        logmsg(LOG_DEBUG, "tfs_mkdir - reused temporary directory: [%s]", g_conf._temppath);
        g_conf._temppath_reused = 1;
    }

    return EXIT_SUCCESS;
}
