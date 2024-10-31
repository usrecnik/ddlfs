#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#ifndef _MSC_VER
	// @todo: check if we actually need those
	#include <unistd.h>
	#include <utime.h>
#else
	#pragma warning(disable:4996)
	#include <sys/utime.h>
#endif

#include "config.h"
#include "logging.h"
#include "oracle.h"
#include "vfs.h"
#include "tempfs.h"
#include "util.h"
#include "query_tables.h"

#define DDLFS_PATH_MAX 8192
#define LOB_BUFFER_SIZE 8192


static int str_append(char **dst, char *src) {
    size_t len = (*dst == NULL ? strlen(src) : strlen(*dst) + strlen(src));
    char *tmp = malloc(len+2);
    if (tmp == NULL) {
        logmsg(LOG_ERROR, "Unable to malloc for tmp string_copy buffer (len=%d)", len);
        return EXIT_FAILURE;
    }
    if (*dst == NULL) {
        strcpy(tmp, src);
    } else {
        strcpy(tmp, *dst);
        strcat(tmp, src);
        free(*dst);
    }
    *dst = tmp;
    return EXIT_SUCCESS;
}

int str_suffix(char **dst, const char *objectType) {
    char *suffix = calloc(10, sizeof(char));
    char *type = strdup(objectType);
    if (suffix == NULL || type == NULL) {
        logmsg(LOG_ERROR, "str_suffix() - unable to malloc suffix and/or type.");
        if (suffix != NULL)
            free(suffix);
        if (type != NULL)
            free(type);
        return EXIT_FAILURE;
    }

    if (strcmp(type, "JAVA_SOURCE") == 0)
        strcpy(suffix, ".JAVA");
    else if (strcmp(type, "JAVA_CLASS") == 0)
        strcpy(suffix, ".CLASS");
    else if (strcmp(type, "JAVA_RESOURCE") == 0)
        strcpy(suffix, ".RES"); // might be anything really, like .properties, .xml, .ini, ...
    else
        strcpy(suffix, ".SQL");
    free(type);
    // logmsg(LOG_DEBUG, "str_suffix() - returning suffix [%s] for type [%s]", suffix, type);

    *dst = suffix;

    return EXIT_SUCCESS;
}

// @todo: this function should probably go to tempfs.c
int qry_object_fname(const char *schema,
                     const char *type,
                     const char *object,
                     char **fname) {
    *fname = malloc(DDLFS_PATH_MAX * sizeof(char));
    if (*fname == NULL) {
        logmsg(LOG_ERROR, "Unable to malloc fname (size=%d)", DDLFS_PATH_MAX);
        return EXIT_FAILURE;
    }
    snprintf(*fname, DDLFS_PATH_MAX, "%s%sddlfs-%s.%s.%s.tmp",
        g_conf._temppath, PATH_SEP, schema, type, object);
    return EXIT_SUCCESS;
}

int str_fn2obj(char **dst, char *src, const char *objectType) {
    *dst = strdup(src);
    if (*dst == NULL) {
        logmsg(LOG_ERROR, "str_fn2obj() - Unable to malloc for dst string.");
        return EXIT_FAILURE;
    }

    if (objectType != NULL) {
        char *expectedSuffix = NULL;
        char *actualSuffix = NULL;

        if (str_suffix(&expectedSuffix, objectType) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "Unable to obtain suffix for object type [%s].", objectType);
            return EXIT_FAILURE;
        }

        // safety check
        size_t len = strlen(*dst);
        size_t expectedLen = strlen(expectedSuffix);
        if (expectedLen >= len) {
            logmsg(LOG_ERROR, "Expected suffix [%s] is longer than original name [%s]", expectedSuffix, *dst);
            free(expectedSuffix);
            return EXIT_FAILURE;
        }

        actualSuffix = calloc(expectedLen+1, sizeof(char));
        if (actualSuffix == NULL) {
            logmsg(LOG_ERROR, "str_fn2obj() - unable to malloc actualSuffix, size=[%d]", expectedLen);
            return EXIT_FAILURE;
        }

        for (size_t i = 0; i < expectedLen; i++) {
            size_t c = len-i-1;
            if (c < 0)
                actualSuffix[i] = '\0';
            else
                actualSuffix[expectedLen-1-i] = (*dst)[c];
        }
        actualSuffix[expectedLen] = '\0';
        if (strcmp(expectedSuffix, actualSuffix) != 0) {
            logmsg(LOG_ERROR, "Expected suffix was [%s], got [%s] from [%s]", expectedSuffix, actualSuffix, *dst);
            free(actualSuffix);
            free(expectedSuffix);
            return EXIT_FAILURE;
        }

        // actually remove '.sql' suffix:
        (*dst)[strlen(*dst)-expectedLen] = '\0';

        free(actualSuffix);
        free(expectedSuffix);
    }

    return 0;
}

