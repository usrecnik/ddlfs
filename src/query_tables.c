#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string.h>

#include "query_tables.h"
#include "config.h"
#include "logging.h"
#include "oracle.h"
#include "vfs.h"
#include "tempfs.h"

static int tab_all_tables(const char *schema, const char *table, struct tabledef *def) {
    char *query = "select \"TEMPORARY\" from \"ALL_TABLES\" where \"OWNER\"=:bind_owner and \"TABLE_NAME\"=:bind_name";
    int retval = EXIT_SUCCESS;
    
    OCIStmt   *o_stm = NULL;
    OCIDefine *o_def = NULL;
    char      *o_sl1 = NULL;
    OCIBind   *o_bn1 = NULL;
    OCIBind   *o_bn2 = NULL;

    if ((o_sl1 = malloc(2*sizeof(char))) == NULL) {
        logmsg(LOG_ERROR, "tab_all_tables(): Unable to allocate memory for o_sl1");
        return EXIT_FAILURE;
    }

    if (ora_stmt_prepare(&o_stm, query)) {
        logmsg(LOG_ERROR, "tab_all_tables(): Unable to prepare statement");
        retval = EXIT_FAILURE;
        goto tab_all_tables_cleanup;    
    }

    if (ora_stmt_define(o_stm, &o_def, 1, o_sl1, 2*sizeof(char), SQLT_STR)) {
        logmsg(LOG_ERROR, "tab_all_tables(): Unable to define statement");
        retval = EXIT_FAILURE;
        goto tab_all_tables_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bn1, 1, (void*) schema, strlen(schema)+1, SQLT_STR)) {
        logmsg(LOG_ERROR, "tab_all_tables(): Unable to bind owner.");
        retval = EXIT_FAILURE;
        goto tab_all_tables_cleanup;
    }
   
    if (ora_stmt_bind(o_stm, &o_bn2, 2, (void*) table, strlen(table)+1, SQLT_STR)) {
        logmsg(LOG_ERROR, "tab_all_tables(): Unable to bind table.");
        retval = EXIT_FAILURE;
        goto tab_all_tables_cleanup;
    }

    if (ora_stmt_execute(o_stm, 1)) {
        logmsg(LOG_ERROR, "tab_all_tables(): Unable to execute query.");
        retval = EXIT_FAILURE;
        goto tab_all_tables_cleanup;
    }

    logmsg(LOG_DEBUG, "TEMPORARY=[%s]", o_sl1); 
    if (strcmp(o_sl1, "Y") == 0)
        def->temporary = 'Y';
    else
        def->temporary = 'N';
        
    
tab_all_tables_cleanup:
    if (o_sl1 != NULL)
        free(o_sl1);

    if (o_stm != NULL)
        ora_stmt_free(o_stm);
    
    return retval;
} 

