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
#include "qcommon_mem.h"
#include "qcommon.h"
#include "cvar.h"
#include "cmd.h"
#include "msg.h"
#include "server.h"
#include "plugin_handler.h"
#include "net_game_conf.h"
#include "misc.h"
#include "g_sv_shared.h"
#include "g_shared.h"
#include "q_platform.h"
#include "punkbuster.h"
#include "sys_thread.h"
#include "sys_main.h"
#include "scr_vm.h"
#include "xassets.h"
#include "nvconfig.h"
#include "hl2rcon.h"

#include <string.h>
#include <stdarg.h>
#include <unistd.h>

cvar_t	*sv_protocol;
cvar_t	*sv_privateClients;		// number of clients reserved for password
cvar_t	*sv_hostname;
#ifdef PUNKBUSTER
cvar_t	*sv_punkbuster;
#endif
cvar_t	*sv_minPing;
cvar_t	*sv_maxPing;
cvar_t	*sv_queryIgnoreMegs;
cvar_t	*sv_queryIgnoreTime;
cvar_t	*sv_privatePassword;		// password for the privateClient slots
cvar_t	*sv_allowDownload;
cvar_t	*sv_wwwDownload;
cvar_t	*sv_wwwBaseURL;
cvar_t	*sv_wwwDlDisconnected;
cvar_t	*sv_voice;
cvar_t	*sv_voiceQuality;
cvar_t	*sv_cheats;
cvar_t	*sv_rconPassword;		// password for remote server commands
cvar_t	*sv_reconnectlimit;		// minimum seconds between connect messages
cvar_t	*sv_padPackets;			// add nop bytes to messages
cvar_t	*sv_mapRotation;
cvar_t	*sv_mapRotationCurrent;
cvar_t	*sv_nextmap;
cvar_t	*sv_paused;
cvar_t	*sv_killserver;			// menu system can set to 1 to shut server down
cvar_t	*sv_timeout;			// seconds without any message while in game
cvar_t	*sv_connectTimeout;		// seconds without any message while connecting
cvar_t	*sv_zombieTime;			// seconds to sink messages after disconnect
cvar_t	*sv_consayname;
cvar_t	*sv_contellname;
cvar_t	*sv_password;
cvar_t	*g_motd;
cvar_t	*sv_modStats;
cvar_t	*sv_authorizemode;
cvar_t	*sv_showasranked;
cvar_t	*sv_statusfile;
cvar_t	*g_friendlyPlayerCanBlock;
cvar_t	*g_FFAPlayerCanBlock;
cvar_t	*sv_autodemorecord;
cvar_t	*sv_demoCompletedCmd;
cvar_t	*sv_mapDownloadCompletedCmd;
cvar_t	*sv_master[MAX_MASTER_SERVERS];	// master server ip address
cvar_t	*g_mapstarttime;
cvar_t	*sv_uptime;

cvar_t* sv_g_gametype;
cvar_t* sv_mapname;
cvar_t* sv_maxclients;
cvar_t* sv_clientSideBullets;
cvar_t* sv_maxRate;
cvar_t* sv_floodProtect;
cvar_t* sv_showcommands;
cvar_t* sv_iwds;
cvar_t* sv_iwdNames;
cvar_t* sv_referencedIwds;
cvar_t* sv_referencedIwdNames;
cvar_t* sv_FFCheckSums;
cvar_t* sv_FFNames;
cvar_t* sv_referencedFFCheckSums;
cvar_t* sv_referencedFFNames;
cvar_t* sv_serverid;
cvar_t* sv_pure;
cvar_t* sv_fps;
cvar_t* sv_showAverageBPS;
cvar_t* sv_botsPressAttackBtn;
cvar_t* sv_debugRate;
cvar_t* sv_debugReliableCmds;
cvar_t* sv_clientArchive;
cvar_t* sv_shownet;
serverStaticExt_t	svse;	// persistant server info across maps
permServerStatic_t	psvs;	// persistant even if server does shutdown

#define SV_OUTPUTBUF_LENGTH 1024

/*
cvar_t	*sv_fps = NULL;			// time rate for running non-clients
cvar_t	*sv_timeout;			// seconds without any message
cvar_t	*sv_zombietime;			// seconds to sink messages after disconnect

cvar_t	*sv_showloss;			// report when usercmds are lost


cvar_t	*sv_mapname;
cvar_t	*sv_mapChecksum;
cvar_t	*sv_serverid;
cvar_t	*sv_minRate;


cvar_t	*sv_pure;

cvar_t	*sv_lanForceRate; // dedicated 1 (LAN) server forces local client rates to 99999 (bug #491)
#ifndef STANDALONE
cvar_t	*sv_strictAuth;
#endif

cvar_t	*sv_banFile;

serverBan_t serverBans[SERVER_MAXBANS];
int serverBansCount = 0;
*/

#define MASTERSERVERSECRETLENGTH 64

static netadr_t	master_adr[MAX_MASTER_SERVERS][2];
static char masterServerSecret[MASTERSERVERSECRETLENGTH +1];

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
void SV_DumpReliableCommands( client_t *client, const char* cmd) {

	if(!com_developer || com_developer->integer < 2)
		return;


	char msg[1040];

	Com_sprintf(msg, sizeof(msg), "Cl: %i, Seq: %i, Time: %i, NotAck: %i, Len: %i, Msg: %s\n",
		client - svs.clients, client->reliableSequence, svs.time ,client->reliableSequence - client->reliableAcknowledge, strlen(cmd), cmd);

	Com_Printf("^5%s", msg);

	Sys_EnterCriticalSection(5);

	if ( com_logfile && com_logfile->integer ) {
    // TTimo: only open the qconsole.log if the filesystem is in an initialized state
    //   also, avoid recursing in the qconsole.log opening (i.e. if fs_debug is on)
	    if ( !reliabledump && FS_Initialized()) {
			struct tm *newtime;
			time_t aclock;

			time( &aclock );
			newtime = localtime( &aclock );

			reliabledump = FS_FOpenFileWrite( "reliableCmds.log" );

			if ( com_logfile->integer > 1 && reliabledump) {
				// force it to not buffer so we get valid
				// data even if we are crashing
				FS_ForceFlush(reliabledump);
			}
			if ( reliabledump ) FS_Write(va("\nLogfile opened on %s\n", asctime( newtime )), strlen(va("\nLogfile opened on %s\n", asctime( newtime ))), reliabledump);
	    }
	    if ( reliabledump && FS_Initialized()) {
		FS_Write(msg, strlen(msg), reliabledump);
	    }
	}
	Sys_LeaveCriticalSection(5);
}
*/

/*
======================
SV_AddServerCommand

The given command will be transmitted to the client, and is guaranteed to
not have future snapshot_t executed before it is executed
======================
*/

void sub_5310E0(client_t *client)
{
	int v1;
	int i;

	v1 = client->reliableSent + 1;

	for(i = client->reliableSent + 1 ; i <= client->reliableSequence; ++i)
	{
		if ( client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)].cmdType )
		{
			if ( (v1 & (MAX_RELIABLE_COMMANDS - 1)) != (i & (MAX_RELIABLE_COMMANDS - 1)) )
			{
				memcpy(&client->reliableCommands[v1 & (MAX_RELIABLE_COMMANDS - 1)], &client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)], sizeof(reliableCommands_t));
			}
			++v1;
		}
	}
	client->reliableSequence = v1 - 1;
}




int sub_530FC0(client_t *client, const char *command)
{

	int i;

	for( i = client->reliableSent + 1; i <= client->reliableSequence; ++i)
	{

		if ( client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)].cmdType == 0 )
			continue;	

		if(client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)].command[0] != command[0])
			continue;
		
		if ( command[0] >= 120 && command[0] <= 122 )
			continue;
			
		if ( !strcmp(&command[1], &client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)].command[1]) )
			return i;
		

		switch ( command[0] )
		{
			case 100:
			case 118:
				if ( !I_IsEqualUnitWSpace( (char*)&command[2], &client->reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)].command[2]))
				{
					continue;
				}
			case 67:
			case 68:
			case 97:
			case 98:
			case 111:
			case 112:
			case 113:
			case 114:
			case 116:
				return i;

			default:
				continue;

		}
	}
	return -1;
}



void __cdecl SV_AddServerCommand(client_t *client, int type, const char *cmd)
{
  int v4;
  int i;
  int j;
  int index;
  char string[64];

    if(client->netchan.remoteAddress.type == NA_BOT)
    {
        return;
    }
	if ( client->canNotReliable )
		return;
	
	if ( client->reliableSequence - client->reliableAcknowledge >= MAX_RELIABLE_COMMANDS / 2 || client->state != CS_ACTIVE)
	{
		sub_5310E0(client);

		if(!type)
			return;

	}
	
	v4 = sub_530FC0(client, cmd);

    if ( v4 < 0 )
    {
        ++client->reliableSequence;
    }
    else
    {
        for ( i = v4 + 1; i <= client->reliableSequence; ++v4 )
        {
          memcpy(&client->reliableCommands[v4 & 0x7F], &client->reliableCommands[i++ & 0x7F], sizeof(reliableCommands_t));
        }
    }

    if ( client->reliableSequence - client->reliableAcknowledge == (MAX_RELIABLE_COMMANDS + 1) )
    {
	Com_PrintNoRedirect("Client: %i lost reliable commands\n", client - svs.clients);
        Com_PrintNoRedirect("===== pending server commands =====\n");
        for ( j = client->reliableAcknowledge + 1; j <= client->reliableSequence; ++j )
	{
		Com_PrintNoRedirect("cmd %5d: %8d: %s\n", j, client->reliableCommands[j & (MAX_RELIABLE_COMMANDS - 1)].cmdTime, &client->reliableCommands[j & (MAX_RELIABLE_COMMANDS - 1)].command);
	}
	Com_PrintNoRedirect("cmd %5d: %8d: %s\n", j, svs.time, cmd);

	NET_OutOfBandPrint( NS_SERVER, &client->netchan.remoteAddress, "disconnect" );
	SV_DelayDropClient(client, "EXE_SERVERCOMMANDOVERFLOW");

        type = 1;
        Com_sprintf(string,sizeof(string),"%c \"EXE_SERVERCOMMANDOVERFLOW\"", 119);
        cmd = string;
    }

    index = client->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
    MSG_WriteReliableCommandToBuffer(cmd, client->reliableCommands[ index ].command, sizeof( client->reliableCommands[ index ].command ));
    client->reliableCommands[ index ].cmdTime = svs.time;
    client->reliableCommands[ index ].cmdType = type;
//    Com_Printf("ReliableCommand: %s\n", cmd);
}




/*
=================
SV_SendServerCommand

Sends a reliable command string to be interpreted by
the client game module: "cp", "print", "chat", etc
A NULL client will broadcast to all clients
=================
*/
/*known stuff
"t \" ==  open callvote screen
"h \" ==  chat
"c \" ==  print bold to players screen
"e \" ==  print to players console
*/
void QDECL SV_SendServerCommandString(client_t *cl, int type, char *message)
{
	client_t	*client;
	int		j;

	if ( cl != NULL ){
		SV_AddServerCommand(cl, type, (char *)message );
		return;
	}

	// hack to echo broadcast prints to console
	if ( !strncmp( (char *)message, "say", 3) ) {
		Com_Printf("broadcast: %s\n", SV_ExpandNewlines((char *)message) );
	}

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < sv_maxclients->integer; j++, client++) {
		if ( client->state < CS_PRIMED ) {
			continue;
		}
		SV_AddServerCommand(client, type, (char *)message );
	}
}

void QDECL SV_SendServerCommand_IW(client_t *cl, int cmdtype, const char *fmt, ...) {

	va_list		argptr;
	byte		message[MAX_MSGLEN];


	va_start (argptr,fmt);
	Q_vsnprintf ((char *)message, sizeof(message), fmt,argptr);
	va_end (argptr);
	SV_SendServerCommandString(cl, cmdtype, (char *)message);

}

void QDECL SV_SendServerCommand(client_t *cl, const char *fmt, ...) {
	va_list		argptr;
	byte		message[MAX_MSGLEN];

	va_start (argptr,fmt);
	Q_vsnprintf ((char *)message, sizeof(message), fmt,argptr);
	va_end (argptr);

	SV_SendServerCommandString(cl, 0, (char *)message);

}


/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

typedef struct leakyBucket_s leakyBucket_t;
struct leakyBucket_s {

	byte	type;

	union {
		byte	_4[4];
		byte	_6[16];
	} ipv;

	unsigned long long	lastTime;
	signed char	burst;

	long		hash;

	leakyBucket_t *prev, *next;
};


typedef struct{

    int max_buckets;
    int max_hashes;
    leakyBucket_t *buckets;
    leakyBucket_t **bucketHashes;
    int queryLimitsEnabled;
    leakyBucket_t infoBucket;
    leakyBucket_t statusBucket;
    leakyBucket_t rconBucket;
}queryLimit_t;


    static queryLimit_t querylimit;



// This is deliberately quite large to make it more of an effort to DoS

/*
================
SVC_RateLimitInit

Init the rate limit system
================
*/
static void SVC_RateLimitInit( ){

	int bytes;

	if(!sv_queryIgnoreMegs->integer)
	{
		Com_Printf("QUERY LIMIT: Querylimiting is disabled\n");
		querylimit.queryLimitsEnabled = 0;
		return;
	}

	bytes = sv_queryIgnoreMegs->integer * 1024*1024;

	querylimit.max_buckets = bytes / sizeof(leakyBucket_t);
	querylimit.max_hashes = 4096; //static

	int totalsize = querylimit.max_buckets * sizeof(leakyBucket_t) + querylimit.max_hashes * sizeof(leakyBucket_t*);

	querylimit.buckets = Z_Malloc(totalsize);

	if(!querylimit.buckets)
	{
		Com_PrintError("QUERY LIMIT: System is out of memory. All queries are disabled\n");
		querylimit.queryLimitsEnabled = -1;
	}

	querylimit.bucketHashes = (leakyBucket_t**)&querylimit.buckets[querylimit.max_buckets];
	Com_Printf("QUERY LIMIT: Querylimiting is enabled\n");
	querylimit.queryLimitsEnabled = 1;
}



/*
================
SVC_HashForAddress
================
*/
__optimize3 __regparm1 static long SVC_HashForAddress( netadr_t *address ) {
	byte 		*ip = NULL;
	size_t	size = 0;
	int			i;
	long		hash = 0;

	switch ( address->type ) {
		case NA_IP:  ip = address->ip;  size = 4; break;
		case NA_IP6: ip = address->ip6; size = 16; break;
		default: break;
	}

	for ( i = 0; i < size; i++ ) {
		hash += (long)( ip[ i ] ) * ( i + 119 );
	}

	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) ^ psvs.randint);
	hash &= ( querylimit.max_hashes - 1 );

	return hash;
}