static int qry_object_all_source(const char *schema,
                                       char *type,
                                 const char *object,
                                 const char *fname,
                                 const  int is_java_source,
                                 const  int is_trigger_source) {

    int is_view_source = ((strcmp(type, "VIEW") == 0) ? 1 : 0);
    int is_mview_source = ((strcmp(type, "MATERIALIZED VIEW") == 0) ? 1 : 0);
    int retval = EXIT_SUCCESS;
    char query_fmt[1024] = "";
    char query[1024] = "";

    if (is_view_source)
        strcpy(query_fmt, // ALL_VIEWS
"select w.text as s,\
 o.status,\
 %s\
 from all_views w\
 join all_objects o on o.owner=w.owner and o.object_name=w.view_name and o.object_type=:bind_type\
 where w.view_name=:bind_object and w.owner=:bind_schema");
    else if (is_mview_source)
        strcpy(query, // ALL_MVIEWS, always non-editionable
"select w.query as s,\
 o.status,\
 null as e\
 from all_mviews w\
 join all_objects o on o.owner=w.owner and o.object_name=w.mview_name and o.object_type=:bind_type\
 where w.mview_name=:bind_object and w.owner=:bind_schema");
    else
        strcpy(query_fmt, // ALL_OBJECTS
"select nvl(s.\"TEXT\", '\n') as s,\
 o.status,\
 %s\
 from all_source s\
 join all_objects o on o.\"OWNER\"=s.\"OWNER\" and o.object_name=s.\"NAME\" and o.object_type=s.\"TYPE\"\
 and (o.object_type != 'TYPE' or o.subobject_name IS NULL)\
 where s.\"TYPE\"=:bind_type and s.\"NAME\"=:bind_object and s.\"OWNER\"=:bind_schema\
 order by s.\"LINE\"");

    if (!is_mview_source) {
        // all_objects.editionable column was introduced in 12.1
        if (g_conf._server_version <= 1102)
            snprintf(query, 1024, query_fmt, "null as e");
        else
            snprintf(query, 1024, query_fmt, "o.\"EDITIONABLE\" as e");
    }

    FILE *fp = NULL;

    ORA_STMT_PREPARE (qry_object_all_source);

    //---
    // Windows won't allow to allocate large arrays on stack (yes, it seems it considers 4mb large)
    // ORA_STMT_DEFINE_STR_I(qry_object_all_source, 1, text,        4*1024*1024);
    char *o_text = calloc(4*1024*1024, sizeof(char));
    if (o_text == NULL) {
        logmsg(LOG_ERROR, "qry_object_all_source(): Unable to allocate 4mb memory for o_text.");
        retval = EXIT_FAILURE;
        goto qry_object_all_source_cleanup;
    }
    sb2 i_text = 0;
    if (ora_stmt_define_i(o_stm, &o_def, 1, o_text, (4*1024*1024)*sizeof(char), SQLT_STR, (dvoid*) &i_text)) {
        logmsg(LOG_ERROR, "%s(): Unable to define %s", "qry_object_all_source", "text");
        retval = EXIT_FAILURE;
        goto qry_object_all_source_cleanup;
    }
    //---

    ORA_STMT_DEFINE_STR_I(qry_object_all_source, 2, valid,       10);
    ORA_STMT_DEFINE_STR_I(qry_object_all_source, 3, editionable, 2);
    ORA_STMT_BIND_STR(qry_object_all_source, 1, type);
    ORA_STMT_BIND_STR(qry_object_all_source, 2, object);
    ORA_STMT_BIND_STR(qry_object_all_source, 3, schema);
    ORA_STMT_EXECUTE(qry_object_all_source, 0);

    char tmpstr[4096];
    size_t bytes_written;
    int first = 1;
    int type_spaces = 0;
    char *tmp = type;
    char *org = NULL;
    char editionable[30] = "";
    int row_count = 0;
    int validity = -1; // -1=unknown, 0=>valid, 1=>invalid;

    for(; *tmp != '\0'; tmp++)
        if (*tmp == ' ')
            type_spaces++;

    while (ORA_STMT_FETCH) {
        row_count++;

        if (first) {

            if (strcmp(ORA_NVL(valid, "INVALID"), "VALID") == 0)
                validity = 0;
            else
                validity = 1;

            if (strcmp(ORA_NVL(editionable, "X"), "Y") == 0)
                 strcpy(editionable, " EDITIONABLE");
            else if (strcmp(ORA_NVL(editionable, "X"), "N") == 0)
                strcpy(editionable, " NONEDITIONABLE");
            else
                strcpy(editionable, ""); // object cannot be editioned at all (like tables for example)

            fp = fopen(fname, "w");
            if (fp == NULL) {
                logmsg(LOG_ERROR, "Unable to open %s. Error=%d.", fname, errno);
                retval = EXIT_FAILURE;
                goto qry_object_all_source_cleanup;
            }

            if (!is_java_source && !is_trigger_source && !is_view_source && !is_mview_source) {
                sprintf(tmpstr, "CREATE OR REPLACE%s %s \"%s\".", editionable, type, schema);
                fwrite(tmpstr, 1, strlen(tmpstr), fp);
            }

        }

        if (first && (is_view_source || is_mview_source)) {
            if (is_view_source)
                sprintf(tmpstr, "CREATE OR REPLACE FORCE%s %s \"%s\".\"%s\" AS \n", editionable, type, schema, object);
            else
                sprintf(tmpstr, "CREATE %s \"%s\".\"%s\" AS \n", type, schema, object);
            fwrite(tmpstr, 1, strlen(tmpstr), fp);

            if (i_text < 0) { // TEXT is null
                // TEXT (datatype=LONG): View text. This column returns the correct value only when the row originates
                // from the current container. The BEQUEATH clause will not appear as part of the TEXT column in
                // this view.
                strcpy(tmpstr, "   -- this view source is stored in another container /\n   select * from dual"); // @todo, comment in comment
                fwrite(tmpstr, 1, strlen(tmpstr), fp);
                break;
            }
        }

        if (first && is_trigger_source) {
            sprintf(tmpstr, "CREATE OR REPLACE%s %s \"%s\".\"%s\" ", editionable, type, schema, object);
            fwrite(tmpstr, 1, strlen(tmpstr), fp);

            // replace everything before 'BEFORE', 'AFTER', 'INSTEAD' with:
            // 'create or replace <editionable> trigger "<owner>"."<trigger-name>" '
            // DEBUG: trigger bucket_racuni_instead instead of update or delete or insert on bucket_racuni
            char *kw_all[3] = {"BEFORE", "AFTER", "INSTEAD"};
            char *kw_which = NULL; // which keyword out of kw_all was found, one of the 3 strings in kw_all
            char *kw_found = NULL; // return value form strcasestr, where did we found the kw_which
            char *kw_after = NULL; // char after keyword (it must be whitespace char, otherwise we found something that is not really a keyword)
            char *kw_before = NULL;
            int line = 1;
            while (kw_found == NULL) {

                for (int i = 0; i < 3; i++) {

                    char *kw_haystack = ORA_NVL(text, " ");
					char *kw_haystack_u = strdup(kw_haystack);
					if (kw_haystack_u == NULL) {
						logmsg(LOG_ERROR, "qry_object_all_source(): Unable to copy kw_haystack");
						goto qry_object_all_source_cleanup;
					}
					for (size_t j = 0; j < strlen(kw_haystack_u); j++)
						kw_haystack_u[j] = (char) toupper(kw_haystack_u[j]);

                    trigger_retry_keyword:
                    // kw_found = strcasestr(kw_haystack, kw_all[i]); // strcasestr is not available on Windows
					kw_found = strstr(kw_haystack_u, kw_all[i]);
                    if (kw_found != NULL) {
                        kw_which = kw_all[i];
                        kw_after = kw_found + strlen(kw_which);
                        kw_before = (kw_found == kw_haystack ? NULL : kw_found - 1);

                        if ((kw_after[0] != ' ' && kw_after[0] != '\n' && kw_after[0] != '\r' && kw_after[0] != '\t') ||
                            (kw_before != NULL && kw_before[0] != ' ' && kw_before[0] != '\n' && kw_before[0] != '\r' && kw_before[0] != '\t')) {
                            // so, this is not really a keyword
                            kw_haystack += strlen(kw_which);
                            kw_which = NULL;
                            kw_after = NULL;
                            kw_before = NULL;
                            kw_found = NULL;
                            goto trigger_retry_keyword;
                        }
                    }
                    if (kw_found != NULL)
                        break;
                }

                if (kw_found == NULL) {
                    // none of 3 keywords was found on this line, fetch next line
                    ora_stmt_fetch(o_stm);
                    line++;
                }
                if (line > 100) {
                    logmsg(LOG_ERROR, "Unable to find specific [before|after|instead] keyword in trigger [%s].[%s]", schema, object);
                    retval = EXIT_FAILURE;
                    goto qry_object_all_source_cleanup;
                }
            }
            if (kw_found == NULL) {
                logmsg(LOG_ERROR, "qry_object_all_source: Unable to find keyword for trigger [%s][%s]", schema, object);
                if (o_text != NULL) // very unlikely case, let's do our best here (even though probably a bit off):
                    fwrite(o_text, 1, strlen(o_text), fp);
            } else {
                fwrite(kw_found, 1, strlen(kw_found), fp);
            }

            first=0;
            continue;
        }

        if (first && !is_view_source && !is_java_source && !is_trigger_source && !is_mview_source) {
            // replace multiple spaces with single space
            org = ORA_NVL(text, " ");
            tmp = ORA_NVL(text, " ");
            while (*tmp != '\0') {
                while (*tmp == ' ' && *(tmp + 1) == ' ')
                    tmp++;

                *(org++) = *(tmp++);
            }
            *org = '\0';

            // skip first word(s)
            org = ORA_NVL(text, " ");
            tmp = ORA_NVL(text, " ");
            for (int i = 0; i < type_spaces+1; i++) {
                while (*tmp != '\0' && *tmp != ' ')
                    tmp++;
                if (*tmp == ' ')
                    tmp++;
            }

            bytes_written = fwrite(tmp, 1, strlen(tmp), fp);
            if (bytes_written != strlen(tmp)) {
                retval = EXIT_FAILURE;
                logmsg(LOG_ERROR, "qry_object_all_source() - Bytes written (%d) != Bytes read (%d)", bytes_written, strlen(tmp));
                goto qry_object_all_source_cleanup;
            }
        } else {
            bytes_written = fwrite(ORA_NVL(text, ""), 1, strlen(ORA_NVL(text, "")), fp);
            if (bytes_written != strlen(ORA_NVL(text, ""))) {
                retval = EXIT_FAILURE;
                logmsg(LOG_ERROR, "qry_object_all_source() - Bytes written (%d) != Bytes read (%d)", bytes_written, strlen(ORA_NVL(text, "")));
                goto qry_object_all_source_cleanup;
            }

            if (is_java_source) {
                // not every line of java source has nl character at the end.
                // it seems that only empty lines include newline character.
                char *java_line = ORA_NVL(text, "");
                char java_last = 'x';
                if (strlen(java_line) > 0)
                    java_last = java_line[strlen(java_line)-1];
                if (java_last != 10)
                  fwrite("\n", 1, 1, fp);
            }
        }

        first=0;
    }

    if (row_count == 0) {
        logmsg(LOG_ERROR, "There is no source in all_source for [%s] [%s].[%s]", type, schema, object);
        // Create empty file (if we die with error here, then mercurial/git probably won't work properly).
        //   This is Oracle Bug, objects without sources should never exist, although they do sometimes, like in this case,
        //   where object in PDB references object in CDB, which does not exist:
        //    SQL> select con_id, sharing, owner, object_name from cdb_objects where object_name='WWV_DBMS_SQL';
        //            CON_ID SHARING         OWNER      OBJECT_NAME
        //        ---------- --------------- ---------- ------------------------------
        //                 3 METADATA LINK   SYS        WWV_DBMS_SQL
        //                 3 METADATA LINK   SYS        WWV_DBMS_SQL
        //

        fp = fopen(fname, "w");
        if (fp == NULL) {
            logmsg(LOG_ERROR, "Unable to open %s. Error=%d (%s).", fname, errno, strerror(errno));
            retval = EXIT_FAILURE;
            goto qry_object_all_source_cleanup;
        }
        char empty_msg[] = "-- source for this object not found in all_source view.\n";
        fwrite(empty_msg, 1, strlen(empty_msg), fp);
    }

#ifndef _MSC_VER
	// this makes no sense on Windows anyway
    chmod(fname, validity == 0 ? 0744 : 0644);
#endif

qry_object_all_source_cleanup:
    ORA_STMT_FREE;

    if (o_text != NULL)
        free(o_text);
    
    if ( (fp != NULL) && (fclose(fp) != 0) )
        logmsg(LOG_ERROR, "qry_object_all_source() - Unable to close FILE* (qry_object_cleanup)");

    return retval;
  
}


