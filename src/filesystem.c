/*
===========================================================================
    Copyright (C) 2010-2013  Ninja and TheKelm of the IceOps-Team
    Copyright (C) 1999-2005 Id Software, Inc.

    This file is part of CoD4X17a-Server source code.

    CoD4X17a-Server source code is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    CoD4X17a-Server source code is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>
===========================================================================
*/


/*****************************************************************************
 * name:		files.c
 *
 * desc:		handle based filesystem for Quake III Arena 
 *
 * $Archive: /MissionPack/code/qcommon/files.c $
 *
 *****************************************************************************/

/*
=============================================================================

QUAKE3 FILESYSTEM

All of Quake's data access is through a hierarchical file system, but the contents of 
the file system can be transparently merged from several sources.

A "qpath" is a reference to game file data.  MAX_ZPATH is 256 characters, which must include
a terminating zero. "..", "\\", and ":" are explicitly illegal in qpaths to prevent any
references outside the quake directory system.

The "base path" is the path to the directory holding all the game directories and usually
the executable.  It defaults to ".", but can be overridden with a "+set fs_basepath c:\quake3"
command line to allow code debugging in a different directory.  Basepath cannot
be modified at all after startup.  Any files that are created (demos, screenshots,
etc) will be created reletive to the base path, so base path should usually be writable.

The "cd path" is the path to an alternate hierarchy that will be searched if a file
is not located in the base path.  A user can do a partial install that copies some
data to a base path created on their hard drive and leave the rest on the cd.  Files
are never writen to the cd path.  It defaults to a value set by the installer, like
"e:\quake3", but it can be overridden with "+set ds_cdpath g:\quake3".

If a user runs the game directly from a CD, the base path would be on the CD.  This
should still function correctly, but all file writes will fail (harmlessly).

The "home path" is the path used for all write access. On win32 systems we have "base path"
== "home path", but on *nix systems the base installation is usually readonly, and
"home path" points to ~/.q3a or similar

The user can also install custom mods and content in "home path", so it should be searched
along with "home path" and "cd path" for game content.


The "base game" is the directory under the paths where data comes from by default, and
can be either "baseq3" or "demoq3".

The "current game" may be the same as the base game, or it may be the name of another
directory under the paths that should be searched for files before looking in the base game.
This is the basis for addons.

Clients automatically set the game directory after receiving a gamestate from a server,
so only servers need to worry about +set fs_game.

No other directories outside of the base game and current game will ever be referenced by
filesystem functions.

To save disk space and speed loading, directory trees can be collapsed into zip files.
The files use a ".pk3" extension to prevent users from unzipping them accidentally, but
otherwise the are simply normal uncompressed zip files.  A game directory can have multiple
zip files of the form "pak0.pk3", "pak1.pk3", etc.  Zip files are searched in decending order
from the highest number to the lowest, and will always take precedence over the filesystem.
This allows a pk3 distributed as a patch to override all existing data.

Because we will have updated executables freely available online, there is no point to
trying to restrict demo / oem versions of the game with code changes.  Demo / oem versions
should be exactly the same executables as release versions, but with different data that
automatically restricts where game media can come from to prevent add-ons from working.

After the paths are initialized, quake will look for the product.txt file.  If not
found and verified, the game will run in restricted mode.  In restricted mode, only 
files contained in demoq3/pak0.pk3 will be available for loading, and only if the zip header is
verified to not have been modified.  A single exception is made for q3config.cfg.  Files
can still be written out in restricted mode, so screenshots and demos are allowed.
Restricted mode can be tested by setting "+set fs_restrict 1" on the command line, even
if there is a valid product.txt under the basepath or cdpath.

If not running in restricted mode, and a file is not found in any local filesystem,
an attempt will be made to download it and save it under the base path.

If the "fs_copyfiles" cvar is set to 1, then every time a file is sourced from the cd
path, it will be copied over to the base path.  This is a development aid to help build
test releases and to copy working sets over slow network links.

File search order: when FS_FOpenFileRead gets called it will go through the fs_searchpaths
structure and stop on the first successful hit. fs_searchpaths is built with successive
calls to FS_AddGameDirectory

Additionaly, we search in several subdirectories:
current game is the current mode
base game is a variable to allow mods based on other mods
(such as baseq3 + missionpack content combination in a mod for instance)
BASEGAME is the hardcoded base game ("baseq3")

e.g. the qpath "sound/newstuff/test.wav" would be searched for in the following places:

home path + current game's zip files
home path + current game's directory
base path + current game's zip files
base path + current game's directory
cd path + current game's zip files
cd path + current game's directory

home path + base game's zip file
home path + base game's directory
base path + base game's zip file
base path + base game's directory
cd path + base game's zip file
cd path + base game's directory

home path + BASEGAME's zip file
home path + BASEGAME's directory
base path + BASEGAME's zip file
base path + BASEGAME's directory
cd path + BASEGAME's zip file
cd path + BASEGAME's directory

server download, to be written to home path + current game's directory


The filesystem can be safely shutdown and reinitialized with different
basedir / cddir / game combinations, but all other subsystems that rely on it
(sound, video) must also be forced to restart.

Because the same files are loaded by both the clip model (CM_) and renderer (TR_)
subsystems, a simple single-file caching scheme is used.  The CM_ subsystems will
load the file with a request to cache.  Only one file will be kept cached at a time,
so any models that are going to be referenced by both subsystems should alternate
between the CM_ load function and the ref load function.

TODO: A qpath that starts with a leading slash will always refer to the base game, even if another
game is currently active.  This allows character models, skins, and sounds to be downloaded
to a common directory no matter which game is active.

How to prevent downloading zip files?
Pass pk3 file names in systeminfo, and download before FS_Restart()?

Aborting a download disconnects the client from the server.

How to mark files as downloadable?  Commercial add-ons won't be downloadable.

Non-commercial downloads will want to download the entire zip file.
the game would have to be reset to actually read the zip in

Auto-update information

Path separators

Casing

  separate server gamedir and client gamedir, so if the user starts
  a local game after having connected to a network game, it won't stick
  with the network game.

  allow menu options for game selection?

Read / write config to floppy option.

Different version coexistance?

When building a pak file, make sure a q3config.cfg isn't present in it,
or configs will never get loaded from disk!

  todo:

  downloading (outside fs?)
  game directory passing and restarting

=============================================================================

*/

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "filesystem.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "qcommon_mem.h"
#include "qcommon_logprint.h"
#include "cvar.h"
#include "q_platform.h"
#include "qcommon_parsecmdline.h"
#include "sys_main.h"
#include "cmd.h"
#include "sys_thread.h"
#include "plugin_handler.h"

#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>



#define BASEGAME "main"
#define fs_gamedirvar fs_gameDirVar

cvar_t* fs_debug;
cvar_t* fs_copyfiles;
cvar_t* fs_cdpath;
cvar_t* fs_basepath;
cvar_t* fs_basegame;
cvar_t* fs_gameDirVar;
cvar_t* fs_ignoreLocalized;
cvar_t* fs_homepath;
cvar_t* fs_restrict;
cvar_t* fs_usedevdir;

cvar_t* loc_language;
cvar_t* loc_forceEnglish;
cvar_t* loc_translate;
cvar_t* loc_warnings;
cvar_t* loc_warningsAsErrors;
qboolean g_currentAsian;



int fs_packFiles;
int fs_checksumFeed;
static int fs_loadStack;
static searchpath_t* fs_searchpaths;
static fileHandleData_t fsh[MAX_FILE_HANDLES +1];

static char fs_gamedir[MAX_OSPATH];
static char lastValidBase[MAX_OSPATH];
static char lastValidGame[MAX_OSPATH];


static int fs_numServerIwds;
static int fs_serverIwds[1024];
static char *fs_serverIwdNames[1024];


#define FS_ListFiles( dir, extension, nfiles ) Sys_ListFiles(dir, extension, 0, nfiles, 0)
#define FS_FreeFileList Sys_FreeFileList

/*
typedef void (__cdecl *tFS_WriteFile)(const char* qpath, const void *buffer, int size);
tFS_WriteFile FS_WriteFile = (tFS_WriteFile)(0x818a58c);

typedef void (__cdecl *tFS_FreeFile)(void *buffer);
tFS_FreeFile FS_FreeFile = (tFS_FreeFile)(0x8187430);

typedef void (__cdecl *tFS_SV_Rename)(const char* from,const char* to);
tFS_SV_Rename FS_SV_Rename = (tFS_SV_Rename)(0x81287da);

typedef int (__cdecl *tFS_Write)(void const* data,int length, fileHandle_t);
tFS_Write FS_Write = (tFS_Write)(0x8186ec4);

typedef int (__cdecl *tFS_Read)(void const* data,int length, fileHandle_t);
tFS_Read FS_Read = (tFS_Read)(0x8186f64);
*/





/*
==============
FS_Initialized
==============
*/

qboolean FS_Initialized() {
	
	return (fs_searchpaths != NULL);
}

/*
=================
FS_LoadStack
return load stack
=================
*/
int FS_LoadStack() {
	int val;
	
	Sys_EnterCriticalSection(CRIT_FILESYSTEM);
	val = fs_loadStack;
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
	
	return val;
}

void FS_LoadStackInc()
{
	Sys_EnterCriticalSection(CRIT_FILESYSTEM);
	fs_loadStack++;
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);	
}
void FS_LoadStackDec()
{
	Sys_EnterCriticalSection(CRIT_FILESYSTEM);
	fs_loadStack--;
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);	
}


static fileHandle_t FS_HandleForFileForThread(int FsThread)
{
  signed int startIndex;
  signed int size;
  signed int i;
  mvabuf;

	
  if ( FsThread == 1 )
  {
    startIndex = 49;
    size = 13;
  }else if ( FsThread == 3 ){
    startIndex = 63;
    size = 1;
  }else if ( FsThread ){
    startIndex = 62;
    size = 1;
  }else{
    startIndex = 1;
	size = 48;
  }
  
  Sys_EnterCriticalSection(CRIT_FILESYSTEM);
	
  for (i = 0 ; size > i ; i++)
  {
    if ( fsh[i + startIndex].handleFiles.file.o == NULL )
    {
		if ( fs_debug->integer > 1 )
			Sys_Print(va("^4Open filehandle: %d\n", i + startIndex));
		
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		
		return i + startIndex;
    }
  }
  
  Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
	
  if ( !FsThread )
  {
	for(i = 1; i < MAX_FILE_HANDLES; i++)
	{
		Com_Printf("FILE %2i: '%s' 0x%x\n", i, fsh[i].name, fsh[i].handleFiles.file.o);
	}
	Com_Error(2, "FS_HandleForFile: none free");
  }
  Com_PrintWarning( "FILE %2i: '%s' 0x%x\n", startIndex, fsh[startIndex].name, fsh[startIndex].handleFiles.file.o);
  Com_PrintWarning( "FS_HandleForFile: none free (%d)\n", FsThread);
  return 0;
}

#define FS_HandleForFile() FS_HandleForFileForThread(0)

static FILE	*FS_FileForHandle( fileHandle_t f ) {

	mvabuf;

	if ( f < 0 || f > MAX_FILE_HANDLES ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: out of range %i\n", f);
	}

	if ( fs_debug->integer > 1 )
		Sys_Print(va("^4Using filehandle: %d Name: %s\n", f, fsh[f].name));

	if ( !fsh[f].handleFiles.file.o ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: NULL" );
	}
	
	return fsh[f].handleFiles.file.o;
}

static void FS_SetFilenameForHandle( fileHandle_t f, const char* filename ) {

	if ( f < 0 || f > MAX_FILE_HANDLES ) {
		Com_Error( ERR_DROP, "FS_SetFilenameForHandle: out of range %i\n", f);
	}
	Q_strncpyz(fsh[f].name, filename, sizeof(fsh[f].name));
}


void	FS_ForceFlush( fileHandle_t f ) {
	FILE *file;

	file = FS_FileForHandle(f);
	setvbuf( file, NULL, _IONBF, 0 );
}


/*
==============
FS_FCloseFile

If the FILE pointer is an open pak file, leave it open.

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
qboolean FS_FCloseFile( fileHandle_t f ) {

	mvabuf;

	if ( f < 0 || f > MAX_FILE_HANDLES ) {
		Com_Error( ERR_DROP, "FS_FCloseFile: out of range %i\n", f);
	}

	if ( fs_debug->integer > 1 )
		Sys_Print(va("^4Close filehandle: %d File: %s\n", f, fsh[f].name));

	if(fsh[f].zipFile)
	{
		unzCloseCurrentFile(fsh[f].handleFiles.file.z);
		if(fsh[f].handleFiles.unique)
		{
			unzClose(fsh[f].handleFiles.file.z);
		}
		Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
		return qtrue;
	}

	if (fsh[f].handleFiles.file.o) {
	// we didn't find it as a pak, so close it as a unique file
	    fclose (fsh[f].handleFiles.file.o);
	    Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
	    return qtrue;
	}

	Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
	return qfalse;
}



/*
================
FS_filelength

If this is called on a non-unique FILE (from a pak file),
it will return the size of the pak file, not the expected
size of the file.
================
*/
int FS_filelength( fileHandle_t f ) {
	int		pos;
	int		end;
	FILE*	h;

	h = FS_FileForHandle(f);
	pos = ftell (h);
	fseek (h, 0, SEEK_END);
	end = ftell (h);
	fseek (h, pos, SEEK_SET);

	return end;
}

/*
====================
FS_ReplaceSeparators

Fix things up differently for win/unix/mac
====================
*/
static void FS_ReplaceSeparators( char *path ) {
	char	*s;

	for ( s = path ; *s ; s++ ) {
		if ( *s == '/' || *s == '\\' ) {
			*s = PATH_SEP;
		}
	}
}


/*
====================
FS_StripTrailingSeperator

Fix things up differently for win/unix/mac
====================
*/
static void FS_StripTrailingSeperator( char *path ) {

	int len = strlen(path);

	if(path[len -1] == PATH_SEP)
	{
		path[len -1] = '\0';
	}
}



