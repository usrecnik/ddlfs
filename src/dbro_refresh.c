#include <stdio.h>
#include <stdlib.h>

#include "logging.h"
#include "oracle.h"
#include "tempfs.h"
#include "util.h"
#include "query.h"

static int dbr_refresh_object(const char *schema,
                              const char *type,
                              const char *object,
                              time_t last_ddl_time) {

    char object_with_suffix[300];

    // get suffix based on type
    char *suffix = NULL;
    if (str_suffix(&suffix, type) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "dbr_refresh_object(): unable to determine file suffix");
        if (suffix != NULL)
            free(suffix);
        return EXIT_FAILURE;
    }
    snprintf(object_with_suffix, 299, "%s%s", object, suffix);

    // get cache filename
    char *fname = NULL;
    if (qry_object_fname(schema, type, object_with_suffix, &fname) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "dbr_refresh_object(): unable to determine cache filename for [%s] [%s].[%s]", type, schema, object_with_suffix);
        if (fname != NULL)
            free(fname);
        if (suffix != NULL)
            free(suffix);
        return EXIT_FAILURE;
    }

    // if cache file is already up2date
    if (tfs_validate2(fname, last_ddl_time) == EXIT_SUCCESS) {
        // then mark it as verified by this mount
        if (tfs_setldt(fname, last_ddl_time) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "dbr_refresh_object(): unable to mark [%s] [%s].[%s] as verified by this mount.", type, schema, object);
            if (fname != NULL)
                free(fname);
            if (suffix != NULL)
                free(suffix);
            return EXIT_FAILURE;
        }
    }

    free(fname);
    free(suffix);
    return EXIT_SUCCESS;
}

int dbr_refresh_cache() {
    int retval = EXIT_SUCCESS;
    const char *query =
"select o.owner, o.object_type, o.object_name, \
 to_char(o.last_ddl_time, 'yyyy-mm-dd hh24:mi:ss') as last_ddl_time\
 from dba_objects o\
 where generated='N'\
 and (o.object_type != 'TYPE' or o.subobject_name IS NULL)\
 and object_type IN (\
 'TABLE',\
 'VIEW',\
 'PROCEDURE',\
 'FUNCTION',\
 'PACKAGE',\
 'PACKAGE BODY',\
 'TRIGGER',\
 'TYPE',\
 'TYPE BODY',\
 'JAVA SOURCE')";

    ORA_STMT_PREPARE(dbr_refresh_state);
    ORA_STMT_DEFINE_STR_I(dbr_refresh_state, 1, schema, 300);
    ORA_STMT_DEFINE_STR_I(dbr_refresh_state, 2, type, 300);
    ORA_STMT_DEFINE_STR_I(dbr_refresh_state, 3, object, 300);
    ORA_STMT_DEFINE_STR_I(dbr_refresh_state, 4, last_ddl_time, 25);
    ORA_STMT_EXECUTE(dbr_refresh_state, 0);
    while (ORA_STMT_FETCH) {
        dbr_refresh_object(
            ORA_NVL(schema, "_UNKNOWN_SCHEMA_"),
            ORA_NVL(type, "_UNKNOWN_TYPE_"),
            ORA_NVL(object, "_UNKNOWN_OBJECT_"),
            utl_str2time(ORA_NVL(last_ddl_time, "1990-01-01 03:00:01")));
    }

dbr_refresh_state_cleanup:
    ORA_STMT_FREE;
    return retval;
}