/*
================
SVC_BucketForAddress

Find or allocate a bucket for an address
================
*/
__optimize3 __regparm3 static leakyBucket_t *SVC_BucketForAddress( netadr_t *address, int burst, int period ) {
	leakyBucket_t		*bucket = NULL;
	int			i;
	long			hash = SVC_HashForAddress( address );
	unsigned long long	now = com_uFrameTime;

	for ( bucket = querylimit.bucketHashes[ hash ]; bucket; bucket = bucket->next ) {

		switch ( bucket->type ) {
			case NA_IP:
				if ( memcmp( bucket->ipv._4, address->ip, 4 ) == 0 ) {
					return bucket;
				}
				break;

			case NA_IP6:
				if ( memcmp( bucket->ipv._6, address->ip6, 16 ) == 0 ) {
					return bucket;
				}
				break;

			default:
				break;
		}

	}

	for ( i = 0; i < querylimit.max_buckets; i++ ) {
		int interval;

		bucket = &querylimit.buckets[ i ];
		interval = now - bucket->lastTime;

		// Reclaim expired buckets
		if ( bucket->lastTime > 0 && ( interval > ( burst * period ) || interval < 0 ) ) 
		{
			if ( bucket->prev != NULL ) {
				bucket->prev->next = bucket->next;
			} else {
				querylimit.bucketHashes[ bucket->hash ] = bucket->next;
			}

			if ( bucket->next != NULL ) {
				bucket->next->prev = bucket->prev;
			}

			Com_Memset( bucket, 0, sizeof( leakyBucket_t ) );
		}

		if ( bucket->type == NA_BAD ) {
			bucket->type = address->type;
			switch ( address->type ) {
				case NA_IP:	bucket->ipv._4[0] = address->ip[0]; 
						bucket->ipv._4[1] = address->ip[1];
						bucket->ipv._4[2] = address->ip[2];
						bucket->ipv._4[3] = address->ip[3];
						break;

				case NA_IP6: Com_Memcpy( bucket->ipv._6, address->ip6, 16 ); break;

				default: return NULL;
			}

			bucket->lastTime = now;
			bucket->burst = 0;
			bucket->hash = hash;

			// Add to the head of the relevant hash chain
			bucket->next = querylimit.bucketHashes[ hash ];
			if ( querylimit.bucketHashes[ hash ] != NULL ) {
				querylimit.bucketHashes[ hash ]->prev = bucket;
			}

			bucket->prev = NULL;
			querylimit.bucketHashes[ hash ] = bucket;

			return bucket;
		}
	}

	// Couldn't allocate a bucket for this address
	return NULL;
}


/*
================
SVC_RateLimit
================
*/
/*__optimize3 __attribute__((always_inline)) */
static qboolean SVC_RateLimit( leakyBucket_t *bucket, int burst, int period ) {
	if ( bucket != NULL ) {
		unsigned long long now = com_uFrameTime;
		int interval = now - bucket->lastTime;
		int expired = interval / period;
		int expiredRemainder = interval % period;

		if ( expired > bucket->burst ) {
			bucket->burst = 0;
			bucket->lastTime = now;
		} else {
			bucket->burst -= expired;
			bucket->lastTime = now - expiredRemainder;
		}

		if ( bucket->burst < burst ) {
			bucket->burst++;

			return qfalse;
		}
	}

	return qtrue;
}

/*
================
SVC_RateLimitAddress

Rate limit for a particular address
================
*/
__optimize3 __regparm3 static qboolean SVC_RateLimitAddress( netadr_t *from, int burst, int period ) {

	if(Sys_IsLANAddress(from))
		return qfalse;


	if(querylimit.queryLimitsEnabled == 1)
	{
		leakyBucket_t *bucket = SVC_BucketForAddress( from, burst, period );
		return SVC_RateLimit( bucket, burst, period );

	}else if(querylimit.queryLimitsEnabled == 0){
		return qfalse;

	}else{//Init error, to be save we deny everything
		return qtrue;
	}

}


/*
================
SVC_Status

Responds with all the info that qplug or qspy can see about the server
and all connected players.  Used for getting detailed information after
the simple info query.
================
*/

__optimize3 __regparm1 void SVC_Status( netadr_t *from ) {
	char player[1024];
	char status[MAX_MSGLEN];
	int i;
	client_t    *cl;
	gclient_t *gclient;
	int statusLength;
	int playerLength;
	char infostring[MAX_INFO_STRING];
	mvabuf;



	// Allow getstatus to be DoSed relatively easily, but prevent
	// excess outbound bandwidth usage when being flooded inbound
	if ( SVC_RateLimit( &querylimit.statusBucket, 20, 20000 ) ) {
	//	Com_DPrintf( "SVC_Status: overall rate limit exceeded, dropping request\n" );
		return;
	}

	// Prevent using getstatus as an amplifier
	if ( SVC_RateLimitAddress( from, 2, sv_queryIgnoreTime->integer*1000 ) ) {
	//	Com_DPrintf( "SVC_Status: rate limit from %s exceeded, dropping request\n", NET_AdrToString( *from ) );
		return;
	}


	if(strlen(SV_Cmd_Argv(1)) > 128)
		return;

	strcpy( infostring, Cvar_InfoString( CVAR_SERVERINFO | CVAR_NORESTART) );
	// echo back the parameter to status. so master servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	Info_SetValueForKey( infostring, "challenge", SV_Cmd_Argv( 1 ) );

	if(*sv_password->string)
	    Info_SetValueForKey( infostring, "pswrd", "1");

	if(sv_authorizemode->integer == 1)		//Backward compatibility
		Info_SetValueForKey( infostring, "type", "1");
	else
		Info_SetValueForKey( infostring, "type", va("%i", sv_authorizemode->integer));
	// add "demo" to the sv_keywords if restricted

	status[0] = 0;
	statusLength = 0;

	for ( i = 0, gclient = level.clients ; i < sv_maxclients->integer ; i++, gclient++ ) {
		cl = &svs.clients[i];
		if ( cl->state >= CS_CONNECTED ) {
			Com_sprintf( player, sizeof( player ), "%i %i \"%s\"\n",
						 gclient->pers.scoreboard.score, cl->ping, cl->name );
			playerLength = strlen( player );
			if ( statusLength + playerLength >= sizeof( status ) ) {
				break;      // can't hold any more
			}
			strcpy( status + statusLength, player );
			statusLength += playerLength;
		}
	}
	NET_OutOfBandPrint( NS_SERVER, from, "statusResponse\n%s\n%s", infostring, status );
}


/*
================
SVC_Info

Responds with a short info message that should be enough to determine
if a user is interested in a server to do a full status
================
*/
__optimize3 __regparm1 void SVC_Info( netadr_t *from ) {
	int		i, count, humans;
	char		infostring[MAX_INFO_STRING];
	char*		s;
	mvabuf;


	s = SV_Cmd_Argv(1);

	// Allow getstatus to be DoSed relatively easily, but prevent
	// excess outbound bandwidth usage when being flooded inbound
	if ( SVC_RateLimit( &querylimit.infoBucket, 100, 100000 ) ) {
	//	Com_DPrintf( "SVC_Info: overall rate limit exceeded, dropping request\n" );
		return;
	}

	// Prevent using getstatus as an amplifier
	if ( SVC_RateLimitAddress( from, 4, sv_queryIgnoreTime->integer*1000 )) {
	//	Com_DPrintf( "SVC_Info: rate limit from %s exceeded, dropping request\n", NET_AdrToString( *from ) );
		return;
	}
	infostring[0] = 0;


	/*
	 * Check whether Cmd_Argv(1) has a sane length. This was not done in the original Quake3 version which led
	 * to the Infostring bug discovered by Luigi Auriemma. See http://aluigi.altervista.org/ for the advisory.
	 */

	// A maximum challenge length of 128 should be more than plenty.
	if(strlen(SV_Cmd_Argv(1)) > 128)
		return;

	// don't count privateclients
	count = humans = 0;
	for ( i = 0 ; i < sv_maxclients->integer ; i++ )
	{
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			count++;
			if (svs.clients[i].netchan.remoteAddress.type != NA_BOT) {
				humans++;
			}
		}
	}

	// echo back the parameter to status. so servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers

	Info_SetValueForKey( infostring, "challenge", s);


	//Info_SetValueForKey( infostring, "gamename", com_gamename->string );
#ifdef COD4X17A
	Info_SetValueForKey(infostring, "protocol", va("%d", sv_protocol->integer));
#else
	Info_SetValueForKey(infostring, "protocol", "6");
#endif
	Info_SetValueForKey( infostring, "hostname", sv_hostname->string );

	if(sv_authorizemode->integer == 1)		//Backward compatibility
		Info_SetValueForKey( infostring, "type", "1");
	else
		Info_SetValueForKey( infostring, "type", va("%i", sv_authorizemode->integer));

	Info_SetValueForKey( infostring, "mapname", sv_mapname->string );
	Info_SetValueForKey( infostring, "clients", va("%i", count) );
	Info_SetValueForKey( infostring, "g_humanplayers", va("%i", humans));
	Info_SetValueForKey( infostring, "sv_maxclients", va("%i", sv_maxclients->integer - sv_privateClients->integer ) );
	Info_SetValueForKey( infostring, "gametype", sv_g_gametype->string );
	Info_SetValueForKey( infostring, "pure", va("%i", sv_pure->boolean ) );
	Info_SetValueForKey( infostring, "build", va("%i", BUILD_NUMBER));
	Info_SetValueForKey( infostring, "shortversion", Q3_VERSION );

        if(*sv_password->string)
	    Info_SetValueForKey( infostring, "pswrd", "1");
	else
	    Info_SetValueForKey( infostring, "pswrd", "0");

	
	    Info_SetValueForKey( infostring, "ff", va("%d", Cvar_VariableIntegerValue("scr_team_fftype")));

        if(Cvar_GetVariantString("scr_game_allowkillcam")){
	    Info_SetValueForKey( infostring, "ki", "1");
	}

        if(Cvar_GetVariantString("scr_hardcore")){
	    Info_SetValueForKey( infostring, "hc", "1");
	}

        if(Cvar_GetVariantString("scr_oldschool")){
	    Info_SetValueForKey( infostring, "od", "1");
	}
	Info_SetValueForKey( infostring, "hw", "1");

        if(fs_gameDirVar->string[0] == '\0' || sv_showasranked->boolean){
	    Info_SetValueForKey( infostring, "mod", "0");
	}else{
	    Info_SetValueForKey( infostring, "mod", "1");
	}
	Info_SetValueForKey( infostring, "voice", va("%i", sv_voice->boolean ) );
#ifdef PUNKBUSTER
	Info_SetValueForKey( infostring, "pb", va("%i", sv_punkbuster->boolean) );
#endif
	if( sv_maxPing->integer ) {
		Info_SetValueForKey( infostring, "sv_maxPing", va("%i", sv_maxPing->integer) );
	}

	if( fs_gameDirVar->string[0] != '\0' ) {
		Info_SetValueForKey( infostring, "game", fs_gameDirVar->string );
	}

	NET_OutOfBandPrint( NS_SERVER, from, "infoResponse\n%s", infostring );
}

#if 0

typedef struct
{
	int challenge;
	int protocol;
	char hostname[256];
	char mapname[64];
	char gamemoddir[64];
	char gamename[64];
	unsigned short steamappid;
	int numplayers;
	int maxclients;
	int numbots;
	char hostos;
	qboolean joinpassword;
	qboolean secure;
	char gameversion[64];
	unsigned int steamid_lower;
	unsigned int steamid_upper;
	unsigned int gameid_lower;
	unsigned int gameid_upper;
	unsigned short joinport;
}queryinfo_t;

typedef struct{
	int challenge;
	int receivedchunks;
	int numchunks;
	char splitmessage[8192];
	int frameseq;
}querysplitmsg_t;


typedef struct{
	int cid;
	char name[32];
	int score;
	float connectedtime;
}queryplayer_t;

typedef struct{
	int count;
	queryplayer_t players[128];
}queryplayers_t;

typedef struct
{
	char name[64];
	char value[256];
}queryrule_t;

typedef struct{
	int count;
	queryrule_t rules[256];
}queryrules_t;

//void CLC_SourceEngineQuery_Players(msg_t* msg, queryinfo_t* query)


void CLC_SourceEngineQuery_Info(msg_t* msg, queryinfo_t* query)
{
	int extrafields;
	char stringbuf[8192];
	
	//OBB-Header already read
	MSG_ReadLong(msg);
	//I message is already read
	MSG_ReadByte(msg);
	
	//Start of message
	query->protocol = MSG_ReadByte(msg);
	MSG_ReadString(msg, query->hostname, sizeof(query->hostname));
	MSG_ReadString(msg, query->mapname, sizeof(query->mapname));
	MSG_ReadString(msg, query->gamemoddir, sizeof(query->gamemoddir));
	MSG_ReadString(msg, query->gamename, sizeof(query->gamename));
	query->steamappid = MSG_ReadShort(msg);
	query->numplayers = MSG_ReadByte(msg);
	query->maxclients = MSG_ReadByte(msg);
	query->numbots = MSG_ReadByte(msg);
	//Reading 'd' ?
	MSG_ReadByte(msg);
	//'l', 'm', 'w'
	query->hostos = MSG_ReadByte(msg);
	query->joinpassword = MSG_ReadByte(msg);
	//Reading 0 ?
	query->secure = MSG_ReadByte(msg);
	MSG_ReadString(msg, query->gameversion, sizeof(query->gameversion));
	
	/* The extra datafields */
	extrafields = MSG_ReadByte(msg);

	if (extrafields & 0x80)
	{
		//Read the join port
		query->joinport = MSG_ReadShort(msg);
	}
	if(extrafields & 0x10)
	{
		//Read the steam id
		query->steamid_lower = MSG_ReadLong(msg);
		query->steamid_upper = MSG_ReadLong(msg);		
	}
	if(extrafields & 0x40)
	{
		//Read the sourceTV stuff
		MSG_ReadShort(msg);
		MSG_ReadString(msg, stringbuf, sizeof(stringbuf));
	}
	if(extrafields & 0x20)
	{
		//Read the tags (future use)
		MSG_ReadString(msg, stringbuf, sizeof(stringbuf));
	}
	if(extrafields & 0x01)
	{
		query->gameid_lower = MSG_ReadLong(msg);
		query->gameid_upper = MSG_ReadLong(msg);			
	}	
	/* Finished with message of type "I" */

	
}

void CLC_SourceEngineQuery_ReadChallenge(msg_t* msg, queryinfo_t* query)
{
	//OBB-Header already read
	MSG_ReadLong(msg);
	//A message is already read
	MSG_ReadByte(msg);
	
	query->challenge = MSG_ReadLong(msg);
}

void CLC_SourceEngineQuery_ReadSplitMessage(msg_t* msg, querysplitmsg_t* query)
{
	int receivedindex;
	int splitsize;
	
	/* reading the split header */
	MSG_ReadLong(msg);
	/* An unique number */
	query->frameseq = MSG_ReadLong(msg);
	/* Total number of packets */
	query->numchunks = MSG_ReadByte(msg);
	/* Packetnumber */
	receivedindex = MSG_ReadByte(msg);
	if(receivedindex > 31)
	{
		Com_PrintWarning("CLC_SourceEngineQuery_ReadSplitMessage: Received out of range splitmessage index packet\n");
		return;
	}
	
	query->receivedchunks |= (1 << receivedindex);
	/* Splitsize */
	splitsize = MSG_ReadShort(msg);
	if(query->numchunks * splitsize >= sizeof(query->splitmessage))
	{
		Com_PrintWarning("CLC_SourceEngineQuery_ReadSplitMessage: Size of the splitmessage would exceed the splitbuffer size\n");
		return;
	}
	if(receivedindex * splitsize >= (sizeof(query->splitmessage) - splitsize))
	{
		Com_PrintWarning("CLC_SourceEngineQuery_ReadSplitMessage: Received out of range splitmessage buffer packet\n");
		return;
	}
	MSG_ReadData(msg, &query->splitmessage[receivedindex * splitsize], splitsize);
}

