#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE

#ifdef __linux__ 
	__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
#endif

#include <fuse.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <ctype.h>
#ifndef _MSC_VER
	#include <unistd.h>
#else
	#pragma warning(disable:4996)
	#pragma warning(disable:4100) //  unreferenced formal parameter (because function signature is from libfuse, we shouldn't change it)
	#define getuid() 0 // @todo - i'm not really sure what to give here for windows...
	#define getgid() 0 // @todo - i'm not really sure what to give here for windows since struct stat st_uid is of type "short"
	#include <io.h>
	#define F_OK    0
    #define O_ACCMODE   (_O_RDONLY|_O_WRONLY|_O_RDWR)
#endif

#include "vfs.h"
#include "logging.h"
#include "query.h"
#include "config.h"
#include "tempfs.h"
#include "fuse-impl.h"
#include "util.h"

#define DEPTH_SCHEMA 0
#define DEPTH_TYPE   1
#define DEPTH_OBJECT 2
#define DEPTH_MAX    3

static void fs_path_free(char **part) {
    for (int i = 0; i < DEPTH_MAX; i++)
        if (part[i] != NULL)
            free(part[i]);
    free(part);
}

static int fs_path_create (char ***part, const char *path) {
    char **r;
    r = malloc(DEPTH_MAX * sizeof(char*));

    for (int i = 0; i < DEPTH_MAX; i++)
        r[i] = NULL;

    char *p = strdup(path);
    char *t = strtok(p, "/");
    int i = 0;
    while(t) {
        r[i] = strdup(t);
        t = strtok(NULL, "/");
        i++;
    }
    free(p);

    *part = r;

    return i;
}

static void qry_any(int depth, t_fsentry *schema, t_fsentry *type) {
    switch (depth) {
        case DEPTH_SCHEMA:
            qry_schemas();
            break;

        case DEPTH_TYPE:
            qry_types(schema);
            break;

        case DEPTH_OBJECT:
            qry_objects(schema, type);
            break;

        default:
            logmsg(LOG_ERROR, "Invalid depth [%d]!", depth);
            break;
    }
}

// e.g. raw_path=/SYS/VIEW/DBA_TABLES.SQL
// e.g. path[0]=SYS, path[1]=VIEW, path[2]=DBA_TABLES.SQL
static int qry_dbro_cache(char **path, t_fsentry *type) {
    char *cache_fname = NULL;// ddlfs-XDB.VIEW.DOCUMENT_LINKS2.SQL.tmp
    if (qry_object_fname(path[0], path[1], path[2], &cache_fname) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "qry_dbro_cache() - unable to determine filename, qry_object_fname() failed");
        if (cache_fname != NULL)
            free(cache_fname);
        return EXIT_FAILURE;
    }

    int retval = EXIT_FAILURE;
    if (tfs_quick_validate(cache_fname) == EXIT_SUCCESS) {

        // get last_ddl_time from cache
        time_t last_ddl_time;
        if (tfs_getldt(cache_fname, &last_ddl_time, NULL, NULL) != EXIT_SUCCESS) {
            if (cache_fname != NULL)
                free(cache_fname);
            return EXIT_FAILURE;
        }

        // get execution bit from cache
        struct stat file_stat;
        if(stat(cache_fname, &file_stat) < 0) {
            logmsg(LOG_ERROR, "qry_dbro_cache() - unable to determine execution bit, stat() failed");
            if (cache_fname != NULL)
                free(cache_fname);
            return EXIT_FAILURE;
        }

#ifdef _MSC_VER
		char ftype = 'F';
#else
        char ftype = ((file_stat.st_mode & S_IXUSR) ? 'F' : 'I');
#endif

        // load vfs from cache
        t_fsentry *entry = vfs_entry_create(
            ftype, // F=valid (execution bit set), I=invalid
            path[DEPTH_OBJECT],
            last_ddl_time,
            last_ddl_time);
        vfs_entry_add(type, entry);
        vfs_entry_sort(type);
        retval = EXIT_SUCCESS;
    }

    if (cache_fname != NULL)
        free(cache_fname);

    return retval;
}

