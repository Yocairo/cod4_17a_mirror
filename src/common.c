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



#include "q_shared.h"
#include "qcommon_io.h"
#include "qcommon_parsecmdline.h"
#include "qcommon_logprint.h"
#include "sys_cod4defs.h"
#include "cvar.h"
#include "filesystem.h"
#include "qcommon_mem.h"
#include "q_platform.h"
#include "sys_main.h"
#include "sys_thread.h"
#include "qcommon.h"
#include "cmd.h"
#include "sys_net.h"
#include "xassets.h"
#include "plugin_handler.h"
#include "misc.h"
#include "scr_vm.h"
#include "netchan.h"
#include "server.h"
#include "nvconfig.h"
#include "hl2rcon.h"
#include "sv_auth.h"
#include "punkbuster.h"
#include "sec_init.h"
#include "sys_cod4loader.h"
#include "httpftp.h"
#include "huffman.h"

#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <time.h>



unsigned long long com_uFrameTime = 0;
unsigned long long com_frameTime = 0;

cvar_t* com_version;
cvar_t* com_shortversion;
cvar_t* com_dedicated;
cvar_t* com_timescale;
cvar_t* com_fixedtime;
cvar_t* com_maxFrameTime;
cvar_t* com_animCheck;
cvar_t* com_developer;
cvar_t* com_useFastfiles;
cvar_t* com_developer;
cvar_t* com_developer_script;
cvar_t* com_logfile;
cvar_t* com_sv_running;
cvar_t* com_securemodevar;
cvar_t* sv_webadmin;
qboolean com_securemode;

char com_errorMessage[MAXPRINTMSG];
qboolean com_errorEntered;
qboolean gamebinary_initialized = qfalse;
qboolean com_fullyInitialized = qfalse;

void Com_WriteConfig_f( void );
void Com_WriteConfiguration( void );
/*
========================================================================

EVENT LOOP

========================================================================
*/

typedef union{
    float f;
    char c;
    int i;
    qboolean b;
    byte by;
    void* p;
}universalArg_t;

typedef struct {
	int evTime;
	sysEventType_t evType;
	int evValue, evValue2;
	int evPtrLength;                // bytes of data pointed to by evPtr, for journaling
	void            *evPtr;         // this must be manually freed if not NULL
} sysEvent_t;


#define MAX_TIMEDEVENTARGS 8

typedef struct{
    universalArg_t arg;
    unsigned int size;
}timedEventArg_t;

typedef timedEventArg_t timedEventArgs_t[MAX_TIMEDEVENTARGS];


#define MAX_QUEUED_EVENTS  256
#define MAX_TIMED_EVENTS  1024
#define MASK_QUEUED_EVENTS ( MAX_QUEUED_EVENTS - 1 )
#define MASK_TIMED_EVENTS ( MAX_TIMED_EVENTS - 1 )

typedef struct{
	int evTime, evTriggerTime;
	timedEventArgs_t evArguments;
	void (*evFunction)();
}timedSysEvent_t;


static sysEvent_t  eventQueue[ MAX_QUEUED_EVENTS ];
static timedSysEvent_t  timedEventBuffer[ MAX_QUEUED_EVENTS ];
static int         eventHead = 0;
static int         eventTail = 0;
static int         timedEventHead = 0;


void EventTimerTest(int time, int triggerTime, int value, char* s){

	Com_Printf("^5Event exectuted: %i %i %i %i %s\n", time, triggerTime, Sys_Milliseconds(), value, s);

}


/*
================
Com_SetTimedEventCachelist

================
*/
void Com_MakeTimedEventArgCached(unsigned int index, unsigned int arg, unsigned int size){

	if(index >= MAX_TIMED_EVENTS)
		Com_Error(ERR_FATAL, "Com_MakeTimedEventArgCached: Bad index: %d", index);

	if(arg >= MAX_TIMEDEVENTARGS)
		Com_Error(ERR_FATAL, "Com_MakeTimedEventArgCached: Bad function argument number. Allowed range is 0 - %d arguments", MAX_TIMEDEVENTARGS);

	timedSysEvent_t  *ev = &timedEventBuffer[index];
	void *ptr = Z_Malloc(size);
	Com_Memcpy(ptr, ev->evArguments[arg].arg.p, size);
	ev->evArguments[arg].size = size;
	ev->evArguments[arg].arg.p = ptr;
}


