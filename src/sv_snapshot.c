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
#include "net_game_conf.h"
#include "server.h"
#include "huffman.h"
#include "msg.h"
#include "sys_main.h"


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/*
=============================================================================

Delta encode a client frame onto the network channel

A normal server packet will look like:

4	sequence number (high bit set if an oversize fragment)
<optional reliable commands>
1	svc_snapshot
4	last client reliable command
4	serverTime
1	lastframe for delta compression
1	snapFlags
1	areaBytes
<areabytes>
<playerstate>
<packetentities>

=============================================================================
*/

/*
==================
SV_UpdateServerCommandsToClient

(re)send all server commands the client hasn't acknowledged yet
==================
*/
__cdecl void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) {
	int i;
//	extclient_t* extcl = &svs.extclients[ client - svs.clients ];

	// write any unacknowledged serverCommands
	for ( i = client->reliableAcknowledge + 1 ; i <= client->reliableSequence ; i++ ) {
		MSG_WriteByte( msg, svc_serverCommand );
		MSG_WriteLong( msg, i );
		//MSG_WriteString( msg, extcl->reliableCommands[ i & ( MAX_RELIABLE_COMMANDS - 1 ) ].command );
		MSG_WriteString( msg, client->reliableCommands[ i & ( MAX_RELIABLE_COMMANDS - 1 ) ].command );
	}

	client->reliableSent = client->reliableSequence;

}

void SV_UpdateServerCommandsToClientRecover( client_t *client, msg_t *msg )
{
	int i;
	int cmdlen;
	
	for(i = client->reliableAcknowledge + 1; i <= client->reliableSequence; i++)
	{
		
		cmdlen = strlen(client->reliableCommands[i & ( MAX_RELIABLE_COMMANDS - 1 )].command);
		
		if ( cmdlen + msg->cursize + 6 >= msg->maxsize )
			break;
		
		MSG_WriteByte(msg, svc_serverCommand);
		MSG_WriteLong(msg, i);
		MSG_WriteString(msg, client->reliableCommands[i & ( MAX_RELIABLE_COMMANDS - 1 )].command);
	
	}
	if ( i - 1 > client->reliableSent )
	client->reliableSent = i - 1;
}


