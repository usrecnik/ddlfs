#!/bin/bash -eu
#

DDLFS_HOST_DIR="$(realpath "$(pwd)/../../")"

echo "$DDLFS_HOST_DIR"

docker run --rm -i -v $DDLFS_HOST_DIR:/srv/workspace/ ubuntu:24.04 << eof
    apt update
    apt install unzip
    
    cd /srv/workspace/ddlfs/src
    ./mkRelease.sh DEB
eof

docker run --rm -i -v $DDLFS_HOST_DIR:/srv/workspace/ oraclelinux:9 << eof
    dnf install unzip rpm-build
    
    cd /srv/workspace/ddlfs/src
    ./mkRelease.sh RPM
eof

