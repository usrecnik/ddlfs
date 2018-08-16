#pragma once

/*
 * Global configuration. 
 */
struct s_global_config {
    char *mountpoint;
    char *temppath;
    char *_temppath; 
    char *username;
    char *password;
    char *userrole;
    char *database;
    char *schemas;
    char *pdb;
    int   dbro;
    int   keepcache;
    int   lowercase;
    int   filesize;
    char *loglevel;
    int   _server_version;

    pid_t  _mount_pid;
    time_t _mount_stamp;
} g_conf;


/**
 * Initializes g_conf using parameters from command line
 * or from fstab (using fuse-supplied functions). 
 * 
 * Return value should be used to start fuse_main().
 */
struct fuse_args parse_arguments(int argc, char *argv[]);
