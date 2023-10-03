#include "cbase.h"
#include "asw_shareddefs.h"
#include "asw_holdout_mode.h"
#include "asw_holdout_wave.h"
#include "filesystem.h"
#ifdef GAME_DLL
#include "asw_spawn_manager.h"
#include "asw_spawn_group.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"
#include "asw_marine.h"
#include "asw_weapon.h"
#include "asw_gamerules.h"
#include "asw_equipment_list.h"
#include "asw_alien.h"
#include "asw_buzzer.h"
#include "entityinput.h"
#include "entityoutput.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Holdout_Mode, DT_ASW_Holdout_Mode )

ConVar asw_holdout_debug( "asw_holdout_debug", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Show debug text for holdout mode" );

BEGIN_NETWORK_TABLE( CASW_Holdout_Mode, DT_ASW_Holdout_Mode )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_nCurrentWave ) ),
	RecvPropInt( RECVINFO( m_nCurrentScore ) ),
	RecvPropInt( RECVINFO( m_nAliensKilledThisWave ) ),
	RecvPropTime( RECVINFO( m_flCurrentWaveStartTime ) ),
	RecvPropTime( RECVINFO( m_flResupplyEndTime ) ),
	RecvPropString( RECVINFO( m_netHoldoutFilename ) ),
#else
	SendPropInt( SENDINFO( m_nCurrentWave ) ),
	SendPropInt( SENDINFO( m_nCurrentScore ) ),
	SendPropInt( SENDINFO( m_nAliensKilledThisWave ) ),
	SendPropTime( SENDINFO( m_flCurrentWaveStartTime ) ),
	SendPropTime( SENDINFO( m_flResupplyEndTime ) ),
	SendPropString( SENDINFO( m_netHoldoutFilename ) ),
#endif
END_NETWORK_TABLE()

#ifdef GAME_DLL
LINK_ENTITY_TO_CLASS( asw_holdout_mode, CASW_Holdout_Mode );

BEGIN_DATADESC( CASW_Holdout_Mode )

	DEFINE_KEYFIELD( m_holdoutFilename,	  FIELD_STRING,  "filename" ),

	DEFINE_THINKFUNC( HoldoutThink ),

	DEFINE_OUTPUT(m_OnWave[0], "OnWave01"),
	DEFINE_OUTPUT(m_OnWave[1], "OnWave02"),
	DEFINE_OUTPUT(m_OnWave[2], "OnWave03"),
	DEFINE_OUTPUT(m_OnWave[3], "OnWave04"),
	DEFINE_OUTPUT(m_OnWave[4], "OnWave05"),
	DEFINE_OUTPUT(m_OnWave[5], "OnWave06"),
	DEFINE_OUTPUT(m_OnWave[6], "OnWave07"),
	DEFINE_OUTPUT(m_OnWave[7], "OnWave08"),
	DEFINE_OUTPUT(m_OnWave[8], "OnWave09"),
	DEFINE_OUTPUT(m_OnWave[9], "OnWave10"),

	DEFINE_OUTPUT(m_OnAnnounceWave[0], "OnAnnounceWave01"),
	DEFINE_OUTPUT(m_OnAnnounceWave[1], "OnAnnounceWave02"),
	DEFINE_OUTPUT(m_OnAnnounceWave[2], "OnAnnounceWave03"),
	DEFINE_OUTPUT(m_OnAnnounceWave[3], "OnAnnounceWave04"),
	DEFINE_OUTPUT(m_OnAnnounceWave[4], "OnAnnounceWave05"),
	DEFINE_OUTPUT(m_OnAnnounceWave[5], "OnAnnounceWave06"),
	DEFINE_OUTPUT(m_OnAnnounceWave[6], "OnAnnounceWave07"),
	DEFINE_OUTPUT(m_OnAnnounceWave[7], "OnAnnounceWave08"),
	DEFINE_OUTPUT(m_OnAnnounceWave[8], "OnAnnounceWave09"),
	DEFINE_OUTPUT(m_OnAnnounceWave[9], "OnAnnounceWave10"),

	DEFINE_OUTPUT(m_OnWaveDefault, "OnWaveDefault"),
	DEFINE_OUTPUT(m_OnAnnounceWaveDefault, "OnAnnounceWaveDefault"),

	DEFINE_INPUTFUNC( FIELD_VOID, "UnlockResupply", InputUnlockResupply ),
