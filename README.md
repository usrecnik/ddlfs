# ddlfs
[![GitHub release](https://img.shields.io/github/release/usrecnik/ddlfs.svg)](https://github.com/usrecnik/ddlfs/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/usrecnik/ddlfs/blob/master/LICENSE)

**FUSE filesystem, which represents Oracle Database objects as their DDL stored in `.SQL` files.**

Installation
------------
`apt install ./ddlfs-<ver>.deb`  
`yum localinstall --nogpgcheck ddlfs-<ver>.rpm`

`.rpm` and `.deb` packages were tested on:

* Ubuntu 16.4 and 17.4
* Oracle Enterprise Linux 6.0 and 7.4

Older/other versions might need to compile from source (due to different libc and fuse versions).


Usage
-----
`mount -t fuse -o username=<username>,password=<password>,database=<host>:<port>/<service> ddlfs <mountpoint>`  
or  
`ddlfs -o username=<username>,password=<password>,database=<host>:<port>/<service> <mountpoint>`

In the latter, you can optionally use `-f` flag to run filesystem in foreground (especially useful with `loglevel=DEBUG` :-) )

When mounted, following directory tree is available under mountpoint:
 
* `<schema>`
  * `function`
  * `java_source`
  * `package_body`
  * `package_spec`
  * `procedure`
  * `type`
  * `type_body`
  * `view`

Each folder has `.sql` file for each object of specified type (parent folder) in database. For example, folder `<schema>/view/`
has one `.sql` file for every view in this schema.

If you write to those files, filesystem will execute DDL stored in the file on file close (thus, you can edit database objects
via this filesystem). Filesystem keeps a local copy on regular filesystem while the file is open (between open and close calls).

If you delete such `.sql` file, a `DROP <object_type> <object_name>` is issued.

If you create new file (e.g. using `touch` utility), a new object is created from template - e.g. for views, it is 
`create view "<schema>"."object_name" as select * from dual"`.

All files have last modified date set to `last_ddl_time` from `all_objects` view. All files report file size 0 (or whatever
number is set for `filesize=` parameter), except for those that are currently open - those report their actual, correct, file size. 

One special file exists, `ddlfs.log`, which contains log of every executed DDL operation along with possible errors and 
warnings (e.g. indicating on which line syntax error occured). You can `tail -F` this file (just use capital `-F`, because this file only exists in-memory and is rewritten in cyclic manner. `tail` will report that file has been truncated when it shrinks. Such implementation was chosen in order to make sure that there is always a constant amount of memory allocated.)


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

**`filesize=`**`0`  
All `.sql` files report file size as specified by this parameter - unless if file is currently open; correct file size 
is always returned for currently open files. Usign default value `0` (or not specifying this parameter) should be OK for 
most cases, however, some applications refuse to read files with zero length and only read files up to returned file size. 
If you use such application with `ddlfs` specify this parameter to be greater than any database object (`10485760`, this 
is 10mb, should be enough in most cases).

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

Version Control
---------------
You can use this filesystem with version control software such as Git or Mercurial. So far I've tested:

* Mercurial (`hg`) seems to work without any issues (just don't specify `filesize` mount option).
* Git won't work properly in current release, because it expects filesystem to report correct filesizes. I'm currently 
working on this - it should be supported in the next release. However, providing correct filesizes means 
reading DDL of specific object even when only its file *attributes* (not contents) are requested. Thus, I expect 
slightly worse performance in this optional mode.
* Subversion won't work because it wants to create `.svn` subfolder in *every* folder. Problem is that ddlfs only 
supports storing of DDL in `.sql` files. (Git and Mercurial only require one folder bellow mountpoint and that's all)


