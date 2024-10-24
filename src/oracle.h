#pragma once

#include <oci.h>

#define ORA_PREFETCH_MEMORY 1024*1024 // 1 MB
#define ORA_PREFETCH_ROWS   1000

/**
 * global variable to hold OCI connection state.
 * */
struct s_connection {
	OCIEnv* 	env;
   	OCIError*   err;
    OCIServer*  srv;
    OCISvcCtx*  svc;
    OCISession* ses;
    int         read_only; // 0 => rw, 1=ro (select open_mode from v$database)
};

extern struct s_connection g_connection;

// @todo - this should be "static"
sword ora_check(sword status);

int ora_connect(char* username, char* password, char* database);

int ora_disconnect();

int ora_is_dba(int *dba);


sword ora_lob_alloc(OCILobLocator **lob);

sword ora_lob_free(OCILobLocator *lob);

sword ora_stmt_prepare(OCIStmt **stm, const char *query);

sword ora_stmt_define(OCIStmt *stm, OCIDefine **def, ub4 pos, void *value, sb4 value_size, ub2 dty);

sword ora_stmt_define_i(OCIStmt *stm, OCIDefine **def, ub4 pos, void *value, sb4 value_size, ub2 dty, dvoid *indp);

sword ora_stmt_bind(OCIStmt *stm, OCIBind **bnd, ub4 pos, void *value, sb4 value_size, ub2 dty);

sword ora_stmt_execute(OCIStmt *stm, ub4 iters);

sword ora_stmt_fetch(OCIStmt *stm);

sword ora_stmt_free(OCIStmt *stm);


// query: select open_mode from v$database
// @return: -1=error (probably due to insuficient privileges), 0=rw, 1=ro
int ora_get_open_mode();


#define ORA_STMT_PREPARE(PROC)                      OCIStmt *o_stm = NULL;\
                                                    OCIDefine *o_def = NULL;\
                                                    if (ora_stmt_prepare(&o_stm, query)) {\
                                                        logmsg(LOG_ERROR, "%s(): Unable to prepare statement [%s]", #PROC, query);\
                                                        retval = EXIT_FAILURE;\
                                                        goto PROC##_cleanup;\
                                                    }

#define ORA_STMT_DEFINE_STR(PROC, I, NAME, S)       char o_##NAME[S] = "\0";\
                                                    if (ora_stmt_define(o_stm, &o_def, I, o_##NAME, S*sizeof(char), SQLT_STR)) {\
                                                        logmsg(LOG_ERROR, "%s(): Unable to define %s", #PROC, #NAME);\
                                                        retval = EXIT_FAILURE;\
                                                        goto PROC##_cleanup;\
                                                    }

#define ORA_STMT_DEFINE_STR_I(PROC, I, NAME, S)     char o_##NAME[S] = "\0";\
                                                    sb2 i_##NAME = 0;\
                                                    if (ora_stmt_define_i(o_stm, &o_def, I, o_##NAME, S*sizeof(char), SQLT_STR, (dvoid*) &i_##NAME)) {\
                                                        logmsg(LOG_ERROR, "%s(): Unable to define %s", #PROC, #NAME);\
                                                        retval = EXIT_FAILURE;\
                                                        goto PROC##_cleanup;\
                                                    }

#define ORA_STMT_DEFINE_INT(PROC, I, NAME)          int o_##NAME = 0;\
                                                    if (ora_stmt_define(o_stm, &o_def, I, &o_##NAME, sizeof(int), SQLT_INT)) {\
                                                        logmsg(LOG_ERROR, "%s(): Unable to define %s", #PROC, #NAME);\
                                                        retval = EXIT_FAILURE;\
                                                        goto PROC##_cleanup;\
                                                    }

#define ORA_STMT_DEFINE_INT_I(PROC, I, NAME)        int o_##NAME = 0;\
                                                    sb2 i_##NAME = 0;\
                                                    if (ora_stmt_define_i(o_stm, &o_def, I, &o_##NAME, sizeof(int), SQLT_INT, (dvoid*) &i_##NAME)) {\
                                                        logmsg(LOG_ERROR, "%s(): Unable to define %s", #PROC, #NAME);\
                                                        retval = EXIT_FAILURE;\
                                                        goto PROC##_cleanup;\
                                                    }

#define ORA_STMT_BIND_STR(PROC, I, NAME)            OCIBind *o_bn##I = NULL;\
                                                    if (ora_stmt_bind(o_stm, &o_bn##I, I, (void*) NAME, (sb4) (strlen(NAME)+1), SQLT_STR)) {\
                                                        logmsg(LOG_ERROR, "%s(): Unable to bind %d", #PROC, #NAME);\
                                                        retval = EXIT_FAILURE;\
                                                        goto PROC##_cleanup;\
                                                    }

#define ORA_STMT_BIND_INT(PROC, I, NAME)            OCIBind *o_bn##I = NULL;\
                                                    if (ora_stmt_bind(o_stm, &o_bn##I, I, (void*) &NAME, sizeof(int), SQLT_INT)) {\
                                                        logmsg(LOG_ERROR, "%s(): Unable to bind %d", #PROC, #NAME);\
                                                        retval = EXIT_FAILURE;\
                                                        goto PROC##_cleanup;\
                                                    }

#define ORA_STMT_EXECUTE(PROC, FETCH)               if (ora_stmt_execute(o_stm, FETCH)) {\
                                                        logmsg(LOG_ERROR, "%s(): Unable to execute query", #PROC);\
                                                        retval = EXIT_FAILURE;\
                                                        goto PROC##_cleanup;\
                                                    }

#define ORA_STMT_FETCH                              ora_stmt_fetch(o_stm) == OCI_SUCCESS

#define ORA_NVL(NAME, FALLBACK)                     (i_##NAME == 0 ? o_##NAME : FALLBACK)

#define ORA_VAL(NAME)                               o_##NAME

#define ORA_STMT_FREE                               if (o_stm != NULL) ora_stmt_free(o_stm);
