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



#ifndef G_SHARED_H
#define G_SHARED_H

#include "filesystem.h"
#include "entity.h"
#include "player.h"
#include "cvar.h"

#include "sys_cod4defs.h"

#define level_ADDR 0x8370440
#define level (*((level_locals_t*)(level_ADDR)))

#define g_gametypes ((gametypes_t*)(0x8583bc0))

// this structure is cleared as each map is entered
//
#define MAX_SPAWN_VARS          64
#define MAX_SPAWN_VARS_CHARS    2048

typedef struct
{
  const char *key;
  const char *value;
}keyValueStr_t;


typedef struct
{
  int isGameLoadingSpawn;
  int numSpawnVars;
  keyValueStr_t keyValPairs[64];
  int numSpawnVarChars;
  char spawnVarChars[2048];
}SpawnVar_t;


typedef struct
{
  void *name;
  int offset;
  int type;
}cspField_t;



typedef struct { //0x8370440
	struct gclient_s    *clients;       // [maxclients]

	struct gentity_s    *gentities;

	int notActive1[2];

	int num_entities;               // current number, <= MAX_GENTITIES

	int unk1;

	fileHandle_t logFile;


	int unknownBig[114];
	// store latched cvars here that we want to get at often
	int maxclients;				//0x1e4
	int framenum;
	int time;                           // in msec		0x1ec
	int previousTime;                   // 0x1f0 so movers can back up when blocked
	int frameTime;                      // Gordon: time the frame started, for antilag stuff

	int startTime;                      // level.time the map was started


	int teamScores[TEAM_NUM_TEAMS];		//0x1fc
	int lastTeamLocationTime;               // last time of client team location update

	int sendscoreboard;		//???? Not known 0x210
	int unknown;
	int clientNameMode;			//0x218 Manual Change mode
	int numPlayingClients;              // connected, non-spectators
	int sortedClients[MAX_CLIENTS];		//sorted by rank or score ? 0x220 



	// voting state
	char voteString[MAX_STRING_CHARS];		//0x320
	char voteDisplayString[MAX_STRING_CHARS];	//0x720
	int voteTime;                       // level.time vote was called	0xb20
	int voteExecuteTime;                // time the vote is executed
	int voteYes;				//0xb28
	int voteNo;				//0xb2c
	int numVotingClients;			// set by CalculateRanks

	SpawnVar_t spawnVars;
	int savePersist;

/*
	// team voting state
	char teamVoteString[2][MAX_STRING_CHARS];
	int teamVoteTime[2];                // level.time vote was called
	int teamVoteYes[2];
	int teamVoteNo[2];
	int numteamVotingClients[2];        // set by CalculateRanks

	// spawn variables
	qboolean spawning;                  // the G_Spawn*() functions are valid
	int numSpawnVars;
	char        *spawnVars[MAX_SPAWN_VARS][2];  // key / value pairs
	int numSpawnVarChars;
	char spawnVarChars[MAX_SPAWN_VARS_CHARS];

	// intermission state
	int intermissionQueued;             // intermission was qualified, but
										// wait INTERMISSION_DELAY_TIME before
										// actually going there so the last
										// frag can be watched.  Disable future
										// kills during this delay
	int intermissiontime;               // time the intermission was started
	char        *changemap;
	qboolean readyToExit;               // at least one client wants to exit
	int exitTime;
	vec3_t intermission_origin;         // also used for spectator spawns
	vec3_t intermission_angle;

	qboolean locationLinked;            // target_locations get linked
	gentity_t   *locationHead;          // head of the location list
	int bodyQueIndex;                   // dead bodies
	gentity_t   *bodyQue[BODY_QUEUE_SIZE];

	int portalSequence;
	// Ridah
	char        *scriptAI;
	int reloadPauseTime;                // don't think AI/client's until this time has elapsed
	int reloadDelayTime;                // don't start loading the savegame until this has expired

	int lastGrenadeKick;

	int loperZapSound;
	int stimSoldierFlySound;
	int bulletRicochetSound;
	// done.

	int snipersound;

	//----(SA)	added
	int knifeSound[4];
	//----(SA)	end

// JPW NERVE
	int capturetimes[4];         // red, blue, none, spectator for WOLF_MP_CPH
	int redReinforceTime, blueReinforceTime;         // last time reinforcements arrived in ms
	int redNumWaiting, blueNumWaiting;         // number of reinforcements in queue
	vec3_t spawntargets[MAX_MULTI_SPAWNTARGETS];      // coordinates of spawn targets
	int numspawntargets;         // # spawntargets in this map
// jpw

	// RF, entity scripting
	char        *scriptEntity;

	// player/AI model scripting (server repository)
	animScriptData_t animScriptData;

	// NERVE - SMF - debugging/profiling info
	int totalHeadshots;
	int missedHeadshots;
	qboolean lastRestartTime;
	// -NERVE - SMF

	int numFinalDead[2];                // DHM - Nerve :: unable to respawn and in limbo (per team)
	int numOidTriggers;                 // DHM - Nerve

	qboolean latchGametype;             // DHM - Nerve*/


} level_locals_t;



