//============= Copyright (c) StargateTC Source Team ============================
//
// Server side of the StargateTC team/class selection flow.
//
// The behavior mirrors the recovered GoldSrc rules: clients enter spectator,
// choose Tau'ri or Goa'uld, then choose one of five team classes. mp_wepselmode
// controls whether that class contributes a weapon on top of the team melee or
// default team pack.
//
//==============================================================================
#include "cbase.h"
#include "hl2mp_player.h"
#include "recipientfilter.h"
#include "stargate/sg_constants.h"
#include <limits.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar mp_wepselmode;

struct SgClassInfo_t
{
	const char *name;     // joinclass token
	const char *display;  // scoreboard/chat label
	const char *avatar;   // avatar sprite name (vgui/stargate/avatars/<avatar>)
	const char *model;    // player model
	const char *weapon;   // signature loadout weapon
	int         team;     // owning team
	int         slot;     // original class slot, 1..5 inside team
	int         limit;    // 0 means unlimited
};

// Class roster recovered from the GoldSrc class menu and class-limit checks.
static const SgClassInfo_t s_Classes[] = {
	{ "oneill",  "O'Neill",      "oneill",  "models/stargate/player/oneill/oneill.mdl",         "weapon_m92s",   StargateTC::TEAM_TORRI,  1, 2 },
	{ "carter",  "Carter",       "carter",  "models/stargate/player/carter/carter.mdl",         "weapon_zat",    StargateTC::TEAM_TORRI,  2, 3 },
	{ "jackson", "Jackson",      "jackson", "models/stargate/player/jackson/jackson.mdl",       "weapon_zat",    StargateTC::TEAM_TORRI,  3, 3 },
	{ "tealc",   "Teal'c",       "tealc",   "models/stargate/player/tealc/tealc.mdl",           "weapon_lance",  StargateTC::TEAM_TORRI,  4, 0 },
	{ "soldat",  "Soldier",      "commder", "models/stargate/player/commander/commander.mdl",   "weapon_m16",    StargateTC::TEAM_TORRI,  5, 0 },
	{ "dieu",    "System Lord",  "lord",    "models/stargate/player/systemlord/systemlord.mdl", "weapon_zatarc", StargateTC::TEAM_GOAULD, 1, 2 },
	{ "anubis",  "Anubis Guard", "anubis",  "models/stargate/player/anubis/anubis.mdl",         "weapon_lance",  StargateTC::TEAM_GOAULD, 2, 2 },
	{ "jaffa",   "Jaffa",        "goauld3", "models/stargate/player/goauld3/goauld3.mdl",       "weapon_lance",  StargateTC::TEAM_GOAULD, 3, 0 },
	{ "prima",   "First Prime",  "snake",   "models/stargate/player/snakegoauld/snakegoauld.mdl", "weapon_lance", StargateTC::TEAM_GOAULD, 4, 3 },
	{ "esclave", "Horus Guard",  "slave",   "models/stargate/player/horus/horus.mdl",           "weapon_lance",  StargateTC::TEAM_GOAULD, 5, 3 },
};

// Chosen class per player slot (1..MAX_PLAYERS).
static char s_PlayerClass[ MAX_PLAYERS + 1 ][ 32 ];

static const SgClassInfo_t *Sg_FindClass( const char *name )
{
	for ( int i = 0; i < ARRAYSIZE( s_Classes ); i++ )
	{
		if ( name && !Q_stricmp( s_Classes[i].name, name ) )
			return &s_Classes[i];
	}

	return NULL;
}

static const SgClassInfo_t *Sg_FindClassBySlot( int team, int slot )
{
	for ( int i = 0; i < ARRAYSIZE( s_Classes ); i++ )
	{
		if ( s_Classes[i].team == team && s_Classes[i].slot == slot )
			return &s_Classes[i];
	}

	return NULL;
}

static int Sg_CountPlayersUsingClass( const SgClassInfo_t *pClass, CHL2MP_Player *pIgnore )
{
	if ( !pClass )
		return 0;

	int count = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pOther = ToHL2MPPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pOther || pOther == pIgnore )
			continue;

		if ( pOther->GetTeamNumber() != pClass->team )
			continue;

		if ( !Q_stricmp( s_PlayerClass[i], pClass->name ) )
			count++;
	}

	return count;
}

static bool Sg_ClassHasRoom( const SgClassInfo_t *pClass, CHL2MP_Player *pPlayer )
{
	if ( !pClass || pClass->limit <= 0 )
		return true;

	return Sg_CountPlayersUsingClass( pClass, pPlayer ) < pClass->limit;
}

static const SgClassInfo_t *Sg_FindAutoClass( int team, CHL2MP_Player *pPlayer )
{
	const int start = random->RandomInt( 0, ARRAYSIZE( s_Classes ) - 1 );
	const SgClassInfo_t *pBest = NULL;
	int bestCount = INT_MAX;

	for ( int offset = 0; offset < ARRAYSIZE( s_Classes ); offset++ )
	{
		const SgClassInfo_t *pClass = &s_Classes[( start + offset ) % ARRAYSIZE( s_Classes )];
		if ( pClass->team != team )
			continue;

		if ( !Sg_ClassHasRoom( pClass, pPlayer ) )
			continue;

		const int count = Sg_CountPlayersUsingClass( pClass, pPlayer );
		if ( count < bestCount )
		{
			pBest = pClass;
			bestCount = count;
		}
	}

	return pBest;
}

