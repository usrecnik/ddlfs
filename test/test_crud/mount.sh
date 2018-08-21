#!/bin/bash -eu
#

export CFG_USERNAME="DDLFS_TESTCASE"
export CFG_PASSWORD="testcase"
export CFG_MOUNT_POINT="./mnt/"

if [ ! -d "$CFG_MOUNT_POINT" ]
then
    mkdir "$CFG_MOUNT_POINT"
fi

ddlfs -f -o\
username=$CFG_USERNAME,\
password=$CFG_PASSWORD,\
schemas=%,\
loglevel=DEBUG,\
database=dbhost:1521/ddlfs.abakus.si \
"$CFG_MOUNT_POINT" \
# &> testcase.log &
