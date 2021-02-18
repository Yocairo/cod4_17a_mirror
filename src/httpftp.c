/*
 *  httpftp.c
 *  CoD4X17a_testing
 *
 */


#include "q_shared.h"
#include "q_platform.h"
#include "httpftp.h"
#include "qcommon.h"
#include "qcommon_mem.h"
#include "qcommon_io.h"
#include "netchan.h"
#include "msg.h"
#include "sys_main.h"
#include "net_game.h"
#include "net_game_conf.h"
#include "webadmin.h"

#include <string.h>
#include <stdint.h>

#define TCP_TIMEOUT 12
#define INITIAL_BUFFERLEN 1024


/*
 ===================================================================
 Common functions required for other general File-Transfer functions
 ===================================================================
*/


static void FT_FreeRequest(ftRequest_t* request)
{
	if(request->lock == qfalse)
		return;
		
	if(request->recvmsg.data != NULL)
	{
		Z_Free(request->recvmsg.data);
		request->recvmsg.data = NULL;
	}
	if(request->sendmsg.data != NULL)
	{
		Z_Free(request->sendmsg.data);
		request->sendmsg.data = NULL;
	}
	if(request->transfermsg.data != NULL)
	{
		Z_Free(request->transfermsg.data);
		request->transfermsg.data = NULL;
	}
	if(request->socket >= 0)
	{
        NET_TcpCloseSocket(request->socket);
		request->socket = -1;
	}
	if(request->transfersocket >= 0)
	{
        NET_TcpCloseSocket(request->transfersocket);
		request->transfersocket = -1;
	}
	Z_Free(request);
}


static ftRequest_t* FT_CreateRequest(const char* address, const char* url)
{
	void* buf;
	
	ftRequest_t* request;
	
	request = Z_Malloc(sizeof(ftRequest_t));
	if(request == NULL)
		return NULL;
	
	Com_Memset(request, 0, sizeof(ftRequest_t));
	request->lock = qtrue;
	request->finallen = -1;
	request->socket = -1;
	request->transfersocket = -1;
	
	if(address != NULL)
	{
		Q_strncpyz(request->address, address, sizeof(request->address));
		/* Open the connection */
		request->socket = NET_TcpClientConnect(request->address);
	
	    if(request->socket < 0)
		{	
			request->socket = -1;
			FT_FreeRequest(request);
			return NULL;
		}
		
	}
	
	/* For proper terminating of string data +1 */
	buf = Z_Malloc(INITIAL_BUFFERLEN +1);
	if( buf == NULL)
	{
		FT_FreeRequest(request);
		return NULL;
	}
	MSG_Init(&request->recvmsg, buf, INITIAL_BUFFERLEN);
	
	buf = Z_Malloc(INITIAL_BUFFERLEN);
	if( buf == NULL)
	{
		FT_FreeRequest(request);
		return NULL;
	}
	MSG_Init(&request->sendmsg, buf, INITIAL_BUFFERLEN);
	
	if(url != NULL)
	{
		Q_strncpyz(request->url, url, sizeof(request->url));
	}
	
	request->startTime = Sys_Milliseconds();
	return request;
	
}


static void FT_ResetRequest( ftRequest_t* request )
{
	
	if(request->socket >= 0)
	{
        NET_TcpCloseSocket(request->socket);
		request->socket = -1;
	}
	if(request->transfersocket >= 0)
	{
        NET_TcpCloseSocket(request->transfersocket);
		request->transfersocket = -1;
	}
	request->lock = qtrue;
	request->finallen = -1;
	request->socket = -1;
	request->transfersocket = -1;
	request->address[0] = '\0';
	request->url[0] = '\0';
	request->username[0] = '\0';
	request->password[0] = '\0';
	request->active = qfalse;
	request->transferactive = qfalse;
	request->transferStartTime = 0;
	request->sentBytes = 0;
	request->totalreceivedbytes = 0;
	request->extrecvmsg = NULL;
	request->extsendmsg = NULL;
	request->complete = qfalse;
	request->code = 0;
	request->version = 0;
	request->status[0] = '\0';
	request->mode = 0;
	request->headerLength = 0;
	request->contentLength = 0;
	request->stage = 0;
	request->protocol = 0;
	MSG_Clear(&request->recvmsg);
	MSG_Clear(&request->sendmsg);
	MSG_Clear(&request->transfermsg);
	
	request->startTime = Sys_Milliseconds();
	
}



static void FT_AddData(ftRequest_t* request, void* data, int len)
{
	void* newbuf;
	int newsize;

	if (request->sendmsg.cursize + len > request->sendmsg.maxsize)
	{
		newsize = request->sendmsg.cursize + len;
	
		newbuf = Z_Malloc(newsize);
		if(newbuf == NULL)
		{
			MSG_WriteData(&request->sendmsg, data, len);
			return;
		}
		Com_Memcpy(newbuf, request->sendmsg.data, request->sendmsg.cursize);
		
		Z_Free(request->sendmsg.data);
		request->sendmsg.data = newbuf;
		request->sendmsg.maxsize = newsize;
	}
	MSG_WriteData(&request->sendmsg, data, len);
}


static int FT_SendData(ftRequest_t* request)
{
	int bytes;
	
	if(request->socket == -1)
		return -1;

	if(request->sendmsg.cursize < 1)
		return 0;
	
	/* Send new bytes */	
	
	bytes = NET_TcpSendData(request->socket, request->sendmsg.data, request->sendmsg.cursize);
	
	if(bytes < 0 || bytes > request->sendmsg.cursize)
	{
		return -1;
	
    }else if(bytes == 0){
		return 0;
	}
	
	request->sendmsg.cursize -= bytes;
	
	memmove(request->sendmsg.data, &request->sendmsg.data[bytes], request->sendmsg.cursize);
	return 1;
}

static int FT_ReceiveData(ftRequest_t* request)
{
	void* newbuf;
	int newsize, status, numbytes;
	
	if(request->socket == -1)
		return -1;
	
	/* In case our buffer is already too small enlarge it */
	if (request->recvmsg.cursize == request->recvmsg.maxsize)
	{
		newsize = 2 * request->recvmsg.maxsize;
		if(request->finallen > 0 && newsize > request->finallen)
		{
			newsize = request->finallen;			
		}
	}else 
		newsize = 0;
	
	if (newsize)
	{
		/* For proper terminating of string data +1 */
		newbuf = Z_Malloc(newsize +1);
		if(newbuf == NULL)
		{
			return -1;
		}
		Com_Memcpy(newbuf, request->recvmsg.data, request->recvmsg.cursize);
		
		Z_Free(request->recvmsg.data);
		request->recvmsg.data = newbuf;
		request->recvmsg.maxsize = newsize;
	}
	numbytes = request->recvmsg.cursize;
	/* Receive new bytes */
	status = NET_TcpReceiveData(request->socket, &request->recvmsg);
	
	
	numbytes = request->recvmsg.cursize - numbytes;
	request->totalreceivedbytes += numbytes;
	
	if (status == 1){
		return 0;
		
	}else if (status == -1){
		
		request->socket = -1;
		return -1;
		
	}else if(status == -2){
		request->socket = -1;			
	}
	
	/* Proper terminating of string data */
	request->recvmsg.data[request->recvmsg.cursize] = '\0';
	return 1;
}


