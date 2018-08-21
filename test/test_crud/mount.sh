#!/bin/bash -eu
#

source cfg.sh

if [ ! -d "$CFG_MOUNT_POINT" ]
then
    mkdir "$CFG_MOUNT_POINT"
fi

ddlfs -f -o\
username=$CFG_USERNAME,\
password=$CFG_PASSWORD,\
schemas=%,\
loglevel=DEBUG,\
database=$CFG_DATABASE \
"$CFG_MOUNT_POINT" \
# &> testcase.log &
