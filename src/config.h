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
    char *database;
    char *schemas;
    int   keepcache;
    int   lowercase;
    int   filesize;
    char *loglevel;
} g_conf;


/**
 * Initializes g_conf using parameters from command line
 * or from fstab (using fuse-supplied functions). 
 * 
 * Return value should be used to start fuse_main().
 */
struct fuse_args parse_arguments(int argc, char *argv[]);
