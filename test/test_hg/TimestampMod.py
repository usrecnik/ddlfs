# begin Extension Help Text:
'''Automatically store and retrieve file modification times.'''
# end help text
#=============================================================================
# TimestampMod.py - Automatically save and restore file modification times
File_Version = '0.3.1'  # Version number definition
# --> !!BETA RELEASE!!! <--
# Copyright 2011-2017 Nathan Durnan <nedmech@gmail.com>
#
# Based on timestamp extension by Friedrich Kastner-Masilko <face@snoopie.at>
# This extension differs from the original in that the original only managed
#   the timestamps of files that had been manually added to the tracking file.
# The default behavior of this extension instead manages the timestamps of
#   ALL files under version control AUTOMATICALLY.  The purpose of this is to
#   minimize the fears of those who are new to Version Control concepts and
#   make the process of updating between changesets "feel" more like native
#   OS file copy methods (preserving file modification times).
#
# This software may be used and distributed according to the terms
# of the GNU General Public License, incorporated herein by reference.
#-----------------------------------------------------------------------------
# Summary: This is an extension written for Mercurial/TortoiseHG that allows
#   the original modification times of files under version control to be
#   recorded and restored during commit, update, revert, etc. operations.
#
# Development information:
#   Mercurial Version:  1.9 - 4.3  (from TortoiseHg package)
#   Python Version:     2.6 - 2.7  (from TortoiseHg package)
#   TortoiseHg Version: 2.1 - 4.3
#
# NOTE: forward development is not guaranteed to remain backward-compatible
#   with older Mercurial/Python/TortoiseHg versions. Some changes in Mercurial
#   and TortoiseHg may break the backwards-compatibility of this extension.
#   The latest versions listed in this file are the last known compatible 
#   versions with this extension.
#=============================================================================

testedwith = '3.6 3.6.2 3.7.2 4.0.1 4.1 4.2.2, 4.3.1'
buglink  = 'https://bitbucket.org/nedmech/timestampmod/issues'

#=============================================================================
# Import Modules
#-----------------------------------------------------------------------------
import sys      # required for debugging/exception info.
import os       # required for filesystem access methods.
import os.path  # required for filesystem path methods.
import time     # required for time functions.
import json     # use JSON format for storing timestamp data in file.
import inspect  # required for getting path/name of current code module.
import fnmatch  # required for checking file patterns.
from mercurial import localrepo          # required for creating pseudo-pre-commit hook.
from mercurial import scmutil            # required for "match" object in commit wrapper.
from mercurial import merge as mergemod  # required for merge state (check unresolved).
from mercurial import cmdutil            # required for working with "Revert" command methods.
from mercurial import commands           # required for working with "Revert" command methods.
#_ end of imported modules____________________________________________________

#=============================================================================
# Global Objects
#-----------------------------------------------------------------------------
File_TimestampRecords = '.hgtimestamp'
LastPlaceholder = 'TimestampMod_LastRecord'
TimeStamp_dict = dict()
#_ end of global objects _____________________________________________________


#=============================================================================
# uisetup Callback Configuration
#-----------------------------------------------------------------------------
# NOTES: Called when the extension is first loaded and receives a ui object.
#   This is the FIRST callback executed when initializing this extension.
#=============================================================================
def uisetup(ui):
    '''Initialize UI-Level Callback'''
    ui.debug('* Loading TimestampMod uisetup\n')
    ui.setconfig("hooks", "post-status.TimestampMod", Hook_Post_Status)
    # Hook_Pre_Commit is deprecated in favor of Wrap_Commit method
    #   (Wrap_Commit works with both native Mercurial and TortoiseHg.)
    '''ui.setconfig("hooks", "pre-commit.TimestampMod", Hook_Pre_Commit)'''
    # NOTE: the post-merge, post-resolve, and post-revert hooks are not picked up
    #   by TortoiseHg.  When using this extension with TortoiseHg, these hooks
    #   must be manually added to the "mercurial.ini" configuration file.
    #   Use the following format:
    #     [hooks]
    #     post-merge.TimestampMod = python:{path-to-TimestampMod.py}:Hook_Post_Merge
    #     post-resolve.TimestampMod = python:{path-to-TimestampMod.py}:Hook_Post_Resolve
    #     post-revert.TimestampMod = python:{path-to-TimestampMod.py}:Hook_Post_Revert
    ui.setconfig("hooks", "post-merge.TimestampMod", Hook_Post_Merge)
    ui.setconfig("hooks", "post-resolve.TimestampMod", Hook_Post_Resolve)
    ui.setconfig("hooks", "post-revert.TimestampMod", Hook_Post_Revert)
#_ end of uisetup ____________________________________________________________


#=============================================================================
# extsetup Callback Configuration
#-----------------------------------------------------------------------------
# NOTES: Called after all the extension have been initially loaded.
#   This is the SECOND callback executed when initializing this extension.
#   It can be used to access other extensions that this one may depend on.
#=============================================================================
def extsetup(ui):
    '''Initialize Extension-Level Callback'''
    ui.debug('* Loading TimestampMod extsetup\n')
    pass
