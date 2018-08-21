#!/bin/bash
#

source ./cfg.sh
hg addremove -s 0 mnt/
hg commit -m "test commit"

fusermount -u mnt/
