#pragma once

#ifdef _MSC_VER
	#define DDLFS_STRUCT_STAT struct FUSE_STAT
#else
	#define DDLFS_STRUCT_STAT struct stat
#endif

int fs_getattr(const char *path,
			   DDLFS_STRUCT_STAT *st);

#ifdef _MSC_VER
int fs_getattr_win(const char *path,
				   DDLFS_STRUCT_STAT *st);
#else
int fs_getattr_lnx(const char *path,
				   DDLFS_STRUCT_STAT *st,
				   struct fuse_file_info *fi);
#endif

int fs_readdir(	const char *path,
               	void *buffer, 
               	fuse_fill_dir_t filler,
				off_t offset,
               	struct fuse_file_info *fi);

#ifdef _MSC_VER
int fs_readdir_win(const char *path,
               	   void *buffer,
               	   fuse_fill_dir_t filler,
				   __int64 offset,
               	   struct fuse_file_info *fi);
#else
int fs_readdir_lnx(const char *path,
               	   void *buffer,
               	   fuse_fill_dir_t filler,
				   off_t offset,
               	   struct fuse_file_info *fi,
               	   enum fuse_readdir_flags flags);
#endif

int fs_read(const char *path, 
			char *buffer, 
			size_t size, 
			off_t offset, 
			struct fuse_file_info *fi);

#ifdef _MSC_VER
int fs_read_win(const char* path,
	char* buffer,
	size_t size,
	__int64 offset,
	struct fuse_file_info* fi);
#endif


int fs_write(const char *path,
			 const char *buf,
			 size_t size,
			 off_t offset,
             struct fuse_file_info *fi);
#ifdef _MSC_VER
int fs_write_win(const char* path,
	const char* buf,
	size_t size,
	__int64 offset,
	struct fuse_file_info* fi);
#endif

int fs_open(const char *path,
            struct fuse_file_info *fi);

int fs_release(const char *path,
			   struct fuse_file_info *fi);

int fs_create (const char *path,
               mode_t mode,
               struct fuse_file_info *fi);

int fs_truncate(const char *path, 
                off_t size,
                struct fuse_file_info *fi);
 
int fs_unlink(const char *path);
