#include <oci.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
	#pragma warning(disable:4996)
#endif

#include "logging.h"
#include "config.h"
#include "oracle.h"

#define MAJOR_NUMVSN(v) ((sword)(((v) >> 24) & 0x000000FF))      /* version number */ 
#define MINOR_NUMRLS(v) ((sword)(((v) >> 20) & 0x0000000F))      /* release number */

sword ora_check(sword status) {
    text errbuf[512];
    sb4 errcode;

    switch (status) {
        case OCI_SUCCESS:
            break;
  
        case OCI_SUCCESS_WITH_INFO:
            OCIErrorGet(
                g_connection.err, (ub4) 1, (text *) NULL, &errcode, errbuf, 
                (ub4) sizeof(errbuf), (ub4) OCI_HTYPE_ERROR);
            if (errcode != 24347) //ORA-24347: Warning of a NULL column in an aggregate function (any query on user_role_privs will produce this warning on 11.2.0.3
                logmsg(LOG_ERROR, "OCI_SUCCESS_WITH_INFO: ORA-%d: %s", errcode, (char *)errbuf);
            return OCI_SUCCESS;
  
        case OCI_NEED_DATA:
            logmsg(LOG_ERROR, "OCI_NEED_DATA");
            break;

        case OCI_NO_DATA:
            logmsg(LOG_ERROR, "OCI_NO_DATA");
            break;

        case OCI_ERROR:
            OCIErrorGet(
                g_connection.err, (ub4) 1, (text *) NULL, &errcode, errbuf, 
                (ub4) sizeof(errbuf), (ub4) OCI_HTYPE_ERROR);
            logmsg(LOG_ERROR, "errcode=%d||%s", errcode, (char *)errbuf);
            break;

        case OCI_INVALID_HANDLE:
            logmsg(LOG_ERROR, "OCI_INVALID_HANDLE");
            break;

        case OCI_STILL_EXECUTING:
            logmsg(LOG_ERROR, "OCI_STILL_EXECUTING");
            break;

        case OCI_CONTINUE:
            logmsg(LOG_ERROR, "CI_CONTINUE");
            break;
    
        default:
            break;
    }
    return status;
}
 
// @return: -1=error, 0=rw, 1=ro
int ora_get_open_mode() {
    const char *query = "select open_mode from v$database";
    int retval = -1;

    ORA_STMT_PREPARE(ora_get_open_mode);
    ORA_STMT_DEFINE_STR(ora_get_open_mode, 1, open_mode, 30);
    ORA_STMT_EXECUTE(ora_get_open_mode, 1);
    logmsg(LOG_DEBUG, "open_mode=[%s]", ORA_VAL(open_mode));

    if (strcmp(ORA_VAL(open_mode), "READ ONLY"))
        retval = 1;
    else
        retval = 0;
    
ora_get_open_mode_cleanup:
    ORA_STMT_FREE;
    return retval;
}