/*
 ========================================================
 Functions for retriving a file located on a HTTP-Server
 ========================================================
*/
static void HTTP_EncodeChar(unsigned char chr, unsigned char* encodedchr)
{
	sprintf((char*)encodedchr, "%%%X", (char)chr);
}

static void HTTP_EncodeURL(char* inurl, char* outencodedurl, int len)
{
	int i, y;
	unsigned char* url = (unsigned char*)inurl;
	unsigned char* encodedurl = (unsigned char*)outencodedurl;
	
	for(i = 0, y = 0; y < len -4 && url[i]; i++)
	{
		switch(url[i])
		{
			case '<':
			case '>':
			case ' ':
			case '"':
			case '#':
			case '%':			
			case '{':
			case '}':
			case '|':
			case '\\':
			case '^':
			case '~':
			case '[':
			case ']':
			case '`':
				
				HTTP_EncodeChar(url[i], &encodedurl[y]);
				y += 3;
				break;
				
			default:
				
				if(url[i] > 0x7f || url[i] < 0x20)
				{
					HTTP_EncodeChar(url[i], &encodedurl[y]);
					y += 3;
				}else{
					encodedurl[y] = url[i];
					++y;
				}
				break;
		}
	}
	encodedurl[y] = '\0';
}

void HTTP_DecodeURL(char* url)
{
	int i, y;
	char parse[3];
	
	parse[2] = '\0';
	
	for (i = 0, y = 0; url[i] != '\0'; ++i, ++y)
	{
		if(url[i] == '%')
		{
			parse[0] = url[i+1];
			parse[1] = url[i+2];
			url[y] = strtol(parse, NULL, 16);
			y += 2;
		}else{
			url[y] = url[i];
		}
	}
}


void HTTP_DecodeURLFormData(char* url)
{	
	Q_strchrrepl(url, '+', ' ');
	
	HTTP_DecodeURL(url);
}


void HTTP_BuildNewRequest( ftRequest_t* request, const char* method, msg_t* msg, const char* additionalheaderlines)
{
	char getbuffer[MAX_STRING_CHARS];
	char encodedUrl[MAX_STRING_CHARS];
	char address[MAX_STRING_CHARS];
	char *port;
	char extheaderfields[512];
	
	request->protocol = FT_PROTO_HTTP;
	request->active = qtrue;
	request->finallen = -1;
	request->version = 0;
	request->code = 0;
	request->status[0] = '\0';
	request->contentLength = '\0';
	request->headerLength = 0;
	request->transferactive = qfalse;
	request->totalreceivedbytes = 0;
	
	MSG_Clear(&request->sendmsg);
	MSG_Clear(&request->recvmsg);
	
	Q_strncpyz(address, request->address, sizeof(address));
	port = strstr(address, ":80");
	if(port != NULL)
	{
		*port = '\0';
	}
	HTTP_EncodeURL(request->url, encodedUrl, sizeof(encodedUrl));
	
	extheaderfields[0] = '\0';
	
	if(msg != NULL)
	{
		Com_sprintf(extheaderfields, sizeof(extheaderfields), "Content-Length: %d\r\n", msg->cursize);
		if(additionalheaderlines && additionalheaderlines[0])
		{
			Q_strcat(extheaderfields, sizeof(extheaderfields), additionalheaderlines);
		}
		
	}
	Com_sprintf(getbuffer, sizeof(getbuffer),
				"%s %s HTTP/1.1\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n"
				"User-Agent: CoD4X HTTP-Agent\r\n"
				"Connection: Keep-Alive\r\n"
				"%s"
				"\r\n", method, encodedUrl, address, extheaderfields);
	
	FT_AddData(request, getbuffer, strlen(getbuffer));
	if(msg != NULL)
	{
		FT_AddData(request, msg->data, msg->cursize);
	}
}



static void HTTPSplitURL(const char* url, char* address, int lenaddress, char* wwwpath, int lenwwwpath)
{
	char* charloc;
	
	/* Strip away leading spaces */
	while(*url == ' ')
		url++;
	
	if(!Q_stricmpn(url, "http://", 7))
	{
		url += 7;
	}
	
	Q_strncpyz(address, url, lenaddress);
	Q_strncpyz(wwwpath, url, lenwwwpath);
	
	charloc = strchr(address, '/');
	if(charloc)
	{
		charloc[0] = '\0';
	}
	
	if (strchr(address, ':') == NULL) {
		Q_strcat(address, lenaddress, ":80");
	}
	
	charloc = strchr(wwwpath, '/');
	if(charloc && charloc != wwwpath)
	{
		Q_bstrcpy(wwwpath, charloc);
	}else{
		wwwpath[0] = '/';
		wwwpath[1] = '\0';
	}
}	