// return NULL if file not found
static t_fsentry* fs_vfs_by_path(char **path, int loadFound) {
    if (path[0] == NULL) {
        qry_any(0, NULL, NULL);
        return g_vfs;
    }

    // don't bother looking for files which cannot possibly exist (e.g. '.Trash', '.hg', ...)
    if (path[DEPTH_OBJECT] != NULL) {
        char *suffix = strrchr(path[DEPTH_OBJECT], '.');
        if (suffix == NULL)
            return NULL;

#ifdef _MSC_VER
		char *suffix_upper = strdup(suffix);
		if (suffix_upper == NULL)
			return NULL;
		for (size_t i = 0; i < strlen(suffix_upper); i++)
			suffix_upper[i] = (char) toupper(suffix_upper[i]);
		if (strcmp(suffix_upper, ".JAVA") != 0 && strcmp(suffix_upper, ".SQL") != 0) {
			free(suffix_upper);
			return NULL;
		}
		free(suffix_upper);
#else
        if (strcasecmp(suffix, ".JAVA") != 0 && strcasecmp(suffix, ".SQL") != 0)
            return NULL;
#endif
    }

    if (path[DEPTH_TYPE] != NULL) {
        char *c = path[DEPTH_TYPE];
        while (*c != '\0') {
            if ((*c >= 'A' && *c <= 'Z') ||
                (*c >= 'a' && *c <= 'z') ||
                (*c == '_')) {
                c++;
                continue;
            }
            return NULL;
        }
    }
    //---

    // DEBUG
    /*
    logmsg(LOG_DEBUG, "-- path dump (loadFound=%d) --", loadFound);
    for (int i = 0; i < DEPTH_MAX; i++) {
        if (path[i] == NULL)
            logmsg(LOG_DEBUG, "path[%d]=NULL", i);
        else
            logmsg(LOG_DEBUG, "path[%d]=[%s]", i, path[i]);
    }
    logmsg(LOG_DEBUG, "-- end of path dump --");
    */
    // -- end of DEBUG --

    t_fsentry *entries[DEPTH_MAX] = {NULL, NULL, NULL};
    for (int i = 0; i < DEPTH_MAX; i++) {
        if (path[i] == NULL) {
            if (loadFound)
                qry_any(i, entries[DEPTH_SCHEMA], entries[DEPTH_TYPE]);
            return entries[i-1];
        }

        entries[i] = vfs_entry_search((i == 0 ? g_vfs : entries[i-1]), path[i]);
        if (entries[i] == NULL) {
            if (g_conf.dbro == 1 && i == DEPTH_OBJECT && path[DEPTH_OBJECT] != NULL) {
                if (qry_dbro_cache(path, entries[DEPTH_TYPE]) != EXIT_SUCCESS) {
                    qry_any(i, entries[DEPTH_SCHEMA], entries[DEPTH_TYPE]);
                }
             } else
                qry_any(i, entries[DEPTH_SCHEMA], entries[DEPTH_TYPE]);

            entries[i] = vfs_entry_search((i == 0 ? g_vfs : entries[i-1]), path[i]);
        }
        if (entries[i] == NULL) {
            // logmsg(LOG_ERROR, "File not found, depth=[%d], path_part=[%s].", i, path[i]);
            return NULL;
        }
    }

    // free cached vfs contents of the other (previous) schema...
    if (entries[DEPTH_SCHEMA] != NULL) {
        if ( (g_vfs_last_schema != NULL) && (strcmp(g_vfs_last_schema->fname, entries[DEPTH_SCHEMA]->fname) != 0)) {
            logmsg(LOG_DEBUG, "Clearing VFS for [%s] in favour of [%s]", g_vfs_last_schema->fname, entries[DEPTH_SCHEMA]->fname);
            vfs_entry_free(g_vfs_last_schema, 1);
        }
    }
    g_vfs_last_schema = entries[DEPTH_SCHEMA];

    return entries[DEPTH_MAX-1];
}

