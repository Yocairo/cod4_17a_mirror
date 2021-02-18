/*
===========================================================================
    Copyright (C) 2010-2013  Ninja and TheKelm of the IceOps-Team

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



#include "hl2rcon.h"
#include "q_shared.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "cmd.h"
#include "msg.h"
#include "sys_net.h"
#include "server.h"
#include "net_game_conf.h"
#include "punkbuster.h"
#include "net_game.h"
#include "g_sv_shared.h"
#include "sv_auth.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
/*
========================================================================

Source Rcon facility

========================================================================
*/

#ifndef MAX_MSGLEN
#define MAX_MSGLEN 0x20000
#endif


sourceRcon_t sourceRcon;
#define HL2RCON_SOURCEOUTPUTBUF_LENGTH 4096


void HL2Rcon_SourceRconStreaming_enable( int type, int uid ){

	rconUser_t* user;
	char* c;
	char* cg;
	char* ch;
	char* ev;

	if(sourceRcon.redirectUser < 1 || sourceRcon.redirectUser > MAX_RCONUSERS){
		Com_Printf("This command can only be used from SourceRcon\n");
		return;
	}

	user = &sourceRcon.activeRconUsers[sourceRcon.redirectUser -1];

	if(Auth_GetClPowerByUID(uid) > 98 || !(type & 1))
	{
		user->streamlog = type & 1;

	}else if(type & 1){
		Com_Printf("Insufficient permissions to open console logfile!\n");

	}
	user->streamgamelog = type & 2;
	user->streamchat = type & 4;
	user->streamevents = type & 8;

	if(user->streamlog)
		c = "logfile";
	else
		c = "";

	if(user->streamgamelog)
		cg = "gamelog";
	else
		cg = "";

	if(user->streamchat)
		ch = "chat";
	else
		ch = "";

	if(user->streamevents)
		ev = "events";
	else
		ev = "";

	Com_Printf("Streaming turned on for: %s %s %s %s\n", c, cg, ch, ev);
}

void HL2Rcon_SourceRconDisconnect(netadr_t *from, int connectionId)
{

	if(connectionId < 0 || connectionId >=  MAX_RCONUSERS){
		Com_Error(ERR_FATAL, "HL2Rcon_SourceRconDisconnect: bad connectionId: %i", connectionId);
		return;
	}
	sourceRcon.activeRconUsers[connectionId].remote.type = NA_BAD;
	sourceRcon.activeRconUsers[connectionId].streamlog = 0;
	sourceRcon.activeRconUsers[connectionId].streamchat = 0;
	sourceRcon.activeRconUsers[connectionId].streamgamelog = 0;
	sourceRcon.activeRconUsers[connectionId].streamevents = 0;

}


