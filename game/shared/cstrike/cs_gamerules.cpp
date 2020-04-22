//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cs_gamerules.h"
#include "cs_ammodef.h"
#include "weapon_csbase.h"
#include "cs_shareddefs.h"
#include "KeyValues.h"
#include "cs_achievement_constants.h"
#include "fmtstr.h"

#ifdef CLIENT_DLL

	#include "networkstringtable_clientdll.h"
	#include "utlvector.h"

#else
	
	#include "bot.h"
	#include "utldict.h"
	#include "cs_player.h"
	#include "cs_team.h"
	#include "cs_gamerules.h"
	#include "voice_gamemgr.h"
	#include "igamesystem.h"
	#include "weapon_c4.h"
	#include "mapinfo.h"
	#include "shake.h"
	#include "mapentities.h"
	#include "game.h"
	#include "cs_simple_hostage.h"
	#include "cs_gameinterface.h"
	#include "player_resource.h"
	#include "info_view_parameters.h"
	#include "cs_bot_manager.h"
	#include "cs_bot.h"
	#include "eventqueue.h"
	#include "fmtstr.h"
	#include "teamplayroundbased_gamerules.h"
	#include "gameweaponmanager.h"

	#include "cs_gamestats.h"
	#include "cs_urlretrieveprices.h"
	#include "networkstringtable_gamedll.h"
	#include "player_resource.h"
	#include "cs_player_resource.h"
	
#if defined( REPLAY_ENABLED )	
	#include "replay/ireplaysystem.h"
	#include "replay/iserverreplaycontext.h"
	#include "replay/ireplaysessionrecorder.h"
#endif // REPLAY_ENABLED

#endif


#include "cs_blackmarket.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifndef CLIENT_DLL


#define CS_GAME_STATS_UPDATE 79200 //22 hours
#define CS_GAME_STATS_UPDATE_PERIOD 7200 // 2 hours

extern IUploadGameStats *gamestatsuploader;

#if defined( REPLAY_ENABLED )
extern IReplaySystem *g_pReplay;
#endif // REPLAY_ENABLED

#endif


/**
 * Player hull & eye position for standing, ducking, etc.  This version has a taller
 * player height, but goldsrc-compatible collision bounds.
 */
static CViewVectors g_CSViewVectors(
	Vector( 0, 0, 64 ),		// eye position

	Vector(-16, -16, 0 ),	// hull min
	Vector( 16,  16, 62 ),	// hull max

	Vector(-16, -16, 0 ),	// duck hull min
	Vector( 16,  16, 45 ),	// duck hull max
	Vector( 0, 0, 47 ),		// duck view

	Vector(-10, -10, -10 ),	// observer hull min
	Vector( 10,  10,  10 ),	// observer hull max

	Vector( 0, 0, 14 )		// dead view height
);


#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS(info_player_terrorist, CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_counterterrorist,CPointEntity);
LINK_ENTITY_TO_CLASS(info_player_logo,CPointEntity);
#endif

REGISTER_GAMERULES_CLASS( CCSGameRules );


BEGIN_NETWORK_TABLE_NOBASE( CCSGameRules, DT_CSGameRules )
	#ifdef CLIENT_DLL
		RecvPropBool( RECVINFO( m_bFreezePeriod ) ),
		RecvPropInt( RECVINFO( m_iRoundTime ) ),
		RecvPropFloat( RECVINFO( m_fRoundStartTime ) ),
		RecvPropFloat( RECVINFO( m_flGameStartTime ) ),
		RecvPropInt( RECVINFO( m_iHostagesRemaining ) ),
		RecvPropBool( RECVINFO( m_bMapHasBombTarget ) ),
		RecvPropBool( RECVINFO( m_bMapHasRescueZone ) ),
		RecvPropBool( RECVINFO( m_bLogoMap ) ),
		RecvPropBool( RECVINFO( m_bBlackMarket ) )
	#else
		SendPropBool( SENDINFO( m_bFreezePeriod ) ),
		SendPropInt( SENDINFO( m_iRoundTime ), 16 ),
		SendPropFloat( SENDINFO( m_fRoundStartTime ), 32, SPROP_NOSCALE ),
		SendPropFloat( SENDINFO( m_flGameStartTime ), 32, SPROP_NOSCALE ),
		SendPropInt( SENDINFO( m_iHostagesRemaining ), 4 ),
		SendPropBool( SENDINFO( m_bMapHasBombTarget ) ),
		SendPropBool( SENDINFO( m_bMapHasRescueZone ) ),
		SendPropBool( SENDINFO( m_bLogoMap ) ),
		SendPropBool( SENDINFO( m_bBlackMarket ) )
	#endif
