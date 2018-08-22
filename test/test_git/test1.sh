#!/bin/bash
#

source cfg.sh

echo "sqlplus -S $CFG_USERNAME/$CFG_PASSWORD@$CFG_DATABASE"
sqlplus -S $CFG_USERNAME/$CFG_PASSWORD@$CFG_DATABASE << eof
    create or replace force view "$CFG_USERNAME"."DDLFS_REMOVED_VW" as select /* R */ * from dual;
    create or replace force view "$CFG_USERNAME"."DDLFS_CHANGED_VW" as select /* C */ * from dual;
    drop view "$CFG_USERNAME"."DDLFS_ADDED_VW";
eof

# cleanup
rm -rf .git*

git init

# gitrc?

# .gitignore
echo "cache/"           >> ".gitignore"

git add mnt/

git commit -m "initial"

fusermount -u mnt/

git reset --hard

sqlplus -S $CFG_USERNAME/$CFG_PASSWORD@$CFG_DATABASE << eof
    drop view "$CFG_USERNAME"."DDLFS_REMOVED_VW";
    create or replace force view "$CFG_USERNAME"."DDLFS_CHANGED_VW" as select /* C2 */ * from dual;
    create or replace force view "$CFG_USERNAME"."DDLFS_ADDED_VW" as select /* A */ * from dual;
eof