void FS_BuildOSPathForThread(const char *base, const char *game, const char *qpath, char *fs_path, int fs_thread)
{
  char basename[MAX_OSPATH];
  char gamename[MAX_OSPATH];

  int len;

  if ( !game || !*game )
    game = fs_gamedir;

  Q_strncpyz(basename, base, sizeof(basename));
  Q_strncpyz(gamename, game, sizeof(gamename));

  len = strlen(basename);
  if(len > 0 && (basename[len -1] == '/' || basename[len -1] == '\\'))
  {
        basename[len -1] = '\0';
  }

  len = strlen(gamename);
  if(len > 0 && (gamename[len -1] == '/' || gamename[len -1] == '\\'))
  {
        gamename[len -1] = '\0';
  }
  if ( Com_sprintf(fs_path, MAX_OSPATH, "%s/%s/%s", basename, gamename, qpath) >= MAX_OSPATH )
  {
    if ( fs_thread )
    {
        fs_path[0] = 0;
        return;
    }
    Com_Error(ERR_FATAL, "FS_BuildOSPath: os path length exceeded");
  }
  FS_ReplaceSeparators(fs_path);
}

/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
qboolean FS_CreatePath (char *OSPath) {
	char	*ofs;

	// make absolutely sure that it can't back up the path
	// FIXME: is c: allowed???
	if ( strstr( OSPath, ".." ) || strstr( OSPath, "::" ) ) {
		Com_Printf( "WARNING: refusing to create relative path \"%s\"\n", OSPath );
		return qtrue;
	}

	for (ofs = OSPath+1 ; *ofs ; ofs++) {
		if (*ofs == PATH_SEP) {	
			// create the directory
			*ofs = 0;
			Sys_Mkdir (OSPath);
			*ofs = PATH_SEP;
		}
	}
	return qfalse;
}


/*
===========
FS_HomeRemove

===========
*/
qboolean FS_HomeRemove( const char *path ) {
	char osPath[MAX_OSPATH];
	FS_BuildOSPathForThread( fs_homepath->string, "", path, osPath, 0);
	return remove( osPath ) == 0;
}

/*
===========
FS_SV_HomeRemove

===========
*/
qboolean FS_SV_HomeRemove( const char *path ) {
	char osPath[MAX_OSPATH];
	FS_BuildOSPathForThread( fs_homepath->string, path, "", osPath, 0 );
	FS_StripTrailingSeperator( osPath );
	return remove( osPath ) == 0;
}

/*
===========
FS_BaseRemove

===========
*/
qboolean FS_BaseRemove( const char *path ) {
	char osPath[MAX_OSPATH];
	FS_BuildOSPathForThread( fs_basepath->string, "", path, osPath, 0);
	return remove( osPath ) == 0;
}

/*
===========
FS_SV_BaseRemove

===========
*/
qboolean FS_SV_BaseRemove( const char *path ) {
	char osPath[MAX_OSPATH];
	FS_BuildOSPathForThread( fs_basepath->string, path, "", osPath, 0 );
	FS_StripTrailingSeperator( osPath );
	return remove( osPath ) == 0;
}


/*
===========
FS_RemoveOSPath

===========
*/
void FS_RemoveOSPath( const char *osPath ) {
	remove( osPath );
}

/*
================
FS_FileExists

Tests if the file exists in the current gamedir, this DOES NOT
search the paths.  This is to determine if opening a file to write
(which always goes into the current gamedir) will cause any overwrites.
NOTE TTimo: this goes with FS_FOpenFileWrite for opening the file afterwards
================
*/
qboolean FS_FileExists( const char *file )
{
	FILE *f;
	char testpath[MAX_OSPATH];

	FS_BuildOSPathForThread( fs_homepath->string, "", file, testpath, 0);

	f = fopen( testpath, "rb" );
	if (f) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}

/*
================
FS_SV_FileExists

Tests if the file exists in the current gamedir, this DOES NOT
search the paths.  This is to determine if opening a file to write
(which always goes into the current gamedir) will cause any overwrites.
NOTE TTimo: this goes with FS_FOpenFileWrite for opening the file afterwards
================
*/
qboolean FS_SV_HomeFileExists( const char *file )
{
	FILE *f;
	char testpath[MAX_OSPATH];

	FS_BuildOSPathForThread( fs_homepath->string, file, "", testpath, 0);
	FS_StripTrailingSeperator( testpath );

	f = fopen( testpath, "rb" );
	if (f) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}

qboolean FS_SV_FileExists( const char *file )
{
	FILE *f;
	char testpath[MAX_OSPATH];

	FS_BuildOSPathForThread( fs_homepath->string, file, "", testpath, 0);
	FS_StripTrailingSeperator( testpath );

	f = fopen( testpath, "rb" );
	if (f) {
		fclose( f );
		return qtrue;
	}

	FS_BuildOSPathForThread( fs_basepath->string, file, "", testpath, 0);
	FS_StripTrailingSeperator( testpath );

	f = fopen( testpath, "rb" );
	if (f) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}



char* FS_SV_GetFilepath( const char *file, char* testpath, int lenpath )
{
	FILE *f;

	if(lenpath < MAX_OSPATH)
		Com_Error(ERR_FATAL, "FS_SV_GetFilepath: Given buffer has less than %d bytes length\n", MAX_OSPATH );
	
	FS_BuildOSPathForThread(fs_homepath->string, file, "", testpath, 0 );
	FS_StripTrailingSeperator( testpath );

	f = fopen( testpath, "rb" );
	if (f) {
		fclose( f );
		return testpath;
	}

	FS_BuildOSPathForThread( fs_basepath->string, file, "", testpath, 0 );
	FS_StripTrailingSeperator( testpath );

	f = fopen( testpath, "rb" );
	if (f) {
		fclose( f );
		return testpath;
	}

	return NULL;
}


/*
===========
FS_Rename

===========
*/
void FS_Rename( const char *from, const char *to ) {
	char	from_ospath[MAX_OSPATH];
	char	to_ospath[MAX_OSPATH];
	mvabuf;

	FS_BuildOSPathForThread( fs_homepath->string, "", from, from_ospath, 0);
	FS_BuildOSPathForThread( fs_homepath->string, "", to, to_ospath, 0);
	FS_StripTrailingSeperator( to_ospath );
	FS_StripTrailingSeperator( from_ospath );

	if ( fs_debug->integer ) {
		Sys_Print(va("^4FS_Rename: %s --> %s\n", from_ospath, to_ospath ));
	}

	if (rename( from_ospath, to_ospath )) {
		// Failed, try copying it and deleting the original
		FS_CopyFile ( from_ospath, to_ospath );
		FS_RemoveOSPath ( from_ospath );
	}
}

/*
===========
FS_RenameOSPath

===========
*/
void FS_RenameOSPath( const char *from_ospath, const char *to_ospath ) {

	mvabuf;

	if ( fs_debug->integer ) {
		Sys_Print(va("^4FS_RenameOSPath: %s --> %s\n", from_ospath, to_ospath ));
	}

	if (rename( from_ospath, to_ospath )) {
		// Failed, try copying it and deleting the original
		FS_CopyFile ( (char*)from_ospath, (char*)to_ospath );
		FS_RemoveOSPath ( from_ospath );
	}
}

/*
===========
FS_FileExistsOSPath

===========
*/
qboolean FS_FileExistsOSPath( const char *ospath ) {

	FILE* f;

	f = fopen( ospath, "rb" );
	if (f) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}



/*
===========
FS_SV_Rename
Will rename files inside fs_homepath and fs_basepath
ignores zip-files
===========
*/
void FS_SV_Rename( const char *from, const char *to ) {
	char	from_ospath[MAX_OSPATH];
	char	to_ospath[MAX_OSPATH];
	FILE *f;
	mvabuf;

	FS_BuildOSPathForThread( fs_homepath->string, from, "", from_ospath, 0);
	FS_BuildOSPathForThread( fs_homepath->string, to, "", to_ospath, 0);
	FS_StripTrailingSeperator( to_ospath );
	FS_StripTrailingSeperator( from_ospath );

	f = fopen( from_ospath, "rb" );
	if (f) {
		fclose( f );
		if ( fs_debug->integer ) {
			Sys_Print(va("^4FS_Rename: %s --> %s\n", from_ospath, to_ospath ));
		}
		if (rename( from_ospath, to_ospath )) {
			// Failed, try copying it and deleting the original
			FS_CopyFile ( from_ospath, to_ospath );
			FS_RemoveOSPath ( from_ospath );
		}
	}

	FS_BuildOSPathForThread( fs_basepath->string, from, "", from_ospath, 0);
	FS_BuildOSPathForThread( fs_basepath->string, to, "", to_ospath, 0);
	FS_StripTrailingSeperator( to_ospath );
	FS_StripTrailingSeperator( from_ospath );

	f = fopen( from_ospath, "rb" );
	if (f) {
		fclose( f );
		if ( fs_debug->integer ) {
			Sys_Print(va("^4FS_Rename: %s --> %s\n", from_ospath, to_ospath ));
		}
		if (rename( from_ospath, to_ospath )) {
			// Failed, try copying it and deleting the original
			FS_CopyFile ( from_ospath, to_ospath );
			FS_RemoveOSPath ( from_ospath );
		}
	}
}

/*
===========
FS_SV_HomeRename
Will rename files inside fs_homepath
ignores zip-files
===========
*/
void FS_SV_HomeRename( const char *from, const char *to ) {
	char	from_ospath[MAX_OSPATH];
	char	to_ospath[MAX_OSPATH];
	mvabuf;


	FS_BuildOSPathForThread( fs_homepath->string, from, "", from_ospath, 0);
	FS_BuildOSPathForThread( fs_homepath->string, to, "", to_ospath, 0);

	FS_StripTrailingSeperator( to_ospath );
	FS_StripTrailingSeperator( from_ospath );

	if ( fs_debug->integer ) {
		Sys_Print(va("^4FS_Rename: %s --> %s\n", from_ospath, to_ospath ));
	}

	if (rename( from_ospath, to_ospath )) {
		// Failed, try copying it and deleting the original
		FS_CopyFile ( from_ospath, to_ospath );
		FS_RemoveOSPath ( from_ospath );
	}
}






/*
===========
FS_FilenameCompare

Ignore case and seprator char distinctions
===========
*/
qboolean FS_FilenameCompare( const char *s1, const char *s2 ) {
	int		c1, c2;
	
	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}
		
		if (c1 != c2) {
			return -1;		// strings not equal
		}
	} while (c1);
	
	return 0;		// strings are equal
}

/*
===========
FS_ShiftedStrStr
===========
*/
char *FS_ShiftedStrStr(const char *string, const char *substring, int shift) {
	char buf[MAX_STRING_TOKENS];
	int i;

	for (i = 0; substring[i]; i++) {
		buf[i] = substring[i] + shift;
	}
	buf[i] = '\0';
	return strstr(string, buf);
}




/*
================
FS_fplength
================
*/

long FS_fplength(FILE *h)
{
	long		pos;
	long		end;

	pos = ftell(h);
	fseek(h, 0, SEEK_END);
	end = ftell(h);
	fseek(h, pos, SEEK_SET);

	return end;
}


/*
===========
FS_IsExt

Return qtrue if ext matches file extension filename
===========
*/

qboolean FS_IsExt(const char *filename, const char *ext, int namelen)
{
	int extlen;

	extlen = strlen(ext);

	if(extlen > namelen)
		return qfalse;

	filename += namelen - extlen;

	return !Q_stricmp(filename, ext);
}




/*
=================================================================================

DIRECTORY SCANNING FUNCTIONS

=================================================================================
*/

#define	MAX_FOUND_FILES	0x1000



/*
===========
FS_ConvertPath
===========
*/
void FS_ConvertPath( char *s ) {
	while (*s) {
		if ( *s == '\\' || *s == ':' ) {
			*s = '/';
		}
		s++;
	}
}

/*
===========
FS_PathCmp

Ignore case and seprator char distinctions
===========
*/
int FS_PathCmp( const char *s1, const char *s2 ) {
	int		c1, c2;
	
	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}
		
		if (c1 < c2) {
			return -1;		// strings not equal
		}
		if (c1 > c2) {
			return 1;
		}
	} while (c1);
	
	return 0;		// strings are equal
}

/*
================
return a hash value for the filename
================
*/
static long FS_HashFileName( const char *fname, int hashSize ) {
	int i;
	long hash;
	char letter;

	hash = 0;
	i = 0;
	while ( fname[i] != '\0' ) {
		letter = tolower( fname[i] );
		if ( letter == '.' ) {
			break;                          // don't include extension
		}
		if ( letter == '\\' ) {
			letter = '/';                   // damn path names
		}
		if ( letter == PATH_SEP ) {
			letter = '/';                           // damn path names
		}
		hash += (long)( letter ) * ( i + 119 );
		i++;
	}
	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) );
	hash &= ( hashSize - 1 );
	return hash;
}


/*
===========
FS_FOpenFileReadDir

Tries opening file "filename" in searchpath "search"
Returns filesize and an open FILE pointer.
===========
*/
extern qboolean		com_fullyInitialized;

