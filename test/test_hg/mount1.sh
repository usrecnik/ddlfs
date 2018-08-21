#!/bin/bash

source cfg.sh

if [ -d "$CFG_MOUNT_POINT" ]
then
    rm -rv "$CFG_MOUNT_POINT"
fi

if [ -d "$CFG_CACHE_DIR" ]
then
    rm -rfv "$CFG_CACHE_DIR"
fi

mkdir "$CFG_MOUNT_POINT"
mkdir "$CFG_CACHE_DIR"

ddlfs -f -o\
ro,dbro,filesize=-1,keepcache,temppath=$(pwd)/$CFG_CACHE_DIR,\
username=$CFG_USERNAME,\
password=$CFG_PASSWORD,\
schemas=$CFG_USERNAME,\
loglevel=DEBUG,\
database=dbhost:1521/ddlfs.abakus.si \
"$CFG_MOUNT_POINT"
