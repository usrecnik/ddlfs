#!/bin/sh
#

if [ -n "${LD_LIBRARY_PATH:-}" ]
then
    export LD_LIBRARY_PATH="/usr/lib/ddlfs/ic:$LD_LIBRARY_PATH"
else
    export LD_LIBRARY_PATH="/usr/lib/ddlfs/ic"
fi

if [ -n "${DDLFS_VALGRIND:-}" ]
then
    valgrind \
        --max-stackframe=4299840 \
        --track-origins=yes --leak-check=full \
        --gen-suppressions=all \
        --suppressions=/vbs/ddlfs/ddlfs-valgrind.supp \
        --show-leak-kinds=definite /usr/lib/ddlfs/ddlfs $@
else
    /usr/lib/ddlfs/ddlfs $@
fi