long FS_FOpenFileReadDir(const char *filename, searchpath_t *search, fileHandle_t *file, qboolean uniqueFILE, qboolean unpure, int FsThread)
{
	long			hash;
	pack_t		*pak;
	fileInPack_t	*pakFile;
	directory_t	*dir;
	char		netpath[MAX_OSPATH];
	FILE		*filep;
	int		len;
	int		err;
	unz_file_info	file_info;
	mvabuf;
	char *noReferenceExts[] = { "cod4_lnxded-bin", ".so", ".dll", ".hlsl", ".txt", ".cfg", ".levelshots", ".menu", ".arena", ".str", NULL };
	char **testExt;


	if(filename == NULL)
		Com_Error(ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed");

	// qpaths are not supposed to have a leading slash
	if(filename[0] == '/' || filename[0] == '\\')
		filename++;

	// make absolutely sure that it can't back up the path.
	// The searchpaths do guarantee that something will always
	// be prepended, so we don't need to worry about "c:" or "//limbo" 
	if(strstr(filename, ".." ) || strstr(filename, "::"))
	{
	        if(file == NULL)
	                return qfalse;
	
		*file = 0;
		return -1;
	}
	if(file == NULL)
	{
		// just wants to see if file is there

		/* Cod4 adds localization check */
		if(search->localized && !fs_ignoreLocalized->boolean && search->langIndex != loc_language->integer)
			return 0;

		// is the element a pak file?
		if(search->pack)
		{
			hash = FS_HashFileName(filename, search->pack->hashSize);

                        if(search->pack->hashTable[hash])
                        {
				// look through all the pak file elements
				pak = search->pack;
				pakFile = pak->hashTable[hash];

				do
				{
					// case and separator insensitive comparisons
					if(!FS_FilenameCompare(pakFile->name, filename))
					{
						err = unzLocateFile( pak->handle, pakFile->name, 2);
						if(err != UNZ_OK)
						{
							Com_PrintWarning("Read error in Zip-file: %s\n", pak->pakFilename);
							return 1;
						}
						err = unzGetCurrentFileInfo( pak->handle, &file_info, netpath, sizeof( netpath ), NULL, 0, NULL, 0 );
						if(err != UNZ_OK)
						{
							Com_PrintWarning("Read error in Zip-file: %s\n", pak->pakFilename);
							return 1;
						}

						// found it!
						if(file_info.uncompressed_size)
							return file_info.uncompressed_size;
                                                else
                                                {
                                                        // It's not nice, but legacy code depends
                                                        // on positive value if file exists no matter
                                                        // what size
                                                        return 1;
                                                }
					}

					pakFile = pakFile->next;
				} while(pakFile != NULL);
			}
		}
		else if(search->dir)
		{
			dir = search->dir;
		
			FS_BuildOSPathForThread(dir->path, dir->gamedir, filename, netpath, FsThread);
			filep = fopen (netpath, "rb");

			if(filep)
			{
			        len = FS_fplength(filep);
				fclose(filep);
				
				if(len)
					return len;
                                else
                                        return 1;
			}
		}
		
		return -1;
	}
	Sys_EnterCriticalSection(CRIT_FILESYSTEM);
	*file = FS_HandleForFileForThread(FsThread);

	if(*file == 0)
		return -1;

	FS_SetFilenameForHandle(*file, filename);

	fsh[*file].handleFiles.unique = uniqueFILE;
	
	// is the element a pak file?
	if(search->pack)
	{

		hash = FS_HashFileName(filename, search->pack->hashSize);

		if(search->pack->hashTable[hash])
		{
			// look through all the pak file elements
			pak = search->pack;
			pakFile = pak->hashTable[hash];
		
			do
			{
				// case and separator insensitive comparisons
				if(!FS_FilenameCompare(pakFile->name, filename))
				{
					// found it!

					// mark the pak as having been referenced and mark specifics on cgame and ui
					// shaders, txt, arena files  by themselves do not count as a reference as 
					// these are loaded from all pk3s 
					// from every pk3 file.. 
					len = strlen(filename);


					if (!(pak->referenced & FS_GENERAL_REF))
					{
						for(testExt = noReferenceExts; *testExt; ++testExt)
						{
							if(FS_IsExt(filename, *testExt, len))
							{
								break;
							}
						}
						if(*testExt == NULL)
						{
							pak->referenced |= FS_GENERAL_REF;
							FS_AddIwdPureCheckReference(search);
						}
					}

					if(uniqueFILE)
					{
						// open a new file on the pakfile
						fsh[*file].handleFiles.file.z = unzOpen(pak->pakFilename);
					
						if(fsh[*file].handleFiles.file.z == NULL)
							
							Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

							Com_Error(ERR_FATAL, "Couldn't open %s", pak->pakFilename);
					}
					else
						fsh[*file].handleFiles.file.z = pak->handle;

					Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
					fsh[*file].zipFile = qtrue;
				
					// set the file position in the zip file (also sets the current file info)
					unzSetOffset(fsh[*file].handleFiles.file.z, pakFile->pos);

					// open the file in the zip
					if(unzOpenCurrentFile(fsh[*file].handleFiles.file.z) != UNZ_OK)
					{
						Com_PrintError("FS_FOpenFileReadDir: Failed to open Zip-File\n");
					}
					fsh[*file].zipFilePos = pakFile->pos;

					Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
					
					if(fs_debug->integer)
					{
						Sys_Print(va("^4FS_FOpenFileRead: %s (found in '%s')\n", filename, pak->pakFilename));
					}

					err = unzGetCurrentFileInfo( fsh[*file].handleFiles.file.z, &file_info, netpath, sizeof( netpath ), NULL, 0, NULL, 0 );
					if(err != UNZ_OK)
					{
						Com_PrintWarning("Read error in Zip-file: %s\n", pak->pakFilename);
						return 1;
					}
					if(file_info.uncompressed_size)
					{
						return file_info.uncompressed_size;
					}
					return 1;
				}
			
				pakFile = pakFile->next;
			} while(pakFile != NULL);
		}
	}
	else if(search->dir)
	{
		// check a file in the directory tree

		dir = search->dir;

		FS_BuildOSPathForThread(dir->path, dir->gamedir, filename, netpath, FsThread);
		filep = fopen(netpath, "rb");

		if (filep == NULL)
		{
			*file = 0;
			Sys_LeaveCriticalSection(CRIT_FILESYSTEM);	
			return -1;
		}

		Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
		fsh[*file].zipFile = qfalse;
		
		if(fs_debug->integer)
		{
		Sys_Print(va("^4FS_FOpenFileRead: %s (found in '%s/%s')\n", filename, dir->path, dir->gamedir));
		}

		fsh[*file].handleFiles.file.o = filep;

		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

		return FS_fplength(filep);
	}
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

	return -1;
}

/*
===========
FS_FOpenFileRead

Finds the file in the search path.
Returns filesize and an open FILE pointer.
Used for streaming data out of either a
separate file or a ZIP file.
===========
*/
long FS_FOpenFileReadForThread(const char *filename, fileHandle_t *file, int fsThread)
{
	searchpath_t *search;
	long len;

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);

	
	if(!FS_Initialized())
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");
	
	for(search = fs_searchpaths; search; search = search->next)
	{
	        len = FS_FOpenFileReadDir(filename, search, file, 0, qfalse, fsThread);
	
	        if(file == NULL)
	        {
				if(len > 0)
				{
					Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
					return len;
				}
	        }
	        else
	        {
				if(len >= 0 && *file)
				{
					Sys_LeaveCriticalSection(CRIT_FILESYSTEM);					
					return len;
				}
	        }
	}

	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

#ifdef FS_MISSING
	if(missingFiles)
		fprintf(missingFiles, "%s\n", filename);
#endif

        if(file)
        	*file = 0;

	return -1;
}

long FS_FOpenFileRead(const char *filename, fileHandle_t *file)
{
    return FS_FOpenFileReadForThread(filename, file, 0);
}

long FS_FOpenFileReadThread1(const char *filename, fileHandle_t *file)
{
    return FS_FOpenFileReadForThread(filename, file, 1);
}

long FS_FOpenFileReadThread2(const char *filename, fileHandle_t *file)
{
    return FS_FOpenFileReadForThread(filename, file, 2);
}


/*
============
FS_Path_f

============
*/
void FS_Path_f( void ) {
	searchpath_t	*s;
	int				i;

	Com_Printf ("Current search path:\n");
	for (s = fs_searchpaths; s; s = s->next) {
		if (s->pack) {
			Com_Printf ("%s (%i files)\n", s->pack->pakFilename, s->pack->numfiles);
		} else {
			Com_Printf ("%s%c%s\n", s->dir->path, PATH_SEP, s->dir->gamedir );
		}
	}

	Com_Printf( "\n" );
	for ( i = 1 ; i < MAX_FILE_HANDLES ; i++ ) {
		if ( fsh[i].handleFiles.file.o ) {
			Com_Printf( "handle %i: %s\n", i, fsh[i].name );
		}
	}
}

/*
============
FS_Which
============
*/

qboolean FS_Which(const char *filename, void *searchPath)
{
	searchpath_t *search = searchPath;
	char netpath[MAX_OSPATH];

	if(FS_FOpenFileReadDir(filename, search, NULL, qfalse, qfalse, 0) > 0)
	{
		if(search->pack)
		{
			Com_Printf("File \"%s\" found in \"%s\"\n", filename, search->pack->pakFilename);
			return qtrue;
		}
		else if(search->dir)
		{
			FS_BuildOSPathForThread(search->dir->path, search->dir->gamedir, filename, netpath, 0);
			Com_Printf( "File \"%s\" found at \"%s\"\n", filename, netpath);
			return qtrue;
		}
	}

	return qfalse;
}

/*
============
FS_Which_f
============
*/
void FS_Which_f( void ) {
	searchpath_t	*search;
	char		*filename;

	filename = Cmd_Argv(1);

	if ( !filename[0] ) {
		Com_Printf( "Usage: which <file>\n" );
		return;
	}

	// qpaths are not supposed to have a leading slash
	if ( filename[0] == '/' || filename[0] == '\\' ) {
		filename++;
	}

	// just wants to see if file is there
	for(search = fs_searchpaths; search; search = search->next)
	{
		if(FS_Which(filename, search))
			return;
	}

	Com_Printf("File not found: \"%s\"\n", filename);
	return;
}


/*
================
FS_Dir_f
================
*/
void FS_Dir_f( void ) {
	char	*path;
	char	*extension;
	char	**dirnames;
	int		ndirs;
	int		i;

	if ( Cmd_Argc() < 2 || Cmd_Argc() > 3 ) {
		Com_Printf( "usage: dir <directory> [extension]\n" );
		return;
	}

	if ( Cmd_Argc() == 2 ) {
		path = Cmd_Argv( 1 );
		extension = "";
	} else {
		path = Cmd_Argv( 1 );
		extension = Cmd_Argv( 2 );
	}

	Com_Printf( "Directory of %s %s\n", path, extension );
	Com_Printf( "---------------\n" );

	dirnames = FS_ListFiles( path, extension, &ndirs );

	for ( i = 0; i < ndirs; i++ ) {
		Com_Printf( "%s\n", dirnames[i] );
	}
	FS_FreeFileList( dirnames );
}


int	FS_FTell( fileHandle_t f ) {
	int pos;
		pos = ftell(fsh[f].handleFiles.file.o);
	return pos;
}

void	FS_Flush( fileHandle_t f ) {
	fflush(fsh[f].handleFiles.file.o);
}


/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile( void *buffer ) {

	if ( !buffer ) {
		Com_Error( ERR_FATAL, "FS_FreeFile( NULL )" );
	}
	FS_LoadStackDec();

	free( buffer );
}

/*
 =============
 FS_FreeFileKeepBuf
 =============
 */
void FS_FreeFileKeepBuf( )
{
	
	FS_LoadStackDec();
}


/*
=================
FS_ReadLine
Custom function that only reads single lines
Properly handles line reads
=================
*/

int FS_ReadLine( void *buffer, int len, fileHandle_t f ) {
	char		*read;
	char		*buf;
	
	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}
	if ( !f ) {
		return 0;
	}

	if(fsh[f].zipFile == qtrue)
	{
		Com_Error( ERR_FATAL, "FS_ReadLine: Can not read ZIP-Files\n" );
	}

	buf = buffer;
        *buf = 0;
	read = fgets (buf, len, fsh[f].handleFiles.file.o);


	if (read == NULL) {	//Error

		if(feof(fsh[f].handleFiles.file.o)) return 0;
		Com_PrintError("FS_ReadLine: couldn't read");
		return -1;
	}
	return 1;
}


/*
===========
FS_SV_FOpenFileRead
search for a file somewhere below the home path, base path or cd path
we search in that order, matching FS_SV_FOpenFileRead order
===========
*/
int FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp ) {
	char ospath[MAX_OSPATH];
	fileHandle_t	f = 0;
	mvabuf;

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);


	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}


	f = FS_HandleForFile();
	if(f == 0){
		
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return 0;
	}
	FS_SetFilenameForHandle(f, filename);
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	// search homepath
	FS_BuildOSPathForThread( fs_homepath->string, filename, "", ospath, 0 );
	// remove trailing slash
	ospath[strlen(ospath)-1] = '\0';

	if ( fs_debug->integer ) {
		Sys_Print(va("FS_SV_FOpenFileRead (fs_homepath): %s\n", ospath ));
	}

	fsh[f].handleFiles.file.o = fopen( ospath, "rb" );
	fsh[f].handleSync = qfalse;

	if (!fsh[f].handleFiles.file.o){
	// NOTE TTimo on non *nix systems, fs_homepath == fs_basepath, might want to avoid
		if (Q_stricmp(fs_homepath->string,fs_basepath->string)){
		// search basepath
			FS_BuildOSPathForThread( fs_basepath->string, filename, "", ospath, 0 );

            ospath[strlen(ospath)-1] = '\0';
            if ( fs_debug->integer ){
				Sys_Print(va("FS_SV_FOpenFileRead (fs_basepath): %s\n", ospath ));
			}

            fsh[f].handleFiles.file.o = fopen( ospath, "rb" );
            fsh[f].handleSync = qfalse;

		}
	}
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

	if ( !fsh[f].handleFiles.file.o ){
		f = 0;
	}

	*fp = f;
	if (f) {
		return FS_filelength(f);
	}
	return 0;
}


/*
===========
FS_SV_FOpenFileAppend

===========
*/
fileHandle_t FS_SV_FOpenFileAppend( const char *filename ) {
	char ospath[MAX_OSPATH];
	fileHandle_t	f;
	mvabuf;

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);

	
	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	f = FS_HandleForFile();
	if(f == 0){
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return 0;
	}
	FS_SetFilenameForHandle(f, filename);
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	FS_BuildOSPathForThread( fs_basepath->string, filename, "", ospath, 0 );
	ospath[strlen(ospath)-1] = '\0';

	if ( fs_debug->integer ) {
		Sys_Print(va("FS_SV_FOpenFileAppend (fs_homepath): %s\n", ospath ));
	}

	if( FS_CreatePath( ospath ) ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return 0;
	}

	fsh[f].handleFiles.file.o = fopen( ospath, "ab" );
	fsh[f].handleSync = qfalse;

	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}


/*
=================
FS_Read

Properly handles partial reads
=================
*/


