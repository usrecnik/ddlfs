#!/bin/bash
#

. cfg.sh

if [ -f cache/*/ddlfs-DDLFS_TESTCASE.VIEW.VIEW_DBRO_VW.SQL.tmp ]
then
    echo "error: cache file was not removed during mount..."
fi

if [ -f cache/*/ddlfs-DDLFS_TESTCASE.VIEW.VIEW_DBRO_VW.SQL.dfs ]
then
    echo "error: meta file was not removed during find..."
fi

fusermount -u $CFG_MOUNT_POINT
