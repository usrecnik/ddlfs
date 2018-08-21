#!/bin/bash -eu
#

export CFG_USERNAME="DDLFS_TESTCASE"
export CFG_PASSWORD="testcase"
export CFG_MOUNT_POINT="./mnt/"
export CFG_CACHE_DIR="./cache/"

INSTANT_CLIENT_PATH="$(cat ../../src/Makefile  | grep '^export LD_LIBRARY_PATH' | cut -d'=' -f2)"
export PATH="../$INSTANT_CLIENT_PATH:$PATH"
export LD_LIBRARY_PATH="../$INSTANT_CLIENT_PATH"