END_DATADESC()
#endif

const char *HoldoutEventTypeAsString( ASW_Holdout_Event_t nEventType )
{
	switch( nEventType )
	{
	case HOLDOUT_EVENT_START_WAVE_ENTRY: return "HOLDOUT_EVENT_START_WAVE_ENTRY";
	case HOLDOUT_EVENT_SINGLE_SPAWN: return "HOLDOUT_EVENT_SINGLE_SPAWN";
	case HOLDOUT_EVENT_STATE_CHANGE: return "HOLDOUT_EVENT_STATE_CHANGE";
	}
	return "Unknown";
}

const char *HoldoutStateAsString( ASW_Holdout_State_t nState )
{
	switch( nState )
	{
	case HOLDOUT_STATE_BRIEFING: return "HOLDOUT_STATE_BRIEFING";
	case HOLDOUT_STATE_ANNOUNCE_NEW_WAVE: return "HOLDOUT_STATE_ANNOUNCE_NEW_WAVE";
	case HOLDOUT_STATE_WAVE_IN_PROGRESS: return "HOLDOUT_STATE_WAVE_IN_PROGRESS";
	case HOLDOUT_STATE_SHOWING_WAVE_SCORE: return "HOLDOUT_STATE_SHOWING_WAVE_SCORE";
	case HOLDOUT_STATE_RESUPPLY: return "HOLDOUT_STATE_RESUPPLY";
	}
	return "Unknown";
}

CASW_Holdout_Mode *g_pHoldoutMode = NULL;
CASW_Holdout_Mode* ASWHoldoutMode() { return g_pHoldoutMode; }

CASW_Holdout_Mode::CASW_Holdout_Mode()
{
	Assert( !g_pHoldoutMode );
	g_pHoldoutMode = this;

	m_nCurrentWave = -1;
	m_nCurrentScore = 0;

#ifdef GAME_DLL
	m_holdoutFilename = NULL_STRING;
	m_nUnlockedResupply = 0;
	m_bResurrectingMarines = false;
#endif
	m_netHoldoutFilename.GetForModify()[0] = 0;
}

CASW_Holdout_Mode::~CASW_Holdout_Mode()
{
	m_Waves.PurgeAndDeleteElements();

	Assert( g_pHoldoutMode == this );
	g_pHoldoutMode = NULL;
}

#ifdef GAME_DLL

void CASW_Holdout_Mode::Spawn()
{
	Precache();

	BaseClass::Spawn();
}

void CASW_Holdout_Mode::Activate( void )
{
	BaseClass::Activate();

	Q_strncpy( m_netHoldoutFilename.GetForModify(), STRING( m_holdoutFilename ), MAX_PATH );
}

void CASW_Holdout_Mode::Precache()
{
	BaseClass::Precache();

	Assert( ASWSpawnManager() );

	int nCount = ASWSpawnManager()->GetNumAlienClasses();
	for ( int i = 0; i < nCount; i++ )
	{
		UTIL_PrecacheOther( ASWSpawnManager()->GetAlienClass(i)->m_pszAlienClass );
	}
}

