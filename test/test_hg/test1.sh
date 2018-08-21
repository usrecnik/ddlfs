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
rm -rf .hg*

hg init

# .hg/hgrc
echo "[ui]"                                     >  ".hg/hgrc"
echo "username = ddlfs-testcase"                >> ".hg/hgrc"
echo ""                                         >> ".hg/hgrc"
echo "[extensions]"                             >> ".hg/hgrc"
echo "TimestampMod = $(pwd)/TimestampMod.py"    >> ".hg/hgrc"

# .hgignore
echo "syntax: glob"     >  ".hgignore"
echo "cache/"           >> ".hgignore"

hg addremove -s 0 mnt/

hg commit -m "initial commit"

fusermount -u mnt/

hg update -C

sqlplus -S $CFG_USERNAME/$CFG_PASSWORD@$CFG_DATABASE << eof
    drop view "$CFG_USERNAME"."DDLFS_REMOVED_VW";
    create or replace force view "$CFG_USERNAME"."DDLFS_CHANGED_VW" as select /* C2 */ * from dual;
    create or replace force view "$CFG_USERNAME"."DDLFS_ADDED_VW" as select /* A */ * from dual;
eof