void CLC_SourceEngineQuery_ReadPlayer(msg_t* msg, queryplayers_t* query)
{
	int numcl, i;
	//MSG_WriteLong(&playermsg, -1);
	/* Write the Command-Header */
	//MSG_WriteByte(&playermsg, 'D');
	/* numClients is 0 for now */
	query->count = MSG_ReadByte(msg);
	
	if(query->count > (  sizeof(query->players)/sizeof(query->players[0])))
	{
		Com_PrintWarning("CLC_SourceEngineQuery_ReadPlayer: Received out of range player count\n");
		query->count = 0;
		return;
	}
	
	for ( i = 0; i < numcl ; i++)
	{
		query->players[i].cid = MSG_ReadByte(msg);
		MSG_ReadString(msg, query->players[i].name, sizeof(query->players[i].name));
		query->players[i].score = MSG_ReadLong(msg);
		query->players[i].connectedtime = MSG_ReadFloat(msg);
	}
}

void CLC_SourceEngineQuery_ReadRules(msg_t* msg, queryrules_t* query)
{
	int i;
	//MSG_ReadLong(msg);
	/* Write the Command-Header */
	//MSG_ReadByte(msg);
	/* count */
	query->count = MSG_ReadShort(msg);
	
	if(query->count > ( sizeof(query->rules)/sizeof(query->rules[0])))
	{
		Com_PrintWarning("CLC_SourceEngineQuery_ReadPlayer: Received out of range player count\n");
		query->count = sizeof(query->rules)/sizeof(query->rules[0]);
	}
	for ( i = 0; i < query->count ; i++)
	{
		MSG_ReadString(msg, query->rules[i].name, sizeof(query->rules[i].name));
		MSG_ReadString(msg, query->rules[i].value, sizeof(query->rules[i].value));
	}
}
#endif

void SVC_SourceEngineQuery_Info( netadr_t* from, const char* challengeStr, const char* mymastersecret )
{

	msg_t msg;
	int i, humans, bots;
	byte buf[MAX_INFO_STRING];

	qboolean masterserver = qfalse;
	
	if(mymastersecret[0])
	{	
		if(strcmp(mymastersecret, masterServerSecret))
		{
			return;
		}
		masterserver = qtrue;
	}
	
	if(!masterserver)
	{
		// Allow getstatus to be DoSed relatively easily, but prevent
		// excess outbound bandwidth usage when being flooded inbound
		if ( SVC_RateLimit( &querylimit.infoBucket, 100, 100000 ) ) {
			//	Com_DPrintf( "SVC_Info: overall rate limit exceeded, dropping request\n" );
			return;
		}
		
		
		// Prevent using getstatus as an amplifier
		if ( SVC_RateLimitAddress( from, 4, sv_queryIgnoreTime->integer*1000 )) {
			//	Com_DPrintf( "SVC_Info: rate limit from %s exceeded, dropping request\n", NET_AdrToString( *from ) );
			return;
		}
	}

	MSG_Init(&msg, buf, sizeof(buf));
	MSG_WriteLong(&msg, -1);
	MSG_WriteByte(&msg, 'I');
	MSG_WriteByte(&msg, sv_protocol->integer);
	MSG_WriteString(&msg, sv_hostname->string);
	MSG_WriteString(&msg, sv_mapname->string);
	if(fs_gameDirVar->string[0] == '\0')
	{
		MSG_WriteString(&msg, "main");
	}else{
		MSG_WriteString(&msg, fs_gameDirVar->string);
	}
	MSG_WriteString(&msg, "Call of Duty 4 - Modern Warfare");
	MSG_WriteShort(&msg, 7940);

	// don't count privateclients
	bots = humans = 0;
	for ( i = 0 ; i < sv_maxclients->integer ; i++ )
	{
		if ( svs.clients[i].state >= CS_CONNECTED )
		{
			if (svs.clients[i].netchan.remoteAddress.type != NA_BOT) {
				humans++;
			}else{
				bots++;
			}
		}
	}

	MSG_WriteByte(&msg, humans);
	MSG_WriteByte(&msg, sv_maxclients->integer - sv_privateClients->integer);
	MSG_WriteByte(&msg, bots);
	MSG_WriteByte(&msg, 'd');

#ifdef _WIN32
	MSG_WriteByte(&msg, 'w');
#else
    #ifdef MACOS_X
	MSG_WriteByte(&msg, 'm');
    #else
	MSG_WriteByte(&msg, 'l');
    #endif
#endif

        if(*sv_password->string){
		MSG_WriteByte(&msg, 1);
	}else{
		MSG_WriteByte(&msg, 0);
	}
	MSG_WriteByte(&msg, 0);
	MSG_WriteString(&msg, "1.7a");
	/*The extra datafield for port*/
	MSG_WriteByte(&msg, 0x80);
	
	MSG_WriteShort(&msg, NET_GetHostPort());
	
	if(challengeStr[0])
	{
		MSG_WriteString( &msg, challengeStr);
		MSG_WriteString( &msg, sv_g_gametype->string );
		
		MSG_WriteByte( &msg, Cvar_VariableIntegerValue("scr_team_fftype"));
		MSG_WriteByte( &msg, Cvar_VariableBooleanValue("scr_game_allowkillcam"));
		MSG_WriteByte( &msg, Cvar_VariableBooleanValue("scr_hardcore"));
		MSG_WriteByte( &msg, Cvar_VariableBooleanValue("scr_oldschool"));
		MSG_WriteByte( &msg, sv_voice->boolean);

		
		if(masterserver)
		{	
			MSG_WriteLong( &msg, psvs.masterServer_id);
			MSG_WriteLong( &msg, BUILD_NUMBER);
			MSG_WriteString( &msg, masterServerSecret);

		}
	}
	NET_SendPacket(NS_SERVER, msg.cursize, msg.data, from);

}


void SVC_SourceEngineQuery_Challenge( netadr_t* from )
{
	msg_t msg;
	byte buf[MAX_INFO_STRING];

	MSG_Init(&msg, buf, sizeof(buf));

	MSG_WriteLong(&msg, -1);

	MSG_WriteByte(&msg, 'A');

	MSG_WriteLong(&msg, NET_CookieHash(from));

	NET_SendPacket(NS_SERVER, msg.cursize, msg.data, from);
}

#define SPLIT_SIZE 1248

void SVC_SourceEngineQuery_SendSplitMessage( netadr_t* from, msg_t* longmsg )
{
	msg_t msg;
	static int seq;
	byte buf[SPLIT_SIZE + 100];
	int i, numpackets;

	seq++;

	/* In case this packet is short enough */
	if(longmsg->cursize <= SPLIT_SIZE)
	{
		NET_SendPacket(NS_SERVER, longmsg->cursize, longmsg->data, from);
		return;
	}
	/* This will become a split response */
	
	MSG_Init(&msg, buf, sizeof(buf));

	numpackets = 1 + (longmsg->cursize / SPLIT_SIZE);

	for(i = 0; i < numpackets; i++)
	{
		MSG_Clear(&msg);
		/* writing the split header */
		MSG_WriteLong(&msg, -2);
		/* An unique number */
		MSG_WriteLong(&msg, seq);
		/* Total number of packets */
		MSG_WriteByte(&msg, numpackets);
		/* Packetnumber */
		MSG_WriteByte(&msg, i);
		/* Splitsize */
		MSG_WriteShort(&msg, SPLIT_SIZE);

		if(longmsg->cursize - longmsg->readcount > SPLIT_SIZE)
		{
			Com_Memcpy(&msg.data[msg.cursize], &longmsg->data[longmsg->readcount], SPLIT_SIZE);
			longmsg->readcount += SPLIT_SIZE;
			msg.cursize += SPLIT_SIZE;
		}else{
			Com_Memcpy(&msg.data[msg.cursize], &longmsg->data[longmsg->readcount], longmsg->cursize - longmsg->readcount);
			msg.cursize += (longmsg->cursize - longmsg->readcount);
			longmsg->readcount = longmsg->cursize;
		}
		NET_SendPacket(NS_SERVER, msg.cursize, msg.data, from);
	}

}

void SVC_SourceEngineQuery_Player( netadr_t* from, msg_t* recvmsg )
{

	msg_t playermsg;
	byte pbuf[MAX_MSGLEN];

	int i, numClients, challenge;
	client_t    *cl;
	gclient_t *gclient;


	/* 1st check the challenge */
	MSG_BeginReading(recvmsg);
	/* OOB-Header */
	MSG_ReadLong(recvmsg);
	/* Command Header */
	MSG_ReadByte(recvmsg);
	/* Challenge */
	challenge = MSG_ReadLong(recvmsg);

	if(NET_CookieHash(from) != challenge)
	{
		SVC_SourceEngineQuery_Challenge( from );
		return;
	}


	MSG_Init(&playermsg, pbuf, sizeof(pbuf));
	/* Write the OOB-Header */
	MSG_WriteLong(&playermsg, -1);
	/* Write the Command-Header */
	MSG_WriteByte(&playermsg, 'D');
	/* numClients is 0 for now */
	MSG_WriteByte(&playermsg, 0);

	for ( i = 0, cl = svs.clients, gclient = level.clients, numClients = 0; i < sv_maxclients->integer ; i++, gclient++, cl++) {

		if ( cl->state >= CS_CONNECTED ) {

			MSG_WriteByte(&playermsg, i);
			MSG_WriteString(&playermsg, cl->name);
			MSG_WriteLong(&playermsg, gclient->pers.scoreboard.score);
			MSG_WriteFloat(&playermsg, ((float)(svs.time - cl->connectedTime))/1000);
			numClients++;
		}
	}
	/* update the playercount */
	playermsg.data[5] = numClients;

	SVC_SourceEngineQuery_SendSplitMessage( from, &playermsg );

}

struct sourceEngineCvars_s
{
	msg_t* msg;
	int num;
};

void	SVC_SourceEngineQuery_WriteCvars(cvar_t const* cvar, void *var ){
    struct sourceEngineCvars_s *data = var;

    if(cvar->flags & (CVAR_SERVERINFO | CVAR_NORESTART) )
    {
        MSG_WriteString(data->msg, cvar->name);
        MSG_WriteString(data->msg, Cvar_DisplayableValue(cvar));
        data->num++;
    }
}


void SVC_SourceEngineQuery_Rules( netadr_t* from, msg_t* recvmsg )
{
	msg_t msg;
	byte buf[MAX_MSGLEN];
	struct sourceEngineCvars_s data;
	int numvars, challenge;

	/* 1st check the challenge */
	MSG_BeginReading(recvmsg);
	/* OOB-Header */
	MSG_ReadLong(recvmsg);
	/* Command Header */
	MSG_ReadByte(recvmsg);
	/* Challenge */
	challenge = MSG_ReadLong(recvmsg);

	if(NET_CookieHash(from) != challenge)
	{
		SVC_SourceEngineQuery_Challenge( from );
		return;
	}

	numvars = 0;

	MSG_Init(&msg, buf, sizeof(buf));
	/* Write the OOB header */
	MSG_WriteLong(&msg, -1);
	/* Write the Command-Header */
	MSG_WriteByte(&msg, 'E');
	/* Number of rules = 0 for now */
	MSG_WriteShort(&msg, numvars);
	/* Write each cvar */
	data.msg = &msg;
	data.num = 0;
	Cvar_ForEach( SVC_SourceEngineQuery_WriteCvars, &data );

	*(short*)&msg.data[5] = data.num;

	SVC_SourceEngineQuery_SendSplitMessage( from, &msg );

}

#ifndef COD4X17A

void SV_RestartForUpdate(netadr_t* from, char* mymastersecret, char* message)
{
	if(!message[0] || strcmp(mymastersecret, masterServerSecret))
	{
		return;
	}
	
	Sys_Restart(message);
	
}

#endif

/*
================
SVC_FlushRedirect

================
*/
static void SV_FlushRedirect( char *outputbuf, qboolean lastcommand ) {
	NET_OutOfBandPrint( NS_SERVER, &svse.redirectAddress, "print\n%s", outputbuf );
}

/*
===============
SVC_RemoteCommand

An rcon packet arrived from the network.
Shift down the remaining args
Redirect all printfs
===============
*/
__optimize3 __regparm2 static void SVC_RemoteCommand( netadr_t *from, msg_t *msg ) {
	// TTimo - scaled down to accumulate, but not overflow anything network wise, print wise etc.
	// (OOB messages are the bottleneck here)
	char		sv_outputbuf[SV_OUTPUTBUF_LENGTH];
	char *cmd_aux;
	char stringlinebuf[MAX_STRING_CHARS];

	svse.redirectAddress = *from;

	if ( strcmp (SV_Cmd_Argv(1), sv_rconPassword->string )) {
		//Send only one deny answer out in 100 ms
		if ( SVC_RateLimit( &querylimit.rconBucket, 1, 100 ) ) {
		//	Com_DPrintf( "SVC_RemoteCommand: rate limit exceeded for bad rcon\n" );
			return;
		}

		Com_Printf ("Bad rcon from %s\n", NET_AdrToString (from) );
		Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);
		Com_Printf ("Bad rcon");
		Com_EndRedirect ();
		return;
	}

	if ( strlen( sv_rconPassword->string) < 8 ) {
		Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);
		Com_Printf ("No rconpassword set on server or password is shorter than 8 characters.\n");
		Com_EndRedirect ();
		return;
	}

	//Everything fine, process the request

	MSG_BeginReading(msg);
	MSG_ReadLong(msg); //0xffffffff
	MSG_ReadLong(msg); //rcon

	cmd_aux = MSG_ReadStringLine(msg, stringlinebuf, sizeof(stringlinebuf));

	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
	// get the command directly, "rcon <pass> <command>" to avoid quoting issues
	// extract the command by walking
	// since the cmd formatting can fuckup (amount of spaces), using a dumb step by step parsing

	while(cmd_aux[0]==' ')//Skipping space before the password
		cmd_aux++;

	if(cmd_aux[0]== '"')//Skipping the password
	{
		cmd_aux++;
		while(cmd_aux[0] != '"' && cmd_aux[0])
			cmd_aux++;

		cmd_aux++;
	}else{
		while(cmd_aux[0] != ' ' && cmd_aux[0])
			cmd_aux++;

	}

	while(cmd_aux[0] == ' ')//Skipping space after the password
		cmd_aux++;

	Com_Printf ("Rcon from %s: %s\n", NET_AdrToString (from), cmd_aux );

	Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);
#ifdef PUNKBUSTER
	if(!Q_stricmpn(cmd_aux, "pb_sv_", 6)){

		Q_strchrrepl(cmd_aux, '\"', ' ');
		Cmd_ExecuteSingleCommand(0,0, cmd_aux);
		PbServerForceProcess();
	}else
#endif
		Cmd_ExecuteSingleCommand(0,0, cmd_aux);
	
	Com_EndRedirect ();

}

#ifdef COD4X18UPDATE
#define UPDATE_PROXYSERVER_NAME "127.0.0.1"
#define UPDATE_PROXYSERVER_PORT 27953

