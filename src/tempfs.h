#pragma once

#include <stdio.h>
#include <time.h>

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
int tfs_getldt(const char *path, time_t *last_ddl_time);

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
 * Create temporary directory for cached files (cached ddl content). 
 * This folder may be removed on umount (depending on parameters).
 * */
int tfs_mkdir();


/**
 * Remove temporary directory for cached files (cached ddl content).
 * This is optionally called on umount.
 * */
int tfs_rmdir();