__cdecl void SV_WriteSnapshotToClient(client_t* client, msg_t* msg){

    snapshotInfo_t snapInfo;
    int lastframe;
    int from_num_entities;
    int newindex, oldindex, newnum, oldnum;
    clientState_ts *newcs, *oldcs;
    entityState_t *newent, *oldent;
    clientSnapshot_t *frame, *oldframe;
    int i;
    int snapFlags;
    int var_x, from_first_entity, from_num_clients, from_first_client;

    snapInfo.clnum = client - svsHeader.clients;
    snapInfo.cl = (void*)client;
    snapInfo.var_01 = 0;
    snapInfo.var_02 = 0;
    snapInfo.var_03 = 0;

    frame = &client->frames[client->netchan.outgoingSequence & PACKET_MASK];
    frame->var_03 = svsHeader.time;

    if(client->deltaMessage <= 0 ||  client->state != CS_ACTIVE) {
        oldframe = NULL;
        lastframe = 0;
        var_x = 0;

    } else if(client->netchan.outgoingSequence - client->deltaMessage >= PACKET_BACKUP - 3) {
        Com_DPrintf("%s: Delta request from out of date packet.\n", client->name);
        oldframe = NULL;
        lastframe = 0;
        var_x = 0;

    } else if(client->demoDeltaFrameCount <= 0 && client->demorecording){

        oldframe = NULL;
        lastframe = 0;
        var_x = 0;
        client->demowaiting = qfalse;
        Com_DPrintf("Force a nondelta frame for %s for demo recording\n", client->name);

        if(client->demoMaxDeltaFrames < 1024)
        {
            client->demoMaxDeltaFrames <<= 1;
        }
        client->demoDeltaFrameCount = client->demoMaxDeltaFrames;


    } else {
        oldframe = &client->frames[client->deltaMessage & PACKET_MASK];
        lastframe = client->netchan.outgoingSequence - client->deltaMessage;
        var_x = oldframe->var_03;
        client->demoDeltaFrameCount--;

        if(oldframe->first_entity <  svsHeader.nextSnapshotEntities - svsHeader.numSnapshotEntities) {
            Com_PrintWarning("%s: Delta request from out of date entities - delta against entity %i, oldest is %i, current is %i.  Their old snapshot had %i entities in it\n",
                            client->name, oldframe->first_entity, svs.nextSnapshotEntities - svs.numSnapshotEntities, svs.nextSnapshotEntities, oldframe->num_entities );
            oldframe = NULL;
            lastframe = 0;
            var_x = 0;

        } else if(oldframe->first_client <  svsHeader.nextSnapshotClients - svsHeader.numSnapshotClients) {

            Com_PrintWarning("%s: Delta request from out of date clients - delta against client %i, oldest is %i, current is %i.  Their old snapshot had %i clients in it\n", 
                            client->name, oldframe->first_client, svs.nextSnapshotClients - svs.numSnapshotClients, svs.nextSnapshotClients, oldframe->num_clients);
            oldframe = NULL;
            lastframe = 0;
            var_x = 0;
        }
    }


    MSG_WriteByte(msg, svc_snapshot);
    MSG_WriteLong(msg, svsHeader.time);
    MSG_WriteByte(msg, lastframe);
    snapInfo.var_01 = var_x;

    snapFlags = svsHeader.snapFlagServerBit;

    if(client->rateDelayed){
	snapFlags |= 1;
    }

    if(client->state == CS_ACTIVE) {

	client->unksnapshotvar = 1;

    } else {
	if(client->state != CS_ZOMBIE){
		client->unksnapshotvar = 0;
	}
    }

    if(!client->unksnapshotvar){
	snapFlags |= 2;
    }

    MSG_WriteByte(msg, snapFlags);

    if(oldframe) {
		MSG_WriteDeltaPlayerstate( &snapInfo, msg, svsHeader.time, &oldframe->ps, &frame->ps);
		from_num_entities = oldframe->num_entities;
		from_first_entity = oldframe->first_entity;
		from_num_clients = oldframe->num_clients;
		from_first_client = oldframe->first_client;
    } else {
	        MSG_WriteDeltaPlayerstate( &snapInfo, msg, svsHeader.time, 0, &frame->ps);
		from_num_entities = 0;
		from_first_entity = 0;
		from_num_clients = 0;
		from_first_client = 0;
    }

    MSG_ClearLastReferencedEntity(msg);


    newindex = 0;
    oldindex = 0;

//    Com_Printf("\nDelta client: %i:\n", snapInfo.clnum);


    while ( newindex < frame->num_entities || oldindex < from_num_entities){
	if ( newindex >= frame->num_entities ) {
		newnum = 9999;
		newent = NULL;
	} else {
		newent = &svsHeader.snapshotEntities[( frame->first_entity + newindex ) % svsHeader.numSnapshotEntities];
		newnum = newent->number;
	}

	if ( oldindex >= from_num_entities ) {
		oldnum = 9999;
		oldent = NULL;
	} else {
		oldent = &svsHeader.snapshotEntities[( from_first_entity + oldindex ) % svsHeader.numSnapshotEntities];
		oldnum = oldent->number;
	}

	if ( newnum == oldnum ) {
		// delta update from old position
		// because the force parm is qfalse, this will not result
		// in any bytes being emited if the entity has not changed at all
//		if(newent->number < 64 || oldent->number < 64)
//			Com_Printf("   Delta Update Entity - New delta: %i, %x  Old delta: %i, %x\n", newent->number, newent, oldent->number, oldent);
		MSG_WriteDeltaEntity( &snapInfo, msg, svsHeader.time, oldent, newent, qfalse );
		oldindex++;
		newindex++;
		continue;
	}

	if ( newnum < oldnum ) {
		// this is a new entity, send it from the baseline
		snapInfo.var_02 = 1;
//		if(newent->number < 64)
//			Com_Printf("   Delta Add Entity: %i, %x\n", newent->number, newent);
		MSG_WriteDeltaEntity( &snapInfo, msg, svsHeader.time, &svsHeader.svEntities[newnum].baseline, newent, qtrue );
		snapInfo.var_02 = 0;
		newindex++;
		continue;
	}

	if ( newnum > oldnum ) {
		// the old entity isn't present in the new message
//		if(oldent->number < 64)
//			Com_Printf("   Delta Remove Entity: %i, %x\n", oldent->number, oldent);
		MSG_WriteDeltaEntity( &snapInfo, msg, svsHeader.time, oldent, NULL, qtrue );
		oldindex++;
		continue;
	}
    }


    MSG_WriteEntityIndex(&snapInfo, msg, ( MAX_GENTITIES - 1 ), GENTITYNUM_BITS);
    MSG_ClearLastReferencedEntity(msg);

    newindex = 0;
    oldindex = 0;

    while(newindex < frame->num_clients || oldindex < from_num_clients){
	if(newindex >= frame->num_clients){
		newnum = 9999;
		newcs = NULL;
	}else{

		newcs = &svsHeader.snapshotClients[(frame->first_client + newindex) % svsHeader.numSnapshotClients];
		newnum = newcs->number;
	}

	if(oldindex >= from_num_clients){
		oldnum = 9999;
		oldcs = NULL;
	}else{

		oldcs = &svsHeader.snapshotClients[(from_first_client + oldindex) % svsHeader.numSnapshotClients];
		oldnum = oldcs->number;
	}

	if ( newnum == oldnum ) {
		// delta update from old position
		// because the force parm is qfalse, this will not result
		// in any bytes being emited if the entity has not changed at all
		MSG_WriteDeltaClient( &snapInfo, msg, svsHeader.time, oldcs, newcs, qfalse );
		oldindex++;
		newindex++;
		continue;
	}

	if ( newnum < oldnum ) {
		MSG_WriteDeltaClient( &snapInfo, msg, svsHeader.time, NULL, newcs, qtrue );
		newindex++;
		continue;
	}

	if ( newnum > oldnum ) {
		MSG_WriteDeltaClient( &snapInfo, msg, svsHeader.time, oldcs, NULL, qtrue );
		oldindex++;
		continue;
	}
    }

    MSG_WriteBit0(msg);

    if(sv_padPackets->integer){
	for( i=0 ; i < sv_padPackets->integer ; i++){
		MSG_WriteByte( msg, 0); //svc_nop
	}
    }
}




