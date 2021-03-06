#include <stdlib.h>
#include <string.h>

#ifdef _WIN64
	#include <SYS\TYPES.H>
	#define strdup _strdup
#endif

#include "vfs.h"
#include "logging.h"

t_fsentry* vfs_entry_create(const char type, 
                            const char *fname, 
                            time_t created, 
                            time_t modified) {

    t_fsentry *t = malloc(sizeof(t_fsentry));
    t->ftype = type;
    t->fname = strdup(fname);
    t->fsize = 0;
    t->created = created;
    t->modified = modified;
    t->count = 0;
    if (type == 'D') {
        t->capacity = 100;
        t->children = malloc(sizeof(t_fsentry*) * t->capacity);
        if (t->children == NULL) {
            logmsg(LOG_ERROR, "Unable to malloc children for %s", t->fname);
            free(t);
            return NULL;
        }
    } else {
        t->capacity = 0;
        t->children = NULL;
    }
    t->allocated = 1;
    // logmsg(LOG_DEBUG, "++ VFS_ENTRY (%s) addr=[%p]", fname, t);
    return t;
}

void vfs_entry_free(t_fsentry *entry, int children_only) {
    //logmsg(LOG_DEBUG, "-- VFS_ENTRY (%s, children_only=%d) addr=[%p]", entry->fname, children_only, entry);

    for (int i = 0; i < entry->count; i++)
        vfs_entry_free(entry->children[i], 0);
        
    entry->count = 0;
    if (!children_only) {
        if (entry->children != NULL)
            free(entry->children);
        entry->allocated = 0;
        free(entry->fname);
        free(entry);
    }
}

void vfs_dump(t_fsentry *entry, int depth) {
    for (int i = 0; i < depth*2; i++)
        printf("..");
    printf("%s (typ=%c, cnt=%d, cap=%d, alc=%d, addr=%p)\n", 
        entry->fname, entry->ftype, entry->count, entry->capacity, entry->allocated, (void*) entry);

    if (entry->children != NULL)
        for (int i = 0; i < entry->count; i++)
            vfs_dump(entry->children[i], depth+1);
}

void vfs_entry_dump(const char *msg, t_fsentry *entry) {
    logmsg(LOG_DEBUG, "%s: %c [%s] (size=%d, capacity=%d)", 
        msg, entry->ftype, entry->fname, entry->count, entry->capacity);
}

void vfs_entry_add(t_fsentry *parent, t_fsentry *child) {
    if (parent->count >= parent->capacity-1) {
        parent->capacity += 100;
        parent->children = realloc(parent->children, parent->capacity * sizeof(t_fsentry*));    
    }
    parent->children[parent->count++] = child;
}
    
t_fsentry* _vfs_search(t_fsentry *entry, const char *fname, int min, int max, int depth) {

    if (max == 0)
        return NULL; // no entries

    int pos = (min == max ? min : min + (max-min)/2);
    if (depth > 1000) {
        logmsg(LOG_ERROR, "vfs_search(), entered recursion more than 1000 times, something probably went wrong!");
        return NULL;
    }
    
    int cmp = strcmp(entry->children[pos]->fname, fname);
    if (cmp == 0)
        return entry->children[pos];

    if (min == max || min == max-1)    
//    if (min == max)
        return NULL;
    
    if (cmp < 0)
        return _vfs_search(entry, fname, pos, max, depth+1);
    else
        return _vfs_search(entry, fname, min, pos, depth+1);
}

// search among children of entry
t_fsentry* vfs_entry_search(t_fsentry *entry, const char *fname) {
    return _vfs_search(entry, fname, 0, entry->count, 0);
}

static int vfs_entry_compare(const void *a, const void *b) {
    t_fsentry *entry_a = *(t_fsentry **) a;
    t_fsentry *entry_b = *(t_fsentry **) b;
    return strcmp(entry_a->fname, entry_b->fname);
}

void vfs_entry_sort(t_fsentry *parent) {
    qsort(parent->children, parent->count, sizeof(t_fsentry*), vfs_entry_compare);
}