tcpclientstate_t HL2Rcon_SourceRconAuth(netadr_t *from, msg_t *msg, int *connectionId){

	int packetlen;
	int packettype;
	int packetid;
	char* loginstring;
	char* username;
	char* password;
	byte msgbuf[32];
	msg_t sendmsg;
	rconUser_t* user;
	int i;
	char buf[MAX_STRING_CHARS];
	char stringlinebuf[MAX_STRING_CHARS];

	MSG_BeginReading(msg);
	packetlen = MSG_ReadLong(msg);

	if(packetlen != msg->cursize - 4){//Not a source rcon packet

		Com_Printf("Not a source rcon packet: len %d size %d\n", packetlen, msg->cursize);

		return TCP_AUTHNOTME;
	}
	packetid = MSG_ReadLong(msg);

	packettype = MSG_ReadLong(msg);

	if(packettype != SERVERDATA_AUTH)//Not a source rcon auth-packet
		return TCP_AUTHNOTME;

	if(SV_PlayerBannedByip(from, buf, sizeof(buf))){
		return TCP_AUTHBAD;
	}
	
	MSG_Init(&sendmsg, msgbuf, sizeof(msgbuf));
	MSG_WriteLong(&sendmsg, 10);
	MSG_WriteLong(&sendmsg, 0);
	MSG_WriteLong(&sendmsg, SERVERDATA_RESPONSE_VALUE);
	MSG_WriteShort(&sendmsg, 0);
	if(NET_SendData(from->sock, &sendmsg) < 1)
	{
		return TCP_AUTHBAD;
	}

	MSG_Init(&sendmsg, msgbuf, sizeof(msgbuf));
	MSG_WriteLong(&sendmsg, 10);

	loginstring = MSG_ReadStringLine(msg, stringlinebuf, sizeof(stringlinebuf));

	Cmd_TokenizeString(loginstring);

	if(Cmd_Argc() != 2){
		goto badrcon;
	}
	username = Cmd_Argv(0);
	password = Cmd_Argv(1);

	if(strlen(password) < 6){
		goto badrcon;
	}

	if(Auth_Authorize(username, password) < 0)
	{
		goto badrcon;
	}

	Com_Printf("Rcon login from: %s Name: %s\n", NET_AdrToString (from), username);

	Cmd_EndTokenizedString();

	for(i = 0, user = sourceRcon.activeRconUsers; i < MAX_RCONUSERS; i++, user++){
		if(user->remote.type == NA_BAD)
			break;
	}

	if(i == MAX_RCONUSERS){
		return TCP_AUTHBAD; //Close connection
	}


	user->remote = *from;
	user->uid = Auth_GetUID(username);
//	user->rconPower = login->power;
	Q_strncpyz(user->rconUsername, username, sizeof(user->rconUsername));
	user->streamchat = 0;
	user->streamlog = 0;
	user->lastpacketid = packetid;
	*connectionId = i;

	MSG_WriteLong(&sendmsg, user->lastpacketid);
	MSG_WriteLong(&sendmsg, SERVERDATA_AUTH_RESPONSE);
	MSG_WriteShort(&sendmsg, 0);
	if(NET_SendData(from->sock, &sendmsg) < 1)
	{
		return TCP_AUTHBAD;
	}

	return TCP_AUTHSUCCESSFULL;


badrcon:
	Cmd_EndTokenizedString();
	Com_Printf ("Bad rcon from %s (TCP)\n", NET_AdrToString (from) );
	//Don't allow another attempt for 20 seconds
	SV_PlayerAddBanByip(from, "Bad rcon", 0, NULL, 0, Com_GetRealtime() + 20);

	MSG_Init(&sendmsg, msgbuf, sizeof(msgbuf));
	MSG_WriteLong(&sendmsg, 10);
	MSG_WriteLong(&sendmsg, -1);
	MSG_WriteLong(&sendmsg, SERVERDATA_AUTH_RESPONSE);
	MSG_WriteShort(&sendmsg, 0);
	NET_SendData(from->sock, &sendmsg);
	return TCP_AUTHBAD;

}



void HL2Rcon_SourceRconSendConsole( const char* data, int msglen)
{
	HL2Rcon_SourceRconSendDataToEachClient( (const byte*)data, msglen, SERVERDATA_CONLOG);
}

void HL2Rcon_SourceRconSendGameLog( const char* data, int msglen)
{
	HL2Rcon_SourceRconSendDataToEachClient( (const byte*)data, msglen, SERVERDATA_GAMELOG);
}



void HL2Rcon_SourceRconSendChat( const char* data, int clientnum, int mode)
{
    HL2Rcon_SourceRconSendChatToEachClient( data, NULL, clientnum, qfalse);
}