int fs_getattr(	const char *path, 
				DDLFS_STRUCT_STAT *st
				/*,  struct fuse_file_info *fi*/)
{
    char **part;
    int depth = fs_path_create(&part, path);
    t_fsentry *entry = fs_vfs_by_path(part, 0);

    if (strcmp(path, "/ddlfs.log") == 0) {
        st->st_uid = getuid();
        st->st_gid = getgid();
        
        #ifndef _MSC_VER
        st->st_atime = g_ddl_log_time;
        st->st_mtime = g_ddl_log_time;
        st->st_ctime = g_ddl_log_time;
        #endif

        st->st_nlink = 1;
        st->st_mode = S_IFREG | 0444;
        st->st_size = (off_t) (g_ddl_log_buf == NULL ? 0 : g_ddl_log_len);
        fs_path_free(part);
        return 0;
    }

    logmsg(LOG_DEBUG, "fuse-getattr: [%s]", path);

    if (entry == NULL) {
        logmsg(LOG_INFO, "fuse-getattr: File not found [%s]\n\n", path);
        fs_path_free(part);
        return -ENOENT;
    }

    struct stat tmp_st;
    tmp_st.st_size = g_conf.filesize;

    if (depth == DEPTH_MAX) {
        char *fname;
        qry_object_fname(part[DEPTH_SCHEMA], part[DEPTH_TYPE], part[DEPTH_OBJECT], &fname);
        stat(fname, &tmp_st);
        free(fname);
    }

#ifndef _MSC_VER
    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_blocks = 1;
#endif

#ifdef _MSC_VER
    // convert from 'time_t' to 'timestruc_t'
    struct timespec tmp;
    tmp.tv_sec = entry->modified;
    tmp.tv_nsec = 0;

    st->st_atim = tmp;
    st->st_mtim = tmp; // /* Time of last modification */
    st->st_ctim = tmp; // /* Time of last status change */
#else _MSC_VER        
    st->st_atime = entry->modified;
    st->st_mtime = entry->modified; // /* Time of last modification */
    st->st_ctime = entry->modified; // /* Time of last status change */
#endif

    if (entry->ftype == 'D') {
        st->st_nlink = 2;
        st->st_mode = S_IFDIR | 0644;
    } else {
        if (g_conf.dbro == 1) {
            // https://github.com/usrecnik/ddlfs/issues/11
            st->st_mode = S_IFREG | 0444;
        } else {
            if ((depth == DEPTH_MAX) &&
                ((strcasecmp(part[DEPTH_TYPE], "TABLE") == 0) || (strcasecmp(part[DEPTH_TYPE], "MATERIALIZED_VIEW") == 0))) {
                // tables and materialized views are always read only as they cannot be "create or REPLACEd"
                st->st_mode = S_IFREG | 0444;
            } else {
                if (entry->ftype == 'F')
                    st->st_mode = S_IFREG | 0744;
                else
                    st->st_mode = S_IFREG | 0644;
            }
        }
        
        st->st_nlink = 1;
        st->st_size = tmp_st.st_size;
    }

    fs_path_free(part);
    return 0;
}

int fs_readdir(	const char *path, 
               	void *buffer, 
               	fuse_fill_dir_t filler,
				off_t offset, 
               	struct fuse_file_info *fi
               	/*, enum fuse_readdir_flags flags */)
{
    logmsg(LOG_DEBUG, "fuse-readdir: [%s]", path);

    char **part;
    fs_path_create(&part, path);
    t_fsentry *entry = fs_vfs_by_path(part, 1);

    if (entry == NULL) {
        logmsg(LOG_DEBUG, "File not found for path [%s]", path);
        fs_path_free(part);
        return -ENOENT;
    }

    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);
    for (int i = 0; i < entry->count; i++)
        filler(buffer, entry->children[i]->fname, NULL, 0);

    fs_path_free(part);
    
    return 0;
}