void CASW_Holdout_Mode::InputUnlockResupply( inputdata_t &inputdata )
{
	m_nUnlockedResupply += 1;

	hudtextparms_s tTextParam;
	tTextParam.x			= -1;
	tTextParam.y			= 0.25;
	tTextParam.effect		= 0;
	tTextParam.r1			= 255;
	tTextParam.g1			= 170;
	tTextParam.b1			= 0;
	tTextParam.a1			= 255;
	tTextParam.r2			= 255;
	tTextParam.g2			= 170;
	tTextParam.b2			= 0;
	tTextParam.a2			= 255;
	tTextParam.fadeinTime	= 0.1;
	tTextParam.fadeoutTime	= 0.5;
	tTextParam.holdTime		= 5.0;
	tTextParam.fxTime		= 0;
	tTextParam.channel		= 1;

	if ( m_nHoldoutState == HOLDOUT_STATE_WAVE_IN_PROGRESS )
	{
		UTIL_HudMessageAll( tTextParam, "Resupply unlocked!\nYou will resupply at the end of THIS round\n" );
	}
	else
	{
		UTIL_HudMessageAll( tTextParam, "Resupply unlocked!\nYou will resupply at the end of NEXT round\n" );
	}
}

void CASW_Holdout_Mode::HoldoutThink()
{
	if ( asw_holdout_debug.GetBool() )
	{
		engine->Con_NPrintf( 1, "Holdout events:" );
		for ( int i = 0; i < m_Events.Count(); i++ )
		{
			CASW_Holdout_Event *pEvent = m_Events[ i ];
			if ( pEvent->m_EventType == HOLDOUT_EVENT_START_WAVE_ENTRY || pEvent->m_EventType == HOLDOUT_EVENT_SINGLE_SPAWN )
			{
				CASW_Holdout_Spawn_Event *pSpawnEvent = static_cast<CASW_Holdout_Spawn_Event*>( pEvent );
				engine->Con_NPrintf( i + 1, "%f:%s %d x %s over %f group=%s",
					gpGlobals->curtime - pEvent->m_flEventTime,
					HoldoutEventTypeAsString( pEvent->m_EventType ),
					pSpawnEvent->m_pWaveEntry->GetQuantity(),
					STRING( pSpawnEvent->m_pWaveEntry->GetAlienClass() ),
					pSpawnEvent->m_pWaveEntry->GetSpawnDuration(),
					pSpawnEvent->m_pWaveEntry->GetSpawnGroupName()
				);
			}
			else if ( pEvent->m_EventType == HOLDOUT_EVENT_STATE_CHANGE )
			{
				CASW_Holdout_State_Change_Event *pStateEvent = static_cast<CASW_Holdout_State_Change_Event*>( pEvent );
				engine->Con_NPrintf( i + 1, "%f:%s to %d",
					gpGlobals->curtime - pEvent->m_flEventTime,
					HoldoutEventTypeAsString( pEvent->m_EventType ),
					HoldoutStateAsString( pStateEvent->m_NewState )
					);
			}
			else
			{
				engine->Con_NPrintf( i + 1, "%f:%s",
					gpGlobals->curtime - pEvent->m_flEventTime,
					HoldoutEventTypeAsString( pEvent->m_EventType )
					);
			}
		}
		engine->Con_NPrintf( m_Events.Count() + 2, " " );
	}

	// see if any events need to be processed	
	while ( m_Events.Count() > 0 )
	{
		CASW_Holdout_Event *pEvent = m_Events[ 0 ];
		if ( pEvent->m_flEventTime > gpGlobals->curtime )
			break;

		m_Events.Remove( 0 );

		ProcessEvent( pEvent );
	}

	SetThink( &CASW_Holdout_Mode::HoldoutThink );

	if ( m_bResurrectingMarines )
	{
		ResurrectDeadMarines();
		SetNextThink( gpGlobals->curtime + 0.25f );
	}
	else if ( m_Events.Count() <= 0 )
	{
		SetNextThink( gpGlobals->curtime + 1.0f );
	}
	else
	{
		SetNextThink( MIN( gpGlobals->curtime + 1.0f, m_Events[ 0 ]->m_flEventTime ) );
	}
}