void HL2Rcon_SourceRconSendDataToEachClient( const byte* data, int msglen, int type){

	rconUser_t* user;
	int i;
	msg_t msg;
	int32_t *updatelen;
	byte sourcemsgbuf[MAX_MSGLEN];
	qboolean msgbuild = qfalse;

	for(i = 0, user = sourceRcon.activeRconUsers; i < MAX_RCONUSERS; i++, user++ ){

		if(!user->streamgamelog && type == SERVERDATA_GAMELOG)
			continue;

		if(!user->streamlog && type == SERVERDATA_CONLOG)
			continue;

		if(!user->streamevents && type == SERVERDATA_EVENT)
			continue;

		
		if(!msgbuild){
			MSG_Init(&msg, sourcemsgbuf, sizeof(sourcemsgbuf));
			MSG_WriteLong(&msg, 0); //writing 0 for now
			MSG_WriteLong(&msg, 0);
			MSG_WriteLong(&msg, type);

			if(type == SERVERDATA_EVENT)
				MSG_WriteData(&msg, data, msglen);
			else
				MSG_WriteBigString(&msg, (char*)data);

			MSG_WriteByte(&msg, 0);

			//Adjust the length
			updatelen = (int32_t*)msg.data;
			*updatelen = msg.cursize - 4;
			msgbuild = qtrue;
		}
		NET_SendData(user->remote.sock, &msg);
	}
}




void HL2Rcon_SourceRconSendChatToEachClient( const char *text, rconUser_t *self, int cid, qboolean onlyme){

	rconUser_t* user;
	int i;
	msg_t msg;
	int32_t *updatelen;
	byte sourcemsgbuf[MAX_MSGLEN];



	for(i = 0, user = sourceRcon.activeRconUsers; i < MAX_RCONUSERS; i++, user++ ){

		if(!user->streamchat)
			continue;

		MSG_Init(&msg, sourcemsgbuf, sizeof(sourcemsgbuf));
		MSG_WriteLong(&msg, 0); //writing 0 for now
		MSG_WriteLong(&msg, 0);
		MSG_WriteLong(&msg, SERVERDATA_CHAT);

		if(self){
			if(self == user)
			{

				MSG_WriteByte(&msg, -2);
			}else{

				MSG_WriteByte(&msg, -1);
				if(onlyme)
				{
				    continue;
				}
			}
			MSG_WriteBigString(&msg, user->rconUsername);

		}else{

			MSG_WriteByte(&msg, cid);
		}


		MSG_WriteBigString(&msg, text);
		MSG_WriteByte(&msg, 0);

		//Adjust the length
		updatelen = (int32_t*)msg.data;
		*updatelen = msg.cursize - 4;

		NET_SendData(user->remote.sock, &msg);
	}
}


void HL2Rcon_SourceRconFlushRedirect(char* outputbuf, qboolean lastcommand){

	rconUser_t* user;

	if(sourceRcon.redirectUser < 1 || sourceRcon.redirectUser > MAX_RCONUSERS)
		return;

	user = &sourceRcon.activeRconUsers[sourceRcon.redirectUser -1];

	msg_t msg;
	int32_t *updatelen;
	byte sourcemsgbuf[HL2RCON_SOURCEOUTPUTBUF_LENGTH+16];

	MSG_Init(&msg, sourcemsgbuf, sizeof(sourcemsgbuf));
	MSG_WriteLong(&msg, 0); //writing 0 for now
	MSG_WriteLong(&msg, user->lastpacketid);
	MSG_WriteLong(&msg, SERVERDATA_RESPONSE_VALUE);
	MSG_WriteBigString(&msg, outputbuf);

	MSG_WriteByte(&msg, 0);

	//Adjust the length
	updatelen = (int32_t*)msg.data;
	*updatelen = msg.cursize - 4;

	NET_SendData(user->remote.sock, &msg);
}


void HL2Rcon_SayToPlayers(int clientnum, int team, const char* chatline)
{

	char		line[512];
	rconUser_t*	user;

	if(sourceRcon.redirectUser < 1 || sourceRcon.redirectUser > MAX_RCONUSERS){
		Com_Printf("This command can only be used from SourceRcon\n");
		return;
	}

	user = &sourceRcon.activeRconUsers[sourceRcon.redirectUser -1];

	if(clientnum != -1)
	{
		Com_sprintf(line, sizeof(line), "^5%s^7(Rcon): %s\n", user->rconUsername, chatline);
		HL2Rcon_SourceRconSendChatToEachClient( chatline, user, 0, qtrue);
	}else{
		Com_sprintf(line, sizeof(line), "^2%s^7(Rcon): %s\n", user->rconUsername, chatline);
		HL2Rcon_SourceRconSendChatToEachClient( chatline, user, 0, qfalse);
	}
        SV_SayToPlayers(clientnum, team, line);
}

