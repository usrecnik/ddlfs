#define _GNU_SOURCE

#include <fuse.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

#include "vfs.h"
#include "logging.h"
#include "query.h"
#include "fuse-impl.h"

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

static void qry_any(int depth, t_fsentry* schema, t_fsentry *type) {
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

// return NULL if file not found
static t_fsentry* fs_vfs_by_path(char **path, int loadFound) {
	if (path[0] == NULL) 
		return g_vfs;

	t_fsentry *entries[DEPTH_MAX] = {NULL, NULL, NULL};
	for (int i = 0; i < DEPTH_MAX; i++) {
		if (path[i] == NULL) {
			if (loadFound)
				qry_any(i, entries[DEPTH_SCHEMA], entries[DEPTH_TYPE]);

			return entries[i-1];
		}
		entries[i] = vfs_entry_search((i == 0 ? g_vfs : entries[i-1]), path[i]);
		if (entries[i] == NULL) {
			qry_any(i, entries[DEPTH_SCHEMA], entries[DEPTH_TYPE]);
			entries[i] = vfs_entry_search((i == 0 ? g_vfs : entries[i-1]), path[i]);
		}
		if (entries[i] == NULL) {
			logmsg(LOG_ERROR, "File not found, depth=[%d], path_part=[%s].", i, path[i]);
			return NULL;
		}
	}
	
	return entries[DEPTH_MAX-1];
}

int fs_getattr( const char *path, struct stat *st )
{
    logmsg(LOG_DEBUG, "fuse-getattr: [%s]", path);
	//vfs_dump(g_vfs, 1);
	char **part;
	int depth = fs_path_create(&part, path);	
	t_fsentry *entry = fs_vfs_by_path(part, 0);
	//printf("\n--\n");
	//vfs_dump(g_vfs, 1);
	if (entry == NULL) {
		logmsg(LOG_ERROR, "fuse-getattr: File not found [%s]\n\n", path);
		return -ENOENT;
	}

	struct stat tmp_st;
	tmp_st.st_size = 0; 

	if (depth == DEPTH_MAX) {
		char *fname;
		qry_object_fname(part[DEPTH_SCHEMA], part[DEPTH_TYPE], part[DEPTH_OBJECT], &fname);
		stat(fname, &tmp_st);
		//logmsg(LOG_DEBUG, "getattr: reading real size for [%s] = [%d]", fname, tmpsize);
	}	

	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = entry->modified;
	st->st_mtime = entry->modified; // /* Time of last modification */
	st->st_ctime = entry->modified; // /* Time of last status change */
	
	if (entry->ftype == 'D') {
		st->st_nlink = 2;
		st->st_mode = S_IFDIR | 0755;
	} else {
		st->st_nlink = 1;
		st->st_mode = S_IFREG | 0755;
		st->st_size = tmp_st.st_size;
	}
	
	if (depth == DEPTH_MAX) {
		if (strcmp(part[DEPTH_OBJECT], "R") == 0) {
			logmsg(LOG_DEBUG, "..R; uid=[%d], gid=[%d], atime=[%d], mtime=[%d], ctime=[%d], nlink=[%d], mode=[%d], size=[%d]",
				st->st_uid, st->st_gid, st->st_atime, st->st_mtime, st->st_ctime, st->st_nlink, st->st_mode, st->st_size);
		}
	}
    return 0;
}

int fs_readdir(const char *path, 
			   void *buffer, 
			   fuse_fill_dir_t filler, 
			   off_t offset, 
			   struct fuse_file_info *fi) {

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
	// logmsg(LOG_DEBUG, ".. number of items in directory [%s] is [%d]", entry->fname, entry->count);
	for (int i = 0; i < entry->count; i++) {
		// logmsg(LOG_DEBUG, ".. index=[%d], name=%s", i, entry->children[i]->fname);
		filler(buffer, entry->children[i]->fname, NULL, 0);
	}
	
	fs_path_free(part);
    return 0;
}

// return file handle or -1 on error
static int fake_open(const char *path,
					 struct fuse_file_info *fi) {

	logmsg(LOG_INFO, "fake-open: [%s]", path);
	char **part;
	int depth = fs_path_create(&part, path);
	if (depth != DEPTH_MAX) {
		logmsg(LOG_ERROR, "Unable to open file at depth=%d (%s).", depth, path);
		return -1;
	}

	char *fname;
	if (qry_object(part[0], part[1], part[2], &fname) != EXIT_SUCCESS)
		return -1;

	logmsg(LOG_DEBUG, "fake open for real file of [%s]", fname);

	int fh;
	if (fi != NULL) {
		fh = open(fname, O_RDWR);
		logmsg(LOG_DEBUG, "FAKE-OPEN opening as read/write");
	} else {
		logmsg(LOG_DEBUG, "FAKE-OPEN opening as read only");
		fh = open(fname, O_RDONLY);
	}

	if (fh < 0) {
		logmsg(LOG_ERROR, "Unable to open [%s] for passthrough (%d)", 
			fname, errno);
		return -1;
	}
	
	logmsg(LOG_DEBUG, "fake_open returning handle %d", fh);
	return fh;	
}

int fs_open(const char *path, 
			struct fuse_file_info *fi) {

	logmsg(LOG_INFO, "fuse-open: [%s]", path);
	int fh = fake_open(path, fi);
	if (fh < 0) {
		logmsg(LOG_ERROR, "Unable to fs_open(%s).", path);
		return -ENOENT;
	}
	fi->direct_io=1;
	fi->fh = fh;
		
	return 0;
}

int fs_read(const char *path, 
			char *buf, 
			size_t size, 
			off_t offset,
		    struct fuse_file_info *fi) {

	logmsg(LOG_INFO, "fuse-read: [%s], offset=[%d]", path, offset);
	
	int fd;
	int res;

	if (fi == NULL)
		fd = fake_open(path, NULL);	
	else
		fd = fi->fh;
	
	if (fd < 0) {
		logmsg(LOG_ERROR, "fuse-read failed, fd is negative (%d)", fd);
		return -ENOENT;
	}

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

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
	int res = pwrite(fi->fh, buf, size, offset);
	if (res == -1)
		return -errno;
	return res;	
}

int fs_release(const char *path, 
			   struct fuse_file_info *fi) {
	
	logmsg(LOG_INFO, "fuse-release: [%s]", path);
	int retval = 0;
	char **part;
	char *fname;
	struct stat tmp_stat; 
	size_t buf_len = 0;
	char *buf = NULL;

    fs_path_create(&part, path);
	qry_object_fname(
		part[DEPTH_SCHEMA], part[DEPTH_TYPE], part[DEPTH_OBJECT], &fname);

	// read file to buffer
	if (fi->flags & O_RDWR || fi->flags & O_WRONLY) {
		// @todo - check if there was at least one write call since the file was open
		logmsg(LOG_DEBUG, "RW FLAGS!");

		if (fstat(fi->fh, &tmp_stat) != 0) {
			logmsg(LOG_ERROR, "fs_release() - fstat failed for [%s], fd=%d", fname, fi->fh);
			retval = -1;
			goto fs_release_final;
		}

		logmsg(LOG_DEBUG, "(temp file size is %d bytes)", tmp_stat.st_size);
	
		buf_len = (tmp_stat.st_size * sizeof(char)) + 1;
		buf = malloc(buf_len);
		if (buf == NULL) {
			logmsg(LOG_ERROR, "fs_release() - unable to malloc buf (buf_len=[%d])", buf_len);
			retval = -ENOMEM;
			goto fs_release_final;
		}

		logmsg(LOG_DEBUG, "Reading %d in buffer size %d, FD=%d", tmp_stat.st_size, buf_len, fi->fh);
		size_t newLen = read(fi->fh, buf, tmp_stat.st_size);
		if (newLen == -1 || newLen != tmp_stat.st_size) {
			logmsg(LOG_ERROR, "fs_release() - unable to read() [%s], errno=%d (%s)", fname, errno, strerror(errno));
			retval = -1;
			goto fs_release_final;
		}
		logmsg(LOG_DEBUG, "Read %d bytes", newLen);
		newLen++;
		buf[newLen] = '\0';

        qry_exec_ddl(buf);
        
		logmsg(LOG_DEBUG, "Write Buffer [%s]", buf);
	}

	if (close(fi->fh) != 0) {
		logmsg(LOG_DEBUG, "Unable to close underlying file (%s), error=%d", fname, errno);
		return -errno;
	}

	if (unlink(fname) != 0) {
		logmsg(LOG_ERROR, "Unable to delete underlying file (%s), error=%d", fname, errno);
		return -errno;
	}

	
fs_release_final:

	if (buf != NULL)
		free (buf);

	logmsg(LOG_DEBUG, "fs_release() returning: %d", retval);
	return retval;
}

int fs_create (const char *path, 
               mode_t mode,
               struct fuse_file_info *fi) {

    struct stat st;
    char **part;
    int depth;
    char empty_ddl[1024] = "";
    logmsg(LOG_INFO, "fs_create() - [%s]", path);

    if (fs_getattr(path, &st) == -ENOENT) {
        logmsg(LOG_INFO, "fs_create() - creating empty object for [%s]", path);
        
        depth = fs_path_create(&part, path);
        logmsg(LOG_INFO, ".. depth=[%d]", depth);
        if (depth != 3) {
            logmsg(LOG_ERROR, "Creating of new objects is only allowed on depth level 3");
            return -EINVAL;
        }
         
        if (strcmp(part[DEPTH_TYPE], "PROCEDURE") == 0) {
            snprintf(empty_ddl, 1023, "CREATE PROCEDURE \"%s\".\"%s\" AS\nBEGIN\n    NULL;\nEND;",
                part[DEPTH_SCHEMA], part[DEPTH_OBJECT]);
        } else {
            logmsg(LOG_ERROR, "Cannot create empty object of type [%s]", part[DEPTH_TYPE]);
            // @todo - support other object types
            return -EINVAL; // invalid argument 
        }
        
        qry_exec_ddl(empty_ddl);

        fs_path_free(part);
    }
    
    return fs_open(path, fi);
}

int fs_truncate(const char *path, 
                off_t size
                /* struct fuse_file_info *fi - in newer fuse */) {
    logmsg(LOG_INFO, "fs_truncate() - [%s]", path);
    // do nothing
    return 0;
}

int fs_unlink(const char *path) {
    logmsg(LOG_INFO, "fs_unlink() - [%s]", path);
    
    char **part;
    int depth = fs_path_create(&part, path);
    char drop_ddl[1024] = "";
    
    if (depth != 3) {
        logmsg(LOG_ERROR, "Cannot unlink objects which are not at level 3");
        return -EINVAL;
    }

    if (strcmp(part[DEPTH_TYPE], "PROCEDURE") == 0) {
        snprintf(drop_ddl, 1023, "DROP PROCEDURE \"%s\".\"%s\"", 
            part[DEPTH_SCHEMA], part[DEPTH_OBJECT]);
    } else {
        logmsg(LOG_ERROR, "Cannot drop object %s.%s, operation not (yet?) supported.", 
            part[DEPTH_SCHEMA], part[DEPTH_OBJECT]);
        return -EINVAL;
    }
    fs_path_free(part);
    
    qry_exec_ddl(drop_ddl);
    return 0;
}