#_ end of extsetup ___________________________________________________________


#=============================================================================
# reposetup Callback Configuration
#-----------------------------------------------------------------------------
# NOTES: Called after the main Mercurial repository initialization.
#   This is the LAST callback executed when initializing this extension.
#   It can be used to setup any local state the extension might need.
#=============================================================================
def reposetup(ui, repo):
    '''Initialize Repository-Level Callback'''
    ui.debug('* Loading TimestampMod reposetup\n')
    # Check for external Python library path in configuration
    # NOTE: keep this in reposetup in case user has configured the
    #   extension to be used per-repository instead of globally.
    #   This will ensure the path config can be picked up from
    #   either the global config or the repository config.
    sPythonLibPath = str(ui.config('paths', 'PythonLibPath'))
    if os.path.exists(sPythonLibPath):
        sys.path.append(sPythonLibPath)
    # end of check for external Python path
    ui.setconfig("hooks", "update.TimestampMod", Hook_Update)
    if not hasattr(localrepo.localrepository, "timestamp_origcommit"):
        '''This is a "dirty" method of wrapping the commit event so
        pre-commit actions are executed.  Normal pre-commit hooks and
        extension.wrapcommand() methods failed to work with the version
        of TortoiseHg used for development.  Ideally, this will be
        revised to a cleaner method in the future.'''
        localrepo.localrepository.timestamp_origcommit = \
            localrepo.localrepository.commit
        localrepo.localrepository.commit = Wrap_Commit
#_ end of reposetup __________________________________________________________


#=============================================================================
# Wrap_Commit Function Definition
#-----------------------------------------------------------------------------
# Summary: Intercept the commit event to update the timestamp record file, and
#   make sure the record file gets included in the commit.
# NOTES: This is a "dirty" method of wrapping the commit event so pre-commit
#   actions are executed. Normal pre-commit hooks and extensions.wrapcommand()
#   methods failed to work with the TortoiseHg version used for development.
#   Ideally, this will be revised to a cleaner method in the future.
#=============================================================================
def Wrap_Commit(
                repo,
                text="",
                user=None,
                date=None,
                match=None,
                force=False,
                editor=False,
                extra={}
            ):
    repo.ui.note('______\nTimestampMod|Wrap_Commit accessed!\n')
    # Check for a merge-commit.
    # Don't run timestamp code until merge is complete.
    if (len(repo[None].parents()) > 1):
        repo.ui.status(
                        'TimestampMod|Wrap_Commit aborted',
                        ' - Merge in progress\n'
                    )
    else:
        # Make sure the match object is created.
        if match is None:
            repo.ui.debug('Empty match: Must create!\n')
            # create an empty match object
            localmatch = scmutil.match(repo[None])
        else:
            # Make sure match.files is a list, not a set or other type.
            if not isinstance(match.files(), list):
                # coerce files() to a list object.
                match._files = list(match.files())
            # end of check for list type
            localmatch = match
        # End of check for non-existent match object.
        # don't add timestamp file here, it will be added later.
        timestamp_mod(
                        repo.ui,
                        repo,
                        **dict({
                                'save': True,
                                'restore': None,
                                'match': localmatch.files()
                            })
                    )
        # Check for match conditions.
        if ((match is None) or
            (not match.files())):
            # Similar to test for generic commit in hgext\largefiles\reposetup.py
            repo.ui.debug('Wrap_Commit: no match specified\n')
        elif (File_TimestampRecords not in match.files()):
            # Record file must get added to list if any match is specified.
            match.files().append(File_TimestampRecords)
            repo.ui.debug('Match Files: ', str(match.files()), '\n')
        # end of check match conditions
    # end of check for merge-commit.
    repo.ui.note('TimestampMod|Wrap_Commit finished!\n______\n')
    return repo.timestamp_origcommit(
                                        text,
                                        user,
                                        date,
                                        match,
                                        force,
                                        editor,
                                        extra
                                    )
#_ end of Wrap_Commit ________________________________________________________


#=============================================================================
# Hook Function Definitions
#-----------------------------------------------------------------------------
# Summary: These functions are intended to be triggered by the hooks defined
#   either by Mercurial or in the configuration files.
# NOTES: The pre-** hooks are the only ones that do not function properly
#   under the TortoiseHg GUI.  All the hooks work from command-line.
#=============================================================================
# Hook_Pre_Commit is deprecated in favor of Wrap_Commit method
#   (Wrap_Commit works with both native Mercurial and TortoiseHg.)
'''def Hook_Pre_Commit(repo, **kwargs):
    repo.ui.note('Pre-Commit Hook accessed!\n')
    repo.ui.debug('kwargs = ',kwargs, "\n")
    timestamp_mod(repo.ui, repo, **dict({'save': True, 'restore': None}))
    kwargs['pats'].append(File_TimestampRecords)
'''  # Hook_Pre_Commit is deprecated


