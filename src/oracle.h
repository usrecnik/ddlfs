#pragma once

#include <oci.h>

/**
 * global variable to hold OCI connection state.
 * */
struct s_connection {
	OCIEnv* 	env;
   	OCIError*   err;
    OCIServer*  srv;
    OCISvcCtx*  svc;
    OCISession* ses;
} g_connection;

// @todo - this should be "static"
sword ora_check(sword status);

int ora_connect(char* username, char* password, char* database);

int ora_disconnect();

sword ora_lob_alloc(OCILobLocator **lob);

sword ora_lob_free(OCILobLocator *lob);

sword ora_stmt_prepare(OCIStmt **stm, const char *query);

sword ora_stmt_define(OCIStmt *stm, OCIDefine **def, ub4 pos, void *value, sb4 value_size, ub2 dty);

sword ora_stmt_define_i(OCIStmt *stm, OCIDefine **def, ub4 pos, void *value, sb4 value_size, ub2 dty, dvoid *indp);

sword ora_stmt_bind(OCIStmt *stm, OCIBind **bnd, ub4 pos, void *value, sb4 value_size, ub2 dty);

sword ora_stmt_execute(OCIStmt *stm, ub4 iters);

sword ora_stmt_fetch(OCIStmt *stm);

sword ora_stmt_free(OCIStmt *stm);


