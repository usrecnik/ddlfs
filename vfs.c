#include <stdlib.h>
#include <string.h>

#include "vfs.h"
#include "logging.h"

t_fsentry* vfs_entry_create(const char type, 
                                  const char *fname, 
                               time_t created, 
                                 time_t modified) {

    t_fsentry *t = malloc(sizeof(t_fsentry));
    t->ftype = type;
    t->fname = strdup(fname);
    t->created = created;
    t->modified = modified;
    t->count = 0;
    if (type == 'D') {
        t->capacity = 100;
        t->children = malloc(sizeof(t_fsentry*) * t->capacity);
        
        if (t->children == NULL)
            logmsg(LOG_ERROR, "Unable to malloc children for %s", t->fname);
    } else {
        t->capacity = 0;
        t->children = NULL;
    }
    t->allocated = 1;
    return t;
}

void vfs_entry_free(t_fsentry *entry, int children_only) {
    for (int i = 0; i < entry->count; i++)
        vfs_entry_free(entry->children[i], 0);
    
    entry->count = 0;
    if (!children_only) {
        if (entry->children != NULL) {
            //logmsg(LOG_DEBUG, "*** FREE CHILDREN ARR OF (%s, %d, %d, %d)", entry->fname, entry->capacity, entry->allocated,
            //    entry->count);
            free(entry->children);
        }
        
        free(entry->fname);
        free(entry);
        entry->allocated = 0;
    }
}

void vfs_dump(t_fsentry *entry, int depth) {
    for (int i = 0; i < depth*2; i++)
        printf("..");
    printf("%s (typ=%c, cnt=%d, cap=%d, alc=%d)\n", 
        entry->fname, entry->ftype, entry->count, entry->capacity, entry->allocated);

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
    //logmsg(LOG_DEBUG, "Min=[%d], Max=[%d], Pos=[%d], Depth=[%d], value=[%s]", min, max, pos, depth,
    //    entry->children[pos]->fname);
    if (depth > 10)
        return NULL;
    
    int cmp = strcmp(entry->children[pos]->fname, fname);
    if (cmp == 0) {
    //    logmsg(LOG_DEBUG, ".. found!");
        return entry->children[pos];
    }
    if (min == max) {
    //    logmsg(LOG_DEBUG, ".. returning NULL");
        return NULL;
    }

    if (cmp < 0)
        return _vfs_search(entry, fname, pos, max, depth+1);
    else
        return _vfs_search(entry, fname, min, pos, depth+1);
}

// search among childrent of entry
t_fsentry* vfs_entry_search(t_fsentry *entry, const char *fname) {
    return _vfs_search(entry, fname, 0, entry->count, 0);
}