def Hook_Post_Status(repo, **kwargs):
    repo.ui.note('______\nTimestampMod|Post-Status Hook accessed!\n')
    myUnresolved = _check_Merge_unresolved(repo)
    if myUnresolved:
        repo.ui.note(
                        'TimestampMod|Post-Status Hook aborted',
                        ' - Unresolved merge detected!\n'
                    )
        return
    # end check for unresolved merge
    timestamp_mod(
                    repo.ui,
                    repo,
                    **dict({
                            'save': None,
                            'restore': None
                        })
                )
    repo.ui.note('TimestampMod|Post-Status Hook finished!\n______\n')


def Hook_Update(repo, **kwargs):
    repo.ui.note('______\nTimestampMod|Hook_Update accessed!\n')
    if (len(repo[None].parents()) > 1):
        repo.ui.note(
                        'TimestampMod|Hook_Update aborted',
                        '- Merge in progress\n'
                    )
    else:
        timestamp_mod(
                        repo.ui,
                        repo,
                        **dict({
                                'save': None,
                                'restore': True
                            })
                    )
    # end of check for merging.
    repo.ui.note('TimestampMod|Hook_Update finished!\n______\n')


def Hook_Post_Merge(repo, **kwargs):
    repo.ui.note('______\nTimestampMod|Post-Merge Hook accessed!\n')
    myUnresolved = _check_Merge_unresolved(repo)
    if myUnresolved:
        repo.ui.note(
                        'TimestampMod|Post-Merge Hook aborted',
                        ' - Unresolved merge detected!\n'
                    )
        return
    # end check for unresolved merge
    myPreview = False  # starting value
    if ('opts' in kwargs):
        if ('preview' in kwargs['opts']):
            myPreview = kwargs['opts']['preview']
        # end of check for 'preview' option.
    # end of check for 'opts' keyword.
    if (not myPreview):
        # only update timestamps if not just a preview.
        timestamp_mod(
                        repo.ui,
                        repo,
                        **dict({
                                'save': None,
                                'restore': True,
                                'postMerge': True
                            })
                    )
    # check for preview option.
    repo.ui.note('TimestampMod|Post-Merge Hook finished!\n______\n')


def Hook_Post_Resolve(repo, **kwargs):
    repo.ui.note('______\nTimestampMod|Post-Resolve Hook accessed!\n')
    myUnresolved = _check_Merge_unresolved(repo)
    if myUnresolved:
        repo.ui.note(
                        'TimestampMod|Post-Resolve Hook aborted',
                        ' - Unresolved merge detected!\n'
                    )
        return
    # end check for unresolved merge
    myResolveAll = False  # starting value
    if ('opts' in kwargs):
        if ('all' in kwargs['opts']):
            myResolveAll = kwargs['opts']['all']
        # end of check for 'all' option.
    # end of check for 'opts' keyword.
    if (myResolveAll or (File_TimestampRecords in str(kwargs['pats']))):
        # Only re-apply timestamps if the timestamp file is being resolved.
        repo.ui.status('Resolved timestamp file - Reapplying timestamps!\n')
        timestamp_mod(
                        repo.ui,
                        repo,
                        **dict({
                                'save': None,
                                'restore': True
                            })
                    )
    # end of check for timestamp file resolve.
    repo.ui.note('TimestampMod|Post-Resolve Hook finished!\n______\n')