static const SgClassInfo_t *Sg_GetStoredClass( CHL2MP_Player *pPlayer )
{
	if ( !pPlayer )
		return NULL;

	const int idx = pPlayer->entindex();
	if ( idx < 1 || idx > MAX_PLAYERS )
		return NULL;

	const SgClassInfo_t *pClass = Sg_FindClass( s_PlayerClass[idx] );
	if ( pClass && pClass->team == pPlayer->GetTeamNumber() )
		return pClass;

	return NULL;
}

//-----------------------------------------------------------------------------
// HUD message senders (client side: CHudStargate::MsgFunc_*).
//-----------------------------------------------------------------------------
void Sg_SendAboutMe( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "SgAboutMe" );
		WRITE_SHORT( pPlayer->entindex() );
		WRITE_SHORT( pPlayer->GetTeamNumber() );
	MessageEnd();
}

void Sg_SendAddPortrait( CBasePlayer *pPlayer, const char *avatar )
{
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter filter( pPlayer );
	filter.MakeReliable();
	UserMessageBegin( filter, "SgAddPortrait" );
		WRITE_SHORT( pPlayer->entindex() );
		WRITE_SHORT( pPlayer->GetTeamNumber() );
		WRITE_STRING( avatar ? avatar : "" );
	MessageEnd();
}

void Sg_ResetPlayerClass( CHL2MP_Player *pPlayer )
{
	if ( !pPlayer )
		return;

	const int idx = pPlayer->entindex();
	if ( idx < 1 || idx > MAX_PLAYERS )
		return;

	s_PlayerClass[idx][0] = '\0';
	pPlayer->SetStargateClass( 0 );
}

//-----------------------------------------------------------------------------
// Apply the stored class. Safe to call from spawn and from joinclass.
//-----------------------------------------------------------------------------
void Sg_ApplyClass( CHL2MP_Player *pPlayer, bool bResetLoadout )
{
	if ( !pPlayer )
		return;

	Sg_SendAboutMe( pPlayer );

	const SgClassInfo_t *pClass = Sg_GetStoredClass( pPlayer );
	if ( !pClass )
	{
		pPlayer->SetStargateClass( 0 );
		return;
	}

	pPlayer->SetStargateClass( pClass->slot );
	pPlayer->SetModel( pClass->model );
	pPlayer->SetupPlayerSoundsByModel( pClass->model );

	Sg_SendAddPortrait( pPlayer, pClass->avatar );

	if ( !pPlayer->IsAlive() )
		return;

	if ( bResetLoadout )
	{
		pPlayer->RemoveAllItems( true );
		pPlayer->GiveDefaultItems();
	}

	if ( mp_wepselmode.GetInt() <= 0 || !pClass->weapon[0] )
		return;

	if ( !pPlayer->Weapon_OwnsThisType( pClass->weapon ) )
		pPlayer->GiveNamedItem( pClass->weapon );

	CBaseCombatWeapon *pWeapon = pPlayer->Weapon_OwnsThisType( pClass->weapon );
	if ( pWeapon )
		pPlayer->Weapon_Switch( pWeapon );
}

//-----------------------------------------------------------------------------
// `joinclass <name>`, `joinclass auto`, and legacy `classe <1..5>`.
//-----------------------------------------------------------------------------
void Sg_HandleJoinClass( CHL2MP_Player *pPlayer, const char *pszClass )
{
	if ( !pPlayer )
		return;

	const int idx = pPlayer->entindex();
	if ( idx < 1 || idx > MAX_PLAYERS )
		return;

	const SgClassInfo_t *pClass = NULL;
	const char *pClassArg = pszClass ? pszClass : "";

	if ( !Q_stricmp( pClassArg, "auto" ) )
	{
		if ( pPlayer->GetTeamNumber() != StargateTC::TEAM_TORRI && pPlayer->GetTeamNumber() != StargateTC::TEAM_GOAULD )
			pPlayer->HandleCommand_JoinTeam( 0 );

		pClass = Sg_FindAutoClass( pPlayer->GetTeamNumber(), pPlayer );
	}
	else if ( pClassArg[0] >= '0' && pClassArg[0] <= '9' )
	{
		pClass = Sg_FindClassBySlot( pPlayer->GetTeamNumber(), atoi( pClassArg ) );
	}
	else
	{
		pClass = Sg_FindClass( pClassArg );
	}

	if ( !pClass )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "joinclass: unknown class\n" );
		return;
	}

	if ( pPlayer->GetTeamNumber() != pClass->team )
	{
		if ( !pPlayer->HandleCommand_JoinTeam( pClass->team ) )
			return;
	}

	if ( !Sg_ClassHasRoom( pClass, pPlayer ) )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "That class is full." );
		return;
	}

	Q_strncpy( s_PlayerClass[idx], pClass->name, sizeof( s_PlayerClass[idx] ) );

	char msg[ 96 ];
	Q_snprintf( msg, sizeof( msg ), "Class: %s\n", pClass->display );
	ClientPrint( pPlayer, HUD_PRINTTALK, msg );

	Sg_ApplyClass( pPlayer, true );
}

//-----------------------------------------------------------------------------
// Called from CHL2MP_Player::Spawn so the class loadout/HUD survive respawns.
//-----------------------------------------------------------------------------
void Sg_OnPlayerSpawn( CHL2MP_Player *pPlayer )
{
	Sg_ApplyClass( pPlayer, false );
}