int ora_connect(char* username, char* password, char* database) {
	sword r = 0;

	g_connection.env = 0;
	g_connection.err = 0;
	g_connection.srv = 0;
	g_connection.svc = 0;
	g_connection.ses = 0;
	g_connection.read_only = 0;
    
    // https://docs.oracle.com/cd/E11882_01/appdev.112/e10646/oci16rel001.htm#LNOCI17121
    ub4 auth_type = OCI_CRED_RDBMS;
    ub4 user_role = OCI_DEFAULT;

    if (strncmp(username, "/", 1) == 0)
        auth_type = OCI_CRED_EXT;

    g_conf._isdba = 0;
    if (g_conf.userrole != NULL && strcmp(g_conf.userrole, "SYSDBA") == 0) {
        user_role = OCI_SYSDBA;
        g_conf._isdba = 1;
    }

    if (g_conf.userrole != NULL && strcmp(g_conf.userrole, "SYSOPER") == 0)
        user_role = OCI_SYSOPER;
    
    
    logmsg(LOG_DEBUG, "Connecting to Oracle Database (%s)", database);
    
    if (auth_type == OCI_CRED_EXT)
        logmsg(LOG_DEBUG, ".. using external authentication.");

    r = OCIEnvCreate(&g_connection.env, OCI_DEFAULT, 0, 0, 0, 0, 0, 0);
    if (ora_check(r) != OCI_SUCCESS)
        return EXIT_FAILURE;

    logmsg(LOG_DEBUG, ".. allocating handles.");
    OCIHandleAlloc(g_connection.env, (dvoid**)&g_connection.err, OCI_HTYPE_ERROR,   0, 0);
    OCIHandleAlloc(g_connection.env, (dvoid**)&g_connection.srv, OCI_HTYPE_SERVER,  0, 0);
    OCIHandleAlloc(g_connection.env, (dvoid**)&g_connection.svc, OCI_HTYPE_SVCCTX,  0, 0);
    OCIHandleAlloc(g_connection.env, (dvoid**)&g_connection.ses, OCI_HTYPE_SESSION, 0, 0);
    
    if (strcmp(g_conf.database, "/") == 0) {
        logmsg(LOG_DEBUG, ".. attaching to server process to *DEFAULT* host.");
        if (auth_type == OCI_CRED_EXT)
            r = OCIServerAttach(
                g_connection.srv,
                g_connection.err,
                NULL,
                0,
                (ub4) OCI_DEFAULT);
    } else {
        logmsg(LOG_DEBUG, ".. attaching server process.");
        r = OCIServerAttach(
            g_connection.srv, 
            g_connection.err, 
            (text*) database, 
            (sb4) (strlen(database)),
            (ub4) OCI_DEFAULT);
    }
    
    if (ora_check(r))
        return EXIT_FAILURE;
    
    logmsg(LOG_DEBUG, ".. setting session attributes (username=%s).", username);
    OCIAttrSet(g_connection.svc, OCI_HTYPE_SVCCTX, g_connection.srv, 0, OCI_ATTR_SERVER, g_connection.err);

    if (auth_type == OCI_CRED_RDBMS) {
        OCIAttrSet(g_connection.ses, OCI_HTYPE_SESSION, username, (sb4) (strlen(username)), OCI_ATTR_USERNAME, g_connection.err); 
        OCIAttrSet(g_connection.ses, OCI_HTYPE_SESSION, password, (sb4) (strlen(password)), OCI_ATTR_PASSWORD, g_connection.err);
    }

    logmsg(LOG_DEBUG, ".. starting [%s] database session.", (g_conf.userrole == NULL ? "default" : g_conf.userrole));
    if (ora_check(OCISessionBegin (
        g_connection.svc, 
        g_connection.err, 
        g_connection.ses,
        auth_type, user_role)))
            return EXIT_FAILURE;

    logmsg(LOG_DEBUG, ".. registering database session.");
    OCIAttrSet(g_connection.svc, OCI_HTYPE_SVCCTX, g_connection.ses, 0, OCI_ATTR_SESSION, g_connection.err);

    logmsg(LOG_DEBUG, ".. determining database version.");
    char version_str[20] = "";
    ub4 version_int = 0;
    if (ora_check(OCIServerRelease( g_connection.svc, g_connection.err,  
                         (OraText*) version_str, 20, OCI_HTYPE_SVCCTX, 
                         &version_int)))
        return EXIT_FAILURE;
    
    int major = MAJOR_NUMVSN(version_int);
    int minor = MINOR_NUMRLS(version_int);
    g_conf._server_version = major*100+minor;
    logmsg(LOG_DEBUG, ".. connected to server version [%s] [%d]", version_str, g_conf._server_version);


    if (g_conf.pdb != NULL) {
        OCIStmt *o_stm = NULL;
        char alter_session_sql[250];
        
        sprintf(alter_session_sql, "alter session set container=%s", g_conf.pdb);
        logmsg(LOG_INFO, ".. sql: [%s]", alter_session_sql);
        
        if (ora_stmt_prepare(&o_stm, alter_session_sql)) {
            logmsg(LOG_ERROR, "Unable to prepare: [%s]", alter_session_sql);
            return EXIT_FAILURE;
        }
        
        if (ora_stmt_execute(o_stm, 1)) {
            logmsg(LOG_ERROR, "Unable to execute: [%s]", alter_session_sql);
            return EXIT_FAILURE;
        }
        
        if (o_stm != NULL)
            ora_stmt_free(o_stm);
    }

    logmsg(LOG_INFO, ".. connected to database server.");

    return EXIT_SUCCESS;
}