#define SV_OUTPUTBUF_LENGTH 4096

void HL2Rcon_ExecuteConsoleCommand(const char* command, int uid)
{
	char sv_outputbuf[SV_OUTPUTBUF_LENGTH];
	char buffer[960];
	char cmd[48];
	int power, powercmd, oldpower, oldinvokeruid, oldinvokerclnum, i;
	
	
	if((power = Auth_GetClPowerByUID(uid)) < 100)
	{
		i = 0;
		/* Get the current user's power 1st */
		while ( command[i] != ' ' && command[i] != '\0' && command[i] != '\n' && i < 32 ){
			i++;
		}
		if(i > 29 || i < 3) return;
		
		Q_strncpyz(cmd,command,i+1);
		
		//Prevent buffer overflow as well as prevent the execution of priveleged commands by using seperator characters
		Q_strncpyz(buffer, command, sizeof(buffer));
		Q_strchrrepl(buffer,';','\0');
		Q_strchrrepl(buffer,'\n','\0');
		Q_strchrrepl(buffer,'\r','\0');
		// start redirecting all print outputs to the packet
		
		powercmd = Cmd_GetPower(cmd);
		if(powercmd > power)
		{
			Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, HL2Rcon_SourceRconFlushRedirect);
			Com_Printf("Insufficient permissions!\n");
			Com_EndRedirect();
			return;
		}
		
		oldpower = Cmd_GetInvokerPower();
		oldinvokeruid = Cmd_GetInvokerUID();
		oldinvokerclnum = Cmd_GetInvokerClnum();
		Cmd_SetCurrentInvokerInfo(uid, power, -1);
		
		Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, HL2Rcon_SourceRconFlushRedirect);
		Cmd_ExecuteSingleCommand(0,0, buffer);
		
		Cmd_SetCurrentInvokerInfo(oldinvokeruid, oldpower, oldinvokerclnum);
		
	}else{
		Com_BeginRedirect (sv_outputbuf, SV_OUTPUTBUF_LENGTH, HL2Rcon_SourceRconFlushRedirect);
		Cmd_ExecuteSingleCommand(0,0, command);
#ifdef PUNKBUSTER
		if(!Q_stricmpn(command, "pb_sv_", 6)) PbServerForceProcess();
#endif
	}

	Com_EndRedirect();
}



