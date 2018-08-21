#!/bin/bash -eu
#

export CFG_MOUNT_POINT="mnt/"

function proc_compare() {
    local readonly l_cmp="$(diff -b "$1" "$2" | wc -l)"
    if [ "$l_cmp" == 0 ]
    then
        return;
    fi

    echo "Files [$1] and [$2] differ!"
    diff -b "$1" "$2"
    exit 1
}

function test_create() {
    local readonly l_type="$1"
    local readonly l_dbf="$2"
    local readonly l_rpf="$3"

    echo "creating $l_type/$l_dbf"
    > "$CFG_MOUNT_POINT/DDLFS_TESTCASE/$l_type/$l_dbf"
    proc_compare "repository/$l_rpf" "$CFG_MOUNT_POINT/DDLFS_TESTCASE/$l_type/$l_dbf"
}

function test_copy() {
    local readonly l_type="$1"
    local readonly l_dbf="$2"
    local readonly l_rpf="$3"

    echo "copying $l_type/$l_dbf"
    cp repository/$l_rpf "$CFG_MOUNT_POINT/DDLFS_TESTCASE/$l_type/$l_dbf"
    proc_compare "repository/$l_rpf" "$CFG_MOUNT_POINT/DDLFS_TESTCASE/$l_type/$l_dbf"
}

function test_delete() {
    local readonly l_type="$1"
    local readonly l_dbf="$2"

    echo "deleting $l_type/$l_dbf";
    rm "$CFG_MOUNT_POINT/DDLFS_TESTCASE/$l_type/$l_dbf"
    if [ -f "$CFG_MOUNT_POINT/DDLFS_TESTCASE/$l_type/$l_dbf" ]
    then
        echo "error: deleted file still exists...";
    fi
    set +e
    ls "$CFG_MOUNT_POINT/" &> /dev/null
    stat "$CFG_MOUNT_POINT/ANONYMOUS/VIEW/NOT_EXISTS.SQL" &> /dev/null
    set -e

    if [ -f "$CFG_MOUNT_POINT/DDLFS_TESTCASE/$l_type/$l_dbf" ]
    then
        echo "error: deleted file re-appeared...";
    fi
}

# Test creating "empty" files:
 test_create "PACKAGE_SPEC"  "PACKAGE1.SQL"      "PACKAGE_SPEC1.SQL"
 test_create "PACKAGE_BODY"  "PACKAGE1.SQL"      "PACKAGE_BODY1.SQL"
 test_create "FUNCTION"      "FUNCTION1.SQL"     "FUNCTION1.SQL"
 test_create "PROCEDURE"     "PROCEDURE1.SQL"    "PROCEDURE1.SQL"
 test_create "VIEW"          "VIEW1.SQL"         "VIEW1.SQL"
 test_create "JAVA_SOURCE"   "JAVA_SOURCE1.JAVA" "JAVA_SOURCE1.JAVA"
 test_create "TYPE"          "TYPE1.SQL"         "TYPE1.SQL"
 test_create "TYPE_BODY"     "TYPE1.SQL"         "TYPE_BODY1.SQL"
#test_create "TRIGGER"       "TRIGGER1.SQL"      "TRIGGER1.SQL"

# Test copying new files:
 test_copy "PACKAGE_SPEC"  "PACKAGE2.SQL"      "PACKAGE_SPEC2.SQL"
 test_copy "PACKAGE_BODY"  "PACKAGE2.SQL"      "PACKAGE_BODY2.SQL"
 test_copy "FUNCTION"      "FUNCTION2.SQL"     "FUNCTION2.SQL"
 test_copy "PROCEDURE"     "PROCEDURE2.SQL"    "PROCEDURE2.SQL"
 test_copy "VIEW"          "VIEW2.SQL"         "VIEW2.SQL"
 test_copy "JAVA_SOURCE"   "JAVA_SOURCE2.JAVA" "JAVA_SOURCE2.JAVA"
 test_copy "TYPE"          "TYPE2.SQL"         "TYPE2.SQL"
 test_copy "TYPE_BODY"     "TYPE2.SQL"         "TYPE_BODY2.SQL"
#test_copy "TRIGGER"       "TRIGGER2.SQL"      "TRIGGER2.SQL"

# Test deleting existing files
 test_delete "PACKAGE_BODY"  "PACKAGE1.SQL"
 test_delete "PACKAGE_SPEC"  "PACKAGE1.SQL"
 test_delete "FUNCTION"      "FUNCTION1.SQL"
 test_delete "PROCEDURE"     "PROCEDURE1.SQL"
 test_delete "VIEW"          "VIEW1.SQL"
 test_delete "JAVA_SOURCE"   "JAVA_SOURCE1.JAVA"
 test_delete "TYPE_BODY"     "TYPE1.SQL"
 test_delete "TYPE"          "TYPE1.SQL"

# @todo - test git & hg

#fusermount -u "$CFG_MOUNT_POINT"