def Hook_Post_Revert(repo, **kwargs):
    repo.ui.note('______\nTimestampMod|Post-Revert Hook accessed!\n')
    bDryRun = False  # starting value
    bAll = False  # starting value
    listExclude = list()
    if ('opts' in kwargs):
        bDryRun = kwargs['opts'].get('dry_run', False)
        bAll = kwargs['opts'].get('all', False)
        listExclude = kwargs['opts'].get('exclude', list())
        if kwargs['opts'].get('date'):
            # Don't bother to check for rev-spec.
            # Original command would've failed and not
            # gotten here if both date and rev were spec'd.
            kwargs['opts']['rev'] = \
                cmdutil.finddate(
                        repo.ui,
                        repo,
                        kwargs['opts']['date']
                    )
        # end of check for date specified.
    # end of check for 'opts' keyword.
    # Create a temporary copy of the timestamp file from the source revision.
    commands.cat(
            repo.ui,
            repo,
            repo.wjoin(File_TimestampRecords),
            repo.wjoin(File_TimestampRecords),
            **dict({
                    'rev': kwargs['opts']['rev'],
                    'output': repo.wjoin('%s.revert')
                })
        )
    # Create dictionary entries for items that were reverted.
    if bAll:
        _get_RepoFileList(repo, list(), TimeStamp_dict)
        for myFile in listExclude:
            if myFile in TimeStamp_dict:
                del TimeStamp_dict[myFile]
            # end of check for file in dictionary.
        # loop through exclude filter
    else:  # only specific files have been reverted.
        # Build initial file lists from the repository contents.
        myTemporaryTimeStamp_dict = dict()
        _get_RepoFileList(repo, list(), myTemporaryTimeStamp_dict)
        TimeStamp_dict.clear()
        def getHgPathFromRoot(repo, pat):
            cwd = repo.getcwd()
            if os.path.isabs(cwd):
                cwd = cwd[len(repo.root):]
            pat = os.path.join(cwd, pat)
            if os.name == 'nt':
                pat = pat.replace("\\",'/')
            return pat

        for myFile in kwargs['pats']:
            myFile = getHgPathFromRoot(repo, myFile)
            myFileName = str(myFile).strip()
            if os.path.isfile(repo.wjoin(myFileName)):
                repo.ui.debug('reverting file...\n')
                TimeStamp_dict[myFileName] = -1
            elif os.path.isdir(repo.wjoin(myFileName)):
                repo.ui.debug('reverting directory...\n')
                # Check the potential list of reverted files
                # against the directory path pattern.
                for sFile in sorted(myTemporaryTimeStamp_dict.keys(), key=str.lower):
                    if fnmatch.fnmatch(sFile, (myFileName + '\\*')):
                        TimeStamp_dict[sFile] = -1
                    # end of checking for matching directory path pattern
                # end of loop through potential reverted file list
            # end check for file or directory.
        # end of loop through pattern list
    # end of check for 'all' flag
    # Retrieve existing timestamps from the record file.
    myErr = _read_TimestampJSONRecords(
            repo,
            (repo.wjoin(File_TimestampRecords + '.revert')),
            TimeStamp_dict
        )
    if (not bDryRun):
        if not myErr:
            _restore_Timestamps(repo, TimeStamp_dict, list())
        # end of check for file-read error.
    # end of check for dry-run operation.
    # Delete temporary timestamp file
    try:
        os.unlink(repo.wjoin(File_TimestampRecords + '.revert'))
    except:
        repo.ui.status(
                        'Post-Revert: ',
                        'error deleting temporary timestamp file!\n'
                    )
    # end of deleting temporary timestamp file.
    repo.ui.note('TimestampMod|Post-Revert Hook finished!\n______\n')
#_ end of Hook Functions _____________________________________________________


#=============================================================================
# Command Table Definition
#-----------------------------------------------------------------------------
# (NOTE: Keep this after command definitions.  cmdtable contents
#   must be defined after the commands/functions referenced!)
# (2017-10-23: something changed between initial development and thg 3.6 that 
#   caused the command table definition to be required BEFORE the commands 
#   instead of AFTER them.)
#=============================================================================
cmdtable = {}
command = cmdutil.command(cmdtable)

@command('timestamp_mod',
        [('s', 'save', None, ('save modification times')),
         ('r', 'restore', None, ('restore modification times'))
        ],
        ('hg timestamp_mod [-s | -r]\n' +
            '\n' + inspect.getfile(inspect.currentframe()) +
            '\n  ' + '(Version ' + File_Version + ')'
        )
    )
#_ end of cmdtable ___________________________________________________________


#=============================================================================
# timestamp_mod
#-----------------------------------------------------------------------------
# Summary: save or restore file modification times.
#
#=============================================================================
def timestamp_mod(ui, repo, **kwargs):
    '''Save or restore file modification times.'''
    repo.ui.note('Executing timestamp_mod function\n')
    # Retrieve Repository file list contents.
    myChangedList = list()
    myDroppedList = list()
    myMatchList = list()
    if ('match' in kwargs):
        repo.ui.debug('-----\nmatch: ', str(kwargs['match']), '\n')
        myMatchList = kwargs['match']
    else:
        repo.ui.debug('-----\nmatch argument not specified\n')
    # end of check for match argument'''
    # Build initial file lists from the repository contents.
    myChangedList, myDroppedList = \
        _get_RepoFileList(repo, myMatchList, TimeStamp_dict)
    if ('postMerge' in kwargs):
        del myChangedList[:]
    repo.ui.debug('myChangedList = ' + str(myChangedList) + '\n')
    # Retrieve existing timestamps from the record file.
    myErr = _read_TimestampJSONRecords(
                                        repo,
                                        File_TimestampRecords,
                                        TimeStamp_dict
                                    )
    # Check for command optional argument
    if kwargs['save']:
        _save_TimestampsJSON(
                                repo,
                                File_TimestampRecords,
                                myMatchList,
                                myChangedList,
                                myDroppedList,
                                TimeStamp_dict
                            )
    elif not myErr:
        # Only evaluate Restore or Display if file was read.
        if kwargs['restore']:
            _restore_Timestamps(repo, TimeStamp_dict, myChangedList)
        else:
            _display_Timestamps(repo, TimeStamp_dict)
        # end of check options (Restore/Display)
    else:
        repo.ui.debug(
                        'Timestamp_Mod can not continue without ',
                        File_TimestampRecords, ' file!\n'
                    )
    # end of check options
