//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "dod_gamerules.h"
#include "ammodef.h"
#include "KeyValues.h"
#include "weapon_dodbase.h"
#include "filesystem.h"		// for WriteStatsFile


#ifdef CLIENT_DLL

	#include "precache_register.h"
	#include "c_dod_player.h"

#else
	
	#include "coordsize.h"
	#include "dod_player.h"
	#include "voice_gamemgr.h"
	#include "team.h"
	#include "dod_player.h"
	#include "dod_bot_temp.h"
	#include "game.h"
	#include "dod_shareddefs.h"
	#include "player_resource.h"
	#include "mapentities.h"
	#include "dod_gameinterface.h"
	#include "dod_objective_resource.h"
	#include "dod_cvars.h"
	#include "dod_team.h"
	#include "dod_playerclass_info_parse.h"
	#include "dod_control_point_master.h"
	#include "dod_bombtarget.h"
	//#include "teamplayroundbased_gamerules.h"
	#include "weapon_dodbipodgun.h"

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef CLIENT_DLL

BEGIN_DATADESC(CSpawnPoint)

	// Keyfields
	DEFINE_KEYFIELD( m_bDisabled,	FIELD_BOOLEAN,	"StartDisabled" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),

END_DATADESC();

LINK_ENTITY_TO_CLASS(info_player_allies, CSpawnPoint);
LINK_ENTITY_TO_CLASS(info_player_axis, CSpawnPoint);

#endif

REGISTER_GAMERULES_CLASS( CDODGameRules );

#define MAX_RESPAWN_WAVES_TO_TRANSMIT	5

#ifdef CLIENT_DLL
void RecvProxy_RoundState( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CDODGameRules *pGamerules = ( CDODGameRules *)pStruct;

	int iRoundState = pData->m_Value.m_Int;

	pGamerules->SetRoundState( iRoundState );
}
#endif

BEGIN_NETWORK_TABLE_NOBASE( CDODGameRules, DT_DODGameRules )
	#ifdef CLIENT_DLL

		RecvPropInt( RECVINFO( m_iRoundState ), 0, RecvProxy_RoundState ),
		RecvPropBool( RECVINFO( m_bInWarmup ) ),
		RecvPropBool( RECVINFO( m_bAwaitingReadyRestart ) ),
		RecvPropTime( RECVINFO( m_flMapResetTime ) ),
		RecvPropTime( RECVINFO( m_flRestartRoundTime ) ),
		RecvPropBool( RECVINFO( m_bAlliesAreBombing ) ),
		RecvPropBool( RECVINFO( m_bAxisAreBombing ) ),

		RecvPropArray3( RECVINFO_ARRAY(m_AlliesRespawnQueue), RecvPropTime( RECVINFO(m_AlliesRespawnQueue[0]) ) ),
		RecvPropArray3( RECVINFO_ARRAY(m_AxisRespawnQueue), RecvPropTime( RECVINFO(m_AxisRespawnQueue[0]) ) ),
		RecvPropInt( RECVINFO( m_iAlliesRespawnHead ) ),
		RecvPropInt( RECVINFO( m_iAlliesRespawnTail ) ),
		RecvPropInt( RECVINFO( m_iAxisRespawnHead ) ),
		RecvPropInt( RECVINFO( m_iAxisRespawnTail ) ),

	#else

		SendPropInt( SENDINFO( m_iRoundState ), 5 ),
		SendPropBool( SENDINFO( m_bInWarmup ) ),
		SendPropBool( SENDINFO( m_bAwaitingReadyRestart ) ),
		SendPropTime( SENDINFO( m_flMapResetTime ) ),
		SendPropTime( SENDINFO( m_flRestartRoundTime ) ),
		SendPropBool( SENDINFO( m_bAlliesAreBombing ) ),
		SendPropBool( SENDINFO( m_bAxisAreBombing ) ),

		SendPropArray3( SENDINFO_ARRAY3(m_AlliesRespawnQueue), SendPropTime( SENDINFO_ARRAY(m_AlliesRespawnQueue) ) ),
		SendPropArray3( SENDINFO_ARRAY3(m_AxisRespawnQueue), SendPropTime( SENDINFO_ARRAY(m_AxisRespawnQueue) ) ),
		SendPropInt( SENDINFO( m_iAlliesRespawnHead ), Q_log2( DOD_RESPAWN_QUEUE_SIZE ) + 1, SPROP_UNSIGNED ),
		SendPropInt( SENDINFO( m_iAlliesRespawnTail ), Q_log2( DOD_RESPAWN_QUEUE_SIZE ) + 1, SPROP_UNSIGNED ),
		SendPropInt( SENDINFO( m_iAxisRespawnHead ), Q_log2( DOD_RESPAWN_QUEUE_SIZE ) + 1, SPROP_UNSIGNED ),
		SendPropInt( SENDINFO( m_iAxisRespawnTail ), Q_log2( DOD_RESPAWN_QUEUE_SIZE ) + 1, SPROP_UNSIGNED ),

	#endif
END_NETWORK_TABLE()

#ifndef CLIENT_DLL
ConVar dod_flagrespawnbonus( "dod_flagrespawnbonus", "1.0", FCVAR_GAMEDLL | FCVAR_CHEAT, "How many seconds per advantage flag to decrease the respawn time" );

ConVar mp_warmup_time( "mp_warmup_time", "0", FCVAR_GAMEDLL, "Warmup time length in seconds" );
ConVar mp_restartwarmup( "mp_restartwarmup", "0", FCVAR_GAMEDLL, "Set to 1 to start or restart the warmup period." );
ConVar mp_cancelwarmup( "mp_cancelwarmup", "0", FCVAR_GAMEDLL, "Set to 1 to end the warmup period." );
#endif

ConVar dod_enableroundwaittime( "dod_enableroundwaittime", "1", FCVAR_REPLICATED, "Enable timers to wait between rounds." );
ConVar mp_allowrandomclass( "mp_allowrandomclass", "1", FCVAR_REPLICATED, "Allow players to select random class" );

ConVar dod_bonusroundtime( "dod_bonusroundtime", "15", FCVAR_REPLICATED, "Time after round win until round restarts", true, 5, true, 15 );