int FS_Read( void *buffer, int len, fileHandle_t f ) {
	int		block, remaining;
	int		read;
	byte	*buf;
	int		tries;

	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !f ) {
		return 0;
	}

	buf = (byte *)buffer;

	if (fsh[f].zipFile == qfalse) {
		remaining = len;
		tries = 0;
		while (remaining) {
			block = remaining;
			read = fread (buf, 1, block, fsh[f].handleFiles.file.o);
			if (read == 0)
			{
				// we might have been trying to read from a CD, which
				// sometimes returns a 0 read on windows
				if (!tries) {
					tries = 1;
				} else {
					return len-remaining;	//Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
				}
			}

			if (read == -1) {
				Com_Error (ERR_FATAL, "FS_Read: -1 bytes read");
			}

			remaining -= read;
			buf += read;
		}
		return len;
	} else {
		return unzReadCurrentFile(fsh[f].handleFiles.file.z, buffer, len);
	}
}




int FS_Read2( void *buffer, int len, fileHandle_t f ) {
	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !f ) {
		return 0;
	}
	if (fsh[f].streamed) {
		int r;
		fsh[f].streamed = qfalse;
		r = FS_Read( buffer, len, f );
		fsh[f].streamed = qtrue;
		return r;
	} else {
		return FS_Read( buffer, len, f);
	}
}


/*
=================
FS_Write

Properly handles partial writes
=================
*/
int FS_Write( const void *buffer, int len, fileHandle_t h ) {
	int		block, remaining;
	int		written;
	byte	*buf;
	int		tries;
	FILE	*f;

	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !h ) {
		return 0;
	}

	f = FS_FileForHandle(h);
	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while (remaining) {
		block = remaining;
		written = fwrite (buf, 1, block, f);
		if (written == 0) {
			if (!tries) {
				tries = 1;
			} else {
				Com_Printf( "FS_Write: 0 bytes written\n" );
				return 0;
			}
		}

		if (written == -1) {
			Com_Printf( "FS_Write: -1 bytes written\n" );
			return 0;
		}

		remaining -= written;
		buf += written;
	}
	if ( fsh[h].handleSync ) {
		fflush( f );
	}

	return len;
}




/*
============
FS_ReadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_ReadFile( const char *qpath, void **buffer ) {
	fileHandle_t	h;
	byte*			buf;
	int			len;

	
	
	if ( !qpath || !qpath[0] ) {
		Com_Error( ERR_FATAL, "FS_ReadFile with empty name\n" );
	}

	buf = NULL;	// quiet compiler warning

	// look for it in the filesystem or pack files
	len = FS_FOpenFileRead( qpath, &h );
	if ( h == 0 ) {
		if ( buffer ) {
			*buffer = NULL;
		}
		return -1;
	}
	
	if ( !buffer ) {
		FS_FCloseFile( h);
		return len;
	}

	FS_LoadStackInc();

	buf = malloc(len+1);
	*buffer = buf;

	FS_Read (buf, len, h);

	// guarantee that it will have a trailing 0 for string operations
	buf[len] = 0;
	FS_FCloseFile( h );
	return len;
}


/*
============
FS_SV_ReadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_SV_ReadFile( const char *qpath, void **buffer ) {
	fileHandle_t	h;
	byte*			buf;
	int			len;

	if ( !qpath || !qpath[0] ) {
		Com_Error( ERR_FATAL, "FS_ReadFile with empty name\n" );
	}

	buf = NULL;	// quiet compiler warning

	// look for it in the filesystem or pack files
	len = FS_SV_FOpenFileRead( qpath, &h );
	if ( h == 0 ) {
		if ( buffer ) {
			*buffer = NULL;
		}
		return -1;
	}
	
	if ( !buffer ) {
		FS_FCloseFile( h);
		return len;
	}

	FS_LoadStackInc();

	buf = malloc(len+1);
	*buffer = buf;

	FS_Read (buf, len, h);

	// guarantee that it will have a trailing 0 for string operations
	buf[len] = 0;
	FS_FCloseFile( h );
	return len;
}



/*
============
FS_WriteFile

Filename are reletive to the quake search path
============
*/
int FS_WriteFile( const char *qpath, const void *buffer, int size ) {
	fileHandle_t f;
	int len;

	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !qpath || !buffer ) {
		Com_Error( ERR_FATAL, "FS_WriteFile: NULL parameter" );
		return -1;
	}

	f = FS_FOpenFileWrite( qpath );
	if ( !f ) {
		Com_Printf( "Failed to open %s\n", qpath );
		return -1;
	}

	len = FS_Write( buffer, size, f );

	FS_FCloseFile( f );

	return len;

}




/*
===========
FS_SV_FOpenFileWrite

===========
*/

static fileHandle_t FS_SV_FOpenFileWriteGeneric( const char *filename, const char* basepath ) {
	char ospath[MAX_OSPATH];
	fileHandle_t	f;
	mvabuf;


	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	FS_BuildOSPathForThread( basepath, filename, "", ospath, 0 );
	FS_StripTrailingSeperator( ospath );

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);

	f = FS_HandleForFile();
	if(f == 0){
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return 0;
	}
	FS_SetFilenameForHandle(f, filename);
	fsh[f].zipFile = qfalse;

	if ( fs_debug->integer ) {
		Sys_Print(va("^4FS_SV_FOpenFileWrite: %s\n", ospath ));
	}

	if( FS_CreatePath( ospath ) ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return 0;
	}

	fsh[f].handleFiles.file.o = fopen( ospath, "wb" );
	fsh[f].handleSync = qfalse;

	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

fileHandle_t FS_SV_FOpenFileWrite( const char *filename )
{
    return FS_SV_FOpenFileWriteGeneric( filename, fs_homepath->string );
}


/*
============
FS_SV_WriteFile

Filename are reletive to the quake search path
============
*/

static int FS_SV_WriteFileGeneric( const char *qpath, const void *buffer, int size, const char* basepath) {
	fileHandle_t f;
	int len;

	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
		return -1;
	}

	if ( !qpath || !buffer ) {
		Com_Error( ERR_FATAL, "FS_WriteFile: NULL parameter" );
		return -1;
	}

	f = FS_SV_FOpenFileWriteGeneric( qpath, basepath );
	if ( !f ) {
		Com_Printf( "Failed to open %s\n", qpath );
		return -1;
	}

	len = FS_Write( buffer, size, f );

	FS_FCloseFile( f );

	return len;
}

int FS_SV_BaseWriteFile( const char *qpath, const void *buffer, int size)
{
    return FS_SV_WriteFileGeneric( qpath, buffer, size, fs_basepath->string);

}


int FS_SV_HomeWriteFile( const char *qpath, const void *buffer, int size)
{
    return FS_SV_WriteFileGeneric( qpath, buffer, size, fs_homepath->string);

}


void QDECL FS_Printf( fileHandle_t h, const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	FS_Write(msg, strlen(msg), h);
}





#define PK3_SEEK_BUFFER_SIZE 65536

/*
=================
FS_Seek

=================
*/
int FS_Seek( fileHandle_t f, long offset, int origin ) {
	int		_origin;

	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
		return -1;
	}

	if (fsh[f].streamed) {
		fsh[f].streamed = qfalse;
	 	FS_Seek( f, offset, origin );
		fsh[f].streamed = qtrue;
	}

	if (fsh[f].zipFile == qtrue) {
		//FIXME: this is incomplete and really, really
		//crappy (but better than what was here before)
		byte	buffer[PK3_SEEK_BUFFER_SIZE];
		int		remainder = offset;

		if( offset < 0 || origin == FS_SEEK_END ) {
			Com_Error( ERR_FATAL, "Negative offsets and FS_SEEK_END not implemented "
					"for FS_Seek on pk3 file contents" );
			return -1;
		}

		switch( origin ) {
			case FS_SEEK_SET:
				unzSetOffset(fsh[f].handleFiles.file.z, fsh[f].zipFilePos);
				if(unzOpenCurrentFile(fsh[f].handleFiles.file.z) != UNZ_OK)
				{
					Com_PrintError("FS_Seek: Failed to open zipfile\n");
					return -1;
				}
				//fallthrough

			case FS_SEEK_CUR:
				while( remainder > PK3_SEEK_BUFFER_SIZE ) {
					FS_Read( buffer, PK3_SEEK_BUFFER_SIZE, f );
					remainder -= PK3_SEEK_BUFFER_SIZE;
				}
				FS_Read( buffer, remainder, f );
				return offset;
				break;

			default:
				Com_Error( ERR_FATAL, "Bad origin in FS_Seek" );
				return -1;
				break;
		}
	} else {
		FILE *file;
		file = FS_FileForHandle(f);
		switch( origin ) {
		case FS_SEEK_CUR:
			_origin = SEEK_CUR;
			break;
		case FS_SEEK_END:
			_origin = SEEK_END;
			break;
		case FS_SEEK_SET:
			_origin = SEEK_SET;
			break;
		default:
			_origin = SEEK_CUR;
			Com_Error( ERR_FATAL, "Bad origin in FS_Seek" );
			break;
		}
		return fseek( file, offset, _origin );
	}
}


__cdecl const char* FS_GetBasepath(){

    if(fs_basepath && *fs_basepath->string){
        return fs_basepath->string;
    }else{
        return "";
    }
}


/*
=================
FS_SV_HomeCopyFile

Copy a fully specified file from one place to another
=================
*/
void FS_SV_HomeCopyFile( char *from, char *to ) {
	FILE	*f;
	int		len;
	byte	*buf;
	char	from_ospath[MAX_OSPATH];
	char	to_ospath[MAX_OSPATH];
	mvabuf;


        FS_BuildOSPathForThread( fs_homepath->string, from, "", from_ospath, 0 );
        FS_BuildOSPathForThread( fs_homepath->string, to, "", to_ospath, 0 );

	from_ospath[strlen(from_ospath)-1] = '\0';
	to_ospath[strlen(to_ospath)-1] = '\0';

	if ( fs_debug->integer ) {
		Sys_Print(va("FS_SVHomeCopyFile: %s --> %s\n", from_ospath, to_ospath ));
	}
	
	Sys_EnterCriticalSection(CRIT_FILESYSTEM);
	
	f = fopen( from_ospath, "rb" );
	if ( !f ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return;
	}
	fseek (f, 0, SEEK_END);
	len = ftell (f);
	fseek (f, 0, SEEK_SET);

	// we are using direct malloc instead of Z_Malloc here, so it
	// probably won't work on a mac... Its only for developers anyway...
	buf = malloc( len );
	if (fread( buf, 1, len, f ) != len)
		Com_Error( ERR_FATAL, "Short read in FS_Copyfiles()\n" );
	fclose( f );

	if( FS_CreatePath( to_ospath ) ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return;
	}

	f = fopen( to_ospath, "wb" );
	if ( !f ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return;
	}
	if (fwrite( buf, 1, len, f ) != len)
		Com_Error( ERR_FATAL, "Short write in FS_Copyfiles()\n" );
	fclose( f );
	free( buf );
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

}


// CVE-2006-2082
// compared requested pak against the names as we built them in FS_ReferencedPakNames
qboolean FS_VerifyPak( const char *pak ) {
	char teststring[ BIG_INFO_STRING ];
	searchpath_t    *search;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( search->pack ) {
			Q_strncpyz( teststring, search->pack->pakGamename, sizeof( teststring ) );
			Q_strcat( teststring, sizeof( teststring ), "/" );
			Q_strcat( teststring, sizeof( teststring ), search->pack->pakBasename );
			Q_strcat( teststring, sizeof( teststring ), ".iwd" );
			if ( !Q_stricmp( teststring, pak ) ) {
				return qtrue;
			}

		}

	}
	Com_sprintf(teststring, sizeof( teststring ), "%s/mod.ff", fs_gameDirVar->string);
	if ( !Q_stricmp( teststring, pak ) ){
		return qtrue;
	}

	if ( !Q_stricmpn( "usermaps/", pak, 9) ){

		if(strstr(pak, "..") || strstr(pak, ";"))
			return qfalse;
		else
			return qtrue;

	}

	return qfalse;
}



/*
==========================================================================

ZIP FILE LOADING

==========================================================================
*/



unsigned Com_BlockChecksumKey32T(void* buffer, int length, int key);