typedef enum
{
    UPDCONN_CHALLENGING,
    UPDCONN_CONNECT
}update_connState_t;

typedef struct
{
    update_connState_t state;
    int mychallenge;
    int serverchallenge;
    char authkey[128];
    netadr_t updateserveradr;
}update_connection_t;

update_connection_t update_connection;



void SV_UpdateProxyUpdateBadChallenge(netadr_t* from)
{
    int mychallenge;

    if(SV_Cmd_Argc() < 2)
    {
        return;
    }

    mychallenge = atoi(SV_Cmd_Argv(1));

    if(mychallenge != update_connection.mychallenge)
    {
        return;
    }

    if(!NET_CompareAdr(from, &update_connection.updateserveradr))
    {
        Com_Printf("SV_UpdateProxyUpdateBadChallenge: Packet not from updateserver\n");
        return;
    }

    update_connection.state = UPDCONN_CHALLENGING;
    Com_Printf("SV_UpdateProxyUpdateBadChallenge: Will start challenging\n");
}

void SV_UpdateProxyChallengeResponse(netadr_t* from)
{
    int mychallenge;
    int svchallenge;

    if(SV_Cmd_Argc() < 3)
    {
        return;
    }

    mychallenge = atoi(SV_Cmd_Argv(2));

    if(mychallenge != update_connection.mychallenge)
    {
        Com_Printf("SV_UpdateProxyChallengeResponse: Bad challenge\n");
        return;
    }

    if(!NET_CompareAdr(from, &update_connection.updateserveradr))
    {
        Com_Printf("SV_UpdateProxyChallengeResponse: Packet not from updateserver\n");
        return;
    }

    svchallenge = atoi(SV_Cmd_Argv(1));
    update_connection.serverchallenge = svchallenge;
    update_connection.state = UPDCONN_CONNECT;
}

void SV_UpdateProxyConnectResponse( netadr_t* from )
{

    int mychallenge;
    int clchallenge;
    int i;
    unsigned short qport;
    client_t* cl;

    if(SV_Cmd_Argc() < 4)
    {
        return;
    }
    mychallenge = atoi(SV_Cmd_Argv(1));
    if(mychallenge != update_connection.mychallenge)
    {
//        Com_Printf("SV_UpdateProxyConnectResponse: Bad challenge\n");
        return;
    }

    if(!NET_CompareAdr(from, &update_connection.updateserveradr))
    {
//        Com_Printf("SV_UpdateProxyConnectResponse: Packet not from updateserver\n");
        return;
    }

    clchallenge = atoi(SV_Cmd_Argv(2));
    qport = atoi(SV_Cmd_Argv(3));

    for(cl = svs.clients, i = 0; i < sv_maxclients->integer; ++i, ++cl)
    {
        if(cl->state == CS_CONNECTED && cl->challenge == clchallenge && cl->netchan.qport == qport)
        {
            break;
        }
    }

    if(i == sv_maxclients->integer)
    {
//        Com_Printf("SV_UpdateProxyConnectResponse: Bad challenge for client\n");
        return;
    }

    cl->updateconnOK = qtrue;

}

void SV_ReceiveFromUpdateProxy( msg_t *msg )
{
    int i;
    client_t* cl;

    /* Callenge 0x4 */
    int clchallenge = MSG_ReadLong(msg);
    /* sequence 0x8 */
    int sequence = MSG_ReadLong(msg);
    /* qport 0xC */
    unsigned short qport = MSG_ReadShort(msg);

    /* data 0xE */
    for(cl = svs.clients, i = 0; i < sv_maxclients->integer; ++i, ++cl)
    {
        if(cl->state == CS_CONNECTED && cl->challenge == clchallenge && cl->netchan.qport == qport)
        {
            break;
        }
    }

    if(i == sv_maxclients->integer)
    {
//        Com_Printf("SV_ReceiveFromUpdateProxy: Received packet for bad client\n");
        NET_OutOfBandPrint(NS_SERVER, &update_connection.updateserveradr, "disconnect %d %d", update_connection.serverchallenge, clchallenge);
        return;
    }

    *(uint32_t*)&msg->data[10] = sequence;
    NET_SendPacket(NS_SERVER, msg->cursize - 10, msg->data +10, &svs.clients[i].netchan.remoteAddress);

}

void SV_PassToUpdateProxy(msg_t *msg, client_t *cl)
{
    byte outbuf[MAX_MSGLEN];

    msg_t outmsg;

    MSG_Init(&outmsg, outbuf, sizeof(outbuf));

    /* Update packet header */
    MSG_WriteLong(&outmsg, 0xfffffffe);
    /* client challenge */
    MSG_WriteLong(&outmsg, cl->challenge);
    MSG_WriteData(&outmsg, msg->data, msg->cursize);

    NET_SendPacket(NS_SERVER, outmsg.cursize, outmsg.data, &update_connection.updateserveradr);

}

void SV_ConnectWithUpdateProxy(client_t *cl)
{


    int res;
    char info[MAX_STRING_CHARS];
    mvabuf;

    switch(update_connection.state)
    {
        case UPDCONN_CHALLENGING:

            if(update_connection.mychallenge == 0)
            {
                Com_RandomBytes((byte*)&update_connection.mychallenge, sizeof(update_connection.mychallenge));
            }

            if(update_connection.updateserveradr.type == NA_BAD)
            {
                Com_Printf("Resolving %s\n", UPDATE_PROXYSERVER_NAME);
                res = NET_StringToAdr(UPDATE_PROXYSERVER_NAME, &update_connection.updateserveradr, NA_IP);

                if(res == 2)
                {
                    // if no port was specified, use the default master port
                    update_connection.updateserveradr.port = BigShort(UPDATE_PROXYSERVER_PORT);
                }
                if(res)
                {
                    Com_Printf( "%s resolved to %s\n", UPDATE_PROXYSERVER_NAME, NET_AdrToString(&update_connection.updateserveradr));
                }else{
                    Com_Printf( "%s has no IPv4 address.\n", UPDATE_PROXYSERVER_NAME);
                    return;
                }
            }

            if(update_connection.updateserveradr.type == NA_IP)
            {
                NET_OutOfBandPrint( NS_SERVER, &update_connection.updateserveradr, "updgetchallenge %d %s", update_connection.mychallenge, "noguid");
            }
            return;

        case UPDCONN_CONNECT:


            info[0] = '\0';

            Info_SetValueForKey(info, "challenge", va("%d", update_connection.serverchallenge));
            Info_SetValueForKey(info, "rtnchallenge", va("%d", update_connection.mychallenge));
            Info_SetValueForKey(info, "clchallenge", va("%d", cl->challenge));
            Info_SetValueForKey(info, "name", cl->name);
            Info_SetValueForKey(info, "clremote", NET_AdrToString(&cl->netchan.remoteAddress));
            Info_SetValueForKey(info, "qport", va("%hi", cl->netchan.qport));
            Info_SetValueForKey(info, "protocol", va("%hi", cl->protocol));

            NET_OutOfBandPrint( NS_SERVER, &update_connection.updateserveradr, "updconnect \"%s\"", info);
            return;

    }


}


#endif




/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
===========h======
*/
__optimize3 __regparm2 void SV_ConnectionlessPacket( netadr_t *from, msg_t *msg ) {
	char	*s;
	char	*c;
	char	stringlinebuf[MAX_STRING_CHARS];

	MSG_BeginReading( msg );
	MSG_ReadLong( msg );		// skip the -1 marker

	s = MSG_ReadStringLine( msg, stringlinebuf, sizeof(stringlinebuf) );
	SV_Cmd_TokenizeString( s );

	c = SV_Cmd_Argv(0);
	Com_DPrintf ("SV packet %s: %s\n", NET_AdrToString(from), s);
	//Most sensitive OOB commands first
        if (!Q_stricmp(c, "getstatus")) {
		SVC_Status( from );

        } else if (!Q_stricmp(c, "getinfo")) {
		SVC_Info( from );

        } else if (!Q_stricmp(c, "rcon")) {
		SVC_RemoteCommand( from, msg );
	} else if (!Q_stricmp(c, "connect")) {
		SV_DirectConnect( from );
#ifdef COD4X17A
	} else if (!Q_stricmp(c, "ipAuthorize")) {
		SV_AuthorizeIpPacket( from );

	} else if (!Q_stricmp(c, "stats")) {
		SV_ReceiveStats(from, msg);

#else

	} else if (!Q_stricmp(c, "updaterestartinfo")) {
		SV_RestartForUpdate(from, SV_Cmd_Argv(1), SV_Cmd_Argv(2));
#endif

#ifndef COD4X17A
#ifdef COD4X18UPDATE

	} else if (!Q_stricmp(c, "stats")) {
		SV_ReceiveStats(from, msg);

#endif
#endif

        } else if (!Q_stricmp(c, "rcon")) {
		SVC_RemoteCommand( from, msg );

	} else if (!Q_stricmp(c, "getchallenge")) {
		SV_GetChallenge(from);

#ifdef COD4X18UPDATE
	} else if (!Q_stricmp(c, "updbadchallenge")) {
		SV_UpdateProxyUpdateBadChallenge( from );
	} else if (!Q_stricmp(c, "updchallengeResponse")) {
		SV_UpdateProxyChallengeResponse( from );
	} else if (!Q_stricmp(c, "updconnectResponse")) {
		SV_UpdateProxyConnectResponse( from );

#endif



	} else if (!strcmp(c, "v")) {
		SV_GetVoicePacket(from, msg);

	} else if (!Q_strncmp("TSource Engine Query", (char *) &msg->data[4], 20)) {
		SVC_SourceEngineQuery_Info( from, SV_Cmd_Argv(3), SV_Cmd_Argv(4));
	} else if(msg->data[4] == 'V'){
		SVC_SourceEngineQuery_Rules( from, msg );
	} else if(msg->data[4] == 'U'){
		SVC_SourceEngineQuery_Player( from, msg );
	} else if(msg->data[4] == 'W'){
		SVC_SourceEngineQuery_Challenge( from );

#ifdef PUNKBUSTER
	} else if (!Q_strncmp("PB_", (char *) &msg->data[4], 3)) {

		int	clnum;
		int	i;
		client_t *cl;

		//pb_sv_process here
		SV_Cmd_EndTokenizedString();

		if(msg->data[7] == 0x43 || msg->data[7] == 0x31 || msg->data[7] == 0x4a)
		    return;

		for ( i = 0, clnum = -1, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ )
		{
			if ( !NET_CompareBaseAdr( from, &cl->netchan.remoteAddress ) )	continue;
			if(cl->netchan.remoteAddress.port != from->port) continue;
			clnum = i;
			break;
		}

		if(NET_GetDefaultCommunicationSocket() == NULL)
			NET_RegisterDefaultCommunicationSocket(from);

		if(clnum == -1)
			return;
		
		PbSvAddEvent(13, clnum, msg->cursize-4, (char *)&msg->data[4]);
		return;
#endif
	} else {
		Com_DPrintf ("bad connectionless packet from %s\n", NET_AdrToString (from));
	}
	SV_Cmd_EndTokenizedString();
	return;
}


//============================================================================

/*
=================
SV_PacketEvent
=================
*/
__optimize3 __regparm2 void SV_PacketEvent( netadr_t *from, msg_t *msg ) {

	client_t    *cl;
	unsigned short qport;
	int seq;
	
	if(!com_sv_running->boolean)
            return;

	if ( msg->cursize < 4 ) 
	{
		return;
	}
	MSG_BeginReading( msg );
	seq = MSG_ReadLong( msg );           // sequence number

	// check for connectionless packet (0xffffffff) first
	if ( seq == -1 )
	{
		SV_ConnectionlessPacket( from, msg );
		return;
	}
	// SV_ResetSekeletonCache();

	// read the qport out of the message so we can fix up
	// stupid address translating routers

#ifdef COD4X18UPDATE
	if(seq == 0xfffffffe)
	{
		SV_ReceiveFromUpdateProxy( msg );
		return;
	}
#endif
	qport = MSG_ReadShort( msg );  // & 0xffff;

	// find which client the message is from
	cl = SV_ReadPackets( from, qport );

	if(cl == NULL)
	{
		// if we received a sequenced packet from an address we don't recognize,
		// send an out of band disconnect packet to it
		NET_OutOfBandPrint( NS_SERVER, from, "disconnect" );
		return;
	}
#ifndef COD4X17A	
	if(seq == 0xfffffff0)
	{
		ReliableMessagesReceiveNextFragment( &cl->relmsg , msg );
		return;
	}
#ifdef COD4X18UPDATE
	if(cl->needupdate && cl->updateconnOK)
	{
		cl->lastPacketTime = svs.time;
		SV_PassToUpdateProxy(msg, cl);
		return;
	}
#endif
#endif
	// make sure it is a valid, in sequence packet
	if ( !Netchan_Process( &cl->netchan, msg ) )
	{
		return;
	}

	// zombie clients still need to do the Netchan_Process
	// to make sure they don't need to retransmit the final
	// reliable message, but they don't do any other processing
	cl->serverId = MSG_ReadByte( msg );
	cl->messageAcknowledge = MSG_ReadLong( msg );

	if(cl->messageAcknowledge < 0){
		Com_Printf("Invalid reliableAcknowledge message from %s - reliableAcknowledge is %i\n", cl->name, cl->reliableAcknowledge);
		return;
	}
	cl->reliableAcknowledge = MSG_ReadLong( msg );

	if((cl->reliableSequence - cl->reliableAcknowledge) > (MAX_RELIABLE_COMMANDS - 1)){
		Com_Printf("Out of range reliableAcknowledge message from %s - reliableSequence is %i, reliableAcknowledge is %i\n",
		cl->name, cl->reliableSequence, cl->reliableAcknowledge);
		cl->reliableAcknowledge = cl->reliableSequence;
		return;
	}
	
	SV_Netchan_Decode(cl, &msg->data[msg->readcount], msg->cursize - msg->readcount);
	
	if ( cl->state == CS_ZOMBIE )
	{
		return;
	}
	
	cl->lastPacketTime = svs.time;  // don't timeout

	SV_ExecuteClientMessage( cl, msg );
}


/*
==============================================================================

MASTER SERVER FUNCTIONS

==============================================================================
*/

/*
================
SV_MasterHeartbeat

Send a message to the masters every few minutes to
let it know we are alive, and log information.
We will also have a heartbeat sent when a server
changes from empty to non-empty, and full to non-full,
but not on every player enter or exit.
================
*/