/*
================
Com_AddTimedEvent

================
*/
int QDECL Com_AddTimedEvent( int delay, void *function, unsigned int argcount, ...)
{
	timedSysEvent_t  *ev;
	int index;
	int i;
	int time;
	int triggerTime;

	if ( timedEventHead >= MAX_TIMED_EVENTS )
	{
		Com_PrintWarning("Com_AddTimedEvent: overflow - Lost one event\n");
		// we are discarding an event, but don't leak memory
		return -1;
	}

	index = timedEventHead;

	time = Sys_Milliseconds();

	triggerTime = delay + time;

	while(qtrue)
	{
		if(index > 0){

			ev = &timedEventBuffer[index -1];

			if(ev->evTriggerTime < triggerTime)
			{
				timedEventBuffer[index] = *ev;
				index--;
				continue;
			}
		}
		break;
	}

	if(argcount > MAX_TIMEDEVENTARGS)
	{
		Com_Error(ERR_FATAL, "Com_AddTimedEvent: Bad number of function arguments. Allowed range is 0 - %d arguments", MAX_TIMEDEVENTARGS);
		return -1;
	}

	ev = &timedEventBuffer[index];

	va_list		argptr;
	va_start(argptr, argcount);

	for(i = 0; i < MAX_TIMEDEVENTARGS; i++)
	{
		if(i < argcount)
			ev->evArguments[i].arg = va_arg(argptr, universalArg_t);

		ev->evArguments[i].size = 0;
	}

	va_end(argptr);

	ev->evTime = time;
	ev->evTriggerTime = triggerTime;
	ev->evFunction = function;
	timedEventHead++;
	return index;
}



void Com_InitEventQueue()
{
    // bk000306 - clear eventqueue
    memset( eventQueue, 0, MAX_QUEUED_EVENTS * sizeof( sysEvent_t ) );
}

/*
================
Com_QueueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/
void Com_QueueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr )
{
	sysEvent_t  *ev;

	ev = &eventQueue[ eventHead & MASK_QUEUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUEUED_EVENTS )
	{
		Com_PrintWarning("Com_QueueEvent: overflow\n");
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr )
		{
			Z_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	if ( time == 0 )
	{
		time = Sys_Milliseconds();
	}

	ev->evTime = time;
	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

/*
================
Com_GetTimedEvent

================
*/
timedSysEvent_t* Com_GetTimedEvent( int time )
{
	timedSysEvent_t  *ev;

	if(timedEventHead > 0)
	{
		ev = &timedEventBuffer[timedEventHead - 1];
		if(ev->evTriggerTime <= time)
		{
			timedEventHead--; //We have removed one event
			return ev;
		}
	}
	return NULL;
}


/*
================
Com_GetSystemEvent

================
*/
sysEvent_t* Com_GetSystemEvent( void )
{
	char        *s;
	// return if we have data

	if ( eventHead > eventTail )
	{
		eventTail++;
		return &eventQueue[ ( eventTail - 1 ) & MASK_QUEUED_EVENTS ];
	}

	Sys_EventLoop();

	// check for console commands
	s = Sys_ConsoleInput();
	if ( s )
	{
		char  *b;
		int   len;

		len = strlen( s ) + 1;
		b = Z_Malloc( len );
		strcpy( b, s );
		Com_QueueEvent( 0, SE_CONSOLE, 0, 0, len, b );
	}

	// return if we have data
	if ( eventHead > eventTail )
	{
		eventTail++;
		return &eventQueue[ ( eventTail - 1 ) & MASK_QUEUED_EVENTS ];
	}

	// create an empty event to return
	return NULL;
}


/*
=================
Com_EventLoop

Returns last event time
=================
*/
void Com_EventLoop( void ) {
	sysEvent_t	*ev;

	while ( 1 ) {
		ev = Com_GetSystemEvent();

		// if no more events are available
		if ( !ev ) {
			break;
		}
			switch(ev->evType)
			{
				case SE_CONSOLE:
					Cbuf_AddText( (char *)ev->evPtr );
					Cbuf_AddText("\n");
				break;
				default:
					Com_Error( ERR_FATAL, "Com_EventLoop: bad event type %i", ev->evType );
				break;
			}
			// free any block data
			if ( ev->evPtr ) {
				Z_Free( ev->evPtr );
			}
	}
}


/*
=================
Com_TimedEventLoop
=================
*/
void Com_TimedEventLoop( void ) {
	timedSysEvent_t	*evt;
	int time = Sys_Milliseconds();
	int i;

	while( qtrue ) {
		evt = Com_GetTimedEvent(time);

		// if no more events are available
		if ( !evt ) {
			break;
		}
		//Execute the passed eventhandler
		if(evt->evFunction)
			evt->evFunction(evt->evArguments[0].arg, evt->evArguments[1].arg, evt->evArguments[2].arg, evt->evArguments[3].arg,
			evt->evArguments[4].arg, evt->evArguments[5].arg, evt->evArguments[6].arg, evt->evArguments[7].arg);

		for(i = 0; i < MAX_TIMEDEVENTARGS; i++)
		{
			if(evt->evArguments[i].size > 0){
				Z_Free(evt->evArguments[i].arg.p);
			}
		}
	}
}


int Com_IsDeveloper()
{
    if(com_developer && com_developer->integer)
        return com_developer->integer;

    return 0;

}

