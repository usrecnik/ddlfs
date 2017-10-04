
**ddlfs** - FUSE filesystem which represents Oracle Database objects as their DDL stored in .sql files.


Installation
------------
`apt install ./ddlfs-<ver>.deb`  
`yum localinstall --nogpgcheck ddlfs-<ver>.rpm`

`.rpm` and `.deb' packages were tested on:

* Ubuntu versions 16.4 and 17.4
* Oracle Enterprise Linux 6.0 and 7.4

Older/other versions might need to compile from source (due to different libc and fuse versions).


Usage
-----
`mount -t fuse -o username=<username>,password=<password>,database=<host>:<port>/<service> ddlfs <mountpoint>`
or
`ddlfs -o username=<username>,password=<password>,database=<host>:<port>/<service> <mountpoint>`

In the latter, you can optionally use `-f` flag to run filesystem in foreground (especially useful with `loglevel=DEBUG` :-) )


Mount Options
-------------

This section describes mount options specific to
ddlfs. Other generic mount options may be used as well; see **mount** and **fuse** for details.


**`database=`**`host:port/service`  
Oracle EasyConnect string, used to connect to database.

**`username=`**`string`  
Username used to connect to database specified by `database` parameter.

**`password=`**`string`  
Password used to connect to database specified by `database` parameter.

**`schemas=`**`string`  
Schema or list of schemas, separated by `:`. Those are schemas of which objects are "exported" as `.sql` files. You may specify (multiple) partial schema name(s) using `%`
sign, e.g.: `APP_%:BLA_%`, which would match all schemas with names starting with either `APP_` or `BLA_`. It defaults to value specified by `username=` parameter.

**`lowercase`**  
Convert all filenames to lowercase, ignoring the original case as stored in database. You can only use this option if you are sure that all object names have distinct file names when converted to lowercase.

**`nolowercase`**  
this is the default (inverse option of `lowercase`) - filenames reflect exact 
object/schema names as stored in database. 

**`loglevel=`**`[DEBUG|INFO|ERROR]`  
Defines verbosity used for stdout messages. Default value is `INFO`.

**`temppath=`**`/tmp`  
Where to store temporary files - offline copies of DDL statements while their files are open. 
`/tmp` location is used by default. All files created by **ddlfs** have names prefixed by `ddlfs-<PID>` in this folder.


Tips for VIM
------------
If you are using VIM editor to edit files mounted with this filesystem, then you will wan to put following to your `.vimrc`.
This is because you can only store `.sql` files containing valid DDL on this filesystem (`~backup` and `.swp` files are not .sql files)

```
" The first line removes the current directory from the backup directory list (to keep the ~ backups out of your
" working directories). The second tells Vim to attempt to save backups to ~/tmp, or to /tmp if that's not possible. "
set backupdir-=.
set backupdir^=~/.vim/tmp,/tmp
```

```
" For Unix and Win32, if a directory ends in two path separators "//" or "\\", the swap file name will be built from
" the complete path to the file with all path separators substituted to percent '%' signs. This will ensure file name
" uniqueness in the preserve directory.
set directory=~/.vim/swapfiles//
```