LINK_ENTITY_TO_CLASS( dod_gamerules, CDODGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( DODGameRulesProxy, DT_DODGameRulesProxy )


#ifdef CLIENT_DLL
	void RecvProxy_DODGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CDODGameRules *pRules = DODGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CDODGameRulesProxy, DT_DODGameRulesProxy )
		RecvPropDataTable( "dod_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_DODGameRules ), RecvProxy_DODGameRules )
	END_RECV_TABLE()
#else
	void* SendProxy_DODGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CDODGameRules *pRules = DODGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CDODGameRulesProxy, DT_DODGameRulesProxy )
		SendPropDataTable( "dod_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_DODGameRules ), SendProxy_DODGameRules )
	END_SEND_TABLE()
#endif

static CDODViewVectors g_DODViewVectors(

	Vector( 0, 0, 58 ),			//VEC_VIEW (m_vView) 
								
	Vector(-16, -16, 0 ),		//VEC_HULL_MIN (m_vHullMin)
	Vector( 16,  16,  72 ),		//VEC_HULL_MAX (m_vHullMax)
													
	Vector(-16, -16, 0 ),		//VEC_DUCK_HULL_MIN (m_vDuckHullMin)
	Vector( 16,  16, 45 ),		//VEC_DUCK_HULL_MAX	(m_vDuckHullMax)
	Vector( 0, 0, 34 ),			//VEC_DUCK_VIEW		(m_vDuckView)
													
	Vector(-10, -10, -10 ),		//VEC_OBS_HULL_MIN	(m_vObsHullMin)
	Vector( 10,  10,  10 ),		//VEC_OBS_HULL_MAX	(m_vObsHullMax)
													
	Vector( 0, 0, 14 ),			//VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight)
								
	Vector(-16, -16, 0 ),		//VEC_PRONE_HULL_MIN (m_vProneHullMin)
	Vector( 16,  16, 24 )		//VEC_PRONE_HULL_MAX (m_vProneHullMax)
);


#ifdef CLIENT_DLL


#else

	void ParseEntKVBlock( CBaseEntity *pNode, KeyValues *pkvNode )
	{
		KeyValues *pkvNodeData = pkvNode->GetFirstSubKey();
		while ( pkvNodeData )
		{
			// Handle the connections block
			if ( !Q_strcmp(pkvNodeData->GetName(), "connections") )
			{
				ParseEntKVBlock( pNode, pkvNodeData );
			}
			else
			{
				pNode->KeyValue( pkvNodeData->GetName(), pkvNodeData->GetString() );
			}

			pkvNodeData = pkvNodeData->GetNextKey();
		}
	}

	CUtlVector<EHANDLE> m_hSpawnedEntities;

	// for now only allow blocker walls to load this way
	bool CanLoadEntityFromEntText( const char *clsName )
	{
		if ( !Q_strcmp( clsName, "func_team_wall" ) )
		{
			return true;
		}

		return false;
	}

    void Load_EntText( void )
	{
		bool oldLock = engine->LockNetworkStringTables( false );

		// remove all ents in m_SpawnedEntities
		for ( int i = 0; i < m_hSpawnedEntities.Count(); i++ )
		{
			UTIL_Remove( m_hSpawnedEntities[i] );
		}

		// delete the items from our list
		m_hSpawnedEntities.RemoveAll();

		// Find the commentary file
		char szFullName[512];
		Q_snprintf(szFullName,sizeof(szFullName), "maps/%s.ent", STRING( gpGlobals->mapname ));
		KeyValues *pkvFile = new KeyValues( "EntText" );
		if ( pkvFile->LoadFromFile( filesystem, szFullName, "MOD" ) )
		{
			DevMsg( "Load_EntText: Loading entity data from %s. \n", szFullName );

			// Load each commentary block, and spawn the entities
			KeyValues *pkvNode = pkvFile->GetFirstSubKey();
			while ( pkvNode )
			{
				// Get node name
				const char *pNodeName = pkvNode->GetName();
				KeyValues *pClassname = pkvNode->FindKey( "classname" );
				if ( pClassname )
				{
					// Use the classname instead
					pNodeName = pClassname->GetString();
				}

				if ( CanLoadEntityFromEntText( pNodeName ) )
				{
					// Spawn the entity
					CBaseEntity *pNode = CreateEntityByName( pNodeName );
					if ( pNode )
					{
						ParseEntKVBlock( pNode, pkvNode );
						DispatchSpawn( pNode );

						EHANDLE hHandle;
						hHandle = pNode;
						m_hSpawnedEntities.AddToTail( hHandle );
					}
					else
					{
						Warning("Load_EntText: Failed to spawn entity, type: '%s'\n", pNodeName );
					}
				}				

				// Move to next entity
				pkvNode = pkvNode->GetNextKey();
			}

			// Then activate all the entities
			for ( int i = 0; i < m_hSpawnedEntities.Count(); i++ )
			{
				m_hSpawnedEntities[i]->Activate();
			}
		}
		else
		{
			DevMsg( "Load_EntText: Could not find entity data file '%s'. \n", szFullName );
		}

		engine->LockNetworkStringTables( oldLock );
	}

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

	// NOTE: the indices here must match TEAM_TERRORIST, TEAM_ALLIES, TEAM_AXIS, etc.
	char *sTeamNames[] =
	{
		"Unassigned",
		"Spectator",
		"Allies",
		"Axis"
	};

	static const char *s_PreserveEnts[] =
	{
		"player",
		"viewmodel",
		"worldspawn",
		"soundent",
		"ai_network",
		"ai_hint",
		"dod_gamerules",
		"dod_team_manager",
		"dod_player_manager",
		"dod_objective_resource",
		"env_soundscape",
		"env_soundscape_proxy",
		"env_soundscape_triggerable",
		"env_sprite",
		"env_sun",
		"env_wind",
		"env_fog_controller",
		"func_brush",
		"func_wall",
		"func_illusionary",
		"info_node",
		"info_target",
		"info_node_hint",
		"info_player_allies",
		"info_player_axis",
		"point_viewcontrol",
		"shadow_control",
		"sky_camera",
		"scene_manager",
		"trigger_soundscape",
		"info_dod_detect",
		"dod_team_allies",
		"dod_team_axis",
		"point_commentary_node",
		"dod_round_timer",
		"func_precipitation",
		"func_team_wall",
		"", // END Marker
	};

	// --------------------------------------------------------------------------------------------------- //
	// Global helper functions.
	// --------------------------------------------------------------------------------------------------- //
	
	// World.cpp calls this but we don't use it in DoD.
	void InitBodyQue()
	{
	}


	// Handler for the "bot" command.
	CON_COMMAND_F( bot, "Add a bot.", FCVAR_CHEAT )
	{
		//CDODPlayer *pPlayer = CDODPlayer::Instance( UTIL_GetCommandClientIndex() );

		// The bot command uses switches like command-line switches.
		// -count <count> tells how many bots to spawn.
		// -team <index> selects the bot's team. Default is -1 which chooses randomly.
		//	Note: if you do -team !, then it 
		// -class <index> selects the bot's class. Default is -1 which chooses randomly.
		// -frozen prevents the bots from running around when they spawn in.

		// Look at -count.
		int count = args.FindArgInt( "-count", 1 );
		count = clamp( count, 1, 16 );

		int iTeam = TEAM_ALLIES;
		const char *pVal = args.FindArg( "-team" );
		if ( pVal )
		{
			iTeam = atoi( pVal );
			iTeam = clamp( iTeam, 0, (GetNumberOfTeams()-1) );
		}

		int iClass = 0;
		pVal = args.FindArg( "-class" );
		if ( pVal )
		{
			iClass = atoi( pVal );
			iClass = clamp( iClass, 0, 10 );
		}

		// Look at -frozen.
		bool bFrozen = !!args.FindArg( "-frozen" );
			
		// Ok, spawn all the bots.
		while ( --count >= 0 )
		{
			BotPutInServer( bFrozen, iTeam, iClass );
		}
	}


	void RestartRound_f()
	{
		DODGameRules()->State_Transition( STATE_RESTART );
	}
	ConCommand cc_Restart( "restartround", RestartRound_f, "Restart the round", FCVAR_CHEAT );

	void CDODGameRules::CopyGamePlayLogic( const CDODGamePlayRules otherGamePlay )
	{
		m_GamePlayRules.CopyFrom( otherGamePlay );
	}

	// --------------------------------------------------------------------------------------------------- //
	// CDODGameRules implementation.
	// --------------------------------------------------------------------------------------------------- //

	CDODGameRules::CDODGameRules()
	{
		InitTeams();

		ResetMapTime();

		m_GamePlayRules.Reset();

		ResetScores();

		m_bInWarmup = false;
		m_bAwaitingReadyRestart = false;
		m_flRestartRoundTime = -1;

		m_iAlliesRespawnHead = 0;
		m_iAlliesRespawnTail = 0;
		m_iAxisRespawnHead = 0;
		m_iAxisRespawnTail = 0;
		m_iNumAlliesRespawnWaves = 0;
		m_iNumAxisRespawnWaves = 0;

		for ( int i=0; i <DOD_RESPAWN_QUEUE_SIZE; i++ )
		{
			m_AlliesRespawnQueue.Set( i, 0 );
			m_AxisRespawnQueue.Set( i, 0 );
		}

		m_bLevelInitialized = false;
		m_iSpawnPointCount_Allies = 0;
		m_iSpawnPointCount_Axis = 0;

		Q_memset( m_vecPlayerPositions,0, sizeof(m_vecPlayerPositions) );

		// Lets execute a map specific cfg file
		// Matt - execute this after server.cfg!
		char szCommand[256] = { 0 };
		// Map names cannot contain quotes or control characters so this is safe but silly that we have to do it.
		Q_snprintf( szCommand, sizeof(szCommand), "exec \"%s.cfg\"\n", STRING(gpGlobals->mapname) );
		engine->ServerCommand( szCommand );

		m_pCurStateInfo = NULL;
		State_Transition( STATE_PREGAME );

		// stats
		memset( m_iStatsKillsPerClass_Allies, 0, sizeof(m_iStatsKillsPerClass_Allies) );
		memset( m_iStatsKillsPerClass_Axis, 0, sizeof(m_iStatsKillsPerClass_Axis) );

		memset( m_iStatsSpawnsPerClass_Allies, 0, sizeof(m_iStatsSpawnsPerClass_Allies) );
		memset( m_iStatsSpawnsPerClass_Axis, 0, sizeof(m_iStatsSpawnsPerClass_Axis) );

		memset( m_iStatsCapsPerClass_Allies, 0, sizeof(m_iStatsCapsPerClass_Allies) );
		memset( m_iStatsCapsPerClass_Axis, 0, sizeof(m_iStatsCapsPerClass_Axis) );

		memset( m_iStatsDefensesPerClass_Allies, 0, sizeof(m_iStatsDefensesPerClass_Allies) );
		memset( m_iStatsDefensesPerClass_Axis, 0, sizeof(m_iStatsDefensesPerClass_Axis) );

		memset( &m_iWeaponShotsFired, 0, sizeof(m_iWeaponShotsFired) );
		memset( &m_iWeaponShotsHit, 0, sizeof(m_iWeaponShotsHit) );
		memset( &m_iWeaponDistanceBuckets, 0, sizeof(m_iWeaponDistanceBuckets) );

		memset( &m_flSecondsPlayedPerClass_Allies, 0, sizeof(m_flSecondsPlayedPerClass_Allies) );
		memset( &m_flSecondsPlayedPerClass_Axis, 0, sizeof(m_flSecondsPlayedPerClass_Axis) );

		m_bUsingTimer = false;
		m_pRoundTimer = NULL;	// created on first round spawn that requires a timer

		m_bAlliesAreBombing = false;
		m_bAxisAreBombing = false;

		m_flNextFailSafeWaveCheckTime = 0;

		// Init the holiday
		int day = 0, month = 0, year = 0;
		
#ifdef WIN32
		GetCurrentDate( &day, &month, &year );
#elif POSIX
		time_t now = time(NULL);
		struct tm *tm = localtime( &now );

		day = tm->tm_mday + 1;
		month = tm->tm_mon;
		year = tm->tm_year + 1900;
#endif

		if ( ( month == 12 && day >= 1 ) || ( month == 1 && day <= 2 ) )
		{
			m_bWinterHolidayActive = true;
		}
		else
		{
			m_bWinterHolidayActive = false;
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CDODGameRules::~CDODGameRules()
	{
		// Note, don't delete each team since they are in the gEntList and will 
		// automatically be deleted from there, instead.
		g_Teams.Purge();
	}

	void CDODGameRules::LevelShutdown( void )
	{
		UploadLevelStats();

		BaseClass::LevelShutdown();
	}

	#define MY_USHRT_MAX	0xffff
	#define MY_UCHAR_MAX	0xff

	void CDODGameRules::UploadLevelStats( void )
	{
		if ( Q_strlen( STRING( gpGlobals->mapname ) ) > 0 )
		{
			int i,j;
			CDODTeam *pAllies = GetGlobalDODTeam( TEAM_ALLIES );
			CDODTeam *pAxis = GetGlobalDODTeam( TEAM_AXIS );

			dod_gamestats_t stats;
			memset( &stats, 0, sizeof(stats) );

			// Header
			stats.header.iVersion = DOD_STATS_BLOB_VERSION;
			Q_strncpy( stats.header.szGameName, "dod", sizeof(stats.header.szGameName) );
			Q_strncpy( stats.header.szMapName, STRING( gpGlobals->mapname ), sizeof( stats.header.szMapName ) );

			ConVar *hostip = cvar->FindVar( "hostip" );
			if ( hostip )
			{
				int ip = hostip->GetInt();
				stats.header.ipAddr[0] = ip >> 24;
				stats.header.ipAddr[1] = ( ip >> 16 ) & 0xff;
				stats.header.ipAddr[2] = ( ip >> 8 ) & 0xff;
				stats.header.ipAddr[3] = ( ip ) & 0xff;
			}			

			ConVar *hostport = cvar->FindVar( "hostip" );
			if ( hostport )
			{
				stats.header.port = hostport->GetInt();
			}			

			stats.header.serverid = 0;

			stats.iMinutesPlayed = clamp( (short)( gpGlobals->curtime / 60 ), 0, MY_USHRT_MAX ); 

			// Team Scores
			stats.iNumAlliesWins = clamp( pAllies->GetRoundsWon(), 0, MY_UCHAR_MAX );
			stats.iNumAxisWins = clamp( pAxis->GetRoundsWon(), 0, MY_UCHAR_MAX );

			stats.iAlliesTickPoints = clamp( pAllies->GetScore(), 0, MY_USHRT_MAX );
			stats.iAxisTickPoints = clamp( pAxis->GetScore(), 0, MY_USHRT_MAX );		

			// Player Data
			for ( i=1;i<=MAX_PLAYERS;i++ )
			{
				CDODPlayer *pPlayer = ToDODPlayer( UTIL_PlayerByIndex( i ) );

				if ( pPlayer )
				{
					// Sum up the time played for players that are still connected.
					// players who disconnected had their connect time added in ClientDisconnected

					// Tally the latest time for this player
					pPlayer->TallyLatestTimePlayedPerClass( pPlayer->GetTeamNumber(), pPlayer->m_Shared.DesiredPlayerClass() );

					for( j=0;j<7;j++ )
					{
						m_flSecondsPlayedPerClass_Allies[j] += pPlayer->m_flTimePlayedPerClass_Allies[j];
						m_flSecondsPlayedPerClass_Axis[j] += pPlayer->m_flTimePlayedPerClass_Axis[j];
					}
				}
			}

			// convert to minutes
			for( j=0;j<7;j++ )
			{
				stats.iMinutesPlayedPerClass_Allies[j] = clamp( (short)( m_flSecondsPlayedPerClass_Allies[j] / 60 ), 0, MY_USHRT_MAX );
				stats.iMinutesPlayedPerClass_Axis[j] = clamp( (short)( m_flSecondsPlayedPerClass_Axis[j] / 60 ), 0, MY_USHRT_MAX );
			}

			for ( i=0;i<6;i++ )
			{
				stats.iKillsPerClass_Allies[i] = clamp( (short)m_iStatsKillsPerClass_Allies[i], 0, MY_USHRT_MAX );
				stats.iKillsPerClass_Axis[i] = clamp( (short)m_iStatsKillsPerClass_Axis[i], 0, MY_USHRT_MAX );

				stats.iSpawnsPerClass_Allies[i] = clamp( (short)m_iStatsSpawnsPerClass_Allies[i], 0, MY_USHRT_MAX );
				stats.iSpawnsPerClass_Axis[i] = clamp( (short)m_iStatsSpawnsPerClass_Axis[i], 0, MY_USHRT_MAX );

				stats.iCapsPerClass_Allies[i] = clamp( (short)m_iStatsCapsPerClass_Allies[i], 0, MY_USHRT_MAX );
				stats.iCapsPerClass_Axis[i] = clamp( (short)m_iStatsCapsPerClass_Axis[i], 0, MY_USHRT_MAX );

				stats.iDefensesPerClass_Allies[i] = clamp( m_iStatsDefensesPerClass_Allies[i], 0, MY_UCHAR_MAX );
				stats.iDefensesPerClass_Axis[i] = clamp( m_iStatsDefensesPerClass_Axis[i], 0, MY_UCHAR_MAX );
			}

			// Server Settings
			stats.iClassLimits_Allies[0] = clamp( mp_limitAlliesRifleman.GetInt(), -1, 254 );
			stats.iClassLimits_Allies[1] = clamp( mp_limitAlliesAssault.GetInt(), -1, 254 );
			stats.iClassLimits_Allies[2] = clamp( mp_limitAlliesSupport.GetInt(), -1, 254 );
			stats.iClassLimits_Allies[3] = clamp( mp_limitAlliesSniper.GetInt(), -1, 254 );
			stats.iClassLimits_Allies[4] = clamp( mp_limitAlliesMachinegun.GetInt(), -1, 254 );
			stats.iClassLimits_Allies[5] = clamp( mp_limitAlliesRocket.GetInt(), -1, 254 );

			stats.iClassLimits_Axis[0] = clamp( mp_limitAxisRifleman.GetInt(), -1, 254 );
			stats.iClassLimits_Axis[1] = clamp( mp_limitAxisAssault.GetInt(), -1, 254 );
			stats.iClassLimits_Axis[2] = clamp( mp_limitAxisSupport.GetInt(), -1, 254 );
			stats.iClassLimits_Axis[3] = clamp( mp_limitAxisSniper.GetInt(), -1, 254 );
			stats.iClassLimits_Axis[4] = clamp( mp_limitAxisMachinegun.GetInt(), -1, 254 );
			stats.iClassLimits_Axis[5] = clamp( mp_limitAxisRocket.GetInt(), -1, 254 );

			// Weapon Data

			// Send hit/shots/distance info for the following guns / modes
			for ( i=0;i<DOD_NUM_DISTANCE_STAT_WEAPONS;i++ )
			{
				int weaponId = iDistanceStatWeapons[i];

				stats.weaponStatsDistance[i].iNumHits = clamp( m_iWeaponShotsHit[weaponId], 0, MY_USHRT_MAX );
				stats.weaponStatsDistance[i].iNumAttacks = clamp( m_iWeaponShotsFired[weaponId], 0, MY_USHRT_MAX );
				for ( int j=0;j<DOD_NUM_WEAPON_DISTANCE_BUCKETS;j++ )
				{
					stats.weaponStatsDistance[i].iDistanceBuckets[j] = clamp( m_iWeaponDistanceBuckets[weaponId][j], 0, MY_USHRT_MAX );
				}
			}

			// Send hit/shots info for the following guns / modes
			for ( i=0;i<DOD_NUM_NODIST_STAT_WEAPONS;i++ )
			{
				int weaponId = iNoDistStatWeapons[i];
				stats.weaponStats[i].iNumHits = clamp( m_iWeaponShotsHit[weaponId], 0, MY_USHRT_MAX );
				stats.weaponStats[i].iNumAttacks = clamp( m_iWeaponShotsFired[weaponId], 0, MY_USHRT_MAX );
			}

			const void *pvBlobData = ( const void * )( &stats );
			unsigned int uBlobSize = sizeof( stats );

			if ( gamestatsuploader )
			{
				gamestatsuploader->UploadGameStats( 
					STRING( gpGlobals->mapname ),
					DOD_STATS_BLOB_VERSION,
					uBlobSize,
					pvBlobData );
			}
		}
	}

	void CDODGameRules::Stats_PlayerKill( int team, int cls )
	{
		Assert( cls >= 0 && cls <= 5 );

		if ( cls >= 0 && cls <= 5 )
		{
			if ( team == TEAM_ALLIES )
                m_iStatsKillsPerClass_Allies[cls]++;
			else if ( team == TEAM_AXIS )
				m_iStatsKillsPerClass_Axis[cls]++;
		}
	}

	void CDODGameRules::Stats_PlayerCap( int team, int cls )
	{
		Assert( cls >= 0 && cls <= 5 );

		if ( cls >= 0 && cls <= 5 )
		{
			if ( team == TEAM_ALLIES )
				m_iStatsCapsPerClass_Allies[cls]++;
			else if ( team == TEAM_AXIS )
				m_iStatsCapsPerClass_Axis[cls]++;
		}
	}

	void CDODGameRules::Stats_PlayerDefended( int team, int cls )
	{
		Assert( cls >= 0 && cls <= 5 );

		if ( cls >= 0 && cls <= 5 )
		{
			if ( team == TEAM_ALLIES )
				m_iStatsDefensesPerClass_Allies[cls]++;
			else if ( team == TEAM_AXIS )
				m_iStatsDefensesPerClass_Axis[cls]++;
		}
	}

	void CDODGameRules::Stats_WeaponFired( int weaponID )
	{
		m_iWeaponShotsFired[weaponID]++;
	}

	void CDODGameRules::Stats_WeaponHit( int weaponID, float flDist )
	{
		m_iWeaponShotsHit[weaponID]++;
		
		int bucket = Stats_WeaponDistanceToBucket( weaponID, flDist );
		m_iWeaponDistanceBuckets[weaponID][bucket]++;
	}

	int CDODGameRules::Stats_WeaponDistanceToBucket( int weaponID, float flDist )
	{
		int bucket = 4;
		int iDist = (int)flDist;

		for ( int i=0;i<DOD_NUM_WEAPON_DISTANCE_BUCKETS-1;i++ )
		{
			if ( iDist < iWeaponBucketDistances[i] )
			{
				bucket = i;
				break;
			}
		}

		return bucket;
	}


	//-----------------------------------------------------------------------------
	// Purpose: DoD Specific Client Commands
	// Input  :
	// Output :
	//-----------------------------------------------------------------------------
	bool CDODGameRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
	{
		CDODPlayer *pPlayer = ToDODPlayer( pEdict );
		const char *pcmd = args[0];
#ifdef DEBUG
		if ( FStrEq( pcmd, "teamwin" ) )
		{
			if ( args.ArgC() < 2 )
				return true;
	
			SetWinningTeam( atoi( args[1] ) );			

			return true;
		}
		else 
#endif
		// Handle some player commands here as they relate more directly to gamerules state
		if ( FStrEq( pcmd, "nextmap" ) )
		{
			CDODPlayer *pDODPlayer = ToDODPlayer(pPlayer);

			if ( pDODPlayer->m_flNextTimeCheck < gpGlobals->curtime )
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
				
				pDODPlayer->m_flNextTimeCheck = gpGlobals->curtime + 1;
			}

			return true;
		}
		else if ( FStrEq( pcmd, "timeleft" ) )
		{	
			CDODPlayer *pDODPlayer = ToDODPlayer(pPlayer);
			
			if ( pDODPlayer->m_flNextTimeCheck < gpGlobals->curtime )
			{
				if ( mp_timelimit.GetInt() > 0 )
				{
					int iTimeLeft = GetTimeLeft();

					char szMinutes[5];
					char szSeconds[3];

					if ( iTimeLeft <= 0 )
					{
						Q_snprintf( szMinutes, sizeof(szMinutes), "0" );
						Q_snprintf( szSeconds, sizeof(szSeconds), "00" );
					}
					else
					{
						Q_snprintf( szMinutes, sizeof(szMinutes), "%d", iTimeLeft / 60 );
						Q_snprintf( szSeconds, sizeof(szSeconds), "%02d", iTimeLeft % 60 );
					}				

					ClientPrint( pPlayer, HUD_PRINTTALK, "#game_time_left1", szMinutes, szSeconds );
				}
				else
				{
					ClientPrint( pPlayer, HUD_PRINTTALK, "#game_time_left2" );
				}

				CDODPlayer *pDODPlayer = ToDODPlayer(pPlayer);
				pDODPlayer->m_flNextTimeCheck = gpGlobals->curtime + 1;
			}
			return true;
		}
		else if ( pPlayer->ClientCommand( args ) )
		{
			return true;
		}
		else if ( BaseClass::ClientCommand( pEdict, args ) )
		{
			return true;
		}

		return false;
	}

	void CDODGameRules::CheckChatForReadySignal( CDODPlayer *pPlayer, const char *chatmsg )
	{
		if( m_bAwaitingReadyRestart && FStrEq( chatmsg, mp_clan_ready_signal.GetString() ) )
		{
			if( !m_bHeardAlliesReady && pPlayer->GetTeamNumber() == TEAM_ALLIES )
			{
				m_bHeardAlliesReady = true;

				IGameEvent *event = gameeventmanager->CreateEvent( "dod_allies_ready" );
				if ( event )
                    gameeventmanager->FireEvent( event );
			}
			else if( !m_bHeardAxisReady && pPlayer->GetTeamNumber() == TEAM_AXIS )
			{
				m_bHeardAxisReady = true;
				IGameEvent *event = gameeventmanager->CreateEvent( "dod_axis_ready" );
				if ( event )
					gameeventmanager->FireEvent( event );
			}
		}
	}

	int CDODGameRules::SelectDefaultTeam()
	{
		int team = TEAM_UNASSIGNED;

		CDODTeam *pAllies = GetGlobalDODTeam(TEAM_ALLIES);
		CDODTeam *pAxis = GetGlobalDODTeam(TEAM_AXIS);

		int iNumAllies = pAllies->GetNumPlayers();
		int iNumAxis = pAxis->GetNumPlayers();

		int iAlliesRoundsWon = pAllies->GetRoundsWon();
		int iAxisRoundsWon  = pAxis->GetRoundsWon();

		int iAlliesPoints = pAllies->GetScore();
		int iAxisPoints  = pAxis->GetScore();

		// Choose the team that's lacking players
		if ( iNumAllies < iNumAxis )
		{
			team = TEAM_ALLIES;
		}
		else if ( iNumAllies > iNumAxis )
		{
			team = TEAM_AXIS;
		}
		// Choose the team that's losing
		else if ( iAlliesRoundsWon < iAxisRoundsWon )
		{
			team = TEAM_ALLIES;
		}
		else if ( iAlliesRoundsWon > iAxisRoundsWon )
		{
			team = TEAM_AXIS;
		}
		// choose the team with fewer points
		else if ( iAlliesPoints < iAxisPoints )
		{
			team = TEAM_ALLIES;
		}
		else if ( iAlliesPoints > iAxisPoints )
		{
			team = TEAM_AXIS;
		}
		else
		{
			// Teams and scores are equal, pick a random team
			team = ( random->RandomInt(0,1) == 0 ) ? TEAM_ALLIES : TEAM_AXIS;		
		}

		if ( TeamFull( team ) )
		{
			// Pick the opposite team
			if ( team == TEAM_ALLIES )
			{
				team = TEAM_AXIS;
			}
			else
			{
				team = TEAM_ALLIES;
			}

			// No choices left
			if ( TeamFull( team ) )
				return TEAM_UNASSIGNED;
		}

		return team;
	}

	bool CDODGameRules::TeamFull( int team_id )
	{
		switch ( team_id )
		{
		case TEAM_ALLIES:
			{
				int iNumAllies = GetGlobalDODTeam(TEAM_ALLIES)->GetNumPlayers();
				return iNumAllies >= m_iSpawnPointCount_Allies;
			}
		case TEAM_AXIS:
			{
				int iNumAxis = GetGlobalDODTeam(TEAM_AXIS)->GetNumPlayers();
				return iNumAxis >= m_iSpawnPointCount_Axis;
			}
		}

		return false;
	}


	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------

	// return a multiplier that should adjust the damage done by a blast at position vecSrc to something at the position
	// vecEnd.  This will take into account the density of an entity that blocks the line of sight from one position to
	// the other.
	//
	// this algorithm was taken from the HL2 version of RadiusDamage.
	float CDODGameRules::GetExplosionDamageAdjustment(Vector & vecSrc, Vector & vecEnd, CBaseEntity *pTarget, CBaseEntity *pEntityToIgnore)
	{
		float retval = 0.0;
		trace_t tr;

		UTIL_TraceLine(vecSrc, vecEnd, MASK_SHOT, pEntityToIgnore, COLLISION_GROUP_NONE, &tr);

		Assert( pTarget );

		// its a hit if we made it to the dest, or if we hit another part of the target on the way
		if (tr.fraction == 1.0 || tr.m_pEnt == pTarget )
		{
			retval = 1.0;
		}
		else if (!(tr.DidHitWorld()) && (tr.m_pEnt != NULL) && (tr.m_pEnt->GetOwnerEntity() != pEntityToIgnore))
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

					const float ONE_OVER_DENSITY_ABSORB_ALL_DAMAGE = ( 1.0 / 3000.0 );
					float scale = flDensity * ONE_OVER_DENSITY_ABSORB_ALL_DAMAGE;

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
	float CDODGameRules::GetAmountOfEntityVisible(Vector & vecSrc, CBaseEntity *entity, CBaseEntity *pIgnoreEntity )
	{
		float retval = 0.0;

		Vector vecHullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;

		const float damagePercentageChest = 0.40;
		const float damagePercentageHead = 0.30;
		const float damagePercentageFoot = 0.10;	// x 2
		const float damagePercentageHand = 0.05;	// x 2

		if (!(entity->IsPlayer()))
		{
			// the entity is not a player, so the damage is all or nothing.
			Vector vecTarget;
			vecTarget = entity->BodyTarget(vecSrc, false);

			return GetExplosionDamageAdjustment(vecSrc, vecTarget, entity, pIgnoreEntity);
		}

		CDODPlayer *player = ToDODPlayer(entity);

		/* 
		new, sane method
		*/

		static int iRHandIndex = 0;
		static int iLHandIndex = 0;
		static int iHeadIndex = 0; 
		static int iChestIndex = 0;
		static int iRFootIndex = 0;
		static int iLFootIndex = 0;

		static bool bInitializedBones = false;

		if ( !bInitializedBones )
		{
			iRHandIndex = player->LookupBone( "ValveBiped.Bip01_R_Hand" );
			iLHandIndex = player->LookupBone( "ValveBiped.Bip01_L_Hand" );
			iHeadIndex = player->LookupBone( "ValveBiped.Bip01_Head1" ); 
			iChestIndex = player->LookupBone( "ValveBiped.Bip01_Spine2" );
			iRFootIndex = player->LookupBone( "ValveBiped.Bip01_R_Foot" );
			iLFootIndex = player->LookupBone( "ValveBiped.Bip01_L_Foot" );

			Assert( iRHandIndex != -1 );
			Assert( iLHandIndex != -1 );
			Assert( iHeadIndex  != -1 );
			Assert( iChestIndex != -1 );
			Assert( iRFootIndex != -1 );
			Assert( iLFootIndex != -1 );

			bInitializedBones = true;
		}

#ifdef _DEBUG
		// verify that these bone indeces don't change
		int checkBoneIndex = player->LookupBone( "ValveBiped.Bip01_R_Hand" );
		Assert( checkBoneIndex == iRHandIndex );
#endif
		

		QAngle dummyAngle;

		Vector vecRHand;
		player->GetBonePosition( iRHandIndex, vecRHand, dummyAngle );

		Vector vecLHand;
		player->GetBonePosition( iLHandIndex, vecLHand, dummyAngle );

		Vector vecHead;
		player->GetBonePosition( iHeadIndex, vecHead, dummyAngle );

		Vector vecChest;
		player->GetBonePosition( iChestIndex, vecChest, dummyAngle );

		Vector vecRFoot;
		player->GetBonePosition( iRFootIndex, vecRFoot, dummyAngle );

		Vector vecLFoot;
		player->GetBonePosition( iLFootIndex, vecLFoot, dummyAngle );

		// right hand
		float damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecRHand, player, pIgnoreEntity );
		retval += (damagePercentageHand * damageAdjustment);

/*
		Msg( "right hand: %.1f\n", damageAdjustment );
		NDebugOverlay::Line( vecSrc, vecRHand,
						(int)(damageAdjustment * 255.0),
						(int)((1.0 - damageAdjustment) * 255.0),
						0,
						true,
						10 );*/


		// left hand
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecLHand, player, pIgnoreEntity );
		retval += (damagePercentageHand * damageAdjustment);

/*
		Msg( "left hand: %.1f\n", damageAdjustment );
		NDebugOverlay::Line( vecSrc, vecLHand,
			(int)(damageAdjustment * 255.0),
			(int)((1.0 - damageAdjustment) * 255.0),
			0,
			true,
			10 );*/


		// head
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecHead, player, pIgnoreEntity );
		retval += (damagePercentageHead * damageAdjustment);

/*
		Msg( "head: %.1f\n", damageAdjustment );
		NDebugOverlay::Line( vecSrc, vecHead,
			(int)(damageAdjustment * 255.0),
			(int)((1.0 - damageAdjustment) * 255.0),
			0,
			true,
			10 );*/


		// chest
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecChest, player, pIgnoreEntity );
		retval += (damagePercentageChest * damageAdjustment);

/*
		Msg( "chest: %.1f\n", damageAdjustment );
		NDebugOverlay::Line( vecSrc, vecChest,
			(int)(damageAdjustment * 255.0),
			(int)((1.0 - damageAdjustment) * 255.0),
			0,
			true,
			10 );*/


		// right foot
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecRFoot, player, pIgnoreEntity );
		retval += (damagePercentageFoot * damageAdjustment);

/*
		Msg( "right foot: %.1f\n", damageAdjustment );
		NDebugOverlay::Line( vecSrc, vecRFoot,
			(int)(damageAdjustment * 255.0),
			(int)((1.0 - damageAdjustment) * 255.0),
			0,
			true,
			10 );*/


		// left foot
		damageAdjustment = GetExplosionDamageAdjustment(vecSrc, vecRFoot, player, pIgnoreEntity );
		retval += (damagePercentageFoot * damageAdjustment);

/*
		Msg( "left foot: %.1f\n", damageAdjustment );
		NDebugOverlay::Line( vecSrc, vecRFoot,
			(int)(damageAdjustment * 255.0),
			(int)((1.0 - damageAdjustment) * 255.0),
			0,
			true,
			10 );*/