#define	HEARTBEAT_USEC	180*1000*1000
void SV_MasterHeartbeat(const char *message)
{
	int			i;
	int			res;
	int			netenabled;

	netenabled = net_enabled->integer;

	// "dedicated 1" is for lan play, "dedicated 2" is for inet public play
	if (com_dedicated->integer != 2 || !(netenabled & (NET_ENABLEV4 | NET_ENABLEV6)))
		return;		// only dedicated servers send heartbeats

	// if not time yet, don't send anything
	if ( com_uFrameTime < svse.nextHeartbeatTime )
		return;

	svse.nextHeartbeatTime = com_uFrameTime + HEARTBEAT_USEC;

	// send to group masters
	for (i = 0; i < MAX_MASTER_SERVERS; i++)
	{
		if(!sv_master[i]->string[0])
			continue;

		// see if we haven't already resolved the name
		// resolving usually causes hitches on win95, so only
		// do it when needed
		if(sv_master[i]->modified || (master_adr[i][0].type == NA_BAD && master_adr[i][1].type == NA_BAD))
		{
			sv_master[i]->modified = qfalse;

			if(netenabled & NET_ENABLEV4)
			{
				Com_Printf("Resolving %s (IPv4)\n", sv_master[i]->string);
				//NA_IPANY For broadcasting to all interfaces
				res = NET_StringToAdr(sv_master[i]->string, &master_adr[i][0], NA_IP);

				if(res == 2)
				{
					// if no port was specified, use the default master port
					master_adr[i][0].port = BigShort(PORT_MASTER);
				}
				master_adr[i][0].sock = 0;

				if(res)
					Com_Printf( "%s resolved to %s\n", sv_master[i]->string, NET_AdrToString(&master_adr[i][0]));
				else
					Com_Printf( "%s has no IPv4 address.\n", sv_master[i]->string);
			}

			if(netenabled & NET_ENABLEV6)
			{
				Com_Printf("Resolving %s (IPv6)\n", sv_master[i]->string);
				res = NET_StringToAdr(sv_master[i]->string, &master_adr[i][1], NA_IP6);

				if(res == 2)
				{
					// if no port was specified, use the default master port
					master_adr[i][1].port = BigShort(PORT_MASTER);
				}

				master_adr[i][1].sock = 0;

				if(res)
					Com_Printf( "%s resolved to %s\n", sv_master[i]->string, NET_AdrToString(&master_adr[i][1]));
				else
					Com_Printf( "%s has no IPv6 address.\n", sv_master[i]->string);
			}

			if(master_adr[i][0].type == NA_BAD && master_adr[i][1].type == NA_BAD)
			{
				// if the address failed to resolve, clear it
				// so we don't take repeated dns hits
				Com_Printf("Couldn't resolve address: %s\n", sv_master[i]->string);
				Cvar_SetString(sv_master[i], "");
				sv_master[i]->modified = qfalse;
				continue;
			}
		}


		Com_Printf ("Sending heartbeat to %s\n", sv_master[i]->string );

		// this command should be changed if the server info / status format
		// ever incompatably changes
		if(i == 7)
		{
			if(master_adr[i][0].type != NA_BAD)
				NET_OutOfBandPrint( NS_SERVER, &master_adr[i][0], "heartbeat %s %s\n", message, masterServerSecret);
			if(master_adr[i][1].type != NA_BAD)
				NET_OutOfBandPrint( NS_SERVER, &master_adr[i][1], "heartbeat %s %s\n", message, masterServerSecret);
			continue;
		}
		if(master_adr[i][0].type != NA_BAD)
			NET_OutOfBandPrint( NS_SERVER, &master_adr[i][0], "heartbeat %s\n", message);
		if(master_adr[i][1].type != NA_BAD)
			NET_OutOfBandPrint( NS_SERVER, &master_adr[i][1], "heartbeat %s\n", message);
	}
}

/*
=================
SV_MasterShutdown

Informs all masters that this server is going down
=================
*/
void SV_MasterShutdown( void ) {
	// send a hearbeat right now
	svse.nextHeartbeatTime = 0;
	SV_MasterHeartbeat(HEARTBEAT_DEAD);

	// send it again to minimize chance of drops
	svse.nextHeartbeatTime = 0;
	SV_MasterHeartbeat(HEARTBEAT_DEAD);

	// when the master tries to poll the server, it won't respond, so
	// it will be removed from the list
}

/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage( const char *message, qboolean arg2 ) {
	int i, j;
	client_t    *cl;


	// send it twice, ignoring rate
	for ( j = 0 ; j < 2 ; j++ ) 
	{


		for ( i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ ) 
		{
			if ( cl->state == CS_ACTIVE ) {
				// don't send a disconnect to a local client
				if ( cl->netchan.remoteAddress.type != NA_LOOPBACK ) {

					if(arg2)
						SV_SendServerCommand_IW( cl, 1, "%c \"%s\"", 0x77, message );
					else
						SV_SendServerCommand_IW( cl, 1, "%c \"%s^7 %s\" PB", 0x77, cl->name ,message);
				}
				// force a snapshot to be sent
				cl->nextSnapshotTime = -1;
				SV_SendClientSnapshot( cl );

			}else if( cl->state >= CS_CONNECTED ){

				NET_OutOfBandPrint(NS_SERVER, &cl->netchan.remoteAddress, "disconnect %s", message);

			}
		}

	}
}


void SV_DisconnectAllClients(){

	int i;
	client_t    *cl;

	for ( i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ ) 
	{

		if( cl->state >= CS_CONNECTED ){

			SV_DropClient(cl, "EXE_DISCONNECTED");

		}
	}
}



/*
================
SV_ClearServer
================
*/
void SV_ClearServer( void ) {
	int i;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( sv.configstringIndex[i] ) {
			SL_RemoveRefToString( sv.configstringIndex[i] );
		}
	}

	if(sv.unkConfigIndex)
	{
		SL_RemoveRefToString(sv.unkConfigIndex);
	}

	Com_Memset( &sv, 0, sizeof( sv ) );
}




/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/

__cdecl void SV_Shutdown( const char *finalmsg ) {

	if ( !com_sv_running || !com_sv_running->integer ) {
		return;
	}

	Com_Printf( "----- Server Shutdown -----\n" );
	Com_Printf( "\nWith the reason: %s\n", finalmsg );
	if(SEH_StringEd_GetString(finalmsg)){
		SV_FinalMessage( finalmsg, qtrue );
	}else {
		SV_FinalMessage( finalmsg, qfalse );
	}



//	SV_RemoveOperatorCommands();
	SV_MasterShutdown();
	SV_ShutdownGameProgs();
	SV_DisconnectAllClients();
	SV_DemoSystemShutdown();
	SV_FreeClients();

	// free current level
	SV_ClearServer();

//	InsertPluginEvent
/*
	CL_ShutdownConnections( );


*/

	// free server static data
	Cvar_SetBool( com_sv_running, qfalse );

	memset( &svs, 0, sizeof( svs ) );
	memset( &svse, 0, sizeof( svse ) );

	Com_Printf( "---------------------------\n" );

	// disconnect any local clients
//	CL_Disconnect( qfalse );

}


/*
=============================================================================

Writing the serverstatus out to a XML-File.
This can be usefull to display serverinfo on a website

=============================================================================
*/

void	serverStatus_WriteCvars(cvar_t const* cvar, void *var ){
    xml_t *xmlbase = var;

    if(cvar->flags & (CVAR_SERVERINFO | CVAR_NORESTART)){
        XML_OpenTag(xmlbase,"Data",2, "Name",cvar->name, "Value",Cvar_DisplayableValue(cvar));
        XML_CloseTag(xmlbase);
    }
}

void	serverStatus_Write(){

    xml_t xmlbase;
    char outputbuffer[32768];
    int i, c;
    client_t    *cl;
    gclient_t	*gclient;
    char	score[16];
    char	team[4];
    char	kills[16];
    char	deaths[16];
    char	assists[16];
    char	teamname[32];
    char	cid[4];
    char	ping[4];
    char	power[4];
    char	rank[4];
    char        timestamp[16];
	mvabuf;


    time_t	realtime = Com_GetRealtime();
    char *timestr = ctime(&realtime);
    timestr[strlen(timestr)-1]= 0;

    if(!*sv_statusfile->string) return;

    XML_Init(&xmlbase,outputbuffer,sizeof(outputbuffer), "ISO-8859-1");
    Com_sprintf(timestamp,sizeof(timestamp),"%d",time(NULL));
    XML_OpenTag(&xmlbase,"B3Status",2,"Time",timestr,"TimeStamp",timestamp);

        XML_OpenTag(&xmlbase,"Game",9,"CapatureLimit","", "FragLimit","", "Map",sv_mapname->string, "MapTime","", "Name","cod4", "RoundTime","", "Rounds","", "TimeLimit","", "Type",sv_g_gametype->string);
            Cvar_ForEach(serverStatus_WriteCvars, &xmlbase);
            if(sv_password->string && *sv_password->string)
            {
                XML_OpenTag(&xmlbase,"Data",2, "Name", "pswrd", "Value", "1");
            }else{
                XML_OpenTag(&xmlbase,"Data",2, "Name", "pswrd", "Value", "0");
            }
            XML_CloseTag(&xmlbase);
			XML_OpenTag(&xmlbase,"Data",2, "Name", "sv_type", "Value", va("%d", sv_authorizemode->integer));
            XML_CloseTag(&xmlbase);
        XML_CloseTag(&xmlbase);

        for ( i = 0, c = 0, cl = svs.clients; i < sv_maxclients->integer ; cl++, i++ ) {
            if ( cl->state >= CS_CONNECTED ) c++;
        }
        XML_OpenTag(&xmlbase, "Clients", 1, "Total",va("%i",c));

            for ( i = 0, cl = svs.clients, gclient = level.clients; i < sv_maxclients->integer ; i++, cl++, gclient++ ) {
                if ( cl->state >= CS_CONNECTED ){
                        Com_sprintf(cid,sizeof(cid),"%i", i);

                        if(cl->state == CS_ACTIVE){


                            Com_sprintf(team,sizeof(team),"%i", gclient->sess.sessionTeam);
                            Com_sprintf(score,sizeof(score),"%i", gclient->pers.scoreboard.score);
                            Com_sprintf(kills,sizeof(kills),"%i", gclient->pers.scoreboard.kills);
                            Com_sprintf(deaths,sizeof(deaths),"%i", gclient->pers.scoreboard.deaths);
                            Com_sprintf(assists,sizeof(assists),"%i", gclient->pers.scoreboard.assists);
                            Com_sprintf(ping,sizeof(ping),"%i", cl->ping);
                            Com_sprintf(power,sizeof(power),"%i", cl->power);
                            Com_sprintf(rank,sizeof(rank),"%i", gclient->sess.rank +1);
                            switch(gclient->sess.sessionTeam){

                                case TEAM_RED:
                                    if(!Q_strncmp(g_TeamName_Axis->string,"MPUI_SPETSNAZ", 13))
                                        Q_strncpyz(teamname, "Spetsnaz", 32);
                                    else if(!Q_strncmp(g_TeamName_Axis->string,"MPUI_OPFOR", 10))
                                        Q_strncpyz(teamname, "Opfor", 32);
                                    else
                                        Q_strncpyz(teamname, g_TeamName_Axis->string, 32);

                                    break;

                                case TEAM_BLUE:
                                    if(!Q_strncmp(g_TeamName_Allies->string,"MPUI_MARINES", 12))
                                        Q_strncpyz(teamname, "Marines", 32);
                                    else if(!Q_strncmp(g_TeamName_Allies->string,"MPUI_SAS", 8))
                                        Q_strncpyz(teamname, "S.A.S.", 32);
                                    else
                                        Q_strncpyz(teamname, g_TeamName_Allies->string,32);

                                    break;

                                case TEAM_FREE:
                                    Q_strncpyz(teamname, "Free",32);
                                    break;
                                case TEAM_SPECTATOR:
                                    Q_strncpyz(teamname, "Spectator", 32);
                                    break;
                                default:
                                    *teamname = 0;
                            }
                        }else{
                            *team = 0;
                            *score = 0;
                            *kills = 0;
                            *deaths = 0;
                            *assists = 0;
                            *ping = 0;
                            *rank = 0;
                            if(cl->state == CS_CONNECTED){
                                Q_strncpyz(teamname, "Connecting...", 32);
                            }else{
                                Q_strncpyz(teamname, "Loading...", 32);
                            }
                        }

                        XML_OpenTag(&xmlbase, "Client", 15, "CID",cid, "ColorName",cl->name, "DBID",va("%i", cl->uid), "IP",NET_AdrToStringShort(&cl->netchan.remoteAddress), "PBID",cl->pbguid, "Score",score, "Kills",kills, "Deaths",deaths, "Assists",assists, "Ping", ping, "Team",team, "TeamName", teamname, "Updated", timestr, "power", power, "rank", rank);
                        XML_CloseTag(&xmlbase);
                }
            }

        XML_CloseTag(&xmlbase);
    XML_CloseTag(&xmlbase);

    FS_SV_WriteFile(sv_statusfile->string, outputbuffer, strlen(outputbuffer));

}

/*
void SV_ValidateServerId()
{
    int res;

    Com_Printf("Resolving cod4master.iceops.in\n");

    res = NET_StringToAdr("cod4master.iceops.in", &psvs.masterServer_adr, NA_IP);
    if(res == 2)
    {
        psvs.masterServer_adr.port = BigShort(PORT_MASTER);
    }
    psvs.masterServer_adr.sock = 0;

    if(res)
    {
        Com_Printf("cod4master.iceops.in resolved to %s\n", NET_AdrToString(&psvs.masterServer_adr));
        NET_OutOfBandPrint(NS_SERVER, &psvs.masterServer_adr, "serverValidate CoD4 %d %s", psvs.masterServer_id, psvs.masterServer_challengepassword);
    }
    else
        Com_Printf("cod4master.iceops.in has no IPv4 address.\n");

}

void SV_ValidationResponse(netadr_t *from, msg_t* msg)
{
    int res;

    Com_Printf("Resolving cod4master.iceops.in\n");

    res = NET_StringToAdr("cod4master.iceops.in", &psvs.masterServer_adr, NA_IP);
    if(res == 2)
    {
        psvs.masterServer_adr.port = BigShort(PORT_MASTER);
    }
    psvs.masterServer_adr.sock = 0;

    if(res)
    {
        Com_Printf("cod4master.iceops.in resolved to %s\n", NET_AdrToString(&psvs.masterServer_adr));
        NET_OutOfBandPrint(NS_SERVER, &psvs.masterServer_adr, "serverValidate CoD4 %d %s", psvs.masterServer_id, psvs.masterServer_challengepassword);
    }
    else
        Com_Printf("cod4master.iceops.in has no IPv4 address.\n");

}
*/
void SV_InitServerId(){
	int i;
	byte masterServerSecretBin[(MASTERSERVERSECRETLENGTH -1) / 2];
/*    int read;
    char buf[256];
    *buf = 0;
    fileHandle_t file;

    FS_SV_FOpenFileRead("server_id.dat", &file);
    if(!file){
        Com_DPrintf("Couldn't open server_id.dat for reading\n");
        return;
    }
    Com_Printf( "Loading file server_id.dat\n");

    read = FS_ReadLine(buf,sizeof(buf),file);

    if(read == -1){
        Com_Printf("Can not read from server_id.dat\n");
        FS_FCloseFile(file);
        loadconfigfiles = qfalse;
        return;
    }


    psvs.masterServer_id = atoi(Info_ValueForKey(buf, "id"));
    Q_strncpyz(psvs.masterServer_challengepassword, Info_ValueForKey(buf, "challenge_password"), sizeof(psvs.masterServer_challengepassword));

    FS_FCloseFile(file);
*/

    Com_RandomBytes((byte*)&psvs.masterServer_id, sizeof(psvs.masterServer_id));
    psvs.masterServer_challengepassword[0] = '-';
    psvs.masterServer_challengepassword[1] = '1';
    psvs.masterServer_challengepassword[2] = '\0';
	Com_RandomBytes((byte*)masterServerSecretBin, sizeof(masterServerSecretBin));
	for (i=0; i < sizeof(masterServerSecretBin); ++i)
	{
		sprintf(&masterServerSecret[2*i], "%02x", masterServerSecretBin[i]);
	}
	masterServerSecret[sizeof(masterServerSecret) -1] = '\0';
	
}