int HTTP_SendReceiveData(ftRequest_t* request)
{
	char* line;
	int status, i;
	qboolean gotheader;
	char stringlinebuf[MAX_STRING_CHARS];
	
	if (request->sendmsg.cursize > 0) {
		status = FT_SendData(request);
		
		if(status < 0)
			return -1;
		return 0;
	}

	status = FT_ReceiveData(request);
	
	if (status == -1) {
		return -1;
	}else if (status == 0) {
		return 0;
	}
	
	if(request->finallen == -1)
	{
		gotheader = qfalse;
		/* 1st check if the header is complete */
		while ((line = MSG_ReadStringLine(&request->recvmsg, stringlinebuf, sizeof(stringlinebuf))) && line[0] != '\0' )
		{
			if(line[0] == '\r')
			{
				gotheader = qtrue;
				break;
			}
		}
		if(gotheader == qfalse)
		{
			return 0;
		}
		MSG_BeginReading(&request->recvmsg);
		
		line = MSG_ReadStringLine(&request->recvmsg, stringlinebuf, sizeof(stringlinebuf));
		if(Q_stricmpn(line,"HTTP/1.",7) || isInteger(line + 7, 2) == qfalse || isInteger(line + 9, 4) == qfalse)
		{
			Com_PrintError("HTTP_ReceiveData: Packet is corrupt!\nDebug: %s\n", line);
			
			return -1;
		}
		
		request->version = atoi(line + 7);
		if (request->version < 0 || request->version > 9)
		{
			Com_PrintError("HTTP_ReceiveData: Packet has unknown HTTP version 1.%d !\n", request->version);
			
			return -1;			
		}
		
		request->code = atoi(line + 9);
		i = 0;
		while (line[i +9] != ' ' && line[i +9] != '\0')
		{
			i++;
		}
		
		if(line[i +9] != '\0')
		{
			Q_strncpyz(request->status, &line[i +9], sizeof(request->status));
		}else{
			Q_strncpyz(request->status, "N/A", sizeof(request->status));
		}
		
		request->contentLength = 0;
		
		while ((line = MSG_ReadStringLine(&request->recvmsg, stringlinebuf, sizeof(stringlinebuf))) && line[0] != '\0' && line[0] != '\r')
		{
			if(!Q_stricmpn("Content-Length:", line, 15))
			{
				if(isInteger(line + 15, 0) == qfalse)
				{
					Com_PrintError("Sec_GetHTTPPacket: Packet is corrupt!\nDebug: %s\n", line);
					return -1;
				}
				request->contentLength = atoi(line + 15);
				if(request->contentLength < 0)
				{
					request->contentLength = 0;
					return -1;
				}
			}
			else if(!Q_stricmpn("Location:", line, 9))
			{
				if(strlen(line +9) > 8)
				{
					/* We have to make it new... */
					FT_ResetRequest( request );
					/* Remove "Location:" */
					Q_bstrcpy(line, line +9);
					/* Remove trailing \r */
					line[strlen(line) -1] = '\0';
					
					HTTPSplitURL(line, request->address, sizeof(request->address), request->url, sizeof(request->url));
					
					if(strlen(request->address) < 2)
						return -1;
					
					Com_Printf("Received redirect request to http://%s%s\n", request->address, request->url);
					
					request->socket = NET_TcpClientConnect(request->address);
					
					if(request->socket < 0)
					{	
						request->socket = -1;
						return -1;
					}
					HTTP_BuildNewRequest( request, "GET", NULL, NULL);
					return 0;
				}
				
			}
			
		}
		if(line[0] == '\0')
			return -1;
		
		request->headerLength = request->recvmsg.readcount;		
		request->finallen = request->contentLength + request->headerLength;

		if(request->finallen > 1024*1024*640)
		{
			request->finallen = request->headerLength;
		}

	}
	/* Header was complete */
	if( request->finallen > 0)
		request->transferactive = qtrue;
	
	request->extrecvmsg = &request->recvmsg;
	
	if (request->totalreceivedbytes < request->finallen) {
		/* Still needing bytes... */
		return 0;
	}
	/* Received full message */
	return 1;
	
}


ftRequest_t* HTTPRequest(const char* url, const char* method, msg_t* msg, const char* addheaderlines)
{
	char address[MAX_OSPATH];
	char wwwpath[MAX_OSPATH];
	
	ftRequest_t* request;
	
	HTTPSplitURL(url, address, sizeof(address), wwwpath, sizeof(wwwpath));
	
	Com_DPrintf("HTTPRequest: Open URL: http://%s\n", url);
	
	if(strlen(address) < 2)
		return NULL;
	
	request = FT_CreateRequest(address, wwwpath);
	
	if(request == NULL)
		return NULL;
	
	HTTP_BuildNewRequest( request, method, msg, addheaderlines);
	
	return request;
	
}

/*
 ========================================================
 Functions for retriving a file located on an FTP-Server
========================================================
*/

static void FTP_SplitURL(const char* url, char* address, int lenaddress, char* path, int lenpath,
						 char* loginname, int loginnamelen, char* password, int passwdlen)
{
	char* charloc;
	
	char parse[MAX_STRING_CHARS];
	
	path[0] = '\0';
	address[0] = '\0';
	password[0] = '\0';
	loginname[0] = '\0';
	
	/* Strip away leading spaces */
	while(*url == ' ')
		url++;
	
	if(!Q_stricmpn(url, "ftp://", 6))
	{
		url += 6;
	}
	
	
	Q_strncpyz(parse, url, sizeof(parse));
	
	/* Find the filepath */
	charloc = strchr(parse, '/');
	if(charloc)
	{
		charloc[0] = '\0';
		Q_strncpyz(path, &charloc[1], lenpath);
	}
	/* Find the address */
	charloc = strchr(parse, '@');	
	if(charloc)
	{
		charloc[0] = '\0';
		Q_strncpyz(address, &charloc[1], lenaddress);
	}else{
		/* No @ char here so I expect the address comes without username:password */
		Q_strncpyz(address, parse, lenaddress);
	}	
	/* See if we have a port for our address given. If not complete this. */
	if (strchr(address, ':') == NULL) {
		Q_strcat(address, lenaddress, ":21");
	}
	/* In case we had no login we are done */
	if(charloc == NULL)
	{
		return;
	}
	/* Find the password */
	charloc = strchr(parse, ':');
	if(charloc)
	{
		charloc[0] = '\0';
		Q_strncpyz(password, &charloc[1], passwdlen);
	}
	/* Copy what is over to the username */
	Q_strncpyz(loginname, parse, loginnamelen);
	
}



static ftRequest_t* FTP_DLRequest(const char* url)
{
	char address[MAX_OSPATH];
	char ftppath[MAX_OSPATH];
	char user[MAX_OSPATH];
	char passwd[MAX_OSPATH];
	
	ftRequest_t* request;
	
	FTP_SplitURL( url, address, sizeof(address), ftppath, sizeof(ftppath), user, sizeof(user), passwd, sizeof(passwd));
	
	if(strlen(address) < 2)
		return NULL;
	//Com_Printf("Connecting to: %s path: %s user: %s passwd: %s\n", address, ftppath, user, passwd);
	request = FT_CreateRequest(address, ftppath);
	
	if(request == NULL)
		return NULL;
	
	if(user[0] == '\0' && passwd[0] == '\0')
	{
		Q_strncpyz(request->username, "anonymous", sizeof(request->username));
		Q_strncpyz(request->password, "cod4x@iceops.in", sizeof(request->password));
	}else{
		Q_strncpyz(request->username, user, sizeof(request->username));
		Q_strncpyz(request->password, passwd, sizeof(request->password));	
	}
	
	request->protocol = FT_PROTO_FTP;
	request->active = qtrue;
	request->stage = 0;
	return request;	
}

static void FTP_GetPassiveAddress(const char* line, netadr_t* adr)
{
	int i, j, port;
	int numbers[24];
		
	Com_Memset(adr, 0, sizeof(netadr_t));
	
	i = 0;
	
	/* Find the start of address */
	while(line[i] != '(' && line[i] != '\0')
		i++;
	
	if(line[i] != '(')
		return;
	i++;
	
	for(j = 0; j < sizeof(numbers); j++)
	{
		numbers[j] = atoi(&line[i]);

		while(line[i] != ',' && line[i] != ')' && line[i] != '\0')
			i++;
		
		if(line[i] == ',')
		{
			/* A number delimiter */
			i++;
		}else if(line[i] == ')' && j >= 5){
			/* End of address */
			adr->type = NA_IP;
			adr->ip[0] = numbers[0];
			adr->ip[1] = numbers[1];
			adr->ip[2] = numbers[2];
			adr->ip[3] = numbers[3];
			port = 256 * numbers[4] + numbers[5];
			adr->port = BigShort(port);
			return;
		}else{
			/* Something is invalid */
			return;
		}

	}	
	
}