static int qry_last_ddl_time(const char *schema,
                             const char *type,
                             const char *object,
                             time_t *last_ddl_time /* out */) {

    const char *query_user =
"select to_char(last_ddl_time, 'yyyy-mm-dd hh24:mi:ss') as last_ddl_time\
 from all_objects where owner=:schema and object_type=:type and object_name=:name";

    const char *query_dba = // @todo - do the decode part on client
"select o.mtime\
 from sys.obj$ o\
 join sys.user$ u on u.user# = o.owner#\
 where u.name=:schema and o.type#=:type and o.name=:name";

    const char *query = (g_conf._isdba == 1 ? query_dba : query_user);

    int retval = EXIT_SUCCESS;
    int type_id = 0;

    if (strcmp(type, "TABLE") == 0)
        type_id = 2;
    else if (strcmp(type, "VIEW") == 0)
        type_id = 4;
    else if (strcmp(type, "PROCEDURE") == 0)
        type_id = 7;
    else if (strcmp(type, "FUNCTION") == 0)
        type_id = 8;
    else if (strcmp(type, "PACKAGE") == 0)
        type_id = 9;
    else if (strcmp(type, "PACKAGE BODY") == 0)
        type_id = 11;
    else if (strcmp(type, "TRIGGER") == 0)
        type_id = 12;
    else if (strcmp(type, "TYPE") == 0)
        type_id = 13;
    else if (strcmp(type, "TYPE BODY") == 0)
        type_id = 14;
    else if (strcmp(type, "JAVA SOURCE") == 0)
        type_id = 28;
    else if (strcmp(type, "MATERIALIZED VIEW") == 0)
        type_id = 42;
    else {
        logmsg(LOG_ERROR, "qry_last_ddl_time(): Unsupported type [%s]!", type);
    }

    OCIBind *o_bn2;
    ORA_STMT_PREPARE(qry_last_ddl_time);
    ORA_STMT_DEFINE_STR(qry_last_ddl_time, 1, last_str_time, 30);
    ORA_STMT_BIND_STR(qry_last_ddl_time, 1, schema);
    int r = 0;
    if (g_conf._isdba == 1)
        ora_stmt_bind(o_stm, &o_bn2, 2, (void*) &type_id, sizeof(int), SQLT_INT);
    else
        ora_stmt_bind(o_stm, &o_bn2, 2, (void*) type, (sb4) strlen(type)+1, SQLT_STR);
    if (r) {
        logmsg(LOG_ERROR, "qry_last_ddl_time(): Unable to bind type");
        retval = EXIT_FAILURE;
        goto qry_last_ddl_time_cleanup;
    }
    ORA_STMT_BIND_STR(qry_last_ddl_time, 3, object);
    ORA_STMT_EXECUTE(qry_last_ddl_time, 0);
    if (ORA_STMT_FETCH) {
        *last_ddl_time = utl_str2time(ORA_VAL(last_str_time));
    } else {
        logmsg(LOG_ERROR, "Unable to obtain last_ddl_time for [%s].[%s] (%s) -> no such object in all_objects", schema, object, type);
        retval = EXIT_FAILURE;
    }

qry_last_ddl_time_cleanup:
    ORA_STMT_FREE;
    return retval;
}

