#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <utime.h>

#include "config.h"
#include "logging.h"
#include "oracle.h"
#include "vfs.h"

#define LOB_BUFFER_SIZE 8196

static void str_lower(char *str) {
    for (int i = 0; str[i]; i++)
        str[i] = tolower(str[i]);
}

static void str_upper(char *str) {
    for (int i = 0; str[i]; i++)
        str[i] = toupper(str[i]);
}

static int str_append(char **dst, char *src) {
    int len = (*dst == NULL ? strlen(src) : strlen(*dst) + strlen(src));
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
    
    str_upper(type);
     
    if (strcmp(type, "JAVA_SOURCE") == 0)
        strcpy(suffix, ".JAVA");
    else if (strcmp(type, "JAVA_CLASS") == 0)
        strcpy(suffix, ".CLASS");
    else if (strcmp(type, "JAVA_RESOURCE") == 0)
        strcpy(suffix, ".RES"); // might be anything really, like .properties, .xml, .ini, ...
    else
        strcpy(suffix, ".SQL");
    
    logmsg(LOG_DEBUG, "str_suffix() - returning suffix [%s] for type [%s]", suffix, type);

    *dst = suffix;

    return EXIT_SUCCESS;
}

int str_fn2obj(char **dst, char *src, const char *objectType) {
    *dst = strdup(src);
    if (*dst == NULL) {
        logmsg(LOG_ERROR, "str_fn2obj() - Unable to malloc for dst string.");
        return EXIT_FAILURE;
    }
    if (g_conf.lowercase)
        str_upper(*dst);
    
    if (objectType != NULL) {
        char *expectedSuffix = NULL;       
        char *actualSuffix = NULL;
        
        if (str_suffix(&expectedSuffix, objectType) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "Unable to obtain suffix for object type [%s].", objectType);
            return EXIT_FAILURE;
        }
    
        // safety check
        int len = strlen(*dst);
        int expectedLen = strlen(expectedSuffix);
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
        
        for (int i = 0; i < expectedLen; i++) {
            int c = len-i-1;
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

static int str_fs2oratype(char **fstype) {
    char *type = *fstype;
    
    if (strcmp(type, "PACKAGE_SPEC") == 0) {
        free(type);
        type = strdup("PACKAGE");
    } else if (strcmp(type, "PACKAGE_BODY") == 0) {
        free(type);
        type = strdup("PACKAGE BODY");
    } else if (strcmp(type, "TYPE_BODY") == 0) {
        free(type);
        type = strdup("TYPE BODY");
    } else if (strcmp(type, "JAVA_SOURCE") == 0) {
        free(type);
        type = strdup("JAVA SOURCE");
    } else if (strcmp(type, "JAVA_CLASS") == 0) {
        free(type);
        type = strdup("JAVA CLASS");
    }

    if (type == NULL) {
        logmsg(LOG_ERROR, "str_fs2oratype() - unable to malloc for normalized type.");
        return EXIT_FAILURE;
    }
    
    *fstype= type;
    return EXIT_SUCCESS;
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
    OCIStmt   *o_stm = NULL;
    OCIDefine *o_def = NULL;
    char      *o_sel = malloc(256 * sizeof(char));
    OCIBind   *o_bnd[200];
    int        o_bnd_idx = 0;
    char tmp[50]; // for converting int to char*
    
    if (o_sel == NULL) {
        logmsg(LOG_ERROR, "Unable to malloc o_sel @ qry_schemas().");
        return EXIT_FAILURE;
    }
        
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

    str_append(&query, "SELECT username FROM all_users WHERE");
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
    if (ora_stmt_prepare(&o_stm, query)) {
        retval = EXIT_FAILURE;
        goto qry_schemas_cleanup;
    }
    if (ora_stmt_define(o_stm, &o_def, 1, (void*) o_sel, 256*sizeof(char), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_schemas_cleanup;
    }

    for (int i = 0; i < bind_in_i; i++) {
        ora_stmt_bind(o_stm, &o_bnd[o_bnd_idx], o_bnd_idx+1, bind_in[i], strlen(bind_in[i])+1, SQLT_STR);
        o_bnd_idx++;
    }

    for (int i = 0; i < bind_like_i; i++) {
        ora_stmt_bind(o_stm, &o_bnd[o_bnd_idx], o_bnd_idx+1, bind_like[i], strlen(bind_like[i])+1, SQLT_STR);
        o_bnd_idx++;
    }

    if (ora_stmt_execute(o_stm, 0)) {
        retval = EXIT_FAILURE;
        goto qry_schemas_cleanup;
    }    
    
    vfs_entry_free(g_vfs, 1);
    
    // loop through query results    
    while (ora_stmt_fetch(o_stm) == OCI_SUCCESS) {
        if (g_conf.lowercase)
            str_lower((char*) o_sel);
        t_fsentry *entry = vfs_entry_create('D', o_sel, time(NULL), time(NULL));
        vfs_entry_add(g_vfs, entry);
    }

    t_fsentry *ddllog = vfs_entry_create('F', "ddlfs.log", time(NULL), time(NULL));
    vfs_entry_add(g_vfs, ddllog);

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
    
    return retval;
}

int qry_types(t_fsentry *schema) {
    vfs_entry_free(schema, 1);
    
    char *types[] = {
        "function",
        "java_source",
        "package_body",
        "package_spec",
        "procedure",
        "type",
        "type_body",
        "view",
        NULL
    };

    for (int i = 0; types[i] != NULL; i++) {
        char *type = strdup(types[i]);
        if (type == NULL) {
            logmsg(LOG_ERROR, "qry_types() - unable to strdup type");
            return EXIT_FAILURE;
        }
        
        if (g_conf.lowercase)
            str_lower(type);
        else
            str_upper(type);
    
        vfs_entry_add(schema, vfs_entry_create('D', type,  time(NULL), time(NULL)));
        free(type);
    }
    
    return EXIT_SUCCESS;
}

int qry_objects(t_fsentry *schema, t_fsentry *type) {
    int retval = EXIT_SUCCESS;
    char *query = "select \
object_name, to_char(last_ddl_time, 'yyyy-mm-dd hh24:mi:ss') as t_modified \
from all_objects where owner=:bind_owner and object_type=:bind_type";    

    OCIStmt   *o_stm = NULL;
    OCIDefine *o_def[2] = {NULL, NULL};
    OCIBind   *o_bnd[2] = {NULL, NULL};
    char      *o_sel[2] = {NULL, NULL};

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
    if (g_conf.lowercase) {
        str_upper(schema_name);
        str_upper(type_name);
    }

    if (str_suffix(&suffix, type_name) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_objects() - Unable to obtain suffix for object type [%s]", type_name);
        if (type_name != NULL)
            free(type_name);
        if (schema_name != NULL)
            free(schema_name);
        return EXIT_FAILURE;
    }

    if (g_conf.lowercase)
        str_lower(suffix);

    if (str_fs2oratype(&type_name) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_objects() - unable to convert fs type to ora type.");
        if (type_name != NULL)
            free(type_name);
        if (schema_name != NULL)
            free(schema_name);
        return EXIT_FAILURE;
    }
    
    struct tm *temptime = malloc(sizeof(struct tm)); 

    vfs_entry_free(type, 1);

    for (int i = 0; i < 2; i++)
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

    if (ora_stmt_bind(o_stm, &o_bnd[0], 1, (void*) schema_name, strlen(schema_name)+1, SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bnd[1], 2, (void*) type_name, strlen(type_name)+1, SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

    if (ora_stmt_execute(o_stm, 0)) {
        retval = EXIT_FAILURE;
        goto qry_objects_cleanup;
    }

        

    while (ora_stmt_fetch(o_stm) == OCI_SUCCESS) {
        
        memset(temptime, 0, sizeof(struct tm));
        char* xx = strptime(((char*)o_sel[1]), "%Y-%m-%d %H:%M:%S", temptime);
        if (*xx != '\0') {
            logmsg(LOG_ERROR, "Unable to parse date!");
            retval = EXIT_FAILURE;
            goto qry_objects_cleanup;
        }

        time_t t_modified = timegm(temptime);
        /*
        char *fname = malloc((strlen((char*)o_sel[0])+4)*sizeof(char));
        strcpy(fname, (char*) o_sel[0]);
        strcat(fname, ".SQL");
        */
        char *fname = malloc((strlen((char*)o_sel[0])+strlen(suffix))*sizeof(char));
        strcpy(fname, (char*) o_sel[0]);
        strcat(fname, suffix);
         
        if (g_conf.lowercase)
            str_lower(fname);
        
        t_fsentry *entry = vfs_entry_create('F', 
            fname, 
            t_modified, 
            t_modified);
       
        free(fname);
        vfs_entry_add(type, entry);    
    }

qry_objects_cleanup:

    if (type_name != NULL)
        free(type_name);    

    if (temptime != NULL)
        free(temptime);

    if (suffix != NULL)
        free(suffix);
    
    if (o_stm != NULL)
        ora_check(OCIHandleFree(o_stm, OCI_HTYPE_STMT));

    for (int i = 0; i < 2; i++)
        if (o_sel[i] != NULL)
            free(o_sel[i]);

    return retval;
}

int qry_object_fname(const char *schema,
                     const char *type,
                     const char *object,
                     char **fname) {
    *fname = malloc(PATH_MAX * sizeof(char));
    if (*fname == NULL) {
        logmsg(LOG_ERROR, "Unable to malloc fname (size=%d)", PATH_MAX);
        return EXIT_FAILURE;
    }
    snprintf(*fname, PATH_MAX, "%s/ddlfs-%d-%s.%s.%s.tmp", 
        g_conf.temppath, getpid(), schema, type, object);
    return EXIT_SUCCESS;
}


static int qry_object_dbms_metadata(const char *schema, 
                                    const char *type, 
                                    const char *object,                                   
                                    const char *fname,
                                    const int is_java_source) {
    
    int retval = EXIT_SUCCESS;
    const char *query = 
"select dbms_metadata.get_ddl(\
:bind_type, :bind_object, :bind_schema) \
as retval from dual";

    OCILobLocator *o_lob = NULL; // free
    OCIStmt       *o_stm = NULL; // free
    OCIDefine     *o_def = NULL; // i *assume* following for OCIDefine as well:
    OCIBind       *o_bn1 = NULL; // The bind handles re freed implicitly when 
    OCIBind       *o_bn2 = NULL; // when the statement handle is deallocated.
    OCIBind       *o_bn3 = NULL;
    
    char *buf = malloc(LOB_BUFFER_SIZE); // free
    oraub8 buf_blen = LOB_BUFFER_SIZE;
    oraub8 buf_clen = 0;
    int lob_offset = 1;
    
    FILE *fp = NULL; // free    
    size_t bytes_written;
    
    if (buf == NULL) {
        logmsg(LOG_ERROR, "Unable to malloc lob buffer (size=%d).", LOB_BUFFER_SIZE);
        retval = EXIT_FAILURE;
        goto qry_object_dbms_metadata_cleanup;
    }
    
    if (ora_lob_alloc(&o_lob)) {
        retval = EXIT_FAILURE;
        goto qry_object_dbms_metadata_cleanup;
    }
    
    if (ora_stmt_prepare(&o_stm, query)) {
        retval = EXIT_FAILURE;
        goto qry_object_dbms_metadata_cleanup;
    }
      
    if (ora_stmt_define(o_stm, &o_def, 1, &o_lob, 0, SQLT_CLOB)) {
        retval = EXIT_FAILURE;
        goto qry_object_dbms_metadata_cleanup;
    }
   
    if (ora_stmt_bind(o_stm, &o_bn1, 1, (void*) type, strlen(type)+1, SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_object_dbms_metadata_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bn2, 2, (void*) object, strlen(object)+1, SQLT_STR))  {
        retval = EXIT_FAILURE;
        goto qry_object_dbms_metadata_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bn3, 3, (void*) schema, strlen(schema)+1, SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_object_dbms_metadata_cleanup;
    }

    if (ora_stmt_execute(o_stm, 1)) {
        retval = EXIT_FAILURE;
        goto qry_object_dbms_metadata_cleanup;
    }
    
    fp = fopen(fname, "w");
    if (fp == NULL) {
        logmsg(LOG_ERROR, "Unable to open %s. Error=%d.", fname, errno);
        retval = EXIT_FAILURE;
        goto qry_object_dbms_metadata_cleanup;
    }
    
    int first = 1;
    int first_offset = 0;
    while (buf_blen > 0) {
        if (ora_check(
            OCILobRead2(
                g_connection.svc,
                g_connection.err,
                o_lob,
                &buf_blen, 
                &buf_clen,
                lob_offset, // offset
                buf,
                LOB_BUFFER_SIZE, // buffer size
                OCI_ONE_PIECE, // Fs.
                NULL,
                NULL,
                0,
                SQLCS_IMPLICIT))) {
                    retval = EXIT_FAILURE;
                    goto qry_object_dbms_metadata_cleanup;
        }
        lob_offset += buf_blen;

        if (first) {
            // remove "create java source" from first line
            first_offset = 0;
            if (is_java_source == 1) {
                first_offset = 1; // because first character is always '\n'
                for (; first_offset < buf_blen; first_offset++)
                    if (buf[first_offset] == '\n' || buf[first_offset] == '\r')
                        break;
            }
            
            // trim leading spaces and newlines. I'm not sure why dbms_metdata
            // writes them anyway as they are totaly useless.
            for (; first_offset < buf_blen; first_offset++) 
                if (buf[first_offset] != ' ' && buf[first_offset] != '\n' && buf[first_offset] != '\r')
                    break;            
            
            bytes_written = fwrite(buf + first_offset, 1, buf_blen - first_offset, fp);
            buf_blen -= first_offset;
        } else { 
            bytes_written = fwrite(buf, 1, buf_blen, fp);
        }
        
        if (bytes_written != buf_blen) {
            retval = EXIT_FAILURE;
            logmsg(LOG_ERROR, "Bytes written (%d) != Bytes read (%d)", 
                bytes_written, buf_blen);
            goto qry_object_dbms_metadata_cleanup;
        }
        first=0;
    }
    
     
qry_object_dbms_metadata_cleanup:

    if (buf != NULL)
        free(buf);
    
    if (o_lob != NULL)
        ora_lob_free(o_lob);

    if (o_stm != NULL)
        ora_stmt_free(o_stm);

    if ( (fp != NULL) && (fclose(fp) != 0) )
        logmsg(LOG_ERROR, "qry_object_dbms_metadata() - Unable to close FILE* (cleanup)");
    
    return retval;
}

static int qry_object_all_source(const char *schema, 
                                       char *type,
                                 const char *object,
                                 const char *fname,
                                 const int is_java_source) {
    
    int retval = EXIT_SUCCESS;
    const char *query = 
"select NVL(\"TEXT\", '\n') from all_source \
where \"TYPE\"=:bind_type and \"NAME\"=:bind_object and \"OWNER\"=:bind_schema \
order by \"LINE\"";
     
    OCIStmt       *o_stm = NULL; // free
    OCIDefine     *o_def = NULL; // i *assume* following for OCIDefine as well:
    char          *o_sel = NULL;
    OCIBind       *o_bn1 = NULL; // The bind handles re freed implicitly when 
    OCIBind       *o_bn2 = NULL; // when the statement handle is deallocated.
    OCIBind       *o_bn3 = NULL;
    
    size_t bytes_written;
    FILE *fp = NULL; // free
    
    if ((o_sel = malloc(4096*sizeof(char))) == NULL) {
        logmsg(LOG_ERROR, "qry_object_all_source() - Unable to allocate memory for o_sel."); 
        return EXIT_FAILURE;
    }
    
    // query 
    if (ora_stmt_prepare(&o_stm, query)) {
        retval = EXIT_FAILURE;
        goto qry_object_all_source_cleanup;
    }
      
    if (ora_stmt_define(o_stm, &o_def, 1, o_sel, 4096*sizeof(char), SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_object_all_source_cleanup;
    }
   
    if (ora_stmt_bind(o_stm, &o_bn1, 1, (void*) type, strlen(type)+1, SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_object_all_source_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bn2, 2, (void*) object, strlen(object)+1, SQLT_STR))  {
        retval = EXIT_FAILURE;
        goto qry_object_all_source_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bn3, 3, (void*) schema, strlen(schema)+1, SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto qry_object_all_source_cleanup;
    }

    if (ora_stmt_execute(o_stm, 0)) {
        retval = EXIT_FAILURE;
        goto qry_object_all_source_cleanup;
    }
    
    fp = fopen(fname, "w");
    if (fp == NULL) {
        logmsg(LOG_ERROR, "Unable to open %s. Error=%d.", fname, errno);
        retval = EXIT_FAILURE;
        goto qry_object_all_source_cleanup;
    }

    if (!is_java_source) {
        sprintf(o_sel, "CREATE OR REPLACE %s \"%s\".", type, schema);
        fwrite(o_sel, 1, strlen(o_sel), fp);
    }
    
    int first = 1;
    int type_spaces = 0;
    char *tmp = type;
    char *org = NULL;

    for(; *tmp != '\0'; tmp++)
        if (*tmp == ' ')
            type_spaces++;

    while (ora_stmt_fetch(o_stm) == OCI_SUCCESS) {
        if (!is_java_source && first) {
            // replace multiple spaces with single space
            org = o_sel;
            tmp = o_sel;
            while (*tmp != '\0') {
                while (*tmp == ' ' && *(tmp + 1) == ' ')
                    tmp++;
                
                *(org++) = *(tmp++);
            }
            *org = '\0';

            // skip first word(s)
            org = o_sel;
            tmp = o_sel;
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
            bytes_written = fwrite(o_sel, 1, strlen(o_sel), fp);
            if (bytes_written != strlen(o_sel)) {
                retval = EXIT_FAILURE;
                logmsg(LOG_ERROR, "qry_object_all_source() - Bytes written (%d) != Bytes read (%d)", bytes_written, strlen(o_sel));
                goto qry_object_all_source_cleanup;
            }
            
            if (is_java_source)
                fwrite("\n", 1, 1, fp);
        }

        first=0;
    }
    
     
qry_object_all_source_cleanup:

    if (o_sel != NULL)
        free(o_sel);
    
    if (o_stm != NULL)
        ora_stmt_free(o_stm);

    if ( (fp != NULL) && (fclose(fp) != 0) )
        logmsg(LOG_ERROR, "qry_object_all_source() - Unable to close FILE* (qry_object_cleanup)");
    
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
    struct utimbuf newtime;    
    
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

    str_upper(object_type);
    if (str_fs2oratype(&object_type) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object() - unable to convert fs type to ora type.");
        if (object_schema != NULL) free(object_schema);
        if (object_type != NULL) free(object_type);
        if (object_name != NULL) free(object_name); 
        return EXIT_FAILURE;
    }
    
    is_java_source = ((strcmp(object_type, "JAVA SOURCE") == 0) ? 1 : 0);
    
    
    // actuall call correct implementation:
    if ((strcmp(type, "VIEW") == 0) || (strcmp(type, "view") == 0))
        // because only dbms_metadata supports getting source of VIEW objects 
        retval = qry_object_dbms_metadata(object_schema, object_type, object_name, *fname, is_java_source);
    else if ((strcmp(type, "JAVA_SOURCE") == 0) || (strcmp(type, "java_source") == 0))
        // because dbms_metadata strips all newlines from java source and is thus unusable for this purpose
        retval = qry_object_all_source(object_schema, object_type, object_name, *fname, is_java_source);
    else
        // either implementation could be used, but all_source should be slightly faster
        retval = qry_object_all_source(object_schema, object_type, object_name, *fname, is_java_source);

    // set timestamp
    newtime.actime = time(NULL);
    newtime.modtime = 0; // important. If this changes to anything else, we know that a write occured on
                         // underlying filesystem. (if file was opened as R/W, that does not mean it actually changed
                         // and that we need to execute DDL upon process closing it)
    if (utime(*fname, &newtime) == -1) {
        logmsg(LOG_ERROR, "qry_object() - unable to reset file modification time!");
        retval = EXIT_FAILURE;
    }
      
    // cleanup
    free(object_schema);
    free(object_type);
    free(object_name);
    
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

    if (ora_stmt_bind(o_stm, &o_bnd[0], 1, (void*) schema, strlen(schema)+1, SQLT_STR)) {
        retval = EXIT_FAILURE;
        goto log_ddl_errors_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bnd[1], 2, (void*) object, strlen(object)+1, SQLT_STR)) {
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
    char *ddl_msg = malloc(120 * sizeof(char));
    if (ddl_msg == NULL) {
        logmsg(LOG_ERROR, "qry_exec_ddl() - unable to malloc ddl_msg");
        return EXIT_FAILURE;
    }
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
                logmsg(LOG_DEBUG, "BEFORE SUCCESS_WITH_INFO");
                strcpy(ociret, "OCI_SUCCESS_WITH_INFO");
                logmsg(LOG_DEBUG, "AFTER_SUCCESS_WIH");
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