//		Msg( "total: %.1f\n", retval );

		return retval;
	}

	void CDODGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore )
	{
		RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore, pEntityIgnore, false );
	}

	ConVar r_visualizeExplosion( "r_visualizeExplosion", "0", FCVAR_CHEAT );

	void CDODGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore, bool bIgnoreWorld /* = false */ )
	{
		CBaseEntity *pEntity = NULL;
		trace_t		tr;
		float		flAdjustedDamage, falloff;
		Vector		vecSpot;
		Vector		vecToTarget;

		Vector vecSrc = vecSrcIn;

		float flDamagePercentage;

		if ( flRadius )
			falloff = info.GetDamage() / flRadius;
		else
			falloff = 1.0;
		
		vecSrc.z += 1;// in case grenade is lying on the ground

		if ( r_visualizeExplosion.GetBool() )
		{
			float flLethalRange = ( info.GetDamage() - 100 ) / falloff;
			float flHalfDamageRange = ( info.GetDamage() - 50 ) / falloff;
			float flZeroDamageRange = ( info.GetDamage() ) / falloff;

			// draw a red sphere representing the kill area
			Vector dest = vecSrc;
			dest.x += flLethalRange;

			NDebugOverlay::HorzArrow( vecSrc, dest, 10, 255, 0, 0, 255, true, 10.0 );

			// yellow for 50 damage
			dest = vecSrc;
			dest.x += flHalfDamageRange;

			NDebugOverlay::HorzArrow( vecSrc, dest, 10, 255, 255, 0, 255, true, 10.0 );

			// green for > 0 damage
			dest = vecSrc;
			dest.x += flZeroDamageRange;

			NDebugOverlay::HorzArrow( vecSrc, dest, 10, 0, 255, 0, 255, true, 10.0 );
		}

		// iterate on all entities in the vicinity.
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
		{
			if ( pEntity->m_takedamage != DAMAGE_NO )
			{
				// UNDONE: this should check a damage mask, not an ignore
				if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
					continue;

				if ( pEntity == pEntityIgnore )
					continue;

				// radius damage can only be blocked by the world
				vecSpot = pEntity->BodyTarget( vecSrc );

				if ( bIgnoreWorld )
				{
					flDamagePercentage = 1.0;
				}
				else
				{
					// get the percentage of the target entity that is visible from the
					// explosion position.
					flDamagePercentage = GetAmountOfEntityVisible(vecSrc, pEntity, info.GetInflictor() );
				}

				if (flDamagePercentage > 0.0)
				{
					// the explosion can 'see' this entity, so hurt them!
					vecToTarget = ( vecSpot - vecSrc );

					// decrease damage for an ent that's farther from the bomb.
					flAdjustedDamage = vecToTarget.Length() * falloff;
					flAdjustedDamage = info.GetDamage() - flAdjustedDamage;

					flAdjustedDamage *= flDamagePercentage;
				
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

						pEntity->TakeDamage( adjustedInfo );
			
						// Now hit all triggers along the way that respond to damage... 
						pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, vecSpot, dir );
					}
				}
			}
		}
	}

	void CDODGameRules::RadiusStun( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius )
	{
		CBaseEntity *pEntity = NULL;
		trace_t		tr;
		float		flAdjustedDamage, falloff;
		Vector		vecSpot;
		Vector		vecToTarget;

		if ( flRadius )
			falloff = info.GetDamage() / flRadius;
		else
			falloff = 1.0;

		// ok, now send updates to all clients
		CBitVec< ABSOLUTE_PLAYER_LIMIT > playerbits;
		playerbits.ClearAll();

		// see which players are actually in the PVS of the grenade
		engine->Message_DetermineMulticastRecipients( false, vecSrc, playerbits );

		// Iterate through all players that made it into playerbits, that are inside the radius
		// and give them stun damage
		for ( int i=0;i<MAX_PLAYERS;i++ )
		{
			if ( playerbits.Get(i) == false )
				continue;

			pEntity = UTIL_EntityByIndex( i+1 );

			if ( !pEntity || !pEntity->IsPlayer() )
				continue;

			if ( pEntity->m_takedamage != DAMAGE_NO )
			{
				// radius damage can only be blocked by the world
				vecSpot = pEntity->BodyTarget( vecSrc );

				// the explosion can 'see' this entity, so hurt them!
				vecToTarget = ( vecSpot - vecSrc );

				float flDist = vecToTarget.Length();

				// make sure they are inside the radius
				if ( flDist > flRadius )
					continue;

				// decrease damage for an ent that's farther from the bomb.
				flAdjustedDamage = flDist * falloff;
				flAdjustedDamage = info.GetDamage() - flAdjustedDamage;

				if ( flAdjustedDamage > 0 )
				{
					CTakeDamageInfo adjustedInfo = info;
					adjustedInfo.SetDamage( flAdjustedDamage );

					pEntity->TakeDamage( adjustedInfo );
				}
			}
		}
	}

	void CDODGameRules::Think()
	{
		if ( g_fGameOver )   // someone else quit the game already
		{
			// check to see if we should change levels now
			if ( m_flIntermissionEndTime < gpGlobals->curtime )
			{
				ChangeLevel(); // intermission is over
			}	
			
			return;
		}

		State_Think();
		
		if ( gpGlobals->curtime > m_flNextPeriodicThink )
		{
			if ( CheckTimeLimit() )
				return;

			if ( CheckWinLimit() )
				return;

			CheckRestartRound();
			CheckWarmup();
			CheckPlayerPositions();

			m_flNextPeriodicThink = gpGlobals->curtime + 1.0;
		}

		CGameRules::Think();
	}

	void CDODGameRules::GoToIntermission( void )
	{
		BaseClass::GoToIntermission();

		// set all players to FL_FROZEN
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CDODPlayer *pPlayer = ToDODPlayer( UTIL_PlayerByIndex( i ) );

			if ( pPlayer )
			{
				pPlayer->AddFlag( FL_FROZEN );

				pPlayer->StatEvent_UploadStats();
			}
		}

		// Print out map stats to a text file
		//WriteStatsFile( "stats.xml" );

		State_Enter( STATE_GAME_OVER );
	}

	void CDODGameRules::SetInWarmup( bool bWarmup )
	{
		if( m_bInWarmup == bWarmup )
			return;

		m_bInWarmup = bWarmup;
		
		if( m_bInWarmup )
		{
			m_flWarmupTimeEnds = gpGlobals->curtime + mp_warmup_time.GetFloat();
			DevMsg( "Warmup_Begin\n" );

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_warmup_begins" );
			if ( event )
				gameeventmanager->FireEvent( event );
		}
		else
		{
			m_flWarmupTimeEnds = -1;
			DevMsg( "Warmup_Ends\n" );

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_warmup_ends" );
			if ( event )
				gameeventmanager->FireEvent( event );
		}
	}

	void CDODGameRules::CheckWarmup( void )
	{
		if( mp_restartwarmup.GetBool() )
		{
			if( m_bInWarmup )
			{
				m_flWarmupTimeEnds = gpGlobals->curtime + mp_warmup_time.GetFloat();
			}
			else
				DODGameRules()->SetInWarmup( true );

			mp_restartwarmup.SetValue( 0 );
		}

		if( mp_cancelwarmup.GetBool() )
		{
			DODGameRules()->SetInWarmup( false );
			mp_cancelwarmup.SetValue( 0 );
		}

		if( m_bInWarmup )
		{
			// only exit the warmup if the time is up, and we are not in a round
			// restart countdown already, and we are not waiting for a ready restart
			if( gpGlobals->curtime > m_flWarmupTimeEnds && m_flRestartRoundTime < 0 && !m_bAwaitingReadyRestart )
			{
				// no need to end the warmup, the restart will end it automatically
				//SetInWarmup( false );

				m_flRestartRoundTime = gpGlobals->curtime;	// reset asap
			}
		}
	}

	void CDODGameRules::CheckRestartRound( void )
	{
		if( mp_clan_readyrestart.GetBool() )
		{
			m_bAwaitingReadyRestart = true;
			m_bHeardAlliesReady = false;
			m_bHeardAxisReady = false;

			const char *pszReadyString = mp_clan_ready_signal.GetString();

			UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#clan_ready_rules", pszReadyString );
			UTIL_ClientPrintAll( HUD_PRINTTALK, "#clan_ready_rules", pszReadyString );

			// Don't let them put anything malicious in there
			if( pszReadyString == NULL || Q_strlen(pszReadyString) > 16 )
			{
				pszReadyString = "ready";
			}

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_ready_restart" );
			if ( event )
				gameeventmanager->FireEvent( event );

			mp_clan_readyrestart.SetValue( 0 );

			// cancel any restart round in progress
			m_flRestartRoundTime = -1;
		}

		// Restart the game if specified by the server
		int iRestartDelay = mp_clan_restartround.GetInt();

		if ( iRestartDelay > 0 )
		{
			if ( iRestartDelay > 60 )
				iRestartDelay = 60;

			m_flRestartRoundTime = gpGlobals->curtime + iRestartDelay;

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_round_restart_seconds" );
			if ( event )
			{
				event->SetInt( "seconds", iRestartDelay );
				gameeventmanager->FireEvent( event );
			}

			mp_clan_restartround.SetValue( 0 );

			// cancel any ready restart in progress
			m_bAwaitingReadyRestart = false;
		}
	}

	bool CDODGameRules::CheckTimeLimit()
	{
		if ( IsGameUnderTimeLimit() )
		{
			if( GetTimeLeft() <= 0 )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "dod_game_over" );
				if ( event )
				{
					event->SetString( "reason", "Reached Time Limit" );
					gameeventmanager->FireEvent( event );
				}

				SendTeamScoresEvent();

				GoToIntermission();
				return true;
			}
		}

		return false;
	}

	bool CDODGameRules::CheckWinLimit()
	{
		// has one team won the specified number of rounds?

		int iWinLimit = mp_winlimit.GetInt();

		if ( iWinLimit > 0 )
		{
			CDODTeam *pAllies = GetGlobalDODTeam(TEAM_ALLIES);
			CDODTeam *pAxis = GetGlobalDODTeam(TEAM_AXIS);

			bool bAlliesWin = pAllies->GetRoundsWon() >= iWinLimit;
			bool bAxisWin = pAxis->GetRoundsWon() >= iWinLimit;

			if ( bAlliesWin || bAxisWin )
			{
				IGameEvent *event = gameeventmanager->CreateEvent( "dod_game_over" );
				if ( event )
				{
					event->SetString( "reason", "Reached Round Win Limit" );
					gameeventmanager->FireEvent( event );
				}

				GoToIntermission();
				return true;
			}
		}

		return false;
	}

	void CDODGameRules::CheckPlayerPositions()
	{
		int i;
		bool bUpdatePlayer[MAX_PLAYERS];
		Q_memset( bUpdatePlayer, 0, sizeof(bUpdatePlayer) );

		// check all players
		for ( i=1; i<=gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			Vector origin = pPlayer->GetAbsOrigin();

			Vector2D pos( (int)(origin.x/4), (int)(origin.y/4) );

			if ( pos == m_vecPlayerPositions[i-1] )
				continue; // player didn't move enough

			m_vecPlayerPositions[i-1] = pos;

			bUpdatePlayer[i-1] = true; // player position changed since last time
		}

		// ok, now send updates to all clients
		CBitVec< ABSOLUTE_PLAYER_LIMIT > playerbits;

		for ( i=1; i<=gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			
			if ( !pPlayer )
				continue;

			if ( !pPlayer->IsConnected() )
				continue;

			CSingleUserRecipientFilter filter(pPlayer);

			UserMessageBegin( filter, "UpdateRadar" );

			playerbits.ClearAll();
				
			// see what other players are in it's PVS, don't update them
			engine->Message_DetermineMulticastRecipients( false, pPlayer->EyePosition(), playerbits );

			for ( int i=0; i < gpGlobals->maxClients; i++ )
			{
				if ( playerbits.Get(i)	)
					continue; // this player is in his PVS, don't update radar pos

				if ( !bUpdatePlayer[i] )
					continue;

				CBasePlayer *pOtherPlayer = UTIL_PlayerByIndex( i+1 );

				if ( !pOtherPlayer )
					continue; // nothing there

				if ( pOtherPlayer == pPlayer )
					continue; // dont update himself

				if ( !pOtherPlayer->IsAlive() || pOtherPlayer->IsObserver() || !pOtherPlayer->IsConnected() )
					continue; // don't update spectators or dead players

				if ( pPlayer->GetTeamNumber() > TEAM_SPECTATOR )
				{
					// update only team mates if not a pure spectator
					if ( pPlayer->GetTeamNumber() != pOtherPlayer->GetTeamNumber() )
						continue;
				}

				WRITE_BYTE( i+1 ); // player entity index 
				WRITE_SBITLONG( m_vecPlayerPositions[i].x, COORD_INTEGER_BITS-1 );
				WRITE_SBITLONG( m_vecPlayerPositions[i].y, COORD_INTEGER_BITS-1 );
				WRITE_SBITLONG( AngleNormalize( pOtherPlayer->GetAbsAngles().y ), 9 );
			}

			WRITE_BYTE( 0 ); // end marker

			MessageEnd();	// send message
		}
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


	void TestSpawnPointType( const char *pEntClassName )
	{
		// Find the next spawn spot.
		CBaseEntity *pSpot = gEntList.FindEntityByClassname( NULL, pEntClassName );

		while( pSpot )
		{
			// check if pSpot is valid
			if( g_pGameRules->IsSpawnPointValid( pSpot, NULL ) )
			{
				// the successful spawn point's location
				NDebugOverlay::Box( pSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 0, 255, 0, 100, 60 );

				// drop down to ground
				Vector GroundPos = DropToGround( NULL, pSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX );

				// the location the player will spawn at
				NDebugOverlay::Box( GroundPos, VEC_HULL_MIN, VEC_HULL_MAX, 0, 0, 255, 100, 60 );

				// draw the spawn angles
				QAngle spotAngles = pSpot->GetLocalAngles();
				Vector vecForward;
				AngleVectors( spotAngles, &vecForward );
				NDebugOverlay::HorzArrow( pSpot->GetAbsOrigin(), pSpot->GetAbsOrigin() + vecForward * 32, 10, 255, 0, 0, 255, true, 60 );
			}
			else
			{
				// failed spawn point location
				NDebugOverlay::Box( pSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX, 255, 0, 0, 100, 60 );
			}

			// increment pSpot
			pSpot = gEntList.FindEntityByClassname( pSpot, pEntClassName );
		}
	}

	void TestSpawns()
	{
		TestSpawnPointType( "info_player_allies" );
		TestSpawnPointType( "info_player_axis" );
	}
	ConCommand cc_TestSpawns( "map_showspawnpoints", TestSpawns, "Dev - test the spawn points, draws for 60 seconds", FCVAR_CHEAT );

	CBaseEntity *CDODGameRules::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
	{
		// get valid spawn point
		CBaseEntity *pSpawnSpot = pPlayer->EntSelectSpawnPoint();

		// drop down to ground
		Vector GroundPos = DropToGround( pPlayer, pSpawnSpot->GetAbsOrigin(), VEC_HULL_MIN, VEC_HULL_MAX );

		// Move the player to the place it said.
		pPlayer->Teleport( &GroundPos, &pSpawnSpot->GetLocalAngles(), &vec3_origin );
		pPlayer->m_Local.m_vecPunchAngle = vec3_angle;

		return pSpawnSpot;
	}

	// checks if the spot is clear of players
	bool CDODGameRules::IsSpawnPointValid( CBaseEntity *pSpot, CBasePlayer *pPlayer )
	{
		if ( !pSpot->IsTriggered( pPlayer ) )
		{
			return false;
		}

		// Check if it is disabled by Enable/Disable
		CSpawnPoint *pSpawnPoint = dynamic_cast< CSpawnPoint * >( pSpot );
		if ( pSpawnPoint )
		{
			if ( pSpawnPoint->IsDisabled() )
			{
				return false;
			}
		}

		Vector mins = GetViewVectors()->m_vHullMin;
		Vector maxs = GetViewVectors()->m_vHullMax;

		Vector vTestMins = pSpot->GetAbsOrigin() + mins;
		Vector vTestMaxs = pSpot->GetAbsOrigin() + maxs;

		// First test the starting origin.
		return UTIL_IsSpaceEmpty( pPlayer, vTestMins, vTestMaxs );
	}

	void CDODGameRules::PlayerSpawn( CBasePlayer *p )
	{	
		CDODPlayer *pPlayer = ToDODPlayer( p );

		int team = pPlayer->GetTeamNumber();

		if( team == TEAM_ALLIES || team == TEAM_AXIS )
		{
			int iPreviousPlayerClass = pPlayer->m_Shared.PlayerClass();

			if( pPlayer->m_Shared.DesiredPlayerClass() == PLAYERCLASS_RANDOM )
			{
				ChooseRandomClass( pPlayer );
				ClientPrint( pPlayer, HUD_PRINTTALK, "#game_now_as", GetPlayerClassName( pPlayer->m_Shared.PlayerClass(), team ) );
			}
			else
			{
				pPlayer->m_Shared.SetPlayerClass( pPlayer->m_Shared.DesiredPlayerClass() );
			}

			int playerclass = pPlayer->m_Shared.PlayerClass();

			if ( playerclass != iPreviousPlayerClass )
			{
				// spawning as a new class, flush stats
				pPlayer->StatEvent_UploadStats();
			}

			if( playerclass != PLAYERCLASS_UNDEFINED )
			{
				//Assert( PLAYERCLASS_UNDEFINED < playerclass && playerclass < NUM_PLAYERCLASSES );

				int i;

				CDODTeam *pTeam = GetGlobalDODTeam( team );
				const CDODPlayerClassInfo &pClassInfo = pTeam->GetPlayerClassInfo( playerclass );

				Assert( pClassInfo.m_iTeam == team );
								
				pPlayer->SetModel( pClassInfo.m_szPlayerModel );
				pPlayer->SetHitboxSet( 0 );

				char buf[64];
				int bufsize = sizeof(buf);

				//Give weapons

				// Primary weapon
				Q_snprintf( buf, bufsize, "weapon_%s", WeaponIDToAlias(pClassInfo.m_iPrimaryWeapon) );
				CBaseEntity *pPrimaryWpn = pPlayer->GiveNamedItem( buf );
				Assert( pPrimaryWpn );

				// Secondary weapon
				CBaseEntity *pSecondaryWpn = NULL;
				if ( pClassInfo.m_iSecondaryWeapon != WEAPON_NONE )
				{
					Q_snprintf( buf, bufsize, "weapon_%s", WeaponIDToAlias(pClassInfo.m_iSecondaryWeapon) );
					pSecondaryWpn = pPlayer->GiveNamedItem( buf );
				}
				
				// Melee weapon
				if ( pClassInfo.m_iMeleeWeapon )
				{
					Q_snprintf( buf, bufsize, "weapon_%s", WeaponIDToAlias(pClassInfo.m_iMeleeWeapon) );
					pPlayer->GiveNamedItem( buf );
				}

				CWeaponDODBase *pWpn = NULL;

				// Primary Ammo
				pWpn = dynamic_cast<CWeaponDODBase *>(pPrimaryWpn);

				if( pWpn )
				{
					int iNumClip = pWpn->GetDODWpnData().m_iDefaultAmmoClips - 1;	//account for one clip in the gun
					int iClipSize = pWpn->GetDODWpnData().iMaxClip1;
					pPlayer->GiveAmmo( iNumClip * iClipSize, pWpn->GetDODWpnData().szAmmo1 );
				}

				// Secondary Ammo
				if ( pSecondaryWpn )
				{
					pWpn = dynamic_cast<CWeaponDODBase *>(pSecondaryWpn);

					if( pWpn )
					{
						int iNumClip = pWpn->GetDODWpnData().m_iDefaultAmmoClips - 1;	//account for one clip in the gun
						int iClipSize = pWpn->GetDODWpnData().iMaxClip1;
						pPlayer->GiveAmmo( iNumClip * iClipSize, pWpn->GetDODWpnData().szAmmo1 );
					}
				}				

				// Grenade Type 1
				if ( pClassInfo.m_iGrenType1 != WEAPON_NONE )
				{
					Q_snprintf( buf, bufsize, "weapon_%s", WeaponIDToAlias(pClassInfo.m_iGrenType1) );
					for ( i=0;i<pClassInfo.m_iNumGrensType1;i++ )
					{
						pPlayer->GiveNamedItem( buf );
					}
				}

				// Grenade Type 2
				if ( pClassInfo.m_iGrenType2 != WEAPON_NONE )
				{
					Q_snprintf( buf, bufsize, "weapon_%s", WeaponIDToAlias(pClassInfo.m_iGrenType2) );
					for ( i=0;i<pClassInfo.m_iNumGrensType2;i++ )
					{
						pPlayer->GiveNamedItem( buf );
					}
				}

				pPlayer->Weapon_Switch( (CBaseCombatWeapon *)pPrimaryWpn );

				// you get a helmet
				pPlayer->SetBodygroup( BODYGROUP_HELMET, pClassInfo.m_iHelmetGroup );

				// no jumpgear
				pPlayer->SetBodygroup( BODYGROUP_JUMPGEAR, BODYGROUP_JUMPGEAR_OFF );

				pPlayer->SetMaxSpeed( 600 );

				Assert( playerclass >= 0 && playerclass <= 5 );
				if ( playerclass >= 0 && playerclass <= 5 )
				{
					if ( team == TEAM_ALLIES )
						m_iStatsSpawnsPerClass_Allies[playerclass]++;
					else if ( team == TEAM_AXIS )
						m_iStatsSpawnsPerClass_Axis[playerclass]++;
				}
			}
			else
			{
				Assert( !"Player spawning with PLAYERCLASS_UNDEFINED" );
				pPlayer->SetModel( NULL );
			}
		}
	}

	const char *CDODGameRules::GetPlayerClassName( int cls, int team )
	{
		CDODTeam *pTeam = GetGlobalDODTeam( team );

		if( cls == PLAYERCLASS_RANDOM )
		{
			return "#class_random";
		}

		if( cls < 0 || cls >= pTeam->GetNumPlayerClasses() )
		{
			Assert( false );
			return NULL;
		}

		const CDODPlayerClassInfo &pClassInfo = pTeam->GetPlayerClassInfo( cls );

		return pClassInfo.m_szPrintName;
	}

	void CDODGameRules::ChooseRandomClass( CDODPlayer *pPlayer )
	{
		int i;
		int numChoices = 0;
		int choices[16];
		int firstclass = 0;

		CDODTeam *pTeam = GetGlobalDODTeam( pPlayer->GetTeamNumber() );

		int lastclass = pTeam->GetNumPlayerClasses();

		int previousClass = pPlayer->m_Shared.PlayerClass();

		// Compile a list of the classes that aren't full
		for( i=firstclass;i<lastclass;i++ )
		{
			// don't join the same class twice in a row
			if ( i == previousClass )
				continue;

			if( CanPlayerJoinClass( pPlayer, i ) )
			{	
				choices[numChoices] = i;
				numChoices++;
			}
		}

		// If ALL the classes are full
		if( numChoices == 0 )
		{
			Msg( "Random class found that all classes were full - ignoring class limits for this spawn\n" );

			pPlayer->m_Shared.SetPlayerClass( random->RandomFloat( firstclass, lastclass ) );
		}
		else
		{
			// Choose a slot randomly
			i = random->RandomInt( 0, numChoices-1 );

			// We are now the class that was in that slot
			pPlayer->m_Shared.SetPlayerClass( choices[i] );
		}
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

		Vector vTestMins = mins;
		Vector vTestMaxs = maxs;

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
			for ( int iDim=0; iDim < 2; iDim++ )
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

	bool CDODGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
	{
		//only allow one primary, one secondary and one melee
		CWeaponDODBase *pWpn = (CWeaponDODBase *)pWeapon;

		if( pWpn )
		{
			int type = pWpn->GetDODWpnData().m_WeaponType;

			switch( type )
			{
			case WPN_TYPE_MELEE:
				{
#ifdef DEBUG
					CWeaponDODBase *pMeleeWeapon = (CWeaponDODBase *)pPlayer->Weapon_GetSlot( WPN_SLOT_MELEE );
					bool bHasMelee = ( pMeleeWeapon != NULL );

					if( bHasMelee )
					{
						Assert( !"Why are we trying to add another melee?" );
						return false;
					}
#endif
				}
				break;
			case WPN_TYPE_PISTOL:
			case WPN_TYPE_SIDEARM:
				{
#ifdef DEBUG
					CWeaponDODBase *pSecondaryWeapon = (CWeaponDODBase *)pPlayer->Weapon_GetSlot( WPN_SLOT_SECONDARY );
					bool bHasPistol = ( pSecondaryWeapon != NULL );

					if( bHasPistol )
					{
						Assert( !"Why are we trying to add another pistol?" );
						return false;
					}
#endif
				}
				break;

			case WPN_TYPE_CAMERA:
				return true;

			case WPN_TYPE_RIFLE:
			case WPN_TYPE_SNIPER:
			case WPN_TYPE_SUBMG:
			case WPN_TYPE_MG:
			case WPN_TYPE_BAZOOKA:
				{
					//Don't pick up dropped weapons if we have one already
					CWeaponDODBase *pPrimaryWeapon = (CWeaponDODBase *)pPlayer->Weapon_GetSlot( WPN_SLOT_PRIMARY );
					bool bHasPrimary = ( pPrimaryWeapon != NULL );

					if( bHasPrimary )
						return false;
				}
				break;

			default:
				break;
			}
		}

		return BaseClass::CanHavePlayerItem( pPlayer, pWeapon );
	}

	void CDODGameRules::ResetMapTime( void )
	{
		m_flMapResetTime = gpGlobals->curtime;

		// send an event with the time remaining until map change

		IGameEvent *event = gameeventmanager->CreateEvent( "dod_map_time_remaining" );
		if ( event )
		{
			event->SetInt( "seconds", GetTimeLeft() );
			gameeventmanager->FireEvent( event );
		}
	}

#endif

bool CDODGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		V_swap(collisionGroup0,collisionGroup1);
	}
	
	//Don't stand on COLLISION_GROUP_WEAPONs
	if( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
		collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		return false;
	}

	// TE shells don't collide with the player
	if ( collisionGroup0 == COLLISION_GROUP_PLAYER &&
		 collisionGroup1 == DOD_COLLISIONGROUP_SHELLS )
	{
		return false;
	}

	// blocker walls only collide with players
	if ( collisionGroup1 == DOD_COLLISIONGROUP_BLOCKERWALL )
		return ( collisionGroup0 == COLLISION_GROUP_PLAYER ) || ( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT );
	
	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 
}

