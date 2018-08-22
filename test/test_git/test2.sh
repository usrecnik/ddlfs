#!/bin/bash
#

source ./cfg.sh
git add -A mnt/
git commit -m "test commit"

fusermount -u mnt/