qboolean HL2Rcon_SourceRconEvent(netadr_t *from, msg_t *msg, int connectionId){

//    int packetlen;
    int packettype;
    int type;
    int8_t team;
    int8_t clientnum;
    int32_t *updatelen;
    char* command;
    char* chatline;
    char sv_outputbuf[HL2RCON_SOURCEOUTPUTBUF_LENGTH];
    msg_t msg2;
    byte data[20000];
	char stringbuf[8 * MAX_STRING_CHARS];

    MSG_BeginReading(msg);

    while(msg->readcount < msg->cursize)
    {
	//packetlen = 
	MSG_ReadLong(msg);

	if(connectionId < 0 || connectionId >= MAX_RCONUSERS)
		return qtrue;

	rconUser_t* user;
	user = &sourceRcon.activeRconUsers[connectionId];

	user->lastpacketid = MSG_ReadLong(msg);

	packettype = MSG_ReadLong(msg);
	
	switch(packettype)
	{
		case SERVERDATA_GETSTATUS:
		//status request
		    //Pop the end of body byte
		    MSG_ReadByte(msg);

		    MSG_Init(&msg2, data, sizeof(data));
		    MSG_WriteLong(&msg2, 0); //writing 0 for now
		    MSG_WriteLong(&msg2, user->lastpacketid); // ID
		    MSG_WriteLong(&msg2, SERVERDATA_STATUSRESPONSE); // Type: status response
		    SV_WriteRconStatus(&msg2);
		    MSG_WriteByte(&msg2, 0);

		    //Adjust the length
		    updatelen = (int32_t*)msg2.data;
		    *updatelen = msg2.cursize - 4;
		    NET_SendData(from->sock, &msg2);
		    break;

		case SERVERDATA_EXECCOMMAND:

		    command = MSG_ReadString(msg, stringbuf, sizeof(stringbuf));

		    //Pop the end of body byte
		    MSG_ReadByte(msg);

		    Com_Printf("Rcon from: %s command: %s\n", NET_AdrToString(from), command);

		    sourceRcon.redirectUser = connectionId+1;
		    HL2Rcon_ExecuteConsoleCommand(command, user->uid);
		    sourceRcon.redirectUser = 0;
		    break;

		case SERVERDATA_TURNONSTREAM:

		    type = MSG_ReadByte(msg);

		    //Pop the end of body byte
		    MSG_ReadByte(msg);

		    sourceRcon.redirectUser = connectionId+1;
		    Com_BeginRedirect (sv_outputbuf, sizeof(sv_outputbuf), HL2Rcon_SourceRconFlushRedirect);
		    HL2Rcon_SourceRconStreaming_enable( type, user->uid );
		    Com_EndRedirect ();
		    sourceRcon.redirectUser = 0;
		    break;

		case SERVERDATA_SAY:
		    clientnum = MSG_ReadByte(msg); // -1 if Team or for all is used
		    team = MSG_ReadByte(msg); // teamnumber or -1 if it is for all team or clientnum is set
		    chatline = MSG_ReadString(msg, stringbuf, sizeof(stringbuf));

		    //Pop the end of body byte
		    MSG_ReadByte(msg);

		    sourceRcon.redirectUser = connectionId+1;
		    HL2Rcon_SayToPlayers(clientnum, team, chatline);
		    sourceRcon.redirectUser = 0;
		    break;

		default:
		//Not a source rcon packet
		Com_Printf("Not a valid source rcon packet from: %s received. Type: %d - Closing connection\n", NET_AdrToString(from), packettype);
		return qtrue;
	}
    }
    return qfalse;
}




void HL2Rcon_Init(){

	static qboolean	initialized;

	if ( initialized ) {
		return;
	}
	initialized = qtrue;

//	Cmd_AddCommand ("rcondeladmin", HL2Rcon_UnsetSourceRconAdmin_f);
//	Cmd_AddCommand ("rconaddadmin", HL2Rcon_SetSourceRconAdmin_f);
//	Cmd_AddCommand ("rconlistadmins", HL2Rcon_ListSourceRconAdmins_f);

	NET_TCPAddEventType(HL2Rcon_SourceRconEvent, HL2Rcon_SourceRconAuth, HL2Rcon_SourceRconDisconnect, 9038723);

	Com_AddRedirect(HL2Rcon_SourceRconSendConsole);
	G_PrintAddRedirect(HL2Rcon_SourceRconSendGameLog);
	G_AddChatRedirect(HL2Rcon_SourceRconSendChat);

}


void HL2Rcon_EventClientEnterWorld(int cid){

    byte data[2];

    data[0] = RCONEVENT_PLAYERENTERGAME;
    data[1] = cid;

    HL2Rcon_SourceRconSendDataToEachClient( data, 2, SERVERDATA_EVENT);

}

void HL2Rcon_EventClientLeave(int cid){

    byte data[2];

    data[0] = RCONEVENT_PLAYERLEAVE;
    data[1] = cid;

    HL2Rcon_SourceRconSendDataToEachClient( data, 2, SERVERDATA_EVENT);

}

void HL2Rcon_EventLevelStart()
{

    byte data[1];

    data[0] = RCONEVENT_LEVELSTART;

    HL2Rcon_SourceRconSendDataToEachClient( data, 1, SERVERDATA_EVENT);

}

void HL2Rcon_EventClientEnterTeam(int cid, int team){

    byte data[4];

    data[0] = RCONEVENT_PLAYERENTERTEAM;
    data[1] = cid;
    data[2] = team;

    HL2Rcon_SourceRconSendDataToEachClient( data, 3, SERVERDATA_EVENT);

}
