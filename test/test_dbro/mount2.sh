#!/bin/bash -eu
#

. cfg.sh

if [ ! -d "$CFG_MOUNT_POINT" ]
then
    mkdir "$CFG_MOUNT_POINT"
fi

ddlfs -f -o\
ro,dbro,filesize=-1,keepcache,temppath=$(pwd)/$CFG_CACHE_DIR,\
username=$CFG_USERNAME,\
password=$CFG_PASSWORD,\
schemas=$CFG_USERNAME,\
loglevel=DEBUG,\
database=dbhost:1521/ddlfs.abakus.si \
"$CFG_MOUNT_POINT"