void SV_CopyCvars()
{

	*(cvar_t**)0x13ed89bc = sv_g_gametype;
	*(cvar_t**)0x13ed8974 = sv_mapname;
	*(cvar_t**)0x13ed8960 = sv_maxclients;
	*(cvar_t**)0x13ed89c8 = sv_clientSideBullets;
	*(cvar_t**)0x13ed89e4 = sv_floodProtect;
	*(cvar_t**)0x13ed89ec = sv_showcommands;
	*(cvar_t**)0x13ed899c = sv_iwds;
	*(cvar_t**)0x13ed89a0 = sv_iwdNames;
	*(cvar_t**)0x13ed89a4 = sv_referencedIwds;
	*(cvar_t**)0x13ed89a8 = sv_referencedIwdNames;
	*(cvar_t**)0x13ed89ac = sv_FFCheckSums;
	*(cvar_t**)0x13ed89b0 = sv_FFNames;
	*(cvar_t**)0x13ed89b4 = sv_referencedFFCheckSums;
	*(cvar_t**)0x13ed89b8 = sv_referencedFFNames;
	*(cvar_t**)0x13ed8978 = sv_serverid;
	*(cvar_t**)0x13ed89d0 = sv_pure;
	*(cvar_t**)0x13ed8950 = sv_fps;
	*(cvar_t**)0x13ed8a04 = sv_botsPressAttackBtn;
	*(cvar_t**)0x13ed89c0 = sv_debugRate;
	*(cvar_t**)0x13ed89c4 = sv_debugReliableCmds;
	*(cvar_t**)0x13f19004 = sv_clientArchive;
	*(cvar_t**)0x13ed8a08 = sv_voice;
	*(cvar_t**)0x13ed8a0c = sv_voiceQuality;
}



void SV_InitCvarsOnce(void){

	sv_paused = Cvar_RegisterBool("sv_paused", qfalse, CVAR_ROM, "True if the server is paused");
	sv_killserver = Cvar_RegisterBool("sv_killserver", qfalse, CVAR_ROM, "True if the server getting killed");
	sv_protocol = Cvar_RegisterInt("protocol", PROTOCOL_VERSION, PROTOCOL_VERSION, PROTOCOL_VERSION, 0x44, "Protocol version");
	sv_privateClients = Cvar_RegisterInt("sv_privateClients", 0, 0, 64, 4, "Maximum number of private clients allowed onto this server");
	sv_hostname = Cvar_RegisterString("sv_hostname", "^5CoD4Host", 5, "Host name of the server");
#ifdef PUNKBUSTER
	sv_punkbuster = Cvar_RegisterBool("sv_punkbuster", qtrue, 0x15, "Enable PunkBuster on this server");
#endif
	sv_minPing = Cvar_RegisterInt("sv_minPing", 0, 0, 1000, 5, "Minimum allowed ping on the server");
	sv_maxPing = Cvar_RegisterInt("sv_maxPing", 0, 0, 1000, 5, "Maximum allowed ping on the server");
	sv_queryIgnoreMegs = Cvar_RegisterInt("sv_queryIgnoreMegs", 1, 0, 32, 0x11, "Number of megabytes of RAM to allocate for the querylimit IP-blacklist. 0 disables this feature");
	sv_queryIgnoreTime = Cvar_RegisterInt("sv_queryIgnoreTime", 2000, 100, 100000, 1, "How much milliseconds have to pass until another two queries are allowed");
	Cvar_RegisterBool("sv_disableClientConsole", qfalse, 4, "Disallow remote clients from accessing the client console");

	sv_privatePassword = Cvar_RegisterString("sv_privatePassword", "", 0, "Password for the private client slots");
	sv_rconPassword = Cvar_RegisterString("rcon_password", "", 0, "Password for the server remote control console");

	sv_allowDownload = Cvar_RegisterBool("sv_allowDownload", qtrue, 1, "Allow clients to download gamefiles from server");
	sv_wwwDownload = Cvar_RegisterBool("sv_wwwDownload", qfalse, 1, "Enable http download");
	sv_wwwBaseURL = Cvar_RegisterString("sv_wwwBaseURL", "", 1, "The base url to files for downloading from the HTTP-Server");
	sv_wwwDlDisconnected = Cvar_RegisterBool("sv_wwwDlDisconnected", qfalse, 1, "Should clients stay connected while downloading from a HTTP-Server?");

	sv_voice = Cvar_RegisterBool("sv_voice", qfalse, 0xd, "Allow serverside voice communication");
	sv_voiceQuality = Cvar_RegisterInt("sv_voiceQuality", 3, 0, 9, 8, "Voice quality");

	sv_cheats = Cvar_RegisterBool("sv_cheats", qfalse, 0x18, "Enable cheats on the server");
	sv_reconnectlimit = Cvar_RegisterInt("sv_reconnectlimit", 5, 0, 1800, 1, "Seconds to disallow a prior connected client to reconnect to the server");
	sv_padPackets = Cvar_RegisterInt("sv_padPackets", 0, 0, 0x7fffffff, 0, "How many nop-bytes to add to insert into each snapshot");

	sv_mapRotation = Cvar_RegisterString("sv_mapRotation", "", 0, "List of all maps server will play");
	sv_mapRotationCurrent = Cvar_RegisterString("sv_mapRotationCurrent", "", 0, "List of remaining maps server has to play");
	sv_nextmap = Cvar_RegisterString("nextmap", "", 0, "String to execute on map_rotate. This will override the default maprotation of CoD4 with the Quake3 style maprotation");

	sv_timeout = Cvar_RegisterInt("sv_timeout", 40, 0, 1800, 0, "Seconds to keep a client on server without a new clientmessage");
	sv_connectTimeout = Cvar_RegisterInt("sv_connectTimeout", 90, 0, 1800, 0, "Seconds to wait for a client which is loading a map without a new clientmessage");
	sv_zombieTime = Cvar_RegisterInt("sv_zombieTime", 2, 0, 1800, 0, "Seconds to keep a disconnected client on server to transmit the last message");

	Cvar_RegisterBool("clientSideEffects", qtrue, 0x80, "Enable loading _fx.gsc files on the client");

	sv_modStats = Cvar_RegisterBool("ModStats", qtrue, 0, "Flag whether to use stats of mod (when running a mod) or to use stats of the Cod4 coregame");
	sv_authorizemode = Cvar_RegisterInt("sv_authorizemode", 1, -1, 1, CVAR_ARCHIVE, "How to authorize clients, 0=acceptall(No GUIDs) 1=accept no one with invalid GUID");
	sv_showasranked = Cvar_RegisterBool("sv_showasranked", qfalse, 0, "List the server in serverlist of ranked servers even when it is modded");
	sv_statusfile = Cvar_RegisterString("sv_statusfilename", "serverstatus.xml", CVAR_ARCHIVE, "Filename to write serverstatus to disk");
	g_mapstarttime = Cvar_RegisterString("g_mapStartTime", "", CVAR_SERVERINFO | CVAR_ROM, "Time when current map has started");
	g_friendlyPlayerCanBlock = Cvar_RegisterBool("g_friendlyPlayerCanBlock", qfalse, 0, "Flag whether friendly players can block each other");
	g_FFAPlayerCanBlock = Cvar_RegisterBool("g_FFAPlayerCanBlock", qtrue, 0, "Flag whether players in non team based games can block each other");
	sv_password = Cvar_RegisterString("g_password", "", CVAR_ARCHIVE, "Password which is required to join this server");
	g_motd = Cvar_RegisterString("g_motd", "", CVAR_ARCHIVE, "Message of the day, which getting shown to every player on his 1st spawn");
	sv_uptime = Cvar_RegisterString("uptime", "", CVAR_SERVERINFO | CVAR_ROM, "Time the server is running since last restart");
	sv_autodemorecord = Cvar_RegisterBool("sv_autodemorecord", qfalse, 0, "Automatically start from each connected client a demo.");
	sv_demoCompletedCmd = Cvar_RegisterString("sv_demoCompletedCmd", "", com_securemode ? CVAR_INIT : 0 , "This program will be executed when a demo has been completed. The demofilename will be passed as argument.");
	sv_mapDownloadCompletedCmd = Cvar_RegisterString("sv_mapDownloadCompletedCmd", "", com_securemode ? CVAR_INIT : 0 , "This program will be executed when a downloaded map was received. The usermaps/mapname will be passed as argument.");
	sv_consayname = Cvar_RegisterString("sv_consayname", "^2Server: ^7", CVAR_ARCHIVE, "If the server broadcast text-messages this name will be used");
	sv_contellname = Cvar_RegisterString("sv_contellname", "^5Server^7->^5PM: ^7", CVAR_ARCHIVE, "If the server broadcast text-messages this name will be used");

	sv_master[0] = Cvar_RegisterString("sv_master1", "", 0, "A masterserver name");
	sv_master[1] = Cvar_RegisterString("sv_master2", "", 0, "A masterserver name");
	sv_master[2] = Cvar_RegisterString("sv_master3", "", 0, "A masterserver name");
	sv_master[3] = Cvar_RegisterString("sv_master4", "", 0, "A masterserver name");
	sv_master[4] = Cvar_RegisterString("sv_master5", "", 0, "A masterserver name");
	sv_master[5] = Cvar_RegisterString("sv_master6", "", 0, "A masterserver name");

	sv_master[6] = Cvar_RegisterString("sv_master7", MASTER_SERVER_NAME, CVAR_ROM, "Default masterserver name");
	sv_master[7] = Cvar_RegisterString("sv_master8", MASTER_SERVER_NAME2, CVAR_ROM, "Default masterserver name");

	sv_g_gametype = Cvar_RegisterString("g_gametype", "war", 0x24, "Current game type");
	sv_mapname = Cvar_RegisterString("mapname", "", CVAR_ROM | CVAR_SERVERINFO, "Current map name");
	sv_maxclients = Cvar_RegisterInt("sv_maxclients", 16, 1, 64, CVAR_INIT | CVAR_SERVERINFO, "Maximum number of clients that can connect to a server");
	sv_clientSideBullets = Cvar_RegisterBool("sv_clientSideBullets", qtrue, 8, "If true, clients will synthesize tracers and bullet impacts");
	sv_maxRate = Cvar_RegisterInt("sv_maxRate", 100000, 2500, 100000, 5, "Maximum allowed bitrate per client");
	sv_floodProtect = Cvar_RegisterInt("sv_floodprotect", 4, 0, 100000, 5, "Prevent malicious lagging by flooding the server with commands. Is the number of client commands allowed to process");
	sv_showcommands = Cvar_RegisterBool("sv_showcommands", qfalse, 0, "Print all client commands");
	sv_iwds = Cvar_RegisterString("sv_iwds", "", 0x48, "IWD server checksums");
	sv_iwdNames = Cvar_RegisterString("sv_iwdNames", "", 0x48, "Names of IWD files used by the server");
	sv_referencedIwds = Cvar_RegisterString("sv_referencedIwds", "", 0x48, "Checksums of all referenced IWD files");
	sv_referencedIwdNames = Cvar_RegisterString("sv_referencedIwdNames", "", 0x48, "Names of all referenced IWD files used by the server");
	sv_FFCheckSums = Cvar_RegisterString("sv_FFCheckSums", "", 0x48, "FastFile server checksums");
	sv_FFNames = Cvar_RegisterString("sv_FFNames", "", 0x48, "Names of FastFiles used by the server");
	sv_referencedFFCheckSums = Cvar_RegisterString("sv_referencedFFCheckSums", "", 0x48, "Checksums of all referenced FastFiles");
	sv_referencedFFNames = Cvar_RegisterString("sv_referencedFFNames", "", 0x48, "Names of all referenced FastFiles used by the server");
	sv_serverid = Cvar_RegisterInt("sv_serverid", 0, 0x80000000, 0x7fffffff, 0x48, "Voice quality");
	sv_pure = Cvar_RegisterBool("sv_pure", qtrue, 0xc, "Cannot use modified IWD files");
	sv_fps = Cvar_RegisterInt("sv_fps", 20, 1, 250, 0, "Server frames per second");
	sv_showAverageBPS = Cvar_RegisterBool("sv_showAverageBPS", qfalse, 0, "Show average bytes per second for net debugging");
	sv_botsPressAttackBtn = Cvar_RegisterBool("sv_botsPressAttackBtn", qtrue, 0, "Allow testclients to press attack button");
	sv_debugRate = Cvar_RegisterBool("sv_debugRate", qfalse, 0, "Enable snapshot rate debugging info");
	sv_debugReliableCmds = Cvar_RegisterBool("sv_debugReliableCmds", qfalse, 0, "Enable debugging information for reliable commands");
	sv_clientArchive = Cvar_RegisterBool("sv_clientArchive", qtrue, 0, "Have the clients archive data to save bandwidth on the server");
	sv_shownet = Cvar_RegisterInt("sv_shownet", -1, -1, 63, 0, "Enable network debugging for a client");
}




void SV_Init(){

        SV_AddSafeCommands();
        SV_InitCvarsOnce();
        SVC_RateLimitInit( );
        SV_InitBanlist();
        Init_CallVote();
        SV_InitServerId();
        Com_RandomBytes((byte*)&psvs.randint, sizeof(psvs.randint));

}


/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
void SV_CalcPings( void ) {
	int i, j;
	client_t    *cl;
	int total, count;
	int delta;

	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		cl = &svs.clients[i];

		if ( cl->state != CS_ACTIVE ) {
			cl->ping = -1;
			continue;
		}
		if ( !cl->gentity ) {
			cl->ping = -1;
			continue;
		}
		if ( cl->netchan.remoteAddress.type == NA_BOT ) {
			cl->ping = 0;
			cl->lastPacketTime = svs.time;
			continue;
		}

		total = 0;
		count = 0;
		for ( j = 0 ; j < PACKET_BACKUP ; j++ ) {
			if ( cl->frames[j].messageAcked == 0xFFFFFFFF ) {
				continue;
			}
			delta = cl->frames[j].messageAcked - cl->frames[j].messageSent;
			count++;
			total += delta;
		}
		if ( !count ) {
			cl->ping = 999;
		} else {
			cl->ping = total / count;
			if ( cl->ping > 999 ) {
				cl->ping = 999;
			}
		}
	}
}

/*
===============
SV_GetConfigstring

===============
*/
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) {

	short strIndex;
	char* cs;

	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bad index %i\n", index );
	}
	strIndex = sv.configstringIndex[index];

	cs = SL_ConvertToString(strIndex);

	Q_strncpyz( buffer, cs, bufferSize );
}

typedef struct{
    int index;
    char* string;
    int unk1;
    int unk2;
}constConfigstring_t;

#define constantConfigstrings (constConfigstring_t*)UNKGAMESTATESTR_ADDR
#define UNKGAMESTATESTR_ADDR (0x826f260)
/*
===============

===============
*/

