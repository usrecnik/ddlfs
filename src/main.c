#include "fuse-version.h"

#include <fuse.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#ifndef _MSC_VER
	#include <unistd.h>
#endif

#include "logging.h"
#include "config.h"
#include "fuse-impl.h"
#include "oracle.h"
#include "query.h"
#include "vfs.h"
#include "tempfs.h"
#include "dbro_refresh.h"


#define DDLFS_VERSION "2.4"

void sigusr1_handler(int signo) {
    logmsg(LOG_INFO, " ");
    logmsg(LOG_INFO, "Received signal %d", signo);
#ifndef _MSC_VER
	if (signo == SIGUSR1) {
        vfs_dump(g_vfs, 0);
    }
#endif
    logmsg(LOG_INFO, "Signal handled");
}

int main(int argc, char *argv[]) {

    memset(&g_conf, 0, sizeof(g_conf));
    g_conf.dbro = -1;
	g_conf.loglevel = malloc(15 * sizeof(char));
	if (g_conf.loglevel == NULL) {
		logmsg(LOG_ERROR, "main(): unable to allocate memory for g_conf.loglevel");
		return 1;
	}
	strcpy(g_conf.loglevel, "INFO");

    g_ddl_log_time = time(NULL);

    logmsg(LOG_INFO, "DDL Filesystem v%s for Oracle Database, FUSE v%d.%d", DDLFS_VERSION, FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);

    g_vfs_last_schema = NULL;
#ifdef _MSC_VER
	g_conf.mountpoint = argv[argc-1];
#else
    g_conf.mountpoint = realpath(argv[argc-1], NULL);
#endif

	logmsg(LOG_DEBUG, ".. mounting at [%s]", g_conf.mountpoint);

    logmsg(LOG_DEBUG, " ");
    logmsg(LOG_DEBUG, "-> mount <-");

#ifndef _MSC_VER
	// this is Linux only feature
    if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR)
        logmsg(LOG_ERROR, "Unable to register SIGUSR1 signal handler");
#endif
	
    struct fuse_args args = parse_arguments(argc, argv);
    if (args.argc == -1) {
        logmsg(LOG_ERROR, "Missing or invalid parameters [%d], exiting.", args.argc);
        return 1;
    }
	
    // force single-threaded mode
    fuse_opt_add_arg(&args, "-s");

    #ifdef _MSC_VER
    struct fuse_operations oper = {
        .getattr  = fs_getattr_v29,
        .readdir  = fs_readdir_v29,
        .read     = fs_read,
        .write    = fs_write,
        .open     = fs_open,
        .create   = fs_create,
        .truncate = fs_truncate,
        .release  = fs_release,
        .unlink   = fs_unlink
    };
    #else
    struct fuse_operations oper = {
        .getattr  = fs_getattr_v30,
        .readdir  = fs_readdir_v30,
        .read     = fs_read,
        .write    = fs_write,
        .open     = fs_open,
        .create   = fs_create,
        .truncate = fs_truncate,
        .release  = fs_release,
        .unlink   = fs_unlink
    };
	#endif

    if (ora_connect(g_conf.username, g_conf.password, g_conf.database) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "Unable to connect to database.");
        return EXIT_FAILURE;
    }

    logmsg(LOG_DEBUG, "is_sysdba=[%d]", g_conf._isdba);
	
    if (g_conf.dbro == -1) {
        if (g_conf._isdba == 1) {
            logmsg(LOG_DEBUG, "neither dbro nor dbrw parameter given, thus trying 'select open_mode from v$database'");
            g_conf.dbro = ora_get_open_mode();
            if (g_conf.dbro == -1) {
                logmsg(LOG_DEBUG, ".. unable to query v$database, assuming the database is opened as read/write");
                g_conf.dbro = 0;
            }
        } else {
            logmsg(LOG_DEBUG, "assuming database opened as read/write because this user has no (sys)dba privileges (needed to obtain this info).");
            g_conf.dbro = 0;
        }
    }
    logmsg(LOG_DEBUG, "dbro=[%d]", g_conf.dbro);

    if (tfs_mkdir() != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "Unable to initialize temp directory (%s)", g_conf.temppath);
        return EXIT_FAILURE;
    }

    if (g_conf._temppath_reused == 1 && g_conf.dbro == 1) {
        logmsg(LOG_INFO, "Cache validation started beacuase temppath_reused=[%d] and dbro=[%d]", g_conf._temppath_reused, g_conf.dbro);
        if (dbr_refresh_cache() != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "Cache validation failed");
            return EXIT_FAILURE;
        }
        logmsg(LOG_INFO, "Cache validation completed.");
    }

    g_vfs = vfs_entry_create('D', "/", time(NULL), time(NULL));

    logmsg(LOG_DEBUG, " ");
    logmsg(LOG_DEBUG, "-> event-loop <-");
    printf("\n");
    int r = fuse_main(args.argc, args.argv, &oper, NULL);

    logmsg(LOG_DEBUG, " ");
    logmsg(LOG_DEBUG, "-> umount <-");
    ora_disconnect();

    if (g_conf.keepcache == 0) {
        if (tfs_rmdir(0) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "Unable to remove cache directory after mount (config keepcache=0).");
        }
    }

    return r;
}