static int tab_all_tab_columns(const char *schema, const char *table, struct tabledef *def) {
    char *query = 
"select \"COLUMN_NAME\", \"DATA_TYPE\", \"DATA_LENGTH\", \"DATA_PRECISION\", \"DATA_SCALE\", \"NULLABLE\", " \
       "\"DEFAULT_LENGTH\", \"DATA_DEFAULT\", \"CHAR_LENGTH\", \"CHAR_USED\" " \
    "from \"ALL_TAB_COLUMNS\" where \"OWNER\"=:bind_owner and \"TABLE_NAME\"=:bind_name " \
    "order by \"COLUMN_ID\"";

    int retval = EXIT_SUCCESS;

    char definition[4096];                  // buffer (on stack) in which to build column representation
    struct deflist *col_start = NULL;     // always points to first column if there is at least one column present
    struct deflist *col_temp = NULL;      // temporary, points to any columns
    struct deflist *col_curr = NULL;      // for loops, used as pointer to current element
    
    OCIStmt   *o_stm = NULL;
    OCIDefine *o_def = NULL;
    OCIBind   *o_bn1 = NULL;
    OCIBind   *o_bn2 = NULL;
    
    char    *o_column_name;         // VARCHAR2(128)    NOT NULL
    char    *o_data_type;           // VARCHAR2(128)
    int      o_data_length;         // NUMBER           NOT NULL
    int      o_data_precision;      // NUMBER
    int      o_data_scale;          // NUMBER
    char    *o_nullable;            // VARCHAR2(1) 
    int      o_default_length;      // NUMBER
    char    *o_data_default;        // LONG
    int      o_char_length;         // NUMBER
    char    *o_char_used;           // VARCHAR2(1)

    // sb2 i_column_name = 0; (not null)
    sb2 i_data_type = 0;
    // sb2 i_data_length = 0; (not null)
    sb2 i_data_precision = 0;
    sb2 i_data_scale = 0;
    sb2 i_nullable = 0;
    sb2 i_default_length = 0;
    sb2 i_data_default = 0;
    sb2 i_char_length = 0;
    sb2 i_char_used = 0;

    
    // allocate
    if ((o_column_name = malloc(129*sizeof(char))) == NULL) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to allocate memory for o_column_name");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if ((o_data_type = malloc(129*sizeof(char))) == NULL) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to allocate memory for o_data_type");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if ((o_nullable = malloc(2*sizeof(char))) == NULL) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to allocate memory for o_nullable");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if ((o_data_default = malloc(32*1024*sizeof(char))) == NULL) { // max 32k, we ignore the rest.
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to allocate memory for o_data_default");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if ((o_char_used = malloc(2*sizeof(char))) == NULL) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to allocate memory for o_char_used");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    // prepare
    if (ora_stmt_prepare(&o_stm, query)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to prepare statement");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;    
    }

    // define
    if (ora_stmt_define(o_stm, &o_def, 1, o_column_name, 129*sizeof(char), SQLT_STR)) { // NOT NULL
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define column_name");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 2, o_data_type, 129*sizeof(char), SQLT_STR, (dvoid*) &i_data_type)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define data_type");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if (ora_stmt_define(o_stm, &o_def, 3, &o_data_length, sizeof(int), SQLT_INT)) { // NOT NULL
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define data_length");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 4, &o_data_precision, sizeof(int), SQLT_INT, (dvoid*) &i_data_precision)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define data_precision");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 5, &o_data_scale, sizeof(int), SQLT_INT, (dvoid*) &i_data_scale)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define data_scale");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 6, o_nullable, 2*sizeof(char), SQLT_STR, (dvoid*) &i_nullable)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define nullable");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 7, &o_default_length, sizeof(int), SQLT_INT, (dvoid*) &i_default_length)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define default_length");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 8, o_data_default, 32*1024*sizeof(char), SQLT_STR, (dvoid*) &i_data_default)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define data_default");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 9, &o_char_length, sizeof(int), SQLT_INT, (dvoid*) &i_char_length)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define char_length");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 10, o_char_used, 2*sizeof(char), SQLT_STR, (dvoid*) &i_char_used)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to define char_used");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }
    
    // bind
    if (ora_stmt_bind(o_stm, &o_bn1, 1, (void*) schema, strlen(schema)+1, SQLT_STR)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to bind owner.");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }
   
    if (ora_stmt_bind(o_stm, &o_bn2, 2, (void*) table, strlen(table)+1, SQLT_STR)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to bind table.");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }
    
    // execute
    if (ora_stmt_execute(o_stm, 0)) {
        logmsg(LOG_ERROR, "tab_all_tab_columns(): Unable to execute query.");
        retval = EXIT_FAILURE;
        goto tab_all_tab_columns_cleanup;
    }
    
    // loop resultset
    char scale[100];
    while (ora_stmt_fetch(o_stm) == OCI_SUCCESS) {
        /*
        logmsg(LOG_DEBUG, "\n");
        logmsg(LOG_DEBUG, "column_name=[%s]", o_column_name);
        logmsg(LOG_DEBUG, ".. data_type=[%s]", (i_data_type == 0 ? o_data_type : "NULL"));
        logmsg(LOG_DEBUG, ".. data_length=[%d]", o_data_length);
        logmsg(LOG_DEBUG, ".. data_precision=[%d]", (i_data_precision == 0 ? o_data_precision : -1));
        logmsg(LOG_DEBUG, ".. data_scale=[%d]", (i_data_scale == 0 ? o_data_scale : -1));
        logmsg(LOG_DEBUG, ".. nullable=[%s]", (i_nullable == 0 ? o_nullable : "NULL"));
        logmsg(LOG_DEBUG, ".. default_length=[%d]", (i_default_length == 0 ? o_default_length : -1));
        logmsg(LOG_DEBUG, ".. data_default=[%s]", (i_data_default == 0 ? o_data_default : "NULL"));
        logmsg(LOG_DEBUG, ".. char_length=[%d]", (i_char_length == 0 ? o_char_length : -1));
        logmsg(LOG_DEBUG, ".. char_used=[%s]", (i_char_used == 0 ? o_char_used : "NULL"));
        */
        
        // data type
        if (strstr(o_data_type, "CHAR") != NULL) {
            if (i_char_used != 0 && i_char_length != 0 && o_char_used[0] == 'C')
                snprintf(scale, 99, "(%d CHAR)", o_char_length);
            else
                snprintf(scale, 99, "(%d BYTE)", o_data_length);
        } else {
            if (i_data_precision != 0 && i_data_scale != 0)
                scale[0] = '\0';
            else if (i_data_precision == 0 && i_data_scale != 0) 
                snprintf(scale, 99, "(%d)", o_data_precision);
            else if (i_data_precision == 0 && i_data_scale == 0)
                snprintf(scale, 99, "(%d,%d)", o_data_precision, o_data_scale);
            else
                scale[0] = '\0'; // this probably should never happen               
        }

        // default
        snprintf(definition, 4090, "\t\"%s\" %s%s%s%s %s",
            o_column_name, o_data_type, scale,
                (i_data_default != 0 ? "" : " DEFAULT "),
                (i_data_default != 0 ? "" : o_data_default),
                (i_nullable != 0 ? "X" : (o_nullable[0] == 'Y' ? "NULL" : "NOT NULL")));

        // allocate and populate new column
        col_temp = malloc(sizeof(struct deflist));
        if (col_temp == NULL) {
            logmsg(LOG_ERROR, "tab_all_tab_columns() - Unable to allocate memory for column definition.");
            goto tab_all_tab_columns_cleanup;
        }
        
        col_temp->definition = calloc(strlen(definition)+1, sizeof(char));
        col_temp->next = NULL;
        
        strcpy(col_temp->definition, definition);
        
        // append created column to linked list
        if (col_start == NULL)
            col_start = col_temp;
        else {
            col_curr = col_start;
            while (col_curr->next != NULL)
                col_curr = col_curr->next;
            col_curr->next = col_temp;            
        } 
    }
    def->columns = col_start;
 
tab_all_tab_columns_cleanup:
    
    if (o_column_name != NULL)
        free(o_column_name);

    if (o_data_type != NULL)
        free(o_data_type);

    if (o_nullable != NULL)
        free(o_nullable);

    if (o_data_default != NULL)
        free(o_data_default);

    if (o_char_used != NULL)
        free(o_char_used); 

    if (o_stm != NULL)
        ora_stmt_free(o_stm);
    
    return retval;
}

static void deflist_append(struct deflist **first, struct deflist *fresh) {
    fresh->next = NULL;
    if (*first == NULL) {
        *first = fresh;
        return;
    }
    
    struct deflist *temp = *first;
    while (temp->next != NULL)
        temp = temp->next;
    temp->next = fresh;
}

static int tab_all_constraints(const char *schema, const char *table, struct tabledef *def) {
    const char *query = 
"select * from (\
 select ac.constraint_name, ac.constraint_type, ac.index_owner, ac.index_name, ac.r_owner, rc.table_name as r_table_name,\
 listagg('\"' || replace(cc.column_name, '\"', '\"\"') || '\"', ', ') within group (order by cc.position) as colstr,\
 listagg('\"' || replace(rc.column_name, '\"', '\"\"') || '\"', ', ') within group (order by cc.position) as r_colstr,\
 null as search_condition\
 from all_constraints ac\
 join all_cons_columns cc on cc.owner=ac.owner and cc.table_name=ac.table_name and cc.constraint_name=ac.constraint_name\
 left join all_cons_columns rc on rc.owner=ac.r_owner and rc.constraint_name=ac.r_constraint_name\
 where ac.owner=:bind_owner and ac.table_name=:bind_name and ac.generated='USER NAME' and ac.constraint_type IN ('P', 'U', 'R')\
 group by ac.constraint_name, ac.constraint_type, ac.index_owner, ac.index_name, ac.r_owner, rc.table_name\
 union all\
 select constraint_name, constraint_type, null, null, null, null,\
 null, null, search_condition\
 from all_constraints\
 where owner=:bind_owner and table_name=:bind_name and generated='USER NAME' and constraint_type='C')\
 order by decode(constraint_type, 'P', 1, 'U', 2, 'R', 3, 'C', 4, 5), constraint_name";

    puts(query);

    int retval = EXIT_SUCCESS;
    
    OCIStmt   *o_stm = NULL;
    OCIDefine *o_def = NULL;
    OCIBind   *o_bn1 = NULL;
    OCIBind   *o_bn2 = NULL;
    OCIBind   *o_bn3 = NULL;
    OCIBind   *o_bn4 = NULL;

    char o_constraint_name[129] = "\0";     // VARCHAR2(128) NOT NULL 
    char o_constraint_type[2] = "\0";       // VARCHAR2(1)
    char o_index_owner[129] = "\0";         // VARCHAR2(128)
    char o_index_name[129] = "\0";          // VARCHAR2(128)
    char o_ref_owner[129] = "\0";           // VARCHAR2(128)
    char o_ref_table[129] = "\0";           // VARCHAR2(128)
    char o_colstr[4000] = "\0";             // VARCHAR2(4000)
    char o_ref_colstr[4000] = "\0";         // VARCHAR2(4000)
    char o_search_condition[32767] = "\0";  // LONG

    sb2 i_constraint_type = 0;
    sb2 i_index_owner = 0;
    sb2 i_index_name = 0;
    sb2 i_ref_owner = 0;
    sb2 i_ref_table = 0;
    sb2 i_colstr = 0;
    sb2 i_ref_colstr = 0;
    sb2 i_search_condition = 0;
    
    struct deflist *tmpdef;
     
    // prepare
    if (ora_stmt_prepare(&o_stm, query)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to prepare statement");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    // define
    if (ora_stmt_define(o_stm, &o_def, 1, o_constraint_name, 129*sizeof(char), SQLT_STR)) { // NOT NULL
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to define constraint_name");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 2, o_constraint_type, 2*sizeof(char), SQLT_STR, (dvoid*) &i_constraint_type)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to define constraint_type");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 3, o_index_owner, 129*sizeof(char), SQLT_STR, (dvoid*) &i_index_owner)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to define index_owner");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 4, o_index_name, 129*sizeof(char), SQLT_STR, (dvoid*) &i_index_name)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to define index_name");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 5, o_ref_owner, 129*sizeof(char), SQLT_STR, (dvoid*) &i_ref_owner)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to define ref_owner");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 6, o_ref_table, 129*sizeof(char), SQLT_STR, (dvoid*) &i_ref_table)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to define ref_table");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 7, o_colstr, 4000*sizeof(char), SQLT_STR, (dvoid*) &i_colstr)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to define colstr");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 8, o_ref_colstr, 4000*sizeof(char), SQLT_STR, (dvoid*) &i_ref_colstr)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to define ref_colstr");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_define_i(o_stm, &o_def, 9, o_search_condition, 32767*sizeof(char), SQLT_STR, (dvoid*) &i_search_condition)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to define search_condition");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    
    // bind
    if (ora_stmt_bind(o_stm, &o_bn1, 1, (void*) schema, strlen(schema)+1, SQLT_STR)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to bind schema (1)");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bn2, 2, (void*) table, strlen(table)+1, SQLT_STR)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to bind table (1)");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bn3, 3, (void*) schema, strlen(schema)+1, SQLT_STR)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to bind schema (2)");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }

    if (ora_stmt_bind(o_stm, &o_bn4, 4, (void*) table, strlen(table)+1, SQLT_STR)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to bind table (2)");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }
    
     // execute
    if (ora_stmt_execute(o_stm, 0)) {
        logmsg(LOG_ERROR, "tab_all_constraints(): Unable to execute query.");
        retval = EXIT_FAILURE;
        goto tab_all_constraints_cleanup;
    }
    
    // loop resultset
    char tmpstr[8192];
    char tmpstr_part[500];
    while (ora_stmt_fetch(o_stm) == OCI_SUCCESS) {

        logmsg(LOG_DEBUG, "constraint {");
        logmsg(LOG_DEBUG, ".. constraint_name=[%s]", o_constraint_name);
        logmsg(LOG_DEBUG, ".. constraint_type=[%s]", (i_constraint_type == 0 ? o_constraint_type : "NULL"));
        logmsg(LOG_DEBUG, ".. index_name=[%s]", (i_index_name == 0 ? o_index_name : "NULL"));
        logmsg(LOG_DEBUG, ".. index_owner=[%s]", (i_index_owner == 0 ? o_index_owner : "NULL"));
        logmsg(LOG_DEBUG, ".. ref_owner=[%s]", (i_ref_owner == 0 ? o_ref_owner : "NULL"));
        logmsg(LOG_DEBUG, ".. ref_table=[%s]", (i_ref_table == 0 ? o_ref_table : "NULL"));
        logmsg(LOG_DEBUG, ".. colstr=[%s]", (i_colstr == 0 ? o_colstr : "NULL"));
        logmsg(LOG_DEBUG, ".. ref_colstr=[%s]", (i_ref_colstr == 0 ? o_ref_colstr : "NULL"));
        logmsg(LOG_DEBUG, ".. search_condition=[%s]", (i_search_condition == 0 ? o_search_condition : "NULL"));
        logmsg(LOG_DEBUG, "}");

        switch((i_constraint_type == 0 ? o_constraint_type[0] : 'x')) {
            case 'P': strcpy(tmpstr_part, "PRIMARY KEY"); break;
            case 'U': strcpy(tmpstr_part, "UNIQUE");      break;
            case 'R': strcpy(tmpstr_part, "FOREIGN KEY"); break;
            case 'C': strcpy(tmpstr_part, "CHECK");       break;
            default : strcpy(tmpstr_part, "UNKNOWN");     break;
        }
        
        snprintf(tmpstr, 8192, "ALTER TABLE \"%s\".\"%s\" ADD CONSTRAINT \"%s\" %s",
            schema, table, o_constraint_name, tmpstr_part
        );
        
        if (i_index_owner == 0 && i_index_name == 0)
            snprintf(tmpstr_part, 500, " USING INDEX \"%s\".\"%s\"", o_index_owner, o_index_name);
        else if (i_index_owner !=0 && i_index_name == 0) 
            snprintf(tmpstr_part, 500, " USING INDEX \"%s\"", o_index_name);
        else
            tmpstr_part[0] = '\0';

        strcat(tmpstr, tmpstr_part);
        tmpstr_part[0] = '\0';

        switch (i_constraint_type == 0 ? o_constraint_type[0] : 'x') {
            case 'R':
                snprintf(tmpstr_part, 500, " (%s) REFERENCES \"%s\".\"%s\"(%s)",
                    (i_colstr == 0 ? o_colstr : "???"),
                    (i_ref_owner == 0 ? o_ref_owner : "???"),
                    (i_ref_table == 0 ? o_ref_table : "???"),
                    (i_ref_colstr == 0 ? o_ref_colstr : "???"));
                break;
            
            case 'C':
                snprintf(tmpstr_part, 500, " (%s)", (i_search_condition == 0 ? o_search_condition : "???"));
                break;
        }
        strcat(tmpstr, tmpstr_part);
        strcat(tmpstr, ";\n");
         
        // append to linked list        
        tmpdef = malloc(sizeof(struct deflist));
        if (tmpdef == NULL) {
            logmsg(LOG_ERROR, "tab_all_constraints(): Unable to malloc tmpdef");
            retval = EXIT_FAILURE;
            goto tab_all_constraints_cleanup;
        }

        tmpdef->definition = malloc(strlen(tmpstr));
        if (tmpdef->definition == NULL) {
            logmsg(LOG_ERROR, "tab_all_constraitns(): Unable to malloc tmpdef->definition");
            retval = EXIT_FAILURE;
            goto tab_all_constraints_cleanup;
        }
        strcpy(tmpdef->definition, tmpstr);
        
        deflist_append(&def->constraints, tmpdef);
    }
    
   
tab_all_constraints_cleanup:
    
    if (o_stm != NULL)
        ora_stmt_free(o_stm);  
 
    return retval; 
}