int CDODGameRules::GetSubTeam( int team )
{
	return SUBTEAM_NORMAL;
}

bool CDODGameRules::IsGameUnderTimeLimit( void )
{
	return ( mp_timelimit.GetInt() > 0 );
}

int CDODGameRules::GetTimeLeft( void )
{
	float flTimeLimit = mp_timelimit.GetInt() * 60;

	Assert( flTimeLimit > 0 && "Should not call this function when !IsGameUnderTimeLimit" );

	float flMapChangeTime = m_flMapResetTime + flTimeLimit;

#ifndef CLIENT_DLL
	// If the round timer is longer, let the round complete
	if ( m_bUsingTimer && m_pRoundTimer )
	{
		float flTimerSeconds = m_pRoundTimer->GetTimeRemaining();
		float flMapChangeSeconds = flMapChangeTime - gpGlobals->curtime;

		// if the map timer is less than the round timer
		// AND
		// the round timer is less than 2 minutes


		// If the map time for any reason goes beyond the end of the round, remove the flag
		if ( flMapChangeSeconds > flTimerSeconds )
		{
			m_bChangeLevelOnRoundEnd = false;
		}
		else if ( m_bChangeLevelOnRoundEnd || flTimerSeconds < 120 )
		{
			// once this happens once in a round, use this until the round ends
			// or else the round will end when a team captures an objective and adds time to above 120
			m_bChangeLevelOnRoundEnd = true;

			return (int)( flTimerSeconds );
		}
	}
#endif

	return ( (int)(flMapChangeTime - gpGlobals->curtime) );
}

int CDODGameRules::GetReinforcementTimerSeconds( int team, float flSpawnEligibleTime )
{
	// find the first wave that this player can fit in

	float flWaveTime = -1;

	switch( team )
	{
	case TEAM_ALLIES:
		{
			int i = m_iAlliesRespawnHead;

			while( i != m_iAlliesRespawnTail )
			{
				if ( flSpawnEligibleTime < m_AlliesRespawnQueue[i] )
				{
					flWaveTime = m_AlliesRespawnQueue[i];
					break;
				}

				i = ( i+1 ) % DOD_RESPAWN_QUEUE_SIZE;
			}
		}
		break;
	case TEAM_AXIS:
		{
			int i = m_iAxisRespawnHead;

			while( i != m_iAxisRespawnTail )
			{
				if ( flSpawnEligibleTime < m_AxisRespawnQueue[i] )
				{
					flWaveTime = m_AxisRespawnQueue[i];
					break;
				}

				i = ( i+1 ) % DOD_RESPAWN_QUEUE_SIZE;
			}
		}
		break;
	default:
		return -1;
	}

	return MAX( 0, (int)( flWaveTime - gpGlobals->curtime ) );
}

const CViewVectors* CDODGameRules::GetViewVectors() const
{
	return &g_DODViewVectors;
}

const CDODViewVectors *CDODGameRules::GetDODViewVectors() const
{
	return &g_DODViewVectors;
}