/*
=================
FS_LoadZipFile

Creates a new pak_t in the search chain for the contents
of a zip file.
=================
*/
static pack_t *FS_LoadZipFile( char *zipfile, const char *basename ) {
	fileInPack_t    *buildBuffer;
	pack_t          *pack;
	unzFile uf;
	int err;
	unz_global_info gi;
	char filename_inzip[MAX_ZPATH];
	unz_file_info file_info;
	int i, len;
	long hash;
	int fs_numHeaderLongs;
	int             *fs_headerLongs;
	char            *namePtr;

	fs_numHeaderLongs = 0;

	uf = unzOpen( zipfile );
	err = unzGetGlobalInfo( uf,&gi );

	if ( err != UNZ_OK ) {
		return NULL;
	}

	fs_packFiles += gi.number_entry;

	len = 0;
	unzGoToFirstFile( uf );
	for ( i = 0; i < gi.number_entry; i++ )
	{
		err = unzGetCurrentFileInfo( uf, &file_info, filename_inzip, sizeof( filename_inzip ), NULL, 0, NULL, 0 );
		if ( err != UNZ_OK ) {
			break;
		}
		len += strlen( filename_inzip ) + 1;
		unzGoToNextFile( uf );
	}


	buildBuffer = Z_Malloc( ( gi.number_entry * sizeof( fileInPack_t ) ) + len );
	namePtr = ( (char *) buildBuffer ) + gi.number_entry * sizeof( fileInPack_t );
	fs_headerLongs = Z_Malloc( gi.number_entry * sizeof( int ) );

	// get the hash table size from the number of files in the zip
	// because lots of custom pk3 files have less than 32 or 64 files
	for ( i = 1; i <= MAX_FILEHASH_SIZE; i <<= 1 ) {
		if ( i > gi.number_entry ) {
			break;
		}
	}

	pack = Z_Malloc( sizeof( pack_t ) + i * sizeof( fileInPack_t * ) );
	pack->hashSize = i;
	pack->hashTable = ( fileInPack_t ** )( ( (char *) pack ) + sizeof( pack_t ) );
	for ( i = 0; i < pack->hashSize; i++ ) {
		pack->hashTable[i] = NULL;
	}

	Q_strncpyz( pack->pakFilename, zipfile, sizeof( pack->pakFilename ) );
	Q_strncpyz( pack->pakBasename, basename, sizeof( pack->pakBasename ) );

	// strip .pk3 if needed
	if ( strlen( pack->pakBasename ) > 4 && !Q_stricmp( pack->pakBasename + strlen( pack->pakBasename ) - 4, ".iwd" ) ) {
		pack->pakBasename[strlen( pack->pakBasename ) - 4] = 0;
	}

	pack->handle = uf;
	pack->numfiles = gi.number_entry;
	pack->unk1 = 0;
	unzGoToFirstFile( uf );

	for ( i = 0; i < gi.number_entry; i++ )
	{
		err = unzGetCurrentFileInfo( uf, &file_info, filename_inzip, sizeof( filename_inzip ), NULL, 0, NULL, 0 );
		if ( err != UNZ_OK ) {
			break;
		}
		if ( file_info.uncompressed_size > 0 ) {
			fs_headerLongs[fs_numHeaderLongs++] = LittleLong( file_info.crc );
		}
		Q_strlwr( filename_inzip );
		hash = FS_HashFileName( filename_inzip, pack->hashSize );
		buildBuffer[i].name = namePtr;
		strcpy( buildBuffer[i].name, filename_inzip );
		namePtr += strlen( filename_inzip ) + 1;
		// store the file position in the zip
		buildBuffer[i].pos = unzGetOffset( uf );
		buildBuffer[i].next = pack->hashTable[hash];
		pack->hashTable[hash] = &buildBuffer[i];
		unzGoToNextFile( uf );
	}

	pack->checksum = Com_BlockChecksumKey32( fs_headerLongs, 4 * fs_numHeaderLongs, LittleLong( 0 ) );
	if(fs_checksumFeed)
		pack->pure_checksum = Com_BlockChecksumKey32( fs_headerLongs, 4 * fs_numHeaderLongs, LittleLong( fs_checksumFeed ) );
	else
		pack->pure_checksum = pack->checksum;
	// TTimo: DO_LIGHT_DEDICATED
	// curious about the size of those
	//Com_DPrintf("Com_BlockChecksumKey: %s %u\n", pack->pakBasename, 4 * fs_numHeaderLongs);
	// cumulated for light dedicated: 21558 bytes
	pack->checksum = LittleLong( pack->checksum );
	pack->pure_checksum = LittleLong( pack->pure_checksum );

	Z_Free( fs_headerLongs );

	pack->buildBuffer = buildBuffer;
	return pack;
}






void FS_CopyCvars()
{
    //SEH
    *(cvar_t**)0x13f9a1e0 = loc_language;
    *(cvar_t**)0x13f9a1e4 = loc_forceEnglish;
    *(cvar_t**)0x13f9a1e8 = loc_translate;
    *(cvar_t**)0x13f9a1ec = loc_warnings;
    *(cvar_t**)0x13f9a1f0 = loc_warningsAsErrors;
    //FS
    *(cvar_t**)0x13f9da00 = fs_debug;
    *(cvar_t**)0x13f9da14 = fs_copyfiles;
    *(cvar_t**)0x13f9da10 = fs_cdpath;
    *(cvar_t**)0x13f9da08 = fs_basepath;
    *(cvar_t**)0x13f9da0C = fs_basegame;
    *(cvar_t**)0x13f9da18 = fs_gameDirVar;
    *(cvar_t**)0x13f9da24 = fs_ignoreLocalized;
    *(cvar_t**)0x13f9da04 = fs_homepath;
    *(cvar_t**)0x13f9da1C = fs_restrict;
    *(cvar_t**)0x13f9da20 = fs_usedevdir;
}


void SEH_InitLanguage()
{
  loc_language = Cvar_RegisterInt("loc_language", 0, 0, 14, 0x21u, "Language");
  loc_forceEnglish = Cvar_RegisterBool("loc_forceEnglish", 0, 0x21u, "Force english localized strings");
  loc_translate = Cvar_RegisterBool("loc_translate", qtrue, 0x20u, "Enable translations");
  loc_warnings = Cvar_RegisterBool("loc_warnings", 0, 0, "Enable localization warnings");
  loc_warningsAsErrors = Cvar_RegisterBool("loc_warningsAsErrors", 0, 0,"Throw an error for any unlocalized string");
  g_currentAsian = (loc_language->integer - 8) <= 4;

}


void FS_InitFilesystem()
{
  Com_StartupVariable("fs_cdpath");
  Com_StartupVariable("fs_basepath");
  Com_StartupVariable("fs_homepath");
  Com_StartupVariable("fs_game");
  Com_StartupVariable("fs_copyfiles");
  Com_StartupVariable("fs_restrict");
  Com_StartupVariable("loc_language");
  SEH_InitLanguage();
  FS_Startup(BASEGAME);
//  _Z17SEH_Init_StringEdv();
//  _Z22SEH_UpdateLanguageInfov();
//  _Z18FS_SetRestrictionsv();
//  if ( FS_ReadFile("fileSysCheck.cfg", 0) <= 0 )
//    Com_Error(ERR_DROP, "Couldn't load %s.  Make sure Call of Duty is run from the correct folder.", "fileSysCheck.cfg");
  Q_strncpyz(lastValidBase, fs_basepath->string, sizeof(lastValidBase));
  Q_strncpyz(lastValidGame, fs_gameDirVar->string, sizeof(lastValidGame));

}




qboolean FS_GameDirDomainFunc(const char *cvar_name, const char *fs_gamestring)
{

  if ( !*fs_gamestring )
    return qtrue;
	
  if ( Q_stricmpn(fs_gamestring, "mods", 4) || strlen(fs_gamestring) < 6 ||
		(fs_gamestring[4] != '/' && fs_gamestring[4] != '\\') ||
		strstr(fs_gamestring, "..") ||
		strstr(fs_gamestring, "::"))
  {
    //LiveStoreage_ClearWriteFlag();
    Com_Error(ERR_DROP, "ERROR: Invalid server value '%s' for '%s'\n\n", fs_gamestring, cvar_name);
    return qfalse;
  }

  return qtrue;
}

void FS_GameCheckDir(cvar_t *var)
{
  if ( !FS_GameDirDomainFunc(var->name, var->string))
  {
    Com_Printf("'%s' is not a valid value for dvar '%s'\n\n", Cvar_DisplayableValue(var), var->name);
    Cvar_SetString(var, var->resetString);
  }
}


void FS_SetDirSep(cvar_t* fs_dir)
{
  int length;
  int i;
  qboolean flag;
  char buf[MAX_OSPATH];


  flag = qfalse;
  Q_strncpyz(buf, fs_dir->string, sizeof(buf));
  length = strlen(buf);

  if( length == 0 )
  {
    return;
  }

  for (i = 0; length >= i; i++)
  {
    if ( buf[i] == '\\' )
    {
        buf[i] = '/';
        flag = qtrue;
    }
    if ( Q_isupper(buf[i]) )
    {
        buf[i] = tolower(buf[i]);
        flag = qtrue;
    }
  }
  if(buf[length -1] == '/')
  {
    buf[length -1] = '\0';
    flag = qtrue;
  }
  if ( flag )
      Cvar_SetString(fs_dir, buf);
}

void FS_AddGameDirectory_Single(const char *path, const char *dir_nolocal, qboolean last, int index);

void FS_AddGameDirectory(const char *path, const char *dir)
{
  signed int i;

  for(i = 14; i >= 0; i--)
    FS_AddGameDirectory_Single(path, dir, qtrue, i--);

  FS_AddGameDirectory_Single(path, dir, 0, 0);
}

static char* g_languages[] = {"english", "french", "german", "italian",
                              "spanish", "british", "russian", "polish",
                              "korean", "taiwanese", "japanese", "chinese",
                              "thai", "leet", "czech"};

int SEH_GetCurrentLanguage(void)
{
	return loc_language->integer;
}

const char* SEH_GetLanguageName(unsigned int langindex)
{
    if(langindex > 14)
        return g_languages[0];

    return g_languages[langindex];
}

qboolean SEH_GetLanguageIndexForName(const char* language, int *langindex)
{
    int i;

    for(i = 0; i < 15; i++)
    {
        if(!Q_stricmp(language, g_languages[i]))
        {
            *langindex = i;
            return qtrue;
        }
    }
    return qfalse;
}

/*
=================
FS_ShutdownSearchpath

Shuts down and clears a single searchpath only
=================
*/

void FS_ShutdownSearchpath(searchpath_t *clear)
{
	searchpath_t    **back, *p;
	
	back = &fs_searchpaths;
	while ( 1 )
	{	
		p = *back;
		if( p == NULL )
		{
			return;
		}
		if(p == clear)
		{
			*back = p->next;
			if ( p->pack ) {
				unzClose( p->pack->handle );
				Z_Free( p->pack->buildBuffer );
				Z_Free( p->pack );
			}
			if ( p->dir ) {
				Z_Free( p->dir );
			}
			Z_Free( p );
			return;
		}
		back = &p->next;
	}
}



void FS_DisplayPath( void ) {
	searchpath_t    *s;
	int i;

	Com_Printf("Current language: %s\n", SEH_GetLanguageName(SEH_GetCurrentLanguage()));
	Com_Printf("Current fs_basepath: %s\n", fs_basepath->string);
	Com_Printf("Current fs_homepath: %s\n", fs_homepath->string);
	if ( fs_ignoreLocalized->integer)
		Com_Printf("    localized assets are being ignored\n");

	Com_Printf( "Current search path:\n" );
	for ( s = fs_searchpaths; s; s = s->next )
	{
		if ( s->pack )
		{
			Com_Printf( "%s (%i files)\n", s->pack->pakFilename, s->pack->numfiles );
			if ( s->localized )
			{
			    Com_Printf("    localized assets iwd file for %s\n", SEH_GetLanguageName(s->langIndex));
			}
//			if ( fs_numServerPaks ) {
//				if ( !FS_PakIsPure( s->pack ) ) {
//					Com_Printf( "    not on the pure list\n" );
//				} else {
//					Com_Printf( "    on the pure list\n" );
//				}
//			}
		} else {
			Com_Printf( "%s/%s\n", s->dir->path, s->dir->gamedir );
			if ( s->localized )
			{
				Com_Printf("    localized assets game folder for %s\n", SEH_GetLanguageName(s->langIndex));
			}
		}
	}
	Com_Printf("\nFile Handles:\n");
	for ( i = 1 ; i < MAX_FILE_HANDLES ; i++ ) {
		if ( fsh[i].handleFiles.file.o ) {
			Com_Printf( "handle %i: %s\n", i, fsh[i].name );
		}
	}
}

void FS_Startup(const char* gameName)
{

  char* homePath;
  cvar_t *levelname;
  mvabuf;

  Sys_EnterCriticalSection(CRIT_FILESYSTEM);

  Com_Printf("----- FS_Startup -----\n");
  fs_debug = Cvar_RegisterInt("fs_debug", 0, 0, 2, 0, "Enable file system debugging information");
  fs_copyfiles = Cvar_RegisterBool("fs_copyfiles", 0, 16, "Copy all used files to another location");
  fs_cdpath = Cvar_RegisterString("fs_cdpath", Sys_DefaultCDPath(), 16, "CD path");
  fs_basepath = Cvar_RegisterString("fs_basepath", Sys_DefaultInstallPath(), 528, "Base game path");
  fs_basegame = Cvar_RegisterString("fs_basegame", "", 16, "Base game name");
  fs_gameDirVar = Cvar_RegisterString("fs_game", "", 28, "Game data directory. Must be \"\" or a sub directory of 'mods/'.");
  fs_ignoreLocalized = Cvar_RegisterBool("fs_ignoreLocalized", qfalse, 160, "Ignore localized files");

  fs_packFiles = 0;

  homePath = (char*)Sys_DefaultHomePath();
  if ( !homePath || !homePath[0] )
    homePath = fs_basepath->resetString;
  fs_homepath = Cvar_RegisterString("fs_homepath", homePath, 528, "Game home path");
  fs_restrict = Cvar_RegisterBool("fs_restrict", qfalse, 16, "Restrict file access for demos etc.");
  fs_usedevdir = Cvar_RegisterBool("fs_usedevdir", qfalse, 16, "Use development directories.");

  levelname = Cvar_FindVar("mapname");

  FS_SetDirSep(fs_homepath);
  FS_SetDirSep(fs_basepath);
  FS_SetDirSep(fs_gameDirVar);
  FS_GameCheckDir(fs_gameDirVar);


  if( fs_basepath->string[0] )
  {
    if( fs_usedevdir->string )
    {
      FS_AddGameDirectory(fs_basepath->string, "devraw_shared");
      FS_AddGameDirectory(fs_basepath->string, "devraw");
      FS_AddGameDirectory(fs_basepath->string, "raw_shared");
      FS_AddGameDirectory(fs_basepath->string, "raw");
    }
    FS_AddGameDirectory(fs_basepath->string, "players");
  }

  if ( fs_homepath->string[0] && Q_stricmp(fs_basepath->string, fs_homepath->string) && fs_usedevdir->string)
  {
    FS_AddGameDirectory(fs_homepath->string, "devraw_shared");
    FS_AddGameDirectory(fs_homepath->string, "devraw");
    FS_AddGameDirectory(fs_homepath->string, "raw_shared");
    FS_AddGameDirectory(fs_homepath->string, "raw");
  }

  if ( fs_cdpath->string[0] && Q_stricmp(fs_basepath->string, fs_cdpath->string) )
  {
    if ( fs_usedevdir->string )
    {
      FS_AddGameDirectory(fs_cdpath->string, "devraw_shared");
      FS_AddGameDirectory(fs_cdpath->string, "devraw");
      FS_AddGameDirectory(fs_cdpath->string, "raw_shared");
      FS_AddGameDirectory(fs_cdpath->string, "raw");
    }
    FS_AddGameDirectory(fs_cdpath->string, gameName);
  }

  if ( fs_basepath->string[0] )
  {
    FS_AddGameDirectory(fs_basepath->string, va("%s_shared", gameName));
    FS_AddGameDirectory(fs_basepath->string, gameName);
  }

  if ( fs_basepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string) )
  {
    FS_AddGameDirectory(fs_basepath->string, va("%s_shared", gameName));
    FS_AddGameDirectory(fs_homepath->string, gameName);
  }



  if ( fs_basegame->string[0] && !Q_stricmp(gameName, BASEGAME) && Q_stricmp(fs_basegame->string, gameName) )
  {
    if ( fs_cdpath->string[0] )
      FS_AddGameDirectory(fs_cdpath->string, fs_basegame->string);
    if ( fs_basepath->string[0] )
      FS_AddGameDirectory(fs_basepath->string, fs_basegame->string);
    if ( fs_homepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string) )
      FS_AddGameDirectory(fs_homepath->string, fs_basegame->string);
  }
	
  if ( fs_gameDirVar->string[0] && !Q_stricmp(gameName, BASEGAME) && Q_stricmp(fs_gameDirVar->string, gameName) && levelname && levelname->string[0])
  {
	if ( fs_cdpath->string[0] )
		FS_AddGameDirectory(fs_cdpath->string, va("usermaps/%s", levelname->string));
	if ( fs_basepath->string[0] )
		FS_AddGameDirectory(fs_basepath->string, va("usermaps/%s", levelname->string));
	if ( fs_homepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string) )
		FS_AddGameDirectory(fs_homepath->string, va("usermaps/%s", levelname->string));
  }

  if ( fs_gameDirVar->string[0] && !Q_stricmp(gameName, BASEGAME) && Q_stricmp(fs_gameDirVar->string, gameName) )
  {
    if ( fs_cdpath->string[0] )
      FS_AddGameDirectory(fs_cdpath->string, fs_gameDirVar->string);
    if ( fs_basepath->string[0] )
      FS_AddGameDirectory(fs_basepath->string, fs_gameDirVar->string);
    if ( fs_homepath->string[0] && Q_stricmp(fs_homepath->string, fs_basepath->string) )
      FS_AddGameDirectory(fs_homepath->string, fs_gameDirVar->string);
  }