void CASW_Holdout_Mode::ProcessEvent( CASW_Holdout_Event *pEvent )
{
	switch( pEvent->m_EventType )
	{
	case HOLDOUT_EVENT_START_WAVE_ENTRY:			// start spawning aliens from a wave entry
		{
			// find the wave entry and spawngroup
			CASW_Holdout_Spawn_Event *pSpawnEvent = static_cast<CASW_Holdout_Spawn_Event*>( pEvent );
			CASW_Holdout_Wave_Entry *pEntry = pSpawnEvent->m_pWaveEntry;
			CASW_Spawn_Group *pSpawnGroup = pEntry->GetSpawnGroup();
			if ( !pSpawnGroup )
			{
				delete pEvent;
				return;
			}

			// spawn any aliens that need to be spawned right now
			int nToSpawnNow = ( pEntry->GetSpawnDuration() <= 0.0f ) ? pEntry->GetQuantity() : MIN( 1, pEntry->GetQuantity() );
			SpawnAliens( nToSpawnNow, pEntry, pSpawnGroup );
			
			// for spawns spread over a duration, add a single spawn event for each
			int nToSpawnLater = pEntry->GetQuantity() - nToSpawnNow;
			float flInterval = pEntry->GetSpawnDuration() / (float) nToSpawnLater;
			for ( int i = 0; i < nToSpawnLater; i++ )
			{
				if ( asw_holdout_debug.GetBool() )
				{
					Msg( "Adding a single spawn from HOLDOUT_EVENT_START_WAVE_ENTRY as %d want to spawn over time\n", nToSpawnLater );
				}
				CASW_Holdout_Spawn_Event *pSingleSpawn = new CASW_Holdout_Spawn_Event;
				pSingleSpawn->m_pWaveEntry = pEntry;
				pSingleSpawn->m_EventType = HOLDOUT_EVENT_SINGLE_SPAWN;
				pSingleSpawn->m_flEventTime = pEvent->m_flEventTime + flInterval * ( i + 1 );
				AddEvent( pSingleSpawn );
			}
		}
		break;
	case HOLDOUT_EVENT_SINGLE_SPAWN:			// single spawn from an entry that spawns its quantity over a duration
		{
			// find the wave entry and spawngroup
			CASW_Holdout_Spawn_Event *pSpawnEvent = static_cast<CASW_Holdout_Spawn_Event*>( pEvent );
			CASW_Holdout_Wave_Entry *pEntry = pSpawnEvent->m_pWaveEntry;
			CASW_Spawn_Group *pSpawnGroup = pEntry->GetSpawnGroup();
			if ( !pSpawnGroup )
			{
				delete pEvent;
				return;
			}

			SpawnAliens( 1, pEntry, pSpawnGroup );
		}
		break;
	case HOLDOUT_EVENT_STATE_CHANGE:
		{
			CASW_Holdout_State_Change_Event *pStateChangeEvent = static_cast<CASW_Holdout_State_Change_Event*>( pEvent );
			ChangeHoldoutState( pStateChangeEvent->m_NewState );
		}
		break;
	}	
	delete pEvent;
}

void CASW_Holdout_Mode::StartWave( int nWave )
{
	m_Events.PurgeAndDeleteElements();
	m_bResurrectingMarines = false;

	if ( nWave < 0 || nWave >= m_Waves.Count() )
	{
		Warning( "StartWave called with wave out of range: %d\n", nWave );
		return;
	}

	m_nCurrentWave = nWave;
	m_nAliensKilledThisWave = 0;
	m_flCurrentWaveStartTime = gpGlobals->curtime;

	// add events to spawn each entry in this wave
	CASW_Holdout_Wave *pWave = m_Waves[ m_nCurrentWave ];
	for ( int i = 0; i < pWave->GetNumEntries(); i++ )
	{
		if ( asw_holdout_debug.GetBool() )
		{
			Msg( "Adding a spawn entry from StartWave. Entry %d\n", i );
		}
		CASW_Holdout_Spawn_Event *pEvent = new CASW_Holdout_Spawn_Event;
		pEvent->m_pWaveEntry = pWave->GetEntry( i );
		pEvent->m_EventType = HOLDOUT_EVENT_START_WAVE_ENTRY;
		pEvent->m_flEventTime = m_flCurrentWaveStartTime + pEvent->m_pWaveEntry->GetSpawnDelay();
		AddEvent( pEvent );
	}

	if ( m_nCurrentWave < MAX_HOLDOUT_WAVES )
	{
		m_OnWave[m_nCurrentWave].FireOutput( this, this );
	}

	m_OnWaveDefault.FireOutput( this, this );
}