END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS( cs_gamerules, CCSGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( CSGameRulesProxy, DT_CSGameRulesProxy )


#ifdef CLIENT_DLL
	void RecvProxy_CSGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CCSGameRules *pRules = CSGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CCSGameRulesProxy, DT_CSGameRulesProxy )
		RecvPropDataTable( "cs_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_CSGameRules ), RecvProxy_CSGameRules )
	END_RECV_TABLE()
#else
	void* SendProxy_CSGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CCSGameRules *pRules = CSGameRules();
		Assert( pRules );
		return pRules;
	}

	BEGIN_SEND_TABLE( CCSGameRulesProxy, DT_CSGameRulesProxy )
		SendPropDataTable( "cs_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_CSGameRules ), SendProxy_CSGameRules )
	END_SEND_TABLE()
#endif



ConVar ammo_50AE_max( "ammo_50AE_max", "35", FCVAR_REPLICATED );
ConVar ammo_762mm_max( "ammo_762mm_max", "90", FCVAR_REPLICATED );
ConVar ammo_556mm_max( "ammo_556mm_max", "90", FCVAR_REPLICATED );
ConVar ammo_556mm_box_max( "ammo_556mm_box_max", "200", FCVAR_REPLICATED );
ConVar ammo_338mag_max( "ammo_338mag_max", "30", FCVAR_REPLICATED );
ConVar ammo_9mm_max( "ammo_9mm_max", "120", FCVAR_REPLICATED );
ConVar ammo_buckshot_max( "ammo_buckshot_max", "32", FCVAR_REPLICATED );
ConVar ammo_45acp_max( "ammo_45acp_max", "100", FCVAR_REPLICATED );
ConVar ammo_357sig_max( "ammo_357sig_max", "52", FCVAR_REPLICATED );
ConVar ammo_57mm_max( "ammo_57mm_max", "100", FCVAR_REPLICATED );
ConVar ammo_hegrenade_max( "ammo_hegrenade_max", "1", FCVAR_REPLICATED );
ConVar ammo_flashbang_max( "ammo_flashbang_max", "2", FCVAR_REPLICATED );
ConVar ammo_smokegrenade_max( "ammo_smokegrenade_max", "1", FCVAR_REPLICATED );

//ConVar mp_dynamicpricing( "mp_dynamicpricing", "0", FCVAR_REPLICATED, "Enables or Disables the dynamic weapon prices" );


extern ConVar sv_stopspeed;

ConVar mp_buytime( 
	"mp_buytime", 
	"1.5",
	FCVAR_REPLICATED,
	"How many minutes after round start players can buy items for.",
	true, 0.25,
	false, 0 );

ConVar mp_playerid(
	"mp_playerid",
	"0",
	FCVAR_REPLICATED,
	"Controls what information player see in the status bar: 0 all names; 1 team names; 2 no names",
	true, 0,
	true, 2 );

ConVar mp_playerid_delay(
	"mp_playerid_delay",
	"0.5",
	FCVAR_REPLICATED,
	"Number of seconds to delay showing information in the status bar",
	true, 0,
	true, 1 );

ConVar mp_playerid_hold(
	"mp_playerid_hold",
	"0.25",
	FCVAR_REPLICATED,
	"Number of seconds to keep showing old information in the status bar",
	true, 0,
	true, 1 );

ConVar mp_round_restart_delay(
	"mp_round_restart_delay",
	"5.0",
	FCVAR_REPLICATED,
	"Number of seconds to delay before restarting a round after a win",
	true, 0.0f,
	true, 10.0f );

ConVar sv_allowminmodels(
	"sv_allowminmodels",
	"1",
	FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Allow or disallow the use of cl_minmodels on this server." );

#ifdef CLIENT_DLL

ConVar cl_autowepswitch(
	"cl_autowepswitch",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Automatically switch to picked up weapons (if more powerful)" );

ConVar cl_autohelp(
	"cl_autohelp",
	"1",
	FCVAR_ARCHIVE | FCVAR_USERINFO,
	"Auto-help" );

#else

	// longest the intermission can last, in seconds
	#define MAX_INTERMISSION_TIME 120

	// Falling damage stuff.
	#define CS_PLAYER_FATAL_FALL_SPEED		1100	// approx 60 feet
	#define CS_PLAYER_MAX_SAFE_FALL_SPEED	580		// approx 20 feet
	#define CS_DAMAGE_FOR_FALL_SPEED		((float)100 / ( CS_PLAYER_FATAL_FALL_SPEED - CS_PLAYER_MAX_SAFE_FALL_SPEED )) // damage per unit per second.

	// These entities are preserved each round restart. The rest are removed and recreated.
	static const char *s_PreserveEnts[] =
	{
		"ai_network",
		"ai_hint",
		"cs_gamerules",
		"cs_team_manager",
		"cs_player_manager",
		"env_soundscape",
		"env_soundscape_proxy",
		"env_soundscape_triggerable",
		"env_sun",
		"env_wind",
		"env_fog_controller",
		"func_brush",
		"func_wall",
		"func_buyzone",
		"func_illusionary",
		"func_hostage_rescue",
		"func_bomb_target",
		"infodecal",
		"info_projecteddecal",
		"info_node",
		"info_target",
		"info_node_hint",
		"info_player_counterterrorist",
		"info_player_terrorist",
		"info_map_parameters",
		"keyframe_rope",
		"move_rope",
		"info_ladder",
		"player",
		"point_viewcontrol",
		"scene_manager",
		"shadow_control",
		"sky_camera",
		"soundent",
		"trigger_soundscape",
		"viewmodel",
		"predicted_viewmodel",
		"worldspawn",
		"point_devshot_camera",
		"", // END Marker
	};


	// --------------------------------------------------------------------------------------------------- //
	// Voice helper
	// --------------------------------------------------------------------------------------------------- //

	class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
	{
	public:
		virtual bool		CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity )
		{
			// Dead players can only be heard by other dead team mates
			if ( pTalker->IsAlive() == false )
			{
				if ( pListener->IsAlive() == false )
					return ( pListener->InSameTeam( pTalker ) );

				return false;
			}

			return ( pListener->InSameTeam( pTalker ) );
		}
	};
	CVoiceGameMgrHelper g_VoiceGameMgrHelper;
	IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;



	// --------------------------------------------------------------------------------------------------- //
	// Globals.
	// --------------------------------------------------------------------------------------------------- //

	// NOTE: the indices here must match TEAM_TERRORIST, TEAM_CT, TEAM_SPECTATOR, etc.
	const char *sTeamNames[] =
	{
		"Unassigned",
		"Spectator",
		"TERRORIST",
		"CT"
	};

	extern ConVar mp_maxrounds;

	ConVar mp_startmoney( 
		"mp_startmoney", 
		"800", 
		FCVAR_REPLICATED | FCVAR_NOTIFY,
		"amount of money each player gets when they reset",
		true, 800,
		true, 16000 );	

	ConVar mp_roundtime( 
		"mp_roundtime",
		"2.5",
		FCVAR_REPLICATED | FCVAR_NOTIFY,
		"How many minutes each round takes.",
		true, 1,	// min value
		true, 9		// max value
		);

	ConVar mp_freezetime( 
		"mp_freezetime",
		"6",
		FCVAR_REPLICATED | FCVAR_NOTIFY,
		"how many seconds to keep players frozen when the round starts",
		true, 0,	// min value
		true, 60	// max value
		);

	ConVar mp_c4timer( 
		"mp_c4timer", 
		"45", 
		FCVAR_REPLICATED | FCVAR_NOTIFY,
		"how long from when the C4 is armed until it blows",
		true, 10,	// min value
		true, 90	// max value
		);

	ConVar mp_limitteams( 
		"mp_limitteams", 
		"2", 
		FCVAR_REPLICATED | FCVAR_NOTIFY,
		"Max # of players 1 team can have over another (0 disables check)",
		true, 0,	// min value
		true, 30	// max value
		);

	ConVar mp_tkpunish( 
		"mp_tkpunish", 
		"0", 
		FCVAR_REPLICATED,
		"Will a TK'er be punished in the next round?  {0=no,  1=yes}" );

	ConVar mp_autokick(
		"mp_autokick",
		"1",
		FCVAR_REPLICATED,
		"Kick idle/team-killing players" );

	ConVar mp_spawnprotectiontime(
		"mp_spawnprotectiontime",
		"5",
		FCVAR_REPLICATED,
		"Kick players who team-kill within this many seconds of a round restart." );

	ConVar mp_humanteam( 
		"mp_humanteam", 
		"any", 
		FCVAR_REPLICATED,
		"Restricts human players to a single team {any, CT, T}" );

	ConVar mp_ignore_round_win_conditions(
		"mp_ignore_round_win_conditions",
		"0",
		FCVAR_REPLICATED,
		"Ignore conditions which would end the current round");

	ConCommand EndRound( "endround", &CCSGameRules::EndRound, "End the current round.", FCVAR_CHEAT );


	// --------------------------------------------------------------------------------------------------- //
	// Global helper functions.
	// --------------------------------------------------------------------------------------------------- //

	void InitBodyQue(void)
	{
		// FIXME: Make this work
	}


	Vector DropToGround( 
		CBaseEntity *pMainEnt, 
		const Vector &vPos, 
		const Vector &vMins, 
		const Vector &vMaxs )
	{
		trace_t trace;
		UTIL_TraceHull( vPos, vPos + Vector( 0, 0, -500 ), vMins, vMaxs, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &trace );
		return trace.endpos;
	}


	//-----------------------------------------------------------------------------
	// Purpose: This function can be used to find a valid placement location for an entity.
	//			Given an origin to start looking from and a minimum radius to place the entity at,
	//			it will sweep out a circle around vOrigin and try to find a valid spot (on the ground)
	//			where mins and maxs will fit.
	// Input  : *pMainEnt - Entity to place
	//			&vOrigin - Point to search around
	//			fRadius - Radius to search within
	//			nTries - Number of tries to attempt
	//			&mins - mins of the Entity
	//			&maxs - maxs of the Entity
	//			&outPos - Return point
	// Output : Returns true and fills in outPos if it found a spot.
	//-----------------------------------------------------------------------------
	bool EntityPlacementTest( CBaseEntity *pMainEnt, const Vector &vOrigin, Vector &outPos, bool bDropToGround )
	{
		// This function moves the box out in each dimension in each step trying to find empty space like this:
		//
		//											  X  
		//							   X			  X  
		// Step 1:   X     Step 2:    XXX   Step 3: XXXXX
		//							   X 			  X  
		//											  X  
		//
			 
		Vector mins, maxs;
		pMainEnt->CollisionProp()->WorldSpaceAABB( &mins, &maxs );
		mins -= pMainEnt->GetAbsOrigin();
		maxs -= pMainEnt->GetAbsOrigin();

		// Put some padding on their bbox.
		float flPadSize = 5;
		Vector vTestMins = mins - Vector( flPadSize, flPadSize, flPadSize );
		Vector vTestMaxs = maxs + Vector( flPadSize, flPadSize, flPadSize );

		// First test the starting origin.
		if ( UTIL_IsSpaceEmpty( pMainEnt, vOrigin + vTestMins, vOrigin + vTestMaxs ) )
		{
			if ( bDropToGround )
			{
				outPos = DropToGround( pMainEnt, vOrigin, vTestMins, vTestMaxs );
			}
			else
			{
				outPos = vOrigin;
			}
			return true;
		}

		Vector vDims = vTestMaxs - vTestMins;

		// Keep branching out until we get too far.
		int iCurIteration = 0;
		int nMaxIterations = 15;
		
		int offset = 0;
		do
		{
			for ( int iDim=0; iDim < 3; iDim++ )
			{
				float flCurOffset = offset * vDims[iDim];

				for ( int iSign=0; iSign < 2; iSign++ )
				{
					Vector vBase = vOrigin;
					vBase[iDim] += (iSign*2-1) * flCurOffset;
				
					if ( UTIL_IsSpaceEmpty( pMainEnt, vBase + vTestMins, vBase + vTestMaxs ) )
					{
						// Ensure that there is a clear line of sight from the spawnpoint entity to the actual spawn point.
						// (Useful for keeping things from spawning behind walls near a spawn point)
						trace_t tr;
						UTIL_TraceLine( vOrigin, vBase, MASK_SOLID, pMainEnt, COLLISION_GROUP_NONE, &tr );

						if ( tr.fraction != 1.0 )
						{
							continue;
						}
						
						if ( bDropToGround )
							outPos = DropToGround( pMainEnt, vBase, vTestMins, vTestMaxs );
						else
							outPos = vBase;

						return true;
					}
				}
			}

			++offset;
		} while ( iCurIteration++ < nMaxIterations );

	//	Warning( "EntityPlacementTest for ent %d:%s failed!\n", pMainEnt->entindex(), pMainEnt->GetClassname() );
		return false;
	}

	int UTIL_HumansInGame( bool ignoreSpectators )
	{
		int iCount = 0;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *entity = CCSPlayer::Instance( i );

			if ( entity && !FNullEnt( entity->edict() ) )
			{
				if ( FStrEq( entity->GetPlayerName(), "" ) )
					continue;

				if ( FBitSet( entity->GetFlags(), FL_FAKECLIENT ) )
					continue;

				if ( ignoreSpectators && entity->GetTeamNumber() != TEAM_TERRORIST && entity->GetTeamNumber() != TEAM_CT )
					continue;

				if ( ignoreSpectators && entity->State_Get() == STATE_PICKINGCLASS )
					continue;

				iCount++;
			}
		}

		return iCount;
	}

	// --------------------------------------------------------------------------------------------------- //
	// CCSGameRules implementation.
	// --------------------------------------------------------------------------------------------------- //

	CCSGameRules::CCSGameRules()
	{
		m_iRoundTime = 0;
		m_iRoundWinStatus = WINNER_NONE;
		m_iFreezeTime = 0;

		m_fRoundStartTime = 0;
		m_bAllowWeaponSwitch = true;
		m_bFreezePeriod = true;
		m_iNumTerrorist = m_iNumCT = 0;	// number of players per team
		m_flRestartRoundTime = 0.1f; // restart first round as soon as possible
		m_iNumSpawnableTerrorist = m_iNumSpawnableCT = 0;
		m_bFirstConnected = false;
		m_bCompleteReset = false;
		m_iAccountTerrorist = m_iAccountCT = 0;
		m_iNumCTWins = 0;
		m_iNumTerroristWins = 0;
		m_iNumConsecutiveCTLoses = 0;
		m_iNumConsecutiveTerroristLoses = 0;
		m_bTargetBombed = false;
		m_bBombDefused = false;
		m_iTotalRoundsPlayed = -1;
		m_iUnBalancedRounds = 0;
		m_flGameStartTime = 0;
		m_iHostagesRemaining = 0;
		m_bLevelInitialized = false;
		m_bLogoMap = false;
		m_tmNextPeriodicThink = 0;

		m_bMapHasBombTarget = false;
		m_bMapHasRescueZone = false;

		m_iSpawnPointCount_Terrorist = 0;
		m_iSpawnPointCount_CT = 0;

		m_bTCantBuy = false;
		m_bCTCantBuy = false;
		m_bMapHasBuyZone = false;

		m_iLoserBonus = 0;

		m_iHostagesRescued = 0;
		m_iHostagesTouched = 0;
		m_flNextHostageAnnouncement = 0.0f;

        //=============================================================================
        // HPE_BEGIN
        // [dwenger] Reset rescue-related achievement values
        //=============================================================================

		// [tj] reset flawless and lossless round related flags
		m_bNoTerroristsKilled = true;
		m_bNoCTsKilled = true;
		m_bNoTerroristsDamaged = true;
		m_bNoCTsDamaged = true;
		m_pFirstKill = NULL;
		m_firstKillTime = 0;

		// [menglish] Reset fun fact values
		m_pFirstBlood = NULL;
		m_firstBloodTime = 0;

        m_bCanDonateWeapons = true;

		// [dwenger] Reset rescue-related achievement values
        m_pLastRescuer = NULL;
        m_iNumRescuers = 0;

		m_hostageWasInjured = false;
		m_hostageWasKilled = false;

		m_pFunFactManager = new CCSFunFactMgr();
		m_pFunFactManager->Init();

        //=============================================================================
        // HPE_END
        //=============================================================================

		m_iHaveEscaped = 0;
		m_bMapHasEscapeZone = false;
		m_iNumEscapers = 0;
		m_iNumEscapeRounds = 0;

		m_iMapHasVIPSafetyZone = 0;
		m_pVIP = NULL;
		m_iConsecutiveVIP = 0;

		m_bMapHasBombZone = false;
		m_bBombDropped = false;
		m_bBombPlanted = false;
		m_pLastBombGuy = NULL;

		m_bAllowWeaponSwitch = true;

		m_flNextHostageAnnouncement = gpGlobals->curtime;	// asap.

		ReadMultiplayCvars();

		m_pPrices = NULL;
		m_bBlackMarket = false;
		m_bDontUploadStats = false;

		// Create the team managers
		for ( int i = 0; i < ARRAYSIZE( sTeamNames ); i++ )
		{
			CTeam *pTeam = static_cast<CTeam*>(CreateEntityByName( "cs_team_manager" ));
			pTeam->Init( sTeamNames[i], i );

			g_Teams.AddToTail( pTeam );
		}

		if ( filesystem->FileExists( UTIL_VarArgs( "maps/cfg/%s.cfg", STRING(gpGlobals->mapname) ) ) )
		{
			// Execute a map specific cfg file - as in Day of Defeat
			// Map names cannot contain quotes or control characters so this is safe but silly that we have to do it.
			engine->ServerCommand( UTIL_VarArgs( "exec \"%s.cfg\" */maps\n", STRING(gpGlobals->mapname) ) );
			engine->ServerExecute();
		}

#ifndef CLIENT_DLL
		// stats

		if ( g_flGameStatsUpdateTime == 0.0f )
		{
			memset( g_iWeaponPurchases, 0, sizeof( g_iWeaponPurchases) );
			memset( g_iTerroristVictories, 0, sizeof( g_iTerroristVictories) );
			memset( g_iCounterTVictories, 0, sizeof( g_iTerroristVictories) );
			g_flGameStatsUpdateTime = CS_GAME_STATS_UPDATE; //Next update is between 22 and 24 hours.
		}
#endif
	}

	void CCSGameRules::AddPricesToTable( weeklyprice_t prices )
	{
		int iIndex = m_StringTableBlackMarket->FindStringIndex( "blackmarket_prices" );

		if ( iIndex == INVALID_STRING_INDEX )
		{
			m_StringTableBlackMarket->AddString( CBaseEntity::IsServer(), "blackmarket_prices", sizeof( weeklyprice_t), &prices );
		}
		else
		{
			m_StringTableBlackMarket->SetStringUserData( iIndex, sizeof( weeklyprice_t), &prices );
		}

		SetBlackMarketPrices( false );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CCSGameRules::~CCSGameRules()
	{
		// Note, don't delete each team since they are in the gEntList and will 
		// automatically be deleted from there, instead.
		g_Teams.Purge();
		if( m_pFunFactManager )
		{
			delete m_pFunFactManager;
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CCSGameRules::UpdateClientData( CBasePlayer *player )
	{
	}

	//-----------------------------------------------------------------------------
	// Purpose: TF2 Specific Client Commands
	// Input  :
	// Output :
	//-----------------------------------------------------------------------------
	bool CCSGameRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
	{
		CCSPlayer *pPlayer = ToCSPlayer( pEdict );

		if ( FStrEq( args[0], "changeteam" ) )
		{
			return true;
		}
		else if ( FStrEq( args[0], "nextmap" ) )
		{
			if ( pPlayer->m_iNextTimeCheck < gpGlobals->curtime )
			{
				char szNextMap[32];

				if ( nextlevel.GetString() && *nextlevel.GetString() )
				{
					Q_strncpy( szNextMap, nextlevel.GetString(), sizeof( szNextMap ) );
				}
				else
				{
					GetNextLevelName( szNextMap, sizeof( szNextMap ) );
				}

				ClientPrint( pPlayer, HUD_PRINTTALK, "#game_nextmap", szNextMap);

				pPlayer->m_iNextTimeCheck = gpGlobals->curtime + 1;
			}
			return true;
		}
		else if( pPlayer->ClientCommand( args ) )
		{
			return true;
		}
		else if( BaseClass::ClientCommand( pEdict, args ) )
		{
			return true;
		}
		else if ( TheBots->ServerCommand( args.GetCommandString() ) )
		{
			return true;
		}
		else
		{
			return TheBots->ClientCommand( pPlayer, args );
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------
	void CCSGameRules::ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer * >( CBaseEntity::Instance( pEntity ) );
		if ( pPlayer )
		{
			char const *pszCommand = pKeyValues->GetName();
			if ( pszCommand && pszCommand[0] )
			{
				if ( FStrEq( pszCommand, "ClanTagChanged" ) )
				{
					pPlayer->SetClanTag( pKeyValues->GetString( "tag", "" ) );

					const char *teamName = "UNKNOWN";
					if ( pPlayer->GetTeam() )
					{
						teamName = pPlayer->GetTeam()->GetName();
					}
					UTIL_LogPrintf("\"%s<%i><%s><%s>\" triggered \"clantag\" (value \"%s\")\n", 
						pPlayer->GetPlayerName(),
						pPlayer->GetUserID(),
						pPlayer->GetNetworkIDString(),
						teamName,
						pKeyValues->GetString( "tag", "unknown" ) );
				}
			}
		}

		BaseClass::ClientCommandKeyValues( pEntity, pKeyValues );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------
	void CCSGameRules::PlayerSpawn( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = ToCSPlayer( pBasePlayer );
		if ( !pPlayer )
			Error( "PlayerSpawn" );

		if ( pPlayer->State_Get() != STATE_ACTIVE )
			return;

		pPlayer->EquipSuit();
		
		bool addDefault = true;

		CBaseEntity	*pWeaponEntity = NULL;
		while ( ( pWeaponEntity = gEntList.FindEntityByClassname( pWeaponEntity, "game_player_equip" )) != NULL )
		{
			if ( addDefault )
			{
				// remove all our weapons and armor before touching the first game_player_equip
				pPlayer->RemoveAllItems( true );
			}
			pWeaponEntity->Touch( pPlayer );
			addDefault = false;
		}


		if ( addDefault || pPlayer->m_bIsVIP )
			pPlayer->GiveDefaultItems();
	}

	void CCSGameRules::BroadcastSound( const char *sound, int team )
	{
		CBroadcastRecipientFilter filter;
		filter.MakeReliable();

		if( team != -1 )
		{
			filter.RemoveAllRecipients();
			filter.AddRecipientsByTeam( GetGlobalTeam(team) );
		}

		UserMessageBegin ( filter, "SendAudio" );
			WRITE_STRING( sound );
		MessageEnd();
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------

	// return a multiplier that should adjust the damage done by a blast at position vecSrc to something at the position
	// vecEnd.  This will take into account the density of an entity that blocks the line of sight from one position to
	// the other.
	//
	// this algorithm was taken from the HL2 version of RadiusDamage.
	float CCSGameRules::GetExplosionDamageAdjustment(Vector & vecSrc, Vector & vecEnd, CBaseEntity *pEntityToIgnore)
	{
		float retval = 0.0;
		trace_t tr;

		UTIL_TraceLine(vecSrc, vecEnd, MASK_SHOT, pEntityToIgnore, COLLISION_GROUP_NONE, &tr);
		if (tr.fraction == 1.0)
		{
			retval = 1.0;
		}
		else if (!(tr.DidHitWorld()) && (tr.m_pEnt != NULL) && (tr.m_pEnt != pEntityToIgnore) && (tr.m_pEnt->GetOwnerEntity() != pEntityToIgnore))
		{
			// if we didn't hit world geometry perhaps there's still damage to be done here.

			CBaseEntity *blockingEntity = tr.m_pEnt;

			// check to see if this part of the player is visible if entities are ignored.
			UTIL_TraceLine(vecSrc, vecEnd, CONTENTS_SOLID, NULL, COLLISION_GROUP_NONE, &tr);

			if (tr.fraction == 1.0)
			{
				if ((blockingEntity != NULL) && (blockingEntity->VPhysicsGetObject() != NULL))
				{
					int nMaterialIndex = blockingEntity->VPhysicsGetObject()->GetMaterialIndex();

					float flDensity;
					float flThickness;
					float flFriction;
					float flElasticity;

					physprops->GetPhysicsProperties( nMaterialIndex, &flDensity,
						&flThickness, &flFriction, &flElasticity );

					const float DENSITY_ABSORB_ALL_DAMAGE = 3000.0;
					float scale = flDensity / DENSITY_ABSORB_ALL_DAMAGE;
					if ((scale >= 0.0) && (scale < 1.0))
					{
						retval = 1.0 - scale;
					}
					else if (scale < 0.0)
					{
						// should never happen, but just in case.
						retval = 1.0;
					}
				}
				else
				{
					retval = 0.75; // we're blocked by something that isn't an entity with a physics module or world geometry, just cut damage in half for now.
				}
			}
		}

		return retval;
	}

	// returns the percentage of the player that is visible from the given point in the world.
	// return value is between 0 and 1.
	float CCSGameRules::GetAmountOfEntityVisible(Vector & vecSrc, CBaseEntity *entity)
	{
		float retval = 0.0;

		const float damagePercentageChest = 0.40;
		const float damagePercentageHead = 0.20;
		const float damagePercentageFeet = 0.20;
		const float damagePercentageRightSide = 0.10;
		const float damagePercentageLeftSide = 0.10;

		if (!(entity->IsPlayer()))
		{
			// the entity is not a player, so the damage is all or nothing.
			Vector vecTarget;
			vecTarget = entity->BodyTarget(vecSrc, false);

			return GetExplosionDamageAdjustment(vecSrc, vecTarget, entity);
		}

		CCSPlayer *player = (CCSPlayer *)entity;

		// check what parts of the player we can see from this point and modify the return value accordingly.
		float chestHeightFromFeet;

		float armDistanceFromChest = HalfHumanWidth;

		// calculate positions of various points on the target player's body
		Vector vecFeet = player->GetAbsOrigin();

		Vector vecChest = player->BodyTarget(vecSrc, false);
		chestHeightFromFeet = vecChest.z - vecFeet.z;  // compute the distance from the chest to the feet. (this accounts for ducking and the like)

		Vector vecHead = player->GetAbsOrigin();
		vecHead.z += HumanHeight;

		Vector vecRightFacing;
		AngleVectors(player->GetAbsAngles(), NULL, &vecRightFacing, NULL);

		vecRightFacing.NormalizeInPlace();
		vecRightFacing = vecRightFacing * armDistanceFromChest;

		Vector vecLeftSide = player->GetAbsOrigin();
		vecLeftSide.x -= vecRightFacing.x;
		vecLeftSide.y -= vecRightFacing.y;
		vecLeftSide.z += chestHeightFromFeet;

		Vector vecRightSide = player->GetAbsOrigin();
		vecRightSide.x += vecRightFacing.x;
		vecRightSide.y += vecRightFacing.y;
		vecRightSide.z += chestHeightFromFeet;

		// check chest
		float damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecChest, entity);
		retval += (damagePercentageChest * damageAdjustment);

		// check top of head
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecHead, entity);
		retval += (damagePercentageHead * damageAdjustment);

		// check feet
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecFeet, entity);
		retval += (damagePercentageFeet * damageAdjustment);

		// check left "edge"
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecLeftSide, entity);
		retval += (damagePercentageLeftSide * damageAdjustment);

		// check right "edge"
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecRightSide, entity);
		retval += (damagePercentageRightSide * damageAdjustment);

		return retval;
	}

	void CCSGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity * pEntityIgnore )
	{
		RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore, false );
	}

	// Add the ability to ignore the world trace
	void CCSGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld )
	{
		CBaseEntity *pEntity = NULL;
		trace_t		tr;
		float		falloff, damagePercentage;
		Vector		vecSpot;
		Vector		vecToTarget;
		Vector		vecEndPos;

        //=============================================================================
        // HPE_BEGIN:        
        //=============================================================================
         
		// [tj] The number of enemy players this explosion killed
        int numberOfEnemyPlayersKilledByThisExplosion = 0;
		
		// [tj] who we award the achievement to if enough players are killed
		CCSPlayer* pCSExplosionAttacker = ToCSPlayer(info.GetAttacker());

		// [tj] used to determine which achievement to award for sufficient kills
		CBaseEntity* pInflictor = info.GetInflictor();
		bool isGrenade = pInflictor && V_strcmp(pInflictor->GetClassname(), "hegrenade_projectile") == 0;
		bool isBomb = pInflictor && V_strcmp(pInflictor->GetClassname(), "planted_c4") == 0;
         
        //=============================================================================
        // HPE_END
        //=============================================================================
        

		vecEndPos.Init();

		Vector vecSrc = vecSrcIn;

		damagePercentage = 1.0;

		if ( flRadius )
			falloff = info.GetDamage() / flRadius;
		else
			falloff = 1.0;

		int bInWater = (UTIL_PointContents ( vecSrc ) & MASK_WATER) ? true : false;
		
		vecSrc.z += 1;// in case grenade is lying on the ground

		// iterate on all entities in the vicinity.
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
		{
			//=============================================================================
			// HPE_BEGIN:
			// [tj] We have to save whether or not the player is killed so we don't give credit 
			//		for pre-dead players.
			//=============================================================================
			bool wasAliveBeforeExplosion = false;
			CCSPlayer* pCSExplosionVictim = ToCSPlayer(pEntity);
			if (pCSExplosionVictim)
			{
				wasAliveBeforeExplosion = pCSExplosionVictim->IsAlive();
			}
			//=============================================================================
			// HPE_END
			//=============================================================================
			if ( pEntity->m_takedamage != DAMAGE_NO )
			{
				// UNDONE: this should check a damage mask, not an ignore
				if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
				{// houndeyes don't hurt other houndeyes with their attack
					continue;
				}

				// blasts don't travel into or out of water
				if ( !bIgnoreWorld )
				{
					if (bInWater && pEntity->GetWaterLevel() == 0)
						continue;
					if (!bInWater && pEntity->GetWaterLevel() == 3)
						continue;
				}

				// radius damage can only be blocked by the world
				vecSpot = pEntity->BodyTarget( vecSrc );

				bool bHit = false;

				if( bIgnoreWorld )
				{
					vecEndPos = vecSpot;
					bHit = true;
				}
				else
				{
					// get the percentage of the target entity that is visible from the
					// explosion position.
					damagePercentage = GetAmountOfEntityVisible(vecSrc, pEntity);
					if (damagePercentage > 0.0)
					{
						vecEndPos = vecSpot;

						bHit = true;
					}
				}

				if ( bHit )
				{
					// the explosion can 'see' this entity, so hurt them!
					//vecToTarget = ( vecSrc - vecEndPos );
					vecToTarget = ( vecEndPos - vecSrc );

					// use a Gaussian function to describe the damage falloff over distance, with flRadius equal to 3 * sigma
					// this results in the following values:
					// 
					// Range Fraction  Damage
					//		0.0			100%
					// 		0.1			96%
					// 		0.2			84%
					// 		0.3			67%
					// 		0.4			49%
					// 		0.5			32%
					// 		0.6			20%
					// 		0.7			11%
					// 		0.8			 6%
					// 		0.9			 3%
					// 		1.0			 1%

					float fDist = vecToTarget.Length();
					float fSigma = flRadius / 3.0f; // flRadius specifies 3rd standard deviation (0.0111 damage at this range)
					float fGaussianFalloff = exp(-fDist * fDist / (2.0f * fSigma * fSigma));
					float flAdjustedDamage = info.GetDamage() * fGaussianFalloff * damagePercentage;
				
					if ( flAdjustedDamage > 0 )
					{
						CTakeDamageInfo adjustedInfo = info;
						adjustedInfo.SetDamage( flAdjustedDamage );

						Vector dir = vecToTarget;
						VectorNormalize( dir );

						// If we don't have a damage force, manufacture one
						if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
						{
							CalculateExplosiveDamageForce( &adjustedInfo, dir, vecSrc, 1.5	/* explosion scale! */ );
						}
						else
						{
							// Assume the force passed in is the maximum force. Decay it based on falloff.
							float flForce = adjustedInfo.GetDamageForce().Length() * falloff;
							adjustedInfo.SetDamageForce( dir * flForce );
							adjustedInfo.SetDamagePosition( vecSrc );
						}

						Vector vecTarget;
						vecTarget = pEntity->BodyTarget(vecSrc, false);

						UTIL_TraceLine(vecSrc, vecTarget, MASK_SHOT, NULL, COLLISION_GROUP_NONE, &tr);

						// blasts always hit chest
						tr.hitgroup = HITGROUP_GENERIC;

						if (tr.fraction != 1.0)
						{
							// this has to be done to make breakable glass work.
							ClearMultiDamage( );
							pEntity->DispatchTraceAttack( adjustedInfo, dir, &tr );
							ApplyMultiDamage();
						}
						else
						{
							pEntity->TakeDamage( adjustedInfo );
						}
			
						// Now hit all triggers along the way that respond to damage... 
						pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, vecEndPos, dir );
						//=============================================================================
						// HPE_BEGIN:
						// [sbodenbender] Increment grenade damage stat
						//=============================================================================
						if (pCSExplosionVictim && pCSExplosionAttacker && isGrenade)
						{
							CCS_GameStats.IncrementStat(pCSExplosionAttacker, CSSTAT_GRENADE_DAMAGE, static_cast<int>(adjustedInfo.GetDamage()));
						}
						//=============================================================================
						// HPE_END
						//=============================================================================
					}
				}
			}
            
            //=============================================================================
            // HPE_BEGIN:
            // [tj] Count up victims of area of effect damage for achievement purposes
            //=============================================================================
             
            if (pCSExplosionVictim)
			{
				//If the bomb is exploding, set the attacker to the planter (we can't put this in the CTakeDamageInfo, since
				//players aren't supposed to get credit for bomb kills)
				if (isBomb)
				{
					CPlantedC4* bomb = static_cast<CPlantedC4*> (pInflictor);
					if (bomb)
					{
						pCSExplosionAttacker = bomb->GetPlanter();
					}
				}

				//Count check to make sure we killed an enemy player
				if(	pCSExplosionAttacker &&                  
					!pCSExplosionVictim->IsAlive() && 
					wasAliveBeforeExplosion &&
					pCSExplosionVictim->GetTeamNumber() != pCSExplosionAttacker->GetTeamNumber())               
				{
					numberOfEnemyPlayersKilledByThisExplosion++;
				}
			}             
            //=============================================================================
            // HPE_END
            //=============================================================================
            
		}

		//=============================================================================
		// HPE_BEGIN:
		// [tj] //Depending on which type of explosion it was, award the appropriate achievement.
		//=============================================================================
		
		if (pCSExplosionAttacker && isGrenade && numberOfEnemyPlayersKilledByThisExplosion >= AchievementConsts::GrenadeMultiKill_MinKills)
		{
			pCSExplosionAttacker->AwardAchievement(CSGrenadeMultikill);    
			pCSExplosionAttacker->CheckMaxGrenadeKills(numberOfEnemyPlayersKilledByThisExplosion);

		}
		if (pCSExplosionAttacker && isBomb && numberOfEnemyPlayersKilledByThisExplosion >= AchievementConsts::BombMultiKill_MinKills)
		{
			pCSExplosionAttacker->AwardAchievement(CSBombMultikill);            
		}

		//=============================================================================
		// HPE_END
		//=============================================================================
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pVictim - 
	//			*pKiller - 
	//			*pInflictor - 
	//-----------------------------------------------------------------------------
	void CCSGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		// Work out what killed the player, and send a message to all clients about it
		const char *killer_weapon_name = "world";		// by default, the player is killed by the world
		int killer_ID = 0;

		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );
		CCSPlayer *pCSVictim = (CCSPlayer*)(pVictim);

		bool bHeadshot = false;

		if ( pScorer )	// Is the killer a client?
		{
			killer_ID = pScorer->GetUserID();
		
			if( info.GetDamageType() & DMG_HEADSHOT )
			{
				//to enable drawing the headshot icon as well as the weapon icon, 
				bHeadshot = true;
			}
			
			if ( pInflictor )
			{
				if ( pInflictor == pScorer )
				{
					// If the inflictor is the killer,  then it must be their current weapon doing the damage
					if ( pScorer->GetActiveWeapon() )
					{
						killer_weapon_name = pScorer->GetActiveWeapon()->GetClassname(); //GetDeathNoticeName();
					}
				}
				else
				{
					killer_weapon_name = STRING( pInflictor->m_iClassname );  // it's just that easy
				}
			}
		}
		else
		{
			killer_weapon_name = STRING( pInflictor->m_iClassname );
		}

		// strip the NPC_* or weapon_* from the inflictor's classname
		if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		{
			killer_weapon_name += 7;
		}
		else if ( strncmp( killer_weapon_name, "NPC_", 8 ) == 0 )
		{
			killer_weapon_name += 8;
		}
		else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		{
			killer_weapon_name += 5;
		}
		else if( strncmp( killer_weapon_name, "hegrenade", 9 ) == 0 )	//"hegrenade_projectile"	
		{
			killer_weapon_name = "hegrenade";
		}
		else if( strncmp( killer_weapon_name, "flashbang", 9 ) == 0 )	//"flashbang_projectile"
		{
			killer_weapon_name = "flashbang";
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "player_death" );

		if ( event )
		{
			event->SetInt("userid", pVictim->GetUserID() );
			event->SetInt("attacker", killer_ID );
			event->SetString("weapon", killer_weapon_name );
			event->SetInt("headshot", bHeadshot ? 1 : 0 );
			event->SetInt("priority", bHeadshot ? 8 : 7 );	// HLTV event priority, not transmitted
			if ( pCSVictim->GetDeathFlags() & CS_DEATH_DOMINATION )
			{
				event->SetInt( "dominated", 1 );
			}
			else if ( pCSVictim->GetDeathFlags() & CS_DEATH_REVENGE )
			{
				event->SetInt( "revenge", 1 );
			}
			
			gameeventmanager->FireEvent( event );
		}
	}

	//=========================================================
	//=========================================================
	void CCSGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );
		CCSPlayer *pCSVictim = (CCSPlayer *)pVictim;
		CCSPlayer *pCSScorer = (CCSPlayer *)pScorer;

		CCS_GameStats.PlayerKilled( pVictim, info );
		//=============================================================================
		// HPE_BEGIN:        
		// [tj] Flag the round as non-lossless for the appropriate team.
		// [menglish] Set the death flags depending on a nemesis system
		//=============================================================================

		if (pVictim->GetTeamNumber() == TEAM_TERRORIST)
		{
			m_bNoTerroristsKilled = false;
			m_bNoTerroristsDamaged = false;            
		}
		if (pVictim->GetTeamNumber() == TEAM_CT)
		{
			m_bNoCTsKilled = false;
			m_bNoCTsDamaged = false;
		}

        m_bCanDonateWeapons = false;

		if ( m_pFirstKill == NULL && pCSScorer != pVictim )
		{
			m_pFirstKill = pCSScorer;
			m_firstKillTime = gpGlobals->curtime - m_fRoundStartTime;
		}

		// determine if this kill affected a nemesis relationship
		int iDeathFlags = 0;
		if ( pScorer )
		{	
            CCS_GameStats.CalculateOverkill( pCSScorer, pCSVictim);
			CCS_GameStats.CalcDominationAndRevenge( pCSScorer, pCSVictim, &iDeathFlags );            
		}
		pCSVictim->SetDeathFlags( iDeathFlags );	
		//=============================================================================
		// HPE_END
		//=============================================================================

		// If we're killed by the C4, we do a subset of BaseClass::PlayerKilled()
		// Specifically, we shouldn't lose any points or show death notices, to match goldsrc
		if ( Q_strcmp(pKiller->GetClassname(), "planted_c4" ) == 0 )
		{
			// dvsents2: uncomment when removing all FireTargets
			// variant_t value;
			// g_EventQueue.AddEvent( "game_playerdie", "Use", value, 0, pVictim, pVictim );
			FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
		}
		else
		{
			BaseClass::PlayerKilled( pVictim, info );
		}

		// check for team-killing, and give monetary rewards/penalties
		// Find the killer & the scorer
		if ( !pScorer )
			return;

		if ( IPointsForKill( pScorer, pVictim ) < 0 )
		{
			// team-killer!
			pCSScorer->AddAccount( -3300 );
			++pCSScorer->m_iTeamKills;
			pCSScorer->m_bJustKilledTeammate = true;

			ClientPrint( pCSScorer, HUD_PRINTCENTER, "#Killed_Teammate" );
			if ( mp_autokick.GetBool() )
			{
				char strTeamKills[64];
				Q_snprintf( strTeamKills, sizeof( strTeamKills ), "%d", pCSScorer->m_iTeamKills );
				ClientPrint( pCSScorer, HUD_PRINTCONSOLE, "#Game_teammate_kills", strTeamKills ); // this includes a " of 3" in it

				if ( pCSScorer->m_iTeamKills >= 3 )
				{
					ClientPrint( pCSScorer, HUD_PRINTCONSOLE, "#Banned_For_Killing_Teammates" );
					engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pCSScorer->GetUserID() ) );
				}
				else if ( mp_spawnprotectiontime.GetInt() > 0 && GetRoundElapsedTime() < mp_spawnprotectiontime.GetInt() )
				{
					ClientPrint( pCSScorer, HUD_PRINTCONSOLE, "#Banned_For_Killing_Teammates" );
					engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pCSScorer->GetUserID() ) );
				}
			}

			if ( !(pCSScorer->m_iDisplayHistoryBits & DHF_FRIEND_KILLED) )
			{
				pCSScorer->m_iDisplayHistoryBits |= DHF_FRIEND_KILLED;
				pCSScorer->HintMessage( "#Hint_careful_around_teammates", false );
			}
		}
		else
		{
			//=============================================================================
			// HPE_BEGIN:
			// [tj] Added a check to make sure we don't get money for suicides.
			//=============================================================================
			if (pCSScorer != pCSVictim)
			{
			//=============================================================================
			// HPE_END
			//=============================================================================
				if ( pCSVictim->IsVIP() )
				{
					pCSScorer->HintMessage( "#Hint_reward_for_killing_vip", true );
					pCSScorer->AddAccount( 2500 );
				}
				else			
				{
					pCSScorer->AddAccount( 300 );
				}
			}

			if ( !(pCSScorer->m_iDisplayHistoryBits & DHF_ENEMY_KILLED) )
			{
				pCSScorer->m_iDisplayHistoryBits |= DHF_ENEMY_KILLED;
				pCSScorer->HintMessage( "#Hint_win_round_by_killing_enemy", false );
			}
		}
	}


	void CCSGameRules::InitDefaultAIRelationships()
	{
		//  Allocate memory for default relationships
		CBaseCombatCharacter::AllocateDefaultRelationships();

		// --------------------------------------------------------------
		// First initialize table so we can report missing relationships
		// --------------------------------------------------------------
		int i, j;
		for (i=0;i<NUM_AI_CLASSES;i++)
		{
			for (j=0;j<NUM_AI_CLASSES;j++)
			{
				// By default all relationships are neutral of priority zero
				CBaseCombatCharacter::SetDefaultRelationship( (Class_T)i, (Class_T)j, D_NU, 0 );
			}
		}
	}

	//------------------------------------------------------------------------------
	// Purpose : Return classify text for classify type
	//------------------------------------------------------------------------------
	const char *CCSGameRules::AIClassText(int classType)
	{
		switch (classType)
		{
			case CLASS_NONE:			return "CLASS_NONE";
			case CLASS_PLAYER:			return "CLASS_PLAYER";
			default:					return "MISSING CLASS in ClassifyText()";
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: When gaining new technologies in TF, prevent auto switching if we
	//  receive a weapon during the switch
	// Input  : *pPlayer - 
	//			*pWeapon - 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CCSGameRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
	{
		bool bIsBeingGivenItem = false;
		CCSPlayer *pCSPlayer = ToCSPlayer( pPlayer );
		if ( pCSPlayer && pCSPlayer->IsBeingGivenItem() )
			bIsBeingGivenItem = true;

		if ( pPlayer->GetActiveWeapon() && pPlayer->IsNetClient() && !bIsBeingGivenItem )
		{
			// Player has an active item, so let's check cl_autowepswitch.
			const char *cl_autowepswitch = engine->GetClientConVarValue( engine->IndexOfEdict( pPlayer->edict() ), "cl_autowepswitch" );
			if ( cl_autowepswitch && atoi( cl_autowepswitch ) <= 0 )
			{
				return false;
			}
		}

		if ( pPlayer->IsBot() && !bIsBeingGivenItem )
		{
			return false;
		}

		if ( !GetAllowWeaponSwitch() )
		{
			return false;
		}

		return BaseClass::FShouldSwitchWeapon( pPlayer, pWeapon );
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : allow - 
	//-----------------------------------------------------------------------------
	void CCSGameRules::SetAllowWeaponSwitch( bool allow )
	{
		m_bAllowWeaponSwitch = allow;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CCSGameRules::GetAllowWeaponSwitch()
	{
		return m_bAllowWeaponSwitch;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Input  : *pPlayer - 
	// Output : const char
	//-----------------------------------------------------------------------------
	const char *CCSGameRules::SetDefaultPlayerTeam( CBasePlayer *pPlayer )
	{
		Assert( pPlayer );
		return BaseClass::SetDefaultPlayerTeam( pPlayer );
	}


	void CCSGameRules::LevelInitPreEntity()
	{
		BaseClass::LevelInitPreEntity();

		// TODO for CZ-style hostages: TheHostageChatter->Precache();
	}


	void CCSGameRules::LevelInitPostEntity()
	{
		BaseClass::LevelInitPostEntity();

		m_bLevelInitialized = false; // re-count CT and T start spots now that they exist

		// Figure out from the entities in the map what kind of map this is (bomb run, prison escape, etc).
		CheckMapConditions();
	}
	
	INetworkStringTable *g_StringTableBlackMarket = NULL;

	void CCSGameRules::CreateCustomNetworkStringTables( void )
	{
		m_StringTableBlackMarket = g_StringTableBlackMarket;

		if ( 0 )//mp_dynamicpricing.GetBool() )
		{
			m_bBlackMarket = BlackMarket_DownloadPrices();

			if ( m_bBlackMarket == false )
			{
				Msg( "ERROR: mp_dynamicpricing set to 1 but couldn't download the price list!\n" );
			}
		}
		else
		{
			m_bBlackMarket = false;
			SetBlackMarketPrices( true );
		}
	}

	float CCSGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
	{
		float fFallVelocity = pPlayer->m_Local.m_flFallVelocity - CS_PLAYER_MAX_SAFE_FALL_SPEED;
		float fallDamage = fFallVelocity * CS_DAMAGE_FOR_FALL_SPEED * 1.25;

		if ( fallDamage > 0.0f )
		{
			// let the bots know
			IGameEvent * event = gameeventmanager->CreateEvent( "player_falldamage" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetFloat( "damage", fallDamage );
				event->SetInt( "priority", 4 );	// HLTV event priority, not transmitted
				
				gameeventmanager->FireEvent( event );
			}
		}

		return fallDamage;
	} 

	
	void CCSGameRules::ClientDisconnected( edict_t *pClient )
	{
		BaseClass::ClientDisconnected( pClient );

        //=============================================================================
        // HPE_BEGIN:
        // [tj] Clear domination data when a player disconnects
        //=============================================================================
         
        CCSPlayer *pPlayer = ToCSPlayer( GetContainingEntity( pClient ) );
        if ( pPlayer )
        {
            pPlayer->RemoveNemesisRelationships();
        }
         
        //=============================================================================
        // HPE_END
        //=============================================================================
        

		CheckWinConditions();
	}


	// Called when game rules are destroyed by CWorld
	void CCSGameRules::LevelShutdown()
	{
		int iLevelIndex = GetCSLevelIndex( STRING( gpGlobals->mapname ) );

		if ( iLevelIndex != -1 )
		{
			g_iTerroristVictories[iLevelIndex] += m_iNumTerroristWins;
			g_iCounterTVictories[iLevelIndex] += m_iNumCTWins;
		}

		BaseClass::LevelShutdown();
	}

	
	//---------------------------------------------------------------------------------------------------
	/**
	 * Check if the scenario has been won/lost.
	 * Return true if the scenario is over, false if the scenario is still in progress
	 */
	bool CCSGameRules::CheckWinConditions( void )
	{
		if ( mp_ignore_round_win_conditions.GetBool() )
		{
			return false;
		}

		// If a winner has already been determined.. then get the heck out of here
		if (m_iRoundWinStatus != WINNER_NONE)
		{
			// still check if we lost players to where we need to do a full reset next round...
			int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
			InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );

			bool bNeededPlayers = false;
			NeededPlayersCheck( bNeededPlayers );

			return true;
		}

		// Initialize the player counts..
		int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
		InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );


		/***************************** OTHER PLAYER's CHECK *********************************************************/
		bool bNeededPlayers = false;
		if ( NeededPlayersCheck( bNeededPlayers ) )
			return false;

		/****************************** ASSASINATION/VIP SCENARIO CHECK *******************************************************/
		if ( VIPRoundEndCheck( bNeededPlayers ) )
			return true;

		/****************************** PRISON ESCAPE CHECK *******************************************************/
		if ( PrisonRoundEndCheck() )
			return true;


		/****************************** BOMB CHECK ********************************************************/
		if ( BombRoundEndCheck( bNeededPlayers ) )
			return true;


		/***************************** TEAM EXTERMINATION CHECK!! *********************************************************/
		// CounterTerrorists won by virture of elimination
		if ( TeamExterminationCheck( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT, bNeededPlayers ) )
			return true;

		
		/******************************** HOSTAGE RESCUE CHECK ******************************************************/
		if ( HostageRescueRoundEndCheck( bNeededPlayers ) )
			return true;

		// scenario not won - still in progress
		return false;
	}


	bool CCSGameRules::NeededPlayersCheck( bool &bNeededPlayers )
	{
		// We needed players to start scoring
		// Do we have them now?
		if( !m_iNumSpawnableTerrorist || !m_iNumSpawnableCT )
		{
			Msg( "Game will not start until both teams have players.\n" );
			UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#Game_scoring" );
			bNeededPlayers = true;

			m_bFirstConnected = false;
		}

		if ( !m_bFirstConnected && m_iNumSpawnableTerrorist && m_iNumSpawnableCT )
		{
			// Start the round immediately when the first person joins
			// UTIL_LogPrintf( "World triggered \"Game_Commencing\"\n" );

			m_bFreezePeriod  = false; //Make sure we are not on the FreezePeriod.
			m_bCompleteReset = true;

			TerminateRound( 3.0f, Game_Commencing );
			m_bFirstConnected = true;
			return true;
		}

		return false;
	}


	void CCSGameRules::InitializePlayerCounts(
		int &NumAliveTerrorist,
		int &NumAliveCT,
		int &NumDeadTerrorist,
		int &NumDeadCT
		)
	{
		NumAliveTerrorist = NumAliveCT = NumDeadCT = NumDeadTerrorist = 0;
		m_iNumTerrorist = m_iNumCT = m_iNumSpawnableTerrorist = m_iNumSpawnableCT = 0;
		m_iHaveEscaped = 0;

		// Count how many dead players there are on each team.
		for ( int iTeam=0; iTeam < GetNumberOfTeams(); iTeam++ )
		{
			CTeam *pTeam = GetGlobalTeam( iTeam );

			for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
			{
				CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
				Assert( pPlayer );
				if ( !pPlayer )
					continue;

				Assert( pPlayer->GetTeamNumber() == pTeam->GetTeamNumber() );

				switch ( pTeam->GetTeamNumber() )
				{
				case TEAM_CT:
					m_iNumCT++;

					if ( pPlayer->State_Get() != STATE_PICKINGCLASS )
						m_iNumSpawnableCT++;

					if ( pPlayer->m_lifeState != LIFE_ALIVE )
						NumDeadCT++;
					else
						NumAliveCT++;

					break;

				case TEAM_TERRORIST:
					m_iNumTerrorist++;

					if ( pPlayer->State_Get() != STATE_PICKINGCLASS )
						m_iNumSpawnableTerrorist++;

					if ( pPlayer->m_lifeState != LIFE_ALIVE )
						NumDeadTerrorist++;
					else
						NumAliveTerrorist++;

					// Check to see if this guy escaped.
					if ( pPlayer->m_bEscaped == true )
						m_iHaveEscaped++;

					break;
				}
			}
		}
	}

	bool CCSGameRules::HostageRescueRoundEndCheck( bool bNeededPlayers )
	{
		// Check to see if 50% of the hostages have been rescued.
		CHostage* hostage = NULL;

		int iNumHostages = g_Hostages.Count();
		int iNumLeftToRescue = 0;
		int i;

		for ( i=0; i<iNumHostages; i++ )
		{
			hostage = g_Hostages[i];

			if ( hostage->m_iHealth > 0 && !hostage->IsRescued() ) // We've found a live hostage. don't end the round
				iNumLeftToRescue++;
		}

		m_iHostagesRemaining = iNumLeftToRescue;

		if ( (iNumLeftToRescue == 0) && (iNumHostages > 0) )
		{
			if ( m_iHostagesRescued >= (iNumHostages * 0.5)	)
			{
				m_iAccountCT += 2500;

				if ( !bNeededPlayers )
				{
					m_iNumCTWins ++;
					// Update the clients team score
					UpdateTeamScores();
				}
				CCS_GameStats.Event_AllHostagesRescued();
				// tell the bots all the hostages have been rescued
				IGameEvent * event = gameeventmanager->CreateEvent( "hostage_rescued_all" );
				if ( event )
				{
					gameeventmanager->FireEvent( event );
				}

				TerminateRound( mp_round_restart_delay.GetFloat(), All_Hostages_Rescued );
				return true;
			}
		}

		return false;
	}


	bool CCSGameRules::PrisonRoundEndCheck()
	{
		//MIKETODO: get this working when working on prison escape
		/*
		if (m_bMapHasEscapeZone == true)
		{
			float flEscapeRatio;

			flEscapeRatio = (float) m_iHaveEscaped / (float) m_iNumEscapers;

			if (flEscapeRatio >= m_flRequiredEscapeRatio)
			{
				BroadcastSound( "Event.TERWin" );
				m_iAccountTerrorist += 3150;

				if ( !bNeededPlayers )
				{
					m_iNumTerroristWins ++;
					// Update the clients team score
					UpdateTeamScores();
				}
				EndRoundMessage( "#Terrorists_Escaped", Terrorists_Escaped );
				TerminateRound( mp_round_restart_delay.GetFloat(), WINNER_TER );
				return;
			}
			else if ( NumAliveTerrorist == 0 && flEscapeRatio < m_flRequiredEscapeRatio)
			{
				BroadcastSound( "Event.CTWin" );
				m_iAccountCT += (1 - flEscapeRatio) * 3500; // CTs are rewarded based on how many terrorists have escaped...
				
				if ( !bNeededPlayers )
				{
					m_iNumCTWins++;
					// Update the clients team score
					UpdateTeamScores();
				}
				EndRoundMessage( "#CTs_PreventEscape", CTs_PreventEscape );
				TerminateRound( mp_round_restart_delay.GetFloat(), WINNER_CT );
				return;
			}

			else if ( NumAliveTerrorist == 0 && NumDeadTerrorist != 0 && m_iNumSpawnableCT > 0 )
			{
				BroadcastSound( "Event.CTWin" );
				m_iAccountCT += (1 - flEscapeRatio) * 3250; // CTs are rewarded based on how many terrorists have escaped...
				
				if ( !bNeededPlayers )
				{
					m_iNumCTWins++;
					// Update the clients team score
					UpdateTeamScores();
				}
				EndRoundMessage( "#Escaping_Terrorists_Neutralized", Escaping_Terrorists_Neutralized );
				TerminateRound( mp_round_restart_delay.GetFloat(), WINNER_CT );
				return;
			}
			// else return;    
		}
		*/

		return false;
	}


	bool CCSGameRules::VIPRoundEndCheck( bool bNeededPlayers )
	{
		if (m_iMapHasVIPSafetyZone != 1)
			return false;

		if (m_pVIP == NULL)
			return false;

		if (m_pVIP->m_bEscaped == true)
		{
			m_iAccountCT += 3500;

			if ( !bNeededPlayers )
			{
				m_iNumCTWins ++;
				// Update the clients team score
				UpdateTeamScores();
			}

			//MIKETODO: get this working when working on VIP scenarios
			/*
			MessageBegin( MSG_SPEC, SVC_DIRECTOR );
				WRITE_BYTE ( 9 );	// command length in bytes
				WRITE_BYTE ( DRC_CMD_EVENT );	// VIP rescued
				WRITE_SHORT( ENTINDEX(m_pVIP->edict()) );	// index number of primary entity
				WRITE_SHORT( 0 );	// index number of secondary entity
				WRITE_LONG( 15 | DRC_FLAG_FINAL);   // eventflags (priority and flags)
			MessageEnd();
			*/

			// tell the bots the VIP got out
			IGameEvent * event = gameeventmanager->CreateEvent( "vip_escaped" );
			if ( event )
			{
				event->SetInt( "userid", m_pVIP->GetUserID() );
				event->SetInt( "priority", 9 );
				gameeventmanager->FireEvent( event );
			}

			//=============================================================================
			// HPE_BEGIN:
			// [menglish] If the VIP has escaped award him an MVP
			//=============================================================================
			 
			m_pVIP->IncrementNumMVPs( CSMVP_UNDEFINED );
			 
			//=============================================================================
			// HPE_END
			//=============================================================================

			TerminateRound( mp_round_restart_delay.GetFloat(), VIP_Escaped );
			return true;
		}
		else if ( m_pVIP->m_lifeState == LIFE_DEAD )   // The VIP is dead
		{
			m_iAccountTerrorist += 3250;

			if ( !bNeededPlayers )
			{
				m_iNumTerroristWins ++;
				// Update the clients team score
				UpdateTeamScores();
			}

			// tell the bots the VIP was killed
			IGameEvent * event = gameeventmanager->CreateEvent( "vip_killed" );
			if ( event )
			{
				event->SetInt( "userid", m_pVIP->GetUserID() );
				event->SetInt( "priority", 9 );
				gameeventmanager->FireEvent( event );
			}

			TerminateRound( mp_round_restart_delay.GetFloat(), VIP_Assassinated );
			return true;
		}

		return false;
	}


	bool CCSGameRules::BombRoundEndCheck( bool bNeededPlayers )
	{
		// Check to see if the bomb target was hit or the bomb defused.. if so, then let's end the round!
		if ( ( m_bTargetBombed == true ) && ( m_bMapHasBombTarget == true ) )
		{
			m_iAccountTerrorist += 3500;

			if ( !bNeededPlayers )
			{
				m_iNumTerroristWins ++;
				// Update the clients team score
				UpdateTeamScores();
			}

			TerminateRound( mp_round_restart_delay.GetFloat(), Target_Bombed );
			return true;
		}
		else
		if ( ( m_bBombDefused == true ) && ( m_bMapHasBombTarget == true ) )
		{
			m_iAccountCT += 3250;

			m_iAccountTerrorist += 800; // give the T's a little bonus for planting the bomb even though it was defused.

			if ( !bNeededPlayers )
			{
				m_iNumCTWins++;
				// Update the clients team score
				UpdateTeamScores();
			}

			TerminateRound( mp_round_restart_delay.GetFloat(), Bomb_Defused );
			return true;
		}

		return false;
	}


	bool CCSGameRules::TeamExterminationCheck(
		int NumAliveTerrorist,
		int NumAliveCT,
		int NumDeadTerrorist,
		int NumDeadCT,
		bool bNeededPlayers
	)
	{
		if ( ( m_iNumCT > 0 && m_iNumSpawnableCT > 0 ) && ( m_iNumTerrorist > 0 && m_iNumSpawnableTerrorist > 0 ) )
		{
			if ( NumAliveTerrorist == 0 && NumDeadTerrorist != 0 && m_iNumSpawnableCT > 0 )
			{
				bool nowin = false;
					
				for ( int iGrenade=0; iGrenade < g_PlantedC4s.Count(); iGrenade++ )
				{
					CPlantedC4 *pC4 = g_PlantedC4s[iGrenade];

					if ( pC4->IsBombActive() )
						nowin = true;
				}

				if ( !nowin )
				{
					if ( m_bMapHasBombTarget )
						m_iAccountCT += 3250;
					else
						m_iAccountCT += 3000;

					if ( !bNeededPlayers )
					{
						m_iNumCTWins++;
						// Update the clients team score
						UpdateTeamScores();
					}

					TerminateRound( mp_round_restart_delay.GetFloat(), CTs_Win );
					return true;
				}
			}
		
			// Terrorists WON
			if ( NumAliveCT == 0 && NumDeadCT != 0 && m_iNumSpawnableTerrorist > 0 )
			{
				if ( m_bMapHasBombTarget )
					m_iAccountTerrorist += 3250;
				else
					m_iAccountTerrorist += 3000;

				if ( !bNeededPlayers )
				{
					m_iNumTerroristWins++;
					// Update the clients team score
					UpdateTeamScores();
				}

				TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Win );
				return true;
			}
		}
		else if ( NumAliveCT == 0 && NumAliveTerrorist == 0 )
		{
			TerminateRound( mp_round_restart_delay.GetFloat(), Round_Draw );
			return true;
		}

		return false;
	}


	void CCSGameRules::PickNextVIP()
	{
		// MIKETODO: work on this when getting VIP maps running.
		/*
		if (IsVIPQueueEmpty() != true)
		{
			// Remove the current VIP from his VIP status and make him a regular CT.
			if (m_pVIP != NULL)
				ResetCurrentVIP();

			for (int i = 0; i <= 4; i++)
			{
				if (VIPQueue[i] != NULL)
				{
					m_pVIP = VIPQueue[i];
					m_pVIP->MakeVIP();

					VIPQueue[i] = NULL;		// remove this player from the VIP queue
					StackVIPQueue();		// and re-organize the queue
					m_iConsecutiveVIP = 0;
					return;
				}
			}
		}
		else if (m_iConsecutiveVIP >= 3)	// If it's been the same VIP for 3 rounds already.. then randomly pick a new one
		{
			m_iLastPick++;

			if (m_iLastPick > m_iNumCT)
				m_iLastPick = 1;

			int iCount = 1;

			CBaseEntity* pPlayer = NULL;
			CBasePlayer* player = NULL;
			CBasePlayer* pLastPlayer = NULL;

			pPlayer = UTIL_FindEntityByClassname ( pPlayer, "player" );
			while (		(pPlayer != NULL) && (!FNullEnt(pPlayer->edict()))	)
			{
				if (	!(pPlayer->pev->flags & FL_DORMANT)	)
				{
					player = GetClassPtr((CBasePlayer *)pPlayer->pev);
					
					if (	(player->m_iTeam == CT) && (iCount == m_iLastPick)	)
					{
						if (	(player == m_pVIP) && (pLastPlayer != NULL)	)
							player = pLastPlayer;

						// Remove the current VIP from his VIP status and make him a regular CT.
						if (m_pVIP != NULL)
							ResetCurrentVIP();

						player->MakeVIP();
						m_iConsecutiveVIP = 0;

						return;
					}
					else if ( player->m_iTeam == CT )
						iCount++;

					if ( player->m_iTeam != SPECTATOR )
						pLastPlayer = player;
				}
				pPlayer = UTIL_FindEntityByClassname ( pPlayer, "player" );
			}
		}
		else if (m_pVIP == NULL)  // There is no VIP and there is no one waiting to be the VIP.. therefore just pick the first CT player we can find.
		{
			CBaseEntity* pPlayer = NULL;
			CBasePlayer* player = NULL;

			pPlayer = UTIL_FindEntityByClassname ( pPlayer, "player" );
			while (		(pPlayer != NULL) && (!FNullEnt(pPlayer->edict()))	)
			{
				if ( pPlayer->pev->flags != FL_DORMANT	)
				{
					player = GetClassPtr((CBasePlayer *)pPlayer->pev);
		
					if ( player->m_iTeam == CT )
					{
						player->MakeVIP();
						m_iConsecutiveVIP = 0;
						return;
					}
				}
				pPlayer = UTIL_FindEntityByClassname ( pPlayer, "player" );
			}
		}
		*/
	}


	void CCSGameRules::ReadMultiplayCvars()
	{
		m_iRoundTime = (int)(mp_roundtime.GetFloat() * 60);
		m_iFreezeTime = mp_freezetime.GetInt();
	}


	void CCSGameRules::RestartRound()
	{
#if defined( REPLAY_ENABLED )
		if ( g_pReplay )
		{
			// Write replay and stop recording if appropriate
			if ( g_pReplay->IsRecording() )
			{
				g_pReplay->SV_EndRecordingSession();
			}
			
			int nActivePlayerCount = m_iNumTerrorist + m_iNumCT;
			if ( nActivePlayerCount && g_pReplay->SV_ShouldBeginRecording( false ) )
			{
				// Tell the replay manager that it should begin recording the new round as soon as possible
				g_pReplay->SV_GetContext()->GetSessionRecorder()->StartRecording();
			}
		}
#endif
		//=============================================================================
		// HPE_BEGIN:
		// [tj] Notify players that the round is about to be reset
		//=============================================================================
        for ( int clientIndex = 1; clientIndex <= gpGlobals->maxClients; clientIndex++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( clientIndex );
			if(pPlayer)
			{
				pPlayer->OnPreResetRound();
			}
		}

		//=============================================================================
		// HPE_END
		//=============================================================================    

		if ( !IsFinite( gpGlobals->curtime ) )
		{
			Warning( "NaN curtime in RestartRound\n" );
			gpGlobals->curtime = 0.0f;
		}

		int i;

		m_iTotalRoundsPlayed++;
		
		//ClearBodyQue();

		// Hardlock the player accelaration to 5.0
		//CVAR_SET_FLOAT( "sv_accelerate", 5.0 );
		//CVAR_SET_FLOAT( "sv_friction", 4.0 );
		//CVAR_SET_FLOAT( "sv_stopspeed", 75 );

		sv_stopspeed.SetValue( 75.0f );

		// Tabulate the number of players on each team.
		int NumDeadCT, NumDeadTerrorist, NumAliveTerrorist, NumAliveCT;
		InitializePlayerCounts( NumAliveTerrorist, NumAliveCT, NumDeadTerrorist, NumDeadCT );
		
		m_bBombDropped = false;
		m_bBombPlanted = false;
		
		if ( GetHumanTeam() != TEAM_UNASSIGNED )
		{
			MoveHumansToHumanTeam();
		}

		/*************** AUTO-BALANCE CODE *************/
		if ( mp_autoteambalance.GetInt() != 0 &&
			(m_iUnBalancedRounds >= 1) )
		{
			if ( GetHumanTeam() == TEAM_UNASSIGNED )
			{
				BalanceTeams();
			}
		}

		if ( ((m_iNumSpawnableCT - m_iNumSpawnableTerrorist) >= 2) ||
			((m_iNumSpawnableTerrorist - m_iNumSpawnableCT) >= 2)	)
		{
			m_iUnBalancedRounds++;
		}
		else
		{
			m_iUnBalancedRounds = 0;
		}

		// Warn the players of an impending auto-balance next round...
		if ( mp_autoteambalance.GetInt() != 0 &&
			(m_iUnBalancedRounds == 1)	)
		{
			if ( GetHumanTeam() == TEAM_UNASSIGNED )
			{
				UTIL_ClientPrintAll( HUD_PRINTCENTER,"#Auto_Team_Balance_Next_Round");
			}
		}

		/*************** AUTO-BALANCE CODE *************/

		if ( m_bCompleteReset )
		{
			// bounds check
			if ( mp_timelimit.GetInt() < 0 )
			{
				mp_timelimit.SetValue( 0 );
			}
			m_flGameStartTime = gpGlobals->curtime;
			if ( !IsFinite( m_flGameStartTime.Get() ) )
			{
				Warning( "Trying to set a NaN game start time\n" );
				m_flGameStartTime.GetForModify() = 0.0f;
			}

			// Reset total # of rounds played
			m_iTotalRoundsPlayed = 0;

			// Reset score info
			m_iNumTerroristWins				= 0;
			m_iNumCTWins					= 0;
			m_iNumConsecutiveTerroristLoses	= 0;
			m_iNumConsecutiveCTLoses		= 0;


			// Reset team scores
			UpdateTeamScores();


			// Reset the player stats
			for ( i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CCSPlayer *pPlayer = CCSPlayer::Instance( i );

				if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
					pPlayer->Reset();
			}
		}

		m_bFreezePeriod = true;

		ReadMultiplayCvars();

		// Check to see if there's a mapping info paramater entity
		if ( g_pMapInfo )
		{
			switch ( g_pMapInfo->m_iBuyingStatus )
			{
				case 0: 
					m_bCTCantBuy = false; 
					m_bTCantBuy = false; 
					Msg( "EVERYONE CAN BUY!\n" );
					break;
				
				case 1: 
					m_bCTCantBuy = false; 
					m_bTCantBuy = true; 
					Msg( "Only CT's can buy!!\n" );
					break;

				case 2: 
					m_bCTCantBuy = true; 
					m_bTCantBuy = false; 
					Msg( "Only T's can buy!!\n" );
					break;
				
				case 3: 
					m_bCTCantBuy = true; 
					m_bTCantBuy = true; 
					Msg( "No one can buy!!\n" );
					break;

				default: 
					m_bCTCantBuy = false; 
					m_bTCantBuy = false; 
					break;
			}
		}
		else
		{
			// by default everyone can buy
			m_bCTCantBuy = false; 
			m_bTCantBuy = false; 
		}
		
		
		// Check to see if this map has a bomb target in it

		if ( gEntList.FindEntityByClassname( NULL, "func_bomb_target" ) )
		{
			m_bMapHasBombTarget		= true;
			m_bMapHasBombZone		= true;
		}
		else if ( gEntList.FindEntityByClassname( NULL, "info_bomb_target" ) )
		{
			m_bMapHasBombTarget		= true;
			m_bMapHasBombZone		= false;
		}
		else
		{
			m_bMapHasBombTarget		= false;
			m_bMapHasBombZone		= false;
		}

		// Check to see if this map has hostage rescue zones

		if ( gEntList.FindEntityByClassname( NULL, "func_hostage_rescue" ) )
			m_bMapHasRescueZone = true;
		else
			m_bMapHasRescueZone = false;


		// See if the map has func_buyzone entities
		// Used by CBasePlayer::HandleSignals() to support maps without these entities
		
		if ( gEntList.FindEntityByClassname( NULL, "func_buyzone" ) )
			m_bMapHasBuyZone = true;
		else
			m_bMapHasBuyZone = false;


		// GOOSEMAN : See if this map has func_escapezone entities
		if ( gEntList.FindEntityByClassname( NULL, "func_escapezone" ) )
		{
			m_bMapHasEscapeZone = true;
			m_iHaveEscaped = 0;
			m_iNumEscapers = 0; // Will increase this later when we count how many Ts are starting
			if (m_iNumEscapeRounds >= 3)
			{
				SwapAllPlayers();
				m_iNumEscapeRounds = 0;
			}

			m_iNumEscapeRounds++;  // Increment the number of rounds played... After 8 rounds, the players will do a whole sale switch..
		}
		else
			m_bMapHasEscapeZone = false;

		// Check to see if this map has VIP safety zones
		if ( gEntList.FindEntityByClassname( NULL, "func_vip_safetyzone" ) )
		{
			PickNextVIP();
			m_iConsecutiveVIP++;
			m_iMapHasVIPSafetyZone = 1;
		}
		else
			m_iMapHasVIPSafetyZone = 2;

		// Update accounts based on number of hostages remaining.. 
		int iRescuedHostageBonus = 0;

		for ( int iHostage=0; iHostage < g_Hostages.Count(); iHostage++ )
		{
			CHostage *pHostage = g_Hostages[iHostage];

			if( pHostage->IsRescuable() )	//Alive and not rescued
			{
				iRescuedHostageBonus += 150;
			}
			
			if ( iRescuedHostageBonus >= 2000 )
				break;
		}

		//*******Catch up code by SupraFiend. Scale up the loser bonus when teams fall into losing streaks
		if (m_iRoundWinStatus == WINNER_TER) // terrorists won
		{
			//check to see if they just broke a losing streak
			if(m_iNumConsecutiveTerroristLoses > 1)
				m_iLoserBonus = 1500;//this is the default losing bonus

			m_iNumConsecutiveTerroristLoses = 0;//starting fresh
			m_iNumConsecutiveCTLoses++;//increment the number of wins the CTs have had
		}
		else if (m_iRoundWinStatus == WINNER_CT) // CT Won
		{
			//check to see if they just broke a losing streak
			if(m_iNumConsecutiveCTLoses > 1)
				m_iLoserBonus = 1500;//this is the default losing bonus

			m_iNumConsecutiveCTLoses = 0;//starting fresh
			m_iNumConsecutiveTerroristLoses++;//increment the number of wins the Terrorists have had
		}

		//check if the losing team is in a losing streak & that the loser bonus hasen't maxed out.
		if((m_iNumConsecutiveTerroristLoses > 1) && (m_iLoserBonus < 3000))
			m_iLoserBonus += 500;//help out the team in the losing streak
		else
		if((m_iNumConsecutiveCTLoses > 1) && (m_iLoserBonus < 3000))
			m_iLoserBonus += 500;//help out the team in the losing streak

		// assign the wining and losing bonuses
		if (m_iRoundWinStatus == WINNER_TER) // terrorists won
		{
			m_iAccountTerrorist += iRescuedHostageBonus;
			m_iAccountCT += m_iLoserBonus;
		}
		else if (m_iRoundWinStatus == WINNER_CT) // CT Won
		{
			m_iAccountCT += iRescuedHostageBonus;
			if (m_bMapHasEscapeZone == false)	// only give them the bonus if this isn't an escape map
				m_iAccountTerrorist += m_iLoserBonus;
		}
		

		//Update CT account based on number of hostages rescued
		m_iAccountCT += m_iHostagesRescued * 750;


		// Update individual players accounts and respawn players

		//**********new code by SupraFiend
		//##########code changed by MartinO 
		//the round time stamp must be set before players are spawned
		m_fRoundStartTime = gpGlobals->curtime + m_iFreezeTime;

		if ( !IsFinite( m_fRoundStartTime.Get() ) )
		{
			Warning( "Trying to set a NaN round start time\n" );
			m_fRoundStartTime.GetForModify() = 0.0f;
		}
		
		//Adrian - No cash for anyone at first rounds! ( well, only the default. )
		if ( m_bCompleteReset )
		{
			m_iAccountTerrorist = m_iAccountCT = 0; //No extra cash!.

			//We are starting fresh. So it's like no one has ever won or lost.
			m_iNumTerroristWins				= 0; 
			m_iNumCTWins					= 0;
			m_iNumConsecutiveTerroristLoses	= 0;
			m_iNumConsecutiveCTLoses		= 0;
			m_iLoserBonus					= 1400;
		}

		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			pPlayer->m_iNumSpawns	= 0;
			pPlayer->m_bTeamChanged	= false;
				
			if ( pPlayer->GetTeamNumber() == TEAM_CT )
			{
				if (pPlayer->DoesPlayerGetRoundStartMoney())
				{
					pPlayer->AddAccount( m_iAccountCT );
				}
			}
			else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
			{
				m_iNumEscapers++;	// Add another potential escaper to the mix!
				if (pPlayer->DoesPlayerGetRoundStartMoney())
				{
					pPlayer->AddAccount( m_iAccountTerrorist );
				}
			}

			// tricky, make players non solid while moving to their spawn points
			if ( (pPlayer->GetTeamNumber() == TEAM_CT) || (pPlayer->GetTeamNumber() == TEAM_TERRORIST) )
			{
				pPlayer->AddSolidFlags( FSOLID_NOT_SOLID );
			}
		}
        
        //=============================================================================
        // HPE_BEGIN:
        // [tj] Keep track of number of players per side and if they have the same uniform
        //=============================================================================
 
        int terroristUniform = -1;
        bool allTerroristsWearingSameUniform = true;
        int numberOfTerrorists = 0;
        int ctUniform = -1;
        bool allCtsWearingSameUniform = true;
        int numberOfCts = 0;
 
        //=============================================================================
        // HPE_END
        //=============================================================================

		// know respawn all players
		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_CT && pPlayer->PlayerClass() >= FIRST_CT_CLASS && pPlayer->PlayerClass() <= LAST_CT_CLASS )
			{
                //=============================================================================
                // HPE_BEGIN:
                // [tj] Increment CT count and check CT uniforms.
                //=============================================================================
                 
                numberOfCts++;
                if (ctUniform == -1)
                {
                    ctUniform = pPlayer->PlayerClass();
                }
                else if (pPlayer->PlayerClass() != ctUniform)
                {
                    allCtsWearingSameUniform = false;
                }
                 
                //=============================================================================
                // HPE_END
                //=============================================================================
                
				pPlayer->RoundRespawn();
			}

			if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST && pPlayer->PlayerClass() >= FIRST_T_CLASS && pPlayer->PlayerClass() <= LAST_T_CLASS )
			{
                //=============================================================================
                // HPE_BEGIN:
                // [tj] Increment terrorist count and check terrorist uniforms
                //=============================================================================
                 
                numberOfTerrorists++;
                if (terroristUniform == -1)
                {
                    terroristUniform = pPlayer->PlayerClass();
                }
                else if (pPlayer->PlayerClass() != terroristUniform)
                {
                    allTerroristsWearingSameUniform = false;
                }
                 
                //=============================================================================
                // HPE_END
                //=============================================================================
                
				pPlayer->RoundRespawn();
			}
			else
			{
				pPlayer->ObserverRoundRespawn();
			}

			if ( pPlayer->m_iAccount > pPlayer->m_iShouldHaveCash )
			{
				m_bDontUploadStats = true;
			}
		}

        //=============================================================================
        // HPE_BEGIN:
        //=============================================================================

        // [tj] Award same uniform achievement for qualifying teams
        for ( i = 1; i <= gpGlobals->maxClients; i++ )
        {
            CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

            if ( !pPlayer )
                continue;

            if ( pPlayer->GetTeamNumber() == TEAM_CT && allCtsWearingSameUniform && numberOfCts >= AchievementConsts::SameUniform_MinPlayers)
            {
                pPlayer->AwardAchievement(CSSameUniform);
            }

            if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST && allTerroristsWearingSameUniform && numberOfTerrorists >= AchievementConsts::SameUniform_MinPlayers)
            {
                pPlayer->AwardAchievement(CSSameUniform);
            }
        }

		// [menglish] reset per-round achievement variables for each player
		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
			if( pPlayer )
			{
				pPlayer->ResetRoundBasedAchievementVariables();
			}
		}

		// [pfreese] Reset all round or match stats, depending on type of restart
		if ( m_bCompleteReset )
		{
			CCS_GameStats.ResetAllStats();
			CCS_GameStats.ResetPlayerClassMatchStats();
		}
		else
		{
			CCS_GameStats.ResetRoundStats();
		}

		//=============================================================================
		// HPE_END
		//=============================================================================

		// Respawn entities (glass, doors, etc..)
		CleanUpMap();

		// now run a tkpunish check, after the map has been cleaned up
		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_CT && pPlayer->PlayerClass() >= FIRST_CT_CLASS && pPlayer->PlayerClass() <= LAST_CT_CLASS )
			{
				pPlayer->CheckTKPunishment();
			}
			if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST && pPlayer->PlayerClass() >= FIRST_T_CLASS && pPlayer->PlayerClass() <= LAST_T_CLASS )
			{
				pPlayer->CheckTKPunishment();
			}
		}

		// Give C4 to the terrorists
		if (m_bMapHasBombTarget == true	)
			GiveC4();

		// Reset game variables
		m_flIntermissionEndTime = 0;
		m_flRestartRoundTime = 0.0;
		m_iAccountTerrorist = m_iAccountCT = 0;
		m_iHostagesRescued = 0;
		m_iHostagesTouched = 0;

        //=============================================================================
        // HPE_BEGIN
        // [dwenger] Reset rescue-related achievement values
        //=============================================================================

		// [tj] reset flawless and lossless round related flags
		m_bNoTerroristsKilled = true;
		m_bNoCTsKilled = true;
		m_bNoTerroristsDamaged = true;
		m_bNoCTsDamaged = true;
		m_pFirstKill = NULL;
		m_pFirstBlood = NULL;

        m_bCanDonateWeapons = true;

		// [dwenger] Reset rescue-related achievement values
        m_iHostagesRemaining = 0;
        m_pLastRescuer = NULL;

		m_hostageWasInjured = false;
		m_hostageWasKilled = false;

        //=============================================================================
        // HPE_END
        //=============================================================================

        m_iNumRescuers = 0;
		m_iRoundWinStatus = WINNER_NONE;
		m_bTargetBombed = m_bBombDefused = false;
		m_bCompleteReset = false;
		m_flNextHostageAnnouncement = gpGlobals->curtime;

		m_iHostagesRemaining = g_Hostages.Count();

		// fire global game event
		IGameEvent * event = gameeventmanager->CreateEvent( "round_start" );
		if ( event )
		{
			event->SetInt("timelimit", m_iRoundTime );
			event->SetInt("fraglimit", 0 );
			event->SetInt( "priority", 6 ); // HLTV event priority, not transmitted
		
			if ( m_bMapHasRescueZone )
			{
				event->SetString("objective","HOSTAGE RESCUE");
			}
			else if ( m_bMapHasEscapeZone )
			{
				event->SetString("objective","PRISON ESCAPE");
			}
			else if ( m_iMapHasVIPSafetyZone == 1 )
			{
				event->SetString("objective","VIP RESCUE");
			}
			else if ( m_bMapHasBombTarget || m_bMapHasBombZone )
			{
				event->SetString("objective","BOMB TARGET");
			}
			else
			{
				event->SetString("objective","DEATHMATCH");
			}

			gameeventmanager->FireEvent( event );
		}
	
		UploadGameStats();

		//=============================================================================
		// HPE_BEGIN:
		// [pfreese] I commented out this call to CreateWeaponManager, as the 
		// CGameWeaponManager object doesn't appear to be actually used by the CSS
		// code, and in any case, the weapon manager does not support wildcards in 
		// entity names (as seemingly indicated) below. When the manager fails to 
		// create its factory, it removes itself in any case.
		//=============================================================================

		// CreateWeaponManager( "weapon_*", gpGlobals->maxClients * 2 );
		
		//=============================================================================
		// HPE_END
		//=============================================================================
	}

	void CCSGameRules::GiveC4()
	{
		enum {
			ALL_TERRORISTS = 0,
			HUMAN_TERRORISTS,
		};
		int iTerrorists[2][ABSOLUTE_PLAYER_LIMIT];
		int numAliveTs[2] = { 0, 0 };
		int lastBombGuyIndex[2] = { -1, -1 };

		//Create an array of the indeces of bomb carrier candidates
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

			if( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TERRORIST && numAliveTs[ALL_TERRORISTS] < ABSOLUTE_PLAYER_LIMIT  )
			{
				if ( pPlayer == m_pLastBombGuy )
				{
					lastBombGuyIndex[ALL_TERRORISTS] = numAliveTs[ALL_TERRORISTS];
					lastBombGuyIndex[HUMAN_TERRORISTS] = numAliveTs[HUMAN_TERRORISTS];
				}

				iTerrorists[ALL_TERRORISTS][numAliveTs[ALL_TERRORISTS]] = i;
				numAliveTs[ALL_TERRORISTS]++;
				if ( !pPlayer->IsBot() )
				{
					iTerrorists[HUMAN_TERRORISTS][numAliveTs[HUMAN_TERRORISTS]] = i;
					numAliveTs[HUMAN_TERRORISTS]++;
				}
			}
		}

		int which = cv_bot_defer_to_human.GetBool();
		if ( numAliveTs[HUMAN_TERRORISTS] == 0 )
		{
			which = ALL_TERRORISTS;
		}

		//pick one of the candidates randomly
		if( numAliveTs[which] > 0 )
		{
			int index = random->RandomInt(0,numAliveTs[which]-1);
			if ( lastBombGuyIndex[which] >= 0 )
			{
				// give the C4 sequentially
				index = (lastBombGuyIndex[which] + 1) % numAliveTs[which];
			}
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( iTerrorists[which][index] ) );

			Assert( pPlayer && pPlayer->GetTeamNumber() == TEAM_TERRORIST && pPlayer->IsAlive() );

			pPlayer->GiveNamedItem( WEAPON_C4_CLASSNAME );
			m_pLastBombGuy = pPlayer;

			//pPlayer->SetBombIcon();
			//pPlayer->pev->body = 1;
			
			pPlayer->m_iDisplayHistoryBits |= DHF_BOMB_RETRIEVED;
			pPlayer->HintMessage( "#Hint_you_have_the_bomb", false, true );

			// Log this information
			//UTIL_LogPrintf("\"%s<%i><%s><TERRORIST>\" triggered \"Spawned_With_The_Bomb\"\n", 
			//	STRING( pPlayer->GetPlayerName() ),
			//	GETPLAYERUSERID( pPlayer->edict() ),
			//	GETPLAYERAUTHID( pPlayer->edict() ) );
		}

		m_bBombDropped = false;
	}

	void CCSGameRules::Think()
	{
		CGameRules::Think();

		for ( int i = 0; i < GetNumberOfTeams(); i++ )
		{
			GetGlobalTeam( i )->Think();
		}

		///// Check game rules /////
		if ( CheckGameOver() )
		{
			return;
		}

		// have we hit the max rounds?
		if ( CheckMaxRounds() )
		{
			return;
		}

		// did somebaody hit the fraglimit ?
		if ( CheckFragLimit() )
		{
			return;
		}

		if ( CheckWinLimit() )
		{
			return;
		}

		
		// Check for the end of the round.
		if ( IsFreezePeriod() )
		{
			CheckFreezePeriodExpired();
		}
		else 
		{
			CheckRoundTimeExpired();
		}

		CheckLevelInitialized();
		
		if ( m_flRestartRoundTime > 0.0f && m_flRestartRoundTime <= gpGlobals->curtime )
		{
			bool botSpeaking = false;
			for ( int i=1; i <= gpGlobals->maxClients; ++i )
			{
				CBasePlayer *player = UTIL_PlayerByIndex( i );
				if (player == NULL)
					continue;

				if (!player->IsBot())
					continue;
				
				CCSBot *bot = dynamic_cast< CCSBot * >(player);
				if ( !bot )
					continue;

				if ( bot->IsUsingVoice() )
				{
					if ( gpGlobals->curtime > m_flRestartRoundTime + 10.0f )
					{
						Msg( "Ignoring speaking bot %s at round end\n", bot->GetPlayerName() );
					}
					else
					{
						botSpeaking = true;
						break;
					}
				}
			}

			if ( !botSpeaking )
			{
				RestartRound();
			}
		}
		
		if ( gpGlobals->curtime > m_tmNextPeriodicThink )
		{
			CheckRestartRound();
			m_tmNextPeriodicThink = gpGlobals->curtime + 1.0;
		}
	}


	// The bots do their processing after physics simulation etc so their visibility checks don't recompute
	// bone positions multiple times a frame.
	void CCSGameRules::EndGameFrame( void )
	{
		TheBots->StartFrame();

		BaseClass::EndGameFrame();
	}

	bool CCSGameRules::CheckGameOver()
	{
		if ( g_fGameOver )   // someone else quit the game already
		{
			//=============================================================================
			// HPE_BEGIN:
			// [Forrest] Calling ChangeLevel multiple times was causing IncrementMapCycleIndex
			// to skip over maps in the list.  Avoid this using a technique from CTeamplayRoundBasedRules::Think.
			//=============================================================================
			// check to see if we should change levels now
			if ( m_flIntermissionEndTime && ( m_flIntermissionEndTime < gpGlobals->curtime ) )
			{
				ChangeLevel(); // intermission is over

				// Don't run this code again
				m_flIntermissionEndTime = 0.f;
			}
			//=============================================================================
			// HPE_END
			//=============================================================================

			return true;
		}

		return false;
	}

	bool CCSGameRules::CheckFragLimit()
	{
		if ( fraglimit.GetInt() <= 0 )
			return false;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

			if ( pPlayer && pPlayer->FragCount() >= fraglimit.GetInt() )
			{
				const char *teamName = "UNKNOWN";
				if ( pPlayer->GetTeam() )
				{
					teamName = pPlayer->GetTeam()->GetName();
				}
				UTIL_LogPrintf("\"%s<%i><%s><%s>\" triggered \"Intermission_Kill_Limit\"\n", 
					pPlayer->GetPlayerName(),
					pPlayer->GetUserID(),
					pPlayer->GetNetworkIDString(),
					teamName
					);
				GoToIntermission();
				return true;
			}
		}

		return false;
	}

	bool CCSGameRules::CheckMaxRounds()
	{
		if ( mp_maxrounds.GetInt() != 0 )
		{
			if ( m_iTotalRoundsPlayed >= mp_maxrounds.GetInt() )
			{
				UTIL_LogPrintf("World triggered \"Intermission_Round_Limit\"\n");
				GoToIntermission();
				return true;
			}
		}
		
		return false;
	}


	bool CCSGameRules::CheckWinLimit()
	{
		// has one team won the specified number of rounds?
		if ( mp_winlimit.GetInt() != 0 )
		{
			if ( m_iNumCTWins >= mp_winlimit.GetInt() )
			{
				UTIL_LogPrintf("Team \"CT\" triggered \"Intermission_Win_Limit\"\n");
				GoToIntermission();
				return true;
			}
			if ( m_iNumTerroristWins >= mp_winlimit.GetInt() )
			{
				UTIL_LogPrintf("Team \"TERRORIST\" triggered \"Intermission_Win_Limit\"\n");
				GoToIntermission();
				return true;
			}
		}

		return false;
	}


	void CCSGameRules::CheckFreezePeriodExpired()
	{
		float startTime = m_fRoundStartTime;
		if ( !IsFinite( startTime ) )
		{
			Warning( "Infinite round start time!\n" );
			m_fRoundStartTime.GetForModify() = gpGlobals->curtime;
		}

		if ( IsFinite( startTime ) && gpGlobals->curtime < startTime )
		{
			return; // not time yet to start round
		}

		// Log this information
		UTIL_LogPrintf("World triggered \"Round_Start\"\n");

		char CT_sentence[40];
		char T_sentence[40];
		
		switch ( random->RandomInt( 0, 3 ) )
		{
		case 0:
			Q_strncpy(CT_sentence,"radio.moveout", sizeof( CT_sentence ) ); 
			Q_strncpy(T_sentence ,"radio.moveout", sizeof( T_sentence ) ); 
			break;

		case 1:
			Q_strncpy(CT_sentence, "radio.letsgo", sizeof( CT_sentence ) ); 
			Q_strncpy(T_sentence , "radio.letsgo", sizeof( T_sentence ) ); 
			break;

		case 2:
			Q_strncpy(CT_sentence , "radio.locknload", sizeof( CT_sentence ) );
			Q_strncpy(T_sentence , "radio.locknload", sizeof( T_sentence ) );
			break;

		default:
			Q_strncpy(CT_sentence , "radio.go", sizeof( CT_sentence ) );
			Q_strncpy(T_sentence , "radio.go", sizeof( T_sentence ) );
			break;
		}

		// More specific radio commands for the new scenarios : Prison & Assasination
		if (m_bMapHasEscapeZone == TRUE)
		{
			Q_strncpy(CT_sentence , "radio.elim", sizeof( CT_sentence ) );
			Q_strncpy(T_sentence , "radio.getout", sizeof( T_sentence ) );
		}
		else if (m_iMapHasVIPSafetyZone == 1)
		{
			Q_strncpy(CT_sentence , "radio.vip", sizeof( CT_sentence ) );
			Q_strncpy(T_sentence , "radio.locknload", sizeof( T_sentence ) );
		}

		// Freeze period expired: kill the flag
		m_bFreezePeriod = false;

		IGameEvent * event = gameeventmanager->CreateEvent( "round_freeze_end" );
		if ( event )
		{
			gameeventmanager->FireEvent( event );
		}

		// Update the timers for all clients and play a sound
		bool bCTPlayed = false;
		bool bTPlayed = false;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = CCSPlayer::Instance( i );
			if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
			{
				if ( pPlayer->State_Get() == STATE_ACTIVE )
				{
					if ( (pPlayer->GetTeamNumber() == TEAM_CT) && !bCTPlayed )
					{
						pPlayer->Radio( CT_sentence );
						bCTPlayed = true;
					}
					else if ( (pPlayer->GetTeamNumber() == TEAM_TERRORIST) && !bTPlayed )
					{
						pPlayer->Radio( T_sentence );
						bTPlayed = true;
					}

				}
				
				//pPlayer->SyncRoundTimer();
			}
		}
	}


	void CCSGameRules::CheckRoundTimeExpired()
	{
		if ( mp_ignore_round_win_conditions.GetBool() )
			return;

		if ( GetRoundRemainingTime() > 0 || m_iRoundWinStatus != WINNER_NONE ) 
			return; //We haven't completed other objectives, so go for this!.

		if( !m_bFirstConnected )
			return;

		// New code to get rid of round draws!!

		if ( m_bMapHasBombTarget )
		{
			//If the bomb is planted, don't let the round timer end the round.
			//keep going until the bomb explodes or is defused
			if( !m_bBombPlanted )
			{
				m_iAccountCT += 3250;
				
				m_iNumCTWins++;
				TerminateRound( mp_round_restart_delay.GetFloat(), Target_Saved );
				UpdateTeamScores();
				MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(TEAM_TERRORIST);
			}
		}
		else if ( m_bMapHasRescueZone )
		{
			m_iAccountTerrorist += 3250; 
			
			m_iNumTerroristWins++;
			TerminateRound( mp_round_restart_delay.GetFloat(), Hostages_Not_Rescued );
			UpdateTeamScores();
			MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(TEAM_CT);
		}
		else if ( m_bMapHasEscapeZone )
		{
			m_iNumCTWins++;
			TerminateRound( mp_round_restart_delay.GetFloat(), Terrorists_Not_Escaped );
			UpdateTeamScores();
		}
		else if ( m_iMapHasVIPSafetyZone == 1 )
		{
			m_iAccountTerrorist += 3250;
			m_iNumTerroristWins++;

			TerminateRound( mp_round_restart_delay.GetFloat(), VIP_Not_Escaped );
			UpdateTeamScores();
		}

#if defined( REPLAY_ENABLED )
		if ( g_pReplay )
		{
			// Write replay and stop recording if appropriate
			g_pReplay->SV_EndRecordingSession();
		}
#endif
	}

	void CCSGameRules::GoToIntermission( void )
	{
		Msg( "Going to intermission...\n" );

		IGameEvent *winEvent = gameeventmanager->CreateEvent( "cs_win_panel_match" );

		if( winEvent )
		{
			for ( int teamIndex = TEAM_TERRORIST; teamIndex <= TEAM_CT; teamIndex++ )
			{
				CTeam *team = GetGlobalTeam( teamIndex );
				if ( team )
				{
					float kills = CCS_GameStats.GetTeamStats(teamIndex)[CSSTAT_KILLS];
					float deaths = CCS_GameStats.GetTeamStats(teamIndex)[CSSTAT_DEATHS];
					// choose dialog variables to set depending on team
					switch ( teamIndex )
					{
					case TEAM_TERRORIST:
						winEvent->SetInt( "t_score", team->GetScore() );
						if(deaths == 0)
						{
							winEvent->SetFloat( "t_kd", kills );
						}
						else
						{
							winEvent->SetFloat( "t_kd", kills / deaths );
						}										
						winEvent->SetInt( "t_objectives_done", CCS_GameStats.GetTeamStats(teamIndex)[CSSTAT_OBJECTIVES_COMPLETED] );
						winEvent->SetInt( "t_money_earned", CCS_GameStats.GetTeamStats(teamIndex)[CSSTAT_MONEY_EARNED] );
						break;
					case TEAM_CT:
						winEvent->SetInt( "ct_score", team->GetScore() );
						if(deaths == 0)
						{
							winEvent->SetFloat( "ct_kd", kills );
						}
						else
						{
							winEvent->SetFloat( "ct_kd", kills / deaths );
						}
						winEvent->SetInt( "ct_objectives_done", CCS_GameStats.GetTeamStats(teamIndex)[CSSTAT_OBJECTIVES_COMPLETED] );
						winEvent->SetInt( "ct_money_earned", CCS_GameStats.GetTeamStats(teamIndex)[CSSTAT_MONEY_EARNED] );
						break;
					default:
						Assert( false );
						break;
					}
				}
			}

			gameeventmanager->FireEvent( winEvent );
		}

		BaseClass::GoToIntermission();

		// set all players to FL_FROZEN
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

			if ( pPlayer )
			{
				pPlayer->AddFlag( FL_FROZEN );
			}
		}

		// freeze players while in intermission
		m_bFreezePeriod = true;
	}

	int PlayerScoreInfoSort( const playerscore_t *p1, const playerscore_t *p2 )
	{
		// check frags
		if ( p1->iScore > p2->iScore )
			return -1;
		if ( p2->iScore > p1->iScore )
			return 1;

		// check index
		if ( p1->iPlayerIndex < p2->iPlayerIndex )
			return -1;

		return 1;
	}