// return file handle or -1 on error
static int fake_open(const char *path,
                     struct fuse_file_info *fi) {

    logmsg(LOG_DEBUG, "fake-open: [%s]", path);
    char **part;
    int depth = fs_path_create(&part, path);
    if (depth != DEPTH_MAX) {
        logmsg(LOG_ERROR, "Unable to open file at depth=%d (%s).", depth, path);
        fs_path_free(part);
        return -1;
    }

    char *fname = NULL;
    if (qry_object(part[0], part[1], part[2], &fname) != EXIT_SUCCESS) {
        if (fname != NULL)
            free(fname);
        fs_path_free(part);
        return -1;
    }
    
    int fh;
    if (fi != NULL)
        fh = open(fname, O_RDWR);
    else
        fh = open(fname, O_RDONLY);
    
    if (fh < 0) {
        logmsg(LOG_ERROR, "Unable to open [%s] for passthrough (%d)",
            fname, errno);
        if (fname != NULL)
            free(fname);
        fs_path_free(part);
        return -1;
    }

    if (fname != NULL)
        free(fname);
    fs_path_free(part);

    return fh;
}

int fs_open(const char *path,
            struct fuse_file_info *fi) {

    if (strcmp(path, "/ddlfs.log") == 0) {
        fi->direct_io = 1;
        return 0;
    }

    char flagmsg[30];
    if ((fi->flags & O_ACCMODE) == O_RDONLY)
        strcpy(flagmsg, "RDONLY");
    else if ((fi->flags & O_ACCMODE) == O_WRONLY)
        strcpy(flagmsg, "O_WRONLY");
    else if ((fi->flags & O_ACCMODE) == O_RDWR)
        strcpy(flagmsg, "O_RDWR");
    else
        strcpy(flagmsg, "none");

    logmsg(LOG_INFO, "fuse-open: [%s], [%s]", path, flagmsg);

    int fh = fake_open(path, fi);
    if (fh < 0) {
        logmsg(LOG_ERROR, "Unable to fs_open(%s).", path);
        return -ENOENT;
    }
    fi->direct_io = 1;
    fi->fh = fh;
    logmsg(LOG_DEBUG, "URHDBG fs_open(): opened file handle [%d]", fi->fh);
    return 0;
}

static size_t fs_read_ddl_log(char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (g_ddl_log_buf == NULL)
        return 0;

    size_t len = strlen(g_ddl_log_buf);
    if ((int) offset >= (int) len)
        return 0;

    if (offset + size > len) {
        memcpy(buf, g_ddl_log_buf + offset, len - offset);
        return len - offset;
    }

    memcpy(buf, g_ddl_log_buf + offset, size);
    return size;
}

int fs_read(const char *path, 
			char *buffer, 
			size_t size, 
			off_t offset, 
			struct fuse_file_info *fi)
{

    logmsg(LOG_DEBUG, "fuse-read: [%s], offset=[%d]", path, offset);

    int fd;
    int res;

#ifdef _MSC_VER
    if (strcmp(path, "/ddlfs.log") == 0)
#else
    if (strcmp(path, "\\ddlfs.log") == 0)
#endif
        return (int) fs_read_ddl_log(buffer, size, offset, fi);

    if (fi == NULL)
        fd = fake_open(path, NULL);
    else
        fd = (int) fi->fh;

    if (fd < 0) {
        logmsg(LOG_ERROR, "fuse-read failed, fd is negative (%d)", fd);
        return -ENOENT;
    }

    res = pread(fd, buffer, size, offset);
    if (res == -1) {
        logmsg(LOG_ERROR, "pread returned error.");
        res = -errno;
    }

    if (fi == NULL )
        close(fd);

    return res;
}

int fs_write(const char *path,
             const char *buf,
             size_t size,
             off_t offset,
             struct fuse_file_info *fi) {
    logmsg(LOG_INFO, "fuse-write: [%s]", path);
    int res = pwrite((int) fi->fh, buf, size, offset);
    if (res == -1)
        return -errno;
    return res;
}