void CASW_Holdout_Mode::AddEvent( CASW_Holdout_Event *pEvent )
{
	m_Events.Insert( pEvent );

	if ( pEvent->m_flEventTime < GetNextThink() )
	{
		SetNextThink( pEvent->m_flEventTime );
	}
}

int CASW_Holdout_Mode::SpawnAliens( int nQuantity, CASW_Holdout_Wave_Entry *pEntry, CASW_Spawn_Group *pSpawnGroup )
{
	Assert( ASWSpawnManager() );

	// pick N spawners from the spawngroup
	CUtlVector< CASW_Base_Spawner* > spawners;
	pSpawnGroup->PickSpawnersRandomly( nQuantity, false, &spawners );
	
	const char *szAlienClass = STRING( pEntry->GetAlienClass() );
	int nSpawned = 0;

	Vector vecHullMins, vecHullMaxs;
	ASWSpawnManager()->GetAlienBounds( pEntry->GetAlienClass(), vecHullMins, vecHullMaxs );

	// tell each spawner to spawn an alien of our class
	for ( int i = 0; i < spawners.Count(); i++ )
	{
		IASW_Spawnable_NPC* pAlien = spawners[i]->SpawnAlien( szAlienClass, vecHullMins, vecHullMaxs );
		if ( pAlien )
		{
			pAlien->SetHoldoutAlien();
			nSpawned++;
		}
	}
	
	// for each failure, add an event to try again
	// TODO: keep track of failure count, so we give up eventually with a warning?
	for ( int i = 0; i < ( nQuantity - nSpawned ); i++ )
	{
		if ( asw_holdout_debug.GetBool() )
		{
			Msg( "Adding a spawn entry from SpawnAliens. As some aliens failed to spawn %d/%d\n", i, ( nQuantity - nSpawned ) );
		}
		CASW_Holdout_Spawn_Event *pSingleSpawn = new CASW_Holdout_Spawn_Event;
		pSingleSpawn->m_pWaveEntry = pEntry;
		pSingleSpawn->m_EventType = HOLDOUT_EVENT_SINGLE_SPAWN;
		pSingleSpawn->m_flEventTime = gpGlobals->curtime + 1.0f + 0.1f * i;
		AddEvent( pSingleSpawn );
	}

	return nSpawned;
}

ConVar asw_holdout_announce_time( "asw_holdout_announce_time", "5.0f", FCVAR_CHEAT, "How many seconds between announcing a wave and actually starting a wave.");
ConVar asw_holdout_wave_score_time( "asw_holdout_wave_score_time", "5.0f", FCVAR_CHEAT, "How many seconds to show the end wave scores for.");
ConVar asw_holdout_resupply_time( "asw_holdout_resupply_time", "20.0f", FCVAR_CHEAT, "How many seconds marines have to pick new weapons in the resupply stage.");