int qry_object(char *schema,
               char *type,
               char *object,
               char **fname) {

    int retval = EXIT_SUCCESS;
    char *object_schema = NULL;
    char *object_type = NULL;
    char *object_name = NULL;
    int is_java_source = 0;
    int is_trigger_source = 0;
    struct utimbuf newtime;
    time_t last_ddl_time = 0;

    // determine fname
    if (qry_object_fname(schema, type, object, fname) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object_all_source() - unable to determine filename, qry_object_fname() failed.");
        return EXIT_FAILURE;
    }

    // normalize parameters
    if (str_fn2obj(&object_schema, schema, NULL) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object() - unable to normalize object_schema.");
        if (object_schema != NULL) free(object_schema);
        if (object_type != NULL) free(object_type);
        if (object_name != NULL) free(object_name);
        return EXIT_FAILURE;
    }

    if (str_fn2obj(&object_type, type, NULL) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object() - unable to normalize object_type.");
        if (object_schema != NULL) free(object_schema);
        if (object_type != NULL) free(object_type);
        if (object_name != NULL) free(object_name);
        return EXIT_FAILURE;
    }

    if (str_fn2obj(&object_name, object, type) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object() - unable to normalize object_name.");
        if (object_schema != NULL) free(object_schema);
        if (object_type != NULL) free(object_type);
        if (object_name != NULL) free(object_name);
        return EXIT_FAILURE;
    }

    if (utl_fs2oratype(&object_type) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object() - unable to convert fs type to ora type.");
        if (object_schema != NULL) free(object_schema);
        if (object_type != NULL) free(object_type);
        if (object_name != NULL) free(object_name);
        return EXIT_FAILURE;
    }

    is_java_source = ((strcmp(object_type, "JAVA SOURCE") == 0) ? 1 : 0);
    is_trigger_source = ((strcmp(object_type, "TRIGGER") == 0) ? 1 : 0);

    logmsg(LOG_DEBUG, "query %s: [%s].[%s]", object_type, object_schema, object_name);
    if (g_conf.dbro == 0 || (g_conf.dbro == 1 && tfs_quick_validate(*fname) != EXIT_SUCCESS)) {

        qry_last_ddl_time(object_schema, object_type, object_name, &last_ddl_time);
        if (tfs_validate2(*fname, last_ddl_time) == EXIT_SUCCESS) {
            logmsg(LOG_DEBUG, ".. got it from standard cache");
        } else {
            if (strcmp(object_type, "TABLE") == 0) {
                qry_object_all_tables(object_schema, object_name, *fname);
            } else {
                qry_object_all_source(object_schema, object_type, object_name, *fname, is_java_source, is_trigger_source);
            }
        }

        // set standard file attributes on cached file (atime & mtime)
        newtime.actime = time(NULL);
        newtime.modtime = 0; // important. If this changes to anything else, we know that a write occured on
                             // underlying filesystem. (if file was opened as R/W, that does not mean it actually changed
                             // and that we need to execute DDL upon process closing it)
        if (utime(*fname, &newtime) == -1) {
            logmsg(LOG_ERROR, "qry_object() - unable to reset file modification time!");
            retval = EXIT_FAILURE;
        }

        if (tfs_setldt(*fname, last_ddl_time) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "qry_object() - unable to set last_ddl_time on [%s], caching won't work (= disabled)", *fname);
            // this is not a fatal error - it should only have impact on performance, not functionality.
        } else {
            // (this si too verbose) logmsg(LOG_DEBUG, "qry_object() - set LDT for [%s] to [%d]", *fname, last_ddl_time);
        }
    } else {
        logmsg(LOG_DEBUG, ".. got it from quick cache");
    }

    free(object_schema);
    free(object_type);
    free(object_name);

    return retval;
}

