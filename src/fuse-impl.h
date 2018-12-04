#pragma once

#ifdef _MSC_VER
int fs_getattr(const char *path, struct FUSE_STAT *st /*,  struct fuse_file_info *fi*/);
#else
int fs_getattr(const char *path, struct stat *st /*,  struct fuse_file_info *fi*/);
#endif

#ifdef _MSC_VER
int fs_readdir(const char *path, 
               void *buffer, 
               fuse_fill_dir_t filler,
               FUSE_OFF_T offset, //off_t offset, 
               struct fuse_file_info *fi
               /*, enum fuse_readdir_flags flags */);
#else
int fs_readdir(const char *path,
	           void *buffer,
	           fuse_fill_dir_t filler,
	           off_t offset,
	           struct fuse_file_info *fi);
#endif

int fs_read(const char *path, 
			char *buffer, 
			size_t size, 
			off_t offset, 
			struct fuse_file_info *fi);

int fs_write(const char *path,
			 const char *buf,
			 size_t size,
			 off_t offset,
             struct fuse_file_info *fi);

int fs_open(const char *path,
            struct fuse_file_info *fi);

int fs_release(const char *path,
			   struct fuse_file_info *fi);

int fs_create (const char *path,
               mode_t mode,
               struct fuse_file_info *fi);

int fs_truncate(const char *path, 
                off_t size
                /* @todo in newer fuse: struct fuse_file_info *fi*/);
 
int fs_unlink(const char *path);