/*
====================
SV_RateMsec

Return the number of msec a given size message is supposed
to take to clear, based on the current rate
TTimo - use sv_maxRate or sv_dl_maxRate depending on regular or downloading client
====================
*/
#define HEADER_RATE_BYTES   48      // include our header, IP header, and some overhead

static int SV_RateMsec( client_t *client, int messageSize ) {
	int rate;
	int rateMsec;
	int maxRate;

	// individual messages will never be larger than fragment size
	if ( messageSize > 1500 ) {
		messageSize = 1500;
	}
	// low watermark for sv_maxRate, never 0 < sv_maxRate < 1000 (0 is no limitation)
	if ( sv_maxRate->integer && sv_maxRate->integer < 1000 ) {
		Cvar_SetInt( sv_maxRate, 1000 );
	}
	rate = client->rate;
	maxRate = sv_maxRate->integer;

	if ( maxRate ) {
		if ( maxRate < rate ) {
			rate = maxRate;
		}
	}
	rateMsec = ( messageSize + HEADER_RATE_BYTES ) * 1000 / rate;

	return rateMsec;
}


int irand()
{

    return svs.time ^ rand();

}

void SV_TrackHuffmanCompression(int compsize, int uncompsize)
{
    static int totaluncompbytes = 0;
    static int totalcompbytes = 0;
    static int nextnotifybytes = 0;

    totaluncompbytes += uncompsize;
    totalcompbytes += compsize;

    if(totalcompbytes > nextnotifybytes)
    {
	Com_Printf("Huffman compressionrate: %.2f\n", (float)totaluncompbytes / (float)totalcompbytes);
	nextnotifybytes += 1024*16;
    }
}

