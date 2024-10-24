#pragma once

/*
 * Global configuration.
 */
struct s_global_config {
    char *mountpoint;
    char *temppath;
    char *username;
    char *password;
    char *userrole;
    char *database;
    char *schemas;
    char *pdb;
    int   dbro;
    int   keepcache;
    int   filesize;
    char *loglevel;

    int    _temppath_reused;
    char  *_temppath;
    int    _server_version;
    int    _isdba;
#ifndef _MSC_VER
    pid_t  _mount_pid;
#else
	int _mount_pid;
#endif
    time_t _mount_stamp;
};

extern struct s_global_config g_conf;


/**
 * Initializes g_conf using parameters from command line
 * or from fstab (using fuse-supplied functions).
 *
 * Return value should be used to start fuse_main().
 */
struct fuse_args parse_arguments(int argc, char *argv[]);
