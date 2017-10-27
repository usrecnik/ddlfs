#include <oci.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "oracle.h"

sword ora_check(sword status) {
    text errbuf[512];
    sb4 errcode;

    switch (status) {
        case OCI_SUCCESS:
            break;
  
        case OCI_SUCCESS_WITH_INFO:
            logmsg(LOG_ERROR, "OCI_SUCCESS_WITH_INFO");
            break;
  
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

int ora_connect(char* username, char* password, char* database) {
    sword r;
    
    logmsg(LOG_INFO, "Connecting to Oracle Database (%s).", database);
    r = OCIEnvCreate(&g_connection.env, OCI_DEFAULT, 0, 0, 0, 0, 0, 0);
    if (ora_check(r) != OCI_SUCCESS)
        return EXIT_FAILURE;

    logmsg(LOG_DEBUG, ".. allocating handles.");
    OCIHandleAlloc(g_connection.env, (dvoid**)&g_connection.err, OCI_HTYPE_ERROR,   0, 0);
    OCIHandleAlloc(g_connection.env, (dvoid**)&g_connection.srv, OCI_HTYPE_SERVER,  0, 0);
    OCIHandleAlloc(g_connection.env, (dvoid**)&g_connection.svc, OCI_HTYPE_SVCCTX,  0, 0);
    OCIHandleAlloc(g_connection.env, (dvoid**)&g_connection.ses, OCI_HTYPE_SESSION, 0, 0);
        
    logmsg(LOG_DEBUG, ".. attaching to server process.");
    r = OCIServerAttach(
        g_connection.srv, 
        g_connection.err, 
        (text*) database, 
        strlen(database), 
        (ub4) OCI_DEFAULT);

    if (ora_check(r))
        return EXIT_FAILURE;
    
    logmsg(LOG_DEBUG, ".. setting session attributes (user=%s).", username);
    OCIAttrSet(g_connection.svc, OCI_HTYPE_SVCCTX, g_connection.srv, 0, OCI_ATTR_SERVER, g_connection.err);
    OCIAttrSet(g_connection.ses, OCI_HTYPE_SESSION, username, strlen(username), OCI_ATTR_USERNAME, g_connection.err); 
    OCIAttrSet(g_connection.ses, OCI_HTYPE_SESSION, password, strlen(password), OCI_ATTR_PASSWORD, g_connection.err);

    logmsg(LOG_DEBUG, ".. starting database session.");
    r = OCISessionBegin (
        g_connection.svc, 
        g_connection.err, 
        g_connection.ses,
        OCI_CRED_RDBMS, OCI_DEFAULT);
    if (ora_check(r))
        return EXIT_FAILURE;

    logmsg(LOG_DEBUG, ".. registering database session.");
    OCIAttrSet(g_connection.svc, OCI_HTYPE_SVCCTX, g_connection.ses, 0, OCI_ATTR_SESSION, g_connection.err);

    logmsg(LOG_INFO, ".. connected.");
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

    r = OCIHandleAlloc(g_connection.env, (void **) stm, OCI_HTYPE_STMT, 0, 0);
    if (ora_check(r))
        return r;

    r = OCIStmtPrepare(*stm, g_connection.err, (text*) query, strlen(query), OCI_NTV_SYNTAX, OCI_DEFAULT);
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