/*
=============
Com_Error_f

Just throw a fatal error to
test error shutdown procedures
=============
*/
static void Com_Error_f (void) {
	if ( Cmd_Argc() > 1 ) {
		Com_Error( ERR_DROP, "Testing drop error" );
	} else {
		Com_Error( ERR_FATAL, "Testing fatal error" );
	}
}


/*
=============
Com_Freeze_f

Just freeze in place for a given number of seconds to test
error recovery
=============
*/
static void Com_Freeze_f (void) {
	float	s;
	int		start, now;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "freeze <seconds>\n" );
		return;
	}
	s = atof( Cmd_Argv(1) );

	start = Sys_Milliseconds();

	while ( 1 ) {
		now = Sys_Milliseconds();
		if ( ( now - start ) * 0.001 > s ) {
			break;
		}
	}
}

/*
=================
Com_Crash_f

A way to force a bus error for development reasons
=================
*/
static void Com_Crash_f( void ) {
	* ( int * ) 0 = 0x12345678;
}


/*
==================
Com_RandomBytes

fills string array with len radom bytes, peferably from the OS randomizer
==================
*/
void Com_RandomBytes( byte *string, int len )
{
	int i;

	if( Sys_RandomBytes( string, len ) )
		return;

	Com_Printf( "Com_RandomBytes: using weak randomization\n" );
	for( i = 0; i < len; i++ )
		string[i] = (unsigned char)( rand() % 255 );
}


/*
============
Com_HashKey
============
*/
int Com_HashKey( char *string, int maxlen ) {
	int register hash, i;

	hash = 0;
	for ( i = 0; i < maxlen && string[i] != '\0'; i++ ) {
		hash += string[i] * ( 119 + i );
	}
	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) );
	return hash;
}


