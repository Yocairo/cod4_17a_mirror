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



#ifndef __PLAYER_H__
#define __PLAYER_H__

#include "q_shared.h"
#include "q_math.h"
#include "entity.h"

typedef struct{
    int	score; //0x2f78
    int	deaths; //0x2f7c
    int	kills; //0x2f80
    int	assists; //0x2f84
}clientScoreboard_t;

typedef struct{
	int	sprintButtonUpRequired;
	int	sprintDelay;
	int	lastSprintStart;
	int	lastSprintEnd;
	int	sprintStartMaxLength;
}sprintState_t;

typedef struct{
	int	yaw;
	int	timer;
	int	transIndex;
	int	flags;
}mantleState_t;

typedef struct playerState_s {
	int		commandTime;  // 0
	int		pm_type;  // 4
	int		bobCycle;  // 8
	int		pm_flags;  // 12
	int		weapFlags;  // 16
	int		otherFlags;  // 20
	int		pm_time;  // 24
	vec3_t		origin;  // 28
	
	// http://zeroy.com/script/player/getvelocity.htm
	vec3_t		velocity;  // 40

	int		var_01;  //
	int		var_02;  //

	int		weaponTime;  // 60
	int		weaponDelay;  // 64
	int		grenadeTimeLeft;  // 68
	int		throwBackGrenadeOwner;  // 72
	int		throwBackGrenadeTimeLeft;  // 76
	int		weaponRestrictKickTime;  // 80
	int		foliageSoundTime;  // 84
	int		gravity;  // 88
	int		leanf;  // 92
	int		speed;  // 96
	vec3_t		delta_angles;  // 100
	
	/*The ground entity's rotation will be added onto the player's view.  In particular, this will 
	* cause the player's yaw to rotate around the entity's z-axis instead of the world z-axis. 
	* Any rotation that the reference entity undergoes will affect the player.
	* http://zeroy.com/script/player/playersetgroundreferenceent.htm */
	int		groundEntityNum;  // 112

	vec3_t		vLadderVec;  // 116
	int		jumpTime;  // 128
	float		jumpOriginZ;  // 132
	
	// Animations as in mp/playeranim.script and animtrees/multiplayer.atr, it also depends on mp/playeranimtypes.txt (the currently used weapon)
	int		legsTimer;  // 136
	int		legsAnim;  // 140
	int		torsoTimer;  // 144
	int		torsoAnim;  // 148

	int		var_03;  //
	int		var_04;  //

	int		damageTimer;  // 160
	int		damageDuration;  // 164
	int		flinchYawAnim;  // 168
	int		movementDir;  // 172
	int		eFlags;  // 176
	int		eventSequence;  // 180

	vec4_t		events;  // 184
	vec4_t		eventParms;  // 200

	int		var_05;  //

	int		clientNum;  // 220
	int		offHandIndex;  // 224
	int		offhandSecondary;  // 228
	int		weapon;  // 232
	int		weaponstate;  // 236
	int		weaponShotCount;  // 240
	int		fWeaponPosFrac;  // 244
	int		adsDelayTime;  // 248
	
	// http://zeroy.com/script/player/resetspreadoverride.htm
	// http://zeroy.com/script/player/setspreadoverride.htm
	int		spreadOverride;  // 252
	int		spreadOverrideState;  // 256
	
	int		viewmodelIndex;  // 260

	vec3_t		viewangles;  // 264
	float		viewHeightTarget;  // 276
	float		viewHeightCurrent;  // 280
	int		viewHeightLerpTime;  // 284
	int		viewHeightLerpTarget;  // 288
	int		viewHeightLerpDown;  // 292
	vec2_t		viewAngleClampBase;  // 296
	vec2_t		viewAngleClampRange;  // 304

	int		damageEvent;  // 312
	int		damageYaw;  // 316
	int		damagePitch;  // 320
	int		damageCount;  // 324

	int		unk1[261];  // 328

	vec4_t		weapons;  // 1372

	vec4_t		weaponold;  // 1388

	vec4_t		weaponrechamber;  // 1404

	int		proneDirection;  // 1420
	int		proneDirectionPitch;  // 1424
	int		proneTorsoPitch;  // 1428
	int		viewlocked;  // 1432
	int		viewlocked_entNum;  // 1436

	int		cursorHint;  // 1440
	int		cursorHintString;  // 1444
	int		cursorHintEntIndex;  // 1448

	int		iCompassPlayerInfo;  // 1452
	int		radarEnabled;  // 1456

	int		locationSelectionInfo;  // 1460
	sprintState_t	sprintState;  // 1464
	
	// used for leaning?
	int		fTorsoPitch;  // 1484
	int		fWaistPitch;  // 1488

	int		holdBreathScale;  // 1492
	int		holdBreathTimer;  // 1496
	
	// Scales player movement speed by this amount, ???it's actually a float???
	// http://zeroy.com/script/player/setmovespeedscale.htm
	int		moveSpeedScaleMultiplier;  // 1500
	
	mantleState_t	mantleState;  // 1504
	int		meleeChargeYaw;  // 1520
	int		meleeChargeDist;  // 1524
	int		meleeChargeTime;  // 1528
	int		perks;  // 1532

	vec4_t		actionSlotType;  // 1536
	vec4_t		actionSlotParam;  // 1552

	int		var_06; // 1568

	int		weapAnim;  // 1572
	int		aimSpreadScale;  // 1576
	
	// http://zeroy.com/script/player/shellshock.htm
	int		shellshockIndex;  // 1580
	int		shellshockTime;  // 1584
	int		shellshockDuration;  // 1588

	// http://zeroy.com/script/player/setdepthoffield.htm
	int		dofNearStart;  // 1592
	int		dofNearEnd;  // 1596
	int		dofFarStart;  // 1600
	int		dofFarEnd;  // 1604
	float		dofNearBlur;  // 1608
	float		dofFarBlur;  // 1612
	int		dofViewmodelStart;  // 1616
	int		dofViewmodelEnd;  // 1620

	int		unk2[145];  // 1624

	int		deltaTime;  // 2204
	int		killCamEntity;  // 2208

	int		unk3[2480];  // 2212
} playerState_t;//Size: 0x2f64