int qry_schemas() {
    // variables
    int retval = EXIT_SUCCESS;
    char *query = NULL, *query_in = NULL, *query_like = NULL;
    char *input = strdup(g_conf.schemas);
    char *token;
    char *bind_in[100], *bind_like[100];
    int bind_in_i = 0, bind_like_i = 0;
    int pos_in, pos_like;
    int        o_bnd_idx = 0;
    char tmp[50]; // for converting int to char*

    // build query
    pos_in = 0;
    pos_like = 0;
    token = strtok(input, ":");
    while (token) {
        if (strstr(token, "%") == NULL) {
            if (pos_in != 0)
                str_append(&query_in, ", ");
            str_append(&query_in, ":bi_");
            snprintf(tmp, 50, "%d",pos_in++);
            str_append(&query_in, tmp);
            bind_in[bind_in_i] = strdup(token);
            bind_in_i++;
        } else {
            if (pos_like != 0)
                str_append(&query_like, " OR ");
            str_append(&query_like, "username LIKE :bl_");
            snprintf(tmp, 50, "%d", pos_like++);
            str_append(&query_like, tmp);
            bind_like[bind_like_i] = strdup(token);
            bind_like_i++;
        }
        token = strtok(NULL, ":");

        if (bind_in_i >= 100 || bind_like_i >= 100) {
            logmsg(LOG_ERROR, "Specified more than maximum number of schemas (allowed: 100x like, 100x in; got %dx in, %dx like", bind_in_i, bind_like_i);
            return EXIT_FAILURE;
        }
    }

    str_append(&query, "SELECT username, to_char(created, 'yyyy-mm-dd hh24:mi:ss') as created FROM all_users WHERE");
    if (pos_in != 0 && pos_like != 0) {
        str_append(&query, " username IN (");
        str_append(&query, query_in);
        str_append(&query, " OR ");
        str_append(&query, query_like);
    } else if (pos_in == 0 && pos_like != 0) {
        str_append(&query, " ");
        str_append(&query, query_like);
    } else if (pos_in != 0 && pos_like == 0) {
        str_append(&query, " username IN (");
        str_append(&query, query_in);
        str_append(&query, ")");
    } else {
        logmsg(LOG_ERROR, "Seems like no schemas is given in parameter?!");
    }
    str_append(&query, " ORDER BY username");

    // logmsg(LOG_DEBUG, "query=[%s]", query);
    // prepare and execute sql statement

    ORA_STMT_PREPARE (qry_schemas);
    ORA_STMT_DEFINE_STR_I(qry_schemas, 1, username, 300);
    ORA_STMT_DEFINE_STR_I(qry_schemas, 2, created,  30);

    for (int i = 0; i < bind_in_i; i++) {
        o_bnd_idx++;
        ORA_STMT_BIND_STR(qry_schemas, o_bnd_idx, bind_in[i]);
        //o_bnd_idx++;
    }

    for (int i = 0; i < bind_like_i; i++) {
        o_bnd_idx++;
        ORA_STMT_BIND_STR(qry_schemas, o_bnd_idx, bind_like[i]);
    }

    ORA_STMT_EXECUTE(qry_schemas, 0);

    // vfs_entry_free(g_vfs, 1);

    while (ORA_STMT_FETCH) {
        // convert created date
        /*
		struct tm *temptime = malloc(sizeof(struct tm));
        if (temptime == NULL) {
            logmsg(LOG_ERROR, "qry_schemas(): unable to allocate memory for temptime");
            retval = EXIT_FAILURE;
            goto qry_schemas_cleanup;
        }
        memset(temptime, 0, sizeof(struct tm));
        char* xx = strptime(ORA_VAL(created), "%Y-%m-%d %H:%M:%S", temptime);
        if (xx == NULL || *xx != '\0') {
            logmsg(LOG_ERROR, "qry_schemas(): unable to parse created date=[%s] for user=[%s]", ORA_VAL(username), ORA_VAL(created));
            retval = EXIT_FAILURE;
            free(temptime);
            goto qry_schemas_cleanup;
        }
        time_t created_time = timegm(temptime);
        free(temptime);
		*/
		time_t created_time = utl_str2time(ORA_VAL(created));
        // end of date conversion

        t_fsentry *entry = vfs_entry_create('D', o_username, created_time, created_time);
        t_fsentry *exists = vfs_entry_search(g_vfs, entry->fname);
        if (exists == NULL)
            vfs_entry_add(g_vfs, entry);
        else
            vfs_entry_free(entry, 0);
    }

    t_fsentry *ddllog = vfs_entry_create('F', "ddlfs.log", time(NULL), time(NULL));
    t_fsentry *exists = vfs_entry_search(g_vfs, ddllog->fname);
    if (exists == NULL)
        vfs_entry_add(g_vfs, ddllog);
    else
        vfs_entry_free(ddllog, 0);

    vfs_entry_sort(g_vfs);


qry_schemas_cleanup:
    if (input != NULL)
        free(input);

    if (query != NULL)
        free(query);

    if (query_in != NULL)
        free(query_in);

    if (query_like != NULL)
        free(query_like);

    if (o_stm != NULL)
        ora_stmt_free(o_stm);

    for (int i = 0; i < bind_in_i; i++)
        free(bind_in[i]);

     for (int i = 0; i < bind_like_i; i++)
        free(bind_like[i]);

    return retval;
}