/*
=======================
SV_SendMessageToClient

Called by SV_SendClientSnapshot and SV_SendClientGameState
=======================
*/
__cdecl void SV_SendMessageToClient( msg_t *msg, client_t *client ) {
	int rateMsec;

#ifdef COD4X17A
	int len;
	*(int32_t*)0x13f39080 = *(int32_t*)msg->data;
	len = MSG_WriteBitsCompress( 0, msg->data + 4 ,(byte*)0x13f39084 , msg->cursize - 4);
//	SV_TrackHuffmanCompression(len, msg->cursize - 4);
	len += 4;
#endif
	if(client->delayDropMsg){
		SV_DropClient(client, client->delayDropMsg);
	}

	if(client->demorecording && !client->demowaiting)
	{
#ifdef COD4X17A
		SV_WriteDemoMessageForClient((byte*)0x13f39080, len, client);
#else
		SV_WriteDemoMessageForClient(msg->data, msg->cursize, client);
#endif
	}

	// record information about the message
#ifdef COD4X17A
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSize = len;
#else
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSize = msg->cursize;
#endif
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageSent = Sys_Milliseconds();
	client->frames[client->netchan.outgoingSequence & PACKET_MASK].messageAcked = 0xFFFFFFFF;

	// send the datagram
#ifdef COD4X17A
	SV_Netchan_Transmit( client, (byte*)0x13f39080, len );
#else
	SV_Netchan_Transmit( client, msg->data, msg->cursize );
#endif
	// set nextSnapshotTime based on rate and requested number of updates

	// local clients get snapshots every frame
	// TTimo - show_bug.cgi?id=491
	// added sv_lanForceRate check

	if(client->state == CS_ACTIVE && client->deltaMessage >= 0 && client->netchan.outgoingSequence - client->deltaMessage > 28){

		client->nextSnapshotTime = svs.time + client->snapshotMsec * irand();

		if(client->unknown6 +1 > 8)
		{
			client->unknown6 = 8;
		}
	}

	client->unknown6 = 0;

	if ( client->netchan.remoteAddress.type == NA_LOOPBACK || Sys_IsLANAddress( &client->netchan.remoteAddress )) {
		client->nextSnapshotTime = svs.time - 1;
		return;
	}

	// normal rate / snapshotMsec calculation
	rateMsec = SV_RateMsec( client, msg->cursize );

	// TTimo - during a download, ignore the snapshotMsec
	// the update server on steroids, with this disabled and sv_fps 60, the download can reach 30 kb/s
	// on a regular server, we will still top at 20 kb/s because of sv_fps 20
	if ( !*client->downloadName && rateMsec < client->snapshotMsec ) {
		// never send more packets than this, no matter what the rate is at
		rateMsec = client->snapshotMsec;
		client->rateDelayed = qfalse;
	} else {
		client->rateDelayed = qtrue;
	}

	client->nextSnapshotTime = svs.time + rateMsec;

	// don't pile up empty snapshots while connecting
	if ( client->state != CS_ACTIVE && !*client->downloadName) {
		// a gigantic connection message may have already put the nextSnapshotTime
		// more than a second away, so don't shorten it
		// do shorten if client is downloading
		if (  client->nextSnapshotTime < svs.time + 1000 ) {
			client->nextSnapshotTime = svs.time + 1000;
		}
	}
#ifdef COD4X17A
	sv.bpsTotalBytes += len;
#else
	sv.bpsTotalBytes += msg->cursize;
#endif
}

void SV_SendClientSnapshot(client_t *cl){

	msg_t msg;

	SV_SetServerStaticHeader();

	SV_BeginClientSnapshot(cl, &msg);

	if(cl->state == CS_ACTIVE || cl->state == CS_ZOMBIE)
		SV_WriteSnapshotToClient(cl, &msg);

	SV_EndClientSnapshot(cl, &msg);

	SV_GetServerStaticHeader();
}