typedef enum {
	CON_DISCONNECTED,
	CON_CONNECTING,
	CON_CONNECTED
} clientConnected_t;

typedef enum {
	SPECTATOR_NOT,
	SPECTATOR_FREE,
	SPECTATOR_FOLLOW,
	SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum {
	TEAM_BEGIN,     // Beginning a team game, spawn at base
	TEAM_ACTIVE     // Now actively playing
} playerTeamStateState_t;

typedef struct {
	playerTeamStateState_t state;

	int location;

	int captures;
	int basedefense;
	int carrierdefense;
	int flagrecovery;
	int fragcarrier;
	int assists;

	float lasthurtcarrier;
	float lastreturnedflag;
	float flagsince;
	float lastfraggedcarrier;
} playerTeamState_t;

typedef enum {
	TEAM_FREE,
	TEAM_RED,
	TEAM_BLUE,
	TEAM_SPECTATOR,
	TEAM_NUM_TEAMS
}team_t;

// the auto following clients don't follow a specific client
// number, but instead follow the first two active players
#define FOLLOW_ACTIVE1  -1
#define FOLLOW_ACTIVE2  -2

// client data that stays across multiple levels or tournament restarts
// this is achieved by writing all the data to cvar strings at game shutdown
// time and reading them back at connection time.  Anything added here
// MUST be dealt with in G_InitSessionData() / G_ReadSessionData() / G_WriteSessionData()
typedef struct {

	int clientState;		// 0x300c
	//Most is not active
	team_t sessionTeam;		//0x3010
	int spectatorTime;              // for determining next-in-line to play
	spectatorState_t spectatorState;
	int spectatorClient_Unknown;            // for chasecam and follow mode
	int wins, losses;               // tournament stats
	int playerType;                 // DHM - Nerve :: for GT_WOLF
	int playerWeapon;               // DHM - Nerve :: for GT_WOLF
//	int playerItem;                 // DHM - Nerve :: for GT_WOLF
	int playerTagIndex;		//0x3030
	int playerSkin;                 // DHM - Nerve :: for GT_WOLF
//	int spawnObjectiveIndex;         // JPW NERVE index of objective to spawn nearest to (returned from UI)
	int latchPlayerType;            // DHM - Nerve :: for GT_WOLF not archived
	int latchPlayerWeapon;          // DHM - Nerve :: for GT_WOLF not archived
	int latchPlayerItem;            // DHM - Nerve :: for GT_WOLF not archived
	int latchPlayerSkin;            // DHM - Nerve :: for GT_WOLF not archived
	char netname[MAX_NAME_LENGTH];	//0x3048
	int lastFollowedClient;
	int rank;			//0x305c
	int prestige;			//0x3060
	int perkIndex;			//0x3064
	int vehicleOwnerNum;		//0x3068  //Mybe vehicletype ?
	int vehicleRideSlot;		//0x306c
	int PSOffsetTime;		//0x3070 ???
	int spectatorClient;           //0x3074 for chasecam and follow mode
} clientSession_t;

#define MAX_NETNAME         16

#define PICKUP_ACTIVATE 0   // pickup items only when using "+activate"
#define PICKUP_TOUCH    1   // pickup items when touched
#define PICKUP_FORCE    2   // pickup the next item when touched (and reset to PICKUP_ACTIVATE when done)

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {//Not Known
	int			serverTime;
	int			buttons;
	int			angles[3];
	byte weapon;
	byte offHandIndex;
	byte field_16;
	byte field_17;
	int field_18;
	int field_1C;
} usercmd_t;


// client data that stays across multiple respawns, but is cleared
// on each level change or team change at ClientBegin()
typedef struct {

        enum{	STATE_PLAYING, STATE_DEAD,
		STATE_SPECTATOR, STATE_INTERMISSION
	}playerState;//0x2f64

	int unknownStateVar;		//0x2f68
	int unknownStateVar2;		//0x2f6c
	int unknown[2];			//0x2f70

	clientScoreboard_t	scoreboard;	//0x2f78
	qboolean initialSpawn;          //0x2f88 the first spawn should be at a cool location
	clientConnected_t connected;	//0x2f8c maybe
	usercmd_t cmd;                  //0x2f90 we would lose angles if not persistant
	usercmd_t oldcmd;               //0x2fb0 previous command processed by pmove()
	qboolean localClient;           //0x2fd0 true if "ip" info key is "localhost"

	qboolean predictItemPickup;     //0x2fd4 based on cg_predictItems userinfo
	char netname[MAX_NETNAME];	//0x2fd8

	int maxHealth;                  // 0x2fe8 for handicapping
	int enterTime;                  // 0x2fec level.time the client entered the game
	int connectTime;                // DHM - Nerve :: level.time the client first connected to the server  // N/A
//	playerTeamState_t teamState;    // status in teamplay games
	int voteCount;                  // 0x2ff4 to prevent people from constantly calling votes
	int teamVoteCount;              // to prevent people from constantly calling votes		// N/A

	int moveSpeedScale;		// 0x2ffc

	int viewModel;			// 0x3000 //Model-index

	int clientCanSpectate;		// 0x3004
	int freeaddr1;			// 0x3008

} clientPersistant_t;


typedef struct {
	vec3_t mins;
	vec3_t maxs;

	vec3_t origin;

	int time;
	int servertime;
} clientMarker_t;

#define MAX_CLIENT_MARKERS 10

#define LT_SPECIAL_PICKUP_MOD   3       // JPW NERVE # of times (minus one for modulo) LT must drop ammo before scoring a point
#define MEDIC_SPECIAL_PICKUP_MOD    4   // JPW NERVE same thing for medic




typedef struct {
void* dummy;
}animModelInfo_t; //Dummy


typedef struct {
	qboolean bAutoReload; // do we predict autoreload of weapons
	int blockCenterViewTime; // don't let centerview happen for a little while

	// Arnout: MG42 aiming
	float varc, harc;
	vec3_t centerangles;

} pmoveExt_t;   // data used both in client and server - store it here


typedef struct gclient_s gclient_t;


// this structure is cleared on each ClientSpawn(),
// except for 'client->pers' and 'client->sess'
struct gclient_s {
	// ps MUST be the first element, because the server expects it
	playerState_t ps;               //0x00 communicated by server to clients

	// the rest of the structure is private to game

	clientPersistant_t pers;	//0x2f64
	clientSession_t sess;		//0x3010

	qboolean noclip;		//0x3078
	qboolean ufo;			//0x307c
	qboolean freezeControls;	//0x3080

	int lastCmdTime;                //0x3084 level.time of last usercmd_t, for EF_CONNECTION
									// we can't just use pers.lastCommand.time, because
									// of the g_sycronousclients case
	//Buttonlogic/exact addresses is not known but scope
	int buttons;			//0x3088
	int oldbuttons;	
	int unk1;
	int latched_buttons;		//0x3094

	int wbuttons;
	int oldwbuttons;
	int latched_wbuttons;

	// sum up damage over an entire frame, so
	// shotgun blasts give a single big kick
	int damage_armor;               //0x30a4 damage absorbed by armor
	int damage_blood;               // damage taken out of health
	int damage_knockback;           // impact damage
	vec3_t damage_from;             //0x30b0 origin for vector calculation
	qboolean damage_fromWorld;      //0x30bc if true, don't use the damage_from vector

	int accurateCount;              // for "impressive" reward sound	N/A

	int accuracy_shots;             // total number of shots		N/A
	int accuracy_hits;              // total number of hits			N/A

	//
//	int lastkilled_client;          // last client that this client killed
//	int lasthurt_client;            // last client that damaged this client
//	int lasthurt_mod;               // type of damage the client did

	// timers
//	int respawnTime;                // can respawn when time > this, force after g_forcerespwan
	int inactivityTime;             //0x30cc kick players when time > this
	qboolean inactivityWarning;     //0x30d0 qtrue if the five second warning has been given
	int playerTalkTime;		//0x30d4 ??
	int rewardTime;                 // clear the EF_AWARD_IMPRESSIVE, etc when time > this  N/A
        vec3_t unk;			//0x30dc


	int airOutTime;			//0x30e8 Unknown only reset

	int lastKillTime;               // ???for multiple kill rewards
	int dummy34;

	qboolean fireHeld;              // ???used for hook
	gentity_t   *hook;              //0x30f8 grapple hook if out

	int switchTeamTime;             //0x30fc time the player switched teams



	int IMtooLazy[33];
	//Not anymore resolved

	// timeResidual is used to handle events that happen every second
	// like health / armor countdowns and regeneration
/*	int timeResidual;

	float currentAimSpreadScale;

	int medicHealAmt;

	// RF, may be shared by multiple clients/characters
	animModelInfo_t *modelInfo;

	// -------------------------------------------------------------------------------------------
	// if working on a post release patch, new variables should ONLY be inserted after this point

	gentity_t   *persistantPowerup;
	int portalID;
	int ammoTimes[WP_NUM_WEAPONS];
	int invulnerabilityTime;

	gentity_t   *cameraPortal;              // grapple hook if out
	vec3_t cameraOrigin;

	int dropWeaponTime;         // JPW NERVE last time a weapon was dropped
	int limboDropWeapon;         // JPW NERVE weapon to drop in limbo
	int deployQueueNumber;         // JPW NERVE player order in reinforcement FIFO queue
	int sniperRifleFiredTime;         // JPW NERVE last time a sniper rifle was fired (for muzzle flip effects)
	float sniperRifleMuzzleYaw;       // JPW NERVE for time-dependent muzzle flip in multiplayer
	int lastBurnTime;         // JPW NERVE last time index for flamethrower burn
	int PCSpecialPickedUpCount;         // JPW NERVE used to count # of times somebody's picked up this LTs ammo (or medic health) (for scoring)
	int saved_persistant[MAX_PERSISTANT];           // DHM - Nerve :: Save ps->persistant here during Limbo

	// g_antilag.c
	int topMarker;
	clientMarker_t clientMarkers[MAX_CLIENT_MARKERS];
	clientMarker_t backupMarker;

	gentity_t       *tempHead;  // Gordon: storing a temporary head for bullet head shot detection

	pmoveExt_t pmext;*/
};//Size: 0x3184

#endif

