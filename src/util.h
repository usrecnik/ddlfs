#include <time.h>

/**
 * Convert string (formatted as 'yyyy-mm-dd hh24:mi:ss') to time_t
 * */
time_t utl_str2time(char *time);

/**
 * Convert filesystem type (e.g. "PACKAGE_SPEC") to Oracle type (e.g. "PACKAGE")
 * *fstype must not be allocated on stack (use malloc() or anything similar)
 * */
int utl_fs2oratype(char **fstype);

/**
 * Convert Oracle type (e.g. "PACKAGE") to filesystem type (e.g. "PACKAGE_SPEC")
 * */
int utl_ora2fstype(char **oratype);