#if defined (_DEBUG)
	void TestRoundWinpanel( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "round_end" );
		event->SetInt( "winner", TEAM_TERRORIST );

		if ( event )
		{
			gameeventmanager->FireEvent( event );
		}


		IGameEvent *event2 = gameeventmanager->CreateEvent( "player_death" );
		if ( event2 )
		{
			CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex(1) );
			
			// pCappingPlayers is a null terminated list of player indeces
			event2->SetInt("userid", pPlayer->GetUserID() );
			event2->SetInt("attacker", pPlayer->GetUserID() );
			event2->SetString("weapon", "Bare Hands" );
			event2->SetInt("headshot", 1 );
			event2->SetInt( "revenge", 1 );

			gameeventmanager->FireEvent( event2 );
		}

		IGameEvent *winEvent = gameeventmanager->CreateEvent( "cs_win_panel_round" );

		if ( winEvent )
		{
			if ( 1 )
			{
				if ( 0 /*team == m_iTimerWinTeam */)
				{
					// timer expired, defenders win
					// show total time that was defended
					winEvent->SetBool( "show_timer_defend", true );
					winEvent->SetInt( "timer_time", 0 /*m_pRoundTimer->GetTimerMaxLength() */);
				}
				else
				{
					// attackers win
					// show time it took for them to win
					winEvent->SetBool( "show_timer_attack", true );

					int iTimeElapsed = 90; //m_pRoundTimer->GetTimerMaxLength() - (int)m_pRoundTimer->GetTimeRemaining();
					winEvent->SetInt( "timer_time", iTimeElapsed );
				}
			}
			else
			{
				winEvent->SetBool( "show_timer_attack", false );
				winEvent->SetBool( "show_timer_defend", false );
			}

			int iLastEvent = Terrorists_Win;

			winEvent->SetInt( "final_event", iLastEvent );

			// Set the fun fact data in the event
			winEvent->SetString( "funfact_token", "#funfact_first_blood" );
			winEvent->SetInt( "funfact_player", 1 );
			winEvent->SetInt( "funfact_data1", 20 );
			winEvent->SetInt( "funfact_data2", 31 );
			winEvent->SetInt( "funfact_data3", 45 );

			gameeventmanager->FireEvent( winEvent );
		}
	}
	ConCommand test_round_winpanel( "test_round_winpanel", TestRoundWinpanel, "", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

	void TestMatchWinpanel( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "round_end" );
		event->SetInt( "winner", TEAM_TERRORIST );

		if ( event )
		{
			gameeventmanager->FireEvent( event );
		}

		IGameEvent *winEvent = gameeventmanager->CreateEvent( "cs_win_panel_match" );

		if ( winEvent )
		{
			winEvent->SetInt( "t_score", 4 );
			winEvent->SetInt( "ct_score", 1 );

			winEvent->SetFloat( "t_kd", 1.8f );
			winEvent->SetFloat( "ct_kd", 0.4f );

			winEvent->SetInt( "t_objectives_done", 5 );
			winEvent->SetInt( "ct_objectives_done", 2 );

			winEvent->SetInt( "t_money_earned", 30000 );
			winEvent->SetInt( "ct_money_earned", 19999 );

			gameeventmanager->FireEvent( winEvent );
		}
	}
	ConCommand test_match_winpanel( "test_match_winpanel", TestMatchWinpanel, "", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

	void TestFreezePanel( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "freezecam_started" );
		if ( event )
		{
			gameeventmanager->FireEvent( event );
		}

		IGameEvent *winEvent = gameeventmanager->CreateEvent( "show_freezepanel" );

		if ( winEvent )
		{
			winEvent->SetInt( "killer", 1 );
			gameeventmanager->FireEvent( winEvent );
		}
	}
	ConCommand test_freezepanel( "test_freezepanel", TestFreezePanel, "", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
#endif // _DEBUG

	static void PrintToConsole( CBasePlayer *player, const char *text )
	{
		if ( player )
		{
			ClientPrint( player, HUD_PRINTCONSOLE, text );
		}
		else
		{
			Msg( "%s", text );
		}
	}

	void CCSGameRules::DumpTimers( void ) const
	{
		extern ConVar bot_join_delay;
		CBasePlayer *player = UTIL_GetCommandClient();
		CFmtStr str;

		PrintToConsole( player, str.sprintf( "Timers and related info at %f:\n", gpGlobals->curtime ) );
		PrintToConsole( player, str.sprintf( "m_bCompleteReset: %d\n", m_bCompleteReset ) );
		PrintToConsole( player, str.sprintf( "m_iTotalRoundsPlayed: %d\n", m_iTotalRoundsPlayed ) );
		PrintToConsole( player, str.sprintf( "m_iRoundTime: %d\n", m_iRoundTime.Get() ) );
		PrintToConsole( player, str.sprintf( "m_iRoundWinStatus: %d\n", m_iRoundWinStatus ) );

		PrintToConsole( player, str.sprintf( "first connected: %d\n", m_bFirstConnected ) );
		PrintToConsole( player, str.sprintf( "intermission end time: %f\n", m_flIntermissionEndTime ) );
		PrintToConsole( player, str.sprintf( "freeze period: %d\n", m_bFreezePeriod.Get() ) );
		PrintToConsole( player, str.sprintf( "round restart time: %f\n", m_flRestartRoundTime ) );
		PrintToConsole( player, str.sprintf( "game start time: %f\n", m_flGameStartTime.Get() ) );
		PrintToConsole( player, str.sprintf( "m_fRoundStartTime: %f\n", m_fRoundStartTime.Get() ) );
		PrintToConsole( player, str.sprintf( "freeze time: %d\n", m_iFreezeTime ) );
		PrintToConsole( player, str.sprintf( "next think: %f\n", m_tmNextPeriodicThink ) );

		PrintToConsole( player, str.sprintf( "fraglimit: %d\n", fraglimit.GetInt() ) );
		PrintToConsole( player, str.sprintf( "mp_maxrounds: %d\n", mp_maxrounds.GetInt() ) );
		PrintToConsole( player, str.sprintf( "mp_winlimit: %d\n", mp_winlimit.GetInt() ) );
		PrintToConsole( player, str.sprintf( "bot_quota: %d\n", cv_bot_quota.GetInt() ) );
		PrintToConsole( player, str.sprintf( "bot_quota_mode: %s\n", cv_bot_quota_mode.GetString() ) );
		PrintToConsole( player, str.sprintf( "bot_join_after_player: %d\n", cv_bot_join_after_player.GetInt() ) );
		PrintToConsole( player, str.sprintf( "bot_join_delay: %d\n", bot_join_delay.GetInt() ) );
		PrintToConsole( player, str.sprintf( "nextlevel: %s\n", nextlevel.GetString() ) );

		int humansInGame = UTIL_HumansInGame( true );
		int botsInGame = UTIL_BotsInGame();
		PrintToConsole( player, str.sprintf( "%d humans and %d bots in game\n", humansInGame, botsInGame ) );

		PrintToConsole( player, str.sprintf( "num CTs (spawnable): %d (%d)\n", m_iNumCT, m_iNumSpawnableCT ) );
		PrintToConsole( player, str.sprintf( "num Ts (spawnable): %d (%d)\n", m_iNumTerrorist, m_iNumSpawnableTerrorist ) );

		if ( g_fGameOver )
		{
			PrintToConsole( player, str.sprintf( "Game is over!\n" ) );
		}
		PrintToConsole( player, str.sprintf( "\n" ) );
	}

	CON_COMMAND( mp_dump_timers, "Prints round timers to the console for debugging" )
	{
		if ( !UTIL_IsCommandIssuedByServerAdmin() )
			return;

		if ( CSGameRules() )
		{
			CSGameRules()->DumpTimers();
		}
	}


	// living players on the given team need to be marked as not receiving any money
	// next round.
	void CCSGameRules::MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(int team)
	{
		int playerNum;
		for (playerNum = 1; playerNum <= gpGlobals->maxClients; ++playerNum)
		{
			CCSPlayer *player = (CCSPlayer *)UTIL_PlayerByIndex(playerNum);
			if (player == NULL)
			{
				continue;
			}

			if ((player->GetTeamNumber() == team) && (player->IsAlive()))
			{
				player->MarkAsNotReceivingMoneyNextRound();
			}
		}
	}


	void CCSGameRules::CheckLevelInitialized( void )
	{
		if ( !m_bLevelInitialized )
		{
			// Count the number of spawn points for each team
			// This determines the maximum number of players allowed on each

			CBaseEntity* ent = NULL; 
			
			m_iSpawnPointCount_Terrorist	= 0;
			m_iSpawnPointCount_CT			= 0;

			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_terrorist" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) )
				{
					m_iSpawnPointCount_Terrorist++;
				}
				else
				{
					Warning("Invalid terrorist spawnpoint at (%.1f,%.1f,%.1f)\n",
						ent->GetAbsOrigin()[0],ent->GetAbsOrigin()[2],ent->GetAbsOrigin()[2] );
				}
			}

			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_counterterrorist" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) ) 
				{
					m_iSpawnPointCount_CT++;
				}
				else
				{
					Warning("Invalid counterterrorist spawnpoint at (%.1f,%.1f,%.1f)\n",
						ent->GetAbsOrigin()[0],ent->GetAbsOrigin()[2],ent->GetAbsOrigin()[2] );
				}
			}

			// Is this a logo map?
			if ( gEntList.FindEntityByClassname( NULL, "info_player_logo" ) )
				m_bLogoMap = true;

			m_bLevelInitialized = true;
		}
	}

	void CCSGameRules::ShowSpawnPoints( void )
	{
		CBaseEntity* ent = NULL;
		
		while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_terrorist" ) ) != NULL )
		{
			if ( IsSpawnPointValid( ent, NULL ) )
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 200, 600 );
			}
			else
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, 600);
			}
		}

		while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_counterterrorist" ) ) != NULL )
		{
			if ( IsSpawnPointValid( ent, NULL ) ) 
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 200, 600 );
			}
			else
			{
				NDebugOverlay::Box( ent->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 200, 600 );
			}
		}
	}

	void CCSGameRules::CheckRestartRound( void )
	{
		// Restart the game if specified by the server
		int iRestartDelay = mp_restartgame.GetInt();

		if ( iRestartDelay > 0 )
		{
			if ( iRestartDelay > 60 )
				iRestartDelay = 60;

			// log the restart
			UTIL_LogPrintf( "World triggered \"Restart_Round_(%i_%s)\"\n", iRestartDelay, iRestartDelay == 1 ? "second" : "seconds" );

			UTIL_LogPrintf( "Team \"CT\" scored \"%i\" with \"%i\" players\n", m_iNumCTWins, m_iNumCT );
			UTIL_LogPrintf( "Team \"TERRORIST\" scored \"%i\" with \"%i\" players\n", m_iNumTerroristWins, m_iNumTerrorist );

			// let the players know
			char strRestartDelay[64];
			Q_snprintf( strRestartDelay, sizeof( strRestartDelay ), "%d", iRestartDelay );
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "#Game_will_restart_in", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS" );
			UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#Game_will_restart_in", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS" );

			m_flRestartRoundTime = gpGlobals->curtime + iRestartDelay;
			m_bCompleteReset = true;
			mp_restartgame.SetValue( 0 );
		}
	}


	class SetHumanTeamFunctor
	{
	public:
		SetHumanTeamFunctor( int targetTeam )
		{
			m_targetTeam = targetTeam;
			m_sourceTeam = ( m_targetTeam == TEAM_CT ) ? TEAM_TERRORIST : TEAM_CT;

			m_traitors.MakeReliable();
			m_loyalists.MakeReliable();
			m_loyalists.AddAllPlayers();
		}

		bool operator()( CBasePlayer *basePlayer )
		{
			CCSPlayer *player = ToCSPlayer( basePlayer );
			if ( !player )
				return true;

			if ( player->IsBot() )
				return true;

			if ( player->GetTeamNumber() != m_sourceTeam )
				return true;

			if ( player->State_Get() == STATE_PICKINGCLASS )
				return true;

			if ( CSGameRules()->TeamFull( m_targetTeam ) )
				return false;

			if ( CSGameRules()->TeamStacked( m_targetTeam, m_sourceTeam ) )
				return false;

			player->SwitchTeam( m_targetTeam );
			m_traitors.AddRecipient( player );
			m_loyalists.RemoveRecipient( player );

			return true;
		}

		void SendNotice( void )
		{
			if ( m_traitors.GetRecipientCount() > 0 )
			{
				UTIL_ClientPrintFilter( m_traitors, HUD_PRINTCENTER, "#Player_Balanced" );
				UTIL_ClientPrintFilter( m_loyalists, HUD_PRINTCENTER, "#Teams_Balanced" );
			}
		}

	private:
		int m_targetTeam;
		int m_sourceTeam;

		CRecipientFilter m_traitors;
		CRecipientFilter m_loyalists;
	};


	void CCSGameRules::MoveHumansToHumanTeam( void )
	{
		int targetTeam = GetHumanTeam();
		if ( targetTeam != TEAM_TERRORIST && targetTeam != TEAM_CT )
			return;

		SetHumanTeamFunctor setTeam( targetTeam );
		ForEachPlayer( setTeam );

		setTeam.SendNotice();
	}


	void CCSGameRules::BalanceTeams( void )
	{
		int iTeamToSwap = TEAM_UNASSIGNED;
		int iNumToSwap;

		if (m_iMapHasVIPSafetyZone == 1) // The ratio for teams is different for Assasination maps
		{
			int iDesiredNumCT, iDesiredNumTerrorist;
			
			if ( (m_iNumCT + m_iNumTerrorist)%2 != 0)	// uneven number of players
				iDesiredNumCT			= (int)((m_iNumCT + m_iNumTerrorist) * 0.55) + 1;
			else
				iDesiredNumCT			= (int)((m_iNumCT + m_iNumTerrorist)/2);
			iDesiredNumTerrorist	= (m_iNumCT + m_iNumTerrorist) - iDesiredNumCT;

			if ( m_iNumCT < iDesiredNumCT )
			{
				iTeamToSwap = TEAM_TERRORIST;
				iNumToSwap = iDesiredNumCT - m_iNumCT;
			}
			else if ( m_iNumTerrorist < iDesiredNumTerrorist )
			{
				iTeamToSwap = TEAM_CT;
				iNumToSwap = iDesiredNumTerrorist - m_iNumTerrorist;
			}
			else
				return;
		}
		else
		{
			if (m_iNumCT > m_iNumTerrorist)
			{
				iTeamToSwap = TEAM_CT;
				iNumToSwap = (m_iNumCT - m_iNumTerrorist)/2;
				
			}
			else if (m_iNumTerrorist > m_iNumCT)
			{
				iTeamToSwap = TEAM_TERRORIST;
				iNumToSwap = (m_iNumTerrorist - m_iNumCT)/2;
			}
			else
			{
				return;	// Teams are even.. Get out of here.
			}
		}

		if (iNumToSwap > 3) // Don't swap more than 3 players at a time.. This is a naive method of avoiding infinite loops.
			iNumToSwap = 3;

		int iTragetTeam = TEAM_UNASSIGNED;

		if ( iTeamToSwap == TEAM_CT )
		{
			iTragetTeam = TEAM_TERRORIST;
		}
		else if ( iTeamToSwap == TEAM_TERRORIST )
		{
			iTragetTeam = TEAM_CT;
		}
		else
		{
			// no valid team to swap
			return;
		}

		CRecipientFilter traitors;
		CRecipientFilter loyalists;

		traitors.MakeReliable();
		loyalists.MakeReliable();
		loyalists.AddAllPlayers();

		for (int i = 0; i < iNumToSwap; i++)
		{
			// last person to join the server
			int iHighestUserID = -1;
			CCSPlayer *pPlayerToSwap = NULL;

			// check if target team is full, exit if so
			if ( TeamFull(iTragetTeam) )
				break;

			// search for player with highest UserID = most recently joined to switch over
			for ( int j = 1; j <= gpGlobals->maxClients; j++ )
			{
				CCSPlayer *pPlayer = (CCSPlayer *)UTIL_PlayerByIndex( j );

				if ( !pPlayer )
					continue;

				CCSBot *bot = dynamic_cast< CCSBot * >(pPlayer);
				if ( bot )
					continue; // don't swap bots - the bot system will handle that

				if ( pPlayer &&
					 ( m_pVIP != pPlayer ) && 
					 ( pPlayer->GetTeamNumber() == iTeamToSwap ) && 
					 ( engine->GetPlayerUserId( pPlayer->edict() ) > iHighestUserID ) &&
					 ( pPlayer->State_Get() != STATE_PICKINGCLASS ) )
					{
						iHighestUserID = engine->GetPlayerUserId( pPlayer->edict() );
						pPlayerToSwap = pPlayer;
					}
			}

			if ( pPlayerToSwap != NULL )
			{
				traitors.AddRecipient( pPlayerToSwap );
				loyalists.RemoveRecipient( pPlayerToSwap );
				pPlayerToSwap->SwitchTeam( iTragetTeam );
			}
		}

		if ( traitors.GetRecipientCount() > 0 )
		{
			UTIL_ClientPrintFilter( traitors, HUD_PRINTCENTER, "#Player_Balanced" );
			UTIL_ClientPrintFilter( loyalists, HUD_PRINTCENTER, "#Teams_Balanced" );
		}
	}


	bool CCSGameRules::TeamFull( int team_id )
	{
		CheckLevelInitialized();

		switch ( team_id )
		{
		case TEAM_TERRORIST:
			return m_iNumTerrorist >= m_iSpawnPointCount_Terrorist;

		case TEAM_CT:
			return m_iNumCT >= m_iSpawnPointCount_CT;
		}

		return false;
	}
	
	int CCSGameRules::GetHumanTeam()
	{
		if ( FStrEq( "CT", mp_humanteam.GetString() ) )
		{
			return TEAM_CT;
		}
		else if ( FStrEq( "T", mp_humanteam.GetString() ) )
		{
			return TEAM_TERRORIST;
		}
		
		return TEAM_UNASSIGNED;
	}

	int CCSGameRules::SelectDefaultTeam( bool ignoreBots /*= false*/ )
	{
		if ( ignoreBots && ( FStrEq( cv_bot_join_team.GetString(), "T" ) || FStrEq( cv_bot_join_team.GetString(), "CT" ) ) )
		{
			ignoreBots = false;	// don't ignore bots when they can't switch teams
		}

		if ( ignoreBots && !mp_autoteambalance.GetBool() )
		{
			ignoreBots = false;	// don't ignore bots when they can't switch teams
		}

		int team = TEAM_UNASSIGNED;
		int numTerrorists = m_iNumTerrorist;
		int numCTs = m_iNumCT;
		if ( ignoreBots )
		{
			numTerrorists = UTIL_HumansOnTeam( TEAM_TERRORIST );
			numCTs = UTIL_HumansOnTeam( TEAM_CT );
		}

		// Choose the team that's lacking players
		if ( numTerrorists < numCTs )
		{
			team = TEAM_TERRORIST;
		}
		else if ( numTerrorists > numCTs )
		{
			team = TEAM_CT;
		}
		// Choose the team that's losing
		else if ( m_iNumTerroristWins < m_iNumCTWins )
		{
			team = TEAM_TERRORIST;
		}
		else if ( m_iNumCTWins < m_iNumTerroristWins )
		{
			team = TEAM_CT;
		}
		else
		{
			// Teams and scores are equal, pick a random team
			if ( random->RandomInt( 0, 1 ) == 0 )
			{
				team = TEAM_CT;
			}
			else
			{
				team = TEAM_TERRORIST;
			}
		}

		if ( TeamFull( team ) )
		{
			// Pick the opposite team
			if ( team == TEAM_TERRORIST )
			{
				team = TEAM_CT;
			}
			else
			{
				team = TEAM_TERRORIST;
			}

			// No choices left
			if ( TeamFull( team ) )
				return TEAM_UNASSIGNED;
		}

		return team;
	}

	//checks to see if the desired team is stacked, returns true if it is
	bool CCSGameRules::TeamStacked( int newTeam_id, int curTeam_id  )
	{
		//players are allowed to change to their own team
		if(newTeam_id == curTeam_id)
			return false;

		// if mp_limitteams is 0, don't check
		if ( mp_limitteams.GetInt() == 0 )
			return false;

		switch ( newTeam_id )
		{
		case TEAM_TERRORIST:
			if(curTeam_id != TEAM_UNASSIGNED && curTeam_id != TEAM_SPECTATOR)
			{
				if((m_iNumTerrorist + 1) > (m_iNumCT + mp_limitteams.GetInt() - 1))
					return true;
				else
					return false;
			}
			else
			{
				if((m_iNumTerrorist + 1) > (m_iNumCT + mp_limitteams.GetInt()))
					return true;
				else
					return false;
			}
			break;
		case TEAM_CT:
			if(curTeam_id != TEAM_UNASSIGNED && curTeam_id != TEAM_SPECTATOR)
			{
				if((m_iNumCT + 1) > (m_iNumTerrorist + mp_limitteams.GetInt() - 1))
					return true;
				else
					return false;
			}
			else
			{
				if((m_iNumCT + 1) > (m_iNumTerrorist + mp_limitteams.GetInt()))
					return true;
				else
					return false;
			}
			break;
		}

		return false;
	}


	//=========================================================
	//=========================================================
	bool CCSGameRules::FPlayerCanRespawn( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = ToCSPlayer( pBasePlayer );
		if ( !pPlayer )
			Error( "FPlayerCanRespawn: pPlayer=0" );

		// Player cannot respawn twice in a round
		if ( pPlayer->m_iNumSpawns > 0 && m_bFirstConnected )
			return false;

		// If they're dead after the map has ended, and it's about to start the next round,
		// wait for the round restart to respawn them.
		if ( gpGlobals->curtime < m_flRestartRoundTime )
			return false;

		// Only valid team members can spawn
		if ( pPlayer->GetTeamNumber() != TEAM_CT && pPlayer->GetTeamNumber() != TEAM_TERRORIST )
			return false;

		// Only players with a valid class can spawn
		if ( pPlayer->GetClass() == CS_CLASS_NONE )
			return false;

		// Player cannot respawn until next round if more than 20 seconds in

		// Tabulate the number of players on each team.
		m_iNumCT = GetGlobalTeam( TEAM_CT )->GetNumPlayers();
		m_iNumTerrorist = GetGlobalTeam( TEAM_TERRORIST )->GetNumPlayers();

		if ( m_iNumTerrorist > 0 && m_iNumCT > 0 )
		{
			if ( gpGlobals->curtime > (m_fRoundStartTime + 20) )
			{
				//If this player just connected and fadetoblack is on, then maybe
				//the server admin doesn't want him peeking around.
				color32_s clr = {0,0,0,255};
				if ( mp_fadetoblack.GetBool() )
				{
					UTIL_ScreenFade( pPlayer, clr, 3, 3, FFADE_OUT | FFADE_STAYOUT );
				}

				return false;
			}
		}

		// Player cannot respawn while in the Choose Appearance menu
		//if ( pPlayer->m_iMenu == Menu_ChooseAppearance )
		//	return false;

		return true;
	}

	void CCSGameRules::TerminateRound(float tmDelay, int iReason )
	{
		variant_t emptyVariant;
		int iWinnerTeam = WINNER_NONE;
		const char *text = "UNKNOWN";
				
		// UTIL_ClientPrintAll( HUD_PRINTCENTER, sentence );

		switch ( iReason )
		{
// Terror wins:
			case Target_Bombed:	
				text = "#Target_Bombed";
				iWinnerTeam = WINNER_TER;
				break;

			case VIP_Assassinated:
				text = "#VIP_Assassinated";
				iWinnerTeam = WINNER_TER;
				break;

			case Terrorists_Escaped:
				text = "#Terrorists_Escaped";
				iWinnerTeam = WINNER_TER;
				break;

			case Terrorists_Win:
				text = "#Terrorists_Win";
				iWinnerTeam = WINNER_TER;
				break;

			case Hostages_Not_Rescued:
				text = "#Hostages_Not_Rescued";
				iWinnerTeam = WINNER_TER;
				break;

			case VIP_Not_Escaped:
				text = "#VIP_Not_Escaped";
				iWinnerTeam = WINNER_TER;
				break;
// CT wins:
			case VIP_Escaped:
				text = "#VIP_Escaped";
				iWinnerTeam = WINNER_CT;
				break;

			case CTs_PreventEscape:
				text = "#CTs_PreventEscape";
				iWinnerTeam = WINNER_CT;
				break;

			case Escaping_Terrorists_Neutralized:
				text = "#Escaping_Terrorists_Neutralized";
				iWinnerTeam = WINNER_CT;
				break;

			case Bomb_Defused:
				text = "#Bomb_Defused";
				iWinnerTeam = WINNER_CT;
				break;

			case CTs_Win:
				text = "#CTs_Win";
				iWinnerTeam = WINNER_CT;
				break;

			case All_Hostages_Rescued:
				text = "#All_Hostages_Rescued";
				iWinnerTeam = WINNER_CT;
				break;

			case Target_Saved:
				text = "#Target_Saved";
				iWinnerTeam = WINNER_CT;
				break;

			case Terrorists_Not_Escaped:
				text = "#Terrorists_Not_Escaped";
				iWinnerTeam = WINNER_CT;
				break;
// no winners:
			case Game_Commencing:
				text = "#Game_Commencing";
				iWinnerTeam = WINNER_DRAW;
				break;

			case Round_Draw:
				text = "#Round_Draw";
				iWinnerTeam = WINNER_DRAW;
				break;

			default:
				DevMsg("TerminateRound: unknown round end ID %i\n", iReason );
				break;
		}

		m_iRoundWinStatus = iWinnerTeam;
		m_flRestartRoundTime = gpGlobals->curtime + tmDelay;

		if ( iWinnerTeam == WINNER_CT )
		{
			for( int i=0;i<g_Hostages.Count();i++ )
				g_Hostages[i]->AcceptInput( "CTsWin", NULL, NULL, emptyVariant, 0 );
		}

		else if ( iWinnerTeam == WINNER_TER )
		{
			for( int i=0;i<g_Hostages.Count();i++ )
				g_Hostages[i]->AcceptInput( "TerroristsWin", NULL, NULL, emptyVariant, 0 );
		}
		else
		{
			Assert( iWinnerTeam == WINNER_NONE || iWinnerTeam == WINNER_DRAW );
		}

		//=============================================================================
		// HPE_BEGIN:		
		//=============================================================================

		// [tj] Check for any non-player-specific achievements.
		ProcessEndOfRoundAchievements(iWinnerTeam, iReason);

		if( iReason != Game_Commencing )
		{
			// [pfreese] Setup and send win panel event (primarily funfact data)

			FunFact funfact;
			funfact.szLocalizationToken = "";
			funfact.iPlayer = 0;
			funfact.iData1 = 0;
			funfact.iData2 = 0;
			funfact.iData3 = 0;

			m_pFunFactManager->GetRoundEndFunFact( iWinnerTeam, iReason, funfact);

			//Send all the info needed for the win panel
			IGameEvent *winEvent = gameeventmanager->CreateEvent( "cs_win_panel_round" );

			if ( winEvent )
			{
				// determine what categories to send
				if ( GetRoundRemainingTime() <= 0 )
				{
					// timer expired, defenders win
					// show total time that was defended
					winEvent->SetBool( "show_timer_defend", true );
					winEvent->SetInt( "timer_time", m_iRoundTime );
				}
				else
				{
					// attackers win
					// show time it took for them to win
					winEvent->SetBool( "show_timer_attack", true );

					int iTimeElapsed = m_iRoundTime - GetRoundRemainingTime();
					winEvent->SetInt( "timer_time", iTimeElapsed );
				}

				winEvent->SetInt( "final_event", iReason );

				// Set the fun fact data in the event
				winEvent->SetString( "funfact_token", funfact.szLocalizationToken);
				winEvent->SetInt( "funfact_player", funfact.iPlayer );
				winEvent->SetInt( "funfact_data1", funfact.iData1 );
				winEvent->SetInt( "funfact_data2", funfact.iData2 );
				winEvent->SetInt( "funfact_data3", funfact.iData3 );
				gameeventmanager->FireEvent( winEvent );
			}
		}

		// [tj] Inform players that the round is over
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
			if(pPlayer)
			{
				pPlayer->OnRoundEnd(iWinnerTeam, iReason);
			}
		}
		//=============================================================================
		// HPE_END
		//=============================================================================

		IGameEvent * event = gameeventmanager->CreateEvent( "round_end" );
		if ( event )
		{
			event->SetInt( "winner", iWinnerTeam );
			event->SetInt( "reason", iReason );
			event->SetString( "message", text );
			event->SetInt( "priority", 6 ); // HLTV event priority, not transmitted
			gameeventmanager->FireEvent( event );
		}

		if ( GetMapRemainingTime() == 0.0f  )
		{
			UTIL_LogPrintf("World triggered \"Intermission_Time_Limit\"\n");
			GoToIntermission();
		}
	}

	//=============================================================================
	// HPE_BEGIN:	
	//=============================================================================

	// Helper to determine if all players on a team are playing for the same clan
	static bool IsClanTeam( CTeam *pTeam )
	{
		uint32 iTeamClan = 0;
		for ( int iPlayer = 0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
		{
			CBasePlayer *pPlayer = pTeam->GetPlayer( iPlayer );
			if ( !pPlayer )
				return false;

			const char *pClanID = engine->GetClientConVarValue( pPlayer->entindex(), "cl_clanid" );
			uint32 iPlayerClan = atoi( pClanID );
			if ( iPlayer == 0 )
			{
				// Initialize the team clan
				iTeamClan = iPlayerClan;
			}
			else
			{
				if ( iPlayerClan != iTeamClan || iPlayerClan == 0 )
					return false;
			}
		}
		return iTeamClan != 0;
	}

	// [tj] This is where we check non-player-specific that occur at the end of the round
	void CCSGameRules::ProcessEndOfRoundAchievements(int iWinnerTeam, int iReason)
	{
		if (iWinnerTeam == WINNER_CT || iWinnerTeam == WINNER_TER)
		{
			int losingTeamId = (iWinnerTeam == TEAM_CT) ? TEAM_TERRORIST : TEAM_CT;
			CTeam* losingTeam = GetGlobalTeam(losingTeamId);

			
			//Check for players we should ignore when checking team size.
			int ignoreCount = 0;
			
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CCSPlayer* pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
				if (pPlayer)
				{
					int teamNum = pPlayer->GetTeamNumber();
					if ( teamNum == losingTeamId )
					{
						if (pPlayer->WasNotKilledNaturally())
						{
							ignoreCount++;
						}
					}
				}
			}


			// [tj] Check extermination with no losses achievement
			if ( ( ( iReason == CTs_Win && m_bNoCTsKilled ) || ( iReason == Terrorists_Win && m_bNoTerroristsKilled ) ) 
				&& losingTeam && losingTeam->GetNumPlayers() - ignoreCount >= AchievementConsts::DefaultMinOpponentsForAchievement)
			{
				CTeam *pTeam = GetGlobalTeam( iWinnerTeam );

				for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
					Assert( pPlayer );
					if ( !pPlayer )
						continue;

					pPlayer->AwardAchievement(CSLosslessExtermination);
				}
			}

			// [tj] Check flawless victory achievement - currently requiring extermination
			if (((iReason == CTs_Win && m_bNoCTsDamaged) || (iReason == Terrorists_Win && m_bNoTerroristsDamaged))
				&& losingTeam && losingTeam->GetNumPlayers() - ignoreCount >= AchievementConsts::DefaultMinOpponentsForAchievement)
			{
				CTeam *pTeam = GetGlobalTeam( iWinnerTeam );

				for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
					Assert( pPlayer );
					if ( !pPlayer )
						continue;

					pPlayer->AwardAchievement(CSFlawlessVictory);
				}
			}

			// [tj] Check bloodless victory achievement
			if (((iWinnerTeam == TEAM_TERRORIST && m_bNoCTsKilled) || (iWinnerTeam == Terrorists_Win && m_bNoTerroristsKilled))
				&& losingTeam && losingTeam->GetNumPlayers() >= AchievementConsts::DefaultMinOpponentsForAchievement)
			{
				CTeam *pTeam = GetGlobalTeam( iWinnerTeam );

				for ( int iPlayer=0; iPlayer < pTeam->GetNumPlayers(); iPlayer++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( pTeam->GetPlayer( iPlayer ) );
					Assert( pPlayer );
					if ( !pPlayer )
						continue;

					pPlayer->AwardAchievement(CSBloodlessVictory);
				}
			}

			// Check the clan match achievement
			CTeam *pWinningTeam = GetGlobalTeam( iWinnerTeam );
			if ( pWinningTeam && pWinningTeam->GetNumPlayers() >= AchievementConsts::DefaultMinOpponentsForAchievement &&
				 losingTeam && losingTeam->GetNumPlayers() - ignoreCount >= AchievementConsts::DefaultMinOpponentsForAchievement &&
				 IsClanTeam( pWinningTeam ) && IsClanTeam( losingTeam ) )
			{
				for ( int iPlayer=0; iPlayer < pWinningTeam->GetNumPlayers(); iPlayer++ )
				{
					CCSPlayer *pPlayer = ToCSPlayer( pWinningTeam->GetPlayer( iPlayer ) );
					if ( !pPlayer )
						continue;

					pPlayer->AwardAchievement( CSWinClanMatch );
				}
			}
		}
	}

	//[tj] Counts the number of players in each category in the struct (dead, alive, etc...)
	void CCSGameRules::GetPlayerCounts(TeamPlayerCounts teamCounts[TEAM_MAXCOUNT])
	{
		memset(teamCounts, 0, sizeof(TeamPlayerCounts) * TEAM_MAXCOUNT);

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer* pPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
			if (pPlayer)
			{
				int iTeam = pPlayer->GetTeamNumber();

				if (iTeam >= 0 && iTeam < TEAM_MAXCOUNT)
				{
					++teamCounts[iTeam].totalPlayers;
					if (pPlayer->IsAlive())
					{
						++teamCounts[iTeam].totalAlivePlayers;
					}
					else
					{
						++teamCounts[iTeam].totalDeadPlayers;

						//If the player has joined a team bit isn't in the game yet
						if (pPlayer->State_Get() == STATE_PICKINGCLASS)
						{
							++teamCounts[iTeam].unenteredPlayers;
						}
						else if (pPlayer->WasNotKilledNaturally())
						{
							++teamCounts[iTeam].suicidedPlayers;
						}
						else
						{
							++teamCounts[iTeam].killedPlayers;
						}						
					}
				}
			}
		}
	}
	//=============================================================================
	// HPE_END
	//=============================================================================

	void CCSGameRules::UpdateTeamScores()
	{
		CTeam *pTerrorists = GetGlobalTeam( TEAM_TERRORIST );
		CTeam *pCTs = GetGlobalTeam( TEAM_CT );

		Assert( pTerrorists && pCTs );

		if( pTerrorists )
			pTerrorists->SetScore( m_iNumTerroristWins );

		if( pCTs )
			pCTs->SetScore( m_iNumCTWins );
	}


	void CCSGameRules::CheckMapConditions()
	{
		// Check to see if this map has a bomb target in it
		if ( gEntList.FindEntityByClassname( NULL, "func_bomb_target" ) )
		{
			m_bMapHasBombTarget		= true;
			m_bMapHasBombZone		= true;
		}
		else if ( gEntList.FindEntityByClassname( NULL, "info_bomb_target" ) )
		{
			m_bMapHasBombTarget		= true;
			m_bMapHasBombZone		= false;
		}
		else
		{
			m_bMapHasBombTarget		= false;
			m_bMapHasBombZone		= false;
		}

		// See if the map has func_buyzone entities
		// Used by CBasePlayer::HandleSignals() to support maps without these entities
		if ( gEntList.FindEntityByClassname( NULL, "func_buyzone" ) )
		{
			m_bMapHasBuyZone = true;
		}
		else
		{
			m_bMapHasBuyZone = false;
		}

		// Check to see if this map has hostage rescue zones
		if ( gEntList.FindEntityByClassname( NULL, "func_hostage_rescue" ) )
		{
			m_bMapHasRescueZone = true;
		}
		else
		{
			m_bMapHasRescueZone = false;
		}

		// GOOSEMAN : See if this map has func_escapezone entities
		if ( gEntList.FindEntityByClassname( NULL, "func_escapezone" ) )
		{
			m_bMapHasEscapeZone = true;
		}
		else
		{
			m_bMapHasEscapeZone = false;
		}

		// Check to see if this map has VIP safety zones
		if ( gEntList.FindEntityByClassname( NULL, "func_vip_safetyzone" ) )
		{
			m_iMapHasVIPSafetyZone = 1;
		}
		else
		{
			m_iMapHasVIPSafetyZone = 2;
		}
	}


	void CCSGameRules::SwapAllPlayers()
	{
		// MOTODO we have to make sure that enought spaning points exits
		Assert ( 0 );
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			/* CCSPlayer *pPlayer = CCSPlayer::Instance( i );
			if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
				pPlayer->SwitchTeam(); */
		}

		// Swap Team victories
		int iTemp;

		iTemp = m_iNumCTWins;
		m_iNumCTWins = m_iNumTerroristWins;
		m_iNumTerroristWins = iTemp;
		
		// Update the clients team score
		UpdateTeamScores();
	}


	bool CS_FindInList( const char **pStrings, const char *pToFind )
	{
		return FindInList( pStrings, pToFind );
	}

	void CCSGameRules::CleanUpMap()
	{
		if (IsLogoMap())
			return;

		// Recreate all the map entities from the map data (preserving their indices),
		// then remove everything else except the players.

		// Get rid of all entities except players.
		CBaseEntity *pCur = gEntList.FirstEnt();
		while ( pCur )
		{
			CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* >( pCur );
			// Weapons with owners don't want to be removed..
			if ( pWeapon )
			{
                //=============================================================================
                // HPE_BEGIN:
                // [dwenger] Handle round restart processing for the weapon.
                //=============================================================================

                pWeapon->OnRoundRestart();

                //=============================================================================
                // HPE_END
                //=============================================================================

                if ( pWeapon->ShouldRemoveOnRoundRestart() )
				{
					UTIL_Remove( pCur );
				}
			}
			// remove entities that has to be restored on roundrestart (breakables etc)
			else if ( !CS_FindInList( s_PreserveEnts, pCur->GetClassname() ) )
			{
				UTIL_Remove( pCur );
			}
			
			pCur = gEntList.NextEnt( pCur );
		}
		
		// Really remove the entities so we can have access to their slots below.
		gEntList.CleanupDeleteList();

		// Cancel all queued events, in case a func_bomb_target fired some delayed outputs that
		// could kill respawning CTs
		g_EventQueue.Clear();

		// Now reload the map entities.
		class CCSMapEntityFilter : public IMapEntityFilter
		{
		public:
			virtual bool ShouldCreateEntity( const char *pClassname )
			{
				// Don't recreate the preserved entities.
				if ( !CS_FindInList( s_PreserveEnts, pClassname ) )
				{
					return true;
				}
				else
				{
					// Increment our iterator since it's not going to call CreateNextEntity for this ent.
					if ( m_iIterator != g_MapEntityRefs.InvalidIndex() )
						m_iIterator = g_MapEntityRefs.Next( m_iIterator );
				
					return false;
				}
			}


			virtual CBaseEntity* CreateNextEntity( const char *pClassname )
			{
				if ( m_iIterator == g_MapEntityRefs.InvalidIndex() )
				{
					// This shouldn't be possible. When we loaded the map, it should have used 
					// CCSMapLoadEntityFilter, which should have built the g_MapEntityRefs list
					// with the same list of entities we're referring to here.
					Assert( false );
					return NULL;
				}
				else
				{
					CMapEntityRef &ref = g_MapEntityRefs[m_iIterator];
					m_iIterator = g_MapEntityRefs.Next( m_iIterator );	// Seek to the next entity.

					if ( ref.m_iEdict == -1 || engine->PEntityOfEntIndex( ref.m_iEdict ) )
					{
						// Doh! The entity was delete and its slot was reused.
						// Just use any old edict slot. This case sucks because we lose the baseline.
						return CreateEntityByName( pClassname );
					}
					else
					{
						// Cool, the slot where this entity was is free again (most likely, the entity was 
						// freed above). Now create an entity with this specific index.
						return CreateEntityByName( pClassname, ref.m_iEdict );
					}
				}
			}

		public:
			int m_iIterator; // Iterator into g_MapEntityRefs.
		};
		CCSMapEntityFilter filter;
		filter.m_iIterator = g_MapEntityRefs.Head();

		// DO NOT CALL SPAWN ON info_node ENTITIES!

		MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );
	}


	bool CCSGameRules::IsThereABomber()
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CCSPlayer *pPlayer = CCSPlayer::Instance( i );

			if ( pPlayer && !FNullEnt( pPlayer->edict() ) )
			{
				if ( pPlayer->GetTeamNumber() == TEAM_CT )
					continue;

				if ( pPlayer->HasC4() )
					 return true; //There you are.
			}
		}

		//Didn't find a bomber.
		return false;
	}


	void CCSGameRules::EndRound()
	{
		// fake a round end
		CSGameRules()->TerminateRound( 0.0f, Round_Draw );
	}

	CBaseEntity *CCSGameRules::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
	{
		// gat valid spwan point
		CBaseEntity *pSpawnSpot = pPlayer->EntSelectSpawnPoint();

		// drop down to ground
		Vector GroundPos = DropToGround( pPlayer, pSpawnSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX );

		// Move the player to the place it said.
		pPlayer->Teleport( &pSpawnSpot->GetAbsOrigin(), &pSpawnSpot->GetLocalAngles(), &vec3_origin );
		pPlayer->m_Local.m_vecPunchAngle = vec3_angle;
		
		return pSpawnSpot;
	}
	
	// checks if the spot is clear of players
	bool CCSGameRules::IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer )
	{
		if ( !pSpot->IsTriggered( pPlayer ) )
		{
			return false;
		}

		Vector mins = GetViewVectors()->m_vHullMin;
		Vector maxs = GetViewVectors()->m_vHullMax;

		Vector vTestMins = pSpot->GetAbsOrigin() + mins;
		Vector vTestMaxs = pSpot->GetAbsOrigin() + maxs;
		
		// First test the starting origin.
		return UTIL_IsSpaceEmpty( pPlayer, vTestMins, vTestMaxs );
	}


	bool CCSGameRules::IsThereABomb()
	{
		bool bBombFound = false;

		/* are there any bombs, either laying around, or in someone's inventory? */
		if( gEntList.FindEntityByClassname( NULL, WEAPON_C4_CLASSNAME ) != 0 )
		{
			bBombFound = true;
		}
		/* what about planted bombs!? */
		else if( gEntList.FindEntityByClassname( NULL, PLANTED_C4_CLASSNAME ) != 0 )
		{
			bBombFound = true;
		}
		
		return bBombFound;
	}

	void CCSGameRules::HostageTouched()
	{
		if( gpGlobals->curtime > m_flNextHostageAnnouncement && m_iRoundWinStatus == WINNER_NONE )
		{
			//BroadcastSound( "Event.HostageTouched" );
			m_flNextHostageAnnouncement = gpGlobals->curtime + 60.0;
		}		
	}

	void CCSGameRules::CreateStandardEntities()
	{
		// Create the player resource
		g_pPlayerResource = (CPlayerResource*)CBaseEntity::Create( "cs_player_manager", vec3_origin, vec3_angle );
	
		// Create the entity that will send our data to the client.
#ifdef DBGFLAG_ASSERT
		CBaseEntity *pEnt = 
#endif
			CBaseEntity::Create( "cs_gamerules", vec3_origin, vec3_angle );
		Assert( pEnt );
	}

