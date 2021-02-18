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



#ifndef __QCOMMON_H__
#define __QCOMMON_H__

#include <time.h>
#include "cvar.h"
#include "qcommon_io.h"


typedef enum {
	// bk001129 - make sure SE_NONE is zero
	SE_NONE = 0,    // evTime is still valid
	SE_CONSOLE, // evPtr is a char*
	SE_PACKET   // evPtr is a netadr_t followed by data bytes to evPtrLength
} sysEventType_t;

void Com_QueueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );

void __cdecl Com_InitParse( void );
void __cdecl Com_Restart(void);
void __cdecl Com_WriteConfig_f(void);
void __cdecl Com_WriteDefaults_f(void);
void __cdecl Com_Close(void);
int __cdecl Com_Filter( char* filter, char *name, int casesensitive);

void Com_Frame(void);
void Com_Init(char* cmdline);
void Com_UpdateRealtime();
time_t Com_GetRealtime();
int QDECL Com_AddTimedEvent( int delay, void *function, unsigned int argcount, ...);
int Com_FilterPath( char *filter, char *name, int casesensitive );

void Com_RandomBytes( byte *string, int len );
int Com_HashKey( char *string, int maxlen );
void Com_Quit_f( void );
unsigned int Com_GetFrameTime(void);
unsigned long long Com_GetUsecFrameTime(void);

qboolean Com_InError();


extern unsigned long long com_frameTime;
extern unsigned long long com_uFrameTime;
extern qboolean gamebinary_initialized;
extern cvar_t* com_dedicated;
extern cvar_t* com_timescale;
extern cvar_t* com_sv_running;
extern cvar_t* com_logfile;
extern cvar_t* com_developer;
extern cvar_t* com_useFastfiles;
extern cvar_t* com_animCheck;
extern cvar_t* com_version;
extern qboolean com_securemode;

int Com_IsDeveloper();
qboolean Com_LoadBinaryImage();

#define MAXPRINTMSG 4096
#define	MAX_RELIABLE_COMMANDS	128	// max string commands buffered for restransmit
#define MAX_DOWNLOAD_WINDOW	8	// max of eight download frames
#define MAX_DOWNLOAD_BLKSIZE	2048	// 2048 byte block chunks
#define MAX_PACKET_USERCMDS	32

#define	PACKET_BACKUP		32
#define PACKET_MASK ( PACKET_BACKUP - 1 )



#define Q3CONFIG_CFG "q3config_server.cfg"


#endif

