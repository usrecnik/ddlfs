struct columndef {
    char *definition; 
    struct columndef *next;
};

struct tabledef {
    char temporary; // 'Y' or 'N'
    struct columndef *columns;
};

int qry_object_all_tables(const char *schema,
                          const char *table,
                          const char *fname,
                          time_t *last_ddl_time);