void CASW_Holdout_Mode::ChangeHoldoutState( ASW_Holdout_State_t nNewState )
{
	if ( asw_holdout_debug.GetBool() )
	{
		Msg( "%f: ChangeHoldoutState %s\n", gpGlobals->curtime, HoldoutStateAsString( nNewState ) );
	}
	//ASW_Holdout_State_t nOldState = GetHoldoutState();

	m_nHoldoutState = (int) nNewState;

	// States with a fixed duration schedule another state change here
	switch( nNewState )
	{
		case HOLDOUT_STATE_ANNOUNCE_NEW_WAVE:
			{
				// advance to the next wave
				SetCurrentWave( GetCurrentWave() + 1 );

				m_bResurrectingMarines = false;

				// send a message to all clients telling them to show the wave announce animation
				CRecipientFilter filter;
				filter.AddAllPlayers();
				UserMessageBegin( filter, "ASWNewHoldoutWave" );
				WRITE_BYTE( GetCurrentWave() );
				WRITE_FLOAT( asw_holdout_announce_time.GetFloat() );
				MessageEnd();

				// actually start the wave after a short delay
				CASW_Holdout_State_Change_Event *pStateEvent = new CASW_Holdout_State_Change_Event;
				pStateEvent->m_EventType = HOLDOUT_EVENT_STATE_CHANGE;
				pStateEvent->m_NewState = HOLDOUT_STATE_WAVE_IN_PROGRESS;
				pStateEvent->m_flEventTime = gpGlobals->curtime + asw_holdout_announce_time.GetFloat();
				AddEvent( pStateEvent );

				if ( m_nCurrentWave < MAX_HOLDOUT_WAVES )
				{
					m_OnAnnounceWave[m_nCurrentWave].FireOutput( this, this );
				}

				m_OnAnnounceWaveDefault.FireOutput( this, this );
			}
			break;
		case HOLDOUT_STATE_WAVE_IN_PROGRESS:
			{
				StartWave( GetCurrentWave() );
			}
			break;
		case HOLDOUT_STATE_SHOWING_WAVE_SCORE:
			{
				CASW_Holdout_State_Change_Event *pStateEvent = new CASW_Holdout_State_Change_Event;
				pStateEvent->m_EventType = HOLDOUT_EVENT_STATE_CHANGE;				
				pStateEvent->m_flEventTime = gpGlobals->curtime + asw_holdout_wave_score_time.GetFloat();
				if ( GetCurrentWave() >= m_Waves.Count() - 1 )
				{
					pStateEvent->m_NewState = HOLDOUT_STATE_COMPLETE;		// finished holdout mode!
				}
				else if ( m_nUnlockedResupply > 0 || m_Waves[ GetCurrentWave() ]->WaveHasResupply() )
				{
					pStateEvent->m_NewState = HOLDOUT_STATE_RESUPPLY;
				}
				else
				{
					pStateEvent->m_NewState = HOLDOUT_STATE_ANNOUNCE_NEW_WAVE;
				}
				AddEvent( pStateEvent );

				// resurrect dead marines
				m_bResurrectingMarines = true;

				// tell clients to show the wave end scores:
				CRecipientFilter filter;
				filter.AddAllPlayers();
				UserMessageBegin( filter, "ASWShowHoldoutWaveEnd" );
				WRITE_BYTE( GetCurrentWave() );
				WRITE_FLOAT( asw_holdout_wave_score_time.GetFloat() );
				MessageEnd();
			}
			break;
		case HOLDOUT_STATE_RESUPPLY:
			{
				if ( GetCurrentWave() < m_Waves.Count() - 1 )
				{
					if ( m_nUnlockedResupply > 0 )
					{
						m_nUnlockedResupply -= 1;
					}
					else
					{
						Warning( "Giving a resupply, but it has not been unlocked!  (m_nUnlockedResupply <= 0)\n" );
					}

					RefillMarineAmmo();

					CASW_Holdout_State_Change_Event *pStateEvent = new CASW_Holdout_State_Change_Event;
					pStateEvent->m_EventType = HOLDOUT_EVENT_STATE_CHANGE;
					pStateEvent->m_NewState = HOLDOUT_STATE_ANNOUNCE_NEW_WAVE;
					pStateEvent->m_flEventTime = gpGlobals->curtime + asw_holdout_resupply_time.GetFloat();
					AddEvent( pStateEvent );

					// tell clients to show the resupply UI:
					CRecipientFilter filter;
					filter.AddAllPlayers();
					UserMessageBegin( filter, "ASWShowHoldoutResupply" );
					MessageEnd();
					m_flResupplyEndTime = pStateEvent->m_flEventTime;
				}
				else
				{
					// completed all the waves!
					if ( asw_holdout_debug.GetBool() )
					{
						Msg( "Finished all holdout waves!\n" );
					}
				}
			}
			break;
	}
}

