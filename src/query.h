#pragma once

#include "vfs.h"

/**
 * String utility and query functions.
 * All functions returing an int return EXIT_SUCCESS on success and EXIT_FAILURE on failure.
 * */

/**
 * Determine correct suffix (such as .sql or .java) for specified objectType.
 * */
int str_suffix(char **dst, const char *objectType);

/**
 * Convert filename to correct case and optionally remove suffix (.sql)
 * */
int str_fn2obj(char **dst, char *src, const char *expectedSuffix);

/**
 * Populate g_vfs with list of schemas. Those are first-level folder entries.
 * */
void qry_schemas();

/**
 * Populate g_vfs with list of object types.
 * */
int qry_types(t_fsentry *schema);

/**
 * Populate g_vfs with list of actual objects (.sql files).
 * */
void qry_objects(t_fsentry *schema, t_fsentry *type);

/**
 * Write DDL to of object specified by schema,type,object parameters to file name fname.
 * Parameters are those available in vfs (actual filesystem path parts), not yet converted to
 * actual database object names (str_fn2obj).
 * fname is determined by this function.
 * */
int qry_object(const char *schema,
               const char *type,
               const char *object,
               char **fname);

/**
 * Assemble temporary filename based on schema, type and object.
 * */
int qry_object_fname(const char *schema,
					 const char *type,
					 const char *object,
					 char **fname);

/**
 * Execute DDL statement.
 * */
int qry_exec_ddl(char *schema, char *object, char *ddl);