/*
=============
Com_Quit_f

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void Com_Quit_f( void ) {
	// don't try to shutdown if we are in a recursive error
	Com_Printf("quitting...\n");

	Sys_EnterCriticalSection( 2 );

	if(gamebinary_initialized == qtrue)
	{
		Scr_Cleanup();
		GScr_Shutdown();
	}

	if ( !com_errorEntered ) {
		// Some VMs might execute "quit" command directly,
		// which would trigger an unload of active VM error.
		// Sys_Quit will kill this process anyways, so
		// a corrupt call stack makes no difference
		if(gamebinary_initialized == qtrue)
		{
			Hunk_ClearTempMemory();
			Hunk_ClearTempMemoryHigh();
			SV_Shutdown("EXE_SERVERQUIT");

			Com_Close();
		}
		Com_CloseLogFiles( );

		FS_Shutdown(qtrue);

		if(gamebinary_initialized == qtrue)
		{
			FS_ShutdownIwdPureCheckReferences();
			FS_ShutdownServerIwdNames();
			FS_ShutdownServerReferencedIwds();
			FS_ShutdownServerReferencedFFs();
		}
		NET_Shutdown();
	}

	Sys_LeaveCriticalSection( 2 );

	Sys_Quit ();
}

static void Com_InitCvars( void ){
    static const char* dedicatedEnum[] = {"listen server", "dedicated LAN server", "dedicated internet server", NULL};
    static const char* logfileEnum[] = {"disabled", "async file write", "sync file write", NULL};
	mvabuf;

    char* s;

    com_dedicated = Cvar_RegisterEnum("dedicated", dedicatedEnum, 2, CVAR_INIT, "True if this is a dedicated server");
    com_timescale = Cvar_RegisterFloat("timescale", 1.0, 0.0, 1000.0, CVAR_CHEAT | CVAR_SYSTEMINFO, "Scale time of each frame");
    com_fixedtime = Cvar_RegisterInt("fixedtime", 0, 0, 1000, 0x80, "Use a fixed time rate for each frame");
    com_maxFrameTime = Cvar_RegisterInt("com_maxFrameTime", 100, 50, 1000, 0, "Time slows down if a frame takes longer than this many milliseconds");
    com_animCheck = Cvar_RegisterBool("com_animCheck", qfalse, 0, "Check anim tree");
    s = va("%s %s %s build %i %s", GAME_STRING,Q3_VERSION,PLATFORM_STRING, BUILD_NUMBER, __DATE__);

    com_version = Cvar_RegisterString ("version", s, CVAR_ROM | CVAR_SERVERINFO , "Game version");
    com_shortversion = Cvar_RegisterString ("shortversion", Q3_VERSION, CVAR_ROM | CVAR_SERVERINFO , "Short game version");

    Cvar_RegisterString ("build", va("%i", BUILD_NUMBER), CVAR_ROM | CVAR_SERVERINFO , "");
    com_useFastfiles = Cvar_RegisterBool ("useFastFiles", qtrue, 16, "Enables loading data from fast files");
    //MasterServer
    //AuthServer
    //MasterServerPort
    //AuthServerPort
    sv_webadmin = Cvar_RegisterBool ("sv_webadmin", qtrue, 0, "Enable HTTP Web Admin");
    com_developer = Cvar_RegisterInt("developer", 0, 0, 2, 0, "Enable development options");
    com_developer_script = Cvar_RegisterBool ("developer_script", qfalse, 16, "Enable developer script comments");
    com_logfile = Cvar_RegisterEnum("logfile", logfileEnum, 0, 0, "Write to logfile");
    com_sv_running = Cvar_RegisterBool("sv_running", qfalse, 64, "Server is running");
    com_securemodevar = Cvar_RegisterBool("securemode", qfalse, CVAR_INIT, "CoD4 runs in secure mode which restricts execution of external scripts/programs and loading of unauthorized shared libraries/plugins. This is recommended in a shared hosting environment");
    com_securemode = com_securemodevar->boolean;
}



void Com_InitThreadData()
{
    static jmp_buf jmpbuf_obj;

    Sys_SetValue(1, 0);
    Sys_SetValue(2, &jmpbuf_obj);
    Sys_SetValue(3, (const void*)0x14087620);
}


void Com_CopyCvars()
{
    *(cvar_t**)0x88a6170 = com_useFastfiles;
    *(cvar_t**)0x88a6184 = com_developer;
    *(cvar_t**)0x88a6188 = com_developer_script;
    *(cvar_t**)0x88a61b0 = com_logfile;
    *(cvar_t**)0x88a61a8 = com_sv_running;

}

void Com_PatchError()
{
	*(char**)0x8121C28 = com_errorMessage;
	*(char**)0x81225C5 = com_errorMessage;
	*(char**)0x812262C = com_errorMessage;
	*(char**)0x812265A = com_errorMessage;
	*(char**)0x8123CFB = com_errorMessage;
	*(char**)0x8123D45 = com_errorMessage;
	*(char**)0x8123DAB = com_errorMessage;
	*(char**)0x8123E40 = com_errorMessage;
	*(char**)0x8123EBA = com_errorMessage;
	*(char**)0x8123F1E = com_errorMessage;
	*(char**)0x81240A3 = com_errorMessage;
}

void Com_InitGamefunctions()
{
    int msec = 0;

    FS_CopyCvars();
    Com_CopyCvars();
    SV_CopyCvars();
#ifndef COD4X17A
    XAssets_PatchLimits();  //Patch several asset-limits to higher values
#endif
    SL_Init();
    Swap_Init();

    CSS_InitConstantConfigStrings();

    if(com_useFastfiles->integer){

        Mem_Init();

        DB_SetInitializing( qtrue );

        Com_Printf("begin $init\n");

        msec = Sys_Milliseconds();

        Mem_BeginAlloc("$init", qtrue);
    }
//    Con_InitChannels();

    if(!com_useFastfiles->integer) SEH_UpdateLanguageInfo();

    Com_InitHunkMemory();

    Hunk_InitDebugMemory();

    Scr_InitVariables();

    Scr_Init(); //VM_Init

    Scr_Settings(com_logfile->integer || com_developer->integer ,com_developer_script->integer, com_developer->integer);

    XAnimInit();

    DObjInit();

    Mem_EndAlloc("$init", qtrue);
    DB_SetInitializing( qfalse );
    Com_Printf("end $init %d ms\n", Sys_Milliseconds() - msec);

    SV_Cmd_Init();
    SV_AddOperatorCommands();
	SV_RemoteCmdInit();
	
    cvar_t **msg_dumpEnts = (cvar_t**)(0x8930c1c);
    cvar_t **msg_printEntityNums = (cvar_t**)(0x8930c18);
    *msg_dumpEnts = Cvar_RegisterBool( "msg_dumpEnts", qfalse, CVAR_TEMP, "Print snapshot entity info");
    *msg_printEntityNums = Cvar_RegisterBool( "msg_printEntityNums", qfalse, CVAR_TEMP, "Print entity numbers");

    if(com_useFastfiles->integer)
        R_Init();

    Com_InitParse();

#ifdef PUNKBUSTER
    Com_AddRedirect(PbCaptureConsoleOutput_wrapper);
    if(!PbServerInitialize()){
        Com_Printf("Unable to initialize PunkBuster.  PunkBuster is disabled.\n");
    }
#endif

}

qboolean Com_LoadBinaryImage()
{

    if(gamebinary_initialized == qtrue)
	return qtrue;

    Com_Printf("--- Game binary initialization ---\n");

    if(Sys_LoadImage() == qtrue)
    {
        Com_InitGamefunctions();
        gamebinary_initialized = qtrue;
        Com_Printf("--- Game Binary Initialization Complete ---\n");
    }else{
        Com_Printf("^1--- Game Binary Initialization Failed ---\n");
        return qfalse;
    }
    return qtrue;
}

/*
=================
Com_Init

The games main initialization
=================
*/

