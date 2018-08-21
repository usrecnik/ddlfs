#!/bin/bash
#

source cfg.sh

# this will create a cache of every object in database:
find ./ &> /dev/null

fusermount -u mnt/

sqlplus -S $CFG_USERNAME/$CFG_PASSWORD@dbhost:1521/ddlfs.abakus.si << eof
    drop view $CFG_USERNAME.VIEW_DBRO_VW;
eof

if [ ! -f cache/*/ddlfs-DDLFS_TESTCASE.VIEW.VIEW_DBRO_VW.SQL.tmp ]
then
    echo "error: cache file was not created during find..."
fi

if [ ! -f cache/*/ddlfs-DDLFS_TESTCASE.VIEW.VIEW_DBRO_VW.SQL.dfs ]
then
    echo "error: meta file was not created during find... "
fi
