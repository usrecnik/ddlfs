#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>

#include "logging.h"
#include "config.h"

enum {
     KEY_HELP,
};

#define MYFS_OPT(t, p, v) { t, offsetof(struct s_global_config, p), v }
static struct fuse_opt ddlfs_opts[] = {
    MYFS_OPT("username=%s", username, 1),
    MYFS_OPT("password=%s", password, 1),
    MYFS_OPT("database=%s", database, 1),
    MYFS_OPT("schemas=%s",  schemas,  1),
    MYFS_OPT("loglevel=%s", loglevel, 1),
    MYFS_OPT("temppath=%s", temppath, 1),
    MYFS_OPT("lowercase",   lowercase, 1),
    MYFS_OPT("nolowercase", lowercase, 0),
        
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

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
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

    if (g_conf.loglevel == NULL)
        g_conf.loglevel = "INFO";

    if (g_conf.schemas == NULL)
        g_conf.schemas = g_conf.username;

    if (strlen(g_conf.schemas) > 500) {
        logmsg(LOG_ERROR, "Parameter [%s] can have at most 500 characters.");
        args.argc = -1;
        return args;
    }
    
    if (g_conf.temppath == NULL)
        g_conf.temppath = "/tmp";
    
    logmsg(LOG_DEBUG, "Parameters:");
    logmsg(LOG_DEBUG, ".. username : [%s]", g_conf.username);
    logmsg(LOG_DEBUG, ".. password : [%s]", g_conf.password);
    logmsg(LOG_DEBUG, ".. database : [%s]", g_conf.database);
    logmsg(LOG_DEBUG, ".. loglevel : [%s]", g_conf.loglevel);
    logmsg(LOG_DEBUG, ".. schemas  : [%s]", g_conf.schemas);
    logmsg(LOG_DEBUG, ".. lowercase: [%d]", g_conf.lowercase);
    logmsg(LOG_DEBUG, ".. temppath : [%s]", g_conf.temppath);
    logmsg(LOG_DEBUG, ".");
    
    return args;
}