void CASW_Holdout_Mode::OnMissionStart()
{
	// announce the first wave after a short delay
	CASW_Holdout_State_Change_Event *pStateEvent = new CASW_Holdout_State_Change_Event;
	pStateEvent->m_EventType = HOLDOUT_EVENT_STATE_CHANGE;
	pStateEvent->m_NewState = HOLDOUT_STATE_ANNOUNCE_NEW_WAVE;
	pStateEvent->m_flEventTime = gpGlobals->curtime + 3.0f;
	AddEvent( pStateEvent );
}

void CASW_Holdout_Mode::OnAlienKilled( CBaseEntity *pAlien, const CTakeDamageInfo &info )
{
	if ( !pAlien )
		return;

	IASW_Spawnable_NPC *pSpawnable = NULL;
	if ( IsAlienClass( pAlien->Classify() ) )
	{
		CASW_Alien *pAlienNPC = static_cast<CASW_Alien*>( pAlien );
		pSpawnable = pAlienNPC;
	}
	else if ( pAlien->Classify() == CLASS_ASW_BUZZER )
	{
		CASW_Buzzer *pBuzzer = static_cast<CASW_Buzzer*>( pAlien );
		pSpawnable = pBuzzer;
	}
	if ( !pSpawnable || !pSpawnable->IsHoldoutAlien() )
		return;
		
	m_nAliensKilledThisWave++;

	if ( m_nAliensKilledThisWave >= m_Waves[ GetCurrentWave() ]->GetTotalAliens() )
	{
		// all aliens have been killed - show the wave scores after a short delay
		CASW_Holdout_State_Change_Event *pStateEvent = new CASW_Holdout_State_Change_Event;
		pStateEvent->m_EventType = HOLDOUT_EVENT_STATE_CHANGE;
		pStateEvent->m_NewState = HOLDOUT_STATE_SHOWING_WAVE_SCORE;
		pStateEvent->m_flEventTime = gpGlobals->curtime + 2.0f;
		AddEvent( pStateEvent );
	}
	m_nCurrentScore += 100;		// TODO: different values per alien type
}


void CASW_Holdout_Mode::RefillMarineAmmo()
{
	CASW_Game_Resource *pGameResource = ASWGameResource();
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		if (pGameResource->GetMarineResource(i) != NULL && pGameResource->GetMarineResource(i)->GetMarineEntity())
		{
			CASW_Marine *pMarine = pGameResource->GetMarineResource(i)->GetMarineEntity();
			for (int k=0;k<3;k++)
			{
				CASW_Weapon *pWeapon = pMarine->GetASWWeapon(k);
				if (!pWeapon)
					continue;

				// refill bullets in the gun
				pWeapon->m_iClip1 = pWeapon->GetMaxClip1();
				pWeapon->m_iClip2 = pWeapon->GetMaxClip2();

				// give the marine a load of ammo of that type
				pMarine->GiveAmmo(10000, pWeapon->GetPrimaryAmmoType());
				pMarine->GiveAmmo(10000, pWeapon->GetSecondaryAmmoType());
			}
		}
	}
}

void CASW_Holdout_Mode::ResurrectDeadMarines()
{
	const int numMarineResources = ASWGameResource()->GetMaxMarineResources();
	CASW_Marine *pAliveMarine = NULL;
	for ( int i=0; i < numMarineResources ; i++ )
	{
		CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		CASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
		if ( pMarine && pMR->GetHealthPercent() > 0 )
		{
			pAliveMarine = pMarine;
			break;
		}
	}

	for ( int i=0; i < numMarineResources ; i++ )
	{
		CASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource( i );
		if ( pMR && pMR->GetHealthPercent() <= 0 ) // if marine exists, is dead
		{
			ASWGameRules()->Resurrect( pMR, pAliveMarine );
			return; // don't do two in a frame
		}
	}
}