#define MY_USHRT_MAX	0xffff
#define MY_UCHAR_MAX	0xff

bool DataHasChanged( void )
{
	for ( int i = 0; i < CS_NUM_LEVELS; i++ )
	{
		if ( g_iTerroristVictories[i] != 0 || g_iCounterTVictories[i] != 0 )
			return true;
	}

	for ( int i = 0; i < WEAPON_MAX; i++ )
	{
		if ( g_iWeaponPurchases[i] != 0 )
			return true;
	}

	return false;
}

void CCSGameRules::UploadGameStats( void )
{
	g_flGameStatsUpdateTime -= gpGlobals->curtime;

	if ( g_flGameStatsUpdateTime > 0 )
		return;

	if ( IsBlackMarket() == false )
		return;

	if ( m_bDontUploadStats == true )
		return;

	if ( DataHasChanged() == true )
	{
		cs_gamestats_t stats;
		memset( &stats, 0, sizeof(stats) );

		// Header
		stats.header.iVersion = CS_STATS_BLOB_VERSION;
		Q_strncpy( stats.header.szGameName, "cstrike", sizeof(stats.header.szGameName) );
		Q_strncpy( stats.header.szMapName, STRING( gpGlobals->mapname ), sizeof( stats.header.szMapName ) );

		ConVar *hostip = cvar->FindVar( "hostip" );
		if ( hostip )
		{
			int ip = hostip->GetInt();
			stats.header.ipAddr[0] = ip >> 24;
			stats.header.ipAddr[1] = ( ip >> 16 ) & MY_UCHAR_MAX;
			stats.header.ipAddr[2] = ( ip >> 8 ) & MY_UCHAR_MAX;
			stats.header.ipAddr[3] = ( ip ) & MY_UCHAR_MAX;
		}			

		ConVar *hostport = cvar->FindVar( "hostip" );
		if ( hostport )
		{
			stats.header.port = hostport->GetInt();
		}			

		stats.header.serverid = 0;

		stats.iMinutesPlayed = clamp( (short)( gpGlobals->curtime / 60 ), 0, MY_USHRT_MAX ); 

		memcpy( stats.iTerroristVictories, g_iTerroristVictories, sizeof( g_iTerroristVictories) );
		memcpy( stats.iCounterTVictories, g_iCounterTVictories, sizeof( g_iCounterTVictories) );
		memcpy( stats.iBlackMarketPurchases, g_iWeaponPurchases, sizeof( g_iWeaponPurchases) );

		stats.iAutoBuyPurchases = g_iAutoBuyPurchases;
		stats.iReBuyPurchases = g_iReBuyPurchases;

		stats.iAutoBuyM4A1Purchases = g_iAutoBuyM4A1Purchases;
		stats.iAutoBuyAK47Purchases = g_iAutoBuyAK47Purchases;
		stats.iAutoBuyFamasPurchases = g_iAutoBuyFamasPurchases;
		stats.iAutoBuyGalilPurchases = g_iAutoBuyGalilPurchases;
		stats.iAutoBuyVestHelmPurchases = g_iAutoBuyVestHelmPurchases;
		stats.iAutoBuyVestPurchases = g_iAutoBuyVestPurchases;

		const void *pvBlobData = ( const void * )( &stats );
		unsigned int uBlobSize = sizeof( stats );

		if ( gamestatsuploader )
		{
			gamestatsuploader->UploadGameStats( 
				STRING( gpGlobals->mapname ),
				CS_STATS_BLOB_VERSION,
				uBlobSize,
				pvBlobData );
		}


		memset( g_iWeaponPurchases, 0, sizeof( g_iWeaponPurchases) );
		memset( g_iTerroristVictories, 0, sizeof( g_iTerroristVictories) );
		memset( g_iCounterTVictories, 0, sizeof( g_iTerroristVictories) );

		g_iAutoBuyPurchases = 0;
		g_iReBuyPurchases = 0;

		g_iAutoBuyM4A1Purchases = 0;
		g_iAutoBuyAK47Purchases = 0;
		g_iAutoBuyFamasPurchases = 0;
		g_iAutoBuyGalilPurchases = 0;
		g_iAutoBuyVestHelmPurchases = 0;
		g_iAutoBuyVestPurchases = 0;
	}

	g_flGameStatsUpdateTime = CS_GAME_STATS_UPDATE; //Next update is between 22 and 24 hours.
}
#endif	// CLIENT_DLL