void Com_Init(char* commandLine){


    static char creator[16];
    char creatorname[37];
	mvabuf;

    unsigned int	qport;

    jmp_buf* abortframe = (jmp_buf*)Sys_GetValue(2);

    if(setjmp(*abortframe)){
        Sys_Error(va("Error during Initialization:\n%s\n", com_errorMessage));
    }
    Com_Printf("%s %s %s build %i %s\n", GAME_STRING,Q3_VERSION,PLATFORM_STRING, BUILD_NUMBER, __DATE__);


    Cbuf_Init();

    Cmd_Init();

    Com_InitEventQueue();

    Com_ParseCommandLine(commandLine);
    Com_StartupVariable(NULL);

    Com_InitCvars();

    Cvar_Init();

    Sec_Init();

    FS_InitFilesystem();

    if(FS_SV_FileExists("securemode"))
    {
        com_securemode = qtrue;
    }

    Cbuf_AddText( "exec default_mp.cfg\n");
    Cbuf_Execute(0,0); // Always execute after exec to prevent text buffer overflowing
    Cbuf_AddText( "exec " Q3CONFIG_CFG "\n");
    Cbuf_Execute(0,0); // Always execute after exec to prevent text buffer overflowing
    if(com_securemode)
    {
        Cvar_SetStringByName("sv_democompletedCmd", "");
        Cvar_SetStringByName("sv_mapDownloadCompletedCmd", "");
        Cvar_SetBool(com_securemodevar, qtrue);
        Com_Printf("Info: SecureMode is enabled on this server!\n");
    }

    Com_StartupVariable(NULL);


    creator[0] = '_';
    creator[1] = 'C';
    creator[2] = 'o';
    creator[3] = 'D';
    creator[4] = '4';
    creator[5] = ' ';
    creator[6] = 'X';

    creator[7] = ' ';
    creator[8] = 'C';
    creator[9] = 'r';
    creator[10] = 'e';
    creator[11] = 'a';
    creator[12] = 't';
    creator[13] = 'o';
    creator[14] = 'r';
    creator[15] = '\0';

    creatorname[0] = 'N';
    creatorname[1] = 'i';
    creatorname[2] = 'n';
    creatorname[3] = 'j';
    creatorname[4] = 'a';
    creatorname[5] = 'm';
    creatorname[6] = 'a';
    creatorname[7] = 'n';
    creatorname[8] = ',';
    creatorname[9] = ' ';
    creatorname[10] = 'T';
    creatorname[11] = 'h';
    creatorname[12] = 'e';
    creatorname[13] = 'K';
    creatorname[14] = 'e';
    creatorname[15] = 'l';
    creatorname[16] = 'm';
    creatorname[17] = ' ';
    creatorname[18] = '@';
    creatorname[19] = ' ';
    creatorname[20] = 'h';
    creatorname[21] = 't';
    creatorname[22] = 't';
    creatorname[23] = 'p';
    creatorname[24] = ':';
    creatorname[25] = '/';
    creatorname[26] = '/';
    creatorname[27] = 'i';
    creatorname[28] = 'c';
    creatorname[29] = 'e';
    creatorname[30] = 'o';
    creatorname[31] = 'p';
    creatorname[32] = 's';
    creatorname[33] = '.';
    creatorname[34] = 'i';
    creatorname[35] = 'n';
    creatorname[36] = '\0';

    Cvar_RegisterString (creator, creatorname, CVAR_ROM | CVAR_SERVERINFO , "");

    cvar_modifiedFlags &= ~CVAR_ARCHIVE;

    if (com_developer && com_developer->integer)
    {
        Cmd_AddCommand ("error", Com_Error_f);
        Cmd_AddCommand ("crash", Com_Crash_f);
        Cmd_AddCommand ("freeze", Com_Freeze_f);
    }
    Cmd_AddCommand ("quit", Com_Quit_f);
    Cmd_AddCommand ("writeconfig", Com_WriteConfig_f );

//    Com_AddLoggingCommands();
//    HL2Rcon_AddSourceAdminCommands();

    Sys_Init();

    Com_UpdateRealtime();

    Com_RandomBytes( (byte*)&qport, sizeof(int) );
    Netchan_Init( qport );
	Huffman_InitMain();

    PHandler_Init();

    NET_Init();


    SV_Init();

    com_frameTime = Sys_Milliseconds();

    NV_LoadConfig();

    Com_Printf("--- Common Initialization Complete ---\n");

    Cbuf_Execute( 0, 0 );

    abortframe = (jmp_buf*)Sys_GetValue(2);

    if(setjmp(*abortframe)){
        Sys_Error(va("Error during Initialization:\n%s\n", com_errorMessage));
    }
    if(com_errorEntered) Com_Error(ERR_FATAL,"Recursive error");


    HL2Rcon_Init( );
/*
    if(sv_webadmin->boolean)
    {
        HTTPServer_Init();
    }
*/
    Auth_Init( );

    AddRedirectLocations( );

    Com_LoadBinaryImage( );

    com_fullyInitialized = qtrue;

    Com_AddStartupCommands( );
}