// player has picked a new weapon from the resupply panel
void CASW_Holdout_Mode::LoadoutSelect( CASW_Marine_Resource *pMarineResource, int nEquipIndex, int nInvSlot )
{
	CASW_Marine *pMarine = pMarineResource->GetMarineEntity();
	if ( !pMarine )
		return;

	// remove the old weapon in this slot, if any
	CASW_Weapon *pOldWeapon = pMarine->GetASWWeapon( nInvSlot );
	if ( pOldWeapon )
	{
		pMarine->DropWeapon( pOldWeapon, true );
		UTIL_Remove( pOldWeapon );
	}

	// give them the new weapon
	ASWGameRules()->GiveStartingWeaponToMarine( pMarine, nEquipIndex, nInvSlot );
	pMarineResource->UpdateWeaponIndices();
}

CON_COMMAND( asw_holdout_start_wave, "Starts a holdout wave" )
{
	if ( !ASWHoldoutMode() )
	{
		Msg( "Unable to start holdout wave as this isn't a holdout map\n" );
		return;
	}

	if ( args.ArgC() != 2 )
	{
		Msg( "Usage:  asw_holdout_start_wave [wave num]\n" );
		return;
	}

	ASWHoldoutMode()->StartWave( atoi( args[1] ) );
}

#endif		// GAME_DLL

void CASW_Holdout_Mode::LevelInitPostEntity()
{	
#ifdef GAME_DLL
	LoadWaves();

	SetThink( &CASW_Holdout_Mode::HoldoutThink );
	SetNextThink( gpGlobals->curtime + 1.0f );
#endif
}

#ifdef CLIENT_DLL
void CASW_Holdout_Mode::OnDataChanged(DataUpdateType_t updateType)
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		LoadWaves();
	}
}
#endif

void CASW_Holdout_Mode::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

	m_Waves.PurgeAndDeleteElements();
}

void CASW_Holdout_Mode::LoadWaves()
{
	m_Waves.PurgeAndDeleteElements();

	if ( asw_holdout_debug.GetBool() )
	{
		Msg( "CASW_Holdout_Mode::LoadWaves\n" );
	}

	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/holdout/%s.txt", m_netHoldoutFilename.Get() );

	KeyValues *pKV = new KeyValues( "HoldoutWaves" );
	if ( !pKV->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
	{
		Warning( "Failed to load holdout resource file: '%s'  Attempting to load default.\n", tempfile );

		Q_snprintf( tempfile, sizeof( tempfile ), "resource/holdout/%s.txt", "HoldoutWaves_Default" );
		if ( !pKV->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
		{
			Warning( "WARNING: Failed to load default holdout resource file.  'resource/holdout/HoldoutWaves_Default.txt' is missing!\n" );
			return;
		}
	}


	KeyValues *pKeys = pKV;
	while ( pKeys )
	{		
		if ( asw_holdout_debug.GetBool() )
		{
			Msg( "  Loading a wave\n" );
		}
		CASW_Holdout_Wave* pWave = new CASW_Holdout_Wave;
		pWave->LoadFromKeyValues( m_Waves.Count(), pKeys );
		m_Waves.AddToTail( pWave );

		pKeys = pKeys->GetNextKey();
	}
	pKV->deleteThis();
}

float CASW_Holdout_Mode::GetWaveProgress()
{
	if ( GetCurrentWave() < 0 || GetCurrentWave() >= m_Waves.Count() )
		return 1.0f;


	return 1.0f - ( (float) GetAliensKilledThisWave() / (float) m_Waves[ GetCurrentWave() ]->GetTotalAliens() );
}