void SV_BeginClientSnapshot(client_t *client, msg_t *msg)
{
	static byte tempSnapshotMsgBuf[NETCHAN_UNSENTBUFFER_SIZE];
	
	
	MSG_Init( msg, tempSnapshotMsgBuf, sizeof(tempSnapshotMsgBuf) );
	MSG_ClearLastReferencedEntity( msg );
	
	MSG_WriteLong( msg, client->lastClientCommand );
	
	if ( client->state == CS_ACTIVE || client->state == CS_ZOMBIE )
		SV_UpdateServerCommandsToClient( client, msg );
}

void SV_EndClientSnapshot(client_t *client, msg_t *msg)
{

	if ( client->state != CS_ZOMBIE )
		SV_WriteDownloadToClient(client, msg);
		
	MSG_WriteByte(msg, svc_EOF);
		
	if ( msg->overflowed == qtrue)
	{		
		Com_PrintWarning( "WARNING: msg overflowed for %s, trying to recover\n", client->shortname);
		
		if ( client->state == CS_ACTIVE || client->state == CS_ZOMBIE )
		{
			SV_ShowClientUnAckCommands(client);
			
			MSG_Clear( msg );
			MSG_WriteLong(msg, client->lastClientCommand);
			
			SV_UpdateServerCommandsToClientRecover( client, msg );
			
			MSG_WriteByte(msg, svc_EOF);
						
		}
		if ( msg->overflowed == qtrue)
		{
			Com_PrintWarning("WARNING: client disconnected for msg overflow: %s\n", client->shortname);
			NET_OutOfBandPrint(NS_SERVER, &client->netchan.remoteAddress, "disconnect");
			SV_DropClient(client, "EXE_SERVERMESSAGEOVERFLOW");
		}
	}
		
	SV_SendMessageToClient(msg, client);
}

/*
 =======================
 SV_SendClientMessages
 =======================
 */