static void FTP_GetExtendedPassiveAddress(const char* line, const char* address, netadr_t* adr)
{
	int i, j, port;
	
	Com_Memset(adr, 0, sizeof(netadr_t));
	
	i = 0;
	/* Remove the port number */

	if(NET_StringToAdr(address, adr, NA_UNSPEC) == 0)
	{	/* No valid address string. Must not happen.  */
		return;
	}
	
	/* Find the start of address */
	while(line[i] != '(' && line[i] != '\0')
		i++;
	
	if(line[i] != '(')
	{
		Com_Memset(adr,0, sizeof(netadr_t));
		return;
	}
	
	i++;
	
	for(j = 0; j < 3; j++)
	{
		while(line[i] != '|' && line[i] != ')' && line[i] != '\0')
			i++;
		
		if(line[i] == '|')
		{
			/* A delimiter */
			i++;
		}

	}	

	port = atoi(&line[i]);	
	
	if(port < 1 || port > 65535)
	{
		/* Something is invalid */
		Com_Memset(adr, 0, sizeof(netadr_t));
		return;
	}
	
	while(line[i] != '|' && line[i] != '\0')
		i++;
		
	if(line[i] == '|')
	{
		/* A delimiter */
		i++;
	}else {
		/* Something is invalid */
		Com_Memset(adr, 0, sizeof(netadr_t));
		return;
	}
	
	while(line[i] != ')' && line[i] != '\0')
		i++;	
	
	if(line[i] == ')')
	{
		adr->port = BigShort(port);
	}else {
		/* Something is invalid */
		Com_Memset(adr, 0, sizeof(netadr_t));
	}
	

	
}


static int FTP_ReceiveData(ftRequest_t* request)
{
	byte* newbuf;
	int newsize, numbytes, status;
	
	newsize = 0;
	
	/* In case our buffer is already too small enlarge it */
	if (request->transfermsg.cursize == request->transfermsg.maxsize)
	{
		newsize = 2 * request->transfermsg.maxsize;
		if(request->finallen > 0 && newsize > request->finallen)
		{
			newsize = request->finallen;			
		}
		
	}
	
	if (newsize)
	{
		newbuf = Z_Malloc(newsize +1);
		if(newbuf == NULL)
		{
			return -1;
		}
		Com_Memcpy(newbuf, request->transfermsg.data, request->transfermsg.cursize);
		
		Z_Free(request->transfermsg.data);
		request->transfermsg.data = newbuf;
		request->transfermsg.maxsize = newsize;
	}
	
	numbytes = request->transfermsg.cursize;
	/* Receive new bytes */
	status = NET_TcpReceiveData(request->transfersocket, &request->transfermsg);
	
	numbytes = request->transfermsg.cursize - numbytes;
	/* Track and update the total received bytes */
	request->transfertotalreceivedbytes += numbytes;
	
	if (status == -1 || status == -2){
		request->transfersocket = -1;
		
	}
	return status;
	
}