int qry_types(t_fsentry *schema) {
    vfs_entry_free(schema, 1);

    char *types[] = {
        "FUNCTION",
        "JAVA_SOURCE",
        "MATERIALIZED_VIEW",
        "PACKAGE_BODY",
        "PACKAGE_SPEC",
        "PROCEDURE",
        "TABLE",
        "TRIGGER",
        "TYPE",
        "TYPE_BODY",
        "VIEW",
        NULL
    };

    time_t fixed_date = utl_str2time("1990-01-01 01:01:01");
    for (int i = 0; types[i] != NULL; i++)
        vfs_entry_add(schema, vfs_entry_create('D', types[i], fixed_date, fixed_date));

    vfs_entry_sort(schema);

    return EXIT_SUCCESS;
}

static int qry_objects_filesize(t_fsentry *schema, t_fsentry *type) {
    for (int i = 0; i < type->count; i++) {
        t_fsentry *object = type->children[i];
        char *fname; // physical temp file name

        qry_object(schema->fname, type->fname, object->fname, &fname);

        struct stat st;
        stat(fname, &st);

        object->fsize = st.st_size;

        free(fname);
    }

    return EXIT_SUCCESS;
}

int qry_objects(t_fsentry *schema, t_fsentry *type) {
    int retval = EXIT_SUCCESS;
    char query[500] = "select \
o.object_name, to_char(o.last_ddl_time, 'yyyy-mm-dd hh24:mi:ss') as t_modified, o.status \
from all_objects o where o.owner=:bind_owner and o.object_type=:bind_type and generated='N'";

    OCIStmt   *o_stm = NULL;
    OCIDefine *o_def[3] = {NULL, NULL, NULL};
    OCIBind   *o_bnd[2] = {NULL, NULL};
    char      *o_sel[3] = {NULL, NULL, NULL};

    char *schema_name = strdup(schema->fname);
    char *type_name = strdup(type->fname);
    char *suffix = NULL;

    if (type_name == NULL || schema_name == NULL) {
        logmsg(LOG_ERROR, "qry_objects() - Unable to strdup type_name and/or schema_name");
        if (type_name != NULL)
            free(type_name);
        if (schema_name != NULL)
            free(schema_name);
        return EXIT_FAILURE;
    }

    if ((strcmp(schema_name, "SYS") == 0) && (strcmp(type_name, "TYPE") == 0)) {
        strcat(query, " and exists (\
select 1 from all_source s \
where s.owner='SYS' and s.\"TYPE\"='TYPE' AND s.\"NAME\"=o.object_name)");
    }

    if (str_suffix(&suffix, type_name) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_objects() - Unable to obtain suffix for object type [%s]", type_name);
        if (type_name != NULL)
            free(type_name);
        if (schema_name != NULL)
            free(schema_name);
        return EXIT_FAILURE;
    }

    if (utl_fs2oratype(&type_name) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_objects() - unable to convert fs type to ora type.");
        if (type_name != NULL)
            free(type_name);
        if (schema_name != NULL)
            free(schema_name);
        return EXIT_FAILURE;
    }

    struct tm *temptime = malloc(sizeof(struct tm));

    vfs_entry_free(type, 1);

    for (int i = 0; i < 3; i++)
        if ((o_sel[i] = malloc(256*sizeof(char))) == NULL) {
            logmsg(LOG_ERROR, "Unable to allocate memory for sel[%d]", i);
            return EXIT_FAILURE;
    }

    if (ora_stmt_prepare(&o_stm, query)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

    if (ora_stmt_define(o_stm, &o_def[0], 1, (void*) o_sel[0], 256*sizeof(char), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

    if (ora_stmt_define(o_stm, &o_def[1], 2, (void*) o_sel[1], 256*sizeof(char), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

    if (ora_stmt_define(o_stm, &o_def[2], 3, (void*) o_sel[2], 256*sizeof(char), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bnd[0], 1, (void*) schema_name, (sb4) (strlen(schema_name)+1), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bnd[1], 2, (void*) type_name, (sb4) (strlen(type_name)+1), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

    if (ora_stmt_execute(o_stm, 0)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

    while (ora_stmt_fetch(o_stm) == OCI_SUCCESS) {
		/*
        memset(temptime, 0, sizeof(struct tm));
        char* xx = strptime(((char*)o_sel[1]), "%Y-%m-%d %H:%M:%S", temptime);
        if (*xx != '\0') {
            logmsg(LOG_ERROR, "Unable to parse date!");
            retval = EXIT_FAILURE;
            goto qry_objects_cleanup;
        }

        time_t t_modified = timegm(temptime);
		*/
		time_t t_modified = utl_str2time((char*)o_sel[1]);
        size_t fname_len = ((strlen((char*)o_sel[0])+strlen(suffix))+1)*sizeof(char);
        char *fname = malloc(fname_len);
        if (fname == NULL) {
            logmsg(LOG_ERROR, "qry_objects() - Unable to malloc for fname, fname_len=[%d]", fname_len);
            retval = EXIT_FAILURE;
            goto qry_objects_cleanup;
        }
        strcpy(fname, (char*) o_sel[0]);
        strcat(fname, suffix);

        // https://stackoverflow.com/questions/9847288/is-it-possible-to-use-in-a-filename
        char *tmp = fname;
        int has_slash = 0;
        while (*tmp != '\0')
            if (*(tmp++) == '/')
                has_slash = 1;

        if (has_slash) {
            logmsg(LOG_ERROR, "Skipping object named [%s], because it has '/' in the name.", fname);
            continue;
        }

        char ftype = (strcmp(o_sel[2], "VALID") == 0 ? 'F' : 'I');
        t_fsentry *entry = vfs_entry_create(ftype,
            fname,
            t_modified,
            t_modified);

        free(fname);

        vfs_entry_add(type, entry);
    }
    vfs_entry_sort(type);

    if (g_conf.filesize == -1)
        qry_objects_filesize(schema, type);

qry_objects_cleanup:

    if (schema_name != NULL)
        free(schema_name);

    if (type_name != NULL)
        free(type_name);

    if (temptime != NULL)
        free(temptime);

    if (suffix != NULL)
        free(suffix);

    if (o_stm != NULL)
        ora_check(OCIHandleFree(o_stm, OCI_HTYPE_STMT));

    for (int i = 0; i < 3; i++)
        if (o_sel[i] != NULL)
            free(o_sel[i]);

    return retval;
}

static int log_ddl_errors(const char *schema, const char *object) {
    const char *query = "SELECT \"ATTRIBUTE\" || ', line ' || \"LINE\" || ', column ' || \"POSITION\" || ': ' || \"TEXT\" as msg \
FROM all_errors \
WHERE \"OWNER\"=:object_schema AND \"NAME\"=:object_name \
ORDER BY \"SEQUENCE\"";

    int retval = EXIT_SUCCESS;
    OCIStmt   *o_stm = NULL;
    OCIDefine *o_def = NULL;
    char      *o_sel = NULL;
    OCIBind   *o_bnd[2] = {NULL, NULL};

    if ((o_sel = malloc(5000*sizeof(char))) == NULL) {
        logmsg(LOG_ERROR, "log_ddl_errors() - Unable to allocate memory for o_sel.");
        return EXIT_FAILURE;
    }

    if (ora_stmt_prepare(&o_stm, query)) {
        retval = EXIT_FAILURE;
        goto log_ddl_errors_cleanup;
    }

    if (ora_stmt_define(o_stm, &o_def, 1, (void*) o_sel, 5000*sizeof(char), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto log_ddl_errors_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bnd[0], 1, (void*) schema, (sb4) (strlen(schema)+1), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto log_ddl_errors_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bnd[1], 2, (void*) object, (sb4) (strlen(object)+1), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto log_ddl_errors_cleanup;
    }

    if (ora_stmt_execute(o_stm, 0)) {
        retval = EXIT_FAILURE;
        goto log_ddl_errors_cleanup;
    }

    while (ora_stmt_fetch(o_stm) == OCI_SUCCESS)
        logddl(".. %s", o_sel);


log_ddl_errors_cleanup:

    if (o_stm != NULL)
        ora_stmt_free(o_stm);

    if (o_sel != NULL)
        free(o_sel);

    return retval;
}

int qry_exec_ddl(char *schema, char *object, char *ddl) {
    int retval = EXIT_SUCCESS;
    OCIStmt *stm = NULL;
    char ddl_msg[120];

    // prepare log message (first 120 characters without newlines)
    strncpy(ddl_msg, ddl, 119);
    ddl_msg[119] = '\0';
    for (int i = 0; i < 120; i++) {
        switch (ddl_msg[i]) {
            case '\n':
            case '\r':
            case '\t':
                ddl_msg[i] = ' ';
        }
    }
    logddl(ddl_msg);

    if (ora_stmt_prepare(&stm, ddl)) {
        return EXIT_FAILURE;
    }

    logmsg(LOG_DEBUG, "Executing DDL (%s)", ddl); // @todo - remove this debug messages

    int ddlret = ora_stmt_execute(stm, 1);
    if (ddlret) {
        char ociret[50];
        retval = EXIT_FAILURE;
        switch (ddlret) {
            case OCI_SUCCESS_WITH_INFO:
                strcpy(ociret, "OCI_SUCCESS_WITH_INFO");
                break;

            case OCI_NEED_DATA:
                strcpy(ociret, "OCI_NEED_DATA");
                break;

            case OCI_NO_DATA:
                strcpy(ociret, "OCI_NO_DATA");
                break;

            case OCI_ERROR:
                strcpy(ociret, "OCI_ERROR");
                break;

            case OCI_INVALID_HANDLE:
                strcpy(ociret, "OCI_INVALID_HANDLE");
                break;

            case OCI_STILL_EXECUTING:
                strcpy(ociret, "OCI_STILL_EXECUTING");
                break;

            case OCI_CONTINUE:
                strcpy(ociret, "OCI_CONTINUE");
                break;

            default:
                strcpy(ociret, "OCI_UKNOWN");
                break;
        }
        logddl(".. %s,", ociret);
        if (log_ddl_errors(schema, object) != EXIT_SUCCESS)
            logmsg(LOG_ERROR, "qry_exec_ddl() - Unable to log ddl errors to ddlfs.log");
        logddl(" ");

        goto qry_exec_ddl_cleanup;
    }
    logddl(".. SUCCESS\n");


qry_exec_ddl_cleanup:

    if (stm != NULL)
        ora_stmt_free(stm);

    return retval;
}
