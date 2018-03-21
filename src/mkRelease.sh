#!/bin/bash -eu
#

PKG_NAME='ddlfs'
PKG_VERS="$(cat main.c  | grep '#define DDLFS_VERSION' | awk '{print $3}' | tr -d '"' | tr '-' '.')"
PKG_ARCH='amd64'
PKG_MAIN='"Urh Srecnik" <urh.srecnik@abakus.si>'
PKG_DESC='Filesystem which represents Oracle Database objects as their DDL stored in .sql files.'
PKG_FULL_NAME="${PKG_NAME}-${PKG_VERS}"


function proc_copy() {
    mkdir -p ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/
    mkdir -p ../target/${PKG_FULL_NAME}/usr/bin
    mkdir -p ../target/${PKG_FULL_NAME}/usr/share/man/man1
    cp wrapper.sh ../target/${PKG_FULL_NAME}/usr/bin/ddlfs
    cp ../target/ddlfs ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ddlfs
    chmod a+x ../target/${PKG_FULL_NAME}/usr/bin/ddlfs
    chmod a+x ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ddlfs
    cp ../docs/ddlfs.man ../target/${PKG_FULL_NAME}/usr/share/man/man1/ddlfs.1
    unzip ../../instantclient_12_2/instantclient-basic-linux.x64-12.2.0.1.0.zip -d ../target/${PKG_FULL_NAME}/usr/lib/ddlfs
    mv -v ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/instantclient_12_2 ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
}

function proc_deb() {
    readonly l_cf="../target/${PKG_FULL_NAME}/DEBIAN/control"
    readonly l_cp="../target/${PKG_FULL_NAME}/DEBIAN/copyright"
    mkdir -p "$(dirname "$l_cf")"

    > "$l_cf"   
    echo "Package: $PKG_NAME"      >> "$l_cf"
    echo "Version: $PKG_VERS"      >> "$l_cf"
    echo "Architecture: $PKG_ARCH" >> "$l_cf"
    echo "Maintainer: $PKG_MAIN"   >> "$l_cf"
    echo "Depends: fuse, libaio1"  >> "$l_cf"
    echo "Description: $PKG_DESC"  >> "$l_cf"
    echo ' n/a'                    >> "$l_cf" # extended description

    > "$l_cp"
    echo "Files: *"                 >> "$l_cp"
    echo "Copyright: $PKG_MAIN"     >> "$l_cp"
    echo "License: MIT"             >> "$l_cp"
    
    chown -R root:root ../target/*
    dpkg-deb -b ../target/$PKG_FULL_NAME

    rm -r "$(dirname "$l_cf")"
}

function proc_rpm() {
    readonly l_spec="../target/${PKG_FULL_NAME}.spec"
    
    > "$l_spec"
    echo "Name: $PKG_NAME"              >> "$l_spec"
    echo "Version: $PKG_VERS"           >> "$l_spec"
    echo "Release: 1"                   >> "$l_spec"
    echo "Summary: $PKG_DESC"           >> "$l_spec"
    echo "License: MIT"                 >> "$l_spec"
    echo "Group: Unknown"               >> "$l_spec"
    echo "AutoReqProv: no"              >> "$l_spec"
    echo "Requires: libaio, fuse, fuse-libs" >> "$l_spec"
    echo '%description'                 >> "$l_spec"
    echo 'n/a'                          >> "$l_spec"
    echo '%files'                       >> "$l_spec"
    echo '%dir "/usr/lib/ddlfs/"'       >> "$l_spec"
    echo '%dir "/usr/lib/ddlfs/ic/"'    >> "$l_spec" 
    
    pushd "../target/${PKG_FULL_NAME}/"
    find usr/ -type f | \
    while read l_line
    do
        echo "\"/${l_line}\"" >> "../${PKG_FULL_NAME}.spec"
    done
    popd
    
    pushd "../target/"
    rpmbuild -bb \
        --buildroot="$(pwd)/${PKG_FULL_NAME}/" \
        --define "_rpmdir $(pwd)" \
        ${PKG_FULL_NAME}.spec
    mv x86_64/*.rpm ./
    rm *.spec
    rm -r x86_64 
    mv *.rpm ${PKG_FULL_NAME}.rpm
    popd
}

###
# main()
###

echo "Building ${PKG_NAME}-${PKG_VERS}"
echo "--------------------------------"

proc_copy
proc_deb
proc_rpm