static int FTP_SendReceiveData(ftRequest_t* request)
{
	char* line;
	char command[MAX_STRING_CHARS];
	int status, bytes;
	byte* buf;
	netadr_t pasvadr;
	char stringlinebuf[MAX_STRING_CHARS];
	
	status = FT_ReceiveData(request);
	
	if (status == -1 && request->stage < 9999) {
		return -1;
	}
	if(status == 0 && request->stage < 9999)
	{
		status = FT_SendData(request);
	}
	if (status == -1 && request->stage < 9999) {
		return -1;
	}
	
	while ((line = MSG_ReadStringLine(&request->recvmsg, stringlinebuf, sizeof(stringlinebuf))) && line[0] != '\0' )
	{	
		if(isNumeric(line, 0) == qfalse)
			continue;
		
		request->code = atoi(line);
		
		if(request->stage > 60)
			Com_DPrintf("\n");
		
		Com_DPrintf("Response Code = %d\n", request->code);
		
		if (request->stage < 0)
		{
			continue;
		}
		
		switch (request->code)
		{
				
			case 220:
				if(request->stage == 0)
				{
					// Initial OK response received /
					Com_DPrintf("FTP_SendReceiveData: Inital OK response received\n");
					request->stage = 1;
				}else {
					Com_PrintWarning("\nFTP_SendReceiveData: Received: %s - Should not happen!\n", line);
				}
				break;
			case 202:
				if(request->stage < 21 && request->stage > 10)
				{
					request->stage = 21;
				}else {
					Com_Printf("\nFTP_SendReceiveData: Received unexpected response: %s - Will abort!\n", line);
					request->stage = -20;
				}
				break;								
			case 331:
				if(request->stage == 10)
				{
					Com_DPrintf("FTP_SendReceiveData: Need Password\n");
					request->stage = 11;
					
				}else if(request->stage == 16){
					Com_DPrintf("FTP_SendReceiveData: Need Password\n");
					request->stage = 17;
				}else {
					Com_Printf("FTP_SendReceiveData: Received unexpected response: %s - Will abort!\n", line);
					request->stage = -20;
				}
				break;
			case 332:
				if(request->stage == 10 || request->stage == 12)
				{
					Com_DPrintf("FTP_SendReceiveData: Need Account\n");
					request->stage = 15;
				}else {
					Com_Printf("\nFTP_SendReceiveData: Received unexpected response: %s - Will abort!\n", line);
					request->stage = -20;
				}
				break;				
			case 230:
				if (request->stage <= 20)
				{
					Com_DPrintf("FTP_SendReceiveData: Logged in OK\n");
					request->stage = 21;
				}else {
					Com_Printf("\nFTP_SendReceiveData: Received unexpected response: %s - Will abort!\n", line);
					request->stage = -20;
				}
				break;
			case 227:
				if(request->stage == 36)
				{
					FTP_GetPassiveAddress(line, &pasvadr);
					if(pasvadr.type == NA_IP)
					{
						Com_DPrintf("FTP_SendReceiveData: Entering Passive Mode at %s OK\n", NET_AdrToString(&pasvadr));
						request->transfersocket = NET_TcpClientConnect(NET_AdrToString(&pasvadr));
						if(request->transfersocket < 0)
						{	
							request->transfersocket = -1;
							return -1;
						}
						request->stage = 41;
					}else {
						Com_PrintWarning("FTP_SendReceiveData: Couldn't read the address/port of passive mode response\n");
						return -1;
					}
					
				}else {
					Com_Printf("\nFTP_SendReceiveData: Received unexpected response: %s - Will abort!\n", line);
					request->stage = -20;
				}
				break;
			case 229:
				if(request->stage == 32)
				{
					FTP_GetExtendedPassiveAddress(line, request->address, &pasvadr);
					if(pasvadr.type == NA_IP || pasvadr.type == NA_IP6)
					{
						Com_DPrintf("FTP_SendReceiveData: Entering Extended Passive Mode at %s OK\n", NET_AdrToString(&pasvadr));
						request->transfersocket = NET_TcpClientConnect(NET_AdrToString(&pasvadr));
						if(request->transfersocket < 0)
						{	
							request->transfersocket = -1;
							return -1;
						}
						request->stage = 41;
					}else {
						Com_PrintWarning("FTP_SendReceiveData: Couldn't read the address/port of passive mode response\n");
						return -1;
					}
					
				}else {
					Com_Printf("\nFTP_SendReceiveData: Received unexpected response: %s - Will abort!\n", line);
					request->stage = -20;
				}
				break;			
			case 213:
				if(request->stage == 50)
				{	
					bytes = atoi(&line[4]);
					Com_DPrintf("FTP_SendReceiveData: Requested file will have %d bytes\n", bytes);
					if(bytes < 1 || bytes > 1024*1024*1024)
					{
						Com_PrintWarning("FTP_SendReceiveData: Requested file exceeds %d bytes.\n", 1024*1024*1024);
						
					}
					request->finallen = bytes;
					request->contentLength = bytes;
					request->headerLength = 0;
					request->transfertotalreceivedbytes = 0;
					
					buf = Z_Malloc(INITIAL_BUFFERLEN +1);
					if( buf == NULL)
					{
						Com_PrintWarning("FTP_SendReceiveData: Failed to allocate %d bytes for download file!\n", bytes);
						return -1;
					}
					MSG_Init(&request->transfermsg, buf, INITIAL_BUFFERLEN);
					request->extrecvmsg = &request->transfermsg;
					request->stage = 51;
					
				}
				break;
			case 150:
			case 125:
				if (request->stage == 60) {
					request->stage = 61;
					Com_DPrintf("FTP_SendReceiveData: Begin File Transfer\n");
				}
				break;
			case 226:
				/* File transfer is completed from the servers view */
				if(request->stage < 9999) 
					request->stage = 9999;
				break;
			case 221:
				if(request->stage < 10000)
				{
					Com_Printf("\nThe FTP server closed the control connection before the transfer was completed!\n");
					request->stage = -1;
				}
				break;
			case 228:
				Com_Printf( "\nLong Passive Mode not supported and not requested!\n" );
				request->stage = -20;
				break;
				
			case 120:
				Com_Printf( "The FTP server is not ready at the moment!\n" );
				request->stage = -20;
				break;
			case 231:
				if(request->stage < 10000)
				{
					Com_Printf("\nThe FTP server logged us out before the transfer was completed!\n");
					request->stage = -20;
				}
				break;
			case 350:
				if(request->stage < 10000)
				{
					Com_Printf("\nThe FTP server returned \'%s\' before the transfer was completed. Must not happen!\n", line);
					request->stage = -20;
				}
				break;
			case 421:
				request->stage = -1;
				break;
			case 500:
			case 501:
			case 502:
			case 503:
				if (request->stage == 32) {
					request->stage = 35;
					Com_DPrintf("FTP_SendReceiveData: Command EPSV is not implemented on FTP server. Trying PASV...\n");
					break;
				}else if (request->stage == 36) {
					Com_Printf("FTP_SendReceiveData: FTP Server does not support passive mode. Request failed!\n");
					request->stage = -10;
				} 
			default:
				if (request->code >= 200 && request->code < 300 && request->stage >= 30){
					Com_DPrintf("\n");
					Com_DPrintf("FTP_SendReceiveData: %s\n", line);					
					request->stage ++;
					break;
				}else if (request->code >= 400) {
					Com_Printf("\nThe FTP server connection got ended with the message: %s\n", line);
					request->stage = -20;
				}
				break;
		}
	}
	
	
	switch(request->stage)
	{
		case 1:
			/* Waiting for OK response code (220) */
			Com_sprintf(command, sizeof(command), "USER %s\r\n", request->username);
			FT_AddData(request, command, strlen(command));
			request->stage = 10;
			break;
		case 11:
			Com_sprintf(command, sizeof(command), "PASS %s\r\n", request->password);
			FT_AddData(request, command, strlen(command));
			request->stage = 12;
			break;
		case 15:
			Com_sprintf(command, sizeof(command), "ACCT %s\r\n", "noaccount");
			FT_AddData(request, command, strlen(command));
			request->stage = 16;
			break;
		case 17:
			Com_sprintf(command, sizeof(command), "PASS %s\r\n", request->password);
			FT_AddData(request, command, strlen(command));
			request->stage = 20;
			break;			
		case 21:
			Com_sprintf(command, sizeof(command), "TYPE I\r\n");
			FT_AddData(request, command, strlen(command));
			request->stage = 30;
			break;
		case 31:
			Com_sprintf(command, sizeof(command), "EPSV\r\n");
			FT_AddData(request, command, strlen(command));
			request->stage = 32;
			break;
		case 35:
			Com_sprintf(command, sizeof(command), "PASV\r\n");
			FT_AddData(request, command, strlen(command));
			request->stage = 36;
			break;
		case 41:
			Com_sprintf(command, sizeof(command), "SIZE %s\r\n", request->url);
			FT_AddData(request, command, strlen(command));
			request->stage = 50;
			break;
		case 51:
			Com_sprintf(command, sizeof(command), "RETR %s\r\n", request->url);
			FT_AddData(request, command, strlen(command));
			request->stage = 60;
			break;			
			
		case 61:
			
			request->transferactive = qtrue;
			
			FTP_ReceiveData( request );
			
			if(request->transfertotalreceivedbytes == request->finallen && request->finallen != 0)
			{
				/* Complete file retrived */
				request->stage = 9999;
				break;
			}else if(request->transfersocket == -1){
				request->stage = -20;
			}
			break;
			
		case 9999:
			
			status = -1;
			request->transferactive = qfalse;
			
			if(request->transfersocket >= 0)
			{
				status = FTP_ReceiveData( request );
			}
			if (status == -1)
			{
				request->transfersocket = -1;
			}
			if(request->transfertotalreceivedbytes == request->finallen)
			{
				/* Comple file retrived */
				Com_sprintf(command, sizeof(command), "QUIT\r\n");
				FT_AddData(request, command, strlen(command));
				request->stage = 10000;
				break;
			}else {
				Com_Printf("\nThe FTP server closed the data connection before the transfer was completed!\n");
				Com_Printf("Expected %d bytes but got %d bytes\n", request->finallen, request->transfertotalreceivedbytes);
				request->stage = -20;
				break;
			}
		case -20:
			request->transferactive = qfalse;
			Com_Printf("\nFTP File Transfer has failed!\n");
		case -10:
			request->transferactive = qfalse;
			if(request->socket >= 0 )
			{
				Com_sprintf(command, sizeof(command), "QUIT\r\n", request->url);
				FT_AddData(request, command, strlen(command));
			}
			request->stage = -1;
			break;
			
		case -1:
			Com_Printf("\n");
			return -1;
			break;
			
		case 10000:
			request->transferactive = qfalse;
			
			if(request->socket >= 0)
			{
				NET_TcpCloseSocket(request->socket);
				request->socket = -1;
			}
			if(request->transfersocket >= 0)
			{
				NET_TcpCloseSocket(request->transfersocket);
				request->transfersocket = -1;
			}
			if(request->recvmsg.data != NULL)
			{
				Z_Free(request->recvmsg.data);
				request->recvmsg.data = NULL;
			}
			if(request->transfermsg.data == NULL)
			{
				request->stage = -1;
				Com_PrintError("\nReceived complete message but message buffer is NULL!\n");
				break;
			}
			request->stage = 10001;
			request->code = 200;
			Q_strncpyz(request->status, "OK", sizeof(request->status));
			break;
		case 10001:
			Com_Printf("\n");
			return 1;
		default:
			return 0;
	}
	return 0;
	
}