typedef struct {
    char	gametypename[64];
    char	gametypereadable[68];
} gametype_t;


typedef struct {
    int		var_01;
    int		var_02;
    int		var_03;
    int		numGametypes;
    gametype_t	gametype[];
} gametypes_t;


#define iDFLAGS_RADIUS					1			// explosive damage
#define iDFLAGS_NO_ARMOR				2			// ???
#define iDFLAGS_NO_KNOCKBACK			4			// players dont get pushed in damage_dir
#define iDFLAGS_PENETRATION				8			// bullets can penetrate walls
#define iDFLAGS_NO_TEAM_PROTECTION		16			// team kills/damage in TDM/SD etc
#define iDFLAGS_NO_PROTECTION			32			// nothing can stop damage
#define iDFLAGS_PASSTHRU				64			// bullet passed through non solid surface (???)

//
// config strings are a general means of communicating variable length strings
// from the server to all connected clients.
//

// CS_SERVERINFO and CS_SYSTEMINFO are defined in q_shared.h
/*

#define CS_MUSIC                2
#define CS_MESSAGE              3       // from the map worldspawn's message field
#define CS_MOTD                 4       // g_motd string for server message of the day
#define CS_WARMUP               5       // server time when the match will be restarted
#define CS_SCORES1              6
#define CS_SCORES2              7
*/

#define CS_VOTE_TIME            13
#define CS_VOTE_STRING          14
#define CS_VOTE_YES             15
#define CS_VOTE_NO              16
/*
#define CS_GAME_VERSION         12
#define CS_LEVEL_START_TIME     13      // so the timer only shows the current level
#define CS_INTERMISSION         14      // when 1, intermission will start in a second or two

*/
// DHM - Nerve :: Wolf Multiplayer information

// TTimo - voting config flags
#define VOTEFLAGS_RESTART           ( 1 << 0 )
#define VOTEFLAGS_GAMETYPE          ( 1 << 1 )
#define VOTEFLAGS_STARTMATCH        ( 1 << 2 )
#define VOTEFLAGS_NEXTMAP           ( 1 << 3 )
#define VOTEFLAGS_SWAP              ( 1 << 4 )
#define VOTEFLAGS_TYPE              ( 1 << 5 )
#define VOTEFLAGS_KICK              ( 1 << 6 )
#define VOTEFLAGS_MAP               ( 1 << 7 )
#define VOTEFLAGS_ANYMAP            ( 1 << 8 )
/*
// entityState_t->eFlags
#define EF_DEAD             0x00000001      // don't draw a foe marker over players with EF_DEAD
#define EF_NONSOLID_BMODEL  0x00000002      // bmodel is visible, but not solid
#define EF_TELEPORT_BIT     0x00000004      // toggled every time the origin abruptly changes
#define EF_MONSTER_EFFECT   0x00000008      // draw an aiChar dependant effect for this character
#define EF_CAPSULE          0x00000010      // use capsule for collisions
#define EF_CROUCHING        0x00000020      // player is crouching
#define EF_MG42_ACTIVE      0x00000040      // currently using an MG42
#define EF_NODRAW           0x00000080      // may have an event, but no model (unspawned items)
#define EF_FIRING           0x00000100      // for lightning gun
#define EF_INHERITSHADER    EF_FIRING       // some ents will never use EF_FIRING, hijack it for "USESHADER"
#define EF_BOUNCE_HEAVY     0x00000200      // more realistic bounce.  not as rubbery as above (currently for c4)
#define EF_SPINNING         0x00000400      // (SA) added for level editor control of spinning pickup items
#define EF_BREATH           EF_SPINNING     // Characters will not have EF_SPINNING set, hijack for drawing character breath

#define EF_MELEE_ACTIVE     0x00000800      // (SA) added for client knowledge of melee items held (chair/etc.)
#define EF_TALK             0x00001000      // draw a talk balloon
#define EF_SMOKING          EF_MONSTER_EFFECT3  // DHM - Nerve :: ET_GENERAL ents will emit smoke if set // JPW switched to this after my code change
#define EF_CONNECTION       0x00002000      // draw a connection trouble sprite
#define EF_MONSTER_EFFECT2  0x00004000      // show the secondary special effect for this character
#define EF_SMOKINGBLACK     EF_MONSTER_EFFECT2  // JPW NERVE -- like EF_SMOKING only darker & bigger
#define EF_HEADSHOT         0x00008000      // last hit to player was head shot
#define EF_MONSTER_EFFECT3  0x00010000      // show the third special effect for this character
#define EF_HEADLOOK         0x00020000      // make the head look around*/