void SV_SendClientMessages( void ) {
	int i, freeBytes, index;
	msg_t msg;
	byte buf[0x20000];
	client_t *c;
	byte snapClients[MAX_CLIENTS];
	int numclients = 0; // NERVE - SMF - net debugging
	/*
	SV_SendClientMessagesA( );
	return;
	 */
	sv.bpsTotalBytes = 0; // NERVE - SMF - net debugging
	sv.ubpsTotalBytes = 0; // NERVE - SMF - net debugging
	
	// send a message to each connected client
	for ( i = 0, c = svs.clients ; i < sv_maxclients->integer ; i++, c++ ) {
		if ( !c->state || c->netchan.remoteAddress.type == NA_BOT)
		{
			snapClients[i] = 0;		
			continue; // not connected
		}
#ifndef COD4X17A		
		ReliableMessageSetCurrentTime(&c->relmsg, svs.time);
		ReliableMessagesTransmitNextFragment(&c->relmsg);
		Net_TestingFunction(&c->relmsg);
#endif
		if ( svs.time < c->nextSnapshotTime ) {
			snapClients[i] = 0;	
			continue; // not time yet
		}
		
		numclients++; // NERVE - SMF - net debugging
		
		// send additional message fragments if the last message
		// was too large to send at once
		if ( c->netchan.unsentFragments ) {
			c->nextSnapshotTime = svs.time + SV_RateMsec( c, c->netchan.unsentLength - c->netchan.unsentFragmentStart );
			SV_Netchan_TransmitNextFragment( c );
			snapClients[i] = 0;	
			continue;
		}
		
		// generate a new message
		snapClients[i] = 1;
		
		if ( c->state == CS_ACTIVE || c->state == CS_ZOMBIE )
            SV_BuildClientSnapshot( c );
		
	}
	
	SV_SetServerStaticHeader();
	
	for (i = 0, c = svs.clients; i < sv_maxclients->integer; i++, c++) {
	
		if(snapClients[i] == 0)
			continue;
		
		SV_BeginClientSnapshot( c, &msg );
		
		if(c->state == CS_ACTIVE || c->state == CS_ZOMBIE)
			SV_WriteSnapshotToClient( c, &msg );
		
		SV_EndClientSnapshot(c, &msg);
		SV_SendClientVoiceData( c );
	}
	
	// NERVE - SMF - net debugging
	if ( sv_showAverageBPS->integer && numclients > 0 ) {
		float ave = 0, uave = 0;
		
		for ( i = 0; i < MAX_BPS_WINDOW - 1; i++ ) {
			sv.bpsWindow[i] = sv.bpsWindow[i + 1];
			ave += sv.bpsWindow[i];
			
			sv.ubpsWindow[i] = sv.ubpsWindow[i + 1];
			uave += sv.ubpsWindow[i];
		}
		
		sv.bpsWindow[MAX_BPS_WINDOW - 1] = sv.bpsTotalBytes;
		ave += sv.bpsTotalBytes;
		
		sv.ubpsWindow[MAX_BPS_WINDOW - 1] = sv.ubpsTotalBytes;
		uave += sv.ubpsTotalBytes;
		
		if ( sv.bpsTotalBytes >= sv.bpsMaxBytes ) {
			sv.bpsMaxBytes = sv.bpsTotalBytes;
		}
		
		if ( sv.ubpsTotalBytes >= sv.ubpsMaxBytes ) {
			sv.ubpsMaxBytes = sv.ubpsTotalBytes;
		}
		
		sv.bpsWindowSteps++;
		
		if ( sv.bpsWindowSteps >= MAX_BPS_WINDOW ) {
			float comp_ratio;
			
			sv.bpsWindowSteps = 0;
			
			ave = ( ave / (float)MAX_BPS_WINDOW );
			uave = ( uave / (float)MAX_BPS_WINDOW );
			
			comp_ratio = ( 1 - ave / uave ) * 100.f;
			sv.ucompAve += comp_ratio;
			sv.ucompNum++;
			
			Com_DPrintf( "bpspc(%2.0f) bps(%2.0f) pk(%i) ubps(%2.0f) upk(%i) cr(%2.2f) acr(%2.2f)\n",
						ave / (float)numclients, ave, sv.bpsMaxBytes, uave, sv.ubpsMaxBytes, comp_ratio, sv.ucompAve / sv.ucompNum );
		}
	}
	// -NERVE - SMF
	
	if ( sv.state != SS_GAME )
	{
		SV_GetServerStaticHeader();
		return;
	}

	MSG_Init(&msg, buf, sizeof(buf));
	SV_ArchiveSnapshot(&msg);
	
	SV_GetServerStaticHeader();
	
	if ( msg.overflowed == qtrue )
	{
		Com_DPrintf("SV_ArchiveSnapshot: ignoring snapshot because it overflowed.\n");
		return;
	}
		
	svs.archiveSnaps[svs.nextArchivedSnapshotFrames % 1200].buffer = svs.nextArchivedSnapshotBuffer;
	svs.archiveSnaps[svs.nextArchivedSnapshotFrames % 1200].msgsize = msg.cursize;

	index = svs.nextArchivedSnapshotBuffer % 0x2000000;

	svs.nextArchivedSnapshotBuffer += msg.cursize;
	
	if ( svs.nextArchivedSnapshotBuffer >= (signed int)0x7FFFFFFE )
	{
		Com_Error(0, "svs.nextArchivedSnapshotBuffer wrapped");
		return;
	}
	freeBytes = 0x2000000 - index;
	
	if ( msg.cursize > freeBytes )
	{
		memcpy(&svs.archiveSnapBuffer[index], msg.data, freeBytes);
		memcpy(svs.archiveSnapBuffer, &msg.data[freeBytes], msg.cursize - freeBytes);

	}
	else
	{
		memcpy(&svs.archiveSnapBuffer[index], msg.data, msg.cursize);
	}
	
	svs.nextArchivedSnapshotFrames++;
	
	if (  svs.nextArchivedSnapshotFrames >= (signed int)0x7FFFFFFE  ){
		Com_Error(0, "svs.nextArchivedSnapshotFrames wrapped");
	}

}