/*
================
Com_ModifyUsec
================
*/

unsigned int Com_ModifyUsec( unsigned int usec ) {
	int		clampTime;

	//
	// modify time for debugging values
	//
	if ( com_fixedtime->integer ) {
		usec = com_fixedtime->integer*1000;
	} else if ( com_timescale->value ) {
		usec *= com_timescale->value;
	}
	
	// don't let it scale below 1 usec
	if ( usec < 1 && com_timescale->value) {
		usec = 1;
	}

	if ( com_dedicated->integer ) {
		// dedicated servers don't want to clamp for a much longer
		// period, because it would mess up all the client's views
		// of time.
		if (usec > 500000)
			Com_Printf( "^5Hitch warning: %i msec frame time\n", usec / 1000 );

		clampTime = 5000000;
	} else if ( !com_sv_running->boolean ) {
		// clients of remote servers do not want to clamp time, because
		// it would skew their view of the server's time temporarily
		clampTime = 5000000;
	} else {
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clampTime = 200000;
	}

	if ( usec > clampTime ) {
		usec = clampTime;
	}

	return usec;
}

static time_t realtime;

time_t Com_GetRealtime()
{
	return realtime;
}

void Com_UpdateRealtime()
{
	time(&realtime);
}

/*
=================
Com_Frame
=================
*/
__optimize3 void Com_Frame( void ) {

	unsigned int			usec;
	static unsigned long long	lastTime;
	static unsigned int		com_frameNumber;


	jmp_buf* abortframe = (jmp_buf*)Sys_GetValue(2);

	if(setjmp(*abortframe)){
		/* Invokes Com_Error if needed */
		Sys_EnterCriticalSection(CRIT_ERRORCHECK);
		if(Com_InError() == qtrue)
		{
			Com_Error(0, "Error Cleanup");		
		}
		Sys_LeaveCriticalSection(CRIT_ERRORCHECK);
	}
	//
	// main event loop
	//
#ifdef TIMEDEBUG

	int		timeBeforeFirstEvents = 0;
	int		timeBeforeServer = 0;
	int		timeBeforeEvents = 0;
	int		timeBeforeClient = 0;
	int		timeAfter = 0;


	if ( com_speeds->integer ) {
		timeBeforeFirstEvents = Sys_Milliseconds ();
	}
#endif
	// Figure out how much time we have
//	if(com_dedicated->integer)
//		minUsec = SV_FrameUsec();
/*	else
	{
		if(com_minimized->integer && com_maxfpsMinimized->integer > 0)
			minMsec = 1000 / com_maxfpsMinimized->integer;
		else if(com_unfocused->integer && com_maxfpsUnfocused->integer > 0)
			minMsec = 1000 / com_maxfpsUnfocused->integer;
		else if(com_maxfps->integer > 0)
			minMsec = 1000 / com_maxfps->integer;
		else
			minMsec = 1;
		
		timeVal = com_frameTime - lastTime;
		bias += timeVal - minMsec;
		
		if(bias > minMsec)
			bias = minMsec;
		
		// Adjust minMsec if previous frame took too long to render so
		// that framerate is stable at the requested value.
		minMsec -= bias;
	}*/
//	timeVal = Com_TimeVal(minUsec);


/*	do
	{
		if(timeVal < 1){
			NET_Sleep(0);
		}else{
			NET_Sleep((timeVal - 1));
		}
		timeVal = Com_TimeVal(minUsec);
	} while( timeVal );
*/

	com_frameTime = Sys_MillisecondsLong();
	com_uFrameTime = Sys_MicrosecondsLong();

	usec = com_uFrameTime - lastTime;
	lastTime = com_uFrameTime;

	// mess with msec if needed
	usec = Com_ModifyUsec(usec);

	Cbuf_Execute (0 ,0);
	//
	// server side
	//
	
	Com_EventLoop();

	
#ifdef TIMEDEBUG
	if ( com_speeds->integer ) {
		timeBeforeServer = Sys_Milliseconds ();
	}
#endif
	if(!SV_Frame( usec ))
		return;

	PHandler_Event(PLUGINS_ONFRAME);

	Com_TimedEventLoop();
	Cbuf_Execute (0 ,0);
	NET_Sleep(0);
	NET_TcpServerPacketEventLoop();
	Sys_RunThreadCallbacks();
	Cbuf_Execute (0 ,0);

#ifdef TIMEDEBUG
	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds ();
		timeBeforeEvents = timeAfter;
		timeBeforeClient = timeAfter;
	}
#endif

