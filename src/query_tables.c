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

static int tab_all_tables(const char *schema, const char *table, struct tabledef *def) {
    const char *query = 
"select \"TEMPORARY\" from all_tables\
 where owner=:bind_owner and table_name=:bind_name";
    
    int retval = EXIT_SUCCESS;

    ORA_STMT_PREPARE(tab_all_tables);
    ORA_STMT_DEFINE_STR_I(1, 2, temporary, tab_all_tables);
    ORA_STMT_BIND_STR(1, schema, tab_all_tables);
    ORA_STMT_BIND_STR(2, table,  tab_all_tables);
    ORA_STMT_EXECUTE(tab_all_tables, 1);    

    def->temporary = (strcmp(ORA_NVL(temporary, "N"), "Y") == 0) ? 'Y' : 'N';
    
tab_all_tables_cleanup:
    ORA_STMT_FREE;
    return retval;
} 

static int tab_all_tab_columns(const char *schema, const char *table, struct tabledef *def) {
    const char *query = 
"select column_name, data_type, data_length, data_precision, data_scale, nullable,\
 default_length, data_default, char_length, char_used\
 from all_tab_columns where owner=:bind_owner and table_name=:bind_name\
 order by column_id";

    int retval = EXIT_SUCCESS;
        
    ORA_STMT_PREPARE(tab_all_tab_columns);
    ORA_STMT_DEFINE_STR  (1, 129,  column_name,    tab_all_tab_columns);
    ORA_STMT_DEFINE_STR_I(2, 129,  data_type,      tab_all_tab_columns);
    ORA_STMT_DEFINE_INT  (3,       data_length,    tab_all_tab_columns);
    ORA_STMT_DEFINE_INT_I(4,       data_precision, tab_all_tab_columns);
    ORA_STMT_DEFINE_INT_I(5,       data_scale,     tab_all_tab_columns);
    ORA_STMT_DEFINE_STR_I(6, 2,    nullable,       tab_all_tab_columns);
    ORA_STMT_DEFINE_INT_I(7,       default_length, tab_all_tab_columns);
    ORA_STMT_DEFINE_STR_I(8, 4000, data_default,   tab_all_tab_columns);
    ORA_STMT_DEFINE_INT_I(9,       char_length,    tab_all_tab_columns);
    ORA_STMT_DEFINE_STR_I(10, 2,   char_used,      tab_all_tab_columns);
    
    ORA_STMT_BIND_STR(1, schema, tab_all_tab_columns);
    ORA_STMT_BIND_STR(2, table,  tab_all_tab_columns);
    
    ORA_STMT_EXECUTE(tab_all_tab_columns, 0);

    char definition[4096];                // buffer (on stack) in which to build column representation
    char scale[100]; 
    struct deflist *col_start = NULL;     // always points to first column if there is at least one column present
    struct deflist *col_temp = NULL;      // temporary, points to any columns
    struct deflist *col_curr = NULL;      // for loops, used as pointer to current element

    while (ORA_STMT_FETCH) {
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
        if (strstr(ORA_NVL(data_type, "X"), "CHAR") != NULL) {
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
    ORA_STMT_FREE; 
    return retval;
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

    int retval = EXIT_SUCCESS;
     
    ORA_STMT_PREPARE(tab_all_constraints);
    ORA_STMT_DEFINE_STR  (1, 129,  constraint_name,   tab_all_constraints);
    ORA_STMT_DEFINE_STR_I(2, 2,    constraint_type,   tab_all_constraints);
    ORA_STMT_DEFINE_STR_I(3, 129,  index_owner,       tab_all_constraints);
    ORA_STMT_DEFINE_STR_I(4, 129,  index_name,        tab_all_constraints);
    ORA_STMT_DEFINE_STR_I(5, 129,  ref_owner,         tab_all_constraints);
    ORA_STMT_DEFINE_STR_I(6, 129,  ref_table,         tab_all_constraints);
    ORA_STMT_DEFINE_STR_I(7, 4000, colstr,            tab_all_constraints);
    ORA_STMT_DEFINE_STR_I(8, 4000, ref_colstr,        tab_all_constraints);
    ORA_STMT_DEFINE_STR_I(9, 32767, search_condition, tab_all_constraints);
    ORA_STMT_BIND_STR(1, schema, tab_all_constraints);
    ORA_STMT_BIND_STR(2, table,  tab_all_constraints);
    ORA_STMT_BIND_STR(3, schema, tab_all_constraints);
    ORA_STMT_BIND_STR(4, table,  tab_all_constraints); 
    ORA_STMT_EXECUTE(tab_all_constraints, 0);
    
    struct deflist *tmpdef;    
    char tmpstr[8192];
    char tmpstr_part[500];
    while (ora_stmt_fetch(o_stm) == OCI_SUCCESS) {
        /*
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
        */

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
    ORA_STMT_FREE;
    return retval; 
}

int tab_all_indexes(const char *schema, const char *table, struct tabledef *def) {

// index_type='BITMAP', 'NORMAL'
    const char *query = 
"select ai.owner, ai.index_name, ai.index_type, ai.uniqueness, ai.compression, ai.prefix_length,\
 ic.column_name, ic.column_position, ic.descend as column_descend,\
 ie.column_expression as expression_string\
 from all_indexes ai\
 left join all_ind_columns ic on ic.index_owner=ai.owner and ic.index_name = ai.index_name\
 left join all_ind_expressions ie on ie.index_owner = ai.owner\
 and ie.index_name = ai.index_name and ie.column_position=ic.column_position\
 where ai.table_owner=:bind_owner and ai.table_name=:bind_name\
 order by ai.owner, ai.index_name, ic.column_position";
    
    int retval = EXIT_SUCCESS;
     
    ORA_STMT_PREPARE(tab_all_indexes);
    
    ORA_STMT_DEFINE_STR  (1, 129, index_owner,     tab_all_indexes);
    ORA_STMT_DEFINE_STR  (2, 129, index_name,      tab_all_indexes);
    ORA_STMT_DEFINE_STR_I(3, 30,  index_type,      tab_all_indexes);
    ORA_STMT_DEFINE_STR_I(4, 15,  index_unique,    tab_all_indexes);
    ORA_STMT_DEFINE_STR_I(5, 15,  index_compress,  tab_all_indexes);
    ORA_STMT_DEFINE_INT_I(6,      index_prefix,    tab_all_indexes);
    ORA_STMT_DEFINE_STR_I(7, 129, column_name,     tab_all_indexes);
    ORA_STMT_DEFINE_INT  (8,      column_position, tab_all_indexes);
    ORA_STMT_DEFINE_STR_I(9, 15,  column_descend,  tab_all_indexes);
    
    ORA_STMT_BIND_STR(1, schema, tab_all_indexes); 
    ORA_STMT_BIND_STR(2, table,  tab_all_indexes);
    
    ORA_STMT_EXECUTE(tab_all_indexes, 0);
    
    while (ORA_STMT_FETCH) {
        logmsg(LOG_DEBUG, "index {");
        logmsg(LOG_DEBUG, ".. index_owner=[%s]",     ORA_VAL(index_owner));
        logmsg(LOG_DEBUG, ".. index_name=[%s]",      ORA_VAL(index_name));
        logmsg(LOG_DEBUG, ".. index_type=[%s]",      ORA_NVL(index_type, "N/A"));
        logmsg(LOG_DEBUG, ".. index_unique=[%s]",    ORA_NVL(index_unique, "N/A"));
        logmsg(LOG_DEBUG, ".. index_compress=[%s]",  ORA_NVL(index_compress, "N/A"));
        logmsg(LOG_DEBUG, ".. index_prefix=[%d]",    ORA_NVL(index_prefix, -1));
        logmsg(LOG_DEBUG, ".. column_name=[%s]",     ORA_NVL(column_name, "N/A"));
        logmsg(LOG_DEBUG, ".. column_position=[%d]", ORA_VAL(column_position));
        logmsg(LOG_DEBUG, ".. column_descend=[%s]",  ORA_NVL(column_descend, "N/A"));
        logmsg(LOG_DEBUG, "}");
    }
     
tab_all_indexes_cleanup:    
    ORA_STMT_FREE;
    return retval;
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

    if (tab_all_indexes(schema, table, def) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_object_all_tables(): tab_all_indexes failed.");
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