#define EF_VOTED            0x00100000     // already cast a vote

/*

#define EF_STAND_IDLE2      0x00040000      // when standing, play idle2 instead of the default
#define EF_VIEWING_CAMERA   EF_STAND_IDLE2  // NOTE: REMOVE STAND_IDLE2 !!
#define EF_TAGCONNECT       0x00080000      // connected to another entity via tag
#define EF_MOVER_BLOCKED    0x00100000      // mover was blocked dont lerp on the client
#define EF_FORCED_ANGLES    0x00200000  // enforce all body parts to use these angles

#define EF_ZOOMING          0x00400000      // client is zooming
#define EF_NOSWINGANGLES    0x00800000      // try and keep all parts facing same direction


// !! NOTE: only place flags that don't need to go to the client beyond 0x00800000

#define EF_DUMMY_PMOVE      0x01000000
#define EF_BOUNCE           0x04000000      // for missiles
#define EF_BOUNCE_HALF      0x08000000      // for missiles
#define EF_MOVER_STOP       0x10000000      // will push otherwise	// (SA) moved down to make space for one more client flag

*/






/*
// --- COD4: raw\maps\mp\gametypes\_missions.gsc --- //

typedef enum
{
	MOD_UNKNOWN,
	MOD_PISTOL_BULLET,
	MOD_RIFLE_BULLET,
	MOD_GRENADE,
	MOD_GRENADE_SPLASH,
	MOD_PROJECTILE,
	MOD_PROJECTILE_SPLASH,
	MOD_MELEE,
	MOD_HEAD_SHOT,
	MOD_CRUSH,
	MOD_TELEFRAG,
	MOD_FALLING,
	MOD_SUICIDE,
	MOD_TRIGGER_HURT,
	MOD_EXPLOSIVE,
	MOD_IMPACT,

	MOD_BAD

} meansOfDeath_t;

#define MOD_NUM	16
char *modNames[MOD_NUM] =
{
	"MOD_UNKNOWN",
	"MOD_PISTOL_BULLET",
	"MOD_RIFLE_BULLET",
	"MOD_GRENADE",
	"MOD_GRENADE_SPLASH",
	"MOD_PROJECTILE",
	"MOD_PROJECTILE_SPLASH",
	"MOD_MELEE",
	"MOD_HEAD_SHOT",
	"MOD_CRUSH",
	"MOD_TELEFRAG",
	"MOD_FALLING",
	"MOD_SUICIDE",
	"MOD_TRIGGER_HURT",
	"MOD_EXPLOSIVE",
	"MOD_IMPACT",
};

static const char *g_HitLocNames[] =
{
	"none",
	"helmet",
	"head",
	"neck",
	"torso_upper",
	"torso_lower",
	"right_arm_upper",
	"left_arm_upper",
	"right_arm_lower",
	"left_arm_lower",
	"right_hand",
	"left_hand",
	"right_leg_upper",
	"left_leg_upper",
	"right_leg_lower",
	"left_leg_lower",
	"right_foot",
	"left_foot",
	"gun",
};

*/




