struct deflist {
    char *definition; 
    struct deflist *next;
};

struct tabledef {
    char exists; // 'Y' or 'N'
    char temporary; // 'Y' or 'N'
    char *last_ddl_time;
    struct deflist *columns;
    struct deflist *constraints;
    struct deflist *indexes;
};

int qry_object_all_tables(const char *schema,
                          const char *table,
                          const char *fname,
                          time_t *last_ddl_time);