#_ end of timestamp_mod ______________________________________________________


#=============================================================================
# _get_fileModTime() Function Definition
#-----------------------------------------------------------------------------
# Summary: Return UTC timestamp value for the specified file's modified time.
#=============================================================================
def _get_fileModTime(repo, IN_FileName):
    '''Retrieve the Modification Timestamp for the specified file.'''
    repo.ui.debug('get_mtime: ')
    myFilePath = repo.wjoin(IN_FileName)
    try:
        myModTime = float(os.stat(myFilePath).st_mtime)
        repo.ui.debug(
                        time.strftime(
                                        '%Y.%m.%d %H:%M:%S',
                                        time.localtime(myModTime)
                                    ),
                        ' \t', IN_FileName, '\n'
                    )
        return myModTime
    except:
        repo.ui.warn(
                        '*** TimestampMod: Get File Stat failed for ',
                        IN_FileName, '!\n'
                    )
        repo.ui.debug('*** Exception: ', str(sys.exc_info()), '  ***\n')
        return -1
    # end of file stat access.
#_ end of _get_fileModTime ___________________________________________________


#=============================================================================
# _set_fileModTime() Function Definition
#-----------------------------------------------------------------------------
# Summary: Set the UTC timestamp value for the specified file's modified time.
#=============================================================================
def _set_fileModTime(
                        repo,
                        IN_FileName,
                        IN_ModTime
                    ):
    '''Assign the Modification Timestamp for the specified file.'''
    repo.ui.debug('set_mtime: ')
    myFilePath = repo.wjoin(IN_FileName)
    try:
        myFileStat = os.stat(myFilePath)
        os.utime(
                    myFilePath,
                    (myFileStat.st_atime,
                        type(myFileStat.st_mtime)(IN_ModTime)
                    )
                )
        repo.ui.debug(
                        time.strftime('%Y.%m.%d %H:%M:%S',
                            time.localtime(IN_ModTime)
                        ),
                        ' \t', IN_FileName, '\n'
                    )
    except:
        repo.ui.warn(
                        '*** TimestampMod: Set File Stat failed for ',
                        IN_FileName, '! ***\n'
                    )
        repo.ui.debug('*** Exception: ', str(sys.exc_info()), '  ***\n')
    # end of file stat access.
#_ end of _set_fileModTime ___________________________________________________


#=============================================================================
# _get_RepoFileList Function Definition
#-----------------------------------------------------------------------------
# Summary: Build lists of files in the Working Directory from the Repository
#   Status entries.  Add active files (clean/added/modified) to the global
#   dictionary collection, and return lists containing changed files (added/
#   modified) and dropped files (removed/deleted).
# NOTE: This function will CLEAR the contents of the global dictionary object
#   and rebuild it from scratch.
#=============================================================================
def _get_RepoFileList(
                        repo,
                        IN_ListMatch,
                        OUT_TimeStamp_dict
                    ):
    '''Build lists of files from the repository status contents.'''
    repo.ui.debug('------ Generating file list from repo...\n')
    # Establish category lists from repository status.
    modified, added, removed, deleted, unknown, ignored, clean = \
        repo.status(ignored=False, clean=True, unknown=False)
    # Changed files include modifications and additions.
    # Leave the match list out of the changed list contents.
    myChanged = modified + added
    myDropped = removed + deleted
    # Be sure to include the match list in the files to review.
    myFiles = IN_ListMatch + myChanged + clean
    # Rebuild global dictionary collection
    # Be sure to start with a clean collection.
    OUT_TimeStamp_dict.clear()
    for myFile in myFiles:
        if myFile not in myDropped:
            # Only add non-dropped files to the list,
            # even if they are part of the match list.
            myFileName = str(myFile).strip()
            # initialize dictionary entry
            OUT_TimeStamp_dict[myFileName] = -1
            repo.ui.debug('Tracking:  ', myFileName, '\n')
        # end of check for dropped files.
    # end of loop through repo files.
    myReturnList = myChanged, myDropped
    return myReturnList
#_ end of _get_RepoFileList __________________________________________________