#ifndef CLIENT_DLL

	extern ConVar dod_bonusround;

	bool CDODGameRules::IsFriendlyFireOn( void )
	{
		// Never friendly fire in bonus round
		if ( IsInBonusRound() )
		{
			return false;
		}
		
		return friendlyfire.GetBool();
	}

	bool CDODGameRules::IsInBonusRound( void )
	{
		return ( dod_bonusround.GetBool() == true && ( State_Get() == STATE_ALLIES_WIN || State_Get() == STATE_AXIS_WIN ) );
	}

	ConVar dod_showroundtransitions( "dod_showroundtransitions", "0", 0, "Show gamestate round transitions" );

	void CDODGameRules::State_Transition( DODRoundState newState )
	{
		State_Leave();
		State_Enter( newState );
	}	

	void CDODGameRules::State_Enter( DODRoundState newState )
	{
		m_iRoundState = newState;
		m_pCurStateInfo = State_LookupInfo( newState );

		if ( dod_showroundtransitions.GetInt() > 0 )
		{
			if ( m_pCurStateInfo )
				Msg( "DODRoundState: entering '%s'\n", m_pCurStateInfo->m_pStateName );
			else
				Msg( "DODRoundState: entering #%d\n", newState );
		}
		
		// Initialize the new state.
		if ( m_pCurStateInfo && m_pCurStateInfo->pfnEnterState )
			(this->*m_pCurStateInfo->pfnEnterState)();
	}

	void CDODGameRules::State_Leave()
	{
		if ( m_pCurStateInfo && m_pCurStateInfo->pfnLeaveState )
		{
			(this->*m_pCurStateInfo->pfnLeaveState)();
		}
	}


	void CDODGameRules::State_Think()
	{
		if ( m_pCurStateInfo && m_pCurStateInfo->pfnThink )
		{
			(this->*m_pCurStateInfo->pfnThink)();
		}
	}


	CDODRoundStateInfo* CDODGameRules::State_LookupInfo( DODRoundState state )
	{
		static CDODRoundStateInfo playerStateInfos[] =
		{
			{ STATE_INIT,		"STATE_INIT",		&CDODGameRules::State_Enter_INIT, NULL, &CDODGameRules::State_Think_INIT },
			{ STATE_PREGAME,	"STATE_PREGAME",	&CDODGameRules::State_Enter_PREGAME, NULL, &CDODGameRules::State_Think_PREGAME },
			{ STATE_STARTGAME,	"STATE_STARTGAME",	&CDODGameRules::State_Enter_STARTGAME, NULL, &CDODGameRules::State_Think_STARTGAME },
			{ STATE_PREROUND,	"STATE_PREROUND",	&CDODGameRules::State_Enter_PREROUND, NULL, &CDODGameRules::State_Think_PREROUND },
			{ STATE_RND_RUNNING,"STATE_RND_RUNNING",&CDODGameRules::State_Enter_RND_RUNNING, NULL,	&CDODGameRules::State_Think_RND_RUNNING },
			{ STATE_ALLIES_WIN,	"STATE_ALLIES_WIN",	&CDODGameRules::State_Enter_ALLIES_WIN, NULL,	&CDODGameRules::State_Think_ALLIES_WIN },
			{ STATE_AXIS_WIN,	"STATE_AXIS_WIN",	&CDODGameRules::State_Enter_AXIS_WIN,	NULL, &CDODGameRules::State_Think_AXIS_WIN },
			{ STATE_RESTART,	"STATE_RESTART",	&CDODGameRules::State_Enter_RESTART,	NULL, &CDODGameRules::State_Think_RESTART },
			{ STATE_GAME_OVER,	"STATE_GAME_OVER",	NULL, NULL, NULL },
		};

		for ( int i=0; i < ARRAYSIZE( playerStateInfos ); i++ )
		{
			if ( playerStateInfos[i].m_iRoundState == state )
				return &playerStateInfos[i];
		}

		return NULL;
	}

	extern ConVar sv_stopspeed;
	extern ConVar sv_friction;

	void CDODGameRules::State_Enter_INIT( void )
	{
		InitTeams();

		sv_stopspeed.SetValue( 50.0f );
		sv_friction.SetValue( 8.0f );

		ResetMapTime();
	}

	void CDODGameRules::State_Think_INIT( void )
	{
		State_Transition( STATE_PREGAME );
	}

	void CDODGameRules::InitTeams( void )
	{
		Assert( g_Teams.Count() == 0 );

		g_Teams.Purge();	// just in case

		// Create the team managers
		int i;
		for ( i = 0; i < 2; i++ )	// Unassigned and Spectators
		{
			CTeam *pTeam = static_cast<CTeam*>(CreateEntityByName( "dod_team_manager" ));
			pTeam->Init( sTeamNames[i], i );

			g_Teams.AddToTail( pTeam );
		}

		// clear the player class data
		ResetFilePlayerClassInfoDatabase();

		CTeam *pAllies = static_cast<CTeam*>(CreateEntityByName( "dod_team_allies" ));
		Assert( pAllies );
		pAllies->Init( sTeamNames[TEAM_ALLIES], TEAM_ALLIES );
		g_Teams.AddToTail( pAllies );

		CTeam *pAxis = static_cast<CTeam*>(CreateEntityByName( "dod_team_axis" ));
		Assert( pAxis );
		pAxis->Init( sTeamNames[TEAM_AXIS], TEAM_AXIS );
		g_Teams.AddToTail( pAxis );
	}

	// dod_control_point_master can take inputs to add time to the round timer
	void CDODGameRules::AddTimerSeconds( int iSecondsToAdd )
	{
		if( m_bUsingTimer && m_pRoundTimer )
		{
			m_pRoundTimer->AddTimerSeconds( iSecondsToAdd );

			float flTimerSeconds = m_pRoundTimer->GetTimeRemaining();

			m_bPlayTimerWarning_1Minute = ( flTimerSeconds > 60 );					
			m_bPlayTimerWarning_2Minute = ( flTimerSeconds > 120 );

			IGameEvent *event = gameeventmanager->CreateEvent( "dod_timer_time_added" );
			if ( event )
			{
				event->SetInt( "seconds_added", iSecondsToAdd );
				gameeventmanager->FireEvent( event );
			}
		}
	}

	int CDODGameRules::GetTimerSeconds( void )
	{
		if( m_bUsingTimer && m_pRoundTimer )
		{
			return m_pRoundTimer->GetTimeRemaining();
		}
		else
		{
			return 0;
		}
	}
	
	// PREGAME - the server is idle and waiting for enough
	// players to start up again. When we find an active player
	// go to STATE_STARTGAME
	void CDODGameRules::State_Enter_PREGAME( void )
	{
		m_flNextPeriodicThink = gpGlobals->curtime + 0.1;

		Load_EntText();
	}

	void CDODGameRules::State_Think_PREGAME( void )
	{
		CheckLevelInitialized();

		if( CountActivePlayers() > 0 )
			State_Transition( STATE_STARTGAME );			
	}

	// STARTGAME - wait a bit and then spawn everyone into the 
	// preround
	void CDODGameRules::State_Enter_STARTGAME( void )
	{
		m_flStateTransitionTime = gpGlobals->curtime + 5 * dod_enableroundwaittime.GetFloat();

		m_bInitialSpawn = true;
	}

	void CDODGameRules::State_Think_STARTGAME()
	{
		if( gpGlobals->curtime > m_flStateTransitionTime )
		{
			if( mp_warmup_time.GetFloat() > 0 )
			{
				// go into warmup, reset at end of it
				SetInWarmup( true );
			}

			State_Transition( STATE_PREROUND );
		}
   	}

	void CDODGameRules::State_Enter_PREROUND( void )
	{
		// Longer wait time if its the first round, let people join
		if ( m_bInitialSpawn )
		{
			m_flStateTransitionTime = gpGlobals->curtime + 10 * dod_enableroundwaittime.GetFloat();
			m_bInitialSpawn = false;
		}
		else
		{
			m_flStateTransitionTime = gpGlobals->curtime + 5 * dod_enableroundwaittime.GetFloat();
		}

		//Game rules may change, if a new one becomes mastered at the end of the last round
		DetectGameRules();

		//reset everything in the level
		RoundRespawn();

		// reset this now! If its reset at round restart, we lose all the players that died
		// during the preround 
		m_iAlliesRespawnHead = 0;
		m_iAlliesRespawnTail = 0;
		m_iAxisRespawnHead = 0;
		m_iAxisRespawnTail = 0;
		m_iNumAlliesRespawnWaves = 0;
		m_iNumAxisRespawnWaves = 0;

		m_iLastAlliesCapEvent = CAP_EVENT_NONE;
		m_iLastAxisCapEvent = CAP_EVENT_NONE;

		//find all the control points, init the timer
		CBaseEntity *pEnt =	gEntList.FindEntityByClassname( NULL, "dod_control_point_master" );

		if( !pEnt )
		{
			Warning( "No dod_control_point_master found in level - control points will not work as expected.\n" );
		}

		bool bFoundTimer = false;

		while( pEnt )
		{
			variant_t emptyVariant;
			pEnt->AcceptInput( "RoundInit", NULL, NULL, emptyVariant, 0 );

			CControlPointMaster *pMaster = dynamic_cast<CControlPointMaster *>( pEnt );

			if ( pMaster && pMaster->IsActive() )
			{
				if ( pMaster->IsUsingRoundTimer() )
				{
					bFoundTimer = true;

					m_bUsingTimer = true;

					int iTimerSeconds;

					pMaster->GetTimerData( iTimerSeconds, m_iTimerWinTeam );

					if ( m_iTimerWinTeam != TEAM_ALLIES && m_iTimerWinTeam != TEAM_AXIS )
					{
						Assert( !"Round timer win team can only be allies or axis!\n" );
					}

					// Timer starts paused
					if ( !m_pRoundTimer.Get() )
					{
						m_pRoundTimer = ( CDODRoundTimer *) CreateEntityByName( "dod_round_timer" );
					}
					
					Assert( m_pRoundTimer );

					if ( m_pRoundTimer )
					{
						m_pRoundTimer->SetTimeRemaining( iTimerSeconds );
						m_pRoundTimer->PauseTimer();

						m_bPlayTimerWarning_1Minute = ( iTimerSeconds > 60 );					
						m_bPlayTimerWarning_2Minute = ( iTimerSeconds > 120 );
					}
				}
			}

			pEnt = gEntList.FindEntityByClassname( pEnt, "dod_control_point_master" );
		}

		if ( bFoundTimer == false )
		{
			// No masters are active that require the round timer, destroy it
			UTIL_Remove( m_pRoundTimer.Get() );
			m_pRoundTimer = NULL;
		}

		//init the cap areas
		pEnt =	gEntList.FindEntityByClassname( NULL, "dod_capture_area" );
		while( pEnt )
		{
			variant_t emptyVariant;
			pEnt->AcceptInput( "RoundInit", NULL, NULL, emptyVariant, 0 );

			pEnt = gEntList.FindEntityByClassname( pEnt, "dod_capture_area" );
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "dod_round_start" );
		if ( event )
			gameeventmanager->FireEvent( event );

		// figure out which teams are bombing
		m_bAlliesAreBombing = false;
		m_bAxisAreBombing = false;

		pEnt =	gEntList.FindEntityByClassname( NULL, "dod_bomb_target" );
		while( pEnt )
		{
			CDODBombTarget *pTarget = dynamic_cast<CDODBombTarget *>( pEnt );

			if ( pTarget && pTarget->State_Get() == BOMB_TARGET_ACTIVE )
			{
				switch( pTarget->GetBombingTeam() )
				{
				case TEAM_ALLIES:
					m_bAlliesAreBombing = true;
					break;
				case TEAM_AXIS:
					m_bAxisAreBombing = true;
					break;
				default:
					break;
				}			
			}

			pEnt = gEntList.FindEntityByClassname( pEnt, "dod_bomb_target" );
		}
	}

	void CDODGameRules::State_Think_PREROUND( void )
	{
		if( gpGlobals->curtime > m_flStateTransitionTime )
			State_Transition( STATE_RND_RUNNING );

		CheckRespawnWaves();
	}

	void CDODGameRules::State_Enter_RND_RUNNING( void )
	{
		//find all the control points, init the timer
		CBaseEntity *pEnt =	gEntList.FindEntityByClassname( NULL, "dod_control_point_master" );

		while( pEnt )
		{
			variant_t emptyVariant;
			pEnt->AcceptInput( "RoundStart", NULL, NULL, emptyVariant, 0 );

			pEnt = gEntList.FindEntityByClassname( pEnt, "dod_control_point_master" );
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "dod_round_active" );
		if ( event )
			gameeventmanager->FireEvent( event );

		if( !IsInWarmup() )
			PlayStartRoundVoice();

		if ( m_bUsingTimer && m_pRoundTimer.Get() != NULL )
		{
			m_pRoundTimer->ResumeTimer();
		}

		m_bChangeLevelOnRoundEnd = false;
	}

	void CDODGameRules::State_Think_RND_RUNNING( void )
	{
		//Where the magic happens

		if ( m_bUsingTimer && m_pRoundTimer )
		{
			float flSecondsRemaining = m_pRoundTimer->GetTimeRemaining();

			if ( flSecondsRemaining <= 0 )
			{
				// if there is a bomb still on a timer, and that bomb has 
				// the potential to add time, then we don't end the game

				bool bBombBlocksWin = false;

				//find all the control points, init the timer
				CBaseEntity *pEnt =	gEntList.FindEntityByClassname( NULL, "dod_bomb_target" );

				while( pEnt )
				{
					CDODBombTarget *pBomb = dynamic_cast<CDODBombTarget *>( pEnt );

					// Find active bombs that have the potential to add round time
					if ( pBomb && pBomb->State_Get() == BOMB_TARGET_ARMED )
					{
						if ( pBomb->GetTimerAddSeconds() > 0 )
						{
							// don't end the round until this bomb goes off or is disarmed
							bBombBlocksWin = true;
							break;
						}

						CControlPoint *pPoint = pBomb->GetControlPoint();
						int iBombingTeam = pBomb->GetBombingTeam();

						if ( pPoint && pPoint->GetBombsRemaining() <= 1 )
						{
							// find active dod_control_point_masters, ask them if this flag capping 
							// would end the game
							CBaseEntity *pEnt =	gEntList.FindEntityByClassname( NULL, "dod_control_point_master" );

							while( pEnt )
							{
								CControlPointMaster *pMaster = dynamic_cast<CControlPointMaster *>( pEnt );

								if ( pMaster->IsActive() )
								{
									// Check TeamOwnsAllPoints, while overriding this particular flag's owner
									if ( pMaster->WouldNewCPOwnerWinGame( pPoint, iBombingTeam ) )
									{
										// This bomb may win the game, don't end the round.
										bBombBlocksWin = true;
										break;
									}
								}

								pEnt = gEntList.FindEntityByClassname( pEnt, "dod_control_point_master" );
							}							
						}
					}

					pEnt = gEntList.FindEntityByClassname( pEnt, "dod_bomb_target" );
				}

				if ( bBombBlocksWin == false )
				{
					SetWinningTeam( m_iTimerWinTeam );

					// tell the dod_control_point_master to fire its outputs for the winning team!
					// minor hackage - dod_gamerules should be responsible for team win events, not dod_cpm

					//find all the control points, init the timer
					CBaseEntity *pEnt =	gEntList.FindEntityByClassname( NULL, "dod_control_point_master" );

					while( pEnt )
					{
						CControlPointMaster *pMaster = dynamic_cast<CControlPointMaster *>( pEnt );

						if ( pMaster->IsActive() )
						{
							pMaster->FireTeamWinOutput( m_iTimerWinTeam );
						}

						pEnt = gEntList.FindEntityByClassname( pEnt, "dod_control_point_master" );
					}
				}                
			}
			else if ( flSecondsRemaining < 60.0 && m_bPlayTimerWarning_1Minute == true )
			{
				// play one minute warning
				DevMsg( 1, "Timer Warning: 1 Minute Remaining\n" );

				IGameEvent *event = gameeventmanager->CreateEvent( "dod_timer_flash" );
				if ( event )
				{
					event->SetInt( "time_remaining", 60 );
					gameeventmanager->FireEvent( event );
				}

				m_bPlayTimerWarning_1Minute = false;
			}
			else if ( flSecondsRemaining < 120.0 && m_bPlayTimerWarning_2Minute == true )
			{
				// play two minute warning
				DevMsg( 1, "Timer Warning: 2 Minutes Remaining\n" );

				IGameEvent *event = gameeventmanager->CreateEvent( "dod_timer_flash" );
				if ( event )
				{
					event->SetInt( "time_remaining", 120 );
					gameeventmanager->FireEvent( event );
				}

				m_bPlayTimerWarning_2Minute = false;
			}
		}

		//if we don't find any active players, return to STATE_PREGAME
		if( CountActivePlayers() <= 0 )
		{
			State_Transition( STATE_PREGAME );
			return;
		}

		CheckRespawnWaves();

		// check round restart
		if( m_flRestartRoundTime > 0 && m_flRestartRoundTime < gpGlobals->curtime )
		{
			// time to restart!
			State_Transition( STATE_RESTART );
			m_flRestartRoundTime = -1;
		}

		// check ready restart
		if( m_bAwaitingReadyRestart && m_bHeardAlliesReady && m_bHeardAxisReady )
		{
			//State_Transition( STATE_RESTART );
			m_flRestartRoundTime = gpGlobals->curtime + 5;
			m_bAwaitingReadyRestart = false;
		}
	}

	void CDODGameRules::CheckRespawnWaves( void )
	{
		bool bDoFailSafeWaveCheck = false;

		if ( m_flNextFailSafeWaveCheckTime < gpGlobals->curtime )
		{
			bDoFailSafeWaveCheck = true;
			m_flNextFailSafeWaveCheckTime = gpGlobals->curtime + 3.0;
		}

		//Respawn Timers
		if( m_iNumAlliesRespawnWaves > 0 )
		{
			if ( m_AlliesRespawnQueue[m_iAlliesRespawnHead] < gpGlobals->curtime )
			{
				DevMsg( "Wave: Respawning Allies\n" );

				RespawnTeam( TEAM_ALLIES ); 

				PopWaveTime( TEAM_ALLIES );
			}
		}
		else if ( bDoFailSafeWaveCheck )
		{
			// if there are any allied people waiting to spawn, spawn them
			FailSafeSpawnPlayersOnTeam( TEAM_ALLIES );
		}

		if( m_iNumAxisRespawnWaves > 0 )
		{
			if ( m_AxisRespawnQueue[m_iAxisRespawnHead] < gpGlobals->curtime )
			{
				DevMsg( "Wave: Respawning Axis\n" );

				RespawnTeam( TEAM_AXIS ); 

				PopWaveTime( TEAM_AXIS );
			}
		}
		else if ( bDoFailSafeWaveCheck )
		{
			// if there are any axis people waiting to spawn, spawn them
			FailSafeSpawnPlayersOnTeam( TEAM_AXIS );
		}
	}

	void CDODGameRules::FailSafeSpawnPlayersOnTeam( int iTeam )
	{
		DODRoundState roundState = State_Get();

		CDODTeam *pTeam = GetGlobalDODTeam( iTeam );
		if ( pTeam )
		{
			int iNumPlayers = pTeam->GetNumPlayers();
			for ( int i=0;i<iNumPlayers;i++ )
			{
				CDODPlayer *pPlayer = pTeam->GetDODPlayer(i);
				if ( !pPlayer )
					continue;

				// if this player is waiting to spawn, spawn them

				if ( pPlayer->IsAlive() )
					continue;

				if( pPlayer->m_Shared.DesiredPlayerClass() == PLAYERCLASS_UNDEFINED )
					continue;

				if ( gpGlobals->curtime < ( pPlayer->GetDeathTime() + DEATH_CAM_TIME ) )
					continue;

				if ( pPlayer->GetObserverMode() == OBS_MODE_FREEZECAM )
					continue;

				if ( roundState != STATE_PREROUND && pPlayer->State_Get() == STATE_DEATH_ANIM )
					continue;

				// Respawn this player
				pPlayer->DODRespawn();

				Assert( !"This will happen, but see if we can figure out why we get here" );
			}
		}
	}

	//ALLIES WIN
	void CDODGameRules::State_Enter_ALLIES_WIN( void )
	{
		float flTime = MAX( 5, dod_bonusroundtime.GetFloat() );

		m_flStateTransitionTime = gpGlobals->curtime + flTime * dod_enableroundwaittime.GetFloat();

		if ( m_bUsingTimer && m_pRoundTimer )
		{
			m_pRoundTimer->PauseTimer();
		}
	}

	void CDODGameRules::State_Think_ALLIES_WIN( void )
	{
		if( gpGlobals->curtime > m_flStateTransitionTime )
		{
			State_Transition( STATE_PREROUND );
		}
	}

	//AXIS WIN
	void CDODGameRules::State_Enter_AXIS_WIN( void )
	{
		float flTime = MAX( 5, dod_bonusroundtime.GetFloat() );

		m_flStateTransitionTime = gpGlobals->curtime + flTime * dod_enableroundwaittime.GetFloat();

		if ( m_bUsingTimer && m_pRoundTimer )
		{
			m_pRoundTimer->PauseTimer();
		}
	}

	void CDODGameRules::State_Think_AXIS_WIN( void )
	{
		if( gpGlobals->curtime > m_flStateTransitionTime )
		{
			State_Transition( STATE_PREROUND );
		}
	}

	// manual restart
	void CDODGameRules::State_Enter_RESTART( void )
	{
		// send scores
		SendTeamScoresEvent();

		// send restart event
		IGameEvent *event = gameeventmanager->CreateEvent( "dod_restart_round" );
		if ( event )
			gameeventmanager->FireEvent( event );

		SetInWarmup( false );

		ResetScores();

		// reset the round time
		ResetMapTime();

		State_Transition( STATE_PREROUND );
	}

	void CDODGameRules::SendTeamScoresEvent( void )
	{
		// send scores
		IGameEvent *event = gameeventmanager->CreateEvent( "dod_team_scores" );

		if ( event )
		{
			CDODTeam *pAllies = GetGlobalDODTeam( TEAM_ALLIES );
			CDODTeam *pAxis = GetGlobalDODTeam( TEAM_AXIS );

			Assert( pAllies && pAxis );

			event->SetInt( "allies_caps", pAllies->GetRoundsWon() );
			event->SetInt( "allies_tick", pAllies->GetScore() );
			event->SetInt( "allies_players", pAllies->GetNumPlayers() );
			event->SetInt( "axis_caps", pAxis->GetRoundsWon() );
			event->SetInt( "axis_tick", pAxis->GetScore() );
			event->SetInt( "axis_players", pAxis->GetNumPlayers() );

			gameeventmanager->FireEvent( event );
		}
	}

	void CDODGameRules::State_Think_RESTART( void )
	{
		Assert( 0 ); // should never get here, State_Enter_RESTART sets us into a different state
	}

	void CDODGameRules::ResetScores( void )
	{
		GetGlobalDODTeam( TEAM_ALLIES )->ResetScores();
		GetGlobalDODTeam( TEAM_AXIS )->ResetScores();

		CDODPlayer *pDODPlayer;

		for( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pDODPlayer = ToDODPlayer( UTIL_PlayerByIndex( i ) );

			if (pDODPlayer == NULL)
				continue;

			if (FNullEnt( pDODPlayer->edict() ))
				continue;

			pDODPlayer->ResetScores();
        }
	}

	ConVar dod_showcleanedupents( "dod_showcleanedupents", "0", 0, "Show entities that are removed on round respawn" );

	// Utility function
	bool FindInList( const char **pStrings, const char *pToFind )
	{
		int i = 0;
		while ( pStrings[i][0] != 0 )
		{
			if ( Q_stricmp( pStrings[i], pToFind ) == 0 )
				return true;
			i++;
		}

		return false;
	}

	void CDODGameRules::CleanUpMap()
	{
		// Recreate all the map entities from the map data (preserving their indices),
		// then remove everything else except the players.

		if( dod_showcleanedupents.GetBool() )
		{
			Msg( "CleanUpMap\n===============\n" );
		}

		// Get rid of all entities except players.
		CBaseEntity *pCur = gEntList.FirstEnt();
		while ( pCur )
		{
			if ( !FindInList( s_PreserveEnts, pCur->GetClassname() ) )
			{
				if( dod_showcleanedupents.GetBool() )
				{
					Msg( "Removed Entity: %s\n", pCur->GetClassname() );
				}
				UTIL_Remove( pCur );
			}
			
			pCur = gEntList.NextEnt( pCur );
		}
		
		// Really remove the entities so we can have access to their slots below.
		gEntList.CleanupDeleteList();
		
		// Now reload the map entities.
		class CDODMapEntityFilter : public IMapEntityFilter
		{
		public:
			virtual bool ShouldCreateEntity( const char *pClassname )
			{
				// Don't recreate the preserved entities.
				if ( !FindInList( s_PreserveEnts, pClassname ) )
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
					// CDODMapLoadEntityFilter, which should have built the g_MapEntityRefs list
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
		CDODMapEntityFilter filter;
		filter.m_iIterator = g_MapEntityRefs.Head();

		// DO NOT CALL SPAWN ON info_node ENTITIES!

		MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );
	}

	int CDODGameRules::CountActivePlayers( void )
	{
		int i;
		int count = 0;
		CDODPlayer *pDODPlayer;

		for (i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pDODPlayer = ToDODPlayer( UTIL_PlayerByIndex( i ) );

			if( pDODPlayer )
			{
				if( pDODPlayer->IsReadyToPlay() )
				{
					count++;
				}
			}
		}

		return count;
	}

	void CDODGameRules::RoundRespawn( void )
	{
		CleanUpMap();
		RespawnAllPlayers();

		// reset per-round scores for each player
		for ( int i=0;i<MAX_PLAYERS;i++ )
		{
			CDODPlayer *pPlayer = ToDODPlayer( UTIL_PlayerByIndex( i ) );

			if ( pPlayer )
			{
				pPlayer->ResetPerRoundStats();
			}
		}
	}

	typedef struct {
		int iPlayerIndex;
		int iScore;
	} playerscore_t;

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

	// Store which event happened most recently, flag cap or bomb explode
	void CDODGameRules::CapEvent( int event, int team )
	{
		switch( team )
		{
		case TEAM_ALLIES:
			m_iLastAlliesCapEvent = event;
			break;
		case TEAM_AXIS:
			m_iLastAxisCapEvent = event;
			break;
		}
	}

	void FillEventCategory( IGameEvent *event, int side, int category, CUtlVector<playerscore_t> &pList )
	{
		switch ( side )
		{
		case 0:
			event->SetInt( "category_left", category );
			break;
		case 1:
			event->SetInt( "category_right", category );
			break;
		}

		static const char *pCategoryNames[2][6] = 
		{
			{
				"left_1",
				"left_score_1",
				"left_2",
				"left_score_2",
				"left_3",
				"left_score_3"
			},
			{
				"right_1",
				"right_score_1",
				"right_2",
				"right_score_2",
				"right_3",
				"right_score_3"
			}
		};

		int iNumInList = pList.Count();

		if ( iNumInList > 0 )
		{
			event->SetInt( pCategoryNames[side][0], pList[0].iPlayerIndex );
			event->SetInt( pCategoryNames[side][1], pList[0].iScore );
		}
		else
			event->SetInt( pCategoryNames[side][0], 0 );

		if ( iNumInList > 1 )
		{
			event->SetInt( pCategoryNames[side][2], pList[1].iPlayerIndex );
			event->SetInt( pCategoryNames[side][3], pList[1].iScore );
		}
		else
			event->SetInt( pCategoryNames[side][2], 0 );

		if ( iNumInList > 2 )
		{
			event->SetInt( pCategoryNames[side][4], pList[2].iPlayerIndex );
			event->SetInt( pCategoryNames[side][5], pList[2].iScore );
		}
		else
			event->SetInt( pCategoryNames[side][4], 0 );
	}

	//Input for other entities to declare a round winner.
	//Most often a dod_control_point_master saying that the
	//round timer expired or that someone capped all the flags
	void CDODGameRules::SetWinningTeam( int team )
	{
		if ( team != TEAM_ALLIES && team != TEAM_AXIS )
		{
			Assert( !"bad winning team set" );
			return;
		}

		PlayWinSong(team);

		GetGlobalDODTeam( team )->IncrementRoundsWon();

		switch(team)
		{
		case TEAM_ALLIES:
			{
				State_Transition( STATE_ALLIES_WIN );
			}
			break;
		case TEAM_AXIS:
			{
				State_Transition( STATE_AXIS_WIN );
			}
			break;
		default:
			break;
		}		

		IGameEvent *event = gameeventmanager->CreateEvent( "dod_round_win" );
		if ( event )
		{
			event->SetInt( "team", team );
			gameeventmanager->FireEvent( event );
		}

		// if this was in colmar, and the losing team did not cap any points,
		// the winners may have gotten an achievement
		if ( FStrEq( STRING(gpGlobals->mapname), "dod_colmar" ) )
		{
			CControlPointMaster *pMaster = dynamic_cast<CControlPointMaster *>( gEntList.FindEntityByClassname( NULL, "dod_control_point_master" ) );

			if ( pMaster )
			{
				// 1. losing team must not own any control points
				// 2. for each point that the winning team owns, that takes bombs, it must still have 2 bombs required
				bool bFlawlessVictory = true;

				int iNumCP = pMaster->GetNumPoints();

				for ( int i=0;i<iNumCP;i++ )
				{
					CControlPoint *pPoint = pMaster->GetCPByIndex(i);

					if ( !pPoint || !pPoint->PointIsVisible() )
						continue;

					// if the enemy owns any visible points, not a flawless victory
					if ( pPoint->GetOwner() != team )
					{
						bFlawlessVictory = false;
					}

					// 0 bombs remaining means we blew it up and now own it.
					// 1 bomb remaining means we own it, but the other team blew it up a bit.
					else if ( pPoint->GetBombsRequired() > 0 && pPoint->GetBombsRemaining() == 1 )	
					{																
						bFlawlessVictory = false;				
					}		
				}

				if ( bFlawlessVictory )
				{
					GetGlobalDODTeam( team )->AwardAchievement( ACHIEVEMENT_DOD_COLMAR_DEFENSE );
				}
			}
		}

		// send team scores
		SendTeamScoresEvent();

		IGameEvent *winEvent = gameeventmanager->CreateEvent( "dod_win_panel" );
		
		if ( winEvent )
		{
			// determine what categories to send

			if ( m_bUsingTimer )
			{
				if ( team == m_iTimerWinTeam )
				{
					// timer expired, defenders win
					// show total time that was defended
					winEvent->SetBool( "show_timer_defend", true );
					winEvent->SetInt( "timer_time", m_pRoundTimer->GetTimerMaxLength() );
				}
				else
				{
					// attackers win
					// show time it took for them to win
					winEvent->SetBool( "show_timer_attack", true );

					int iTimeElapsed = m_pRoundTimer->GetTimerMaxLength() - (int)m_pRoundTimer->GetTimeRemaining();
					winEvent->SetInt( "timer_time", iTimeElapsed );
				}
			}
			else
			{
				winEvent->SetBool( "show_timer_attack", false );
				winEvent->SetBool( "show_timer_defend", false );
			}

			int iLastEvent = ( team == TEAM_ALLIES ) ? m_iLastAlliesCapEvent : m_iLastAxisCapEvent;

			winEvent->SetInt( "final_event", iLastEvent );

			int i;
			int index;

			CUtlVector<playerscore_t> m_TopCappers;
			CUtlVector<playerscore_t> m_TopDefenders;
			CUtlVector<playerscore_t> m_TopBombers;
			CUtlVector<playerscore_t> m_TopKills;

			CDODTeam *pWinningTeam = GetGlobalDODTeam( team );

			int iNumPlayers = pWinningTeam->GetNumPlayers();

			for ( i=0;i<iNumPlayers;i++ )
			{
				CDODPlayer *pPlayer = dynamic_cast<CDODPlayer *>( pWinningTeam->GetPlayer(i) );

				if ( pPlayer )
				{
					int iCaps = pPlayer->GetPerRoundCaps();
					if ( iCaps )
					{
						index = m_TopCappers.AddToTail();
						m_TopCappers[index].iPlayerIndex = pPlayer->entindex();
						m_TopCappers[index].iScore = iCaps;
					}

					int iDefenses = pPlayer->GetPerRoundDefenses();
					if ( iDefenses )
					{
						index = m_TopDefenders.AddToTail();
						m_TopDefenders[index].iPlayerIndex = pPlayer->entindex();
						m_TopDefenders[index].iScore = iDefenses;
					}

					// bombs
					int iBombsDetonated = pPlayer->GetPerRoundBombsDetonated();
					if ( iBombsDetonated )
					{
						index = m_TopBombers.AddToTail();
						m_TopBombers[index].iPlayerIndex = pPlayer->entindex();
						m_TopBombers[index].iScore = iBombsDetonated;
					}

					// kills
					int iKills = pPlayer->GetPerRoundKills();
					if ( iKills )
					{
						index = m_TopKills.AddToTail();
						m_TopKills[index].iPlayerIndex = pPlayer->entindex();
						m_TopKills[index].iScore = iKills;
					}

					pPlayer->StatEvent_RoundWin();
				}				
			}

			CDODTeam *pLosingTeam = GetGlobalDODTeam( ( team == TEAM_ALLIES ) ? TEAM_AXIS : TEAM_ALLIES );

			iNumPlayers = pLosingTeam->GetNumPlayers();

			for ( i=0;i<iNumPlayers;i++ )
			{
				CDODPlayer *pPlayer = dynamic_cast<CDODPlayer *>( pLosingTeam->GetPlayer(i) );

				if ( pPlayer )
				{
					pPlayer->StatEvent_RoundLoss();
				}
			}

			m_TopCappers.Sort( PlayerScoreInfoSort );
			m_TopDefenders.Sort( PlayerScoreInfoSort );
			m_TopBombers.Sort( PlayerScoreInfoSort );
			m_TopKills.Sort( PlayerScoreInfoSort );

			// Decide what two categories to show in the winpanel
			// based on the gametype and which event have good information 
			// to show

			int iCategoryPriority[8];
			int pos = 0;

			// Default is to show two blank sides
			iCategoryPriority[pos++] = WINPANEL_TOP3_NONE;
			iCategoryPriority[pos++] = WINPANEL_TOP3_NONE;

			// Only show a category if it has information in it
			if ( m_TopKills.Count() > 0 )
			{
				iCategoryPriority[pos++] = WINPANEL_TOP3_KILLERS;
			}

			if ( m_TopDefenders.Count() > 0 )
			{
				iCategoryPriority[pos++] = WINPANEL_TOP3_DEFENDERS;
			}

			if ( m_TopBombers.Count() > 0 )
			{
				iCategoryPriority[pos++] = WINPANEL_TOP3_BOMBERS;
			}
			else if ( m_TopCappers.Count() > 0 )
			{
				iCategoryPriority[pos++] = WINPANEL_TOP3_CAPPERS;
			}

			// Get the two most interesting
			int iLeftCategory = iCategoryPriority[pos-1];
			int iRightCategory = iCategoryPriority[pos-2];

			switch( iLeftCategory )
			{
				case WINPANEL_TOP3_BOMBERS:
					FillEventCategory( winEvent, 0, iLeftCategory, m_TopBombers );
					break;
				case WINPANEL_TOP3_CAPPERS:
					FillEventCategory( winEvent, 0, iLeftCategory, m_TopCappers );
					break;
				case WINPANEL_TOP3_DEFENDERS:
					FillEventCategory( winEvent, 0, iLeftCategory, m_TopDefenders );
					break;
				case WINPANEL_TOP3_KILLERS:
					FillEventCategory( winEvent, 0, iLeftCategory, m_TopKills );
					break;
				case WINPANEL_TOP3_NONE:
				default:
					break;				
			}

			switch( iRightCategory )
			{
			case WINPANEL_TOP3_BOMBERS:
				FillEventCategory( winEvent, 1, iRightCategory, m_TopBombers );
				break;
			case WINPANEL_TOP3_CAPPERS:
				FillEventCategory( winEvent, 1, iRightCategory, m_TopCappers );
				break;
			case WINPANEL_TOP3_DEFENDERS:
				FillEventCategory( winEvent, 1, iRightCategory, m_TopDefenders );
				break;
			case WINPANEL_TOP3_KILLERS:
				FillEventCategory( winEvent, 1, iRightCategory, m_TopKills );
				break;
			case WINPANEL_TOP3_NONE:
			default:
				break;				
			}

			gameeventmanager->FireEvent( winEvent );
		}
	}

	void TestWinpanel( void )
	{
		
		IGameEvent *event = gameeventmanager->CreateEvent( "dod_round_win" );
		event->SetInt( "team", TEAM_ALLIES );

		if ( event )
		{
			gameeventmanager->FireEvent( event );
		}

		IGameEvent *event2 = gameeventmanager->CreateEvent( "dod_point_captured" );
		if ( event2 )
		{
			char cappers[9];	// pCappingPlayers is max length 8
			int i;
			for( i=0;i<1;i++ )
			{
				cappers[i] = (char)1;
			}

			cappers[i] = '\0';

			// pCappingPlayers is a null terminated list of player indeces
			event2->SetString( "cappers", cappers );
			event2->SetBool( "bomb", true );

			gameeventmanager->FireEvent( event2 );
		}

		IGameEvent *winEvent = gameeventmanager->CreateEvent( "dod_win_panel" );

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

			int iLastEvent = CAP_EVENT_FLAG;

			winEvent->SetInt( "final_event", iLastEvent );

			CUtlVector<playerscore_t> m_TopKillers;
			CUtlVector<playerscore_t> m_TopDefenders;
			CUtlVector<playerscore_t> m_TopCappers;
			CUtlVector<playerscore_t> m_TopBombers;

			CDODPlayer *pPlayer = ToDODPlayer( UTIL_PlayerByIndex(1) );

			if ( !pPlayer )
				return;

			int i = 0;
			int index;
			for ( i=0;i<3;i++ )
			{
				index = m_TopCappers.AddToTail();
				m_TopCappers[index].iPlayerIndex = pPlayer->entindex();
				m_TopCappers[index].iScore = pPlayer->GetPerRoundCaps() + 1;

				index = m_TopDefenders.AddToTail();
				m_TopDefenders[index].iPlayerIndex = pPlayer->entindex();
				m_TopDefenders[index].iScore = pPlayer->GetPerRoundDefenses() + 1;

				index = m_TopBombers.AddToTail();
				m_TopBombers[index].iPlayerIndex = pPlayer->entindex();
				m_TopBombers[index].iScore = pPlayer->GetPerRoundBombsDetonated() + 1;
						
				index = m_TopKillers.AddToTail();
				m_TopKillers[index].iPlayerIndex = pPlayer->entindex();
				m_TopKillers[index].iScore = pPlayer->GetPerRoundKills() + 1;
			}		

			m_TopCappers.Sort( PlayerScoreInfoSort );
			m_TopDefenders.Sort( PlayerScoreInfoSort );

			//FillEventCategory( winEvent, 0, WINPANEL_TOP3_KILLERS, m_TopKillers );
			//FillEventCategory( winEvent, 1, WINPANEL_TOP3_DEFENDERS, m_TopDefenders );
			FillEventCategory( winEvent, 0, WINPANEL_TOP3_BOMBERS, m_TopBombers );
			FillEventCategory( winEvent, 1, WINPANEL_TOP3_CAPPERS, m_TopCappers );

			gameeventmanager->FireEvent( winEvent );
		}
	}
	ConCommand dod_test_winpanel( "dod_test_winpanel", TestWinpanel, "", FCVAR_CHEAT );

	// bForceRespawn - respawn player even if dead or dying
	// bTeam - if true, only respawn the passed team
	// iTeam  - team to respawn
	void CDODGameRules::RespawnPlayers( bool bForceRespawn, bool bTeam /* = false */, int iTeam/* = TEAM_UNASSIGNED */ )
	{
		if ( bTeam )
		{
			if ( iTeam == TEAM_ALLIES )
				DevMsg( 2, "Respawning Allies\n" );
			else if ( iTeam == TEAM_AXIS )
				DevMsg( 2, "Respawning Axis\n" );
			else
				Assert(!"Trying to respawn a strange team");
		}	

		CDODPlayer *pPlayer;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pPlayer = ToDODPlayer( UTIL_PlayerByIndex( i ) );

			if ( !pPlayer )
				continue;

			// Check for team specific spawn
			if ( bTeam && pPlayer->GetTeamNumber() != iTeam )
				continue;

			// players that haven't chosen a class can never spawn
			if( pPlayer->m_Shared.DesiredPlayerClass() == PLAYERCLASS_UNDEFINED )
			{
				ClientPrint(pPlayer, HUD_PRINTTALK, "#game_will_spawn");
				continue;
			}

			// If we aren't force respawning, don't respawn players that:
			// - are alive
			// - are still in the death anim stage of dying
			if ( !bForceRespawn )
			{
				if ( pPlayer->IsAlive() )
					continue;

				if ( gpGlobals->curtime < ( pPlayer->GetDeathTime() + DEATH_CAM_TIME ) )
					continue;

				if ( pPlayer->GetObserverMode() == OBS_MODE_FREEZECAM )
					continue;

				if ( State_Get() != STATE_PREROUND && pPlayer->State_Get() == STATE_DEATH_ANIM )
					continue;
			}

			// Respawn this player
			pPlayer->DODRespawn();
		}
	}
	
	bool CDODGameRules::IsPlayerClassOnTeam( int cls, int team )
	{
		if( cls == PLAYERCLASS_RANDOM )
			return true;

		CDODTeam *pTeam = GetGlobalDODTeam( team );

		return ( cls >= 0 && cls < pTeam->GetNumPlayerClasses() );
	}

	bool CDODGameRules::CanPlayerJoinClass( CDODPlayer *pPlayer, int cls )
	{
		if( cls == PLAYERCLASS_RANDOM )
		{
			return mp_allowrandomclass.GetBool();
		}

		if( ReachedClassLimit( pPlayer->GetTeamNumber(), cls ) )
			return false;

		return true;
	}

	bool CDODGameRules::ReachedClassLimit( int team, int cls )
	{
		Assert( cls != PLAYERCLASS_UNDEFINED );
		Assert( cls != PLAYERCLASS_RANDOM );

		// get the cvar
		int iClassLimit = GetClassLimit( team, cls );

		// count how many are active
		int iClassExisting = CountPlayerClass( team, cls );

		CDODTeam *pTeam = GetGlobalDODTeam( team );
		const CDODPlayerClassInfo &pThisClassInfo = pTeam->GetPlayerClassInfo( cls );

		if( mp_combinemglimits.GetBool() && pThisClassInfo.m_bClassLimitMGMerge )
		{
			// find the other classes that have "mergemgclasses"

			for( int i=0; i<pTeam->GetNumPlayerClasses();i++ )
			{
				if( i != cls )
				{
					const CDODPlayerClassInfo &pClassInfo = pTeam->GetPlayerClassInfo( i );
					if( pClassInfo.m_bClassLimitMGMerge )
					{
						// add that class' limits and counts
						iClassLimit += GetClassLimit( team, i );
						iClassExisting += CountPlayerClass( team, i );
					}
				}
			}			
		}

		if( iClassLimit > -1 && iClassExisting >= iClassLimit )
		{
			return true;
		}

		return false;
	}

	int CDODGameRules::CountPlayerClass( int team, int cls )
	{
		int num = 0;
		CDODPlayer *pDODPlayer;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pDODPlayer = ToDODPlayer( UTIL_PlayerByIndex( i ) );

			if (pDODPlayer == NULL)
				continue;

			if (FNullEnt( pDODPlayer->edict() ))
				continue;

			if( pDODPlayer->GetTeamNumber() != team )
				continue;

			if( pDODPlayer->m_Shared.DesiredPlayerClass() == cls )
				num++;
		}

		return num;
	}

	int CDODGameRules::GetClassLimit( int team, int cls )
	{
		CDODTeam *pTeam = GetGlobalDODTeam( team );

		Assert( pTeam );

		const CDODPlayerClassInfo &pClassInfo = pTeam->GetPlayerClassInfo( cls );

		int iClassLimit;

		ConVar *pLimitCvar = ( ConVar * )cvar->FindVar( pClassInfo.m_szLimitCvar );

		Assert( pLimitCvar );

		if( pLimitCvar )
			iClassLimit = pLimitCvar->GetInt();
		else
			iClassLimit = -1;

		return iClassLimit;
	}

	void CDODGameRules::CheckLevelInitialized()
	{
		if ( !m_bLevelInitialized )
		{
			// Count the number of spawn points for each team
			// This determines the maximum number of players allowed on each

			CBaseEntity* ent = NULL;
			
			m_iSpawnPointCount_Allies	= 0;
			m_iSpawnPointCount_Axis		= 0;

			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_allies" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) )
				{
					m_iSpawnPointCount_Allies++;

					// store in a list
					m_AlliesSpawnPoints.AddToTail( ent );
				}
				else
				{
					Warning("Invalid allies spawnpoint at (%.1f,%.1f,%.1f)\n",
						ent->GetAbsOrigin()[0],ent->GetAbsOrigin()[2],ent->GetAbsOrigin()[2] );
				}
			}

			while ( ( ent = gEntList.FindEntityByClassname( ent, "info_player_axis" ) ) != NULL )
			{
				if ( IsSpawnPointValid( ent, NULL ) ) 
				{
					m_iSpawnPointCount_Axis++;

					// store in a list
					m_AxisSpawnPoints.AddToTail( ent );
				}
				else
				{
					Warning("Invalid axis spawnpoint at (%.1f,%.1f,%.1f)\n",
						ent->GetAbsOrigin()[0],ent->GetAbsOrigin()[2],ent->GetAbsOrigin()[2] );
				}
			}

			m_bLevelInitialized = true;
		}
	}

	CUtlVector<EHANDLE> *CDODGameRules::GetSpawnPointListForTeam( int iTeam )
	{
		switch ( iTeam )
		{
		case TEAM_ALLIES:
			return &m_AlliesSpawnPoints;
		case TEAM_AXIS:
			return &m_AxisSpawnPoints;
		default:
			break;
		}

		return NULL;
	}

	/* create some proxy entities that we use for transmitting data */
	void CDODGameRules::CreateStandardEntities()
	{
		// Create the player resource
		g_pPlayerResource = (CPlayerResource*)CBaseEntity::Create( "dod_player_manager", vec3_origin, vec3_angle );

		// Create the objective resource
		g_pObjectiveResource = (CDODObjectiveResource *)CBaseEntity::Create( "dod_objective_resource", vec3_origin, vec3_angle );
	
		Assert( g_pObjectiveResource );

		// Create the entity that will send our data to the client.
#ifdef DBGFLAG_ASSERT
		CBaseEntity *pEnt = 
#endif
			CBaseEntity::Create( "dod_gamerules", vec3_origin, vec3_angle );
		Assert( pEnt );
	}

	ConVar dod_waverespawnfactor( "dod_waverespawnfactor", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT, "Factor for respawn wave timers" );
	
	float CDODGameRules::GetWaveTime( int iTeam )
	{
		float flRespawnTime = 0.0f;

		switch( iTeam )
		{
		case TEAM_ALLIES:
			flRespawnTime = ( m_iNumAlliesRespawnWaves > 0 ) ? m_AlliesRespawnQueue[m_iAlliesRespawnHead] : -1;
			break;
		case TEAM_AXIS:
			flRespawnTime = ( m_iNumAxisRespawnWaves > 0 ) ? m_AxisRespawnQueue[m_iAxisRespawnHead] : -1;
			break;
		default:
			Assert( !"Why are you trying to get the wave time for a non-team?" );
			break;
		}

		return flRespawnTime;
	}

	float CDODGameRules::GetMaxWaveTime( int nTeam )
	{
		float fTime = 0;

		// Quick waves to get everyone in if we are in PREROUND
		if ( State_Get() == STATE_PREROUND )
		{
			return 1.0;
		}

		int nNumPlayers = GetGlobalDODTeam( nTeam )->GetNumPlayers();

		if( nNumPlayers < 3 )
			fTime = 6.f;
		else if( nNumPlayers < 6 )
			fTime = 8.f;
		else if( nNumPlayers < 8 )
			fTime = 10.f;
		else if( nNumPlayers < 10 )
			fTime = 11.f;
		else if( nNumPlayers < 12 )
			fTime = 12.f;
		else if( nNumPlayers < 14 )
			fTime = 13.f;
		else
			fTime = 14.f;

		//adjust wave time based on mapper settings
		//they can adjust the factor ( default 1.0 ) 
		// to give longer or shorter wait times for 
		// either team
		if( nTeam == TEAM_ALLIES )
			fTime *= m_GamePlayRules.m_fAlliesRespawnFactor;
		else if( nTeam == TEAM_AXIS )
			fTime *= m_GamePlayRules.m_fAxisRespawnFactor;

		// Finally, adjust the respawn time based on how well the team is doing
		// a team with more flags should respawn faster.
		// Give a bonus to respawn time for each flag that we own that we 
		// don't own by default.

		CControlPointMaster *pMaster =	dynamic_cast<CControlPointMaster*>( gEntList.FindEntityByClassname( NULL, "dod_control_point_master" ) );

		if( pMaster )
		{
			int advantageFlags = pMaster->CountAdvantageFlags( nTeam );

			// this can be negative if we are losing, this will add time!

			fTime -= (float)(advantageFlags) * dod_flagrespawnbonus.GetFloat();
		}		

		fTime *= dod_waverespawnfactor.GetFloat();

		// Minimum 5 seconds
		if (fTime <= DEATH_CAM_TIME)
			fTime = DEATH_CAM_TIME;

		// Maximum 20 seconds
		if ( fTime > MAX_WAVE_RESPAWN_TIME )
		{
			fTime = MAX_WAVE_RESPAWN_TIME;
		}

		return fTime;
	}


	void CDODGameRules::CreateOrJoinRespawnWave( CDODPlayer *pPlayer )
	{
		int team = pPlayer->GetTeamNumber();
		float flWaveTime = GetWaveTime( team ) - gpGlobals->curtime;

		if( flWaveTime <= 0 )
		{
			// start a new wave

			DevMsg( "Wave: Starting a new wave for team %d, time %.1f\n", team, GetMaxWaveTime(team) );

			//start a new wave with this player
			AddWaveTime( team, GetMaxWaveTime(team) );
		}
		else
		{
			// see if this player needs to start a new wave

			int team = pPlayer->GetTeamNumber();
			float flSpawnEligibleTime = gpGlobals->curtime + DEATH_CAM_TIME;

			if ( team == TEAM_ALLIES )
			{
				bool bFoundWave = false;

				int i = m_iAlliesRespawnHead;

				while( i != m_iAlliesRespawnTail )
				{
					// if the player can fit in this wave, set bFound = true
					if ( flSpawnEligibleTime < m_AlliesRespawnQueue[i] )
					{
						bFoundWave = true;
						break;
					}

					i = ( i+1 ) % DOD_RESPAWN_QUEUE_SIZE;
				}

				if ( !bFoundWave )
				{
					// add a new wave to the end
					AddWaveTime( TEAM_ALLIES, GetMaxWaveTime(TEAM_ALLIES) );
				}
			}
			else if ( team == TEAM_AXIS )
			{
				bool bFoundWave = false;

				int i = m_iAxisRespawnHead;

				while( i != m_iAxisRespawnTail )
				{
					// if the player can fit in this wave, set bFound = true
					if ( flSpawnEligibleTime < m_AxisRespawnQueue[i] )
					{
						bFoundWave = true;
						break;
					}

					i = ( i+1 ) % DOD_RESPAWN_QUEUE_SIZE;
				}

				if ( !bFoundWave )
				{
					// add a new wave to the end
					AddWaveTime( TEAM_AXIS, GetMaxWaveTime(TEAM_AXIS) );
				}
			}
			else
				Assert( 0 );
		}
	}

	bool CDODGameRules::InRoundRestart( void )
	{
		if ( State_Get() == STATE_PREROUND )
			return true;

		return false;
	}

	void CDODGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		CDODPlayer *pDODVictim = ToDODPlayer( pVictim );

		// if you're still playing dod, you know how this works, let's not
		// interfere with the freezecam panel
		//bool bPlayed = pDODVictim->HintMessage( HINT_PLAYER_KILLED_WAVETIME );

		// If we already played the killed hint, play the deathcam hint
		//if ( !bPlayed )
		//{
		//	pDODVictim->HintMessage( HINT_DEATHCAM );
		//}

		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CDODPlayer *pScorer = ToDODPlayer( GetDeathScorer( pKiller, pInflictor ) );

		if( pScorer && pScorer->IsPlayer() && pScorer != pVictim )
		{
			if( pVictim->GetTeamNumber() == pScorer->GetTeamNumber() )
			{
				pScorer->HintMessage( HINT_FRIEND_KILLED, true );	//force this
			}
			else
			{
				pScorer->HintMessage( HINT_ENEMY_KILLED );
			}
		}

		// determine if this kill affected a nemesis relationship
		int iDeathFlags = 0;
		if ( pScorer )
		{	
			CalcDominationAndRevenge( pScorer, pDODVictim, &iDeathFlags );
		}

		pDODVictim->SetDeathFlags( iDeathFlags );	// for deathnotice I assume?

		DeathNotice( pVictim, info );

		// dvsents2: uncomment when removing all FireTargets
		// variant_t value;
		// g_EventQueue.AddEvent( "game_playerdie", "Use", value, 0, pVictim, pVictim );
		FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );

		bool bScoring = !IsInWarmup();

		if( bScoring )
		{
			pVictim->IncrementDeathCount( 1 );
		}
	
		// Did the player kill himself?
		if ( pVictim == pScorer )  
		{
			// Players lose a frag for killing themselves
			//if( bScoring )
			//	pVictim->IncrementFragCount( -1 );
		}
		else if ( pScorer )
		{
			// if a player dies in a deathmatch game and the killer is a client, award the killer some points
			if( bScoring )
				pScorer->IncrementFragCount( DODPointsForKill( pVictim, info ) );

			// Allow the scorer to immediately paint a decal
			pScorer->AllowImmediateDecalPainting();

			// dvsents2: uncomment when removing all FireTargets
			//variant_t value;
			//g_EventQueue.AddEvent( "game_playerkill", "Use", value, 0, pScorer, pScorer );
			FireTargets( "game_playerkill", pScorer, pScorer, USE_TOGGLE, 0 );

			// see if this saved a capture
			if ( pDODVictim->m_signals.GetState() & SIGNAL_CAPTUREAREA )
			{
				//find the area the player is in and see if his death causes a block
				CAreaCapture *pArea = dynamic_cast<CAreaCapture *>(gEntList.FindEntityByClassname( NULL, "dod_capture_area" ) );
				while( pArea )
				{
					if ( pArea->CheckIfDeathCausesBlock( pDODVictim, pScorer ) )
					{
						break;
					}

					pArea = dynamic_cast<CAreaCapture *>( gEntList.FindEntityByClassname( pArea, "dod_capture_area" ) );
				}
			}
			if ( pDODVictim->m_bIsDefusing && pDODVictim->m_pDefuseTarget && pScorer->GetTeamNumber() != pDODVictim->GetTeamNumber() )
			{
				CDODBombTarget *pTarget = pDODVictim->m_pDefuseTarget;

				pTarget->DefuseBlocked( pScorer );

				IGameEvent *event = gameeventmanager->CreateEvent( "dod_kill_defuser" );
				if ( event )
				{
					event->SetInt( "userid", pScorer->GetUserID() );
					event->SetInt( "victimid", pDODVictim->GetUserID() );

					gameeventmanager->FireEvent( event );
				}
			}
		}
		else
		{  
			// Players lose a frag for letting the world kill them
			//if( bScoring )
			//	pVictim->IncrementFragCount( -1 );
		}
	}

	void CDODGameRules::DetectGameRules( void )
	{
		bool bFound = false;

		CBaseEntity *pEnt = NULL;
		
		pEnt = gEntList.FindEntityByClassname( pEnt, "info_doddetect" );

		while( pEnt )
		{
			CDODDetect *pDetect = dynamic_cast<CDODDetect *>(pEnt);

			if( pDetect && pDetect->IsMasteredOn() )
			{
				CDODGamePlayRules *pRules = pDetect->GetGamePlay();
				Assert( pRules );
				CopyGamePlayLogic( *pRules );
				bFound = true;
				break;
			}

			pEnt = gEntList.FindEntityByClassname( pEnt, "info_doddetect" );
		}

		if( !bFound )
		{
			m_GamePlayRules.Reset();
		}
	}

	void CDODGameRules::PlayWinSong( int team )
	{
		switch(team)
		{
		case TEAM_ALLIES:	
			BroadcastSound( "Game.USWin" );
			break;
		case TEAM_AXIS:
			BroadcastSound( "Game.GermanWin" );
			break;
		default:
			Assert(0);
			break;
		}
	}

	void CDODGameRules::BroadcastSound( const char *sound )
	{
		//send it to everyone
		IGameEvent *event = gameeventmanager->CreateEvent( "dod_broadcast_audio" );
		if ( event )
		{
			event->SetString( "sound", sound );
			gameeventmanager->FireEvent( event );
		}
	}

	void CDODGameRules::PlayStartRoundVoice( void )
	{
		// One for the Allies..
		switch( m_GamePlayRules.m_iAlliesStartRoundVoice )
		{
		case STARTROUND_ATTACK:			
			PlaySpawnSoundToTeam( "Voice.US_ObjectivesAttack", TEAM_ALLIES );
			break;

		case STARTROUND_DEFEND:			
			PlaySpawnSoundToTeam( "Voice.US_ObjectivesDefend", TEAM_ALLIES );
			break;

		case STARTROUND_BEACH:			
			PlaySpawnSoundToTeam( "Voice.US_Beach", TEAM_ALLIES );
			break;

		case STARTROUND_ATTACK_TIMED:			
			PlaySpawnSoundToTeam( "Voice.US_ObjectivesAttackTimed", TEAM_ALLIES );
			break;

		case STARTROUND_DEFEND_TIMED:			
			PlaySpawnSoundToTeam( "Voice.US_ObjectivesDefendTimed", TEAM_ALLIES );
			break;

		case STARTROUND_FLAGS:
		default:
			PlaySpawnSoundToTeam( "Voice.US_Flags", TEAM_ALLIES );
			break;
		}

		// and one for the Axis
		switch( m_GamePlayRules.m_iAxisStartRoundVoice )
		{
		case STARTROUND_ATTACK:			
			PlaySpawnSoundToTeam( "Voice.German_ObjectivesAttack", TEAM_AXIS );
			break;

		case STARTROUND_DEFEND:			
			PlaySpawnSoundToTeam( "Voice.German_ObjectivesDefend", TEAM_AXIS );
			break;

		case STARTROUND_BEACH:			
			PlaySpawnSoundToTeam( "Voice.German_Beach", TEAM_AXIS );
			break;

		case STARTROUND_ATTACK_TIMED:			
			PlaySpawnSoundToTeam( "Voice.German_ObjectivesAttackTimed", TEAM_AXIS );
			break;

		case STARTROUND_DEFEND_TIMED:			
			PlaySpawnSoundToTeam( "Voice.German_ObjectivesDefendTimed", TEAM_AXIS );
			break;

		case STARTROUND_FLAGS:
		default:
			PlaySpawnSoundToTeam( "Voice.German_Flags", TEAM_AXIS );
			break;
		}
	}

	void CDODGameRules::PlaySpawnSoundToTeam( const char *sound, int team )
	{
		// find the first valid player and make them do it as a voice command
		CDODPlayer *pPlayer;

		static int iLastSpeaker = 1;

		int iCurrent = iLastSpeaker;

		bool bBreakLoop = false;

		while( !bBreakLoop )
		{
			iCurrent++;
			if( iCurrent > gpGlobals->maxClients )
				iCurrent = 1;

			if( iCurrent == iLastSpeaker )
			{
				// couldn't find a different player. check the same player again
				// and then break regardless
				bBreakLoop = true;
			}

			pPlayer = ToDODPlayer( UTIL_PlayerByIndex( iCurrent ) );

			if (pPlayer == NULL)
				continue;

			if (FNullEnt( pPlayer->edict() ))
				continue;

			if( pPlayer && pPlayer->GetTeamNumber() == team && pPlayer->IsAlive() )
			{
				CPASFilter filter( pPlayer->WorldSpaceCenter() );
				pPlayer->EmitSound( filter, pPlayer->entindex(), sound );

				pPlayer->DoAnimationEvent( PLAYERANIMEVENT_HANDSIGNAL );

				iLastSpeaker = iCurrent;
				break;
			}
		}
	}	

	void CDODGameRules::ClientDisconnected( edict_t *pClient )
	{
		CDODPlayer *pPlayer = ToDODPlayer( GetContainingEntity( pClient ) );

		if( pPlayer )
		{
			pPlayer->DestroyRagdoll();

			pPlayer->StatEvent_UploadStats();
		}

		// Tally the latest time for this player
		pPlayer->TallyLatestTimePlayedPerClass( pPlayer->GetTeamNumber(), pPlayer->m_Shared.DesiredPlayerClass() );

		pPlayer->RemoveNemesisRelationships();

		for( int j=0;j<7;j++ )
		{
			m_flSecondsPlayedPerClass_Allies[j] += pPlayer->m_flTimePlayedPerClass_Allies[j];
			m_flSecondsPlayedPerClass_Axis[j] += pPlayer->m_flTimePlayedPerClass_Axis[j];
		}

		int iPlayerIndex = pPlayer->entindex();
		Assert( iPlayerIndex >= 1 && iPlayerIndex <= MAX_PLAYERS);
		if ( iPlayerIndex >= 1 && iPlayerIndex <= MAX_PLAYERS )
		{
			// for every other player, set all all the kills with respect to this player to 0
			for ( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				CDODPlayer *p = ToDODPlayer( UTIL_PlayerByIndex(i) );
				if ( !p )
					continue;

				p->iNumKilledByUnanswered[iPlayerIndex] = 0;
			}
		}

		BaseClass::ClientDisconnected( pClient );
	}

	void CDODGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		// Work out what killed the player, and send a message to all clients about it
		const char *killer_weapon_name = "world";		// by default, the player is killed by the world
		int killer_ID = 0;

		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CDODPlayer *pScorer = ToDODPlayer( GetDeathScorer( pKiller, pInflictor ) );
		CDODPlayer *pDODVictim = ToDODPlayer( pVictim );

		Assert( pDODVictim );

		if ( pScorer )	// Is the killer a client?
		{
			killer_ID = pScorer->GetUserID();
		
			if ( pInflictor )
			{
				if ( pInflictor == pScorer )
				{
					CWeaponDODBase *pWeapon = pScorer->GetActiveDODWeapon();

					if ( pWeapon )
					{
						int iWeaponType = pWeapon->GetDODWpnData().m_WeaponType;

						// Putting this here because we already have the weapon pointer.
						if ( iWeaponType == WPN_TYPE_MG && pScorer->GetTeamNumber() != pVictim->GetTeamNumber() )
						{
							CDODBipodWeapon *pMG = dynamic_cast<CDODBipodWeapon *>( pWeapon );
							Assert( pMG );
							if ( pMG->IsDeployed() )
							{
								pScorer->HandleDeployedMGKillCount( 1 );
							}
						}

						// if the weapon does not belong to the same team
						if ( pWeapon->GetDODWpnData().m_iDefaultTeam != pScorer->GetTeamNumber() &&
								pScorer->GetTeamNumber() != pVictim->GetTeamNumber() )
						{
							pScorer->HandleEnemyWeaponsAchievement( 1 );
						}

						// achievement for getting kills with several different weapon types in one life
						if ( pScorer->GetTeamNumber() != pVictim->GetTeamNumber() )
						{
							pScorer->HandleComboWeaponKill( iWeaponType );
						}

						if( info.GetDamageCustom() & MELEE_DMG_SECONDARYATTACK )
						{
							//it was a butt or bayonet!
							killer_weapon_name = pWeapon->GetSecondaryDeathNoticeName();
						}
						// If the inflictor is the killer,  then it must be their current weapon doing the damage
						else
						{
							killer_weapon_name = pWeapon->GetClassname(); 
						}
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
		else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		{
			killer_weapon_name += 5;
		}
		else if ( strncmp( killer_weapon_name, "rocket_", 7 ) == 0 )
		{
			killer_weapon_name += 7;
		}
		else if ( strncmp( killer_weapon_name, "grenade_", 8 ) == 0 )
		{
			killer_weapon_name += 8;

			// achievement for getting kills with several different weapon types in one life
			if ( pScorer && pScorer->GetTeamNumber() != pVictim->GetTeamNumber() )
			{
				pScorer->HandleComboWeaponKill( WPN_TYPE_GRENADE );
			}
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "player_death" );

		if ( event )
		{
			event->SetInt("userid", pVictim->GetUserID() );
			event->SetInt("attacker", killer_ID );
			event->SetString("weapon", killer_weapon_name );
			event->SetInt("priority", 7 );

			if ( pDODVictim->GetDeathFlags() & DOD_DEATHFLAG_DOMINATION )
			{
				event->SetInt( "dominated", 1 );
			}
			if ( pDODVictim->GetDeathFlags() & DOD_DEATHFLAG_REVENGE )
			{
				event->SetInt( "revenge", 1 );
			}

			gameeventmanager->FireEvent( event );
		}
	}


	// CDODDetect - map entity for mappers to choose game rules
	LINK_ENTITY_TO_CLASS( info_doddetect, CDODDetect );

	CDODDetect::CDODDetect()
	{
		m_GamePlayRules.Reset();
	}

	void CDODDetect::Spawn( void )
	{
		SetSolid( SOLID_NONE );

		BaseClass::Spawn();	
	}

	bool CDODDetect::IsMasteredOn( void )
	{
		//For now return true
		return true;
	}

	bool CDODDetect::KeyValue( const char *szKeyName, const char *szValue )
	{
		if (FStrEq(szKeyName, "detect_allies_respawnfactor"))
		{
			m_GamePlayRules.m_fAlliesRespawnFactor = atof(szValue);
		}
		else if (FStrEq(szKeyName, "detect_axis_respawnfactor"))
		{
			m_GamePlayRules.m_fAxisRespawnFactor = atof(szValue);
		}
		else if (FStrEq(szKeyName, "detect_allies_startroundvoice"))
		{
			m_GamePlayRules.m_iAlliesStartRoundVoice = atoi(szValue);
		}
		else if (FStrEq(szKeyName, "detect_axis_startroundvoice"))
		{
			m_GamePlayRules.m_iAxisStartRoundVoice = atof(szValue);
		}
		else
			return CBaseEntity::KeyValue( szKeyName, szValue );
		
		return true;
	}

	//checks to see if the desired team is stacked, returns true if it is
	bool CDODGameRules::TeamStacked( int iNewTeam, int iCurTeam  )
	{
		//players are allowed to change to their own team
		if(iNewTeam == iCurTeam)
			return false;

		int iTeamLimit = mp_limitteams.GetInt();

		// Tabulate the number of players on each team.
		int iNumAllies = GetGlobalTeam( TEAM_ALLIES )->GetNumPlayers();
		int iNumAxis = GetGlobalTeam( TEAM_AXIS )->GetNumPlayers();

		switch ( iNewTeam )
		{
		case TEAM_ALLIES:
			if( iCurTeam != TEAM_UNASSIGNED && iCurTeam != TEAM_SPECTATOR )
			{
				if((iNumAllies + 1) > (iNumAxis + iTeamLimit - 1))
					return true;
				else
					return false;
			}
			else
			{
				if((iNumAllies + 1) > (iNumAxis + iTeamLimit))
					return true;
				else
					return false;
			}
			break;
		case TEAM_AXIS:
			if( iCurTeam != TEAM_UNASSIGNED && iCurTeam != TEAM_SPECTATOR )
			{
				if((iNumAxis + 1) > (iNumAllies + iTeamLimit - 1))
					return true;
				else
					return false;
			}
			else
			{
				if((iNumAxis + 1) > (iNumAllies + iTeamLimit))
					return true;
				else
					return false;
			}
			break;
		}

		return false;
	}

	// Falling damage stuff.
	#define DOD_PLAYER_FATAL_FALL_SPEED		900		// approx 60 feet
	#define DOD_PLAYER_MAX_SAFE_FALL_SPEED	500		// approx 20 feet
	#define DOD_DAMAGE_FOR_FALL_SPEED		((float)100 / ( DOD_PLAYER_FATAL_FALL_SPEED - DOD_PLAYER_MAX_SAFE_FALL_SPEED )) // damage per unit per second.

	/*
	#define PLAYER_FALL_PUNCH_THRESHHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.
	*/

	float CDODGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
	{
		pPlayer->m_Local.m_flFallVelocity -= DOD_PLAYER_MAX_SAFE_FALL_SPEED;
		return pPlayer->m_Local.m_flFallVelocity * DOD_DAMAGE_FOR_FALL_SPEED;
	} 

#endif

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

CAmmoDef* GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;

		//pistol ammo
		def.AddAmmoType( DOD_AMMO_COLT,		DMG_BULLET, TRACER_NONE,	0, 0, 21,	5000, 10, 14 );
		def.AddAmmoType( DOD_AMMO_P38,		DMG_BULLET, TRACER_NONE,	0, 0, 24,	5000, 10, 14 );
		def.AddAmmoType( DOD_AMMO_C96,		DMG_BULLET, TRACER_NONE,	0, 0, 60,	5000, 10, 14 );
		
		//rifles
		def.AddAmmoType( DOD_AMMO_GARAND,		DMG_BULLET, TRACER_NONE,	0, 0, 88,		9000, 10, 14 );
		def.AddAmmoType( DOD_AMMO_K98,			DMG_BULLET, TRACER_NONE,	0, 0, 65,		9000, 10, 14 );
		def.AddAmmoType( DOD_AMMO_M1CARBINE,	DMG_BULLET, TRACER_NONE,	0, 0, 165,		9000, 10, 14 );
		def.AddAmmoType( DOD_AMMO_SPRING,		DMG_BULLET, TRACER_NONE,	0, 0, 55,		9000, 10, 14 );

		//submg
		def.AddAmmoType( DOD_AMMO_SUBMG,		DMG_BULLET, TRACER_NONE,			0, 0, 210,		7000, 10, 14 );
		def.AddAmmoType( DOD_AMMO_BAR,			DMG_BULLET, TRACER_LINE_AND_WHIZ,	0, 0, 260,		9000, 10, 14 );

		//mg
		def.AddAmmoType( DOD_AMMO_30CAL,		DMG_BULLET | DMG_MACHINEGUN, TRACER_LINE_AND_WHIZ,	0, 0, 300,		9000, 10, 14 );
		def.AddAmmoType( DOD_AMMO_MG42,			DMG_BULLET | DMG_MACHINEGUN, TRACER_LINE_AND_WHIZ,	0, 0, 500,		9000, 10, 14 );

		//rockets
		def.AddAmmoType( DOD_AMMO_ROCKET,		DMG_BLAST,	TRACER_NONE,			0, 0, 5,	9000, 10, 14 );

		//grenades
		def.AddAmmoType( DOD_AMMO_HANDGRENADE,		DMG_BLAST,	TRACER_NONE,		0, 0, 2, 1, 4, 8 );
		def.AddAmmoType( DOD_AMMO_STICKGRENADE,		DMG_BLAST,	TRACER_NONE,		0, 0, 2, 1, 4, 8 );	
		def.AddAmmoType( DOD_AMMO_HANDGRENADE_EX,	DMG_BLAST,	TRACER_NONE,		0, 0, 1, 1, 4, 8 );
		def.AddAmmoType( DOD_AMMO_STICKGRENADE_EX,	DMG_BLAST,	TRACER_NONE,		0, 0, 1, 1, 4, 8 );

		// smoke grenades
		def.AddAmmoType( DOD_AMMO_SMOKEGRENADE_US,		DMG_BLAST,	TRACER_NONE,	0, 0, 2, 1, 4, 8 );
		def.AddAmmoType( DOD_AMMO_SMOKEGRENADE_GER,		DMG_BLAST,	TRACER_NONE,	0, 0, 2, 1, 4, 8 );
		def.AddAmmoType( DOD_AMMO_SMOKEGRENADE_US_LIVE,	DMG_BLAST,	TRACER_NONE,	0, 0, 2, 1, 4, 8 );
		def.AddAmmoType( DOD_AMMO_SMOKEGRENADE_GER_LIVE,DMG_BLAST,	TRACER_NONE,	0, 0, 2, 1, 4, 8 );

		// rifle grenades
		def.AddAmmoType( DOD_AMMO_RIFLEGRENADE_US,		DMG_BLAST,	TRACER_NONE,	0, 0, 2, 1, 4, 8 );
		def.AddAmmoType( DOD_AMMO_RIFLEGRENADE_GER,		DMG_BLAST,	TRACER_NONE,	0, 0, 2, 1, 4, 8 );
		def.AddAmmoType( DOD_AMMO_RIFLEGRENADE_US_LIVE,	DMG_BLAST,	TRACER_NONE,	0, 0, 2, 1, 4, 8 );
		def.AddAmmoType( DOD_AMMO_RIFLEGRENADE_GER_LIVE,DMG_BLAST,	TRACER_NONE,	0, 0, 2, 1, 4, 8 );
	}

	return &def;
}

#ifndef CLIENT_DLL
void CDODGameRules::AddWaveTime( int team, float flTime )
{
	switch ( team )
	{
	case TEAM_ALLIES:
		{
			Assert( m_iNumAlliesRespawnWaves < DOD_RESPAWN_QUEUE_SIZE );

			if ( m_iNumAlliesRespawnWaves >= DOD_RESPAWN_QUEUE_SIZE )
			{
				Warning( "Trying to add too many allies respawn waves\n" );
				return;
			}

			m_AlliesRespawnQueue.Set( m_iAlliesRespawnTail, gpGlobals->curtime + flTime );
			m_iNumAlliesRespawnWaves++;

			m_iAlliesRespawnTail = ( m_iAlliesRespawnTail + 1 ) % DOD_RESPAWN_QUEUE_SIZE;

			DevMsg( 1, "AddWaveTime ALLIES head %d tail %d numtotal %d time %.1f\n",
				m_iAlliesRespawnHead.Get(),
				m_iAlliesRespawnTail.Get(),
				m_iNumAlliesRespawnWaves,
				gpGlobals->curtime + flTime );
		}
		break;
	case TEAM_AXIS:
		{
			Assert( m_iNumAxisRespawnWaves < DOD_RESPAWN_QUEUE_SIZE );

			if ( m_iNumAxisRespawnWaves >= DOD_RESPAWN_QUEUE_SIZE )
			{
				Warning( "Trying to add too many axis respawn waves\n" );
				return;
			}

			m_AxisRespawnQueue.Set( m_iAxisRespawnTail, gpGlobals->curtime + flTime );
			m_iNumAxisRespawnWaves++;

			m_iAxisRespawnTail = ( m_iAxisRespawnTail + 1 ) % DOD_RESPAWN_QUEUE_SIZE;

			DevMsg( 1, "AddWaveTime AXIS head %d tail %d numtotal %d time %.1f\n",
				m_iAxisRespawnHead.Get(),
				m_iAxisRespawnTail.Get(),
				m_iNumAxisRespawnWaves,
				gpGlobals->curtime + flTime );
		}
		break;
	default:
		Assert(0);
		break;
	}
}

void CDODGameRules::PopWaveTime( int team )
{
	switch ( team )
	{
	case TEAM_ALLIES:
		{
			Assert( m_iNumAlliesRespawnWaves > 0 );

			m_iAlliesRespawnHead = ( m_iAlliesRespawnHead + 1 ) % DOD_RESPAWN_QUEUE_SIZE;
			m_iNumAlliesRespawnWaves--;

			DevMsg( 1, "PopWaveTime ALLIES head %d tail %d numtotal %d time %.1f\n",
				m_iAlliesRespawnHead.Get(),
				m_iAlliesRespawnTail.Get(),
				m_iNumAlliesRespawnWaves,
				gpGlobals->curtime );
		}
		break;
	case TEAM_AXIS:
		{
			Assert( m_iNumAxisRespawnWaves > 0 );

			m_iAxisRespawnHead = ( m_iAxisRespawnHead + 1 ) % DOD_RESPAWN_QUEUE_SIZE;
			m_iNumAxisRespawnWaves--;

			DevMsg( 1, "PopWaveTime AXIS head %d tail %d numtotal %d time %.1f\n",
				m_iAxisRespawnHead.Get(),
				m_iAxisRespawnTail.Get(),
				m_iNumAxisRespawnWaves,
				gpGlobals->curtime );
		}
		break;
	default:
		Assert(0);
		break;
	}
}

#endif


#ifndef CLIENT_DLL

const char *CDODGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	char *pszPrefix = "";

	if ( !pPlayer )  // dedicated server output
	{
		pszPrefix = "";
	}
	else
	{
		// don't show dead prefix if in the bonus round or at round end
		// because we can chat at these times.
		bool bShowDeadPrefix = ( pPlayer->IsAlive() == false ) && !IsInBonusRound() &&
			( State_Get() != STATE_GAME_OVER );

		if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		{
			return "";
		}

		if ( bTeamOnly )
		{
			if ( bShowDeadPrefix )
			{
				pszPrefix =	"(Dead)(Team)";	//#chatprefix_deadteam";
			}
			else
			{
				//MATTTODO: localize chat prefixes
				pszPrefix = "(Team)";	//"#chatprefix_team"; 
			}
		}
		// everyone
		else
		{
			if ( bShowDeadPrefix )
			{
				pszPrefix = "(Dead)";	//"#chatprefix_dead";
			}
		}
	}

	return pszPrefix;
}

void CDODGameRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
	CDODPlayer *pDODPlayer = ToDODPlayer( pPlayer );

	Assert( pDODPlayer );

	pDODPlayer->SetAutoReload( Q_atoi( engine->GetClientConVarValue( pPlayer->entindex(), "cl_autoreload" ) ) > 0 );
	pDODPlayer->SetShowHints( Q_atoi( engine->GetClientConVarValue( pPlayer->entindex(), "cl_showhelp" ) ) > 0 );
	pDODPlayer->SetAutoRezoom( Q_atoi( engine->GetClientConVarValue( pPlayer->entindex(), "cl_autorezoom" ) ) > 0 );

	BaseClass::ClientSettingsChanged( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: Determines if attacker and victim have gotten domination or revenge
//-----------------------------------------------------------------------------
void CDODGameRules::CalcDominationAndRevenge( CDODPlayer *pAttacker, CDODPlayer *pVictim, int *piDeathFlags )
{
	// team kills don't count
	if ( pAttacker->GetTeamNumber() == pVictim->GetTeamNumber() )
		return;

	int iKillsUnanswered = ++(pVictim->iNumKilledByUnanswered[pAttacker->entindex()]);

	pAttacker->iNumKilledByUnanswered[pVictim->entindex()] = 0;

	if ( DOD_KILLS_DOMINATION == iKillsUnanswered )
	{			
		// this is the Nth unanswered kill between killer and victim, killer is now dominating victim
		*piDeathFlags |= DOD_DEATHFLAG_DOMINATION;

		// set victim to be dominated by killer
		pAttacker->m_Shared.SetPlayerDominated( pVictim, true );

		pAttacker->StatEvent_ScoredDomination();
	}
	else if ( pVictim->m_Shared.IsPlayerDominated( pAttacker->entindex() ) )
	{
		// the killer killed someone who was dominating him, gains revenge
		*piDeathFlags |= DOD_DEATHFLAG_REVENGE;

		// set victim to no longer be dominating the killer
		pVictim->m_Shared.SetPlayerDominated( pAttacker, false );

		pAttacker->StatEvent_ScoredRevenge();
	}
}

int CDODGameRules::DODPointsForKill( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( IsInWarmup() )
		return 0;

	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CDODPlayer *pScorer = ToDODPlayer( GetDeathScorer( pKiller, pInflictor ) );

	// Don't give -1 points for killing a teammate with the bomb.
	// It was their fault for standing too close really.
	if ( pVictim->GetTeamNumber() == pScorer->GetTeamNumber() &&
		info.GetDamageType() & DMG_BOMB )
	{
		return 0;
	}

	return BaseClass::IPointsForKill( pScorer, pVictim );
	
}

//-----------------------------------------------------------------------------
// Purpose: Returns the weapon in the player's inventory that would be better than
//			the given weapon.
// Note, this version allows us to switch to a weapon that has no ammo as a last
// resort.
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CDODGameRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
{
	CBaseCombatWeapon *pCheck;
	CBaseCombatWeapon *pBest;// this will be used in the event that we don't find a weapon in the same category.

	int iCurrentWeight = -1;
	int iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
	pBest = NULL;

	// If I have a weapon, make sure I'm allowed to holster it
	if ( pCurrentWeapon )
	{
		if ( !pCurrentWeapon->AllowsAutoSwitchFrom() || !pCurrentWeapon->CanHolster() )
		{
			// Either this weapon doesn't allow autoswitching away from it or I
			// can't put this weapon away right now, so I can't switch.
			return NULL;
		}

		iCurrentWeight = pCurrentWeapon->GetWeight();
	}

	for ( int i = 0 ; i < pPlayer->WeaponCount(); ++i )
	{
		pCheck = pPlayer->GetWeapon( i );
		if ( !pCheck )
			continue;

		// If we have an active weapon and this weapon doesn't allow autoswitching away
		// from another weapon, skip it.
		if ( pCurrentWeapon && !pCheck->AllowsAutoSwitchTo() )
			continue;

		int iWeight = pCheck->GetWeight();

		// Empty weapons are lowest priority
		if ( !pCheck->HasAnyAmmo() )
		{
			iWeight = 0;
		}

		if ( iWeight > -1 && iWeight == iCurrentWeight && pCheck != pCurrentWeapon )
		{
			// this weapon is from the same category. 
			if ( pPlayer->Weapon_CanSwitchTo( pCheck ) )
			{
				return pCheck;
			}
		}
		else if ( iWeight > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
		{
			//Msg( "Considering %s\n", STRING( pCheck->GetClassname() );
			// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
			// that the player was using. This will end up leaving the player with his heaviest-weighted 
			// weapon. 

			// if this weapon is useable, flag it as the best
			iBestWeight = pCheck->GetWeight();
			pBest = pCheck;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable 
	// weapon in the same catagory as the current weapon. 

	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
	// at least get the crowbar, but ya never know.
	return pBest;
}

char *szHitgroupNames[] = 
{
	"generic",
	"head",
	"chest",
	"stomach",
	"arm_left",
	"arm_right",
	"leg_left",
	"leg_right"
};

void CDODGameRules::WriteStatsFile( const char *pszLogName )
{
	int i, j, k;

	FileHandle_t hFile = filesystem->Open( pszLogName, "w" );
	if ( hFile == FILESYSTEM_INVALID_HANDLE )
	{
		Warning( "Helper_LoadFile: missing %s\n", pszLogName );
		return;
	}

	// Header
	filesystem->FPrintf( hFile, "<?xml version=\"1.0\" ?>\n\n" );

	// open stats
	filesystem->FPrintf( hFile, "<stats>\n" );

	// per player
	for ( i=0;i<MAX_PLAYERS;i++ )
	{
		CDODPlayer *pPlayer = ToDODPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer )
		{
			filesystem->FPrintf( hFile, "\t<player>\n" );

			filesystem->FPrintf( hFile, "\t\t<playername>%s</playername>", pPlayer->GetPlayerName() );
			filesystem->FPrintf( hFile, "\t\t<steamid>STEAM:0:01</steamid>" );

			//float flTimePlayed = gpGlobals->curtime - pPlayer->m_flConnectTime;
			//filesystem->FPrintf( hFile, "\t\t<time_played>%.1f</time_played>\n", flTimePlayed );

			/*
			pPlayer->TallyLatestTimePlayedPerClass( pPlayer->GetTeamNumber(), pPlayer->m_Shared.DesiredPlayerClass() );
			
			filesystem->FPrintf( hFile, "\t\t<time_played_per_class>\n" );
			for( j=0;j<7;j++ )
			{
				// TODO : add real class names
				filesystem->FPrintf( hFile, "\t\t\t<class%i>%.1f</class%i>\n",
					j, 
					pPlayer->m_flTimePlayedPerClass[j],
					j );
			}					
			filesystem->FPrintf( hFile, "\t\t</time_played_per_class>\n" );
			*/

			filesystem->FPrintf( hFile, "\t\t<area_captures>%i</area_captures>\n", pPlayer->m_iNumAreaCaptures );
			filesystem->FPrintf( hFile, "\t\t<area_defenses>%i</area_defenses>\n", pPlayer->m_iNumAreaDefenses );
			filesystem->FPrintf( hFile, "\t\t<bonus_round_kills>%i</bonus_round_kills>\n", pPlayer->m_iNumBonusRoundKills );

			for ( j=0;j<MAX_WEAPONS;j++ )
			{
				if ( pPlayer->m_WeaponStats[j].m_iNumShotsTaken > 0 )
				{
					filesystem->FPrintf( hFile, "\t\t<weapon>\n" );

					// weapon id
					// weapon name

					filesystem->FPrintf( hFile, "\t\t\t<weaponname>%s</weaponname>\n", WeaponIDToAlias( j ) );
					filesystem->FPrintf( hFile, "\t\t\t<weaponid>%i</weaponid>\n", j );
					filesystem->FPrintf( hFile, "\t\t\t<shots>%i</shots>\n", pPlayer->m_WeaponStats[j].m_iNumShotsTaken );
					filesystem->FPrintf( hFile, "\t\t\t<hits>%i</hits>\n", pPlayer->m_WeaponStats[j].m_iNumShotsHit );
					filesystem->FPrintf( hFile, "\t\t\t<damage>%i</damage>\n", pPlayer->m_WeaponStats[j].m_iTotalDamageGiven );
					filesystem->FPrintf( hFile, "\t\t\t<avgdist>%.1f</avgdist>\n", pPlayer->m_WeaponStats[j].m_flAverageHitDistance );
					filesystem->FPrintf( hFile, "\t\t\t<kills>%i</kills>\n", pPlayer->m_WeaponStats[j].m_iNumKills );

					filesystem->FPrintf( hFile, "\t\t\t<hitgroups_hit>\n" );
					for( k=0;k<8;k++ )
					{
						if ( pPlayer->m_WeaponStats[j].m_iBodygroupsHit[k] > 0 )
						{
							filesystem->FPrintf( hFile, "\t\t\t\t<%s>%i</%s>\n", 
								szHitgroupNames[k],
								pPlayer->m_WeaponStats[j].m_iBodygroupsHit[k],
								szHitgroupNames[k] );
						}
					}
					filesystem->FPrintf( hFile, "\t\t\t</hitgroups_hit>\n" );	

					filesystem->FPrintf( hFile, "\t\t\t<times_hit>%i</times_hit>\n", pPlayer->m_WeaponStats[j].m_iNumHitsTaken );
					filesystem->FPrintf( hFile, "\t\t\t<damage_taken>%i</damage_taken>\n", pPlayer->m_WeaponStats[j].m_iTotalDamageTaken );
					filesystem->FPrintf( hFile, "\t\t\t<times_killed>%i</times_killed>\n", pPlayer->m_WeaponStats[j].m_iTimesKilled );

					filesystem->FPrintf( hFile, "\t\t\t<hit_in_hitgroups>\n" );
					for( k=0;k<8;k++ )
					{
						if ( pPlayer->m_WeaponStats[j].m_iHitInBodygroups[k] > 0 )
						{
							filesystem->FPrintf( hFile, "\t\t\t\t<%s>%i</%s>\n", 
								szHitgroupNames[k],
								pPlayer->m_WeaponStats[j].m_iHitInBodygroups[k],
								szHitgroupNames[k] );
						}
					}

					filesystem->FPrintf( hFile, "\t\t\t</hit_in_hitgroups>\n" );

					filesystem->FPrintf( hFile, "\t\t</weapon>\n" );
				}
			}

			int numKilled = pPlayer->m_KilledPlayers.Count();
			for ( j=0;j<numKilled;j++ )
			{
				filesystem->FPrintf( hFile, "<victim>\n" );
				filesystem->FPrintf( hFile, "\t<name>%s</name>\n", pPlayer->m_KilledPlayers[j].m_szPlayerName );
				filesystem->FPrintf( hFile, "\t<userid>%i</userid>\n", pPlayer->m_KilledPlayers[j].m_iUserID );
				filesystem->FPrintf( hFile, "\t<kills>%i</kills>\n", pPlayer->m_KilledPlayers[j].m_iKills );
				filesystem->FPrintf( hFile, "\t<damage>%i</damage>\n", pPlayer->m_KilledPlayers[j].m_iTotalDamage );
				filesystem->FPrintf( hFile, "</victim>" );
			}

			int numAttackers = pPlayer->m_KilledByPlayers.Count();
			for ( j=0;j<numAttackers;j++ )
			{
				filesystem->FPrintf( hFile, "<attacker>" );
				filesystem->FPrintf( hFile, "\t<name>%s</name>\n", pPlayer->m_KilledByPlayers[j].m_szPlayerName );
				filesystem->FPrintf( hFile, "\t<userid>%i</userid>\n", pPlayer->m_KilledByPlayers[j].m_iUserID );
				filesystem->FPrintf( hFile, "\t<kills>%i</kills>\n", pPlayer->m_KilledByPlayers[j].m_iKills );
				filesystem->FPrintf( hFile, "\t<damage>%i</damage>\n", pPlayer->m_KilledByPlayers[j].m_iTotalDamage );
				filesystem->FPrintf( hFile, "</attacker>" );	
			}

			filesystem->FPrintf( hFile, "\t</player>\n" );
		}
	}

	// close stats
	filesystem->FPrintf( hFile, "</stats>\n" );

	filesystem->Close( hFile );
}

#include "dod_basegrenade.h"

//==========================================================
// Called on physics entities that the player +uses ( if sv_turbophysics is on )
// Here we want to exclude grenades
//==========================================================
bool CDODGameRules::CanEntityBeUsePushed( CBaseEntity *pEnt )
{
	CDODBaseGrenade *pGrenade = dynamic_cast<CDODBaseGrenade *>( pEnt );

	if ( pGrenade )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Engine asks for the list of convars that should tag the server
//-----------------------------------------------------------------------------
void CDODGameRules::GetTaggedConVarList( KeyValues *pCvarTagList )
{
	BaseClass::GetTaggedConVarList( pCvarTagList );

	KeyValues *pKV = new KeyValues( "tag" );
	pKV->SetString( "convar", "mp_fadetoblack" );
	pKV->SetString( "tag", "fadetoblack" );

	pCvarTagList->AddSubKey( pKV );
}

#endif	//indef CLIENT_DLL

#ifdef CLIENT_DLL

void CDODGameRules::SetRoundState( int iRoundState )
{
	m_iRoundState = iRoundState;

	m_flLastRoundStateChangeTime = gpGlobals->curtime;
}

#endif // CLIENT_DLL

bool CDODGameRules::IsBombingTeam( int team )
{
	if ( team == TEAM_ALLIES )
		return m_bAlliesAreBombing;

	if ( team == TEAM_AXIS )
		return m_bAxisAreBombing;

	return false;
}

bool CDODGameRules::IsConnectedUserInfoChangeAllowed( CBasePlayer *pPlayer )
{
#ifdef GAME_DLL
	if( pPlayer )
	{
		int iPlayerTeam = pPlayer->GetTeamNumber();
		if( ( iPlayerTeam == TEAM_ALLIES ) || ( iPlayerTeam == TEAM_AXIS ) )
			return false;
	}
#else
	int iLocalPlayerTeam = GetLocalPlayerTeam();
	if( ( iLocalPlayerTeam == TEAM_ALLIES ) || ( iLocalPlayerTeam == TEAM_AXIS ) )
			return false;
#endif

	return true;
}


#ifndef CLIENT_DLL

ConVar dod_winter_never_drop_presents( "dod_winter_never_drop_presents", "0", FCVAR_CHEAT );
ConVar dod_winter_always_drop_presents( "dod_winter_always_drop_presents", "0", FCVAR_CHEAT );
ConVar dod_winter_present_drop_chance( "dod_winter_present_drop_chance", "0.2", FCVAR_CHEAT, "", true, 0.0, true, 1.0 );

float CDODGameRules::GetPresentDropChance( void )
{
	if ( dod_winter_never_drop_presents.GetBool() )
	{
		return 0.0;
	}

	if ( dod_winter_always_drop_presents.GetBool() )
	{
		return 1.0;
	}

	if ( m_bWinterHolidayActive )
	{
		return dod_winter_present_drop_chance.GetFloat();
	}

	return 0.0;
}

#endif

//=========================

#ifndef CLIENT_DLL

class CFuncTeamWall : public CBaseEntity
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CFuncTeamWall, CBaseEntity );

public:
	virtual void Spawn();
	virtual bool KeyValue( const char *szKeyName, const char *szValue ) ;
	virtual bool ShouldCollide( int collisionGroup, int contentsMask ) const;

	void WallTouch( CBaseEntity *pOther );

	void DrawThink( void );

private:
	Vector m_vecMaxs;
	Vector m_vecMins;

	int m_iBlockTeam;

	float m_flNextHintTime;

	bool m_bShowWarning;
};

BEGIN_DATADESC( CFuncTeamWall )

	DEFINE_KEYFIELD( m_iBlockTeam, FIELD_INTEGER, "blockteam" ),

	DEFINE_KEYFIELD( m_vecMaxs,		FIELD_VECTOR,  "maxs" ),
	DEFINE_KEYFIELD( m_vecMins,		FIELD_VECTOR,  "mins" ),

	DEFINE_KEYFIELD( m_bShowWarning, FIELD_BOOLEAN, "warn" ),

	DEFINE_THINKFUNC( DrawThink ),

	DEFINE_FUNCTION( WallTouch ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( func_team_wall, CFuncTeamWall );

ConCommand cc_Load_Blocker_Walls( "load_enttext", Load_EntText, 0, FCVAR_CHEAT );

ConVar showblockerwalls( "showblockerwalls", "0", FCVAR_CHEAT, "Set to 1 to visualize blocker walls" );

void CFuncTeamWall::Spawn( void )
{
	SetMoveType( MOVETYPE_PUSH );  // so it doesn't get pushed by anything
	SetModel( STRING( GetModelName() ) );
	AddEffects( EF_NODRAW );
	SetSolid( SOLID_BBOX );

	// set our custom collision if we declared this ent through the .ent file
	if ( m_vecMins != vec3_origin && m_vecMaxs != vec3_origin )
	{
		SetCollisionBounds( m_vecMins, m_vecMaxs );

		// If we delcared an angle in the .ent file, make us OBB
		if ( GetAbsAngles() != vec3_angle )
		{
			SetSolid( SOLID_OBB );
		}
	}	

	SetThink( &CFuncTeamWall::DrawThink );
	SetNextThink( gpGlobals->curtime + 0.1 );

	SetTouch( &CFuncTeamWall::WallTouch );
	m_flNextHintTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Parse data from a map file
//-----------------------------------------------------------------------------
bool CFuncTeamWall::KeyValue( const char *szKeyName, const char *szValue ) 
{
	if ( FStrEq( szKeyName, "mins" ))
	{
		UTIL_StringToVector( m_vecMins.Base(), szValue );
		return true;
	}

	if ( FStrEq( szKeyName, "maxs" ))
	{
		UTIL_StringToVector( m_vecMaxs.Base(), szValue );
		return true;
	}

	if ( FStrEq( szKeyName, "warn" ))
	{
		m_bShowWarning = atoi(szValue) > 0;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

bool CFuncTeamWall::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	bool bShouldCollide = false;

	if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
	{
		switch ( m_iBlockTeam )
		{
		case TEAM_UNASSIGNED:
			bShouldCollide = ( contentsMask & ( CONTENTS_TEAM1 | CONTENTS_TEAM2 ) ) > 0;
			break;

		case TEAM_ALLIES:
			bShouldCollide = ( contentsMask & CONTENTS_TEAM1 ) > 0;
			break;

		case TEAM_AXIS:
			bShouldCollide = ( contentsMask & CONTENTS_TEAM2 ) > 0;
			break;

		default:
			break;
		}
	}

	return bShouldCollide;
}

void CFuncTeamWall::WallTouch( CBaseEntity *pOther )
{
	if ( !m_bShowWarning )
		return;

	if ( pOther && pOther->IsPlayer() )
	{
		CDODPlayer *pPlayer = ToDODPlayer( pOther );

		if ( pPlayer->GetTeamNumber() == m_iBlockTeam )
		{
			// show a "go away" icon
			if ( m_flNextHintTime < gpGlobals->curtime )
			{
				pPlayer->HintMessage( "#dod_wrong_way" );

				// global timer, but not critical to keep timer per player.
				m_flNextHintTime = gpGlobals->curtime + 1.0;
			}
		}
	}
}

void CFuncTeamWall::DrawThink( void )
{
	if ( showblockerwalls.GetBool() )
	{
		NDebugOverlay::EntityBounds( this, 255, 0, 0, 0, 0.2 );
	}

	SetNextThink( gpGlobals->curtime + 0.1 );
}

#endif


#ifdef GAME_DLL

	#include "modelentities.h"

	#define SF_TEAM_WALL_NO_HINT	(1<<1)

	//-----------------------------------------------------------------------------
	// Purpose: Visualizes a respawn room to the enemy team
	//-----------------------------------------------------------------------------
	class CFuncNewTeamWall : public CFuncBrush
	{
		DECLARE_CLASS( CFuncNewTeamWall, CFuncBrush );
	public:
		DECLARE_DATADESC();
		DECLARE_SERVERCLASS();

		virtual void Spawn( void );

		virtual int		UpdateTransmitState( void );
		virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo );
		virtual bool	ShouldCollide( int collisionGroup, int contentsMask ) const;
		
		void WallTouch( CBaseEntity *pOther );

		void SetActive( bool bActive );

	private:
		float m_flNextHintTime;
	};

	//===========================================================================================================

	LINK_ENTITY_TO_CLASS( func_teamblocker, CFuncNewTeamWall );

	BEGIN_DATADESC( CFuncNewTeamWall )
	END_DATADESC()

	IMPLEMENT_SERVERCLASS_ST( CFuncNewTeamWall, DT_FuncNewTeamWall )
	END_SEND_TABLE()

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CFuncNewTeamWall::Spawn( void )
	{
		BaseClass::Spawn();

		SetActive( true );

		SetCollisionGroup( DOD_COLLISIONGROUP_BLOCKERWALL );

		if ( FBitSet( m_spawnflags, SF_TEAM_WALL_NO_HINT ) == false )
		{
			SetTouch( &CFuncNewTeamWall::WallTouch );
			m_flNextHintTime = gpGlobals->curtime;
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	int CFuncNewTeamWall::UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Only transmit this entity to clients that aren't in our team
	//-----------------------------------------------------------------------------
	int CFuncNewTeamWall::ShouldTransmit( const CCheckTransmitInfo *pInfo )
	{
		return FL_EDICT_ALWAYS;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	void CFuncNewTeamWall::SetActive( bool bActive )
	{
		if ( bActive )
		{
			// We're a trigger, but we want to be solid. Out ShouldCollide() will make
			// us non-solid to members of the team that spawns here.
			RemoveSolidFlags( FSOLID_TRIGGER );
			RemoveSolidFlags( FSOLID_NOT_SOLID );	
		}
		else
		{
			AddSolidFlags( FSOLID_NOT_SOLID );
			AddSolidFlags( FSOLID_TRIGGER );	
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool CFuncNewTeamWall::ShouldCollide( int collisionGroup, int contentsMask ) const
	{
		if ( GetTeamNumber() == TEAM_UNASSIGNED )
			return false;

		if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
		{
			switch( GetTeamNumber() )
			{
			case TEAM_ALLIES:
				if ( !(contentsMask & CONTENTS_TEAM1) )
					return false;
				break;

			case TEAM_AXIS:
				if ( !(contentsMask & CONTENTS_TEAM2) )
					return false;
				break;
			}

			return true;
		}

		return false;
	}

	void CFuncNewTeamWall::WallTouch( CBaseEntity *pOther )
	{
		//if ( !m_bShowWarning )
		//	return;

		if ( pOther && pOther->IsPlayer() )
		{
			CDODPlayer *pPlayer = ToDODPlayer( pOther );

			if ( pPlayer->GetTeamNumber() == GetTeamNumber() )
			{
				// show a "go away" icon
				if ( m_flNextHintTime < gpGlobals->curtime )
				{
					pPlayer->HintMessage( "#dod_wrong_way" );

					// global timer, but not critical to keep timer per player.
					m_flNextHintTime = gpGlobals->curtime + 1.0;
				}
			}
		}
	}

#else

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	class C_FuncNewTeamWall : public C_BaseEntity
	{
		DECLARE_CLASS( C_FuncNewTeamWall, C_BaseEntity );
	public:
		DECLARE_CLIENTCLASS();

		virtual bool ShouldCollide( int collisionGroup, int contentsMask ) const;

		virtual int DrawModel( int flags );
	};

	IMPLEMENT_CLIENTCLASS_DT( C_FuncNewTeamWall, DT_FuncNewTeamWall, CFuncNewTeamWall )
	END_RECV_TABLE()

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	bool C_FuncNewTeamWall::ShouldCollide( int collisionGroup, int contentsMask ) const
	{
		if ( GetTeamNumber() == TEAM_UNASSIGNED )
			return false;

		if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT )
		{
			switch( GetTeamNumber() )
			{
			case TEAM_ALLIES:
				if ( !(contentsMask & CONTENTS_TEAM1) )
					return false;
				break;

			case TEAM_AXIS:
				if ( !(contentsMask & CONTENTS_TEAM2) )
					return false;
				break;
			}

			return true;
		}

		return false;
	}

	int C_FuncNewTeamWall::DrawModel( int flags )
	{
		return 1;
	}

#endif 