int fs_release(const char *path,
               struct fuse_file_info *fi) {

    int retval = 0;
    char **part = NULL;
    char *fname = NULL;
    char *object_name = NULL;
    char *object_schema = NULL;
    struct stat tmp_stat;
    size_t buf_len = 0;
    char *buf = NULL;
    int fd = -1;
    int cache_already_removed = 0;

    if (strcmp(path, "/ddlfs.log") == 0)
        return 0;

    logmsg(LOG_INFO, "fuse-release: [%s]", path);

    fs_path_create(&part, path);
    qry_object_fname(part[DEPTH_SCHEMA], part[DEPTH_TYPE], part[DEPTH_OBJECT], &fname);

    int is_java_source = (strcmp(part[DEPTH_TYPE], "JAVA_SOURCE") == 0 ? 1 : 0);
    int is_table_source = (strcmp(part[DEPTH_TYPE], "TABLE") == 0 ? 1 : 0);
    int is_mview_source = (strcmp(part[DEPTH_TYPE], "MATERIALIZED_VIEW") == 0 ? 1 : 0);

    if (close((int) fi->fh) != 0) {
        // closing also flushes metadata, such as mtime
        logmsg(LOG_DEBUG, "Unable to close underlying file (%s), error=%d", fname, errno);
        fs_path_free(part);
        free(fname);
        return -errno;
    }
    logmsg(LOG_DEBUG, "URHDBG fs_release(): closed file handle [%d]", fi->fh);

    // read file to buffer
    if (fi->flags & O_RDWR || fi->flags & O_WRONLY) {
        logmsg(LOG_DEBUG, "fs_release() - URHDBG: READING FILE TO BUFFER");
        if (stat(fname, &tmp_stat) != 0) {
            logmsg(LOG_ERROR, "fs_release() - fstat failed for [%s]", fname);
            retval = -1;
            goto fs_release_final;
        }
        if (tmp_stat.st_mtime == 0) {
            logmsg(LOG_DEBUG, "fs_release() - no write occured even though file was opened as r/w.");
        } else if (is_table_source || is_mview_source) {
            logmsg(LOG_ERROR, "fs_release() - write not supported for TABLE and MATERIALIZED_VIEW types.");
        } else {
            logmsg(LOG_DEBUG, "(temp file size is %d bytes)", tmp_stat.st_size);

            buf_len = ((tmp_stat.st_size + (is_java_source * 350)) * sizeof(char)) + 1;
            buf = malloc(buf_len);
            if (buf == NULL) {
                logmsg(LOG_ERROR, "fs_release() - unable to malloc buf (buf_len=[%d])", buf_len);
                retval = -ENOMEM;
                goto fs_release_final;
            }

             // determine object name/schema
            if (str_fn2obj(&object_name, part[DEPTH_OBJECT], part[DEPTH_TYPE]) != EXIT_SUCCESS) {
                logmsg(LOG_ERROR, "fs_create() - unable to convert object to file name");
                return -ENOMEM;
            }

            if (str_fn2obj(&object_schema, part[DEPTH_SCHEMA], NULL) != EXIT_SUCCESS) {
                logmsg(LOG_ERROR, "fs_create() - unable to convert schema to file name");
                return -ENOMEM;
            }

            logmsg(LOG_DEBUG, "Reading %d in buffer size %d, FD=%d", tmp_stat.st_size, buf_len, fi->fh);
            fd = open(fname, O_RDONLY);
            if (fd == -1) {
                logmsg(LOG_ERROR, "fs_release() - unable to re-open temp file as r/o.");
                retval = -1;
                goto fs_release_final;
            }
            buf[0] = '\0';

            size_t newLenJava = 0;
            if (is_java_source == 1)
                newLenJava = sprintf(buf, "CREATE OR REPLACE AND COMPILE JAVA SOURCE NAMED \"%s\".\"%s\" AS\n",
                    object_schema, object_name);

            size_t newLen = read(fd, buf+strlen(buf), tmp_stat.st_size);
            if (newLen == -1 || newLen != tmp_stat.st_size) {
                logmsg(LOG_ERROR, "fs_release() - unable to read() [%s], errno=%d (%s) [%d]!=[%d]", fname, errno, strerror(errno), newLen, tmp_stat.st_size);
                retval = -1;
                goto fs_release_final;
            }
            logmsg(LOG_DEBUG, "Read %d bytes", newLen);
            // newLen++;
            if (is_java_source == 1)
              buf[newLen+newLenJava-1] = '\0';
            else
              buf[newLen] = '\0';

            if (newLen == 0)
                logmsg(LOG_DEBUG, "Skipping execution of DDL as input file size is 0.");
            else
                qry_exec_ddl(object_schema, object_name, buf);

            if (tfs_rmfile(fname) != EXIT_SUCCESS) {
                logmsg(LOG_ERROR, "fs_release - unable to remove cache file [%s] after DDL.", fname);
            }
            cache_already_removed = 1;
        }
    }

    // @todo - consider *never* deleting this file here (and delete all temp files on umount).
    if (g_conf.filesize != -1) {
        // delete temp file. g_conf.filesie=-1 means exact filesizes. deleting this file would
        // invalidate cached files (which would be cause a bit of performance degradation,
        // functionally it wold be ok)

        if ((cache_already_removed == 0) && (tfs_rmfile(fname) != EXIT_SUCCESS))
            logmsg(LOG_ERROR, "fs_release - unable to remove cached file [%s] fd=[%d]", fname, fd);
    }

fs_release_final:

    if (fd != -1)
        if (close(fd) == -1)
            logmsg(LOG_ERROR, "fs_release() - unable to close underlying r/o file");

    if (buf != NULL)
        free(buf);

    if (object_schema != NULL)
        free(object_schema);

    if (object_name != NULL)
        free(object_name);

    fs_path_free(part);
    free(fname);

    return retval;
}


