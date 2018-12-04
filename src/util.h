#pragma once
#include <sys/types.h>
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

#ifdef _MSC_VER
/**
 * Implementation of strcasecmp() as found in strings.h. It's here becasue
 * this function is not part of the standard and so MSC doesn't have it.
 */
int strcasecmp(const char *s1, const char *s2);

// these two functions are not available on Windows, so they're created as
// wrappers for ReadFile and WriteFile
int pread(int fd, void *buf, size_t count, off_t offset);
int pwrite(int fd, const void *buf, size_t count, off_t offset);

#endif