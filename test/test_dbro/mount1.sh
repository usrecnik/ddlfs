#!/bin/bash

source cfg.sh

if [ ! -d "$CFG_MOUNT_POINT" ]
then
    mkdir "$CFG_MOUNT_POINT"
fi

if [ -d "$CFG_CACHE_DIR" ]
then
    rm -rfv "$CFG_CACHE_DIR"
fi

mkdir "$CFG_CACHE_DIR"

sqlplus $CFG_USERNAME/$CFG_PASSWORD@dbhost:1521/ddlfs.abakus.si << eof
    create or replace view $CFG_USERNAME.VIEW_DBRO_VW
        as select * from dual;
    create or replace package $CFG_USERNAME.PACKAGE_X as
        procedure tmp;
    end;
    /
    create or replace package body $CFG_USERNAME.PACKAGE_X as
        procedure tmp as
        begin
            null;
        end;
    end;
    /
eof

ddlfs -f -o\
ro,dbro,filesize=-1,keepcache,temppath=$(pwd)/$CFG_CACHE_DIR,\
username=$CFG_USERNAME,\
password=$CFG_PASSWORD,\
schemas=$CFG_USERNAME,\
loglevel=DEBUG,\
database=dbhost:1521/ddlfs.abakus.si \
"$CFG_MOUNT_POINT"
