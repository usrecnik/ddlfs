#!/bin/bash -eu
#
# prereq:
#   cp instantclient-basic-linux.x64-23.6.0.24.10.zip ./instantclient_23_6/  # (basic.zip is expected inside this folder)
#

PKG_NAME='ddlfs'
PKG_VERS="$(cat main.c  | grep '#define DDLFS_VERSION' | awk '{print $3}' | tr -d '"' | tr '-' '.')"
PKG_ARCH='amd64'
PKG_MAIN='"Urh Srecnik" <urh.srecnik@abakus.si>'
PKG_DESC='Filesystem which represents Oracle Database objects as their DDL stored in .sql files.'
PKG_FULL_NAME="${PKG_NAME}-${PKG_VERS}"
INSTANT_CLIENT_PATH="$(cat Makefile  | grep '^export LD_LIBRARY_PATH' | cut -d'=' -f2)"
PKG_TYPE="${1:-ALL}"

function proc_copy() {
    rm -rf ../target/${PKG_FULL_NAME}
    mkdir -p ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/
    mkdir -p ../target/${PKG_FULL_NAME}/usr/bin
    mkdir -p ../target/${PKG_FULL_NAME}/usr/share/man/man1
    cp wrapper.sh ../target/${PKG_FULL_NAME}/usr/bin/ddlfs
    cp ../target/ddlfs ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ddlfs
    cp valgrind.supp ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/valgrind.supp
    chmod a+x     ../target/${PKG_FULL_NAME}/usr/bin/ddlfs
    chmod a+x     ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ddlfs
    chmod a+r,a-w ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/valgrind.supp
    cp ../docs/ddlfs.man ../target/${PKG_FULL_NAME}/usr/share/man/man1/ddlfs.1
    unzip ${INSTANT_CLIENT_PATH}/instantclient-basic-linux.x64-*.zip -d ../target/${PKG_FULL_NAME}/usr/lib/ddlfs
    mv -v ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/instantclient_* ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
}

# P1=ubuntu|debian
function proc_deb() {
    echo ' '
    echo 'Building .deb'
    echo '-------------'
    echo ' '    

    l_cf="../target/${PKG_FULL_NAME}/DEBIAN/control"
    l_cp="../target/${PKG_FULL_NAME}/DEBIAN/copyright"
    l_pi="../target/${PKG_FULL_NAME}/DEBIAN/postinst"
    mkdir -p "$(dirname "$l_cf")"

    case "$1" in
        ubuntu24)
            l_deps='fuse3, libaio1t64'
            ;;

        debian)
            l_deps='fuse3, libaio1'
            ;;
    esac

    > "$l_cf"   
    echo "Package: $PKG_NAME"      >> "$l_cf"
    echo "Version: $PKG_VERS"      >> "$l_cf"
    echo "Architecture: $PKG_ARCH" >> "$l_cf"
    echo "Maintainer: $PKG_MAIN"   >> "$l_cf"
    echo "Depends: $l_deps"        >> "$l_cf"
    echo "Description: $PKG_DESC"  >> "$l_cf"
    echo ' n/a'                    >> "$l_cf" # extended description

    > "$l_cp"
    echo "Files: *"                 >> "$l_cp"
    echo "Copyright: $PKG_MAIN"     >> "$l_cp"
    echo "License: MIT"             >> "$l_cp"

    > "$l_pi"
    echo "#!/bin/bash" >> "$l_pi"
    echo "if [ ! -e /usr/lib/x86_64-linux-gnu/libaio.so.1 ]" >> "$l_pi"
    echo "then" >> "$l_pi"
    # ubuntu has renamed this package, so we need to "fix" it:
    echo "  ln -s /usr/lib/x86_64-linux-gnu/libaio.so.1t64 /usr/lib/x86_64-linux-gnu/libaio.so.1" >> "$l_pi"
    echo "fi" >> "$l_pi"
    
    chmod 775 "$l_pi"

    chown -R root:root ../target/*
    dpkg-deb -b ../target/$PKG_FULL_NAME
    mv ../target/${PKG_FULL_NAME}.deb ../target/${PKG_FULL_NAME}-${1}.deb

    rm -r "$(dirname "$l_cf")"
}

function proc_rpm() {
    echo ' '
    echo 'Building .rpm'
    echo '-------------'
    echo ' '
    
    readonly l_spec="../target/${PKG_FULL_NAME}.spec"
    
    > "$l_spec"
    echo "Name: $PKG_NAME"              >> "$l_spec"
    echo "Version: $PKG_VERS"           >> "$l_spec"
    echo "Release: 1"                   >> "$l_spec"
    echo "Summary: $PKG_DESC"           >> "$l_spec"
    echo "License: MIT"                 >> "$l_spec"
    echo "Group: Unknown"               >> "$l_spec"
    echo "AutoReqProv: no"              >> "$l_spec"
    echo "Requires: libaio, fuse3"      >> "$l_spec"
    echo '%description'                 >> "$l_spec"
    echo 'n/a'                          >> "$l_spec"
    echo '%files'                       >> "$l_spec"
    echo '%dir "/usr/lib/ddlfs/"'       >> "$l_spec"
    echo '%dir "/usr/lib/ddlfs/ic/"'    >> "$l_spec" 
    
    pushd "../target/${PKG_FULL_NAME}/"
    find usr/ \( -type f -o -type l \) | \
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

echo "Building ${PKG_NAME}-${PKG_VERS} (pkg_type=$PKG_TYPE)"
echo "--------------------------------"

proc_copy

case "$PKG_TYPE" in
    "ALL")
        proc_deb 'ubuntu24'
        proc_deb 'debian'
        proc_rpm
        ;;

    "DEB")
        proc_deb 'ubuntu24'
        proc_deb 'debian'
        ;;

    "RPM")
        proc_rpm
        ;;

    *)
esac