int fs_create (const char *path,
               mode_t mode,
               struct fuse_file_info *fi) {

    DDLFS_STRUCT_STAT st;
    char **part;
    int depth;
    char *object_type = NULL;
    char *object_name = NULL;
    char *object_schema = NULL;
    char empty_ddl[1024] = "";
    logmsg(LOG_INFO, "fs_create() - [%s]", path);


    if (fs_getattr(path, &st) == -ENOENT) {
        logmsg(LOG_INFO, "fs_create() - creating empty object for [%s]", path);

        depth = fs_path_create(&part, path);

        if (str_fn2obj(&object_type, part[DEPTH_TYPE], NULL) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "fs_create() - unable to convert object to file name");
            fs_path_free(part);
            return -ENOMEM;
        }

        if (str_fn2obj(&object_name, part[DEPTH_OBJECT], part[DEPTH_TYPE]) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "fs_create() - unable to convert object to file name");
            fs_path_free(part);
            return -ENOMEM;
        }

        if (str_fn2obj(&object_schema, part[DEPTH_SCHEMA], NULL) != EXIT_SUCCESS) {
            logmsg(LOG_ERROR, "fs_create() - unable to convert schema to file name");
            fs_path_free(part);
            return -ENOMEM;
        }

        if (depth != 3) {
            logmsg(LOG_ERROR, "Creating of new objects is only allowed on depth level 3");
            fs_path_free(part);
            return -EINVAL;
        }

        if (strcmp(object_type, "PROCEDURE") == 0)
            snprintf(empty_ddl, 1023, "CREATE PROCEDURE \"%s\".\"%s\" AS\nBEGIN\n    NULL;\nEND;",
                object_schema, object_name);
        else if (strcmp(object_type, "FUNCTION") == 0)
            snprintf(empty_ddl, 1023, "CREATE FUNCTION \"%s\".\"%s\" RETURN NUMBER AS\nBEGIN\n    RETURN NULL;\nEND;",
                object_schema, object_name);
        else if (strcmp(object_type, "VIEW") == 0)
            snprintf(empty_ddl, 1023, "CREATE VIEW \"%s\".\"%s\" AS\nSELECT * FROM dual",
                object_schema, object_name);
        else if (strcmp(object_type, "TYPE") == 0)
            snprintf(empty_ddl, 1023, "CREATE TYPE \"%s\".\"%s\" AS OBJECT(\nn NUMBER)",
                object_schema, object_name);
        else if (strcmp(object_type, "TYPE_BODY") == 0)
            snprintf(empty_ddl, 1023, "CREATE TYPE BODY \"%s\".\"%s\" AS\n\nEND;",
                object_schema, object_name);
        else if (strcmp(object_type, "PACKAGE_SPEC") == 0)
            snprintf(empty_ddl, 1023, "CREATE PACKAGE \"%s\".\"%s\" AS\n\nEND;",
                object_schema, object_name);
        else if (strcmp(object_type, "PACKAGE_BODY") == 0)
            snprintf(empty_ddl, 1023, "CREATE PACKAGE BODY \"%s\".\"%s\" AS\n\nEND;",
                object_schema, object_name);
        else if (strcmp(object_type, "JAVA_SOURCE") == 0)
            snprintf(empty_ddl, 1023, "CREATE AND COMPILE JAVA SOURCE NAMED \"%s\".\"%s\" AS\npublic class %s {\n}",
                object_schema, object_name, object_name);
        else {
            logmsg(LOG_ERROR, "Cannot create empty object of type [%s]- this is not supported.", part[DEPTH_TYPE]);

            fs_path_free(part);
            // @todo - support other object types
            return -EINVAL; // invalid argument
        }

        qry_exec_ddl(object_schema, object_name, empty_ddl);

        fs_path_free(part);
    }

    if (object_type != NULL)
        free(object_type);

    if (object_name != NULL)
        free(object_name);

    if (object_schema != NULL)
        free(object_schema);

    return fs_open(path, fi);
}