int ora_disconnect() {
    logmsg(LOG_INFO, "Disconnecting from Oracle Database.");
    
    logmsg(LOG_DEBUG, ".. freeing handles");
    OCIHandleFree(g_connection.env, OCI_HTYPE_ENV   );
    OCIHandleFree(g_connection.err, OCI_HTYPE_ERROR );
    OCIHandleFree(g_connection.srv, OCI_HTYPE_SERVER);
    OCIHandleFree(g_connection.svc, OCI_HTYPE_SVCCTX);

    logmsg(LOG_DEBUG, ".. terminating OCI.");
    OCITerminate(OCI_DEFAULT);

    logmsg(LOG_INFO, ".. disconnected.");

    return 0;
}


sword ora_stmt_prepare(OCIStmt **stm, const char *query) {
    sword r;

    sb4 prefetch_memory = ORA_PREFETCH_MEMORY;
    sb4 prefetch_rows = ORA_PREFETCH_ROWS;

    r = OCIHandleAlloc(g_connection.env, (void **) stm, OCI_HTYPE_STMT, 0, 0);
    if (ora_check(r))
        return r;

    r = OCIStmtPrepare(*stm, g_connection.err, (text*) query, (sb4) strlen(query), OCI_NTV_SYNTAX, OCI_DEFAULT);
    if (ora_check(r))
        return r;
    
    r = OCIAttrSet(*stm, OCI_HTYPE_STMT, &prefetch_memory, sizeof(prefetch_memory), OCI_ATTR_PREFETCH_MEMORY, g_connection.err);
    if (ora_check(r))
        return r;

    r = OCIAttrSet(*stm, OCI_HTYPE_STMT, &prefetch_rows, sizeof(prefetch_rows), OCI_ATTR_PREFETCH_ROWS, g_connection.err);
    if (ora_check(r))
        return r;
    
    return r;
}

sword ora_stmt_define(OCIStmt *stm, OCIDefine **def, ub4 pos, void *value, sb4 value_size, ub2 dty) {
    sword r = OCIDefineByPos(
        stm, def, g_connection.err, pos, value, value_size, dty,  
        0, 0, 0, OCI_DEFAULT);
    ora_check(r);
    return r;
}

sword ora_stmt_define_i(OCIStmt *stm, OCIDefine **def, ub4 pos, void *value, sb4 value_size, ub2 dty, dvoid *indp) {
    sword r = OCIDefineByPos(
        stm, def, g_connection.err, pos, value, value_size, dty, indp, 0, 0, OCI_DEFAULT);
    ora_check(r);
    return r;
}

sword ora_stmt_bind(OCIStmt *stm, OCIBind **bnd, ub4 pos, void *value, sb4 value_size, ub2 dty) {
    sword r = OCIBindByPos(
        stm, bnd, g_connection.err, pos, value, value_size, dty, 
        0, 0, 0, 0, 0, OCI_DEFAULT);
    ora_check(r);
    return r;
}

sword ora_stmt_execute(OCIStmt *stm, ub4 iters) {
    sword r = OCIStmtExecute(
        g_connection.svc, stm, g_connection.err, iters,
        0, 0, 0, OCI_DEFAULT);
    ora_check(r);
    return r;
}

sword ora_stmt_fetch(OCIStmt *stm) {
    sword r = OCIStmtFetch2(
        stm, g_connection.err, 1, OCI_FETCH_NEXT,
        0, OCI_DEFAULT);
    if (r == OCI_NO_DATA)
        return r;
    ora_check(r);
    return r;
}

sword ora_stmt_free(OCIStmt *stm) {
    sword r = OCIHandleFree(stm, OCI_HTYPE_STMT);
    ora_check(r);
    return r;
}

sword ora_lob_alloc(OCILobLocator **lob) {
    sword r = OCIDescriptorAlloc(g_connection.env, (void **) lob, OCI_DTYPE_LOB, 0, 0);
    ora_check(r);
    return r;
}

sword ora_lob_free(OCILobLocator *lob)  {
    sword r = OCIDescriptorFree(lob, OCI_DTYPE_LOB);
    ora_check(r);
    return r;
}