/*
 =====================================================================
 HTTP-Server
 =====================================================================
 */

int HTTPServer_ReadMessage(netadr_t* from, msg_t* msg, ftRequest_t* request)
{
	
	char stringlinebuf[MAX_STRING_CHARS];
	byte* newbuf;
	char* line;
	int newsize, i;
	qboolean gotheader;
	
	if(request->remote.type == 0)
	{
		
		Com_Memcpy(&request->remote, from, sizeof(request->remote));
		
	}
	
	if (request->recvmsg.maxsize - request->recvmsg.cursize < msg->cursize) 
	{
		if(request->finallen != -1)
		{
			newsize = request->finallen;
		}else {
			newsize = 2 * request->recvmsg.maxsize + msg->cursize;
		}
		
		newbuf = Z_Malloc(newsize);
		if(newbuf == NULL)
		{
			return -1;
		}
		
		Com_Memcpy(newbuf, request->recvmsg.data, request->recvmsg.cursize);
		
		Z_Free(request->recvmsg.data);
		request->recvmsg.data = newbuf;
		request->recvmsg.maxsize = newsize;
		
	}

	Com_Memcpy(&request->recvmsg.data[request->recvmsg.cursize], msg->data, msg->cursize);

	request->recvmsg.cursize += msg->cursize;

	/* Is header complete ? */
	if(request->finallen == -1)
	{
		gotheader = qfalse;
		/* 1st check if the header is complete */
		while ((line = MSG_ReadStringLine(&request->recvmsg, stringlinebuf, sizeof(stringlinebuf))) && line[0] != '\0' )
		{
			if(line[0] == '\r')
			{
				gotheader = qtrue;
				break;
			}
		}
		if(gotheader == qfalse)
		{
			return 0;
		}
		MSG_BeginReading(&request->recvmsg);
		
		line = MSG_ReadStringLine(&request->recvmsg, stringlinebuf, sizeof(stringlinebuf));
		if (!Q_strncmp(line, "HEAD", 4)) {
			request->mode = HTTP_HEAD;
			line += 5;
		}else if (!Q_strncmp(line, "POST", 4)) {
			request->mode = HTTP_POST;
			line += 5;
		}else if (!Q_strncmp(line, "GET", 3)) {
			request->mode = HTTP_GET;
			line += 4;
		}else {
			Com_DPrintf("Invalid HTTP method from %s\n", NET_AdrToString(from));
			return -1;
		}
		
		i = 0;
		while (*line != ' ' && *line != '\r' && *line)
		{
			request->url[i] = *line;
			i++;
			line++;
		}
		
		if(*line == ' ')
		{
			line++;
		}
		
		if(Q_stricmpn(line,"HTTP/1.",7) || isInteger(line + 7, 2) == qfalse || isInteger(line + 9, 4) == qfalse)
		{
			Com_PrintError("HTTP_ReceiveData: Packet is corrupt!\nDebug: %s\n", line);
			
			return -1;
		}
		
		request->version = atoi(line + 7);
		if (request->version != 0 && request->version != 1)
		{
			Com_PrintError("HTTP_ReceiveData: Packet has unknown HTTP version 1.%d !\n", request->version);
			
			return -1;			
		}
		
		
		request->contentLength = 0;
		
		while ((line = MSG_ReadStringLine(&request->recvmsg, stringlinebuf, sizeof(stringlinebuf))) && line[0] != '\0' && line[0] != '\r')
		{
			if(!Q_stricmpn("Content-Length:", line, 15))
			{
				if(isInteger(line + 15, 0) == qfalse)
				{
					Com_PrintError("Sec_GetHTTPPacket: Packet is corrupt!\nDebug: %s\n", line);
					return -1;
				}
				request->contentLength = atoi(line + 15);
				if(request->contentLength < 0)
				{
					request->contentLength = 0;
					return -1;
				}
			}
			else if(!Q_stricmpn("Content-Type:", line, 13))
			{

				if(line[13] == ' ')
				{
					Q_strncpyz(request->contentType, &line[14], sizeof(request->contentType));
				}else{
					Q_strncpyz(request->contentType, &line[13], sizeof(request->contentType));
				}

			}
			else if(!Q_stricmpn("Cookie:", line, 7))
			{
				
				if(line[7] == ' ')
				{
					Q_strncpyz(request->cookie, &line[8], sizeof(request->cookie));
				}else{
					Q_strncpyz(request->cookie, &line[7], sizeof(request->cookie));
				}
				
			}				
		}
		if(line[0] == '\0')
			return -1;
		
		request->headerLength = request->recvmsg.readcount;		
		request->finallen = request->contentLength + request->headerLength;
		
		if(request->finallen > 1024*1024*640)
		{
			request->finallen = request->headerLength;
		}
		
	}
	if(request->recvmsg.maxsize == request->finallen)
	request->transferactive = qtrue;


	if (request->recvmsg.cursize < request->finallen) {
		/* Still needing bytes... */
		return 0;
	}
//	Com_Printf("^6HTTP-Message is complete!\n");
//	Com_Printf("^6Received: Version: HTTP/1.%d   Method: %d   Content-Length: %d   URL: %s\n", request->version, request->mode, request->contentLength, request->url);
	return 1;
}

void HTTPServer_BuildMessage( ftRequest_t* request, char* status, char* message, int len, char* sessionkey)
{
	int headerlen;
	byte* newbuf;
	
	char header[MAX_STRING_CHARS];
	headerlen = Com_sprintf(header, sizeof(header),
					  "HTTP/1.0 %s\r\n"
					  "Connection: close\r\n"
					  "Content-Length: %d\r\n"
					  "Access-Control-Allow-Origin: none\r\n"
					  "Content-Type: text/html\r\n"
					  "Set-Cookie: SessionId=%s; Path=/; HttpOnly\r\n"
					  "\r\n", status, len, sessionkey);
	
	
	newbuf = Z_Malloc(headerlen + len);
	if(newbuf == NULL)
	{	
		return;
	}
	if(request->sendmsg.data)
	{
		Z_Free(request->sendmsg.data);
	}
	request->sendmsg.data = newbuf;
	request->sendmsg.maxsize = headerlen + len;
	
	MSG_WriteData(&request->sendmsg, header, headerlen);
	MSG_WriteData(&request->sendmsg, message, len);
}