/*  Com_ReadCDKey(); */
  Cmd_AddCommand("path", FS_Path_f);
  Cmd_AddCommand("which", FS_Which_f);
/*  Cmd_AddCommand("dir", FS_Dir_f ); */
  FS_DisplayPath();
/*  Cvar_ClearModified(fs_gameDirVar);*/
  fs_gameDirVar->modified = 0;
  Com_Printf("----------------------\n");
  Com_Printf("%d files in iwd files\n", fs_packFiles);

  Sys_LeaveCriticalSection(CRIT_FILESYSTEM);


    PHandler_Event(PLUGINS_ONFSSTARTED, fs_searchpaths);

}

void FS_AddIwdFilesForGameDirectory(const char *path, const char *dir);

void FS_AddGameDirectory_Single(const char *path, const char *dir_nolocal, qboolean localized, int index)
{
  const char* localization;
  searchpath_t *search;
  searchpath_t *sp;
  searchpath_t *prev;
  const char *language_name;
  char ospath[MAX_OSPATH];
  char dir[MAX_QPATH];


	if ( localized )
	{
		language_name = SEH_GetLanguageName(index);
		Com_sprintf(dir, sizeof(dir), "%s/%s", dir_nolocal, language_name);
	}
	else
	{
		Q_strncpyz(dir, dir_nolocal, sizeof(dir));
	}

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);

	for (sp = fs_searchpaths ; sp ; sp = sp->next)
	{
		if ( sp->dir && !Q_stricmp(sp->dir->path, path) && !Q_stricmp(sp->dir->gamedir, dir) )
		{
			if ( localized != sp->localized )
			{
				localization = "localized";
				if ( !sp->localized )
					localization = "non-localized";
				Com_PrintWarning("WARNING: game folder %s/%s added as both localized & non-localized. Using folder as %s\n", path, dir, localization);
			}
			if ( sp->localized && index != sp->localized )
				Com_PrintWarning( "WARNING: game folder %s/%s re-added as localized folder with different language\n", path, dir);

			Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
			return;
		}
	}

    if ( localized )
    {

      FS_BuildOSPathForThread(path, dir, "", ospath, 0);
      if(ospath[0])
          ospath[strlen(ospath) -1] = 0;
		if ( !Sys_DirectoryHasContent(ospath) )
		{
			Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
			return;
		}
    }
    else
    {
      Q_strncpyz(fs_gamedir, dir, 256);
    }
    search = (searchpath_t *)Z_Malloc(sizeof(searchpath_t));
    search->dir = (directory_t *)Z_Malloc(sizeof(directory_t));
    Q_strncpyz(search->dir->path, path, sizeof(search->dir->path));
    Q_strncpyz(search->dir->gamedir, dir, sizeof(search->dir->gamedir));
    search->localized = localized;
    search->langIndex = index;
    search->val_2 = Q_stricmp(dir_nolocal, BASEGAME) == 0;
    search->val_3 = Q_stricmp(dir_nolocal, "players") == 0;

    prev = (searchpath_t*)&fs_searchpaths;
    sp = fs_searchpaths;

    if (search->localized)
    {
        for (sp = fs_searchpaths; sp != NULL && !sp->localized; sp = sp->next)
        {
            prev = sp;
        }
    }
    search->next = sp;
    prev->next = search;
    FS_AddIwdFilesForGameDirectory(path, dir);
	
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

}


static const char* sub_55D700(const char *instr)
{
  signed int i;
  static qboolean flip;
  static char Array64[128];

  flip ^= 1u;
  if ( strlen(instr) >= 10 )
  {
    Com_Memset(&Array64[64 * flip], 0, 64);
	for(i = 10; i < 64 && instr[i] != '\0' && isalpha(instr[i]) != '\0'; i++)
	{
		Array64[(64 * flip) + i - 10] = instr[i];
    }
    return &Array64[64 * flip];
  }

  Array64[64 * flip] = 0;
  return &Array64[64 * flip];
}

static signed int iwdsort(const void *cmp1_arg, const void *cmp2_arg)
{
  const char* cmp1;
  const char* cmp2;

  cmp1 = *(const char**)cmp1_arg;
  cmp2 = *(const char**)cmp2_arg;

  if(Q_stricmpn(cmp1, "          ", 10) || Q_stricmpn(cmp2, "          ", 10) )
        return FS_PathCmp(cmp1, cmp2);

  if ( Q_stricmp( sub_55D700(cmp1), "english") )
  {
    if ( !Q_stricmp( sub_55D700(cmp2), "english") )
      return 1;
  }
  else
  {
    if ( Q_stricmp( sub_55D700(cmp2), "english") )
      return -1;
  }
  return FS_PathCmp(cmp1, cmp2);
}


#define MAX_PAKFILES 1024
void FS_AddIwdFilesForGameDirectory(const char *path, const char *dir)
{

  searchpath_t *search, *prev, *sp;
  int langindex;
  int numfiles;
  const char* language;
  qboolean islocalized;
  int i, j;
  pack_t* pak;
  char** pakfiles;
  char pakfile[MAX_OSPATH];
  char* sorted[MAX_PAKFILES];
  qboolean languagesListed;

  FS_BuildOSPathForThread(path, dir, "", pakfile, 0);
  pakfile[strlen(pakfile) - 1] = 0;
  pakfiles = Sys_ListFiles(pakfile, "iwd", 0, &numfiles, 0);

  if(!pakfiles)
    return;

  if ( numfiles > MAX_PAKFILES )
  {
    Com_PrintWarning("WARNING: Exceeded max number of iwd files in %s/%s (%1/%1)\n", path, dir, numfiles, MAX_PAKFILES);
    numfiles = MAX_PAKFILES;
  }
  if ( !numfiles && !Q_stricmp(dir, BASEGAME) && !Q_stricmp(path, fs_basepath->string) )
    Com_Error(ERR_FATAL, "No IWD files found in /main");

	
  for(i = 0; i < numfiles; i++)
  {	
	sorted[i] = pakfiles[i];
	
	if ( !Q_strncmp(sorted[i], "localized_", 10) )
	{
		Com_Memcpy(sorted[i],  "          ", 10);
	}
  }

  qsort(sorted, numfiles, 4, iwdsort);

  langindex = 0;
  languagesListed = 0;

	for(i = 0; i < numfiles; i++)
	{
		if(!Q_strncmp(sorted[i], "          ", 10))
		{
			Com_Memcpy(sorted[i],  "localized_", 10);
			language = sub_55D700(sorted[i]);
			if ( !language[0] )
			{
				Com_PrintWarning("WARNING: Localized assets iwd file %s/%s/%s has invalid name (no language specified). Proper naming convention is: localized_[language]_iwd#.iwd\n", path, dir, sorted[i]);
				continue;
			}
			if ( !SEH_GetLanguageIndexForName(language, &langindex))
			{
				Com_PrintWarning("WARNING: Localized assets iwd file %s/%s/%s has invalid name (bad language name specified). Proper naming convention is: localized_[language]_iwd#.iwd\n", path, dir, sorted[i]);
			  if ( !languagesListed )
			  {
				Com_Printf("Supported languages are:\n");
				for(j = 0; j < 15; j++)
				{
					Com_Printf("    %s\n", SEH_GetLanguageName(j));
				}
				languagesListed = 1;
			  }
			  continue;
			}
			islocalized = qtrue;
		}else{
		    if ( !Q_stricmp(dir, BASEGAME) && !Q_stricmp(path, fs_basepath->string) && Q_stricmpn(sorted[i], "iw_", 3) && Q_stricmpn(sorted[i], "xiceops_", 7))
			{
				Com_PrintWarning("WARNING: Invalid IWD %s in \\main.\n", sorted[i]);
				continue;
			}
			islocalized = qfalse;
		}

		FS_BuildOSPathForThread(path, dir, sorted[i], pakfile, 0);
		pak = FS_LoadZipFile( pakfile, sorted[i]);
		if(pak == NULL)
		{
			continue;
		}
		/* Shutdown already loaded pak files with same name to circumvent conflicts */

		for(sp = fs_searchpaths; sp != NULL; sp = sp->next)
		{
			if(sp->pack != NULL && !Q_stricmp(sp->pack->pakBasename, pak->pakBasename))
			{
				FS_ShutdownSearchpath(sp);
				break; //Break out - sp is now invalid
			}	
		}
		
		Q_strncpyz(pak->pakGamename, dir, sizeof(pak->pakGamename));
		
		search = (searchpath_t *)Z_Malloc(sizeof(searchpath_t));
		search->pack = pak;
		search->localized = islocalized;
		search->langIndex = langindex;

		prev = (searchpath_t*)&fs_searchpaths;
		sp = fs_searchpaths;

		if (search->localized)
		{
			for (sp = fs_searchpaths; sp != NULL && !sp->localized; sp = sp->next)
			{
				prev = sp;
			}
			Cvar_SetInt(loc_language, search->langIndex);
		}
		search->next = sp;
		prev->next = search;
	}
/*	Sys_FreeFileList(sorted); */
}


unsigned Com_BlockChecksumKey32(void* buffer, int length, int key)
{
        int i, j;
        unsigned int q = ~key;
        byte* val = buffer;

        for(i = 0; i < length; i++)
        {
            q = val[i] ^ q;

            for(j = 0; j < 8; j++)
            {
                if(q & 1)
                    q = (q >> 1) ^ 0xEDB88320;
                else
                    q = (q >> 1) ^ 0;
            }
        }
        return ~q;
}