#=============================================================================
# _read_TimestampRecords Function Definition
#-----------------------------------------------------------------------------
# Summary: Read in the data from the Timestamp Record File and assign the
#   timestamps to their corresponding entries in the global file dictionary
#   collection object.
# NOTES:
# * This method is to be kept in tact to deal with the original
#   CSV timestamp file format that may have been committed using
#   previous versions of this extension.  DO NOT LOSE this functionality!
#=============================================================================
def _read_TimestampRecords(
                            repo,
                            IN_TimestampFileName,
                            INOUT_TimeStamp_dict
                        ):
    '''Read data from Timestamp Record File.'''
    myTimeStampRecordsFile = ''
    try:
        myTimeStampRecordsFile = open(repo.wjoin(IN_TimestampFileName), 'r')
    except:
        repo.ui.warn(
                        '*** Error accessing ',
                        IN_TimestampFileName,
                        ' file! ***\n'
                    )
        repo.ui.debug('*** Exception: ', str(sys.exc_info()), '  ***\n')
        return True  # set return flag on error.
    # end of opening record file.
    repo.ui.debug('------ Retrieving timestamps from record file:\n')
    for myLine in myTimeStampRecordsFile.readlines():
        # Read the data from the line.
        # (CSV format: [FileName],[ModificationTime])
        try:
            myFileName, myModTime = myLine.strip().split(',')
            # Only import data for entries already in local dictionary.
            if myFileName in INOUT_TimeStamp_dict:
                # Make sure incoming data is properly formatted.
                INOUT_TimeStamp_dict[str(myFileName)] = float(myModTime)
                repo.ui.debug(
                                'UTC: ', myModTime,
                                '\t: ', myFileName, '\n'
                            )
            # end of check file exists in dictionary.
        except:
            repo.ui.debug('Invalid data on line: ', myLine)
        # end of read data from line.
    # end of readlines from record file.
    myTimeStampRecordsFile.close()
    return False  # no return flag as successful.
#_ end of _read_TimestampRecords _____________________________________________


#=============================================================================
# _read_TimestampJSONRecords Function Definition
#-----------------------------------------------------------------------------
# Summary: Read in the data from the Timestamp JSON Record File and assign the
#   timestamps to their corresponding entries in the global file dictionary
#   collection object.
# NOTES:
# * This method will fall back to use the previous CSV file read method if it
#   can not read the file as JSON data.
# * Many thanks to BitBucket user TAKAHIRO Kitahara (flied_onion)
#   (https://bitbucket.org/flied_onion) for huge contributions with
#   implementing and testing correct encoding for international support.
#=============================================================================
def _read_TimestampJSONRecords(
                                repo,
                                IN_TimestampFileName,
                                INOUT_TimeStamp_dict
                            ):
    '''Read data from Timestamp JSON Record File.'''
    myTimeStampJSONFile = ''
    myErr = False  # initialize as boolean
    if not os.path.exists(repo.wjoin(IN_TimestampFileName)):
        # No file to read from, so just return and treat it as an error.
        # If saving, a new file will be created.
        # If restoring, nothing more can be done without the file data.
        return True
    # end of check for existing file
    # Open timestamp record file.
    try:
        myTimeStampJSONFile = open(repo.wjoin(IN_TimestampFileName), 'rb')
    except:  # report errors
        repo.ui.warn(
                        '*** Error opening ',
                        IN_TimestampFileName,
                        ' file! ***\n'
                    )
        repo.ui.debug('*** Exception: ', str(sys.exc_info()), '  ***\n')
        return True  # set return flag on error.
    # end of opening record file.
    repo.ui.debug('------ Retrieving timestamps from JSON record file:\n')
    # Read in file content into local string.
    s_Content = myTimeStampJSONFile.read()
    myTimeStampJSONFile.close()
    # Attempt to load JSON data with correct encoding.
    openedEncoding = None
    encodingList = list(('utf_8', sys.getfilesystemencoding(), 'ascii'))
    for testEncoding in encodingList:
        repo.ui.debug('testEncoding = ', testEncoding, '\n')
        try:
            myData = json.loads(unicode(s_Content, testEncoding))
        except:
            repo.ui.debug(
                            '*** ', testEncoding, ' Exception:\n',
                            str(sys.exc_info()), '\n***\n'
                        )
            # move on to check next encoding.
            continue
        else:
            openedEncoding = testEncoding
            # valid encoding found, no need to try others.
            break
        # end of load attempt.
    # end of loop through encoding list.
    if not openedEncoding:
        # No encoding could be determined - Treat as failure.
        # Allow error checking to try opening as CSV file.
        repo.ui.debug('Failed to open as JSON file with available Encoding.\n')
        # Try opening as a pre-v0.2.0 CSV file instead:
        repo.ui.warn(
                        'Attempting to open ',
                        IN_TimestampFileName,
                        ' as CSV file\n'
                    )
        return _read_TimestampRecords(
                                        repo,
                                        IN_TimestampFileName,
                                        INOUT_TimeStamp_dict
                                    )
    # Check for version and object data:
    if ('FileData' in myData):
        if (LastPlaceholder in myData['FileData']):
            # Don't import placeholder into local dictionary.
            del myData['FileData'][LastPlaceholder]
        myWarnCount = 0  # initialize counter.
        for s_fileName, obj_fileData in myData['FileData'].items():
            # Convert s_fileName encode (Unicode)
            # to sys.getfilesystemencoding()
            s_fileName = s_fileName.encode(sys.getfilesystemencoding())
            # Only import data for entries already in local dictionary.
            if s_fileName in INOUT_TimeStamp_dict:
                myWarn = False  # initialize warning flag for each item.
                # Make sure incoming data is properly formatted.
                if isinstance(obj_fileData, float):
                    # Keep this section for compatibility
                    # with v0.2.1 timestamp file.
                    f_fileModTime = float(obj_fileData)
                    #myErr = False  # no return flag as successful.
                elif isinstance(obj_fileData, dict):
                    # As of v0.2.2, file data is stored within
                    # a dict object for future extensibility.
                    if ('timestamp' in obj_fileData):
                        f_fileModTime = float(obj_fileData['timestamp'])
                        #myErr = False  # no return flag as successful.
                    else:  # Error - missing timestamp in dictionary.
                        # use explicitly invalid value if none found.
                        f_fileModTime = -1
                        myWarn = True
                        repo.ui.warn(
                                        'WARNING: ',
                                        'Missing timestamp definition for ',
                                        str(s_fileName), ': ',
                                        str(obj_fileData), '\n'
                                    )
                else:  # Error - not a recognized data type.
                    # use explicitly invalid value if none found.
                    f_fileModTime = -1
                    myWarn = True
                    repo.ui.warn(
                                    'WARNING: Undefined data for ',
                                    str(s_fileName), ': ',
                                    str(type(obj_fileData)),
                                    ' = [', str(obj_fileData), ']\n'
                                )
                # end of check for data type
                if not myWarn:
                    INOUT_TimeStamp_dict[str(s_fileName)] = \
                        f_fileModTime
                    repo.ui.debug(
                                    'UTC: ', str(f_fileModTime),
                                    '\t: ', str(s_fileName), '\n'
                                )
                else:
                    myWarnCount += 1  # increment count of problem reads.
                # end of check for per-item warning.
            # end of check file exists in dictionary.
        # end of loop through items.
        if (len(myData['FileData']) <= 0):
            # This is not an error condition.
            # Likely just an empty working directory.
            repo.ui.debug(
                            IN_TimestampFileName,
                            ' contains no file records',
                            ' - Working Directory is empty?\n'
                        )
        elif (myWarnCount >= len(myData['FileData'])):
            repo.ui.debug(
                            'ERROR: Could not read file data from ',
                            IN_TimestampFileName, '\n'
                        )
            myErr = True  # Error - could not find JSON data
        # end of check warning count.
    else:  # JSON file data is not present
        repo.ui.debug(
                        IN_TimestampFileName,
                        ' does not have JSON file data.\n'
                    )
        myErr = True  # Error - could not find JSON FileData records
    # end of check for file data.
    if myErr:  # error return detected
        repo.ui.warn('Failed to read JSON file.\n')
    # end of final error check
    return myErr