/*
// --- COD4: raw\maps\mp\gametypes\_hud.gsc --- //

// Edge relative placement values for rect->h_align and rect->v_align
#define HORIZONTAL_ALIGN_SUBLEFT		0	// left edge of a 4:3 screen (safe area not included)
#define HORIZONTAL_ALIGN_LEFT			1	// left viewable (safe area) edge
#define HORIZONTAL_ALIGN_CENTER			2	// center of the screen (reticle)
#define HORIZONTAL_ALIGN_RIGHT			3	// right viewable (safe area) edge
#define HORIZONTAL_ALIGN_FULLSCREEN		4	// disregards safe area
#define HORIZONTAL_ALIGN_NOSCALE		5	// uses exact parameters - neither adjusts for safe area nor scales for screen size
#define HORIZONTAL_ALIGN_TO640			6	// scales a real-screen resolution x down into the 0 - 640 range
#define HORIZONTAL_ALIGN_CENTER_SAFEAREA 7	// center of the safearea
#define HORIZONTAL_ALIGN_MAX			HORIZONTAL_ALIGN_CENTER_SAFEAREA
#define HORIZONTAL_ALIGN_DEFAULT		HORIZONTAL_ALIGN_SUBLEFT

#define VERTICAL_ALIGN_SUBTOP			0	// top edge of the 4:3 screen (safe area not included)
#define VERTICAL_ALIGN_TOP				1	// top viewable (safe area) edge
#define VERTICAL_ALIGN_CENTER			2	// center of the screen (reticle)
#define VERTICAL_ALIGN_BOTTOM			3	// bottom viewable (safe area) edge
#define VERTICAL_ALIGN_FULLSCREEN		4	// disregards safe area
#define VERTICAL_ALIGN_NOSCALE			5	// uses exact parameters - neither adjusts for safe area nor scales for screen size
#define VERTICAL_ALIGN_TO480			6	// scales a real-screen resolution y down into the 0 - 480 range
#define VERTICAL_ALIGN_CENTER_SAFEAREA	7	// center of the save area
#define VERTICAL_ALIGN_MAX				VERTICAL_ALIGN_CENTER_SAFEAREA
#define VERTICAL_ALIGN_DEFAULT			VERTICAL_ALIGN_SUBTOP

static const char *g_he_font[] =
{
	"default",		// HE_FONT_DEFAULT
	"bigfixed",		// HE_FONT_BIGFIXED
	"smallfixed",	// HE_FONT_SMALLFIXED
	"objective",	// HE_FONT_OBJECTIVE
};


// These values correspond to the defines in q_shared.h
static const char *g_he_alignx[] =
{
	"left",   // HE_ALIGN_LEFT
	"center", // HE_ALIGN_CENTER
	"right",  // HE_ALIGN_RIGHT
};


static const char *g_he_aligny[] =
{
	"top",    // HE_ALIGN_TOP
	"middle", // HE_ALIGN_MIDDLE
	"bottom", // HE_ALIGN_BOTTOM
};


// These values correspond to the defines in menudefinition.h
static const char *g_he_horzalign[] =
{
	"subleft",			// HORIZONTAL_ALIGN_SUBLEFT
	"left",				// HORIZONTAL_ALIGN_LEFT
	"center",			// HORIZONTAL_ALIGN_CENTER
	"right",			// HORIZONTAL_ALIGN_RIGHT
	"fullscreen",		// HORIZONTAL_ALIGN_FULLSCREEN
	"noscale",			// HORIZONTAL_ALIGN_NOSCALE
	"alignto640",		// HORIZONTAL_ALIGN_TO640
	"center_safearea",	// HORIZONTAL_ALIGN_CENTER_SAFEAREA
};
//cassert( ARRAY_COUNT( g_he_horzalign ) == HORIZONTAL_ALIGN_MAX + 1 );


static const char *g_he_vertalign[] =
{
	"subtop",			// VERTICAL_ALIGN_SUBTOP
	"top",				// VERTICAL_ALIGN_TOP
	"middle",			// VERTICAL_ALIGN_CENTER
	"bottom",			// VERTICAL_ALIGN_BOTTOM
	"fullscreen",		// VERTICAL_ALIGN_FULLSCREEN
	"noscale",			// VERTICAL_ALIGN_NOSCALE
	"alignto480",		// VERTICAL_ALIGN_TO480
	"center_safearea",	// VERTICAL_ALIGN_CENTER_SAFEAREA
};
//cassert( ARRAY_COUNT( g_he_vertalign ) == VERTICAL_ALIGN_MAX + 1 );

*/

extern cvar_t *g_allowConsoleSay;
extern cvar_t *g_disabledefcmdprefix;
extern cvar_t *g_votedMapName;
extern cvar_t *g_votedGametype;
extern cvar_t *g_allowVote;
extern cvar_t *g_TeamName_Axis;
extern cvar_t *g_TeamName_Allies;
extern cvar_t *g_gravity;
extern cvar_t *jump_height;
extern cvar_t *jump_stepSize;
extern cvar_t *jump_slowdownEnable;


extern qboolean onExitLevelExecuted;

int BG_GetPerkIndexForName(const char* name);
int G_GetSavePersist(void);
void G_SetSavePersist(int val);

int G_GetClientSize();
gclient_t* G_GetPlayerState(int num);
clientSession_t * G_GetClientState(int num);
void SpawnVehicle(gentity_t* ent, const char* vehtype);
void __cdecl G_VehSpawner(gentity_t *ent);
void __cdecl G_VehCollmapSpawner(gentity_t *ent);
void __cdecl G_SetModel(gentity_t *ent, const char* modelname);
//This defines Cvars directly related to executable file
#define getcvaradr(adr) ((cvar_t*)(*(int*)(adr)))

#define g_maxclients getcvaradr(0x84bcfe8)

#endif /*G_SHARED_H*/

