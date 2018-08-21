#!/bin/bash
#

export CFG_USERNAME="DDLFS_TESTCASE"
export CFG_PASSWORD="testcase"
export CFG_DATABASE="dbhost:1521/ddlfs.abakus.si"
export CFG_MOUNT_POINT="./mnt/"

INSTANT_CLIENT_PATH="$(cat ../../src/Makefile  | grep '^export LD_LIBRARY_PATH' | cut -d'=' -f2)"
export PATH="../$INSTANT_CLIENT_PATH:$PATH"
export LD_LIBRARY_PATH="../$INSTANT_CLIENT_PATH"
