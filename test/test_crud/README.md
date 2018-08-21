# Testing ddlfs

1. Create dedicated schema for this purpose using `setup.sql`.
2. Mount `ddlfs` using provided `mount.sh` in one window.
3. Run test cases using `./test.h` in the other window.
4. when finished, unmount by `fusermount -u ./mnt/`

Note that you may use `export DDLFS_VALGRIND='y'` before mounting to report
memory leaks if there are any.
