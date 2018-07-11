struct deflist {
    char *definition; 
    struct deflist *next;
};

struct tabledef {
    char temporary; // 'Y' or 'N'
    struct deflist *columns;
    struct deflist *constraints;
    struct deflist *indexes;
};

int qry_object_all_tables(const char *schema,
                          const char *table,
                          const char *fname,
                          time_t *last_ddl_time);
