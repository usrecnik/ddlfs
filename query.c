#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include "config.h"
#include "logging.h"
#include "oracle.h"
#include "vfs.h"

#define LOB_BUFFER_SIZE    8192

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
	char 	  *o_sel = malloc(256 * sizeof(char));
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
        "package_body",
        "package_spec",
        "procedure",
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
    if (type_name == NULL || schema_name == NULL) {
        logmsg(LOG_ERROR, "qry_objects() - Unable to strdup type_name and/or schema_name");
        return EXIT_FAILURE;
    }
    if (g_conf.lowercase) {
        str_upper(schema_name);
        str_upper(type_name);
    }        
    
	struct tm *temptime = malloc(sizeof(struct tm)); // @todo - free me

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
		char *fname = malloc((strlen((char*)o_sel[0])+4)*sizeof(char));
        strcpy(fname, (char*) o_sel[0]);
        strcat(fname, ".SQL");

        if (g_conf.lowercase)
            str_lower(fname);
        
		t_fsentry *entry = vfs_entry_create('F', 
			fname, 
			t_modified, 
			t_modified);
       
        free(fname);
		vfs_entry_add(type, entry);	
        logmsg(LOG_DEBUG, ".. OBJECT [%s]", entry->fname);
	}

qry_objects_cleanup:

    if (type_name != NULL)
        free(type_name);	

	if (temptime != NULL)
		free(temptime);
	
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

int qry_object(const char *schema, 
			   const char *type, 
			   const char *object,
			   char **fname) {
	
	int retval = EXIT_SUCCESS;
	char *query = 
"select dbms_metadata.get_ddl(\
:bind_type, :bind_object, :bind_schema) \
as retval from dual";

    char *object_schema = NULL; // those three variables represent object in correct case 
    char *object_type = NULL;   // according to g_conf.lowercase;
    char *object_name = NULL;   // object_name is also stripped of ".sql" suffix

	OCILobLocator *o_lob = NULL; // free
	OCIStmt 	  *o_stm = NULL; // free
	OCIDefine 	  *o_def = NULL; // i *assume* following for OCIDefine as well:
	OCIBind 	  *o_bn1 = NULL; // The bind handles re freed implicitly when 
	OCIBind 	  *o_bn2 = NULL; // when the statement handle is deallocated.
	OCIBind 	  *o_bn3 = NULL;
	
	char *buf = malloc(LOB_BUFFER_SIZE); // free
	oraub8 buf_blen = LOB_BUFFER_SIZE;
	oraub8 buf_clen = 0;
	int lob_offset = 1;

	FILE *fp = NULL; // free
	*fname = malloc(PATH_MAX * sizeof(char));

	if (*fname == NULL) {
		logmsg(LOG_ERROR, "Unable to malloc fname (size=%d)", PATH_MAX);
		return EXIT_FAILURE;
	}
    
	size_t bytes_written;
    if (qry_object_fname(schema, type, object, fname) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object() - unable to determine filenam, qry_object_fname() failed.");
        return EXIT_FAILURE;
    }
	logmsg(LOG_DEBUG, "Filename=[%s].", *fname);

    // convert names in correct case if necessary and strip ".sql" suffix from the name
    object_schema = strdup(schema);
    object_type = strdup(type);
    object_name = strdup(object);
    if (object_name == NULL || object_type == NULL || object_name == NULL) {
        logmsg(LOG_DEBUG, "Unable to malloc object_schema, object_type and/or object_name.");
        return EXIT_FAILURE;
    }
    object_name[strlen(object)-4] = '\0';
    if (g_conf.lowercase) {
        str_upper(object_schema);
        str_upper(object_type);
        str_upper(object_name);
    }

    logmsg(LOG_DEBUG, "**** DEBUG, object_name=[%s].[%s] - [%s]", object_schema, object_name, object_type);
    
    
	if (buf == NULL) {
		logmsg(LOG_ERROR, "Unable to malloc lob buffer (size=%d).", LOB_BUFFER_SIZE);
		return EXIT_FAILURE;
	}
	
	if (ora_lob_alloc(&o_lob)) {
        retval = EXIT_FAILURE;
		goto qry_object_cleanup;
	}
	
    if (ora_stmt_prepare(&o_stm, query)) {
		retval = EXIT_FAILURE;
		goto qry_object_cleanup;
	}
  	
    if (ora_stmt_define(o_stm, &o_def, 1, &o_lob, 0, SQLT_CLOB)) {
		retval = EXIT_FAILURE;
		goto qry_object_cleanup;
	}
   
    if (ora_stmt_bind(o_stm, &o_bn1, 1, (void*) object_type, strlen(object_type)+1, SQLT_STR)) {
		retval = EXIT_FAILURE;
		goto qry_object_cleanup;
	}

	if (ora_stmt_bind(o_stm, &o_bn2, 2, (void*) object_name, strlen(object_name)+1, SQLT_STR))  {
		retval = EXIT_FAILURE;
		goto qry_object_cleanup;
	}

    if (ora_stmt_bind(o_stm, &o_bn3, 3, (void*) object_schema, strlen(object_schema)+1, SQLT_STR)) {
		retval = EXIT_FAILURE;
		goto qry_object_cleanup;
	}

	if (ora_stmt_execute(o_stm, 1)) {
		retval = EXIT_FAILURE;
		goto qry_object_cleanup;
	}
	
	fp = fopen(*fname, "w");
	if (fp == NULL) {
		logmsg(LOG_ERROR, "Unable to open %s. Error=%d.", fname, errno);
		retval = EXIT_FAILURE;
		goto qry_object_cleanup;
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
					goto qry_object_cleanup;
		}
		lob_offset += buf_blen;

		if (first) {
			// trim leading spaces and newlines. I'm not sure why dbms_metdata
			// writes them anyway as they are totaly useless.
			for (first_offset = 0; first_offset < buf_blen; first_offset++)
				if (buf[first_offset] != ' ' && buf[first_offset] != '\n')
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
			goto qry_object_cleanup;
		}
		first=0;
	}

qry_object_cleanup:

    if (object_schema != NULL)
        free(object_schema);

    if (object_type != NULL)
        free(object_type);

    if (object_name != NULL)
        free(object_name);
    
	if (buf != NULL)
		free(buf);
	
	if (o_lob != NULL)
		ora_lob_free(o_lob);

	if (o_stm != NULL)
        ora_stmt_free(o_stm);

	if ( (fp != NULL) && (fclose(fp) != 0) )
		logmsg(LOG_ERROR, "Unable to close FILE* (qry_object_cleanup)");
	
	return retval;
}

int qry_exec_ddl(char *ddl) {
    int retval = EXIT_SUCCESS;
    OCIStmt *stm = NULL;
    
    if (ora_stmt_prepare(&stm, ddl)) {
        return EXIT_FAILURE;
    }

    logmsg(LOG_DEBUG, "Executing DDL (%s)", ddl);
    
    if (ora_stmt_execute(stm, 1)) {
        retval = EXIT_FAILURE;
        goto qry_exec_ddl_cleanup;
    }

    logmsg(LOG_DEBUG, "Executed DDL: [%s]", ddl);

qry_exec_ddl_cleanup:

    if (stm != NULL)
        ora_stmt_free(stm);

    return retval;
}