void SV_WriteGameState( msg_t* msg, client_t* cl ) {

	char* cs;
	int i, edi, ebx, numConfigstrings, esi, var_03, clnum;
	entityState_t nullstate, *base;
	snapshotInfo_t snapInfo;
	constConfigstring_t *gsbase = constantConfigstrings;
	constConfigstring_t *gsindex;
	unsigned short strindex;

	MSG_WriteByte( msg, svc_gamestate );
	MSG_WriteLong( msg, cl->reliableSequence );
	MSG_WriteByte( msg, svc_configstring );

	for ( esi = 0, numConfigstrings = 0, var_03 = 0 ; esi < MAX_CONFIGSTRINGS ; esi++) {

		strindex = sv.configstringIndex[esi];
		gsindex = &gsbase[var_03];

		if(gsindex->index != esi){

			if(strindex != sv.unkConfigIndex)
				numConfigstrings++;

			continue;
		}

		cs = SL_ConvertToString(strindex);

		if(gsindex->index > 820){
			if(Q_stricmp(gsindex->string, cs))
			{
				numConfigstrings++;
			}
		}else{
			if(strcmp(gsindex->string, cs))
			{
				numConfigstrings++;
			}
		}
		var_03++;
	}
	MSG_WriteShort(msg, numConfigstrings);

	for ( ebx = 0, edi = -1, var_03 = 0 ; ebx < MAX_CONFIGSTRINGS ; ebx++) {

		gsindex = &gsbase[var_03];
		strindex = sv.configstringIndex[ebx];

		if(gsindex->index == ebx){
					//ebx and gsindex->index are equal
			var_03++;

			cs = SL_ConvertToString(strindex);

			if(ebx > 820){
				if(!Q_stricmp(gsindex->string, cs))
				{
					continue;
				}
			}else{
				if(!strcmp(gsindex->string, cs))
				{
					continue;
				}
			}

			if(sv.unkConfigIndex == strindex)
			{
				MSG_WriteBit0(msg);
				MSG_WriteBits(msg, gsindex->index, 12);
				MSG_WriteBigString(msg, "");
				edi = ebx;
				continue;
			}
		}

		strindex = sv.configstringIndex[ebx];
		if(sv.unkConfigIndex != strindex)
		{
			if(edi+1 == ebx){

				MSG_WriteBit1(msg);
			}else{
				MSG_WriteBit0(msg);
				MSG_WriteBits(msg, ebx, 12);
			}
			MSG_WriteBigString(msg, SL_ConvertToString(strindex));
		}
	}
	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	clnum = cl - svs.clients;
	// baselines
	for ( i = 0; i < MAX_GENTITIES ; i++ )
	{
		base = &sv.svEntities[i].baseline;
		if ( !base->number ) {
			continue;
		}
		MSG_WriteByte( msg, svc_baseline );

		snapInfo.clnum = clnum;
		snapInfo.cl = NULL;
		snapInfo.var_01 = 0xFFFFFFFF;
		snapInfo.var_02 = qtrue;

		MSG_WriteDeltaEntity( &snapInfo, msg, 0, &nullstate, base, qtrue );
	}
	MSG_WriteByte( msg, svc_EOF );

}

/*

void SV_WriteGameState( msg_t* msg, client_t* cl ) {

	int i, numConfigstrings, clnum;
	entityState_t nullstate, *base;
	snapshotInfo_t snapInfo;
	unsigned short strindex;

	MSG_WriteByte( msg, svc_gamestate );
	MSG_WriteLong( msg, cl->reliableSequence );
	MSG_WriteByte( msg, svc_configstring );

	for ( i = 0, numConfigstrings = 0; i < MAX_CONFIGSTRINGS ; i++) {

		strindex = sv.configstringIndex[i];
		if(strindex != 0)
		{
			numConfigstrings++;
		}
	}
	MSG_WriteShort(msg, numConfigstrings);

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++)
	{

		strindex = sv.configstringIndex[i];

		if(strindex == 0)
		{
			continue;
		}
		MSG_WriteBit0(msg);
		MSG_WriteBits(msg, i, 12);
		MSG_WriteBigString(msg, SL_ConvertToString(strindex));
		Com_Printf("CS %d: %s\n", i, SL_ConvertToString(strindex));
	}
	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	clnum = cl - svs.clients;
	// baselines
	for ( i = 0; i < MAX_GENTITIES ; i++ ) {
		base = &sv.svEntities[i].baseline;
		if ( !base->number ) {
			continue;
		}
		MSG_WriteByte( msg, svc_baseline );

		snapInfo.clnum = clnum;
		snapInfo.cl = NULL;
		snapInfo.var_01 = 0xFFFFFFFF;
		snapInfo.var_02 = qtrue;

		MSG_WriteDeltaEntity( &snapInfo, msg, 0, &nullstate, base, qtrue );
	}
	MSG_WriteByte( msg, svc_EOF );

}

*/
/*
================
SV_RconStatusWrite

Used by rcon to retrive all serverdata as detailed as possible
================
*/
void SV_WriteRconStatus( msg_t* msg ) {

	//Reserve 19000 free bytes for msg_t struct

	int i;
	client_t    *cl;
	gclient_t *gclient;
	char infostring[MAX_INFO_STRING];
	mvabuf;

	infostring[0] = 0;

	if(!com_sv_running->boolean)
            return;

	strcpy( infostring, Cvar_InfoString( CVAR_SERVERINFO | CVAR_NORESTART));
	// echo back the parameter to status. so master servers can use it as a challenge
	// to prevent timed spoofed reply packets that add ghost servers
	if(*sv_password->string)
	    Info_SetValueForKey( infostring, "pswrd", "1");

	//Write teamnames
        if(!Q_strncmp(g_TeamName_Axis->string,"MPUI_SPETSNAZ", 13))
            Info_SetValueForKey( infostring, "axis", "Spetsnaz");
        else if(!Q_strncmp(g_TeamName_Axis->string,"MPUI_OPFOR", 10))
            Info_SetValueForKey( infostring, "axis", "Opfor");
        else
            Info_SetValueForKey( infostring, "axis", g_TeamName_Axis->string);

        if(!Q_strncmp(g_TeamName_Allies->string,"MPUI_MARINES", 12))
            Info_SetValueForKey( infostring, "allies", "Marines");
        else if(!Q_strncmp(g_TeamName_Allies->string,"MPUI_SAS", 8))
            Info_SetValueForKey( infostring, "allies", "S.A.S.");
        else
            Info_SetValueForKey( infostring, "allies", g_TeamName_Allies->string);

        Info_SetValueForKey( infostring, "svtime", va("%i", svs.time));

	//Writing general server info to msg (Reserve 1024 bytes)
	MSG_WriteString(msg, infostring);

	//Reserve 64 * 280 bytes = 18000
	//Writing clientinfo to msg
	for ( i = 0, gclient = level.clients ; i < sv_maxclients->integer ; i++, gclient++ ) {

		cl = &svs.clients[i];

		infostring[0] = 0;

		if ( cl->state >= CS_CONNECTED ) {
			MSG_WriteByte( msg, i );	//ClientIndex
			Info_SetValueForKey( infostring, "name", cl->name);
			Info_SetValueForKey( infostring, "uid", va("%i", cl->uid));
			Info_SetValueForKey( infostring, "pbguid", cl->pbguid);
			Info_SetValueForKey( infostring, "team", va("%i", gclient->sess.sessionTeam));
			Info_SetValueForKey( infostring, "score", va("%i", gclient->pers.scoreboard.score));
			Info_SetValueForKey( infostring, "kills", va("%i", gclient->pers.scoreboard.kills));
			Info_SetValueForKey( infostring, "deaths", va("%i", gclient->pers.scoreboard.deaths));
			Info_SetValueForKey( infostring, "assists", va("%i", gclient->pers.scoreboard.assists));
			Info_SetValueForKey( infostring, "ping", va("%i", cl->ping));

			if(cl->netchan.remoteAddress.type == NA_BOT)
				Info_SetValueForKey( infostring, "ipconn", "BOT");
			else
				Info_SetValueForKey( infostring, "ipconn", NET_AdrToConnectionString(&cl->netchan.remoteAddress));

			Info_SetValueForKey( infostring, "state", va("%i", cl->state));
			Info_SetValueForKey( infostring, "os", va("%c", cl->OS));
			Info_SetValueForKey( infostring, "power", va("%i", cl->power));
			Info_SetValueForKey( infostring, "rate", va("%i", cl->rate));
			MSG_WriteString(msg, infostring);
		}
	}
	MSG_WriteByte( msg, -1 );	//Terminating ClientIndex
}

void SV_GetServerStaticHeader(){
	svs.nextCachedSnapshotFrames = svsHeader.nextCachedSnapshotFrames;
	svs.nextCachedSnapshotEntities = svsHeader.nextCachedSnapshotEntities;
	svs.nextCachedSnapshotClients = svsHeader.nextCachedSnapshotClients;
}

void SV_SetServerStaticHeader()
{
	svsHeader.mapCenter[0] = svs.mapCenter[0];
	svsHeader.mapCenter[1] = svs.mapCenter[1];
	svsHeader.mapCenter[2] = svs.mapCenter[2];

	svsHeader.snapFlagServerBit = svs.snapFlagServerBit;
	svsHeader.time = svs.time;
	svsHeader.numSnapshotEntities = svs.numSnapshotEntities;
	svsHeader.numSnapshotClients = svs.numSnapshotClients;
	svsHeader.nextSnapshotEntities = svs.nextSnapshotEntities;
	svsHeader.nextSnapshotClients = svs.nextSnapshotClients;
	svsHeader.nextCachedSnapshotFrames = svs.nextCachedSnapshotFrames;
	svsHeader.nextArchivedSnapshotFrames = svs.nextArchivedSnapshotFrames;
	svsHeader.nextCachedSnapshotEntities = svs.nextCachedSnapshotEntities;
	svsHeader.nextCachedSnapshotClients = svs.nextCachedSnapshotClients;
	svsHeader.num_entities = sv.num_entities;
	svsHeader.clients = svs.clients;
	svsHeader.snapshotEntities = svs.snapshotEntities;
	svsHeader.snapshotClients = svs.snapshotClients;
	svsHeader.svEntities = sv.svEntities;
	svsHeader.archivedEntities = svs.archivedEntities;
	svsHeader.entUnknown2 = svs.entUnknown2;
	svsHeader.archiveSnapBuffer = svs.archiveSnapBuffer;
	svsHeader.entUnknown1 = svs.entUnknown1;
	svsHeader.maxClients = sv_maxclients->integer;
	svsHeader.fps = sv_fps->integer;
	svsHeader.gentitySize = sv.gentitySize;
	svsHeader.canArchiveData = sv_clientArchive->integer;

	svsHeader.gentities = sv.gentities;
	svsHeader.gclientstate = G_GetClientState( 0 );
	svsHeader.gplayerstate = G_GetPlayerState( 0 );
	svsHeader.gclientSize = G_GetClientSize();

}


void SV_InitArchivedSnapshot(){
	
	svs.nextArchivedSnapshotFrames = 0;
	svs.nextArchivedSnapshotBuffer = 0;
	svs.nextCachedSnapshotEntities = 0;
	svs.nextCachedSnapshotEntities = 0;
	svs.nextCachedSnapshotFrames = 0;
}


void SV_RunFrame(){
	SV_ResetSekeletonCache();
	G_RunFrame(svs.time);
}


int SV_ClientAuthMode(){

	return sv_authorizemode->integer;

}

const char* SV_GetMessageOfTheDay(){

	return g_motd->string;

}


qboolean SV_FriendlyPlayerCanBlock(){

	return g_friendlyPlayerCanBlock->boolean;

}

qboolean SV_FFAPlayerCanBlock(){

	return g_FFAPlayerCanBlock->boolean;

}

//Few custom added things which should happen if we load a new level or restart a level
void SV_PreLevelLoad(){

	client_t* client;
	int i;
	char buf[MAX_STRING_CHARS];
	
	Com_UpdateRealtime();
	time_t realtime = Com_GetRealtime();
	char *timestr = ctime(&realtime);
	timestr[strlen(timestr)-1]= 0;

	Sys_TermProcess(); //term childs

	Cvar_SetString(g_mapstarttime, timestr);

	if(!onExitLevelExecuted)
	{
		PHandler_Event(PLUGINS_ONEXITLEVEL, NULL);
	}
	onExitLevelExecuted = qfalse;

	SV_RemoveAllBots();
	SV_ReloadBanlist();

	NV_LoadConfig();

	G_InitMotd();

	for ( client = svs.clients, i = 0 ; i < sv_maxclients->integer ; i++, client++ ) {

		G_DestroyAdsForPlayer(client); //Remove hud message ads

		// send the new gamestate to all connected clients
		if ( client->state < CS_ACTIVE ) {
			continue;
		}

		if ( client->netchan.remoteAddress.type == NA_BOT ) {
			continue;
		}

		if(SV_PlayerIsBanned(client->uid, client->pbguid, &client->netchan.remoteAddress, buf, sizeof(buf))){
			SV_DropClient(client, "Prior kick/ban");
			continue;
		}
	}
	Pmove_ExtendedResetState();

	HL2Rcon_EventLevelStart();

}

void SV_PostLevelLoad(){
	PHandler_Event(PLUGINS_ONSPAWNSERVER, NULL);
	sv.frameusec = 1000000 / sv_fps->integer;
	sv.serverId = com_frameTime;
}

void SV_LoadLevel(const char* levelname)
{
	char mapname[MAX_QPATH];

	Q_strncpyz(mapname, levelname, sizeof(mapname));
	FS_ConvertPath(mapname);
	SV_PreLevelLoad();
	SV_SpawnServer(mapname);

#ifndef COD4X17A
	char cs[MAX_STRING_CHARS];
	char list[MAX_STRING_CHARS];
	DB_BuildOverallocatedXAssetList(list, sizeof(list));
	Com_sprintf(cs, sizeof(cs), "cod%d %s", PROTOCOL_VERSION, list);
	SV_SetConfigstring(2, cs);
#endif

	SV_PostLevelLoad();
}


/*
==================
SV_Map

Restart the server on a different map
==================
*/
qboolean SV_Map( const char *levelname ) {
	char        *map;
	char mapname[MAX_QPATH];
	char expanded[MAX_QPATH];

	if(gamebinary_initialized == qfalse)
	{
		if(Com_LoadBinaryImage() == qfalse)
			return qtrue;
        }

	map = FS_GetMapBaseName(levelname);
	Q_strncpyz(mapname, map, sizeof(mapname));
	Q_strlwr(mapname);

	if(!com_useFastfiles->integer)
	{
		Com_sprintf(expanded, sizeof(expanded), "maps/mp/%s.d3dbsp", mapname);
		if ( FS_ReadFile( expanded, NULL ) == -1 ) {
			Com_PrintError( "Can't find map %s\n", expanded );
			return qfalse;
		}
	}

	if(!DB_FileExists(mapname, 0) && (!fs_gameDirVar->string[0] || !DB_FileExists(mapname, 2))){
		Com_PrintError("Can't find map %s\n", mapname);
		if(!fs_gameDirVar->string[0])
			Com_PrintError("A mod is required to run custom maps\n");
		return qfalse;
	}

//	Cbuf_ExecuteBuffer(0, 0, "selectStringTableEntryInDvar mp/didyouknow.csv 0 didyouknow");

	SV_LoadLevel(mapname);
	return qtrue;
}


void SV_PreFastRestart(){
	PHandler_Event(PLUGINS_ONPREFASTRESTART, NULL);
}
void SV_PostFastRestart(){
	PHandler_Event(PLUGINS_ONPOSTFASTRESTART, NULL);
}

