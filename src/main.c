#define FUSE_USE_VERSION 29

#include <fuse.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "logging.h"
#include "config.h"
#include "fuse-impl.h"
#include "oracle.h"
#include "query.h"
#include "vfs.h"

int main(int argc, char *argv[]) {
    memset(&g_conf, 0, sizeof(g_conf));
    g_conf.loglevel = "DEBUG";
    g_ddl_log_time = time(NULL);

    logmsg(LOG_INFO, "DDL Filesystem for Oracle Database, FUSE v%d.%d", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
    
    g_conf.mountpoint = realpath(argv[argc-1], NULL);
    logmsg(LOG_DEBUG, ".. mounting at [%s]", g_conf.mountpoint);

    logmsg(LOG_INFO, " ");
    logmsg(LOG_INFO, "-> mount <-");
        
    struct fuse_args args = parse_arguments(argc, argv);
    if (args.argc == -1) {
        logmsg(LOG_ERROR, "Missing or invalid parameters [%d], exiting.", args.argc);
        return 1;
    }
    
    // force single-threaded mode
    fuse_opt_add_arg(&args, "-s");

    struct fuse_operations oper = {
        .getattr  = fs_getattr,
        .readdir  = fs_readdir,
        .read     = fs_read,
        .write    = fs_write,
        .open     = fs_open,
        .create   = fs_create,
        .truncate = fs_truncate,
        .release  = fs_release,
        .unlink   = fs_unlink
    };
    
    if (ora_connect(g_conf.username, g_conf.password, g_conf.database) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "Unable to connect to database.");
        return EXIT_SUCCESS;
    }

    g_vfs = vfs_entry_create('D', "/", time(NULL), time(NULL));    
    
    logmsg(LOG_INFO, " ");
    logmsg(LOG_INFO, "-> event-loop <-");
    int r = fuse_main(args.argc, args.argv, &oper, NULL);
    
    logmsg(LOG_INFO, " ");
    logmsg(LOG_INFO, "-> umount <-");
    ora_disconnect();

    return r;
}