//	NET_FlushPacketQueue();
#ifdef TIMEDEBUG
	//
	// report timing information
	//
	if ( com_speeds->integer ) {
		int			all, sv, ev, cl;

		all = timeAfter - timeBeforeServer;
		sv = timeBeforeEvents - timeBeforeServer;
		ev = timeBeforeServer - timeBeforeFirstEvents + timeBeforeClient - timeBeforeEvents;
		cl = timeAfter - timeBeforeClient;
		sv -= time_game;
		cl -= time_frontend + time_backend;

		Com_Printf ("frame:%i all:%3i sv:%3i ev:%3i cl:%3i gm:%3i rf:%3i bk:%3i\n", 
					 com_frameNumber, all, sv, ev, cl, time_game, time_frontend, time_backend );
	}	
#endif
	//
	// trace optimization tracking
	//
#ifdef TRACEDEBUG
	if ( com_showtrace->integer ) {
	
		extern	int c_traces, c_brush_traces, c_patch_traces;
		extern	int	c_pointcontents;

		Com_Printf ("%4i traces  (%ib %ip) %4i points\n", c_traces,
			c_brush_traces, c_patch_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_patch_traces = 0;
		c_pointcontents = 0;
	}
#endif
	com_frameNumber++;
	Com_WriteConfiguration( );
	Com_UpdateRealtime();
	
	/* Invokes Com_Error if needed */
	Sys_EnterCriticalSection(CRIT_ERRORCHECK);
	if(Com_InError() == qtrue)
	{
		Com_Error(0, "Error Cleanup");		
	}
	Sys_LeaveCriticalSection(CRIT_ERRORCHECK);
}









/*
============
Com_StringContains
============
*/
char *Com_StringContains( char *str1, char *str2, int casesensitive ) {
	int len, i, j;

	len = strlen( str1 ) - strlen( str2 );
	for ( i = 0; i <= len; i++, str1++ ) {
		for ( j = 0; str2[j]; j++ ) {
			if ( casesensitive ) {
				if ( str1[j] != str2[j] ) {
					break;
				}
			} else {
				if ( toupper( str1[j] ) != toupper( str2[j] ) ) {
					break;
				}
			}
		}
		if ( !str2[j] ) {
			return str1;
		}
	}
	return NULL;
}