CBaseCombatWeapon *CCSGameRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
	CBaseCombatWeapon *bestWeapon = NULL;

	// search all the weapons looking for the closest next
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		CBaseCombatWeapon *weapon = pPlayer->GetWeapon(i);
		if ( !weapon )
			continue;

		if ( !weapon->CanBeSelected() || weapon == pCurrentWeapon )
			continue;

#ifndef CLIENT_DLL
		CCSPlayer *csPlayer = ToCSPlayer(pPlayer);
		CWeaponCSBase *csWeapon = static_cast< CWeaponCSBase * >(weapon);
		if ( csPlayer && csPlayer->IsBot() && !TheCSBots()->IsWeaponUseable( csWeapon ) )
			continue;
#endif // CLIENT_DLL

		if ( bestWeapon )
		{
			if ( weapon->GetSlot() < bestWeapon->GetSlot() )
			{
				bestWeapon = weapon;
			}
			else if ( weapon->GetSlot() == bestWeapon->GetSlot() && weapon->GetPosition() < bestWeapon->GetPosition() )
			{
				bestWeapon = weapon;
			}
		}
		else
		{
			bestWeapon = weapon;
		}
	}

	return bestWeapon;
}

float CCSGameRules::GetMapRemainingTime()
{
#ifdef GAME_DLL
	if ( nextlevel.GetString() && *nextlevel.GetString() )
	{
		return 0;
	}
#endif

	// if timelimit is disabled, return -1
	if ( mp_timelimit.GetInt() <= 0 )
		return -1;

	// timelimit is in minutes
	float flTimeLeft =  ( m_flGameStartTime + mp_timelimit.GetInt() * 60 ) - gpGlobals->curtime;

	// never return a negative value
	if ( flTimeLeft < 0 )
		flTimeLeft = 0;

	return flTimeLeft;
}

