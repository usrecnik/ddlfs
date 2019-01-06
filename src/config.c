#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <sys/types.h>
#ifndef _MSC_VER
	#include <unistd.h>
#else
	#pragma warning(disable:4996)
#endif

#include "logging.h"
#include "config.h"

enum {
     KEY_HELP,
};

#define MYFS_OPT(t, p, v) { t, offsetof(struct s_global_config, p), v }
static struct fuse_opt ddlfs_opts[] = {
    MYFS_OPT("username=%s", username,  1),
    MYFS_OPT("password=%s", password,  1),
    MYFS_OPT("database=%s", database,  1),
    MYFS_OPT("schemas=%s",  schemas,   1),
    MYFS_OPT("userrole=%s", userrole,  1),
    MYFS_OPT("loglevel=%s", loglevel,  1),
    MYFS_OPT("temppath=%s", temppath,  1),
    MYFS_OPT("filesize=%d", filesize,  1),
    MYFS_OPT("pdb=%s",      pdb,       1),
    MYFS_OPT("dbro",        dbro,      1),
    MYFS_OPT("dbrw",        dbro,      0),
    MYFS_OPT("keepcache",   keepcache, 1),
    MYFS_OPT("nokeepcache", keepcache, 0),

    FUSE_OPT_KEY("-h",      KEY_HELP),
    FUSE_OPT_KEY("--help",  KEY_HELP),
    FUSE_OPT_END
};

static int ddlfs_opts_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
    switch(key) {
        case KEY_HELP:
            // @todo - write an actual documentation:
            fprintf(stderr,
                "usage: %s mountpoint [options]\n"
                "\n"
                "DDLFS options:\n"
                "    -o mynum=NUM\n"
                "    -o mystring=STRING\n\n"
                , outargs->argv[0]);
            exit(1);
    }

	// to keep compiler quiet about unused data & arg params
	if (data == NULL || arg == NULL)
		fprintf(stderr, " ");
	
    return 1;
}

static int mandatory_parameter(const char *value, const char *parameter) {
    if (value == NULL) {
        logmsg(LOG_ERROR, "Parameter [%s] is mandatory.", parameter);
        return 1;
    }
    return 0;
}

struct fuse_args parse_arguments(int argc, char *argv[]) {

    g_conf.keepcache = -1;
	
	g_conf.mountpoint = calloc(1000, sizeof(char));
	g_conf.temppath	= calloc(1000, sizeof(char));
	g_conf.username = calloc(130, sizeof(char));
	g_conf.password = calloc(130, sizeof(char));
	g_conf.userrole = calloc(30, sizeof(char));
	g_conf.database = calloc(300, sizeof(char));
	g_conf.schemas = calloc(500, sizeof(char));
	g_conf.pdb = calloc(130, sizeof(char));
	g_conf.loglevel = calloc(15, sizeof(char));
	g_conf._temppath = calloc(1000, sizeof(char));
	
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	if (g_conf.mountpoint == NULL ||
		g_conf.temppath == NULL ||
		g_conf.username == NULL ||
		g_conf.password == NULL ||
		g_conf.userrole == NULL ||
		g_conf.database == NULL ||
		g_conf.schemas == NULL ||
		g_conf.pdb == NULL ||
		g_conf.loglevel == NULL ||
		g_conf._temppath == NULL) {
		
		logmsg(LOG_ERROR, "Unable to allocate memory for configuration entries (g_conf).");
		return args;
	}

	fuse_opt_parse(&args, &g_conf, ddlfs_opts, ddlfs_opts_proc);

    if (mandatory_parameter(g_conf.username, "username")) {
        args.argc = -1;
        return args;
    }

    if (mandatory_parameter(g_conf.password, "password")) {
        args.argc = -1;
        return args;
    }

    if (mandatory_parameter(g_conf.database, "database")) {
        args.argc = -1;
        return args;
    }

    if (g_conf.loglevel[0] == '\0')
        strcpy(g_conf.loglevel, "INFO");

    if (g_conf.schemas[0] == '\0')
        strcpy(g_conf.schemas, "%");

    if (g_conf.keepcache == -1)
        g_conf.keepcache = 0;

    if (g_conf.userrole[0] != '\0') {
        if (strcmp(g_conf.userrole, "SYSDBA") != 0 && strcmp(g_conf.userrole, "SYSOPER") != 0) {
            logmsg(LOG_ERROR, "Parameter userrole can only have the value of 'SYSDBA' or 'SYSOPER' if it is set.");
            args.argc = -1;
            return args;
        }
    }

    if (strlen(g_conf.schemas) > 500) {
        logmsg(LOG_ERROR, "Parameter 'schemas' can have at most 500 characters.");
        args.argc = -1;
        return args;
    }

    if (g_conf.temppath == NULL || g_conf.temppath[0] == '\0') {
#ifdef _MSC_VER
        char *wintmp = getenv("TEMP");
        if (wintmp == NULL)
		    strcpy(g_conf.temppath, "C:\\tmp\\");
        else
            strcpy(g_conf.temppath, wintmp);
#else
        strcpy(g_conf.temppath, "/tmp");
#endif
    }

#ifdef _MSC_VER
	g_conf._mount_pid = GetCurrentProcessId();
#else
    g_conf._mount_pid = getpid();
#endif

    g_conf._mount_stamp = time(NULL);

    logmsg(LOG_DEBUG, "Parameters:");
    logmsg(LOG_DEBUG, ".. username : [%s]", g_conf.username);
    logmsg(LOG_DEBUG, ".. password : [****]"); // intentionally hidden
    logmsg(LOG_DEBUG, ".. database : [%s]", g_conf.database);
    logmsg(LOG_DEBUG, ".. loglevel : [%s]", g_conf.loglevel);
    logmsg(LOG_DEBUG, ".. schemas  : [%s]", g_conf.schemas);
    logmsg(LOG_DEBUG, ".. userrole : [%s]", g_conf.userrole);
    logmsg(LOG_DEBUG, ".. temppath : [%s]", g_conf.temppath);
    logmsg(LOG_DEBUG, ".. filesize : [%d]", g_conf.filesize);
    logmsg(LOG_DEBUG, ".. keepcache: [%d]", g_conf.keepcache);
    logmsg(LOG_DEBUG, ".. pdb      : [%s]", g_conf.pdb);
    logmsg(LOG_DEBUG, ".. dbro     : [%d]", g_conf.dbro);
    logmsg(LOG_DEBUG, ".");

    return args;
}