void FS_PatchFileHandleData()
{
	/* Copy the our fsh handle */
	*(fileHandleData_t**)0x8128977 = fsh;
	*(fileHandleData_t**)0x81289FA = fsh;
	*(fileHandleData_t**)0x8128A66 = fsh;
	*(fileHandleData_t**)0x8128AED = fsh;
	*(fileHandleData_t**)0x8128E8C = fsh;
	*(fileHandleData_t**)0x8128EC0 = fsh;
	*(fileHandleData_t**)0x81865A3 = fsh;
	*(fileHandleData_t**)0x8186955 = fsh;
	*(fileHandleData_t**)0x8186963 = fsh;
	*(fileHandleData_t**)0x8186A02 = fsh;
	*(fileHandleData_t**)0x8186EE2 = fsh;
	*(fileHandleData_t**)0x8186FA5 = fsh;
	*(fileHandleData_t**)0x8186FBB = fsh;
	*(fileHandleData_t**)0x81870E8 = fsh;
	*(fileHandleData_t**)0x81870F4 = fsh;
	*(fileHandleData_t**)0x8187299 = fsh;
	*(fileHandleData_t**)0x81872DA = fsh;
	*(fileHandleData_t**)0x8187304 = fsh;
	*(fileHandleData_t**)0x818731B = fsh;
	*(fileHandleData_t**)0x818732C = fsh;
	*(fileHandleData_t**)0x8187451 = fsh;
	*(fileHandleData_t**)0x818747E = fsh;
	*(fileHandleData_t**)0x818748E = fsh;
	*(fileHandleData_t**)0x8187619 = fsh;
	*(fileHandleData_t**)0x8187627 = fsh;
	*(fileHandleData_t**)0x818765C = fsh;
	*(fileHandleData_t**)0x818768E = fsh;
	*(fileHandleData_t**)0x81876BC = fsh;
	*(fileHandleData_t**)0x8187703 = fsh;
	*(fileHandleData_t**)0x8187733 = fsh;
	*(fileHandleData_t**)0x8187741 = fsh;
	*(fileHandleData_t**)0x818775D = fsh;
	*(fileHandleData_t**)0x8187775 = fsh;
	*(fileHandleData_t**)0x8187797 = fsh;
	*(fileHandleData_t**)0x81877A8 = fsh;
	*(fileHandleData_t**)0x81877E4 = fsh;
	*(fileHandleData_t**)0x81877F8 = fsh;
	*(fileHandleData_t**)0x818780B = fsh;
	*(fileHandleData_t**)0x818782B = fsh;
	*(fileHandleData_t**)0x818783C = fsh;
	*(fileHandleData_t**)0x8188DFD = fsh;
	*(fileHandleData_t**)0x8188E98 = fsh;
	*(fileHandleData_t**)0x818A21C = fsh;
	*(fileHandleData_t**)0x818A22A = fsh;
	*(fileHandleData_t**)0x818A2D2 = fsh;
	*(fileHandleData_t**)0x818A350 = fsh;
	*(fileHandleData_t**)0x818A47F = fsh;
	*(fileHandleData_t**)0x818A5BF = fsh;
	*(fileHandleData_t**)0x818A71C = fsh;
	*(fileHandleData_t**)0x818A72A = fsh;
	*(fileHandleData_t**)0x818A7CB = fsh;
	*(fileHandleData_t**)0x818A950 = fsh;
	*(fileHandleData_t**)0x818AB46 = fsh;
	*(fileHandleData_t**)0x818AB75 = fsh;
	*(fileHandleData_t**)0x818ACF0 = fsh;
	*(fileHandleData_t**)0x818ACFE = fsh;
	*(fileHandleData_t**)0x818ADA6 = fsh;
	*(fileHandleData_t**)0x818B0D0 = fsh;
	*(fileHandleData_t**)0x818B0E7 = fsh;
	*(fileHandleData_t**)0x818B1C0 = fsh;
	*(fileHandleData_t**)0x818B233 = fsh;
	*(fileHandleData_t**)0x818B28C = fsh;
	*(fileHandleData_t**)0x818B2ED = fsh;
	*(fileHandleData_t**)0x818B4A6 = fsh;
	*(fileHandleData_t**)0x818B4BD = fsh;
	*(fileHandleData_t**)0x818B8FF = fsh;
	*(fileHandleData_t**)0x818BC49 = fsh;
	*(fileHandleData_t**)0x818BC5A = fsh;
	*(fileHandleData_t**)0x818BDE8 = fsh;

	*(qboolean**)0x8187288 = &fsh[0].handleFiles.unique;
	*(qboolean**)0x81872A7 = &fsh[0].handleFiles.unique;
	*(qboolean**)0x818B212 = &fsh[0].handleFiles.unique;
	*(qboolean**)0x818B479 = &fsh[0].handleFiles.unique;

	*(qboolean**)0x812897E = &fsh[0].handleSync;
	*(qboolean**)0x8128A6D = &fsh[0].handleSync;
	*(qboolean**)0x8128AF4 = &fsh[0].handleSync;
	*(qboolean**)0x8128EB5 = &fsh[0].handleSync;
	*(qboolean**)0x8186F49 = &fsh[0].handleSync;
	*(qboolean**)0x8188E5F = &fsh[0].handleSync;
	*(qboolean**)0x8188EFF = &fsh[0].handleSync;
	*(qboolean**)0x818A376 = &fsh[0].handleSync;
	*(qboolean**)0x818A55C = &fsh[0].handleSync;
	*(qboolean**)0x818A69C = &fsh[0].handleSync;
	*(qboolean**)0x818A957 = &fsh[0].handleSync;
	*(qboolean**)0x818AB6A = &fsh[0].handleSync;
	*(qboolean**)0x818BB1C = &fsh[0].handleSync;
	*(qboolean**)0x818BE0B = &fsh[0].handleSync;

	*(int**)0x818BAF6 = &fsh[0].fileSize;

	*(int**)0x818760F = &fsh[0].zipFilePos;
	*(int**)0x8187729 = &fsh[0].zipFilePos;
	*(int**)0x818778D = &fsh[0].zipFilePos;
	*(int**)0x8187821 = &fsh[0].zipFilePos;
	*(int**)0x818B314 = &fsh[0].zipFilePos;

	*(qboolean**)0x81288ED = &fsh[0].zipFile;
	*(qboolean**)0x8128E36 = &fsh[0].zipFile;
	*(qboolean**)0x8186F91 = &fsh[0].zipFile;
	*(qboolean**)0x81870DD = &fsh[0].zipFile;
	*(qboolean**)0x8187473 = &fsh[0].zipFile;
	*(qboolean**)0x81875E8 = &fsh[0].zipFile;
	*(qboolean**)0x818767F = &fsh[0].zipFile;
	*(qboolean**)0x81876AD = &fsh[0].zipFile;
	*(qboolean**)0x818774F = &fsh[0].zipFile;
	*(qboolean**)0x818A33F = &fsh[0].zipFile;
	*(qboolean**)0x818A829 = &fsh[0].zipFile;
	*(qboolean**)0x818A9A6 = &fsh[0].zipFile;
	*(qboolean**)0x818B147 = &fsh[0].zipFile;
	*(qboolean**)0x818B1B1 = &fsh[0].zipFile;
	*(qboolean**)0x818B27B = &fsh[0].zipFile;
	*(qboolean**)0x818BC33 = &fsh[0].zipFile;
	*(qboolean**)0x818BDD7 = &fsh[0].zipFile;

	*(qboolean**)0x818BB07 = &fsh[0].streamed;

	*(char**)0x8128905 = fsh[0].name;
	*(char**)0x8128EA6 = fsh[0].name;
	*(char**)0x8186A0C = fsh[0].name;
	*(char**)0x818A2DC = fsh[0].name;
	*(char**)0x818A367 = fsh[0].name;
	*(char**)0x818A7D5 = fsh[0].name;
	*(char**)0x818A841 = fsh[0].name;
	*(char**)0x818AB5B = fsh[0].name;
	*(char**)0x818ADB0 = fsh[0].name;
	*(char**)0x818B128 = fsh[0].name;
	*(char**)0x818B256 = fsh[0].name;
	*(char**)0x818BDFC = fsh[0].name;

	/* 2nd element of fsh */

	*(fileHandleData_t**)0x81869A0 = &fsh[1];
	*(fileHandleData_t**)0x8187B9E = &fsh[1];
	*(fileHandleData_t**)0x818A26D = &fsh[1];
	*(fileHandleData_t**)0x818A766 = &fsh[1];
	*(fileHandleData_t**)0x818A993 = &fsh[1];
	*(fileHandleData_t**)0x818AA47 = &fsh[1];
	*(fileHandleData_t**)0x818AA97 = &fsh[1];
	*(fileHandleData_t**)0x818AD41 = &fsh[1];
	*(fileHandleData_t**)0x818BDC0 = &fsh[1];
	*(fileHandleData_t**)0x818BE4B = &fsh[1];
	*(fileHandleData_t**)0x818BE9B = &fsh[1];

	*(int**)0x8187360 = &fsh[1].fileSize;
	*(int**)0x818E766 = &fsh[1].fileSize;

	*(int**)0x818699A = &fsh[1].zipFilePos;
	*(int**)0x8187B98 = &fsh[1].zipFilePos;
	*(int**)0x818A267 = &fsh[1].zipFilePos;
	*(int**)0x818A760 = &fsh[1].zipFilePos;
	*(int**)0x818AA41 = &fsh[1].zipFilePos;
	*(int**)0x818AD3B = &fsh[1].zipFilePos;
	*(int**)0x818BE45 = &fsh[1].zipFilePos;

	*(char**)0x818AAA3 = fsh[1].name;
	*(char**)0x818BEA7 = fsh[1].name;

	/* Done with fsh patch */

	*(searchpath_t***)0x81280DA = &fs_searchpaths;
	*(searchpath_t***)0x8128297 = &fs_searchpaths;
	*(searchpath_t***)0x8128402 = &fs_searchpaths;
	*(searchpath_t***)0x8128477 = &fs_searchpaths;
	*(searchpath_t***)0x812857E = &fs_searchpaths;
	*(searchpath_t***)0x8129018 = &fs_searchpaths;
	*(searchpath_t***)0x8129657 = &fs_searchpaths;
	*(searchpath_t***)0x81864E7 = &fs_searchpaths;
	*(searchpath_t***)0x818656B = &fs_searchpaths;
	*(searchpath_t***)0x818663E = &fs_searchpaths;
	*(searchpath_t***)0x818737F = &fs_searchpaths;
	*(searchpath_t***)0x818738A = &fs_searchpaths;
	*(searchpath_t***)0x8187ABC = &fs_searchpaths;
	*(searchpath_t***)0x8187CDE = &fs_searchpaths;
	*(searchpath_t***)0x8188826 = &fs_searchpaths;
	*(searchpath_t***)0x8188B18 = &fs_searchpaths;
	*(searchpath_t***)0x8188B97 = &fs_searchpaths;
	*(searchpath_t***)0x8188CCA = &fs_searchpaths;
	*(searchpath_t***)0x8189690 = &fs_searchpaths;
	*(searchpath_t***)0x818969C = &fs_searchpaths;
	*(searchpath_t***)0x818998A = &fs_searchpaths;
	*(searchpath_t***)0x8189B41 = &fs_searchpaths;
	*(searchpath_t***)0x8189B4D = &fs_searchpaths;
	*(searchpath_t***)0x8189BCF = &fs_searchpaths;
	*(searchpath_t***)0x818AE36 = &fs_searchpaths;
	*(searchpath_t***)0x818B6BF = &fs_searchpaths;
	*(searchpath_t***)0x818E785 = &fs_searchpaths;
	*(searchpath_t***)0x818E790 = &fs_searchpaths;
	*(searchpath_t***)0x818E7A4 = &fs_searchpaths;

	*(char**)0x8186B14 = fs_gamedir;
	*(char**)0x818790F = fs_gamedir;
	*(char**)0x818799C = fs_gamedir;
	*(char**)0x8187A37 = fs_gamedir;
	*(char**)0x8189BB4 = fs_gamedir;
	*(char**)0x818A3EC = fs_gamedir;
	*(char**)0x818A431 = fs_gamedir;
	*(char**)0x818A50B = fs_gamedir;
	*(char**)0x818A59E = fs_gamedir;
	*(char**)0x818A64B = fs_gamedir;
	*(char**)0x818A86A = fs_gamedir;
	*(char**)0x818A9CB = fs_gamedir;
	*(char**)0x818BB2C = fs_gamedir;
	*(char**)0x819C381 = fs_gamedir;
	*(char**)0x819C64D = fs_gamedir;
	*(char**)0x819C679 = fs_gamedir;
	*(char**)0x819CEFD = fs_gamedir;
	*(char**)0x819CF27 = fs_gamedir;
	*(char**)0x819D09A = fs_gamedir;

	*(int**)0x818655E = &fs_loadStack;
	*(int**)0x81865FD = &fs_loadStack;
	*(int**)0x8187435 = &fs_loadStack;
	*(int**)0x818BBCE = &fs_loadStack;

	*(int**)0x818819d = &fs_numServerIwds;
	*(int**)0x81885fc = &fs_numServerIwds;

	*(int**)0x81881b8 = fs_serverIwds;
	*(int**)0x81881c7 = fs_serverIwds;

}



/*
================
FS_Shutdown

Frees all resources and closes all files
================
*/
void FS_Shutdown( qboolean closemfp ) {
	searchpath_t    *p, *next;
	int i;

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);
	
	for ( i = 0; i < MAX_FILE_HANDLES; i++ ) {
		if ( fsh[i].handleSync ) {
			FS_FCloseFile( i );
		}
	}

	// free everything
	for( p = fs_searchpaths ; p ; p = next ) {
		next = p->next;

		if ( p->pack ) {
			unzClose( p->pack->handle );
			Z_Free( p->pack->buildBuffer );
			Z_Free( p->pack );
		}
		if ( p->dir ) {
			Z_Free( p->dir );
		}
		Z_Free( p );
	}

	// any FS_ calls will now be an error until reinitialized
	fs_searchpaths = NULL;

	Cmd_RemoveCommand( "path" );
	Cmd_RemoveCommand( "which" );
	Cmd_RemoveCommand( "fdir" );


#ifdef FS_MISSING
	if ( closemfp ) {
		fclose( missingFiles );
	}
#endif
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
}


/*
=====================
FS_ClearPakReferences
=====================
*/
void FS_ClearPakReferences( int flags ) {
	searchpath_t *search;

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);
	
	if ( !flags ) {
		flags = -1;
	}
	for ( search = fs_searchpaths; search; search = search->next ) {
		// is the element a pak file and has it been referenced?
		if ( search->pack ) {
			search->pack->referenced &= ~flags;
		}
	}
	
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
}



/*
================
FS_Restart
================
*/
void FS_Restart( int checksumFeed ) {

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);
	
	// free anything we currently have loaded
	FS_Shutdown( qfalse );

	// set the checksum feed
	fs_checksumFeed = checksumFeed;

	// clear pak references
	FS_ClearPakReferences( 0 );

	// try to start up normally
	FS_Startup( BASEGAME );

	// see if we are going to allow add-ons
	//FS_SetRestrictions();

	// if we can't find default.cfg, assume that the paths are
	// busted and error out now, rather than getting an unreadable
	// graphics screen when the font fails to load
/*	if ( FS_ReadFile( "default.cfg", NULL ) <= 0 ) {
		// this might happen when connecting to a pure server not using BASEGAME/pak0.pk3
		// (for instance a TA demo server)
		if ( lastValidBase[0] ) {
			FS_PureServerSetLoadedPaks( "", "" );
			Cvar_Set( "fs_basepath", lastValidBase );
			Cvar_Set( "fs_gamedirvar", lastValidGame );
			lastValidBase[0] = '\0';
			lastValidGame[0] = '\0';
			Cvar_Set( "fs_restrict", "0" );
			FS_Restart( checksumFeed );
			Com_Error( ERR_DROP, "Invalid game folder\n" );
			return;
		}
		Com_Error( ERR_FATAL, "Couldn't load default.cfg" );
	}
*/
	// bk010116 - new check before safeMode
	if ( Q_stricmp( fs_gamedirvar->string, lastValidGame ) ) {
		// skip the wolfconfig.cfg if "safe" is on the command line
		if ( !Com_SafeMode() ) {
			Cbuf_AddText( "exec config_mp.cfg\n" );
		}
	}

	Q_strncpyz( lastValidBase, fs_basepath->string, sizeof( lastValidBase ) );
	Q_strncpyz( lastValidGame, fs_gamedirvar->string, sizeof( lastValidGame ) );
	
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

}