float CCSGameRules::GetMapElapsedTime( void )
{
	return gpGlobals->curtime;
}

float CCSGameRules::GetRoundRemainingTime()
{
	return (float) (m_fRoundStartTime + m_iRoundTime) - gpGlobals->curtime; 
}

float CCSGameRules::GetRoundStartTime()
{
	return m_fRoundStartTime;
}


float CCSGameRules::GetRoundElapsedTime()
{
	return gpGlobals->curtime - m_fRoundStartTime;
}


bool CCSGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		::V_swap(collisionGroup0,collisionGroup1);
	}
	
	//Don't stand on COLLISION_GROUP_WEAPONs
	if( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
		collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		return false;
	}

	// TODO: make a CS-SPECIFIC COLLISION GROUP FOR PHYSICS PROPS THAT USE THIS COLLISION BEHAVIOR.

	
	if ( (collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
		collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
	{
		return false;
	}

	if ( collisionGroup0 == COLLISION_GROUP_DEBRIS && collisionGroup1 == COLLISION_GROUP_PUSHAWAY )
	{
		// let debris and multiplayer objects collide
		return true;
	}

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}


bool CCSGameRules::IsFreezePeriod()
{
	return m_bFreezePeriod;
}


bool CCSGameRules::IsVIPMap() const
{
	//MIKETODO: VIP mode
	return false;
}


bool CCSGameRules::IsBombDefuseMap() const
{
	return m_bMapHasBombTarget;
}

bool CCSGameRules::IsHostageRescueMap() const
{
	return m_bMapHasRescueZone;
}

bool CCSGameRules::IsLogoMap() const
{
	return m_bLogoMap;
}

float CCSGameRules::GetBuyTimeLength() const
{
	return ( mp_buytime.GetFloat() * 60 );
}

bool CCSGameRules::IsBuyTimeElapsed()
{
	return ( GetRoundElapsedTime() > GetBuyTimeLength() );
}

int CCSGameRules::DefaultFOV()
{
	return 90;
}

const CViewVectors* CCSGameRules::GetViewVectors() const
{
	return &g_CSViewVectors;
}


//-----------------------------------------------------------------------------
// Purpose: Init CS ammo definitions
//-----------------------------------------------------------------------------

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			1	

// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)


static CCSAmmoDef ammoDef;
CCSAmmoDef* GetCSAmmoDef()
{
	GetAmmoDef(); // to initialize the ammo info
	return &ammoDef;
}

CAmmoDef* GetAmmoDef()
{
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;
		
		ammoDef.AddAmmoType( BULLET_PLAYER_50AE,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_50AE_max",		2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_762MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_762mm_max",	2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_556MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_max",	2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_556MM_BOX,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_556mm_box_max",2400 * BULLET_IMPULSE_EXAGGERATION, 0, 10, 14 );
		ammoDef.AddAmmoType( BULLET_PLAYER_338MAG,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_338mag_max",	2800 * BULLET_IMPULSE_EXAGGERATION, 0, 12, 16 );
		ammoDef.AddAmmoType( BULLET_PLAYER_9MM,			DMG_BULLET, TRACER_LINE, 0, 0, "ammo_9mm_max",		2000 * BULLET_IMPULSE_EXAGGERATION, 0, 5, 10 );
		ammoDef.AddAmmoType( BULLET_PLAYER_BUCKSHOT,	DMG_BULLET, TRACER_LINE, 0, 0, "ammo_buckshot_max", 600 * BULLET_IMPULSE_EXAGGERATION,  0, 3, 6 );
		ammoDef.AddAmmoType( BULLET_PLAYER_45ACP,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_45acp_max",	2100 * BULLET_IMPULSE_EXAGGERATION, 0, 6, 10 );
		ammoDef.AddAmmoType( BULLET_PLAYER_357SIG,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_357sig_max",	2000 * BULLET_IMPULSE_EXAGGERATION, 0, 4, 8 );
		ammoDef.AddAmmoType( BULLET_PLAYER_57MM,		DMG_BULLET, TRACER_LINE, 0, 0, "ammo_57mm_max",		2000 * BULLET_IMPULSE_EXAGGERATION, 0, 4, 8 );
		ammoDef.AddAmmoType( AMMO_TYPE_HEGRENADE,		DMG_BLAST,	TRACER_LINE, 0, 0, "ammo_hegrenade_max", 1, 0 );
		ammoDef.AddAmmoType( AMMO_TYPE_FLASHBANG,		0,			TRACER_LINE, 0,	0, "ammo_flashbang_max", 1, 0 );
		ammoDef.AddAmmoType( AMMO_TYPE_SMOKEGRENADE,	0,			TRACER_LINE, 0, 0, "ammo_smokegrenade_max", 1, 0 );

		//Adrian: I set all the prices to 0 just so the rest of the buy code works
		//This should be revisited.
		ammoDef.AddAmmoCost( BULLET_PLAYER_50AE, 0, 7 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_762MM, 0, 30 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_556MM, 0, 30 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_556MM_BOX, 0, 30 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_338MAG, 0, 10 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_9MM, 0, 30 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_BUCKSHOT, 0, 8 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_45ACP, 0, 25 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_357SIG, 0, 13 );
		ammoDef.AddAmmoCost( BULLET_PLAYER_57MM, 0, 50 );
	}

	return &ammoDef;
}

#ifndef CLIENT_DLL
const char *CCSGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	const char *pszPrefix = NULL;

	if ( !pPlayer )  // dedicated server output
	{
		pszPrefix = "";
	}
	else
	{
		// team only
		if ( bTeamOnly == TRUE )
		{
			if ( pPlayer->GetTeamNumber() == TEAM_CT )
			{
				if ( pPlayer->m_lifeState == LIFE_ALIVE )
				{
					pszPrefix = "(Counter-Terrorist)";
				}
				else 
				{
					pszPrefix = "*DEAD*(Counter-Terrorist)";
				}
			}
			else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
			{
				if ( pPlayer->m_lifeState == LIFE_ALIVE )
				{
					pszPrefix = "(Terrorist)";
				}
				else
				{
					pszPrefix = "*DEAD*(Terrorist)";
				}
			}
			else if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
			{
				pszPrefix = "(Spectator)";
			}
		}
		// everyone
		else
		{
			if ( pPlayer->m_lifeState == LIFE_ALIVE )
			{
				pszPrefix = "";
			}
			else
			{
				if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
				{
					pszPrefix = "*DEAD*";	
				}
				else
				{
					pszPrefix = "*SPEC*";
				}
			}
		}
	}

	return pszPrefix;
}

const char *CCSGameRules::GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( !pPlayer )  // dedicated server output
	{
		return NULL;
	}

	// only teammates see locations
	if ( !bTeamOnly )
		return NULL;

	// only living players have locations
	if ( pPlayer->GetTeamNumber() != TEAM_CT && pPlayer->GetTeamNumber() != TEAM_TERRORIST )
		return NULL;

	if ( !pPlayer->IsAlive() )
		return NULL;

	return pPlayer->GetLastKnownPlaceName();
}

const char *CCSGameRules::GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( !pPlayer )  // dedicated server output
	{
		return NULL;
	}

	const char *pszFormat = NULL;

	// team only
	if ( bTeamOnly == TRUE )
	{
		if ( pPlayer->GetTeamNumber() == TEAM_CT )
		{
			if ( pPlayer->m_lifeState == LIFE_ALIVE )
			{
				const char *chatLocation = GetChatLocation( bTeamOnly, pPlayer );
				if ( chatLocation && *chatLocation )
				{
					pszFormat = "Cstrike_Chat_CT_Loc";
				}
				else
				{
					pszFormat = "Cstrike_Chat_CT";
				}
			}
			else 
			{
				pszFormat = "Cstrike_Chat_CT_Dead";
			}
		}
		else if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
		{
			if ( pPlayer->m_lifeState == LIFE_ALIVE )
			{
				const char *chatLocation = GetChatLocation( bTeamOnly, pPlayer );
				if ( chatLocation && *chatLocation )
				{
					pszFormat = "Cstrike_Chat_T_Loc";
				}
				else
				{
					pszFormat = "Cstrike_Chat_T";
				}
			}
			else
			{
				pszFormat = "Cstrike_Chat_T_Dead";
			}
		}
		else if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			pszFormat = "Cstrike_Chat_Spec";
		}
	}
	// everyone
	else
	{
		if ( pPlayer->m_lifeState == LIFE_ALIVE )
		{
			pszFormat = "Cstrike_Chat_All";
		}
		else
		{
			if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			{
				pszFormat = "Cstrike_Chat_AllDead";	
			}
			else
			{
				pszFormat = "Cstrike_Chat_AllSpec";
			}
		}
	}

	return pszFormat;
}

void CCSGameRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
	const char *pszNewName = engine->GetClientConVarValue( pPlayer->entindex(), "name" );
	const char *pszOldName = pPlayer->GetPlayerName();
	CCSPlayer *pCSPlayer = (CCSPlayer*)pPlayer;		
	if ( pszOldName[0] != 0 && Q_strncmp( pszOldName, pszNewName, MAX_PLAYER_NAME_LENGTH-1 ) )		
	{
		pCSPlayer->ChangeName( pszNewName );		
	}

	pCSPlayer->m_bShowHints = true;
	if ( pCSPlayer->IsNetClient() )
	{
		const char *pShowHints = engine->GetClientConVarValue( engine->IndexOfEdict( pCSPlayer->edict() ), "cl_autohelp" );
		if ( pShowHints && atoi( pShowHints ) <= 0 )
		{
			pCSPlayer->m_bShowHints = false;
		}
	}
}

bool CCSGameRules::FAllowNPCs( void )
{
	return false;
}

bool CCSGameRules::IsFriendlyFireOn( void )
{
	return friendlyfire.GetBool();
}


CON_COMMAND( map_showspawnpoints, "Shows player spawn points (red=invalid)" )
{
	CSGameRules()->ShowSpawnPoints();
}

void DrawSphere( const Vector& pos, float radius, int r, int g, int b, float lifetime )
{
	Vector edge, lastEdge;
	NDebugOverlay::Line( pos, pos + Vector( 0, 0, 50 ), r, g, b, true, lifetime );

	lastEdge = Vector( radius + pos.x, pos.y, pos.z );
	float angle;
	for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
	{
		edge.x = radius * BotCOS( angle ) + pos.x;
		edge.y = pos.y;
		edge.z = radius * BotSIN( angle ) + pos.z;

		NDebugOverlay::Line( edge, lastEdge, r, g, b, true, lifetime );

		lastEdge = edge;
	}

	lastEdge = Vector( pos.x, radius + pos.y, pos.z );
	for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
	{
		edge.x = pos.x;
		edge.y = radius * BotCOS( angle ) + pos.y;
		edge.z = radius * BotSIN( angle ) + pos.z;

		NDebugOverlay::Line( edge, lastEdge, r, g, b, true, lifetime );

		lastEdge = edge;
	}

	lastEdge = Vector( pos.x, radius + pos.y, pos.z );
	for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
	{
		edge.x = radius * BotCOS( angle ) + pos.x;
		edge.y = radius * BotSIN( angle ) + pos.y;
		edge.z = pos.z;

		NDebugOverlay::Line( edge, lastEdge, r, g, b, true, lifetime );

		lastEdge = edge;
	}
}

