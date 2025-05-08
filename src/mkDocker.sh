#!/bin/bash -eu
#

DDLFS_HOST_DIR="$(realpath "$(pwd)/../../")"
echo "$DDLFS_HOST_DIR"


CFG_COMMAND="${1:-build}"

# P1: docker image (e.g. ubuntu:24.04)
# P2: distro tag for package (parameter to mkRelease.sh)
function proc_build_deb() {
    docker run --rm -i -v $DDLFS_HOST_DIR:/srv/workspace/ ${1} << eof
        apt update
        apt install unzip
    
        cd /srv/workspace/ddlfs/src
        ./mkRelease.sh DEB ${2}
eof
}

function proc_build_rpm_dnf() {
    docker run --rm -i -v $DDLFS_HOST_DIR:/srv/workspace/ ${1} << eof
        dnf install unzip rpm-build
    
        cd /srv/workspace/ddlfs/src
        ./mkRelease.sh RPM ${2}
eof
}

function proc_build_rpm_yum() {
    docker run --rm -i -v $DDLFS_HOST_DIR:/srv/workspace/ ${1} << eof
        yum install -y unzip rpm-build
    
        cd /srv/workspace/ddlfs/src
        ./mkRelease.sh RPM ${2}
eof
}




if [ "$CFG_COMMAND" == 'build' ]
then
    proc_build_deb ubuntu:24.04 ubuntu24
    proc_build_deb ubuntu:22.04 ubuntu22
    proc_build_deb debian:12 debian12
    proc_build_deb debian:11 debian11

    proc_build_rpm_yum oraclelinux:7 el7
    proc_build_rpm_yum oraclelinux:8 el8
    proc_build_rpm_dnf oraclelinux:9 el9
elif [ "$CFG_COMMAND" == 'test' ]
then
    docker run --rm -it -v $DDLFS_HOST_DIR:/srv/workspace $2
else
    echo 'Unknown command.'
fi