/*
=================
FS_CopyFile

Copy a fully specified file from one place to another
=================
*/
void FS_CopyFile( char *fromOSPath, char *toOSPath ) {
	FILE    *f;
	int len;
	byte    *buf;
	mvabuf;

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);

	if ( fs_debug->integer ) {
		Sys_Print( va("^4copy %s to %s\n", fromOSPath, toOSPath ) );
	}

	f = fopen( fromOSPath, "rb" );
	if ( !f ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return;
	}
	fseek( f, 0, SEEK_END );
	len = ftell( f );
	fseek( f, 0, SEEK_SET );

	// we are using direct malloc instead of Z_Malloc here, so it
	// probably won't work on a mac... Its only for developers anyway...
	buf = malloc( len );
	if ( fread( buf, 1, len, f ) != len ) {
		Com_Error( ERR_FATAL, "Short read in FS_Copyfiles()\n" );
	}
	fclose( f );

	if ( FS_CreatePath( toOSPath ) ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return;
	}

	f = fopen( toOSPath, "wb" );
	if ( !f ) {
		free( buf );    //DAJ free as well
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return;
	}
	if ( fwrite( buf, 1, len, f ) != len ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		Com_Error( ERR_FATAL, "Short write in FS_Copyfiles()\n" );
	}
	fclose( f );
	free( buf );
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

}

/*
===========
FS_FOpenFileWrite

===========
*/
fileHandle_t FS_FOpenFileWrite( const char *filename ) {
	char            ospath[MAX_OSPATH];
	fileHandle_t f;
	mvabuf;


	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);

	f = FS_HandleForFile();
	if(f == 0){
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return 0;
	}
	FS_SetFilenameForHandle(f, filename);
	fsh[f].zipFile = qfalse;

	FS_BuildOSPathForThread( fs_homepath->string, fs_gamedir, filename, ospath, 0 );

	if ( fs_debug->integer ) {
		Sys_Print(va("FS_FOpenFileWrite: %s\n", ospath ));
	}

	if ( FS_CreatePath( ospath ) ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return 0;
	}

	// enabling the following line causes a recursive function call loop
	// when running with +set logfile 1 +set developer 1
	//Com_DPrintf( "writing to: %s\n", ospath );
	fsh[f].handleFiles.file.o = fopen( ospath, "wb" );

	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	fsh[f].handleSync = qfalse;

	if ( !fsh[f].handleFiles.file.o ) {
		f = 0;
	}
	return f;
}



/*
===========
FS_FOpenFileAppend

===========
*/
fileHandle_t __cdecl FS_FOpenFileAppend( const char *filename ) {
	char            ospath[MAX_OSPATH];
	fileHandle_t f;
	mvabuf;

	Sys_EnterCriticalSection(CRIT_FILESYSTEM);

	
	if ( !FS_Initialized() ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if(Sys_IsMainThread())
	{
		f = FS_HandleForFileForThread(0);
	}else{
		f = FS_HandleForFileForThread(3);
	}

	if(f == 0){
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return 0;
	}
	FS_SetFilenameForHandle(f, filename);
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	// don't let sound stutter
	//S_ClearSoundBuffer();

	FS_BuildOSPathForThread( fs_homepath->string, fs_gamedir, filename, ospath, 0 );

	if ( fs_debug->integer ) {
		Sys_Print(va("FS_FOpenFileAppend: %s\n", ospath ));
	}

	if ( FS_CreatePath( ospath ) ) {
		Sys_LeaveCriticalSection(CRIT_FILESYSTEM);
		return 0;
	}

	fsh[f].handleFiles.file.o = fopen( ospath, "ab" );
	fsh[f].handleSync = qfalse;
	
	Sys_LeaveCriticalSection(CRIT_FILESYSTEM);

	
	if ( !fsh[f].handleFiles.file.o ) {
		f = 0;
	}
	return f;
}


qboolean FS_SetPermissionsExec(const char* ospath)
{
	return Sys_SetPermissionsExec( ospath );
}

//void DB_BuildOSPath(const char *filename<eax>, int ffdir<edx>, int len<ecx>, char *buff)
__regparm3 void DB_BuildOSPath(const char *filename, int ffdir, int len, char *buff)
{
    const char *languagestr;
    char *mapstrend;
    char mapname[MAX_QPATH];
    char ospath[MAX_OSPATH];

    switch(ffdir)
    {
        case 0:
            languagestr = SEH_GetLanguageName( SEH_GetCurrentLanguage() );
            if ( !languagestr )
            {
                languagestr = "english";
            }

            Com_sprintf(ospath, sizeof(ospath), "zone/%s/%s.ff", languagestr, filename);
            FS_SV_GetFilepath( ospath, buff, len );
            return;

        case 1:

            Com_sprintf(ospath, sizeof(ospath), "%s/%s.ff", fs_gamedirvar->string, filename);
            FS_SV_GetFilepath( ospath, buff, len );
            return;

        case 2:

            Q_strncpyz(mapname, filename, sizeof(mapname));
            mapstrend = strstr(mapname, "_load");
            if ( mapstrend )
            {
                mapstrend[0] = '\0';
            }
            Com_sprintf(ospath, sizeof(ospath), "%s/%s/%s.ff", "usermaps", mapname, filename);
            FS_SV_GetFilepath( ospath, buff, len );
            return;
    }
}



/*
========================================================================================

Handle based file calls for virtual machines

========================================================================================
*/

int     FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	int r;
	qboolean sync;

	sync = qfalse;

	switch ( mode ) {
	case FS_READ:
		r = FS_FOpenFileRead( qpath, f );
		break;
	case FS_WRITE:
/*
#ifdef __MACOS__    //DAJ MacOS file typing
		{
			extern _MSL_IMP_EXP_C long _fcreator, _ftype;
			_ftype = 'WlfB';
			_fcreator = 'WlfM';
		}
#endif
*/
		*f = FS_FOpenFileWrite( qpath );
		r = 0;
		if ( *f == 0 ) {
			r = -1;
		}
		break;
	case FS_APPEND_SYNC:
		sync = qtrue;
	case FS_APPEND:
/*
#ifdef __MACOS__    //DAJ MacOS file typing
		{
			extern _MSL_IMP_EXP_C long _fcreator, _ftype;
			_ftype = 'WlfB';
			_fcreator = 'WlfM';
		}
#endif
*/
		*f = FS_FOpenFileAppend( qpath );
		r = 0;
		if ( *f == 0 ) {
			r = -1;
		}
		break;
	default:
		Com_Error( ERR_FATAL, "FSH_FOpenFile: bad mode" );
		return -1;
	}

	if ( !f ) {
		return r;
	}

	if ( *f ) {

		fsh[*f].fileSize = r;
		fsh[*f].streamed = qfalse;

		// uncommenting this makes fs_reads
		// use the background threads --
		// MAY be faster for loading levels depending on the use of file io
		// q3a not faster
		// wolf not faster

//		if (mode == FS_READ) {
//			Sys_BeginStreamedFile( *f, 0x4000 );
//			fsh[*f].streamed = qtrue;
//		}
	}
	fsh[*f].handleSync = sync;

	return r;
}





typedef struct fsPureSums_s
{

  struct fsPureSums_s *next;
  int checksum;
  char baseName[MAX_OSPATH];
  char gameName[MAX_OSPATH];

}fsPureSums_t;

static fsPureSums_t *fs_iwdPureChecks;


void __cdecl FS_AddIwdPureCheckReference(searchpath_t *search)
{
	
    fsPureSums_t *checks;
    fsPureSums_t *newCheck;

    if( search->localized )
    {
	return;
    }

    for(checks = fs_iwdPureChecks; checks != NULL ; checks = checks->next)
    {
        if ( checks->checksum == search->pack->checksum )
        {
          if ( !Q_stricmp(checks->baseName, search->pack->pakBasename) )
	  {
			return;
	  }
        }
    }
    newCheck = (fsPureSums_t *)Z_Malloc(sizeof(fsPureSums_t));
    newCheck->next = NULL;
    newCheck->checksum = search->pack->checksum;
    Q_strncpyz(newCheck->baseName, search->pack->pakBasename, sizeof(newCheck->baseName));
    Q_strncpyz(newCheck->gameName, search->pack->pakGamename, sizeof(newCheck->gameName));

    if(fs_iwdPureChecks == NULL)
    {
        fs_iwdPureChecks = newCheck;
	return;
    }
	
    for( checks = fs_iwdPureChecks; checks->next != NULL; checks = checks->next );
	
    checks->next = newCheck;
}


void __cdecl FS_ShutdownIwdPureCheckReferences()
{
  fsPureSums_t *cur;
  fsPureSums_t *next;

  cur = fs_iwdPureChecks;

  while( cur )
  {
    next = cur->next;
    Z_Free( cur );
    cur = next;
  }
  fs_iwdPureChecks = 0;
}



void __cdecl FS_ReferencedIwds(char **outChkSums, char **outPathNames)
{
  fsPureSums_t *puresum;
  searchpath_t *search;

  static char chkSumString[8192];
  static char pathString[8192];
  char chksum[1024];
  
  chkSumString[0] = 0;
  pathString[0] = 0;
  
  for ( puresum = fs_iwdPureChecks; puresum; puresum = puresum->next )
  {
	Com_sprintf(chksum, sizeof(chksum), "%i ", puresum->checksum);
	Q_strcat(chkSumString, sizeof(chkSumString), chksum);
	if ( pathString[0] )
	{
		Q_strcat(pathString, sizeof(pathString), " ");
	}
	Q_strcat(pathString, sizeof(pathString), puresum->gameName);
    Q_strcat(pathString, sizeof(pathString), "/");
    Q_strcat(pathString, sizeof(pathString), puresum->baseName);
  }
  
  if ( !fs_gameDirVar->string[0] )
  {
		*outChkSums = chkSumString;
		*outPathNames = pathString;
		return;
  }
  
  for ( search = fs_searchpaths; search; search = search->next )
  {
        if ( search->pack && !search->localized )
        {
	    if ( !(search->pack->referenced & FS_GENERAL_REF) &&
		(!Q_stricmp(search->pack->pakGamename, fs_gameDirVar->string) || !Q_stricmpn(search->pack->pakGamename, "usermaps", 8))
	    )
	    {
		Com_sprintf(chksum, sizeof(chksum), "%i ", search->pack->checksum);
		Q_strcat(chkSumString, sizeof(chkSumString), chksum);
		if ( pathString[0] )
		{
			Q_strcat(pathString, sizeof(pathString), " ");
		}
		Q_strcat(pathString, sizeof(pathString), search->pack->pakGamename);
		Q_strcat(pathString, sizeof(pathString), "/");
		Q_strcat(pathString, sizeof(pathString), search->pack->pakBasename);
	    }
        }
  }
  *outChkSums = chkSumString;
  *outPathNames = pathString;
}

void __cdecl FS_ShutdownReferencedFiles(int *numFiles, char **names)
{
  int i;

  for(i = 0; i < *numFiles; i++)
  {
    if ( names[i] )
    {
	Z_Free(names[i]);
        names[i] = NULL;
    }
	*numFiles = 0;
  }
}


void FS_ShutdownServerIwdNames()
{
    FS_ShutdownReferencedFiles(&fs_numServerIwds, fs_serverIwdNames);
}





/*
=====================
FS_PureServerSetLoadedPaks

If the string is empty, all data sources will be allowed.
If not empty, only pk3 files that match one of the space
separated checksums will be checked for files, with the
exception of .cfg and .dat files.
=====================
*/

int FS_PureServerSetLoadedIwds(const char *paksums, const char *paknames)
{
  int i, k, l, rt;
  int numPakSums;
  fsPureSums_t *pureSums;
  int numPakNames;
  char *lpakNames[1024];
  int lpakSums[1024];

  rt = 0;
  
  Cmd_TokenizeString(paksums);
  
  numPakSums = Cmd_Argc();
  
  if ( numPakSums > sizeof(lpakSums)/sizeof(lpakSums[0]))
  {
    numPakSums = sizeof(lpakSums)/sizeof(lpakSums[0]);
  }
  
  for ( i = 0 ; i < numPakSums ; i++ ) {
	lpakSums[i] = atoi( Cmd_Argv( i ) );
  }
  Cmd_EndTokenizedString();

  Cmd_TokenizeString(paknames);
  numPakNames = Cmd_Argc();
  
  if ( numPakNames > sizeof(lpakNames)/sizeof(lpakNames[0]) )
  {
    numPakNames = sizeof(lpakNames)/sizeof(lpakNames[0]);
  }
  
  for ( i = 0 ; i < numPakNames ; i++ ) {
	lpakNames[i] = CopyString( Cmd_Argv( i ) );
  }
  
  Cmd_EndTokenizedString();

  if ( numPakSums != numPakNames )
  {
    Com_Error(ERR_FATAL, "iwd sum/name mismatch");
	return rt;
  }
  
	if ( numPakSums )
	{
  
		for(pureSums = fs_iwdPureChecks; pureSums; pureSums = pureSums->next)
		{

			for ( i = 0; i < numPakSums; i++)
			{
				if(lpakSums[i] == pureSums->checksum && !Q_stricmp(lpakNames[i], pureSums->baseName))
				{
					break;
				}
			}
			if ( i == numPakSums )
			{
				rt = 1;
				break;
			}
		}
	}

	if ( numPakSums == fs_numServerIwds && rt == 0)
	{
		for ( i = 0, k = 0; i < fs_numServerIwds; )
		{
		  if ( lpakSums[k] == fs_serverIwds[i] && !Q_stricmp(lpakNames[k], fs_serverIwdNames[i]) )
		  {
			++k;
			if ( k < numPakSums )
			{
			  i = 0;
			  continue;
			}
			
			for ( l = 0; l < numPakNames; ++l )
			{
				Z_Free(lpakNames[l]);
				lpakNames[l] = NULL;
			}
			return 0;
		  
		  }
		  ++i;
		}
		if ( numPakSums == 0 )
		{
			return rt;
		}
	}

    //SND_StopSounds(8);
    FS_ShutdownServerIwdNames( );
    fs_numServerIwds = numPakSums;
    if ( numPakSums )
    {
      Com_DPrintf("Connected to a pure server.\n");
      Com_Memcpy(fs_serverIwds, lpakSums, sizeof(int) * fs_numServerIwds);
      Com_Memcpy(fs_serverIwdNames, lpakNames, sizeof(char*) * fs_numServerIwds);
      //fs_fakeChkSum = 0;
    }
    return rt;
}