CON_COMMAND_F( map_showbombradius, "Shows bomb radius from the center of each bomb site and planted bomb.", FCVAR_CHEAT )
{
	float flBombDamage = 500.0f;
	if ( g_pMapInfo )
		flBombDamage = g_pMapInfo->m_flBombRadius;
	float flBombRadius = flBombDamage * 3.5f;
	Msg( "Bomb Damage is %.0f, Radius is %.0f\n", flBombDamage, flBombRadius );

	CBaseEntity* ent = NULL;
	while ( ( ent = gEntList.FindEntityByClassname( ent, "func_bomb_target" ) ) != NULL )
	{
		const Vector &pos = ent->WorldSpaceCenter();
		DrawSphere( pos, flBombRadius, 255, 255, 0, 10 );
	}

	ent = NULL;
	while ( ( ent = gEntList.FindEntityByClassname( ent, "planted_c4" ) ) != NULL )
	{
		const Vector &pos = ent->WorldSpaceCenter();
		DrawSphere( pos, flBombRadius, 255, 0, 0, 10 );
	}
}

CON_COMMAND_F( map_setbombradius, "Sets the bomb radius for the map.", FCVAR_CHEAT )
{
	if ( args.ArgC() != 2 )
		return;

	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pMapInfo )
		CBaseEntity::Create( "info_map_parameters", vec3_origin, vec3_angle );

	if ( !g_pMapInfo )
		return;

	g_pMapInfo->m_flBombRadius = atof( args[1] );
	map_showbombradius( args );
}

void CreateBlackMarketString( void )
{
	g_StringTableBlackMarket = networkstringtable->CreateStringTable( "BlackMarketTable" , 1 );
}

int CCSGameRules::GetStartMoney( void )
{
	if ( IsBlackMarket() )
	{
		return atoi( mp_startmoney.GetDefault() );
	}

	return mp_startmoney.GetInt();
}



//=============================================================================
// HPE_BEGIN:
// [menglish] Set up anything for all players that changes based on new players spawning mid-game
//				Find and return fun fact data
//=============================================================================
 
//-----------------------------------------------------------------------------
// Purpose: Called when a player joins the game after it's started yet can still spawn in
//-----------------------------------------------------------------------------
void CCSGameRules::SpawningLatePlayer( CCSPlayer* pLatePlayer )
{
	//Reset the round kills number of enemies for the opposite team
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );
		if(pPlayer)
		{
			if(pPlayer->GetTeamNumber() == pLatePlayer->GetTeamNumber())
			{
				continue;
			}
			pPlayer->m_NumEnemiesAtRoundStart++;
		}
	}
}

//=============================================================================
// HPE_END
//=============================================================================

//=============================================================================
// HPE_BEGIN:
// [pfreese] Test for "pistol" round, defined as the default starting round
// when players cannot purchase anything primary weapons
//=============================================================================

bool CCSGameRules::IsPistolRound()
{
	return m_iTotalRoundsPlayed == 0 && GetStartMoney() <= 800;
}

//=============================================================================
// HPE_END
//=============================================================================

//=============================================================================
// HPE_BEGIN:
// [tj] So game rules can react to damage taken
// [menglish]
//=============================================================================

void CCSGameRules::PlayerTookDamage(CCSPlayer* player, const CTakeDamageInfo &damageInfo)
{
	CBaseEntity *pInflictor = damageInfo.GetInflictor();
	CBaseEntity *pAttacker = damageInfo.GetAttacker();
	CCSPlayer *pCSScorer = (CCSPlayer *)(GetDeathScorer( pAttacker, pInflictor ));

	if ( player && pCSScorer )
	{
		if (player->GetTeamNumber() == TEAM_CT)
		{
			m_bNoCTsDamaged = false;
		}

		if (player->GetTeamNumber() == TEAM_TERRORIST)
		{
			m_bNoTerroristsDamaged = false;
		}
		// set the first blood if this is the first and the victim is on a different team then the player
		if ( m_pFirstBlood == NULL && pCSScorer != player && pCSScorer->GetTeamNumber() != player ->GetTeamNumber() )
		{
			m_pFirstBlood = pCSScorer;
			m_firstBloodTime = gpGlobals->curtime - m_fRoundStartTime;
		}
	}
}


//=============================================================================
// HPE_END
//=============================================================================
#endif

bool CCSGameRules::IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer )
{
#ifdef GAME_DLL
	if( pPlayer )
	{
		int iPlayerTeam = pPlayer->GetTeamNumber();
		if( ( iPlayerTeam == TEAM_CT ) || ( iPlayerTeam == TEAM_TERRORIST ) )
			return false;
	}
#else
	int iLocalPlayerTeam = GetLocalPlayerTeam();
	if( ( iLocalPlayerTeam == TEAM_CT ) || ( iLocalPlayerTeam == TEAM_TERRORIST ) )
			return false;
#endif

	return true;
}

#ifdef GAME_DLL

struct convar_tags_t
{
	const char *pszConVar;
	const char *pszTag;
};

// The list of convars that automatically turn on tags when they're changed.
// Convars in this list need to have the FCVAR_NOTIFY flag set on them, so the
// tags are recalculated and uploaded to the master server when the convar is changed.
convar_tags_t convars_to_check_for_tags[] =
{
	{ "mp_friendlyfire", "friendlyfire" },
	{ "bot_quota", "bots" },
	{ "sv_nostats", "nostats" },
	{ "mp_startmoney", "startmoney" },
	{ "sv_allowminmodels", "nominmodels" },
	{ "sv_enablebunnyhopping", "bunnyhopping" },
	{ "sv_competitive_minspec", "compspec" },
	{ "mp_holiday_nogifts", "nogifts" },
};

//-----------------------------------------------------------------------------
// Purpose: Engine asks for the list of convars that should tag the server
//-----------------------------------------------------------------------------
void CCSGameRules::GetTaggedConVarList( KeyValues *pCvarTagList )
{
	BaseClass::GetTaggedConVarList( pCvarTagList );

	for ( int i = 0; i < ARRAYSIZE(convars_to_check_for_tags); i++ )
	{
		KeyValues *pKV = new KeyValues( "tag" );
		pKV->SetString( "convar", convars_to_check_for_tags[i].pszConVar );
		pKV->SetString( "tag", convars_to_check_for_tags[i].pszTag );

		pCvarTagList->AddSubKey( pKV );
	}
}

#endif


int CCSGameRules::GetBlackMarketPriceForWeapon( int iWeaponID )
{
	if ( m_pPrices == NULL )
	{
		GetBlackMarketPriceList();
	}

	if ( m_pPrices )
		return m_pPrices->iCurrentPrice[iWeaponID];
	else
		return 0;
}

int CCSGameRules::GetBlackMarketPreviousPriceForWeapon( int iWeaponID )
{
	if ( m_pPrices == NULL )
	{
		GetBlackMarketPriceList();
	}

	if ( m_pPrices )
		return m_pPrices->iPreviousPrice[iWeaponID];
	else
		return 0;
}

const weeklyprice_t *CCSGameRules::GetBlackMarketPriceList( void )
{
	if ( m_StringTableBlackMarket == NULL )
	{
		m_StringTableBlackMarket = networkstringtable->FindTable( CS_GAMERULES_BLACKMARKET_TABLE_NAME);
	}

	if ( m_pPrices == NULL )
	{
		int iSize = 0;
		INetworkStringTable *pTable = m_StringTableBlackMarket;
		if ( pTable && pTable->GetNumStrings() > 0 )
		{
			m_pPrices = (const weeklyprice_t *)pTable->GetStringUserData( 0, &iSize );
		}
	}

	if ( m_pPrices )
	{
		PrepareEquipmentInfo();
	}
	
	return m_pPrices;
}

void CCSGameRules::SetBlackMarketPrices( bool bSetDefaults )
{
	for ( int i = 1; i < WEAPON_MAX; i++ )
	{
		if ( i == WEAPON_SHIELDGUN )
			continue;

		CCSWeaponInfo *info = GetWeaponInfo( (CSWeaponID)i );

		if ( info == NULL )
			continue;

		if ( bSetDefaults == false )
		{
			info->SetWeaponPrice( GetBlackMarketPriceForWeapon( i ) );
			info->SetPreviousPrice( GetBlackMarketPreviousPriceForWeapon( i ) );
		}
		else
		{
			info->SetWeaponPrice( info->GetDefaultPrice() );
		}
	}
}

#ifdef CLIENT_DLL

CCSGameRules::CCSGameRules()
{
	CSGameRules()->m_StringTableBlackMarket = NULL;
	m_pPrices = NULL;
	m_bBlackMarket = false;
}

void TestTable( void )
{
	CSGameRules()->m_StringTableBlackMarket = networkstringtable->FindTable( CS_GAMERULES_BLACKMARKET_TABLE_NAME);

	if ( CSGameRules()->m_StringTableBlackMarket == NULL )
		return;

	int iIndex = CSGameRules()->m_StringTableBlackMarket->FindStringIndex( "blackmarket_prices" );
	int iSize = 0;

	const weeklyprice_t *pPrices = NULL;
	
	pPrices = (const weeklyprice_t *)(CSGameRules()->m_StringTableBlackMarket)->GetStringUserData( iIndex, &iSize );
}

#ifdef DEBUG
ConCommand cs_testtable( "cs_testtable", TestTable );
#endif

//-----------------------------------------------------------------------------
// Enforce certain values on the specified convar.
//-----------------------------------------------------------------------------
void EnforceCompetitiveCVar( const char *szCvarName, float fMinValue, float fMaxValue = FLT_MAX, int iArgs = 0, ... )
{
	// Doing this check first because OK values might be outside the min/max range
	ConVarRef competitiveConvar(szCvarName);
	float fValue = competitiveConvar.GetFloat();
	va_list vl;
	va_start(vl, iArgs);
	for( int i=0; i< iArgs; ++i )
	{
		if( (int)fValue == (int)va_arg(vl,double) )
			return;
	}
	va_end(vl);

	if( fValue < fMinValue || fValue > fMaxValue )
	{
		float fNewValue = MAX( MIN( fValue, fMaxValue ), fMinValue );
		competitiveConvar.SetValue( fNewValue );
		DevMsg( "Convar %s enforced by server (see sv_competitive_minspec.) Set to %2f.\n", szCvarName, fNewValue );
	}
}

//-----------------------------------------------------------------------------
// An interface used by ENABLE_COMPETITIVE_CONVAR macro that lets the classes
// defined in the macro to be stored and acted on.
//-----------------------------------------------------------------------------
class ICompetitiveConvar
{
public:
	// It is a best practice to always have a virtual destructor in an interface
	// class. Otherwise if the derived classes have destructors they will not be
	// called.
	virtual ~ICompetitiveConvar() {}
	virtual void BackupConvar() = 0;
	virtual void EnforceRestrictions() = 0;
	virtual void RestoreOriginalValue() = 0;
	virtual void InstallChangeCallback() = 0;
};

//-----------------------------------------------------------------------------
// A manager for all enforced competitive convars.
//-----------------------------------------------------------------------------
class CCompetitiveCvarManager : public CAutoGameSystem
{
public:
	typedef CUtlVector<ICompetitiveConvar*> CompetitiveConvarList_t;
	static void AddConvarToList( ICompetitiveConvar* pCVar )
	{
		GetConvarList()->AddToTail( pCVar );
	}

	static void BackupAllConvars()
	{
		FOR_EACH_VEC( *GetConvarList(), i )
		{
			(*GetConvarList())[i]->BackupConvar();
		}
	}

	static void EnforceRestrictionsOnAllConvars()
	{
		FOR_EACH_VEC( *GetConvarList(), i )
		{
			(*GetConvarList())[i]->EnforceRestrictions();
		}
	}

	static void RestoreAllOriginalValues()
	{
		FOR_EACH_VEC( *GetConvarList(), i )
		{
			(*GetConvarList())[i]->RestoreOriginalValue();
		}
	}

	static CompetitiveConvarList_t* GetConvarList()
	{
		if( !s_pCompetitiveConvars )
		{
			s_pCompetitiveConvars = new CompetitiveConvarList_t();
		}
		return s_pCompetitiveConvars;
	}

	static KeyValues* GetConVarBackupKV()
	{
		if( !s_pConVarBackups )
		{
			s_pConVarBackups = new KeyValues("ConVarBackups");
		}
		return s_pConVarBackups;
	}

	virtual bool Init() 
	{ 
		FOR_EACH_VEC( *GetConvarList(), i )
		{
			(*GetConvarList())[i]->InstallChangeCallback();
		}
		return true;
	}

	virtual void Shutdown()
	{
		FOR_EACH_VEC( *GetConvarList(), i )
		{
			delete (*GetConvarList())[i];
		}
		delete s_pCompetitiveConvars; 
		s_pCompetitiveConvars = null;
		s_pConVarBackups->deleteThis(); 
		s_pConVarBackups = null;
	}
private:
	static CompetitiveConvarList_t* s_pCompetitiveConvars;
	static KeyValues* s_pConVarBackups;
};
static CCompetitiveCvarManager *s_pCompetitiveCvarManager = new CCompetitiveCvarManager();
CCompetitiveCvarManager::CompetitiveConvarList_t* CCompetitiveCvarManager::s_pCompetitiveConvars = null;
KeyValues* CCompetitiveCvarManager::s_pConVarBackups = null;

//-----------------------------------------------------------------------------
// Macro to define restrictions on convars with "sv_competitive_minspec 1"
// Usage: ENABLE_COMPETITIVE_CONVAR( convarName, minValue, maxValue, optionalValues, opVal1, opVal2, ...
//-----------------------------------------------------------------------------
#define ENABLE_COMPETITIVE_CONVAR( convarName, ... ) \
class CCompetitiveMinspecConvar##convarName : public ICompetitiveConvar { \
public: \
	CCompetitiveMinspecConvar##convarName(){ CCompetitiveCvarManager::AddConvarToList(this);} \
	static void on_changed_##convarName( IConVar *var, const char *pOldValue, float flOldValue ){ \
		if( sv_competitive_minspec.GetBool() ) { \
			EnforceCompetitiveCVar( #convarName , __VA_ARGS__  ); }\
		else {\
			CCompetitiveCvarManager::GetConVarBackupKV()->SetFloat( #convarName, ConVarRef( #convarName ).GetFloat() ); } } \
	virtual void BackupConvar() { CCompetitiveCvarManager::GetConVarBackupKV()->SetFloat( #convarName, ConVarRef( #convarName ).GetFloat() ); } \
	virtual void EnforceRestrictions() { EnforceCompetitiveCVar( #convarName , __VA_ARGS__  ); } \
	virtual void RestoreOriginalValue() { ConVarRef(#convarName).SetValue(CCompetitiveCvarManager::GetConVarBackupKV()->GetFloat( #convarName ) ); } \
	virtual void InstallChangeCallback() { static_cast<ConVar*>(ConVarRef( #convarName ).GetLinkedConVar())->InstallChangeCallback( CCompetitiveMinspecConvar##convarName::on_changed_##convarName); } \
}; \
static CCompetitiveMinspecConvar##convarName *s_pCompetitiveConvar##convarName = new CCompetitiveMinspecConvar##convarName();

//-----------------------------------------------------------------------------
// Callback function for sv_competitive_minspec convar value change.
//-----------------------------------------------------------------------------
void sv_competitive_minspec_changed_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVar *pCvar = static_cast<ConVar*>(var);

	if( pCvar->GetBool() == true && (bool)flOldValue == false )
	{
		// Backup the values of each cvar and enforce new ones
		CCompetitiveCvarManager::BackupAllConvars();
		CCompetitiveCvarManager::EnforceRestrictionsOnAllConvars();
	}
	else if( pCvar->GetBool() == false && (bool)flOldValue == true )
	{
		// If sv_competitive_minspec is disabled, restore old client values
		CCompetitiveCvarManager::RestoreAllOriginalValues();
	}
}
#endif

static ConVar sv_competitive_minspec( "sv_competitive_minspec",
									 "0",
									 FCVAR_REPLICATED | FCVAR_NOTIFY,
									 "Enable to force certain client convars to minimum/maximum values to help prevent competitive advantages:\n \
	r_drawdetailprops = 1\n \
	r_staticprop_lod = minimum -1 maximum 3\n \
	fps_max minimum 59 (0 works too)\n \
	cl_detailfade minimum 400\n \
	cl_detaildist minimum 1200\n \
	cl_interp_ratio = minimum 1 maximum 2\n \
	cl_interp = minimum 0 maximum 0.031\n \
	"
#ifdef CLIENT_DLL
									 ,sv_competitive_minspec_changed_f
#endif
									 );

#ifdef CLIENT_DLL

ENABLE_COMPETITIVE_CONVAR( r_drawdetailprops, true, true ); // force r_drawdetailprops on
ENABLE_COMPETITIVE_CONVAR( r_staticprop_lod, -1, 3 );		// force r_staticprop_lod from -1 to 3
ENABLE_COMPETITIVE_CONVAR( fps_max, 59, FLT_MAX, 1, 0 );	// force fps_max above 59. One additional value (0) works
ENABLE_COMPETITIVE_CONVAR( cl_detailfade, 400 );			// force cl_detailfade above 400.
ENABLE_COMPETITIVE_CONVAR( cl_detaildist, 1200 );			// force cl_detaildist above 1200.
ENABLE_COMPETITIVE_CONVAR( cl_interp_ratio, 1, 2 );			// force cl_interp_ratio from 1 to 2
ENABLE_COMPETITIVE_CONVAR( cl_interp, 0, 0.031 );		// force cl_interp from 0.0152 to 0.031

// Stubs for replay client code
const char *GetMapDisplayName( const char *pMapName )
{
	return pMapName;
}

bool IsTakingAFreezecamScreenshot()
{
	return false;
}

#endif
