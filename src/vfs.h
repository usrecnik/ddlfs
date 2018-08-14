#pragma once

#include <time.h>

typedef struct s_fsentry {
    char    ftype;
    char    *fname;
    off_t   fsize;
    time_t  created;  
    time_t  modified;
    struct  s_fsentry **children;
	int 	capacity;
	int 	count;
	int     allocated; // for debugging
} t_fsentry;

t_fsentry *g_vfs;
t_fsentry *g_vfs_last_schema;


t_fsentry* vfs_entry_create(const char type, 
							const char *fname, 
							time_t created, 
							time_t modified);

t_fsentry* vfs_entry_create2(const char type, 
							const char *fname, 
							time_t created, 
							time_t modified);


void vfs_dump(t_fsentry *entry, int depth);

void vfs_entry_dump(const char *msg, t_fsentry *entry);

void vfs_entry_add(t_fsentry *parent, t_fsentry *child);

void vfs_entry_free(t_fsentry *entry, int children_only);

t_fsentry* vfs_entry_search(t_fsentry *entry, const char *fname);

void vfs_entry_sort(t_fsentry *parent);