int fs_truncate(const char *path,
                off_t size
                /* struct fuse_file_info *fi - in newer fuse */) {
    logmsg(LOG_INFO, "fs_truncate() - [%s]", path);

    char **part;
    int depth = fs_path_create(&part, path);
    char *fname;

    if (depth != DEPTH_MAX) {
        logmsg(LOG_ERROR, "fs_truncate() - you can only truncate files in depth 3 (sql files)");
        fs_path_free(part);
        return -1;
    }

    qry_object_fname(part[DEPTH_SCHEMA], part[DEPTH_TYPE], part[DEPTH_OBJECT], &fname);
   
	int tmpfd = open(fname, O_TRUNC | O_WRONLY);
	if (tmpfd == -1) {
		// if (truncate(fname, 0) == -1) { // this is unix-only function, thus replaced by open(fname, O_TRUNC)
        logmsg(LOG_ERROR, "fs_truncate() - unable to truncate [%s], errno=[%d]", fname, errno);
        fs_path_free(part);
        free(fname);
        return -errno;
    }
	close(tmpfd);

    fs_path_free(part);
    free(fname);
    return 0;
}

int fs_unlink(const char *path) {
    logmsg(LOG_INFO, "fs_unlink() - [%s]", path);

    char **part;
    int depth = fs_path_create(&part, path);
    char drop_ddl[1024] = "";
    char *object_type = NULL;
    char *object_name = NULL;
    char *object_schema = NULL;


    if (depth != 3) {
        logmsg(LOG_ERROR, "fs_unlink() - Cannot unlink objects which are not at level 3");
        fs_path_free(part);
        return -EINVAL;
    }
    if (str_fn2obj(&object_type, part[DEPTH_TYPE], NULL) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "fs_unlink() - unable to convert object to file name");
        fs_path_free(part);
        return -ENOMEM;
    }
    if (str_fn2obj(&object_name, part[DEPTH_OBJECT], part[DEPTH_TYPE]) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "fs_unlink() - unable to convert object to file name");
        fs_path_free(part);
        return -ENOMEM;
    }
    if (str_fn2obj(&object_schema, part[DEPTH_SCHEMA], NULL) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "fs_unlink() - unable to convert schema to file name");
        fs_path_free(part);
        return -ENOMEM;
    }

    if (strcmp(object_type, "PROCEDURE") == 0)
        snprintf(drop_ddl, 1023, "DROP PROCEDURE \"%s\".\"%s\"",            object_schema, object_name);
    else if (strcmp(object_type, "FUNCTION") == 0)
        snprintf(drop_ddl, 1023, "DROP FUNCTION \"%s\".\"%s\"",             object_schema, object_name);
    else if (strcmp(object_type, "VIEW") == 0)
        snprintf(drop_ddl, 1023, "DROP VIEW \"%s\".\"%s\"",                 object_schema, object_name);
    else if (strcmp(object_type, "TYPE") == 0)
        snprintf(drop_ddl, 1023, "DROP TYPE \"%s\".\"%s\"",                 object_schema, object_name);
    else if (strcmp(object_type, "TYPE_BODY") == 0)
        snprintf(drop_ddl, 1023, "DROP TYPE BODY \"%s\".\"%s\"",            object_schema, object_name);
    else if (strcmp(object_type, "PACKAGE_SPEC") == 0)
        snprintf(drop_ddl, 1023, "DROP PACKAGE \"%s\".\"%s\"",              object_schema, object_name);
    else if (strcmp(object_type, "PACKAGE_BODY") == 0)
        snprintf(drop_ddl, 1023, "DROP PACKAGE BODY \"%s\".\"%s\"",         object_schema, object_name);
    else if (strcmp(object_type, "JAVA_SOURCE") == 0)
        snprintf(drop_ddl, 1023, "DROP JAVA SOURCE \"%s\".\"%s\"",          object_schema, object_name);
    else if (strcmp(object_type, "TRIGGER") == 0)
        snprintf(drop_ddl, 1023, "DROP TRIGGER \"%s\".\"%s\"",              object_schema, object_name);
    else if (strcmp(object_type, "TABLE") == 0)
        snprintf(drop_ddl, 1023, "DROP TABLE \"%s\".\"%s\"",                object_schema, object_name);
    else if (strcmp(object_type, "MATERIALIZED_VIEW") == 0)
        snprintf(drop_ddl, 1023, "DROP MATERIALIZED VIEW \"%s\".\"%s\"",    object_schema, object_name);
    else {
        logmsg(LOG_ERROR, "fs_unlink() - Cannot drop object %s.%s, operation not (yet?) supported.",
            part[DEPTH_SCHEMA], part[DEPTH_OBJECT]);
        fs_path_free(part);
        return -EINVAL;
    }

    qry_exec_ddl(object_schema, object_name, drop_ddl);

    // delete cache file
    char *cache_fn = NULL;
    if (qry_object_fname(part[DEPTH_SCHEMA], part[DEPTH_TYPE], part[DEPTH_OBJECT], &cache_fn) != EXIT_SUCCESS) {
        logmsg(LOG_ERROR, "fs_unlink() - unable to determin cache filename for [%s] [%s].[%s]", part[DEPTH_TYPE], part[DEPTH_SCHEMA], part[DEPTH_OBJECT]);
        return -EINVAL;
    }
    if (access(cache_fn, F_OK) == 0)
        tfs_rmfile(cache_fn);

    // remove vfs vfs_etry
    t_fsentry *vfs_schema = vfs_entry_search(g_vfs, part[DEPTH_SCHEMA]);
    if (vfs_schema != NULL) {
        t_fsentry *vfs_type = vfs_entry_search(vfs_schema, part[DEPTH_TYPE]);
        if (vfs_type != NULL) {
            vfs_entry_free(vfs_type, 1);
            logmsg(LOG_DEBUG, "fs_unlink() - cleared vfs for [%s]/[%s]", vfs_schema->fname, vfs_type->fname);
        }
    }

    // cleanup
    fs_path_free(part);
    if (cache_fn != NULL)
        free(cache_fn);

    if (object_type != NULL)
        free(object_type);

    if (object_schema != NULL)
        free(object_schema);

    if (object_name != NULL)
        free(object_name);

    return 0;
}