qboolean HTTPServer_WriteMessage( ftRequest_t* request, netadr_t* from)
{
	int bytes;
	
	bytes = NET_TcpSendData(from->sock, request->sendmsg.data + request->sentBytes, request->sendmsg.cursize - request->sentBytes);
	
	if(bytes < 0 || bytes > (request->sendmsg.cursize - request->sentBytes))
	{
		return qtrue;
	}
	request->sentBytes += bytes;
	return qfalse;
}





void HTTPServer_BuildResponse(ftRequest_t* request, char* sessionkey, httpPostVals_t* values)
{
	qboolean hasmessage;
	msg_t msg;
	
	hasmessage = HTTPCreateWebadminMessage(request, &msg, sessionkey, values);
	if(hasmessage)
	{
		HTTPServer_BuildMessage( request, "200 OK", (char*)msg.data, msg.cursize, sessionkey);
		Z_Free(msg.data);
		return;
	}
	HTTPServer_BuildMessage( request, "403 FORBIDDEN", "Error: Forbidden", strlen("Error: Forbidden"), sessionkey);
}



void HTTPServer_ParseBody(ftRequest_t* request, httpPostVals_t* values)
{
	int i, k;
	
	k = 0;
	char* body = (char*)(request->recvmsg.data + request->headerLength);
	
	for(k = 0; k < MAX_POST_VALS; k++)
	{
		values[k].name[0] = '\0';
		values[k].value[0] = '\0';
	
		i = 0;
		
		while (body[i] != '=' && body[i] != '\0')
		{
			i++;
		}
		if(body[i] == '\0')
		{
			break;
		}
		body[i] = '\0';
		
		Q_strncpyz(values[k].name, body, sizeof(values[k].name));
		
		body += i+1; 
		i = 0;
		
		while (body[i] != '&' && body[i] != '\0')
		{
			i++;
		}
		if(body[i] == '\0')
		{
			Q_strncpyz(values[k].value, body, sizeof(values[k].value));
			break;
		}
		body[i] = '\0';
		
		Q_strncpyz(values[k].value, body, sizeof(values[k].value));

		body += i+1; 
	
	}

	
}

void HTTPServer_ReadSessionId(ftRequest_t* request, char* sessionkey, int len)
{
	char* cookieval;
	int i;

	
	cookieval = strstr(request->cookie, "SessionId=" );
	if(cookieval == NULL)
	{
		sessionkey[0] = '\0';
		return;
	}
	cookieval += 10;
	i = 0;
	
	while (cookieval[i] != '\0' && cookieval[i] != ';' && cookieval[i] != ' ') {
		i++;
	}
	if(cookieval[i] == ';' && cookieval[i] == ' ')
	{
		cookieval[i] = '\0';
	}
	
	Q_strncpyz(sessionkey, cookieval, len);
	
	if(sessionkey[strlen(sessionkey) -1] == '\r')
	{
		sessionkey[strlen(sessionkey) -1] = '\0';	
	}
	
}


qboolean HTTPServer_Event(netadr_t* from, msg_t* msg, int connectionId)
{
	char sessionkey[128];
	httpPostVals_t values[MAX_POST_VALS];

	ftRequest_t* request = (ftRequest_t*)connectionId;
	int ret;
	
	
	if (request->finallen == -1 || request->recvmsg.cursize < request->finallen) {
		ret = HTTPServer_ReadMessage(from, msg, request);
		if(ret  == -1)
		{
			return qtrue;
		}else if (ret == 0) {
			return qfalse;
		}
	}
	/* Received full message */
	if(request->sendmsg.cursize == 0)
	{
		HTTPServer_ReadSessionId(request, sessionkey, sizeof(sessionkey));
		HTTPServer_ParseBody(request, values);
		Com_Printf("SessionID is: %s\n",  sessionkey);

		HTTPServer_BuildResponse( request, sessionkey, values);

	}
	return HTTPServer_WriteMessage(request, from);
	
}

/* Detecting the clientside protocol and process the 1st chunk of data if it is a http client */
tcpclientstate_t HTTPServer_AuthEvent(netadr_t* from, msg_t* msg, int *connectionId)
{
	ftRequest_t* request;
	*connectionId = 0;
	
	char protocol[MAX_STRING_CHARS];
	char* line;
	
	MSG_BeginReading(msg);
	
	line = MSG_ReadStringLine(msg, protocol, sizeof(protocol));
	if (line != NULL) {
		if ( Q_strncmp(line, "GET", 3) && Q_strncmp(line, "POST", 4) && Q_strncmp(line, "HEAD", 4) ) {
			return TCP_AUTHNOTME;
		}
		if( strstr(line, "HTTP/1.0") == NULL && strstr(line, "HTTP/1.1") == NULL )
		{
			/* Is not a HTTP client */
			return TCP_AUTHNOTME;
		}
	}
	
	request = FT_CreateRequest(NULL, NULL);
	
	if(request == NULL)
		return TCP_AUTHBAD;
	
	*connectionId = (int)request;

	
	if (HTTPServer_Event(from, msg, *connectionId) == qtrue)
	{
		return TCP_AUTHBAD;
	}	

	return TCP_AUTHSUCCESSFULL;
}


void HTTPServer_Disconnect(netadr_t* from, int connectionId)
{
	ftRequest_t* request;
	if(connectionId)
	{
		request = (ftRequest_t*)connectionId;
		FT_FreeRequest(request);

	}
}

void HTTPServer_Init()
{
	char magic[] = { 'h','t','t','p' };
	
	/* Register the events */
	NET_TCPAddEventType( HTTPServer_Event, HTTPServer_AuthEvent, HTTPServer_Disconnect, *(int*)magic);

	
	
}

/*
 =====================================================================
 User called File-Transfer functions intended to use in external files
 =====================================================================
*/

/* 
 * Function to open a new request by an valid URL starting with ftp:// or http://
 * Return codes: 
 * NULL = failed, Otherwise a valid handle. A valid handle must be always freed when action is completed.
 * It also must be freed in error cases
 */

ftRequest_t* FileDownloadRequest( const char* url)
{
	/* Strip away leading spaces */
	ftprotocols_t proto;

	
	while(*url == ' ')
		url++;
	
	if(!Q_stricmpn("ftp://", url, 6)){
		
		url += 6;
		proto = FT_PROTO_FTP;
	}else if(!Q_stricmpn("http://", url, 7)){
		
		url += 7;
		proto = FT_PROTO_HTTP;
	}else {
		proto = -1;
	}

	
	switch(proto)
	{
		case FT_PROTO_FTP:
			return FTP_DLRequest(url);
		case FT_PROTO_HTTP:
			return HTTPRequest(url, "GET", NULL, NULL);
		default:
			Com_PrintError("Unsupported File Transfer Protocol in url: %s!\n", url);
			return NULL;
	}
	return NULL;
}

/* 
 * This function must get called periodically (For example a loop) as long as it returns 0 to do the transfer actions
 * Return codes: 
 * 0 = not complete yet; 1 = complete; -1 = failed
 */