/*
============
Com_Filter
============
*/
int Com_Filter( char *filter, char *name, int casesensitive ) {
	char buf[MAX_TOKEN_CHARS];
	char *ptr;
	int i, found;

	while ( *filter ) {
		if ( *filter == '*' ) {
			filter++;
			for ( i = 0; *filter; i++ ) {
				if ( *filter == '*' || *filter == '?' ) {
					break;
				}
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if ( strlen( buf ) ) {
				ptr = Com_StringContains( name, buf, casesensitive );
				if ( !ptr ) {
					return qfalse;
				}
				name = ptr + strlen( buf );
			}
		} else if ( *filter == '?' )      {
			filter++;
			name++;
		} else if ( *filter == '[' && *( filter + 1 ) == '[' )           {
			filter++;
		} else if ( *filter == '[' )      {
			filter++;
			found = qfalse;
			while ( *filter && !found ) {
				if ( *filter == ']' && *( filter + 1 ) != ']' ) {
					break;
				}
				if ( *( filter + 1 ) == '-' && *( filter + 2 ) && ( *( filter + 2 ) != ']' || *( filter + 3 ) == ']' ) ) {
					if ( casesensitive ) {
						if ( *name >= *filter && *name <= *( filter + 2 ) ) {
							found = qtrue;
						}
					} else {
						if ( toupper( *name ) >= toupper( *filter ) &&
							 toupper( *name ) <= toupper( *( filter + 2 ) ) ) {
							found = qtrue;
						}
					}
					filter += 3;
				} else {
					if ( casesensitive ) {
						if ( *filter == *name ) {
							found = qtrue;
						}
					} else {
						if ( toupper( *filter ) == toupper( *name ) ) {
							found = qtrue;
						}
					}
					filter++;
				}
			}
			if ( !found ) {
				return qfalse;
			}
			while ( *filter ) {
				if ( *filter == ']' && *( filter + 1 ) != ']' ) {
					break;
				}
				filter++;
			}
			filter++;
			name++;
		} else {
			if ( casesensitive ) {
				if ( *filter != *name ) {
					return qfalse;
				}
			} else {
				if ( toupper( *filter ) != toupper( *name ) ) {
					return qfalse;
				}
			}
			filter++;
			name++;
		}
	}
	return qtrue;
}

/*
============
Com_FilterPath
============
*/
int Com_FilterPath( char *filter, char *name, int casesensitive ) {
	int i;
	char new_filter[MAX_QPATH];
	char new_name[MAX_QPATH];

	for ( i = 0; i < MAX_QPATH - 1 && filter[i]; i++ ) {
		if ( filter[i] == '\\' || filter[i] == ':' ) {
			new_filter[i] = '/';
		} else {
			new_filter[i] = filter[i];
		}
	}
	new_filter[i] = '\0';
	for ( i = 0; i < MAX_QPATH - 1 && name[i]; i++ ) {
		if ( name[i] == '\\' || name[i] == ':' ) {
			new_name[i] = '/';
		} else {
			new_name[i] = name[i];
		}
	}
	new_name[i] = '\0';
	return Com_Filter( new_filter, new_name, casesensitive );
}

qboolean Com_InError()
{
	return com_errorEntered;
}

/*
=============
Com_Error

Both client and server can use this, and it will
do the appropriate thing.
=============
*/
void QDECL Com_Error( int code, const char *fmt, ... ) {
	va_list		argptr;
	static int	lastErrorTime;
	static int	errorCount;
	static int	lastErrorCode;
	static qboolean mainThreadInError;
	int		currentTime;
	jmp_buf*	abortframe;
	mvabuf;


	if(com_developer && com_developer->integer > 1)
		__builtin_trap ( );
		
	Sys_EnterCriticalSection(CRIT_ERROR);
	
	if(Sys_IsMainThread() == qfalse)
	{
		com_errorEntered = qtrue;
		
		va_start (argptr,fmt);
		Q_vsnprintf (com_errorMessage, sizeof(com_errorMessage),fmt,argptr);
		va_end (argptr);
		lastErrorCode = code;
		/* Terminate this thread and wait for the main-thread entering this function */
		Sys_LeaveCriticalSection(CRIT_ERROR);
		Sys_ExitThread(-1);
		return;
	}
	/* Main thread can't be twice in this function at same time */
	Sys_LeaveCriticalSection(CRIT_ERROR);

	if(mainThreadInError == qtrue)
	{
		/* Com_Error() entered while shutting down. Now a fast shutdown! */
		Sys_Error ("%s", com_errorMessage);
		return;
	}
	mainThreadInError = qtrue;
	
	if(com_errorEntered == qfalse)
	{
		com_errorEntered = qtrue;
		
		va_start (argptr,fmt);
		Q_vsnprintf (com_errorMessage, sizeof(com_errorMessage),fmt,argptr);
		va_end (argptr);
		lastErrorCode = code;
	
	}
	
	code = lastErrorCode;
	
	Cvar_RegisterInt("com_errorCode", code, code, code, CVAR_ROM, "The last calling error code");
	
	// if we are getting a solid stream of ERR_DROP, do an ERR_FATAL
	currentTime = Sys_Milliseconds();
	if ( currentTime - lastErrorTime < 400 ) {
		if ( ++errorCount > 3 ) {
			code = ERR_FATAL;
		}
	} else {
		errorCount = 0;
	}

	lastErrorTime = currentTime;
	abortframe = (jmp_buf*)Sys_GetValue(2);


	if (code != ERR_DISCONNECT)
		Cvar_RegisterString("com_errorMessage", com_errorMessage, CVAR_ROM, "The last calling error message");

	if (code == ERR_DISCONNECT || code == ERR_SERVERDISCONNECT) {
		SV_Shutdown( "Server disconnected" );
		// make sure we can get at our local stuff
		/*FS_PureServerSetLoadedPaks("", "");*/
		com_errorEntered = qfalse;
		mainThreadInError = qfalse;
		longjmp(*abortframe, -1);
	} else if (code == ERR_DROP) {
		Com_Printf ("********************\nERROR: %s\n********************\n", com_errorMessage);
		SV_Shutdown (va("Server crashed: %s",  com_errorMessage));
		/*FS_PureServerSetLoadedPaks("", "");*/
		com_errorEntered = qfalse;
		mainThreadInError = qfalse;
		longjmp (*abortframe, -1);
	} else {
		SV_Shutdown(va("Server fatal crashed: %s", com_errorMessage));
	}
	NET_Shutdown();
	Com_CloseLogFiles( );
	Sys_Error ("%s", com_errorMessage);
	
}




//==================================================================

void Com_WriteConfigToFile( const char *filename ) {
	fileHandle_t	f;

	f = FS_FOpenFileWrite( filename );
	if ( !f ) {
		Com_Printf ("Couldn't write %s.\n", filename );
		return;
	}

	FS_Printf (f, "// generated by quake, do not modify\n");
	Cvar_WriteVariables (f);
	FS_FCloseFile( f );
}


/*
===============
Com_WriteConfiguration

Writes key bindings and archived cvars to config file if modified
===============
*/
void Com_WriteConfiguration( void ) {

	// if we are quiting without fully initializing, make sure
	// we don't write out anything
	if ( !com_fullyInitialized ) {
		return;
	}

	if ( !(cvar_modifiedFlags & CVAR_ARCHIVE ) ) {
		return;
	}
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	Com_WriteConfigToFile( Q3CONFIG_CFG );

}


/*
===============
Com_WriteConfig_f

Write the config file to a specific name
===============
*/
void Com_WriteConfig_f( void ) {
	char	filename[MAX_QPATH];

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: writeconfig <filename>\n" );
		return;
	}

	Q_strncpyz( filename, Cmd_Argv(1), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );
	Com_Printf( "Writing %s.\n", filename );
	Com_WriteConfigToFile( filename );
}