/*
================
SV_MapRestart

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
void SV_MapRestart( qboolean fastRestart ){

	int i, j;
	client_t    *client;
	const char  *denied;
	char cmd[128];

	// make sure server is running
	if ( !com_sv_running->boolean ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	// DHM - Nerve :: Check for invalid gametype
	SV_SetGametype();
	if(Q_stricmp(sv.gametype, sv_g_gametype->string)){
		fastRestart = qfalse; //No fastrestart if we have changed gametypes
	}
	Q_strncpyz(sv.gametype, sv_g_gametype->string, sizeof(sv.gametype));
	int pers = G_GetSavePersist();


	if(!fastRestart)
	{
		G_SetSavePersist(0);
		SV_LoadLevel(sv_mapname->string);
		return;
	}

	SV_PreFastRestart();

	if(com_frameTime == sv.serverId)
		return;

	// connect and begin all the clients
	for ( client = svs.clients, i = 0 ; i < sv_maxclients->integer ; i++, client++ ) {

		G_DestroyAdsForPlayer(client); //Remove hud message ads

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED ) {
			continue;
		}

		if ( client->netchan.remoteAddress.type == NA_BOT ) {
			continue;
		}

		// add the map_restart command
		NET_OutOfBandPrint( NS_SERVER, &client->netchan.remoteAddress, "fast_restart" );
	}

	SV_InitCvars();
	SV_InitArchivedSnapshot();

	svs.snapFlagServerBit ^= 4;

	sv_serverId = (0xf0 & sv_serverId) + ((sv_serverId + 1) & 0xf);

	Cvar_SetInt(sv_serverid, sv_serverId);

	sv.serverId = com_frameTime;

	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	SV_RestartGameProgs(pers);

	// run a few frames to allow everything to settle
	for ( i = 0 ; i < 3 ; i++ ) {
		svs.time += 100;
		SV_RunFrame();
	}

	// connect and begin all the clients
	for ( i = 0, client = svs.clients; i < sv_maxclients->integer ; i++, client++ ) {

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED ) {
			continue;
		}

		if ( client->netchan.remoteAddress.type == NA_BOT ) {
			continue;
		}

		if(!pers)
			j = -1;
		else
			j = 0;
		Com_sprintf(cmd, sizeof(cmd), "%c", ((-44 & j) + 110) );
		SV_AddServerCommand(client, 1, cmd);

		// connect the client again, without the firstTime flag
		denied = ClientConnect(i, client->clscriptid);

		if(denied){
			SV_DropClient(client, denied);
			Com_Printf("SV_MapRestart: dropped client %i - denied!\n", i);
			continue;
		}

		if(client->state == CS_ACTIVE){
			SV_ClientEnterWorld( client, &client->lastUsercmd );
		}
	}

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_GAME;
	sv.restarting = qfalse;
	SV_PostFastRestart();
}



/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->integer
seconds, drop the conneciton.  Server time is used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
#define SV_MAXCS_CONNECTEDTIME 6

void SV_CheckTimeouts( void ) {

	int i;
	client_t    *cl;
	int primeddroppoint;
	int connectdroppoint;
	int activedroppoint;
	int zombiepoint;

	activedroppoint = svs.time - 1000 * sv_timeout->integer;
	primeddroppoint = svs.time - 1000 * sv_connectTimeout->integer;
	connectdroppoint = svs.time - 1000 * SV_MAXCS_CONNECTEDTIME;
	zombiepoint = svs.time - 1000 * sv_zombieTime->integer;

	for ( i = 0,cl = svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
		// message times may be wrong across a changelevel
		if ( cl->lastPacketTime > svs.time ) {
			cl->lastPacketTime = svs.time;
		}

		if ( cl->state == CS_ZOMBIE && cl->lastPacketTime < zombiepoint ) {
			// using the client id cause the cl->name is empty at this point
			Com_DPrintf( "Going from CS_ZOMBIE to CS_FREE for client %d\n", i );
			cl->state = CS_FREE;    // can now be reused
			continue;
		}

		if(cl->state == CS_ACTIVE && cl->lastPacketTime < activedroppoint){
			// wait several frames so a debugger session doesn't
			// cause a timeout
			if ( ++cl->timeoutCount > 5 ) {
				SV_DropClient( cl, "EXE_TIMEDOUT" );
				cl->state = CS_FREE;    // don't bother with zombie state
			}
		} else if ( cl->state == CS_CONNECTED && cl->lastPacketTime < connectdroppoint ) {
			if ( ++cl->timeoutCount > 5 ) {
				SV_DropClient( cl, "EXE_TIMEDOUT" );
				cl->state = CS_FREE;    // don't bother with zombie state
			}
		} else if ( cl->state == CS_PRIMED && cl->lastPacketTime < primeddroppoint ) {
			// wait several frames so a debugger session doesn't
			// cause a timeout
			if ( ++cl->timeoutCount > 5 ) {
				SV_DropClient( cl, "EXE_TIMEDOUT" );
				cl->state = CS_FREE;    // don't bother with zombie state
			}
		} else {
			cl->timeoutCount = 0;
		}
	}
}

/*
==================
SV_CheckPaused
==================
*/
qboolean SV_CheckPaused( void ) {
	client_t    *cl;
	int i;

/*
	if ( !cl_paused->boolean ) {
		return qfalse;
	}
*/
	// only pause if there is not a single client connected
	for ( i = 0,cl = svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
		if ( cl->state >= CS_ZOMBIE && cl->netchan.remoteAddress.type != NA_BOT ) {
			if ( sv_paused->boolean ) {
				Cvar_SetBool( sv_paused, qfalse );
			}
			return qfalse;
		}
	}
	if ( !sv_paused->boolean ) {
		Cvar_SetBool( sv_paused, qtrue );
	}
	return qtrue;
}


/*
==================
SV_FrameMsec
Return time in millseconds until processing of the next server frame.
==================
*/
unsigned int SV_FrameUsec()
{
	if(sv_fps)
	{
		unsigned int frameUsec;
		
		frameUsec = 1000000 / sv_fps->integer;
		
		if(frameUsec < sv.timeResidual)
			return 0;
		else
			return frameUsec - sv.timeResidual;
	}
	else
		return 1;
}

/*
==================
SV_Frame

Player movement occurs as a result of packet events, which
happen before SV_Frame is called
==================
*/
__optimize3 __regparm1 qboolean SV_Frame( unsigned int usec ) {
	unsigned int frameUsec;
	char mapname[MAX_QPATH];
	client_t* client;
	int i;
    static qboolean underattack = qfalse;
	mvabuf;


	if ( !com_sv_running->boolean ) {
		usleep(20000);
		return qtrue;
	}


	// allow pause if only the local client is connected
/*	if ( SV_CheckPaused() ) {
		SV_MasterHeartbeat( HEARTBEAT_GAME );//Still send heartbeats
		CL_WritePacket( &svse.authserver );
		CL_WritePacket( &svse.scrMaster );
		return;
	}
*/
	// if it isn't time for the next frame, do nothing
	frameUsec = sv.frameusec * com_timescale->value;

	// don't let it scale below 1ms
	if(frameUsec < 1000)
	{
		frameUsec = 1000;
	}
	sv.timeResidual += usec;

	if ( sv.timeResidual < frameUsec ) {
		// NET_Sleep will give the OS time slices until either get a packet
		// or time enough for a server frame has gone by
		underattack = NET_Sleep( frameUsec - sv.timeResidual );
		return qfalse;
	}

	if(underattack)
		NET_Clear();

	SV_PreFrame( );

	// run the game simulation in chunks
	while ( sv.timeResidual >= frameUsec ) {
		sv.timeResidual -= frameUsec;
		svs.time += frameUsec / 1000;

		// let everything in the world think and move
		G_RunFrame( svs.time );
	}

	// send messages back to the clients
	SV_SendClientMessages();

	Scr_SetLoading(0);

	// update ping based on the all received frames
	SV_CalcPings();

	// check timeouts
	SV_CheckTimeouts();

	// send a heartbeat to the master if needed
	SV_MasterHeartbeat( HEARTBEAT_GAME );
#ifdef PUNKBUSTER
	PbServerProcessEvents( 0 );
#endif
	// if time is about to hit the 32nd bit, kick all clients
	// and clear sv.time, rather
	// than checking for negative time wraparound everywhere.
	// 2giga-milliseconds = 23 days, so it won't be too often
	if ( svs.time > 0x70000000 ) {
		Q_strncpyz( mapname, sv_mapname->string, sizeof(mapname) );
		SV_Shutdown( "EXE_SERVERRESTARTTIMEWRAP" );
		Com_Restart( );
		// TTimo
		// show_bug.cgi?id=388
		// there won't be a map_restart if you have shut down the server
		// since it doesn't restart a non-running server
		// instead, re-run the current map
		SV_Map( mapname );
		return qtrue;
	}

	// this can happen considerably earlier when lots of clients play and the map doesn't change
	if ( svs.nextSnapshotEntities >= 0x7FFFFFFE - svs.numSnapshotEntities ) {
		Q_strncpyz( mapname, sv_mapname->string, MAX_QPATH );
		SV_Shutdown( "EXE_SERVERRESTARTMISC\x15numSnapshotEntities wrapping" );
		Com_Restart( );
		// TTimo see above
		SV_Map( mapname );
		return qtrue;
	}

	if ( svs.nextSnapshotClients >= 0x7FFFFFFE - svs.numSnapshotClients ) {
		Q_strncpyz( mapname, sv_mapname->string, MAX_QPATH );
		SV_Shutdown( "EXE_SERVERRESTARTMISC\x15numSnapshotClients wrapping" );
		Com_Restart( );
		// TTimo see above
		SV_Map( mapname );
		return qtrue;
	}

	if ( svs.nextCachedSnapshotEntities >= 0x7FFFBFFD ) {
		Q_strncpyz( mapname, sv_mapname->string, MAX_QPATH );
		SV_Shutdown( "EXE_SERVERRESTARTMISC\x15nextCachedSnapshotEntities wrapping" );
		Com_Restart( );
		// TTimo see above
		SV_Map( mapname );
		return qtrue;
	}

	if ( svs.nextCachedSnapshotClients >= 0x7FFFEFFD ) {
		Q_strncpyz( mapname, sv_mapname->string, MAX_QPATH );
		SV_Shutdown( "EXE_SERVERRESTARTMISC\x15nextCachedSnapshotClients wrapping" );
		Com_Restart( );
		// TTimo see above
		SV_Map( mapname );
		return qtrue;
	}


	if ( svs.nextArchivedSnapshotFrames >= 0x7FFFFB4D ) {
		Q_strncpyz( mapname, sv_mapname->string, MAX_QPATH );
		SV_Shutdown( "EXE_SERVERRESTARTMISC\x15nextArchivedSnapshotFrames wrapping" );
		Com_Restart( );
		// TTimo see above
		SV_Map( mapname );
		return qtrue;
	}

	if ( svs.nextArchivedSnapshotBuffer >= 0x7DFFFFFD ) {
		Q_strncpyz( mapname, sv_mapname->string, MAX_QPATH );
		SV_Shutdown( "EXE_SERVERRESTARTMISC\x15nextArchivedSnapshotBuffer wrapping" );
		Com_Restart( );
		// TTimo see above
		SV_Map( mapname );
		return qtrue;
	}

	if ( svs.nextCachedSnapshotFrames >= 0x7FFFFDFD ) {
		Q_strncpyz( mapname, sv_mapname->string, MAX_QPATH );
		SV_Shutdown( "EXE_SERVERRESTARTMISC\x15svs.nextCachedSnapshotFrames wrapping" );
		Com_Restart( );
		// TTimo see above
		SV_Map( mapname );
		return qtrue;
	}
	SetAnimCheck(com_animCheck->boolean);


        if( svs.time > svse.frameNextSecond){	//This runs each second
	    svse.frameNextSecond = svs.time+1000;

	    // the menu kills the server with this cvar
	    if ( sv_killserver->boolean ) {
		SV_Shutdown( "Server was killed.\n" );
		Cvar_SetBool( sv_killserver, qfalse );
		return qtrue;
	    }

	    if(svs.time > svse.frameNextTenSeconds){	//This runs each 10 seconds
		svse.frameNextTenSeconds = svs.time+10000;

		int d, h, m;
		int uptime;

		uptime = Sys_Seconds();
		d = uptime/(60*60*24);
//		uptime = uptime%(60*60*24);
		h = uptime/(60*60);
//		uptime = uptime%(60*60);
		m = uptime/60;

		if(h < 4)
			Cvar_SetString(sv_uptime, va("%i minutes", m));
		else if(d < 3)
			Cvar_SetString(sv_uptime, va("%i hours", h));
		else
			Cvar_SetString(sv_uptime, va("%i days", d));

		serverStatus_Write();

	        PHandler_Event(PLUGINS_ONTENSECONDS, NULL);	// Plugin event
/*		if(svs.time > svse.nextsecret){
			svse.nextsecret = svs.time+80000;
			Com_RandomBytes((byte*)&svse.secret,sizeof(int));
		}*/

		if(level.time > level.startTime + 20000){
			for(client = svs.clients, i = 0; i < sv_maxclients->integer; i++, client++){
				if(client->state != CS_ACTIVE)
					continue;
			
				if(client->netchan.remoteAddress.type == NA_BOT)
					continue;

				G_PrintRuleForPlayer(client);
				G_PrintAdvertForPlayer(client);
			}
		}

	    }
	}

	return qtrue;
}


void SV_SayToPlayers(int clnum, int team, char* text)
{

	client_t *cl;

	if(clnum >= 0 && clnum < 64){
		SV_SendServerCommand(&svs.clients[clnum], "h \"%s\"", text);
		return;
	}
	if(team == -1)
	{
		SV_SendServerCommand(NULL, "h \"%s\"", text);
		return;
	}
	for(cl = svs.clients, clnum = 0; clnum < sv_maxclients->integer; clnum++){

		if(cl->state < CS_ACTIVE)
			continue;

		if(team != level.clients[clnum].sess.sessionTeam)
			continue;

		SV_SendServerCommand(cl, "h \"%s\"", text);
	}
}

/*
===============
SV_GetUserinfo

===============
*/
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetUserinfo: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error( ERR_DROP, "SV_GetUserinfo: bad index %i\n", index );
	}
	Q_strncpyz( buffer, svs.clients[ index ].userinfo, bufferSize );
}


qboolean SV_UseUids()
{
    return psvs.useuids;
}

const char* SV_GetMapRotation()
{
    return sv_mapRotation->string;
}

const char* SV_GetNextMap()
{
    return sv_nextmap->string;
}


void SV_ShowClientUnAckCommands( client_t *client )
{
	int i;
	
	Com_Printf("-- Unacknowledged Server Commands for client %i:%s --\n", client - svs.clients, client->name);
	
	for ( i = client->reliableAcknowledge + 1; i <= client->reliableSequence; ++i )
	{
		Com_Printf("cmd %5d: %8d: %s\n", i, client->reliableCommands[i & (MAX_RELIABLE_COMMANDS -1)].cmdTime,
				   client->reliableCommands[i & (MAX_RELIABLE_COMMANDS -1)].command );
	}
	
	Com_Printf("-----------------------------------------------------\n");
}