#_ end of _read_TimestampJSONRecords _________________________________________


#=============================================================================
# _save_TimestampsJSON Function Definition
#-----------------------------------------------------------------------------
# Summary: Save File Modified Timestamps for files in the global dictionary
#   object to a JSON-formatted record file in the repository root directory.
#   New values for timestamps will be retrieved for files that have changed
#   or are missing timestamp records in the first place.
# NOTES:
# * Many thanks to BitBucket user lboehler (https://bitbucket.org/lboehler)
#   for suggesting using the SORTED list to build the timestamp file.  This
#   really should have been obvious, but I completely overlooked it!
# * The JSON file will be built manually instead of using the built-in JSON
#   methods.  This is because the dict object in Python can not be sorted well
#   enough to generate a repeatable JSON file with the data in the same order
#   all the time.  Maybe when Mercurial starts using Python 2.7+, the newer
#   OrderedDict object type may be able to be used here.
#=============================================================================
def _save_TimestampsJSON(
                            repo,
                            IN_TimestampFileName,
                            IN_MatchList,
                            IN_ChangedList,
                            IN_DroppedList,
                            INOUT_TimeStamp_dict
                        ):
    '''Save File Modification Timestamps to JSON record file.'''
    repo.ui.note('------ Saving timestamps to JSON file...\n')
    # Remove the record file from the list.
    #   It causes confusion and difficulty during merge.
    if IN_TimestampFileName in INOUT_TimeStamp_dict:
        del INOUT_TimeStamp_dict[IN_TimestampFileName]
    # end of check for record file.
    myTimeStampJSONFile = open(repo.wjoin(IN_TimestampFileName), 'w')
    # Record version data for the file.
    myTimeStampJSONFile.write(
                                '{\n"Version": "' +
                                File_Version +
                                '",\n"FileData":{\n'
                            )
    # Make sure to use a sorted dictionary for the file data:
    for s_fileName in sorted(INOUT_TimeStamp_dict.keys(), key=str.lower):
        f_fileModTime = INOUT_TimeStamp_dict[s_fileName]
        if (s_fileName in IN_ChangedList) or (f_fileModTime <= 0):
            # Make sure to only save new file timestamps if they have
            # been included in the commit (will be in the match list).
            if ((f_fileModTime > 0) and
                (len(IN_MatchList) > 0) and
                (s_fileName not in IN_MatchList)):
                # Do not save new timestamp if file is not in match list.
                pass
            else:
                # File is in match list,
                # or match list is empty (save all timestamps)
                f_fileModTime = \
                    INOUT_TimeStamp_dict[s_fileName] = \
                    _get_fileModTime(repo, s_fileName)
            # end of check for file in match list
        # end of update timestamps for changed items or missing timestamps.
        if (s_fileName in IN_DroppedList) or (f_fileModTime <= 0):
            repo.ui.debug(
                            '...removing record of dropped file, ',
                            'or file with missing timestamp ',
                            '(', s_fileName, ')\n'
                        )
            del INOUT_TimeStamp_dict[s_fileName]
        else:  # timestamp is valid
            myTimeStampJSONFile.write(
                                        '"%s": {"timestamp": %s},\n'
                                        % (s_fileName, f_fileModTime)
                                    )
        # end of check for non-existing files or timestamps.
    # end of loop through dictionary items.
    # Add one last record to wrap up the JSON formatting.
    myTimeStampJSONFile.write(
                                '"' + LastPlaceholder +
                                '": {"timestamp": 0}' +
                                '\n}\n}\n'
                            )
    # Make sure to close the file!
    myTimeStampJSONFile.close()
    # Make sure the record file is in the repository.
    if not (IN_TimestampFileName in repo.dirstate):
        repo.ui.debug(
                        '_save_TimestampsJSON: ',
                        IN_TimestampFileName,
                        ' not in repo.dirstate  Adding...\n'
                    )
        #\/ same method used for adding '.hgtags' file in localrepo.py
        repo[None].add([IN_TimestampFileName])
        repo.dirstate.rebuild  # to pick up new record file state.
    # end of check for record file in repository.