static void deflist_free(struct deflist *curr) {
    if (curr == NULL)
        return;
    
    while (curr != NULL) {
        free(curr->definition);
        struct deflist *temp = curr;
        curr = curr->next;
        free(temp);
    }
}

int qry_object_all_tables(const char *schema,
                          const char *table,
                          const char *fname,
                          time_t *last_ddl_time) {
    
    int retval = EXIT_SUCCESS;
    FILE *fp;
    char temp[4096];
    struct tabledef *def = malloc(sizeof(struct tabledef));
    struct deflist *col;
    int colcnt = 0;

    if (def == NULL) {
        logmsg(LOG_ERROR, "qry_object_all_tables(): unable to allocate memory for tabledef.");
        return EXIT_FAILURE;
    }
     
    if (tab_all_tables(schema, table, def) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object_all_tables(): tab_all_tables() failed.");
        retval = EXIT_FAILURE;
        goto qry_object_all_tables_cleanup;
    }
    
    if (tab_all_tab_columns(schema, table, def) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object_all_tables(): tab_all_tab_columns() failed.");
        retval = EXIT_FAILURE;
        goto qry_object_all_tables_cleanup;
    }

    if (tab_all_constraints(schema, table, def) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object_all_tables(): tab_all_constraints() failed.");
        retval = EXIT_FAILURE;
        goto qry_object_all_tables_cleanup;
    }
    
    fp = fopen(fname, "w");
    if (fp == NULL) {
        logmsg(LOG_ERROR, "qry_object_all_tables(): Unable to open [%s]: %d %n", fname, errno, strerror(errno));
        retval = EXIT_FAILURE;
        goto qry_object_all_tables_cleanup;
    }

    snprintf(temp, 4095, "CREATE%s TABLE \"%s\".\"%s\" (\n",
        (def->temporary == 'Y' ? " GLOBAL TEMPORARY" : ""),
        schema, table);
    fwrite(temp, 1, strlen(temp), fp);

    col = def->columns;
    while (col != NULL) {
        if (colcnt++ > 0)
            fwrite(",\n", 1, strlen(",\n"), fp);
        fwrite(col->definition, 1, strlen(col->definition), fp);
        col = col->next;
    }
    fwrite(");\n\n", 1, strlen(");\n\n"), fp);
    
    col = def->constraints;
    while(col != NULL) {
        fwrite(col->definition, 1, strlen(col->definition), fp);
        col = col->next;   
    }
    
    if (fclose(fp) != 0) {
        logmsg(LOG_ERROR, "qry_object_all_tables(): Unable to close [%s]: %d %n", fname, errno, strerror(errno));
        retval = EXIT_FAILURE;
        goto qry_object_all_tables_cleanup;
    }
     
    // @todo: indexes
    // @todo: comments
    
qry_object_all_tables_cleanup:
    if (def != NULL) {
        deflist_free(def->columns);
        deflist_free(def->constraints);
        free(def);
    }
     
    return retval;
}


