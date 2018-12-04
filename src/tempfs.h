#pragma once

#include <stdio.h>
#include <time.h>
#ifdef _MSC_VER
	#define pid_t int
#endif

/**
 * Methods related to management of temporary local cache files. Those
 * files are located in folder specified by g_conf.temppath parameter (see config.h)
 *
 * All methods return EXIT_SUCCESS on success and EXIT_FAILURE on failure.
 * */


/**
 * set "last_ddl_time" extended attribute on file specified by *path.
 * */
int tfs_setldt(const char *path, time_t last_ddl_time);

/**
 * get "last_ddl_time" extended attribute from file specified by *path.
 * */
int tfs_getldt(const char *path, time_t *last_ddl_time, pid_t *mount_pid, time_t *mount_stamp);

/**
 * Remove cached file (and its metadata).
 * */
int tfs_rmfile(const char *cache_fn);

/**
 * Check if cached file named cache_fn is up2date according to last_ddl_time (yyyy-mm-dd hh24:mi:ss format).
 * actual_time is output parameter through which we return parsed last_ddl_time (input parameter)
 * return EXIT_SUCCESS if cached file is up2date and EXIT_FAILURE on either error OR if cached file is outdated.
 * (because failure should invalidate cache anyway)
 * */
int tfs_validate(const char *cache_fn, const char *last_ddl_time, time_t *actual_time);

/**
 * Exactly the same as tfs_validate, except it doesn't need to convert string time to time_t time
 * */
int tfs_validate2(const char *cache_fn, time_t last_ddl_time);

/**                                                                                                                     
 * Check if cached file was modified/created at least once by this process. In readonly connection, we never want to
 * check with the database if the object has changed.
 * @return EXIT_SUCCESS: file is up2date, EXIT_FAILURE: file is outdated
 * */
int tfs_quick_validate(const char *path);

/**
 * Create temporary directory for cached files (cached ddl content). 
 * This folder may be removed on umount (depending on parameters).
 * */
int tfs_mkdir();


/**
 * Remove temporary directory for cached files (cached ddl content).
 * This is optionally called on umount.
 * */
int tfs_rmdir();