int FileDownloadSendReceive( ftRequest_t* request )
{
	
	switch (request->protocol) {
		case FT_PROTO_HTTP:
			return HTTP_SendReceiveData( request );
			
		case FT_PROTO_FTP:
			
			return FTP_SendReceiveData( request );
		default:
			return -1;
	}
	
}


/*
 * To free a valid handle. 
 * It have to get freed also on any error condition
 * Don't free it as long as you need the receive buffer
 */

void FileDownloadFreeRequest(ftRequest_t* request)
{
	if(request == NULL)
		return;
	
	FT_FreeRequest(request);
}

/*
 * An optional function which can be called to write out the download progress
 *
 */

const char* FileDownloadGenerateProgress( ftRequest_t* request )
{
	static char line[97];
	char bar[26];
	int num;
	float percent;
	float rate;
	int elapsedmsec;

	
	if(request->transferactive == qfalse)
	{
		return "";
	}
	
	if(request->transferStartTime == 0)
	{
		request->transferStartTime = Sys_Milliseconds();
	}
	
	if(request->transfermsg.data == NULL && request->recvmsg.data == NULL)
		return "";
	
	if(request->finallen > 0 && request->totalreceivedbytes > 0)
		percent = 100.0f * ((float)request->totalreceivedbytes / (float)request->finallen);
	else
		percent = 0.0f;
	
	num = (int)(percent / 4);
	
	if(num > sizeof(bar) -1)
	{
		num = sizeof(bar) -1;
	}
	Com_Memset(bar, ' ', sizeof(bar));
	Com_Memset(bar, '=', num);
	
	bar[sizeof(bar) -1] = '\0';
	
	elapsedmsec = Sys_Milliseconds() - request->transferStartTime;
	if (elapsedmsec != 0) {
		rate = (((float)request->totalreceivedbytes/1024.0f) * 1000.0f) / (float)elapsedmsec;
	}else {
		rate = 0;
	}
	
	
	Com_sprintf(line, sizeof(line), "\rReceiving: %s %.1f/%.1f kBytes @ %.1f kB/s", bar, (float)request->totalreceivedbytes/1024.0f, (float)request->finallen/1024.0f, rate);
	
	return line;
	
}

/*
 
 
 ftRequest_t* DownloadFile(const char* url)
 {
 ftRequest_t* handle;
	int state;

	handle = FileDownloadRequest( url );

	if (handle == NULL) {
		return NULL;
	}
	
	do {
		state = FileDownloadSendReceive(handle);
		Com_Printf("%s", FileDownloadGenerateProgress( handle ));
		usleep(20000);
	} while (state == 0);
	
	if(state < 0)
	{
		FileDownloadFreeRequest( handle );
		return NULL;
	}
	return handle;

}


void DL_Test_CB(const char* filename, qboolean success, ftRequest_t *handle)
{
	int before;
	if(success == qfalse)
	{
		Com_Printf("^1Transfer of %s has failed...\n", filename);
		return;
	}else {
		Com_Printf("^2Transfer successful...\n");
		before = Sys_Milliseconds();
		FS_WriteFile(filename, handle->recvmsg.data, handle->recvmsg.cursize);
		Com_Printf("Filewrite took %d msec", Sys_Milliseconds() - before);
	}
	
}


#include "sys_thread.h"

void DL_File(char* url)
{
	ftRequest_t* handle;
	//handle = DownloadFile( "http://iceops.in/demos/hardcore_tdm/demo_215216_0000.dm_1" );
	//handle = DownloadFile("ftp://ftp.idsoftware.com/idstuff/teamarena/q3ta_trailer.mpg");
	
	handle = DownloadFile(url);
	if(handle != NULL)
		Sys_SetupThreadCallback(DL_Test_CB, "dlfilename.gz", qtrue, handle);
	else
		Sys_SetupThreadCallback(DL_Test_CB, "dlfilename.gz", qfalse, handle);

}


void DL_Test_f()
{
	Sys_CreateCallbackThread(DL_File, "ftp://ftp.fu-berlin.de/INDEX.gz");
}
*/

/*
 =====================================================================
 Functions I don't use anymore
 =====================================================================
 */

/*
#define MAX_FILEDOWNLOAD_HANDLES 4

typedef struct
{
	ftRequest_t* handle;
	void (*downloadfinishedcb)(const char* filename, qboolean success, ftRequest_t* handle);
	char url[MAX_STRING_CHARS];
	char path[MAX_QPATH];
}fileDownload_t;

fileDownload_t filedownloads[MAX_FILEDOWNLOAD_HANDLES];

qboolean Com_AddDownload(const char* url, const char* savepath, void (*downloadfinishedcb)(const char* filename, qboolean success, ftRequest_t* handle))
{	
	int i;
	
	if(url == NULL || savepath == NULL || downloadfinishedcb == NULL)
		Com_Error(ERR_FATAL, "Com_AddDownload: NULL argument");
	
	for( i = 0; i < MAX_FILEDOWNLOAD_HANDLES; i++ )
	{
		
		if(filedownloads[i].downloadfinishedcb == NULL)
		{
			filedownloads[i].handle = NULL;
			filedownloads[i].downloadfinishedcb = downloadfinishedcb;
			Q_strncpyz(filedownloads[i].path, savepath, sizeof(filedownloads[i].path));
			Q_strncpyz(filedownloads[i].url, url, sizeof(filedownloads[i].url));
			return qtrue;
		}
		
	}
	return qfalse;
}

void Com_ProcessRunningDownloads()
{
	int i, state;
	ftRequest_t *handle;
	
	
	for( i = 0; i < MAX_FILEDOWNLOAD_HANDLES; i++)
	{
		if(filedownloads[i].downloadfinishedcb == NULL)
			return;
		
		if(filedownloads[i].handle == NULL)
		{
			handle = FileDownloadRequest( filedownloads[i].url );
			if(handle == NULL)
			{
				filedownloads[i].downloadfinishedcb(filedownloads[i].path, qfalse, filedownloads[i].handle);
				filedownloads[i].downloadfinishedcb = NULL;
				memmove( &filedownloads[i], &filedownloads[i +1], MAX_FILEDOWNLOAD_HANDLES - (i +1) );
			}else {
				filedownloads[i].handle = handle;
			}
			continue;
		}
		
		state = FileDownloadSendReceive( filedownloads[i].handle );
		
		if(state == 0)
		{
			continue;
		}
		
		if(state < 0)
		{
			filedownloads[i].downloadfinishedcb(filedownloads[i].path, qfalse, filedownloads[i].handle);
		}else {
			filedownloads[i].downloadfinishedcb(filedownloads[i].path, qtrue, filedownloads[i].handle);
		}
		
		FileDownloadFreeRequest(filedownloads[i].handle);
		
		filedownloads[i].downloadfinishedcb = NULL;
		
		memmove( &filedownloads[i], &filedownloads[i +1], MAX_FILEDOWNLOAD_HANDLES - (i +1) );
		
	}
}

*/