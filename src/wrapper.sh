#!/bin/sh
#

if [ -n "${LD_LIBRARY_PATH:-}" ]
then
    export LD_LIBRARY_PATH="/usr/lib/ddlfs/ic:$LD_LIBRARY_PATH"
else
    export LD_LIBRARY_PATH="/usr/lib/ddlfs/ic"
fi

/usr/lib/ddlfs/ddlfs $@