#_ end of _save_TimestampsJSON _______________________________________________


#=============================================================================
# _restore_Timestamps Function Definition
#-----------------------------------------------------------------------------
# Summary: Restore the File Modification Timestamp property for files in the
#   global dictionary collection.
# NOTE: This presumes that the dictionary has been initialized and original
#   timestamp data has already been read into the dictionary.
#=============================================================================
def _restore_Timestamps(repo, IN_TimeStamp_dict, IN_pendingChangedList):
    '''Restore File Modification Timestamps from record file.'''
    repo.ui.note('------ Restoring timestamps...\n')
    for s_fileName, f_fileModTime in IN_TimeStamp_dict.items():
        if (s_fileName in IN_pendingChangedList):
            repo.ui.debug(
                    s_fileName,
                    ' - skipped - pending changes in working directory\n'
                )
            break # do not restore timestamp if file has pending changes.
        # Check for valid timestamp.
        if (f_fileModTime > 0):
            # Valid timestamp detected!
            # Restore file timestamp.
            _set_fileModTime(repo, s_fileName, f_fileModTime)
        else:
            # No valid timestamp recorded, skip this file.
            repo.ui.debug(s_fileName, ' - skipped - no timestamp recorded\n')
        # end of check for valid timestamp.
    # end of loop through dictionary items.
#_ end of _restore_Timestamps ________________________________________________


#=============================================================================
# _display_Timestamps Function Definition
#-----------------------------------------------------------------------------
# Summary: Default action. Display File Modification Timestamp property for
#   files in the global dictionary collection.
# NOTE: This presumes that the dictionary has been initialized and original
#   timestamp data has already been read into the dictionary.
#=============================================================================
def _display_Timestamps(repo, myTimeStamp_dict):
    '''Display File Timestamps currently recorded.'''
    repo.ui.note('------ Displaying timestamps...\n')
    for s_fileName, f_fileModTime in myTimeStamp_dict.items():
        # Check for valid timestamp
        if (f_fileModTime > 0):
            # Valid timestamp detected!
            # Display timestamp using local time adjustment.
            repo.ui.note(
                            time.strftime('%Y.%m.%d %H:%M:%S',
                                            time.localtime(f_fileModTime)
                                        ),
                            ' \t', s_fileName, '\n'
                        )
        else:
            # No valid timestamp recorded, skip this file.
            repo.ui.debug(
                            s_fileName,
                            ' - skipped - no timestamp recorded\n'
                        )
        # end of check for valid timestamp.
    # end of loop through dictionary items.
#_ end of _display_Timestamps ________________________________________________


#=============================================================================
# _check_Merge_unresolved Function Definition
#-----------------------------------------------------------------------------
# Summary: Check the repository's MergeState for any unresolved files
# and return True if any unresolved files are found (False otherwise).
#=============================================================================
def _check_Merge_unresolved(repo):
    '''Determine if there are unresolved files after a merge.'''
    repo.ui.debug('...checking for unresolved files...\n')
    localms = mergemod.mergestate.read(repo)
    for testfile in localms:
        if (localms[testfile] == 'u'):
            repo.ui.debug('Unresolved merge conflicts found!\n')
            return True
    # end of loop through mergestate files
    return False  # will get to here if no unresolved files.
#_ end of _check_Merge_unresolved ____________________________________________
