//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: TF2's player object.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include <stdarg.h>
#include "player.h"
#include "tf_player.h"
#include "gamerules.h"
#include "trains.h"
#include "entitylist.h"
#include "menu_base.h"
#include "basecombatweapon.h"
#include "controlzone.h"
#include "tf_shareddefs.h"
#include "AmmoDef.h"
#include "techtree.h"
#include "in_buttons.h"
#include "tf_team.h"
#include "client.h"
#include "baseviewmodel.h"
#include "tf_gamerules.h"
#include "tf_obj.h"
#include "weapon_builder.h"
#include "orders.h"
#include "decals.h"
#include "tf_func_resource.h"
#include "resource_chunk.h"
#include "team_messages.h"
#include "tier0/dbg.h"
#include "tf_obj_respawn_station.h"
#include "tf_obj_resourcepump.h"
#include "tf_class_commando.h"
#include "tf_class_defender.h"
#include "tf_class_escort.h"
#include "tf_class_infiltrator.h"
#include "tf_class_medic.h"
#include "tf_class_recon.h"
#include "tf_class_sniper.h"
#include "tf_class_support.h"
#include "tf_class_sapper.h"
#include "sendproxy.h"
#include "ragdoll_shadow.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "bone_setup.h"
#include "weapon_combatshield.h"
#include "weapon_twohandedcontainer.h"
#include "NDebugOverlay.h"
#include "tier1/strtools.h"
#include "IEffects.h"
#include "info_act.h"
#include "ai_basehumanoid.h"
#include "tf_stats.h"
#include "iservervehicle.h"
#include "tf_vehicle_teleport_station.h"
#include "globals.h"

#define MAX_EXPLOSIVE_VELOCITY	600.0f

extern ConVar tf_knockdowntime;

extern ConVar inv_demo;

ConVar tf_autoteam( "tf_autoteam", "1", 0, "Automatically place players on the team with the least players." );
ConVar tf_destroyobjects( "tf_destroyobjects", "1", FCVAR_CHEAT, "Destroy objects when players change class or team." );

IMPLEMENT_SERVERCLASS_ST(CBaseTFPlayer, DT_BaseTFPlayer)
	SendPropDataTable(SENDINFO_DT(m_TFLocal), &REFERENCE_SEND_TABLE(DT_TFLocal), SendProxy_SendLocalDataTable),

	SendPropInt(SENDINFO(m_iPlayerClass), 4, SPROP_UNSIGNED),
	
	// Class Data Tables
	SendPropDataTable( SENDINFO_DT( m_PlayerClasses ), &REFERENCE_SEND_TABLE( DT_AllPlayerClasses ), SendProxy_SendLocalDataTable ),

	SendPropEHandle( SENDINFO( m_hSelectedMCV ) ),
	SendPropInt( SENDINFO(m_iCurrentZoneState ), 3 ),
	SendPropInt( SENDINFO(m_iMaxHealth ), 8, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_TFPlayerFlags), TF_PLAYER_NUMFLAGS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bUnderAttack ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bIsBlocking ), 1, SPROP_UNSIGNED ),

	// Sniper - will get moved to a class data table
	SendPropVector( SENDINFO(m_vecDeployedAngles), -1, SPROP_COORD ),
	SendPropInt( SENDINFO( m_bDeployed ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bDeploying ), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bUnDeploying ), 1, SPROP_UNSIGNED ),

	// Infiltrator - will get moved to a class data table
	SendPropFloat( SENDINFO( m_flCamouflageAmount ), 7, SPROP_ROUNDDOWN, 0.0f, 100.0f ),

	SendPropEHandle(SENDINFO(m_hSpawnPoint)),

	SendPropExclude( "DT_BaseAnimating" , "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating" , "m_flPlaybackRate" ),

END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( player, CBaseTFPlayer );
PRECACHE_REGISTER(player);

BEGIN_DATADESC( CBaseTFPlayer )

	DEFINE_INPUTFUNC( FIELD_VOID, "Respawn", InputRespawn ),

	// Function Pointers
	DEFINE_THINKFUNC( TFPlayerDeathThink ),

END_DATADESC()


BEGIN_PREDICTION_DATA_NO_BASE( CTFPlayerLocalData )
END_PREDICTION_DATA()

BEGIN_PREDICTION_DATA( CBaseTFPlayer )
END_PREDICTION_DATA()


bool IsSpawnPointValid( CBaseEntity *pPlayer, CBaseEntity *pSpot );
void respawn( CBaseEntity *pEdict, bool fCopyCorpse );
int TrainSpeed(int iSpeed, int iMax);
void BulletWizz( Vector vecSrc, Vector vecEndPos, edict_t *pShooter, bool isTracer );

extern float	g_flNextReinforcementTime;
extern short	g_sModelIndexFireball;
extern CBaseEntity	*g_pLastSpawn;

//-----------------------------------------------------------------------------
// Purpose: Don't do anything for now
// Input  : *pFormat - 
//			... - 
// Output : static void
//-----------------------------------------------------------------------------
void StatusPrintf( bool clear, int destination, char *pFormat, ... )
{
	return;

	/*
	va_list marker;
	char msg[8192];

	va_start(marker, pFormat);
	Q_vsnprintf(msg, sizeof( msg ), pFormat, marker);
	va_end(marker);	
	
	Msg( msg );
	*/
}

#pragma warning( disable : 4355 )

//=====================================================================
// PLAYER HANDLING
//=====================================================================
CBaseTFPlayer::CBaseTFPlayer() :
	m_PlayerClasses( this ), m_PlayerAnimState( this )
{
	// HACK because player's have pev set in baseclass constructor
	// which triggers an assert that we want to keep.
	{
		edict_t *savepev = edict();
		NetworkProp()->SetEdict( NULL );
		UseClientSideAnimation();
		NetworkProp()->SetEdict( savepev );
	}

	m_bWasMoving = false;

	m_iLastSecondsToGo = -1;
	m_TFLocal.m_nInTacticalView = 0;
	m_TFLocal.m_pPlayer = this;
	m_bSwitchingView = false;
	ClearActiveWeapon();
	
	m_iPlayerClass = TFCLASS_UNDECIDED;
	SetPlayerClass( TFCLASS_UNDECIDED );
	m_pCurrentMenu = NULL;
	m_TFPlayerFlags = 0;
	m_bDeploying = false;
	m_bDeployed = false;
	m_bUnDeploying = false;
	m_flFinishedDeploying = 0;
	SetOrder( NULL );
	
	m_nPreferredTechnology = -1;
	m_nMedicDamageBoosts = 0;

	m_hSpawnPoint = NULL;
	m_flLastTimeDamagedByEnemy = -1000;

	int i;
	for ( i = 0; i < MOMENTUM_MAXSIZE; i++ )
	{
		m_aMomentum[ i ] = 1.0f;
	}
}

void CBaseTFPlayer::UpdateOnRemove( void )
{
	if ( m_hSelectedOrder )
	{
		GetTFTeam()->RemoveOrdersToPlayer( this );
		Assert( !m_hSelectedOrder.Get() );
	}

	ClearPlayerClass();

	ClearClientRagdoll( false );

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

CBaseTFPlayer::~CBaseTFPlayer()
{
	SetPlayerClass( (TFClass)-1 );
}

bool CBaseTFPlayer::IsHidden() const
{
	return (m_TFPlayerFlags & TF_PLAYER_HIDDEN) != 0;
}

void CBaseTFPlayer::SetHidden( bool bHidden )
{
	if ( bHidden )
		m_TFPlayerFlags |= TF_PLAYER_HIDDEN;
	else
		m_TFPlayerFlags &= ~TF_PLAYER_HIDDEN;
}

//-----------------------------------------------------------------------------
// Purpose: Called everytime the player's respawned
//-----------------------------------------------------------------------------
void CBaseTFPlayer::Spawn( void )
{
	m_bUnderAttack = false;
	m_pCurrentZone = NULL;
	ClearClientRagdoll( false );

	g_pNotify->ReportNamedEvent( this, "PlayerSpawned" );

	DeactivateMovementConstraint();

	if ( IsInAVehicle() )
	{
		LeaveVehicle();
	}

	// If the player doesn't have a spawn station set, find one
	if ( m_hSpawnPoint == NULL || !InSameTeam( m_hSpawnPoint ) )
	{
		m_hSpawnPoint = GetInitialSpawnPoint();
	}

	if ( inv_demo.GetBool() )
	{
		if ( !GetPlayerClass() )
		{
			ChangeClass( TFCLASS_MEDIC );
			m_Local.m_iHideHUD |= HIDEHUD_MISCSTATUS;
			engine->ServerCommand("r_DispEnableLOD 0\n");
		}
	}

	// Must be done before baseclass spawn, so it's correct for when we find a spawnpoint
	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->SetPlayerHull();
	}

	// Use human commando model until we know our class
	SetModel( "models/player/human_commando.mdl" );

	BaseClass::Spawn();

	m_flFractionalBoost = 0.0f;

	// Create second view model ( for support/commando, etc )
	CreateViewModel( 1 );

	// Tell the PlayerClass that this player's just respawned
	if ( GetPlayerClass()  )
	{
		RemoveFlag( FL_NOTARGET );
		RemoveSolidFlags( FSOLID_NOT_SOLID );

		GetPlayerClass()->RespawnClass();
		if ( GetActiveWeapon() )
		{
			// Holster weapon immediately, to allow it to cleanup
//			GetActiveWeapon()->Holster( ); // NJS: test

			if (GetActiveWeapon()->HasAnyAmmo())
			{
				Weapon_Switch( GetActiveWeapon() );
			}
			else
			{
				SwitchToNextBestWeapon( GetActiveWeapon() );
			}
		}
		else
		{
			SwitchToNextBestWeapon( NULL );
		}

		SetPlayerModel();

		// Make sure they're not deployed
		FinishUnDeploying();

		// Remove my personal orders
		if ( GetTFTeam() )
		{
			GetTFTeam()->RemoveOrdersToPlayer( this );
		}

		RemoveAllDecals();
	}
	else
	{
		// No class? can't target this dude
		AddFlag( FL_NOTARGET );

		// Remove everything
		RemoveAllItems( false );

		// Set/unset m_bHidden instead to hide the tf player
		SetHidden( true );

		AddSolidFlags( FSOLID_NOT_SOLID );
		SetMoveType( MOVETYPE_NONE );

		SetModel( "models/player/human_commando.mdl" );

		// If they're not in a team, bring up the Team Menu
		if ( !IsInAnyTeam() )
		{
			if ( tf_autoteam.GetFloat() )
			{
				// Autoteam the player
				PlacePlayerInTeam();
				ForceRespawn();
			}
			else
			{
				// Let players choose their team
				m_pCurrentMenu = gMenus[MENU_TEAM];
			}
		}
		else // Bring up the Class Menu
		{
			m_pCurrentMenu = gMenus[MENU_CLASS];
		}

		m_MenuRefreshTime = gpGlobals->curtime;
		
		m_nPreferredTechnology = -1;
	}

	SetCantMove( false );


	m_TFLocal.m_nInTacticalView = 0;
	m_flLastTimeDamagedByEnemy = -1000;

	// Purge resource chunks
	for ( int i=0; i < m_TFLocal.m_iResourceAmmo.Count(); i++ )
		m_TFLocal.m_iResourceAmmo.Set( i, 0 );

	ResetKnockdown();
	SetGagged( false );
	SetUsingThermalVision( false );
	ClearCamouflage();
	SetIDEnt( NULL );
	m_iPowerups = 0;

	// MUST set the right player hull before placing the player somewhere.
	if ( GetPlayerClass() )
		GetPlayerClass()->SetPlayerHull();

	g_pGameRules->GetPlayerSpawnSpot( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::CleanupOnActStart( void )
{
	// Tell all our weapons
	for ( int i = 0; i < WeaponCount(); i++ ) 
	{
		if ( GetWeapon(i) ) 
		{
			((CBaseTFCombatWeapon*)GetWeapon(i))->CleanupOnActStart();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::RecalculateSpeed( void )
{
	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->SetMaxSpeed( GetPlayerClass()->GetMaxSpeed() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: I just killed another player
//-----------------------------------------------------------------------------
void CBaseTFPlayer::KilledPlayer( CBaseTFPlayer *pVictim )
{
	TFStats()->IncrementPlayerStat( this, TF_PLAYER_STAT_KILL_COUNT, 1 );
	TFStats()->IncrementTeamStat( GetTeamNumber(), TF_TEAM_STAT_KILL_COUNT, 1 );

	// Am I in a rampage?
	if ( HasPowerup( POWERUP_RUSH ) && IsInRampage() )
	{
		// Extend my rush
		AttemptToPowerup( POWERUP_RUSH, ADRENALIN_RAMPAGE_EXTEND );

		// Let 'em know
		EmitSound( "BaseTFPlayer.BloodSportKiller" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called only the first time a player's placed in the map
//-----------------------------------------------------------------------------
void CBaseTFPlayer::InitialSpawn( void )
{
	BaseClass::InitialSpawn();
	SetWeaponBuilder( NULL );

	m_bFirstTeamSpawn = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::Precache( void )
{
	//!! hack for radar
	BaseClass::Precache();

	PrecacheScriptSound( "BaseTFPlayer.BloodSportKiller" );
	PrecacheScriptSound( "Humans.Death" );
	PrecacheScriptSound( "AlienCommando.Death" );
	PrecacheScriptSound( "AlienMedic.Death" );
	PrecacheScriptSound( "AlienDefender.Death" );
	PrecacheScriptSound( "AlienEscort.Death" );
	PrecacheScriptSound( "BaseTFPlayer.StartDeploying" );
	PrecacheScriptSound( "BaseTFPlayer.StartUnDeploying" );
	PrecacheScriptSound( "BaseTFPlayer.KnockedDown" );
	PrecacheScriptSound( "BaseTFPlayer.ThermalOn" );
	PrecacheScriptSound( "BaseTFPlayer.ThermalOff" );
	PrecacheScriptSound( "BaseTFPlayer.PickupResources" );
	PrecacheScriptSound( "BaseTFPlayer.DonateResources" );

	// Class specific sounds
	PrecacheScriptSound( "Commando.BootHit" );
	PrecacheScriptSound( "Commando.BootSwing" );
	PrecacheScriptSound( "Commando.BullRushScream" );
	PrecacheScriptSound( "Commando.BullRushFlesh" );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::UpdateClientData( void )
{
	CTeam *pTeam = GetTeam();
	if ( pTeam )
		pTeam->UpdateClientData( this );

	BaseClass::UpdateClientData();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ForceClientDllUpdate( void )
{
	BaseClass::ForceClientDllUpdate();

	// Force any active menu to be reset
	m_MenuRefreshTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Forces an immediate respawn of the player
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ForceRespawn( void )
{
	Spawn();
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that forces a respawn of the player.
//-----------------------------------------------------------------------------
void CBaseTFPlayer::InputRespawn( inputdata_t &inputdata )
{
	ForceRespawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::InitHUD( void )
{
	CSingleUserRecipientFilter user( this );
	user.MakeReliable();

	// If we're in an act, tell it to update the client
	if ( g_hCurrentAct )
	{
		g_hCurrentAct->UpdateClient( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Player has just tried to switch to a new weapon
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SelectItem( const char *pstr, int iSubType )
{
	// can't change weapon while deployed
	if ( IsPlayerLockedInPlace() || IsDeployed() || IsDeploying() )
		return;

	// Pass through to CBaseCombatWeapon code
	BaseClass::SelectItem( pstr, iSubType );
}

//-----------------------------------------------------------------------------
// Purpose: Put the player in the specified team
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ChangeTeam( int iTeamNum )
{
	// If we're changing team, clear my order
	if ( iTeamNum != GetTeamNumber() )
	{
		SetOrder(NULL);
		if ( tf_destroyobjects.GetFloat() )
		{
			RemoveAllObjects( false );
		}
	}

	// Force full tech tree update
	for ( int i = 0 ; i < MAX_TECHNOLOGIES; i++ )
	{
		m_rgClientTechAvail[ i ].m_nAvailable = -1;
	}

	BaseClass::ChangeTeam( iTeamNum );

	// Now handle resources:
	//  - If it's the first spawn ever, give the player the team's currently calculated resource amount
	//  - If the player has more resources than the team's joining amount, drop his resources to that amount. Otherwise, he can keep his current.
	if ( GetGlobalTFTeam( iTeamNum ) )
	{
		float flJoiningResources = GetGlobalTFTeam( iTeamNum )->GetJoiningPlayerResources();
		if ( m_bFirstTeamSpawn )
		{
			m_bFirstTeamSpawn = false;
			SetBankResources( flJoiningResources );
		}
		else
		{
			if ( flJoiningResources < GetBankResources() )
			{
				SetBankResources( flJoiningResources );
			}
		}
	}

	// Clear the client ragdoll, when changing teams.
	ClearClientRagdoll( false );
}

//-----------------------------------------------------------------------------
// Purpose: Automatically place the player in the most appropriate team
//-----------------------------------------------------------------------------
void CBaseTFPlayer::PlacePlayerInTeam( void )
{
	CTFTeam *pTargetTeam = NULL;

	// Find the team with the least players in it
	for ( int i = 0; i < MAX_TF_TEAMS; i++ )
	{
		CTFTeam *pTeam = GetGlobalTFTeam(i);

		if ( pTargetTeam )
		{
			if ( pTeam->GetNumPlayers() < pTargetTeam->GetNumPlayers() )
				pTargetTeam = pTeam;
		}
		else
		{
			pTargetTeam = pTeam;
		}
	}

	ChangeTeam( pTargetTeam->GetTeamNumber() );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the specified class is available to this player
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsClassAvailable( TFClass iClass )
{
	char str[128];
	Q_snprintf( str, sizeof( str ), "class_%s", GetTFClassInfo( iClass )->m_pClassName );
	return HasNamedTechnology( str );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ChangeClass( TFClass iClass )
{
	// If they've got a playerclass, kill it
	if ( GetPlayerClass()  )
	{
		if ( tf_destroyobjects.GetFloat() )
		{
			RemoveAllObjects( false, iClass );
		}

		ClearPlayerClass();
	}

	// can't change class if we have no team
	if ( !IsInAnyTeam() )
		return;

	// Make sure client .dll can find out about it.
	SetPlayerClass( iClass );

	// Clear out current vote....
	CTFTeam *pTFTeam = GetTFTeam();
	SetPreferredTechnology( pTFTeam->m_pTechnologyTree, -1 );

	// Force a respawn if they're alive
	if ( IsAlive() )
	{
		ForceRespawn();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reset player class
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ClearPlayerClass( void )
{
	// Remove all weapons & items
	if ( GetPlayerClass() )
	{
		RemoveAllItems( false );
		m_hWeaponCombatShield = NULL;
	}

	m_iPowerups = 0;
	SetPlayerClass( TFCLASS_UNDECIDED );
}

//-----------------------------------------------------------------------------
// Purpose: Set the player's model to the correct one, taking into account
//			class, gender, team, and disguise.
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetPlayerModel( void )
{
	if (!GetPlayerClass())
	{
		SetHidden( true );
		return;
	}

	string_t sModel = GetPlayerClass()->GetClassModel( GetTeamNumber() );

	// If they don't have a model, make the player invisible
	if ( !sModel )
	{
		SetHidden( true );
		return;
	}

	// Make the player visible
	SetHidden( false );

	// Set the model
	SetModel( STRING( sModel ) );

	if ( GetFlags() & FL_DUCKING ) 
		UTIL_SetSize(this, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	else
		UTIL_SetSize(this, VEC_HULL_MIN, VEC_HULL_MAX);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::PlayerRespawn( void )
{
	m_nButtons = 0;
	m_iRespawnFrames = 0;

	// don't copy a corpse if we're in deathcam.
	respawn( this, !IsObserver() );
	SetThink( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Play a sound when we die
//-----------------------------------------------------------------------------
void CBaseTFPlayer::DeathSound( const CTakeDamageInfo &info )
{
	if ( GetTeamNumber() == TEAM_HUMANS )
	{
		EmitSound( "Humans.Death" );
	}
	else if ( GetTeamNumber() == TEAM_ALIENS )
	{
		switch( PlayerClass() )
		{
		case TFCLASS_COMMANDO:
			EmitSound( "AlienCommando.Death" );
			break;

		case TFCLASS_MEDIC:
			EmitSound( "AlienMedic.Death" );
			break;

		case TFCLASS_DEFENDER:
			EmitSound( "AlienDefender.Death" );
			break;

		case TFCLASS_ESCORT:
			EmitSound( "AlienEscort.Death" );
			break;

		default:
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ItemPostFrame()
{
	// Don't process items while in a vehicle.
	if ( IsInAVehicle() )
	{
		IServerVehicle *pVehicle = GetVehicle();
		Assert( pVehicle );

		// NOTE: We *have* to do this before ItemPostFrame because ItemPostFrame
		// may dump us out of the vehicle
		int nRole = pVehicle->GetPassengerRole( this );
		bool bUsingStandardWeapons = pVehicle->IsPassengerUsingStandardWeapons( nRole );

		pVehicle->ItemPostFrame( this );

		// Fall through and check weapons, etc. if we're using them 
		if (!bUsingStandardWeapons || !IsInAVehicle())
			return;
	}

	// If we're attaching a sapper, handle player use only
	if ( m_TFLocal.m_bAttachingSapper )
	{
		PlayerUse();
		return;
	}

	BaseClass::ItemPostFrame();

	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->ItemPostFrame();	// Let the player class handle it.
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::Jump( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::PreThink(void)
{
	CheckBuffs();

	// Riding a vehicle?
	if ( IsInAVehicle() )
	{
		BaseClass::PreThink();
		return;
	}

	CheckDeployFinish();
	CheckKnockdown();
	CheckCamouflage();
	CheckSapperAttaching();

	// Update reinforcement state
	if (m_lifeState >= LIFE_DYING)
	{
		// After 3 seconds, move them to the Tactical Map
		if ( (gpGlobals->curtime - m_flTimeOfDeath) > 3.0 )
		{
			if ( m_TFLocal.m_nInTacticalView == false )
			{
				ShowTacticalView( 1 );
			}
		}

		// ROBIN: Maps will define whether or not teams reinforce
		/*
		// Aliens respawn in waves
		if ( GetTeamNumber() == TEAM_ALIENS )
		{
			int iSecondsToGo = (int)(g_flNextReinforcementTime - gpGlobals->curtime);
			if ( iSecondsToGo != m_iLastSecondsToGo && iSecondsToGo >= 1 )
			{
				m_iLastSecondsToGo = iSecondsToGo;
				ClientPrint( this, HUD_PRINTCENTER, UTIL_VarArgs("\nReinforcing in %d %s\n", iSecondsToGo, iSecondsToGo > 1 ? "seconds" : "second" ) );
			}
		}
		*/

		TFPlayerDeathThink();
	}

	// Update zone state
	if ( m_pCurrentZone )
	{
		m_iCurrentZoneState = m_pCurrentZone->GetControllingTeam();
		if ( m_iCurrentZoneState != ZONE_CONTESTED )
		{
			// Set the Zone state to the correct one
			if ( m_iCurrentZoneState == GetTeamNumber() )
				m_iCurrentZoneState = ZONE_FRIENDLY;
			else
				m_iCurrentZoneState = ZONE_ENEMY;
		}
	}
	else
	{
		m_iCurrentZoneState = 0;
	}

	BaseClass::PreThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::PostThink()
{
	BaseClass::PostThink();

	// Make sure we have a valid MCV id.
	CVehicleTeleportStation *pMCV = GetSelectedMCV();
	if ( !pMCV || !pMCV->IsDeployed() )
	{
		m_hSelectedMCV = CVehicleTeleportStation::GetFirstDeployedMCV( GetTeamNumber() );
	}

	// Tell the client if our damage is boosted so it can do a smurfy effect on the weapon.
	if ( GetAttackDamageScale( NULL ) == 1 )
		m_TFPlayerFlags &= ~TF_PLAYER_DAMAGE_BOOST;
	else
		m_TFPlayerFlags |= TF_PLAYER_DAMAGE_BOOST;

	m_PlayerAnimState.Update();
//	SetLocalAngles( m_PlayerAnimState.GetRenderAngles() );

	float flTimeSinceAttacked = gpGlobals->curtime - LastTimeDamagedByEnemy();
	m_bUnderAttack = ((flTimeSinceAttacked >= 0.0f) && (flTimeSinceAttacked < 1.0f));

	// TODO: This collision hull is set in the base class PostThink (so this is
	// redundant), but I don't wanna re-write the whole thing at this point.
	// We will just have to deal with a little redundancy for now.
	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->SetPlayerHull();
	}

	// Menus
	MenuDisplay();

	// Player class Think
	if (GetPlayerClass())
	{
		GetPlayerClass()->ClassThink();
	}

	if ( m_bSwitchingView )
	{
		m_bSwitchingView = false;
		SetMoveType( m_TFLocal.m_nInTacticalView ? MOVETYPE_ISOMETRIC : MOVETYPE_WALK );
	}

	FollowClientRagdoll();
}

//-----------------------------------------------------------------------------
// Purpose: selects a valid point that the player can spawn at
// Output : edict_t - the point in the world to spawn at
//-----------------------------------------------------------------------------
CBaseEntity *CBaseTFPlayer::EntSelectSpawnPoint( void )
{
	// If we're in a team, ask the team for a spawnpoint
	if ( GetTeam() )
	{
		CBaseEntity *entity = NULL;
		if ( GetPlayerClass()  )
		{
			// Let individual player classes override the respawn point
			entity = GetPlayerClass()->SelectSpawnPoint();
			if ( entity )
			{
				return entity;
			}

			// Do we have a selected spawn point (from a respawn station)?
			entity = m_hSpawnPoint;
			if (entity && (entity->GetTeam() == GetTeam()))
			{
				PlayRespawnEffect( entity );
				return entity;
			}
		}

		entity = GetTeam()->SpawnPlayer( this );
		if ( entity )
			return entity;
	}

	// If we're not in a team, or the team didn't have a spawnpoint for us,
	// fall back to the basic spawnpoint code.
	return BaseClass::EntSelectSpawnPoint();
}

void CBaseTFPlayer::RemoveShieldOverlays( void )
{
	RemoveGesture( ACT_OVERLAY_SHIELD_UP );
	RemoveGesture( ACT_OVERLAY_SHIELD_DOWN );
	RemoveGesture( ACT_OVERLAY_SHIELD_UP_IDLE );
	RemoveGesture( ACT_OVERLAY_SHIELD_ATTACK );
	RemoveGesture( ACT_OVERLAY_SHIELD_KNOCKBACK );
}

static bool IsShieldOverlay( Activity activity )
{
	switch ( activity )
	{
	default:
		return false;
	case ACT_OVERLAY_SHIELD_UP:
	case ACT_OVERLAY_SHIELD_DOWN:
	case ACT_OVERLAY_SHIELD_UP_IDLE:
	case ACT_OVERLAY_SHIELD_ATTACK:
	case ACT_OVERLAY_SHIELD_KNOCKBACK:
		return true;
	}
	return false;
}

int CBaseTFPlayer::RemoveShieldOverlaysExcept( Activity activity, bool addifnotpresent /*= true */ )
{
	int skip = FindGestureLayer( activity );

	int i;
	for ( i = 0; i < CBaseAnimatingOverlay::MAX_OVERLAYS; i++ )
	{
		if ( i == skip )
			continue;

		if ( IsShieldOverlay( GetLayerActivity( i ) ) )
		{
			RemoveLayer( i, 0.0, 0.0f );
		}
	}
	
	// Add it in if it's not present already
	if ( addifnotpresent && ( skip == -1 ) )
	{
		return AddGesture( activity );
	}
	else
	{
		return skip;
	}
}	

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - i/o :  can be changed to a new activity
//			overlayindex - o:  if an overlay is picked, this gets changed
// Rest of paramters are input only
//			moving - is player moving
//			ducked - is player ducking
//			overlay - animation choices for this state (either full body crouch/stand, or overlay on top of base crouch/stand )
//			crouch - 
//			normal - 
// Overlay parameters
//			autokill - if false, overlay will loop indefinitely
//			blendin - amount of time over which to blend in (0.0f for snap)
//			blendout - same but for blending out instead
//-----------------------------------------------------------------------------
void CBaseTFPlayer::PickShieldAnimation( Activity& activity, int& overlayindex, bool moving, bool ducked, 
	Activity overlay, Activity crouch, Activity normal, 
	bool autokill /*=true*/, float blendin /*=0.0f*/, float blendout /*=0.0f*/ )
{
	if ( moving )
	{
		overlayindex = RemoveShieldOverlaysExcept( overlay );
		if ( overlayindex != -1 )
		{
			if ( blendin > 0.0f )
			{
				SetLayerBlendIn( overlayindex, blendin );
			}	

			if ( blendout > 0.0f )
			{
				SetLayerBlendOut( overlayindex, blendout );
			}

			if ( !autokill )
			{
				SetLayerAutokill( overlayindex, false );
			}
		}
	}
	else
	{
		activity = ducked ? crouch : normal;
	}
}

Activity CBaseTFPlayer::ShieldTranslateActivity( Activity activity )
{
	CWeaponTwoHandedContainer *container = dynamic_cast< CWeaponTwoHandedContainer * >( GetActiveWeapon() );
	if ( !container )
		return activity;

	CWeaponCombatShield *pShield = dynamic_cast< CWeaponCombatShield * >( container->GetLeftWeapon() );
	if ( !pShield )
	{
		pShield =  dynamic_cast< CWeaponCombatShield * >( container->GetRightWeapon() );
		if ( !pShield )
		{
			return activity;
		}
	}

	float speed = GetAbsVelocity().Length2D();
	bool isMoving = speed != 0 ? true : false;
	//bool isRunning = speed > 75 ? true : false;
	bool isDucked = ( GetFlags() & FL_DUCKING ) ? true : false;

	int shieldState = pShield->GetShieldState();

	float startframe = 0.0f;

	bool movechanged = isMoving ^ m_bWasMoving;
	if ( movechanged)
	{
		// Grab frame from overlay
		if ( !isMoving )
		{
			for ( int i = 0; i < MAX_OVERLAYS; i++ )
			{
				if ( IsShieldOverlay( GetLayerActivity( i ) ) )
				{
					startframe = GetLayerCycle( i );
				}
			}

			RemoveShieldOverlays();
		}
		else
		{
			switch ( GetActivity() )
			{
			case ACT_SHIELD_UP:
			case ACT_SHIELD_DOWN:
			case ACT_SHIELD_UP_IDLE:
			case ACT_SHIELD_ATTACK:
			//case ACT_SHIELD_KNOCKBACK:
			case ACT_CROUCHING_SHIELD_UP:
			case ACT_CROUCHING_SHIELD_DOWN:
			case ACT_CROUCHING_SHIELD_UP_IDLE:
			case ACT_CROUCHING_SHIELD_ATTACK:
			//case ACT_CROUCHING_SHIELD_KNOCKBACK:
				startframe = GetCycle();
				break;
			default:
				break;
			}
		}
	}

	// Asume we should fix up animation based on move/stationary state change
	bool fixup = true;
	// Assume no overlay
	int idx = -1;

	switch ( shieldState )
	{
	default:
	case SS_DOWN:
	case SS_UNAVAILABLE:
		RemoveShieldOverlays();
		// By default, remove shield overlays and don't do fixup
		fixup = false;
		break;
	case SS_LOWERING:
		{
			PickShieldAnimation( activity, idx, isMoving, isDucked,
				ACT_OVERLAY_SHIELD_DOWN, ACT_CROUCHING_SHIELD_DOWN, ACT_SHIELD_DOWN,
				true, 0.0f, 0.2f );
		}
		break;
	case SS_RAISING:
		{
			PickShieldAnimation( activity, idx, isMoving, isDucked,
				ACT_OVERLAY_SHIELD_UP, ACT_CROUCHING_SHIELD_UP, ACT_SHIELD_UP,
				true, 0.2f, 0.0f );
		}
		break;
	case SS_UP:
		{
			PickShieldAnimation( activity, idx, isMoving, isDucked,
				ACT_OVERLAY_SHIELD_UP_IDLE, ACT_CROUCHING_SHIELD_UP_IDLE, ACT_SHIELD_UP_IDLE,
				false );
		}
		break;
	case SS_PARRYING:
		{
			PickShieldAnimation( activity, idx, isMoving, isDucked,
				ACT_OVERLAY_SHIELD_ATTACK, ACT_CROUCHING_SHIELD_ATTACK, ACT_SHIELD_ATTACK,
				true, 0.1f, 0.1f );
		}
		break;
	}

	// If started or stopped moving and still using shield, match the cycle to/from the overlay/base animation
	//  being used beforehand
	if ( movechanged && fixup )
	{
		// Fixup overlay frame
		if ( idx != -1 )
		{
			SetLayerCycle( idx, startframe );
		}
		else
		{
			// Force animation blend
			ResetSequenceInfo();

			// Match start frame
			SetCycle( startframe );
		}
	}

	// Remember previous state
	m_bWasMoving = isMoving;

	// Return translated activity
	return activity;
}

void CBaseTFPlayer::StoreCycle( void )
{
	m_flStoredCycle = GetCycle(); // !!!!!
}

float CBaseTFPlayer::RetrieveCycle( void )
{
	return m_flStoredCycle;
}

//-----------------------------------------------------------------------------
// Purpose: Certain activities have matched cycles
// Input  : newActivity - 
//			currentActivity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::ShouldMatchCycles( Activity newActivity, Activity currentActivity )
{
	if ( ( newActivity == ACT_WALK || newActivity == ACT_RUN ) &&
		 ( currentActivity == ACT_WALK || currentActivity == ACT_RUN ) )
	{
		// Don't blend either
		IncrementInterpolationFrame();
		return true;
	}
	return false;
}

#define ARBITRARY_RUN_SPEED 75.0f

//-----------------------------------------------------------------------------
// Purpose: Set the activity based on an event or current state
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	// Assume no change
	Activity idealActivity = GetActivity();
	int animDesired = GetSequence();

	float speed = GetAbsVelocity().Length2D();

	bool isMoving = ( speed != 0.0f ) ? true : false;
	bool isRunning = false;
	
	if ( GetPlayerClass()  )
	{
// FIXME: TF2 makes no distinction between walking and running for now,
//  use the run animation always
		if ( speed > 10.0f )
		{
			isRunning = true;
		}
	}
	else
	{
		if ( speed > ARBITRARY_RUN_SPEED )
		{
			isRunning = true;
		}
	}

	bool isDucked = ( GetFlags() & FL_DUCKING ) ? true : false;
	bool isStillJumping = !( GetFlags() & FL_ONGROUND ) && ( GetActivity() == ACT_HOP );

	StoreCycle();

	// Decide upon an animation activity based upon the desired Player animation
	switch ( playerAnim )
	{
	default:
	case PLAYER_RELOAD:
	case PLAYER_ATTACK1:
	case PLAYER_IDLE:
	case PLAYER_WALK:
		// Are we still jumping?
		// If so, keep playing the jump animation.
		if ( !isStillJumping )
		{
			idealActivity = ACT_WALK;

			if ( isDucked )
			{
				idealActivity = !isMoving ? ACT_CROUCHIDLE : ACT_CROUCH;
			}
			else
			{

				if ( isRunning )
				{
					idealActivity = ACT_RUN;
				}
				else
				{
					idealActivity = isMoving ? ACT_WALK : ACT_IDLE;
				}
			}

			// Allow shield to override
			idealActivity = ShieldTranslateActivity( idealActivity );
			// Allow body yaw to override for standing and turning in place
			idealActivity = m_PlayerAnimState.BodyYawTranslateActivity( idealActivity );
		}
		break;

	case PLAYER_IN_VEHICLE:
		// For now, use manned gun pose for all vehicles
		idealActivity = ACT_RIDE_MANNED_GUN;
		break;

	case PLAYER_JUMP:
		idealActivity = ACT_HOP;
		break;

	case PLAYER_DIE:
		// Uses Ragdoll now???
		idealActivity = ACT_DIESIMPLE;
		break;

	// FIXME:  Use overlays for reload, start/leave aiming, attacking
	case PLAYER_START_AIMING:
	case PLAYER_LEAVE_AIMING:
		idealActivity = ACT_WALK;
		break;
	}

	// No change requested?
	if ( ( GetActivity() == idealActivity ) && ( GetSequence() != -1 ) )
		return;

	bool useStoredCycle = ShouldMatchCycles( idealActivity, GetActivity() );

	animDesired = SelectWeightedSequence( idealActivity );

	SetActivity( idealActivity );

	// Already using the desired animation?
	if ( GetSequence() == animDesired )
		return;

	ResetSequence( animDesired );

	// Reset to first frame of desired animation or match previous animation if activities are
	//  meant to synchronize
	SetCycle( useStoredCycle ? RetrieveCycle() : 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::CheatImpulseCommands( int iImpulse )
{
	switch(iImpulse)
	{
		case 101:
			if ( GetPlayerClass() )
			{
				GetPlayerClass()->ResupplyAmmo( 1.0f, RESUPPLY_ALL_FROM_STATION );
			}
			break;

		case 150:
			if ( GetTFTeam() )
				GetTFTeam()->PostMessage( TEAMMSG_REINFORCEMENTS_ARRIVED );
			break;

		default:
			BaseClass::CheatImpulseCommands(iImpulse);
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetRespawnStation( CBaseEntity* pRespawnStation )
{
	// This can happen because the object may get killed and its index reused
	// between time the message was sent and the 
	if( !pRespawnStation || !FClassnameIs( pRespawnStation, "obj_respawn_station" ) )
		return;

	// Team could have changed (stolen object)
	if ( GetTeam() != pRespawnStation->GetTeam() )
		return;

	// If the respawn station is the same one, then unselect!
	if ( pRespawnStation != m_hSpawnPoint )
	{
		// Make sure the respawn station is a respawn station; it could be some
		m_hSpawnPoint = pRespawnStation;
	}
	else
	{
		m_hSpawnPoint = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find a starting respawn station
//-----------------------------------------------------------------------------
CBaseEntity *CBaseTFPlayer::GetInitialSpawnPoint( void )
{
	if ( !GetTFTeam() )
		return NULL;

	CBaseEntity *pFirstStation = NULL;

	// Cycle through all the respawn stations on my team
	for ( int i = 0; i < GetTFTeam()->GetNumObjects(); i++ )
	{
		CBaseObject *pObject = GetTFTeam()->GetObject(i);
		if ( pObject->GetType() == OBJ_RESPAWN_STATION )
		{
			// Store off the first station we find
			if ( !pFirstStation )
			{
				pFirstStation = pObject;
			}

			// Map specified initial spawnpoint?
			if ( ((CObjectRespawnStation*)pObject)->IsInitialSpawnPoint() )
				return pObject;
		}
	}

	return pFirstStation;
}



CBaseEntity *FindEntityForward( CBasePlayer *pMe, bool fHull );
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::ClientCommand( const CCommand &args )
{
	if( HasClass() )
	{
		if ( GetPlayerClass()->ClientCommand( args ) )
			return true;

		const char *cmd = args[0];
		if ( FStrEq( cmd, "emp" ) )
		{
			Msg( "Self-inflicted EMP:  testing\n" );
			float flTime = 10;
			if ( args.ArgC() == 2 )
			{
				flTime = atof( args[ 1 ] );
			}

			AttemptToPowerup( POWERUP_EMP, flTime );
			return true;
		}

		if ( FStrEq( cmd, "emp_target" ) )
		{
			CBaseEntity *pEntity = FindEntityForward( this, true );
			if ( pEntity && pEntity->CanBePoweredUp() )
			{
				float flTime = 10;
				if ( args.ArgC() == 2 )
				{
					flTime = atof( args[ 1 ] );
				}
				pEntity->AttemptToPowerup( POWERUP_EMP, flTime );
			}
			return true;
		}

		if ( FStrEq( cmd, "dmg_target" ) )
		{
			CBaseEntity *pEntity = FindEntityForward( this, true );
			if ( pEntity && pEntity->m_takedamage )
			{
				float flDamage = 1;
				if ( args.ArgC() == 2 )
				{
					flDamage = atof( args[ 1 ] );
				}
				CBaseEntity *world = CBaseEntity::Instance( engine->PEntityOfEntIndex( 0 ) );
				if ( world )
				{
					pEntity->OnTakeDamage( CTakeDamageInfo( world, world, flDamage, DMG_GENERIC ) );
				}
			}
			return true;
		}
	}

	if ( !stricmp( cmd, "kd" ) )
	{
		Vector force( 0, 0, 0 );
		if ( args.ArgC() == 1 )
		{
			force.x = random->RandomFloat( 0.5, 1.0 );
			force.y = random->RandomFloat( 0.5, 1.0 );

			if ( random->RandomFloat( 0, 1 ) > 0.5 )
			{
				force.x *= -1.0f;
			}
			if ( random->RandomFloat( 0, 1 ) > 0.5 )
			{
				force.y *= -1.0f;
			}
			force.z = random->RandomFloat( 0.5, 1.0 );
		}
		else
		{
			Vector fwd;
			Vector right;
			AngleVectors( GetAbsAngles(), &fwd, &right, NULL );

			if ( !stricmp( args[ 1 ], "f" ) )
			{
				force = fwd * -1.0f;
			}
			else if ( !stricmp( args[ 1 ], "b" ) )
			{
				force = fwd;
			}
			else if ( !stricmp( args[ 1 ], "r" ) )
			{
				force = right * -1.0f;
			}			
			else if ( !stricmp( args[ 1 ], "l" ) )
			{
				force = right;
			}
			else if ( !stricmp( args[ 1 ], "fr" ) )
			{
				force = fwd * -1.0f;
				force += right * -1.0f;
			}
			else if ( !stricmp( args[ 1 ], "br" ) )
			{
				force = fwd;
				force += right * -1.0f;
			}
			else if ( !stricmp( args[ 1 ], "fl" ) )
			{
				force = fwd * -1.0f;
				force += right;
			}			
			else if ( !stricmp( args[ 1 ], "bl" ) )
			{
				force = fwd;
				force += right;
			}

			force.z = 0.8f;
			VectorNormalize( force );
		}

		KnockDownPlayer( force, 500.0f, 3.0f );
		return true;
	}

	if ( FStrEq( cmd, "veryweak" ) )
	{
		int ouch = m_iHealth - 1;

		CBaseEntity *world = CBaseEntity::Instance( engine->PEntityOfEntIndex( 0 ) );
		if ( world )
		{
			OnTakeDamage( CTakeDamageInfo( world, world, (float)ouch, DMG_GENERIC ) );
		}

		return true;
	}

	if ( FStrEq( cmd, "ragdoll" ) )
	{
		bool on = true;
		
		if ( args.ArgC() >= 2 )
		{
			on = atoi( args[ 1 ] ) ? true : false;
		}

		if ( on )
		{
			Vector force = RandomVector( -500, 500 );
			force.z = fabs( force.z );
			force.z = MIN( 200.0f, force.z );

			BecomeRagdollOnClient( force );
		}
		else
		{
			ClearClientRagdoll( true );
		}
		return true;
	}

	if ( FStrEq( cmd, "hbset" ) )
	{
		if ( args.ArgC() >= 2 )
		{
			SetHitboxSet( atoi( args[ 1 ] ) );
			Msg( "Hitboxset forced to %i %s\n", GetHitboxSet(), GetHitboxSetName() );
		}

		return true;
	}

	return BaseClass::ClientCommand( args );
}


//=========================================================
// Purpose: Override base TraceAttack
//=========================================================
void CBaseTFPlayer::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	if ( m_takedamage )
	{
		// Prevent team damage here so blood doesn't appear
		if ( info.GetAttacker() )
		{
			// Take damage from myself
			if ( InSameTeam( info.GetAttacker() ) && info.GetAttacker() != this )
				return;
		}

		// If we hit our shield, ignore the damage
		float flDamage = info.GetDamage();
		if ( IsHittingShield( vecDir, &flDamage ))
			return;

		// Shield may have blocked some
		CTakeDamageInfo subInfo = info;
		subInfo.SetDamage( flDamage );

		SetLastHitGroup( ptr->hitgroup );

		// Hit groups aren't evaluated here, like base TraceAttack.
		// Weapons factor hit location into flDamage before it gets here
/*		//SpawnBlood( ptr->endpos - (vecDir * 5), BloodColor(), subInfo.GetDamage() );
		//TraceBleed( subInfo.GetDamage(), vecDir, ptr, subInfo.GetDamageType() );

		// Show the personal shield effect.
		// What we do here is collide the trace line with an ellipse that is slightly larger
		// than the player and put the effect there.
		
		// Translate the line so the player's (and the ellipse's) center is at the origin.
		Vector vCenter = Center();
		Vector vStart = ptr->startpos - vCenter;
		Vector vEnd = ptr->endpos - vCenter;

		// Figure out the ellipse dimensions.
		Vector vDims = (WorldAlignMaxs() - WorldAlignMins()) * 0.5f;
		Vector vEllipse = vDims * 1.5;
		
		// Squash the line we're testing so we're testing against a sphere of radius 1 at the origin.
		vStart /= vEllipse;
		vEnd /= vEllipse;

		// See where the line hits the sphere.
		Vector vLineDir = vEnd - vStart;
		float f1, f2;
		if ( IntersectInfiniteRayWithSphere( vStart, vLineDir, vec3_origin, 1, &f1, &f2 ) )
		{
			// Use the closest hit point on the sphere.
			float fMin = MIN( f1, f2 );
			Vector vPos = vStart + vLineDir * fMin;

			// Unsquash back to the ellipse's dimensions.
			vPos *= vEllipse;

			ShowPersonalShieldEffect( vPos, vecDir, subInfo.GetDamage() );
		}
*/
		
		AddMultiDamage( subInfo, this );
	}
}


//-----------------------------------------------------------------------------
// Applies a force on the player when he takes damage
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ApplyDamageForce( const CTakeDamageInfo &info, int nDamageToDo )
{
	if (nDamageToDo <= 0)
		return;

	if ( (info.GetDamageType() & (DMG_ENERGYBEAM | DMG_BLAST)) == 0 )
		return;

	if ( !info.GetInflictor() || (info.GetInflictor() == this) || info.GetAttacker()->IsSolidFlagSet(FSOLID_TRIGGER) )
		return;

	// Don't blow ragdolls around
	if ( IsClientRagdoll() )
		return;

	// Don't bother with crouched players, or classes that have other rules about it
	if ( GetFlags() & FL_DUCKING )
		return;

	if (!GetPlayerClass() || !GetPlayerClass()->ShouldApplyDamageForce( info ))
		return;

	Vector vecDir;
	// If the inflictor isn't moving, use the delta between it & me. If it's moving, use it's velocity.
	Vector vecInflictorVelocity;
	info.GetInflictor()->GetVelocity( &vecInflictorVelocity, NULL );

	// Explosives never use the velocity of the inflictor
	if ( !(info.GetDamageType() & DMG_BLAST) && vecInflictorVelocity != vec3_origin )
	{
		vecDir = vecInflictorVelocity;
	}
	else
	{
		vecDir = WorldSpaceCenter( );
		vecDir -= info.GetInflictor()->WorldSpaceCenter( );
	}
	VectorNormalize( vecDir );

	float flForce = (nDamageToDo * 2) + 20;
	if (flForce > 1000.0) 
		flForce = 1000.0;

	// Escorts get knocked half as far
	if ( PlayerClass() == TFCLASS_ESCORT )
	{
		flForce *= 0.5;
	}
	
	vecDir *= flForce;

	if ( (GetMoveType() != MOVETYPE_FLY) && (GetMoveType() != MOVETYPE_FLYGRAVITY) && ((GetFlags() & FL_ONGROUND) != 0) )
	{
		// Need large x-y component to overcome walking	friction
		vecDir.x *= 3;
		vecDir.y *= 3;
	}

	Vector vecNewVelocity = GetAbsVelocity();
	vecNewVelocity += vecDir;

	Vector vecTestVel = vecNewVelocity;
	float flLen = VectorNormalize( vecTestVel );
	if (flLen > MAX_EXPLOSIVE_VELOCITY)
		VectorMultiply( vecTestVel, MAX_EXPLOSIVE_VELOCITY, vecNewVelocity );

	SetAbsVelocity( vecNewVelocity );
}


//-----------------------------------------------------------------------------
// Purpose: Deal damage to the player
//-----------------------------------------------------------------------------
int CBaseTFPlayer::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( !IsAlive() )
		return 0;

	//if ( GetFlags() & FL_GODMODE )
		//return 0;

	// Generate a global order event.
	COrderEvent_PlayerDamaged event;
	event.m_pPlayerDamaged = this;
	event.m_TakeDamageInfo = info;
	GlobalOrderEvent( &event );	

	// Don't do damage if the player's in a vehicle, in a non-damagable spot.
	if ( IsInAVehicle() && m_hVehicle.Get() )
	{
		IServerVehicle* pVehicle = m_hVehicle.Get()->GetServerVehicle();
		Assert( pVehicle );
		int nRole = pVehicle->GetPassengerRole(this);

		if( ( nRole < 0 ) 
		 || !pVehicle->IsPassengerVisible(nRole)
		 || !pVehicle->IsPassengerDamagable(nRole) )
		{
			return 0;
		}
	}

	// Check teams
	CBaseTFPlayer *pPlayer = (CBaseTFPlayer*)info.GetAttacker();
	if ( pPlayer )
	{
		// Take damage from myself
		if ( pPlayer != this )
		{
			if ( InSameTeam(pPlayer) )
			{
				return 0;
			}
			else
			{
				// Store off the last time we were damaged by an enemy so commandos can 
				// get orders to assist.
				m_flLastTimeDamagedByEnemy = gpGlobals->curtime;
			}
		}
	}

	CTakeDamageInfo subInfo = info;

	// Let the playerclass at it
	if ( GetPlayerClass()  )
	{
		subInfo.SetDamage( GetPlayerClass()->OnTakeDamage( subInfo ) );
	}

	if ( !subInfo.GetDamage() )
		return 0;

	//Msg( "Weapon did: %f\n", flDamage );

	int iDamageToDo = Ceil2Int( subInfo.GetDamage() );

	if ( !(GetFlags() & FL_GODMODE) )
	{
		// Only certain damage types knock players around
		ApplyDamageForce( info, iDamageToDo );

		m_iHealth = MAX(0, m_iHealth - iDamageToDo);
	}

	//Msg( "m_iHealth: %d\n\n", m_iHealth );

	// Dead?
	if ( m_iHealth < 1 )
	{
		Event_Killed( subInfo );
	}

	// Let the client know
	// Try and figure out where the damage is coming from
	Vector vecDamageOrigin = info.GetReportedPosition();
	// If we didn't get an origin to use, try using the attacker's origin
	if ( vecDamageOrigin == vec3_origin && info.GetAttacker() )
	{
		vecDamageOrigin = info.GetAttacker()->GetAbsOrigin();
	}

	CSingleUserRecipientFilter user( this );
	UserMessageBegin( user, "Damage" );
		WRITE_BYTE( clamp( iDamageToDo, 0, 255 ) );
		WRITE_FLOAT( vecDamageOrigin.x );	// BUG: Should be fixed point (to hud) not floats
		WRITE_FLOAT( vecDamageOrigin.y );	// BUG: However, the HUD does _not_ implement bitfield messages (yet)
		WRITE_FLOAT( vecDamageOrigin.z );	// BUG: We use WRITE_VEC3COORD for everything else
	MessageEnd();

	// Do special explosion damage effect
	if ( info.GetDamageType() & DMG_BLAST )
	{
		OnDamagedByExplosion( info );
	}

	return iDamageToDo;
}


void CBaseTFPlayer::ShowPersonalShieldEffect( 
	const Vector &vOffsetFromEnt, 
	const Vector &vIncomingDirection, 
	float flDamage )
{
	Vector vNormalized = vIncomingDirection;
	VectorNormalize( vNormalized );

	EntityMessageBegin( this );
		WRITE_BYTE( PLAYER_MSG_PERSONAL_SHIELD );
		WRITE_VEC3COORD( vOffsetFromEnt );
		WRITE_VEC3NORMAL( vNormalized );
		WRITE_SHORT( (short)flDamage );
	MessageEnd();
}


//-----------------------------------------------------------------------------
// Purpose: Player is being healed
//-----------------------------------------------------------------------------
int CBaseTFPlayer::TakeHealth( float flHealth, int bitsDamageType )
{
	if ( m_iHealth == m_iMaxHealth )
		return 0;

	// Heal the location
	float flAmountToHeal = flHealth;
	if ( flAmountToHeal > (m_iMaxHealth - m_iHealth) )
		flAmountToHeal = (m_iMaxHealth - m_iHealth);
	m_iHealth += flAmountToHeal;

	//Msg( "Health: %d\n", m_iHealth );

	return flAmountToHeal;
}


//=====================================================================
// MENU HANDLING
//=====================================================================
void CBaseTFPlayer::MenuDisplay( void )	
{
	if ( !m_pCurrentMenu )
	{
		m_MenuRefreshTime = 0;
		return;
	}

	if ( m_MenuRefreshTime > gpGlobals->curtime )
	{
		// guard against sudden clock changes
		m_MenuRefreshTime = MIN( m_MenuRefreshTime, gpGlobals->curtime + MENU_UPDATETIME );
		return;
	}

	m_MenuRefreshTime = gpGlobals->curtime + MENU_UPDATETIME;

	if ( m_pCurrentMenu )
		m_pCurrentMenu->Display( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::MenuInput( int iInput )
{
	if ( m_pCurrentMenu )
	{
		return m_pCurrentMenu->Input( this, iInput );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::MenuReset( void )
{
	CSingleUserRecipientFilter user( this );
	user.MakeReliable();

	UserMessageBegin( user, "ShowMenu" );
		WRITE_SHORT( 0 );  
		WRITE_CHAR( 0 );		// display time (-1 means unlimited)
		WRITE_BYTE( false );	// is there more message to come? no
		WRITE_STRING( "" );
	MessageEnd();

	Q_strncpy( m_MenuStringBuffer, "" ,  sizeof(m_MenuStringBuffer) );
	m_MenuRefreshTime = m_MenuDisplayTime = 0;

	m_pCurrentMenu = NULL;
};

//-----------------------------------------------------------------------------
// Purpose: Enables/disables tactical/map view for the player
// Input  : bTactical - true == enable it
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ShowTacticalView( bool bTactical )
{
	// TODO:  Decide if we are going to keep the tactical view in TF2
	if ( !inv_demo.GetBool() )
		return;

	m_bSwitchingView	= true;
	m_TFLocal.m_nInTacticalView = bTactical ? 1 : 0;
}

//-----------------------------------------------------------------------------
// returns true if we're in tactical view
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsInTacticalView( void ) const
{
	return m_TFLocal.m_nInTacticalView;
}

int CBaseTFPlayer::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

//-----------------------------------------------------------------------------
// Purpose: Note, an entity can override the send table ( e.g., to send less data or to send minimal data for
//  objects ( prob. players ) that are not in the pvs.
// Input  : **ppSendTable - 
//			*recipient - 
//			*pvs - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CBaseTFPlayer::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Don't transmit if we have no team or class
	if ((PlayerClass() == TFCLASS_UNDECIDED) || (GetTeamNumber() == 0))
		return FL_EDICT_DONTSEND;

	// Thermal vision in effect, if so, cull some players who are too far away
	CBaseTFPlayer *pPlayer = ( ( CBaseTFPlayer * )CBaseEntity::Instance( pInfo->m_pClientEnt ) );
	if ( pPlayer )
	{
		if ( pPlayer->IsUsingThermalVision() )
		{
			// Do a radius check, and force sending of guys nearby (so we can see them through walls)
			Vector dist = GetAbsOrigin() - pPlayer->GetAbsOrigin();
			if ( dist.Length() < THERMAL_VISION_RADIUS )
				return FL_EDICT_ALWAYS;
		}

		// If the player we might see is camouflaged and not on our team, we can preclude based
		//  on distance
		if ( IsCamouflaged() && !InSameTeam( pPlayer ) )
		{
			Vector dist = GetAbsOrigin() - pPlayer->GetAbsOrigin();
			if ( dist.Length() > CAMO_OUTER_RADIUS )
			{
				return FL_EDICT_ALWAYS;
			}
		}
	}

	// Use default pvs etc. rules
	return BaseClass::ShouldTransmit( pInfo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTechnologyTree *CBaseTFPlayer::GetTechTree( void )
{
	CTFTeam *pTeam = GetTFTeam();
	if ( pTeam )
		return pTeam->m_pTechnologyTree;

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseTFPlayer::PlayerClass( void )
{
	return m_iPlayerClass;
}

CPlayerClass *CBaseTFPlayer::GetPlayerClass()
{
	return m_PlayerClasses.GetPlayerClass( PlayerClass() );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetPreferredTechnology( CTechnologyTree *pTechnologyTree, int iTechIndex )
{
	Assert( pTechnologyTree );

	// Have to be on a team to vote for tech
	CTFTeam *pTeam = GetTFTeam();
	if ( !pTeam )
		return;

	if ( iTechIndex == -1 )
	{
		m_nPreferredTechnology = -1;
	}
	else
	{
		if ( iTechIndex < 0 || iTechIndex >= pTechnologyTree->GetNumberTechnologies() )
		{
			Msg( "%s tried to set voting preference to unknown technology index : %d\n",
				GetPlayerName(), iTechIndex );
			return;
		}
		CBaseTechnology *tech = pTechnologyTree->GetTechnology( iTechIndex );
		if ( !tech )
			return;

		// Has the tech got incomplete dependancies?
		if ( tech->HasInactiveDependencies() )
			return;
		// Already have it?
		if ( tech->GetAvailable() )
			return;
		// Can't prefer a hidden tech
		if ( tech->IsHidden() )
			return;

		m_nPreferredTechnology = iTechIndex;
	}

	pTeam->RecomputePreferences();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CBaseTFPlayer::GetPreferredTechnology( void )
{
	return m_nPreferredTechnology;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::HasNamedTechnology( const char *name )
{
	if ( GetTFTeam() == NULL )
		return false;

	return GetTFTeam()->HasNamedTechnology( name );
}

//-----------------------------------------------------------------------------
// Purpose: Networking is about to update this player, let it override and specify it's own pvs
// Input  : **pvs - 
//			**pas - 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize )
{
	// Normal PVS
	BaseClass::SetupVisibility( pViewEntity, pvs, pvssize );

	// PVS has an additional origin
	if ( m_vecAdditionalPVSOrigin != vec3_origin )
	{
		// Add an additional origin to the pvs
		engine->AddOriginToPVS( m_vecAdditionalPVSOrigin );
	}

	if ( m_vecCameraPVSOrigin != vec3_origin )
	{
		engine->AddOriginToPVS( m_vecCameraPVSOrigin );
	}
	
	// If in tactical mode, merge in pvs from all of our teammates, too
	// send all the others team info
	if ( m_TFLocal.m_nInTacticalView )
	{
		int i;
		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseTFPlayer *plr = ToBaseTFPlayer( UTIL_PlayerByIndex(i) );
			if ( plr && 
				( plr != this ) && 
				( plr->TeamID() == TeamID() ) )
			{
				Vector org;
				org = plr->EyePosition();

				engine->AddOriginToPVS( org );
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
// ORDERS
//-----------------------------------------------------------------------------
// Purpose: Assign the player to the specified order
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetOrder( COrder *pOrder )
{
	if ( m_hSelectedOrder.Get() && m_hSelectedOrder != pOrder )
	{
		m_hSelectedOrder->SetOwner( NULL );	
	}
	m_hSelectedOrder = pOrder;
}


int CBaseTFPlayer::GetNumResourceZoneOrders()
{
	return GetTFTeam()->CountOrders( COUNTORDERS_TYPE | COUNTORDERS_OWNER, ORDER_ATTACK, 0, this ) +
		GetTFTeam()->CountOrders( COUNTORDERS_TYPE | COUNTORDERS_OWNER, ORDER_DEFEND, 0, this ) +
		GetTFTeam()->CountOrders( COUNTORDERS_TYPE | COUNTORDERS_OWNER, ORDER_CAPTURE, 0, this );
}


void CBaseTFPlayer::KillResourceZoneOrders()
{
	if( GetNumResourceZoneOrders() )
		GetTFTeam()->RemoveOrdersToPlayer( this );
}


//--------------------------------------------------------------------------------------------------------------
// DEPLOYMENT
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::StartDeploying( void )
{
	if ( !GetPlayerClass()  )
		return;

	m_bDeploying = true;
	m_vecDeployedAngles = GetLocalAngles();

	// No pitch or roll, though
	m_vecDeployedAngles.SetX( 0 );
	m_vecDeployedAngles.SetZ( 0 );

	SetCantMove( true );
	m_flFinishedDeploying = gpGlobals->curtime + GetPlayerClass()->GetDeployTime();

	SetAnimation( PLAYER_START_AIMING );

	EmitSound( "BaseTFPlayer.StartDeploying" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::StartUnDeploying( void )
{
	if ( !GetPlayerClass()  )
		return;

	m_bUnDeploying = true;
	SetCantMove( true );
	m_flFinishedDeploying = gpGlobals->curtime + GetPlayerClass()->GetDeployTime();
	SetAnimation( PLAYER_LEAVE_AIMING );

	EmitSound( "BaseTFPlayer.StartUnDeploying" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::CheckDeployFinish( void )
{
	// Check to see if deployment has finished
	if ( m_bDeploying )
	{
		if ( gpGlobals->curtime > m_flFinishedDeploying )
		{
			FinishDeploying();
		}
		return;
	}

	// Check to see if un-deployment has finished
	if ( m_bUnDeploying )
	{
		if ( gpGlobals->curtime > m_flFinishedDeploying )
		{
			FinishUnDeploying();
		}
		return;
	}

	// Check to see if the player's trying to move while deployed
	if ( IsAlive() && m_bDeployed )
	{
		if ( m_nButtons & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT ) )
		{
			StartUnDeploying();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::FinishDeploying( void )
{
	m_bDeploying = false;
	m_bDeployed = true;
	SetCantMove( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::FinishUnDeploying( void )
{
	m_bUnDeploying = false;
	m_bDeployed = false;
	SetCantMove( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsDeployed( void )
{
	return m_bDeployed;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsDeploying( void )
{
	return (m_bDeploying || m_bUnDeploying);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsUnDeploying( void )
{
	return m_bUnDeploying;
}

void CBaseTFPlayer::OnVehicleStart()
{
	// Do any class-specific stuff
	if (GetPlayerClass())
	{
		GetPlayerClass()->OnVehicleStart();
	}

	IServerVehicle *pVehicle = GetVehicle();
	CBaseCombatWeapon *weapon = GetActiveWeapon();
	if ( pVehicle && weapon )
	{
		// Get Role for this player
		int role = pVehicle->GetPassengerRole( this );
		bool allowweapons = pVehicle->IsPassengerUsingStandardWeapons( role );
		if ( !allowweapons )
		{
			weapon->Holster();
		}
	}
}

void CBaseTFPlayer::OnVehicleEnd( Vector &playerDestPosition )
{
	// Do any class-specific stuff
	if (GetPlayerClass())
	{
		GetPlayerClass()->OnVehicleEnd();
	}

	Vector vNewPos;
	if ( !EntityPlacementTest( this, playerDestPosition, vNewPos, true) )
	{
		Warning("Can't find valid place to exit vehicle.\n");
		return;
	}

	// Move the player up a bit to be safe
	playerDestPosition = vNewPos + Vector(0,0,16);

	CBaseCombatWeapon *weapon = GetActiveWeapon();
	if ( weapon )
	{
		weapon->Deploy();
	}
}

//--------------------------------------------------------------------------------------------------------------
// Purpose:
//--------------------------------------------------------------------------------------------------------------
bool CBaseTFPlayer::CanGetInVehicle( void )
{
	// Class-specific?
	if ( GetPlayerClass() )
	{
		return GetPlayerClass()->CanGetInVehicle();
	}

	return true;
}


CVehicleTeleportStation* CBaseTFPlayer::GetSelectedMCV() const
{
	return dynamic_cast< CVehicleTeleportStation* >( m_hSelectedMCV.Get() );
}


void CBaseTFPlayer::SetSelectedMCV( CVehicleTeleportStation *pMCV )
{
	m_hSelectedMCV = pMCV;
}


//-----------------------------------------------------------------------------
// Purpose: Restore this player's ammo count to it's starting state
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::ResupplyAmmo( float flPercentage, ResupplyReason_t reason )
{
	if ( !GetPlayerClass()  )
		return false;

	return GetPlayerClass()->ResupplyAmmo(flPercentage, reason);
}


//-----------------------------------------------------------------------------
// Purpose: Medic has provided this player with a health boost
//-----------------------------------------------------------------------------
void CBaseTFPlayer::TakeHealthBoost( int iHealthBoost, int iTarget, int iDuration )
{
	m_iHealth += iHealthBoost;
	m_iHealthBoostTarget = iTarget;
	m_flHealthBoostDecrement = ceil((m_iHealth - iTarget) / (float)iDuration);

	// Start the health ticking down
	m_flHealthBoostTime = gpGlobals->curtime + 1.0;

	if ( iTarget >= m_iMaxHealth )
	{
		m_bBuffHealthBoost = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove health buffs
//-----------------------------------------------------------------------------
void CBaseTFPlayer::RemoveHealthBoost( void )
{
	m_bBuffHealthBoost = false;
	m_iHealthBoostTarget = 0;
	m_flHealthBoostTime = 0;

	if ( m_iHealth > m_iMaxHealth )
	{
		m_iHealth = m_iMaxHealth;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check the state of all buffs on this player
//-----------------------------------------------------------------------------
void CBaseTFPlayer::CheckBuffs( void )
{
	// Health boost?
	if ( m_bBuffHealthBoost )
	{
		// Dropped below normal max health?
		if ( m_iHealth <= m_iHealthBoostTarget )
		{
			RemoveHealthBoost();
		}
		else
		{
			if ( m_flHealthBoostTime < gpGlobals->curtime )
			{
				// Ticking down from a boost? or suffering poison damage?
				if ( m_iHealth > m_iMaxHealth )
				{
					// Drop back to normal health in 20 seconds
					m_iHealth -= m_flHealthBoostDecrement;
				}

				m_flHealthBoostTime = gpGlobals->curtime + 1.0;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CBaseTFPlayer::PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
	Assert( iPowerup >= 0 && iPowerup < MAX_POWERUPS );

	switch( iPowerup )
	{
	case POWERUP_BOOST:
		{
			m_hLastBoostEntity = pAttacker;

			// Power up their shield
			if ( GetCombatShield() )
			{
				GetCombatShield()->AddShieldHealth( 0.06 ); 
			}

			// Let their playerclass know
			GetPlayerClass()->PowerupStart( iPowerup, flAmount, pAttacker, pDamageModifier );
		}
		break;

	case POWERUP_EMP:
		{
			// Let the playerclass know about it
			GetPlayerClass()->PowerupStart( iPowerup, flAmount, pAttacker, pDamageModifier );
		}
		break;

	case POWERUP_RUSH:
		{
			// Speed up
			// We need to set this here so RecalculateSpeed() can check HasPowerup(POWERUP_RUSH)
			m_iPowerups |= (1 << iPowerup);
			RecalculateSpeed();
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupStart( iPowerup, flAmount, pAttacker, pDamageModifier );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CBaseTFPlayer::PowerupEnd( int iPowerup )
{
	switch( iPowerup )
	{
	case POWERUP_EMP:
		{
			GetPlayerClass()->PowerupEnd(iPowerup);
		}
		break;

	case POWERUP_RUSH:
		{
			// Slow down
			RecalculateSpeed();
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupEnd( iPowerup );
}

//--------------------------------------------------------------------------------------------------------------
// OBJECTS
//-----------------------------------------------------------------------------
// Purpose: Return true if this player's allowed to build another one of the specified object
//-----------------------------------------------------------------------------
int CBaseTFPlayer::CanBuild( int iObjectType )
{
	if ( iObjectType < 0 || iObjectType >= OBJ_LAST )
		return CB_NOT_RESEARCHED;

	if ( GetPlayerClass()  )
	{
		return GetPlayerClass()->CanBuild( iObjectType );
	}

	return CB_NOT_RESEARCHED;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of objects of the specified type that this player has
//-----------------------------------------------------------------------------
int CBaseTFPlayer::GetNumObjects( int iObjectType )
{
	int iCount = 0;
	for (int i = 0; i < GetObjectCount(); i++)
	{
		if ( !GetObject(i) )
			continue;

		if ( GetObject(i)->GetType() == iObjectType )
		{
			iCount++;
		}
	}

	return iCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseTFPlayer::GetObjectCount( void )
{
	return m_TFLocal.m_aObjects.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject	*CBaseTFPlayer::GetObject( int index )
{
	return m_TFLocal.m_aObjects[index].Get();
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this player is building something
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsBuilding( void )
{
	CWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
		return pBuilder->IsBuilding();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Object built by this player has been destroyed
//-----------------------------------------------------------------------------
void CBaseTFPlayer::OwnedObjectDestroyed( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::OwnedObjectDestroyed player %s object %p:%s\n", gpGlobals->curtime, 
		GetPlayerName(),
		pObject,
		pObject->GetClassname() ) );

	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->OwnedObjectDestroyed( pObject );
	}

	RemoveObject( pObject );

	// Tell our builder weapon so it recalculates the state of the build icons
	CWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->GainedNewTechnology( NULL );
	}
}


//-----------------------------------------------------------------------------
// Removes an object from the player
//-----------------------------------------------------------------------------
void CBaseTFPlayer::RemoveObject( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::RemoveObject %p:%s from player %s\n", gpGlobals->curtime, 
		pObject,
		pObject->GetClassname(),
		GetPlayerName() ) );

	Assert( pObject );
	for (int i = m_TFLocal.m_aObjects.Count(); --i >= 0; )
	{
		// Also, while we're at it, remove all other bogus ones too...
		if ((!m_TFLocal.m_aObjects[i].Get()) || (m_TFLocal.m_aObjects[i] == pObject))
		{
			m_TFLocal.m_aObjects.FastRemove(i);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//			*pNewOwner - 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::OwnedObjectChangeTeam( CBaseObject *pObject, CBaseTFPlayer *pNewOwner )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::OwnedObjectChangeTeam player %s object %p:%s new player %s\n", gpGlobals->curtime, 
		GetPlayerName(),
		pObject,
		pObject->GetClassname(),
		pNewOwner->GetPlayerName() ) );

	if ( pNewOwner && pNewOwner->GetPlayerClass() )
	{
		pNewOwner->GetPlayerClass()->OwnedObjectChangeFromTeam( pObject, this );
	}

	// Remove from my list of objects
	RemoveObject( pObject );

	// Add to new team
	if ( pNewOwner )
	{
		pNewOwner->AddObject( pObject );
	}

	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->OwnedObjectChangeToTeam( pObject, pNewOwner );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pZone - 
// Output : int
//-----------------------------------------------------------------------------
int CBaseTFPlayer::NumPumpsOnResourceZone( CResourceZone *pZone )
{
	int ret = 0;

	for( int iObj=0; iObj < GetObjectCount(); iObj++ )
	{
		CBaseObject *pObj = GetObject(iObj);

		if( pObj->GetType() == OBJ_RESOURCEPUMP )
		{
			CObjectResourcePump *pPump = (CObjectResourcePump*)pObj;

			// Ok, this guy already has a pump here.
			if( pPump->GetResourceZone() == pZone )
				++ret;
		}
	}

	return ret;
}



//-----------------------------------------------------------------------------
// Purpose: Remove all the player's objects
//			If bForceAll is set, remove all of them immediately.
//			Otherwise, make them all deteriorate over time.
//			If iClass is passed in, don't remove any objects that can be built 
//			by that class. If bReturnResources is set, the cost of any destroyed 
//			objects will be returned to the player.
//-----------------------------------------------------------------------------
void CBaseTFPlayer::RemoveAllObjects( bool bForceAll, int iClass, bool bReturnResources )
{
	// Remove all the player's objects
	int iSize = GetObjectCount();
	for (int i = iSize-1; i >= 0; i--)
	{
		CBaseObject *obj = GetObject(i);
		Assert( obj );
		if ( !obj )
		{
			continue;
		}

		if ( !bForceAll )
		{
			if ( iClass )
			{
				// Can our new class build this object?
				if ( ClassCanBuild( iClass, obj->GetType() ) )
					continue;
			}

			// Vehicles don't deteriorate when their owner changes teams/leaves.
			// They'll deteriorate naturally if they're unused for a while.
			if ( IsObjectAVehicle(obj->GetType()) )
			{
				RemoveObject( obj );

				// Just remove it from my list
				obj->SetBuilder( NULL );
				continue;
			}
		}

		// Return the cost of the object?
		if ( bReturnResources )
		{
			GetPlayerClass()->PickupObject( obj );
		}

		// Remove or deteriorate?
		if ( bForceAll )
		{
			UTIL_Remove( obj );
		}
		else
		{
			OwnedObjectDestroyed( obj );
			obj->StartDeteriorating();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::StopPlacement( void )
{
	// Tell our builder weapon
	CWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->StopPlacement();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Player has started building an object
//-----------------------------------------------------------------------------
int	CBaseTFPlayer::StartedBuildingObject( int iObjectType )
{
	// Tell our playerclass
	if ( GetPlayerClass()  )
		return GetPlayerClass()->StartedBuildingObject( iObjectType );

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Player has aborted building something
//-----------------------------------------------------------------------------
void CBaseTFPlayer::StoppedBuilding( int iObjectType )
{
	// Tell our playerclass
	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->StoppedBuilding( iObjectType );
	}

	// Tell our builder weapon
	CWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->StoppedBuilding( iObjectType );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Object has been built by this player
//-----------------------------------------------------------------------------
void CBaseTFPlayer::FinishedObject( CBaseObject *pObject )
{
	AddObject( pObject );

	// Tell our playerclass
	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->FinishedObject( pObject );
	}

	// Tell our builder weapon
	CWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->FinishedObject();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add the specified object to this player's object list.
//-----------------------------------------------------------------------------
void CBaseTFPlayer::AddObject( CBaseObject *pObject )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseTFPlayer::AddObject adding object %p:%s to player %s\n", gpGlobals->curtime, pObject, pObject->GetClassname(), GetPlayerName() ) );

	// Make a handle out of it
	CHandle<CBaseObject> hObject;
	hObject = pObject;

	// Make sure it's not in the list already
	bool alreadyInList = (m_TFLocal.m_aObjects.Find( hObject ) != -1);
	Assert( !alreadyInList );
	if ( !alreadyInList )
	{
		m_TFLocal.m_aObjects.AddToTail( hObject );
	}

	// Stop it deterioating, if it is
	pObject->StopDeteriorating();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetWeaponBuilder( CWeaponBuilder *pBuilder )
{
	m_hWeaponBuilder = pBuilder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponBuilder *CBaseTFPlayer::GetWeaponBuilder( void )
{
	return m_hWeaponBuilder;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *attacker - 
//			sourceDir - 
//			duration - 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::KnockDownPlayer( const Vector& sourceDir, float magnitude, float duration )
{
	// Already knocked down
	if ( m_TFLocal.m_bKnockedDown )
		return;
	// In a vehicle
	if ( IsInAVehicle() )
		return;

	m_TFLocal.m_bKnockedDown = true;

	// Randomize it a bit
	Vector jitter( 0, 0, 0 );
	//jitter.Random( -0.1, 0.1 );

	Vector dir = sourceDir + jitter;

	VectorNormalize( dir );

	Vector force = dir * magnitude;
	ApplyAbsVelocityImpulse( force );

	VectorAngles( dir, m_TFLocal.m_vecKnockDownDir.GetForModify() );

	QAngle ang = GetAbsAngles();
	Vector forward, right;
	AngleVectors( ang, &forward, &right, NULL );

	float dotFwd = dir.Dot( forward );
	float dotRight = dir.Dot( right );

	if ( dotFwd >= 0)
	{
		// if get hit from behind, pitch down a bit
		m_TFLocal.m_vecKnockDownDir.SetX( dotFwd * 20.0f );
		// look in the direction you fell
		m_TFLocal.m_vecKnockDownDir.SetZ( dotRight * 80.0f );
	}
	else
	{
		//Invert knock yaw if hit from front, so you are looking straight up at the direction
		// the hit cam efrom
		m_TFLocal.m_vecKnockDownDir += QAngle( 0, 180, 0 );
		// Look up in the air
		m_TFLocal.m_vecKnockDownDir.SetX( fabs( dotFwd ) * -60.0f );
		// And a bit to the side the hit came from
		m_TFLocal.m_vecKnockDownDir.SetZ( dotRight * 20.0f );
	}

	m_flKnockdownEndTime = gpGlobals->curtime + duration;

	// Play some kind of knockdown sound
	EmitSound( "BaseTFPlayer.KnockedDown" );

	if ( BecomeRagdollOnClient( force ) )
	{
		// We we are using ragdoll flight, then don't change underlying player
		//  velocity
		ApplyAbsVelocityImpulse( -force );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ResetKnockdown( void )
{
	// Don't get up if I'm dead
	if ( IsAlive() )
	{
		if ( !ClearClientRagdoll( true ) )
			return;
	}

	m_TFLocal.m_bKnockedDown = false;
	m_TFLocal.m_vecKnockDownDir.Init();
	m_flKnockdownEndTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsKnockedDown( void )
{
	return m_TFLocal.m_bKnockedDown;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::CheckKnockdown( void )
{
	if ( !m_TFLocal.m_bKnockedDown )
		return;

	if ( gpGlobals->curtime < m_flKnockdownEndTime )
		return;

	// Remove knockdown
	ResetKnockdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsGagged( void )
{
	return m_bGagged;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : gag - 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetGagged( bool gag )
{
	m_bGagged = gag;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::CanSpeak( void )
{
	return !IsGagged();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsUsingThermalVision( void )
{
	return m_TFLocal.m_bThermalVision;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetIDEnt( CBaseEntity *pEntity )
{
	if ( pEntity )
		m_TFLocal.m_iIDEntIndex = pEntity->entindex();
	else
		m_TFLocal.m_iIDEntIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : thermal - 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetUsingThermalVision( bool thermal )
{
	// Play sounds if we're changing
	if ( m_TFLocal.m_bThermalVision != thermal )
	{
		if ( thermal )
		{
			EmitSound( "BaseTFPlayer.ThermalOn" );
		}
		else
		{
			EmitSound( "BaseTFPlayer.ThermalOff" );
		}
	}

	m_TFLocal.m_bThermalVision = thermal;
}


//-----------------------------------------------------------------------------
// Purpose: Add the specified number of resource chunks to the player. Return true if he can carry it all.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::AddResourceChunks( int iChunks, bool bProcessed )
{
	// Am I allowed to carry any more chunks?
	int iCurrentCount = GetTotalResourceChunks();
	// Somewhat hacky
	int iIndex = GetAmmoDef()->Index("ResourceChunks");
	int iMax = GetAmmoDef()->MaxCarry( iIndex );
	if ( iCurrentCount >= iMax )
	{
		bool bSwapped = false;

		// If this is a processed chunk, see if we can swap it for an unprocessed chunk
		if ( bProcessed )
		{
			if ( m_TFLocal.m_iResourceAmmo[ NORMAL_RESOURCES ] )
			{
				// Drop this unprocessed chunk
				Vector vecVelocity = Vector( random->RandomFloat( -250,250 ), random->RandomFloat( -250,250 ), random->RandomFloat( 200,450 ) );
				CResourceChunk::Create( false, GetAbsOrigin() + Vector(0,0,32), vecVelocity );
				RemoveResourceChunks( 1, false );
				bSwapped = true;
			}
		}

		if ( !bSwapped )
			return false;
	}

	m_TFLocal.m_iResourceAmmo.Set( bProcessed, MIN( iMax, m_TFLocal.m_iResourceAmmo[ bProcessed ] + iChunks ) );
	SetAmmoCount( GetTotalResourceChunks(), iIndex );
	CPASAttenuationFilter filter( this,"BaseTFPlayer.PickupResources" );
	EmitSound( filter, entindex(),"BaseTFPlayer.PickupResources" );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Remove the specified number of resources chunks from the player.
//-----------------------------------------------------------------------------
void CBaseTFPlayer::RemoveResourceChunks( int iChunks, bool bProcessed )
{
	// Remove the amount
	m_TFLocal.m_iResourceAmmo.Set( bProcessed, MAX( 0, m_TFLocal.m_iResourceAmmo[ bProcessed ] - iChunks ) );
	int iIndex = GetAmmoDef()->Index("ResourceChunks");
	SetAmmoCount( GetTotalResourceChunks(), iIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of resource chunks of this type the player's carrying
//-----------------------------------------------------------------------------
int CBaseTFPlayer::GetResourceChunkCount( bool bProcessed )
{
	return m_TFLocal.m_iResourceAmmo[ bProcessed ];
}

//-----------------------------------------------------------------------------
// Purpose: Get the total number of resource chunks being carried by the player
//-----------------------------------------------------------------------------
int CBaseTFPlayer::GetTotalResourceChunks( void )
{
	int iCurrentCount = 0;
	for ( int i = 0; i < RESOURCE_TYPES; i++ )
	{
		iCurrentCount += m_TFLocal.m_iResourceAmmo[i];
	}

	return iCurrentCount;
}


//-----------------------------------------------------------------------------
// Purpose: Drop some resource chunks
//-----------------------------------------------------------------------------
void CBaseTFPlayer::DropAllResourceChunks( void )
{
	Vector vecOrigin = GetAbsOrigin() + Vector(0,0,32);

	TFStats()->IncrementTeamStat( GetTeamNumber(), TF_TEAM_STAT_RESOURCE_CHUNKS_DROPPED, resource_chunk_value.GetFloat() );

	// Drop a resource chunk
	Vector vecVelocity = Vector( random->RandomFloat( -250,250 ), random->RandomFloat( -250,250 ), random->RandomFloat( 200,450 ) );
	CResourceChunk *pChunk = CResourceChunk::Create( FALSE, vecOrigin, vecVelocity );
	pChunk->ChangeTeam( GetTeamNumber() );
}


//------------------------------------------------------------------------------------------------------------------
// RESOURCE BANK
//-----------------------------------------------------------------------------
// Purpose: Get the amount of a resource in this player's bank
//-----------------------------------------------------------------------------
int CBaseTFPlayer::GetBankResources( void )
{
	return m_TFLocal.ResourceCount();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetBankResources( int iAmount )
{
	int nOldAmount = m_TFLocal.ResourceCount();

	TFStats()->IncrementPlayerStat( this, TF_PLAYER_STAT_RESOURCES_ACQUIRED, iAmount - nOldAmount );

	m_TFLocal.SetResources( iAmount );

	// Tell the player's builder weapon to update
	CWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->GainedNewTechnology( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add resources to this player's Bank
//-----------------------------------------------------------------------------
void CBaseTFPlayer::AddBankResources( int iAmount )
{
	m_TFLocal.AddResources( iAmount );

	// Tell the player's builder weapon to update
	CWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->GainedNewTechnology( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove resources to this player's Bank
//-----------------------------------------------------------------------------
void CBaseTFPlayer::RemoveBankResources( int iAmount, bool bSpent )
{
	m_TFLocal.RemoveResources( iAmount );

	// Tell the player's builder weapon to update
	CWeaponBuilder *pBuilder = GetWeaponBuilder();
	if ( pBuilder )
	{
		pBuilder->GainedNewTechnology( NULL );
	}

	if (bSpent)
	{
		TFStats()->IncrementPlayerStat( this, TF_PLAYER_STAT_RESOURCES_SPENT, iAmount );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsCamouflaged( void )
{
	return ( m_flCamouflageAmount > 0.0f ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: Change state over time
//-----------------------------------------------------------------------------
void CBaseTFPlayer::CheckCamouflage( void )
{
	if ( m_flCamouflageAmount == m_flGoalCamouflageAmount )
		 return;

	float remaining = m_flGoalCamouflageAmount - m_flCamouflageAmount;
	float maxstep = m_flGoalCamouflageChangeRate * gpGlobals->frametime;

	if ( remaining > 0.0f )
	{
		m_flCamouflageAmount += MIN( remaining, maxstep );
	}
	else
	{
		remaining = -remaining;
		m_flCamouflageAmount -= MIN( remaining, maxstep );
	}

	m_flCamouflageAmount = MAX( 0.0f, m_flCamouflageAmount );
	m_flCamouflageAmount = MIN( 100.0f, m_flCamouflageAmount );
}

//-----------------------------------------------------------------------------
// Purpose: Goal % and rate in percent/second to achieve the goal
// Input  : percentage - 
//			changerate - 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetCamouflaged( int percentage, float changerate )
{
	m_flGoalCamouflageAmount = (float)percentage;
	m_flGoalCamouflageChangeRate = changerate;
}

//-----------------------------------------------------------------------------
// Purpose: Remove the player's camo
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ClearCamouflage( void )
{
	SetCamouflaged( 0, 1000 );

	// Tell the playerclass
	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->ClearCamouflage();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Confirm powerup durations
//-----------------------------------------------------------------------------
float CBaseTFPlayer::PowerupDuration( int iPowerup, float flTime )
{
	// Medics are never EMPed for long
	if ( PlayerClass() == TFCLASS_MEDIC && iPowerup == POWERUP_EMP )
		return 0.2;

	return BaseClass::PowerupDuration( iPowerup, flTime );
}

//-----------------------------------------------------------------------------
// Purpose: Return the player's anim speed multiplier. Used for speeding up viewmodels while rushed.
//-----------------------------------------------------------------------------
float CBaseTFPlayer::GetDefaultAnimSpeed( void )
{
	if ( HasPowerup( POWERUP_RUSH ) )
		return ADRENALIN_ANIM_SPEED;

	// Weapons may modify animation times
	if ( GetActiveWeapon() )
		return GetActiveWeapon()->GetDefaultAnimSpeed();

	return 1.0;
}


//-----------------------------------------------------------------------------
// Donate resources to a teammate
//-----------------------------------------------------------------------------
void CBaseTFPlayer::DonateResources( CBaseTFPlayer *pTarget, int pCount )
{
	Assert( pTarget );

	int nTotalCountDonated = 0;
	int nDonationCount = GetBankResources();
	if ( pCount < nDonationCount )
		nDonationCount = pCount;

	if (nDonationCount)
	{
		RemoveBankResources( nDonationCount, false );
		pTarget->AddBankResources( nDonationCount );
		nTotalCountDonated += nDonationCount;
	}

	if (nTotalCountDonated > 0)
	{
		char buf[1024];
		Q_snprintf( buf, sizeof( buf ), "%s has donated %d resources to you\n",
			GetPlayerName(), nTotalCountDonated );
		ClientPrint( pTarget, HUD_PRINTCENTER, buf );

		CSingleUserRecipientFilter filter( this );
		EmitSound( filter, entindex(), "BaseTFPlayer.DonateResources" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Infilitrator's can +use a corpse to consume it
//-----------------------------------------------------------------------------
void CBaseTFPlayer::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator->IsPlayer() )
	{
		CBaseTFPlayer *pPlayer = static_cast<CBaseTFPlayer*>(pActivator);

		if ( InSameTeam( pActivator ))
		{
			// Resource donation
			pPlayer->DonateResources( this, 25 );
		}
	}

	BaseClass::Use( pActivator, pCaller, useType, value );
}


//-----------------------------------------------------------------------------
// The player's usable...
//-----------------------------------------------------------------------------
int CBaseTFPlayer::ObjectCaps( void ) 
{ 
	return ( (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE ); 
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::CantMove( void )
{
	return m_bCantMove;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetCantMove( bool bCantMove )
{
	m_bCantMove = bCantMove;
	RecalculateSpeed();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ResetViewOffset( void )
{
	if ( GetPlayerClass()  )
	{
		GetPlayerClass()->ResetViewOffset();
	}
	else
	{
		SetViewOffset( VEC_VIEW_SCALED( this ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: If ragdolling, move the player along the path that the ragdoll takes
//-----------------------------------------------------------------------------
void CBaseTFPlayer::FollowClientRagdoll( void )
{
	if (( m_hRagdollShadow == NULL ) || ( GetPlayerClass() == NULL ))
		return;

	Vector vecMin, vecMax;
	GetPlayerClass()->GetPlayerHull( ( ( GetFlags() & FL_DUCKING ) == 1 ), vecMin, vecMax );

	// Follow shadow object
	trace_t tr;

	UTIL_TraceHull( 
		m_hRagdollShadow->GetAbsOrigin() + Vector(0,0,18), 
		m_hRagdollShadow->GetAbsOrigin(), 
		vecMin,
		vecMax,
		MASK_PLAYERSOLID, 
		m_hRagdollShadow, 
		COLLISION_GROUP_NONE, 
		&tr );

	// Only move if we can find a valid spot under where shadow rolled
	if ( !tr.allsolid )
	{
		UTIL_SetOrigin( this, tr.endpos );
		VectorCopy( tr.endpos, m_vecLastGoodRagdollPos );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop being a ragdoll
// Input  : moveplayertofinalspot - 
// Output : return whether or not the ragdoll was cleared
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::ClearClientRagdoll( bool moveplayertofinalspot )
{
	if ( m_hRagdollShadow )
	{
		if ( GetContainingEntity( edict() ) )
		{
			if ( moveplayertofinalspot )
			{
				// Move player to resting spot of shadow object
				FollowClientRagdoll();

				// Check for a valid standing position.  If an entity is blocking impart some
				// velocity to them and check again.
				trace_t trace;
				if ( CheckRagdollToStand( trace ) )
				{
					// Switch back to normal movement and kill off ragdoll bone setup on client
					SetMoveType( MOVETYPE_WALK );
					m_nRenderFX = kRenderFxNone;
					//RemoveSolidFlags( FSOLID_NOTSOLID );
					Assert( GetPlayerClass() != NULL );
					Vector vecMin, vecMax;
					GetPlayerClass()->GetPlayerHull( ( ( GetFlags() & FL_DUCKING ) == 1 ), vecMin, vecMax );
					UTIL_SetSize( this, vecMin, vecMax );
				}
				else
				{
					CBaseEntity *pEntity = trace.m_pEnt;
					if ( pEntity != GetContainingEntity( INDEXENT( 0 ) ) )
					{
						// Check for a physics object and apply force!
						IPhysicsObject *pPhysObject = pEntity->VPhysicsGetObject();
						if ( pPhysObject )
						{
							Vector vecDirection( random->RandomFloat( 0.0f, 1.0f ),
								random->RandomFloat( 0.0f, 1.0f ),
								random->RandomFloat( 0.0f, 1.0f ) );
							vecDirection *= 40000.0f;
							pPhysObject->ApplyForceCenter( vecDirection );
						}
						
						return false;
					}
					else
					{
						UTIL_SetOrigin( this, Vector( m_vecLastGoodRagdollPos.x, m_vecLastGoodRagdollPos.y, m_vecLastGoodRagdollPos.z + 18.0f ) );
						return false;
					}
				}
			}
		}
		// Kill the shadow object
		UTIL_Remove( m_hRagdollShadow );
		m_hRagdollShadow = NULL;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::CheckRagdollToStand( trace_t &trace )
{
	Assert( GetPlayerClass() != NULL );
	Vector vecMin, vecMax;
	GetPlayerClass()->GetPlayerHull( ( ( GetFlags() & FL_DUCKING ) == 1 ), vecMin, vecMax );

	// Write this better -- this is just a test to get things started.
	UTIL_TraceHull( 
		m_vecLastGoodRagdollPos + Vector( 0, 0, 18 ),
		m_vecLastGoodRagdollPos,
		vecMin, 
		vecMax, 
		MASK_PLAYERSOLID, 
		m_hRagdollShadow, 
		COLLISION_GROUP_NONE, 
		&trace );

	if ( !trace.allsolid )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Start being a ragdoll, creates client ragdoll object and server
//  physics shadow object
// Input  : &force - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::BecomeRagdollOnClient( const Vector &force )
{
	// Defender doesn't support it yet
	if ( PlayerClass() == TFCLASS_INFILTRATOR )
		return false;

	// Initialize the good ragdoll position.
	VectorCopy( GetAbsOrigin(), m_vecLastGoodRagdollPos );

	bool bret = BaseClass::BecomeRagdollOnClient( force );

	// ROBIN: Disabled ragdoll shadows for now.
	//		  We'll re-enable them if we need to know the end position again
	//		  If we re-enable them, we need to fix the ragdoll shadow not having the correct mass
	return bret;

	AddSolidFlags( FSOLID_NOT_SOLID );

	// Clear any old shadow object ( should never occur )
	ClearClientRagdoll( false );

	// Create new shadow object
	m_hRagdollShadow = CRagdollShadow::Create( this, force );

	return bret;
}

//=========================================================
// AddGesture - add a gesture into the animation queue
//=========================================================
int CBaseTFPlayer::AddGesture( Activity activity, bool autokill /*= true*/ )
{
	int layer = BaseClass::AddGesture( activity, autokill );
	SetLayerBlendIn( layer, 0.0 );
	SetLayerBlendOut( layer, 0.0 );
	return layer;
}

//-----------------------------------------------------------------------------
// Purpose: Class specific touch functionality!
//-----------------------------------------------------------------------------
void CBaseTFPlayer::ClassTouch( CBaseEntity *pTouched )
{
	if ( m_pfnClassTouch && HasClass() )
	{
		(GetPlayerClass()->*m_pfnClassTouch)( pTouched );
	}
}

const char* CBaseTFPlayer::GetClassModelString( int iClass, int iTeam )
{
	return m_PlayerClasses.GetPlayerClass( iClass )->GetClassModelString( iTeam );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bRampage - 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetRampage( bool bRampage )
{ 
	m_bRampage = bRampage; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseTFPlayer::IsInRampage( void ) 
{ 
	return m_bRampage; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::SetPlayerClass( TFClass iClass )
{
	if ( m_iPlayerClass != iClass )
	{
		m_Timer.End();

		if ( m_iPlayerClass >= 0 && m_iPlayerClass < TFCLASS_CLASS_COUNT )
		{
			void AddPlayerClassTime( int classnum, float seconds );

			AddPlayerClassTime( m_iPlayerClass, m_Timer.GetDuration().GetSeconds() );
		}
	}

	if ( m_iPlayerClass >= 0 )
	{
		if ( GetPlayerClass() )
		{
			GetPlayerClass()->ClassDeactivate();
		}
	}

	m_iPlayerClass = iClass;

	if ( m_iPlayerClass >= 0 )
	{
		SetPlayerModel();

		m_Timer.Start();
		
		if ( GetPlayerClass() )
		{
			GetPlayerClass()->ClassActivate();
			// Setup the class on initial spawn
			GetPlayerClass()->CreateClass();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseTFPlayer::ClassCostAdjustment( ResupplyBuyType_t nType )
{
	if ( m_iPlayerClass >= 0 )
	{
		return GetPlayerClass()->ClassCostAdjustment( nType );
	}

	return 0;
}

//=============================================================================
//
// Player Physics Shadow Code
//
class CPhysicsTFPlayerCallback : public IPhysicsPlayerControllerEvent
{
public:
	int ShouldMoveTo( IPhysicsObject *pObject, const Vector &position )
	{
		CBaseTFPlayer *pPlayer = ( CBaseTFPlayer* )pObject->GetGameData();
		if ( pPlayer->TouchedPhysics() )
		{
			return 0;
		}
		return 1;
	}
};

static CPhysicsTFPlayerCallback TFPlayerCallback;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseTFPlayer::InitVCollision( void )
{
	if ( GetPlayerClass() )
	{
		GetPlayerClass()->InitVCollision();
	}

	// Setup the HL2 specific callback.
	if ( GetPhysicsController() )
	{
		GetPhysicsController()->SetEventHandler( &TFPlayerCallback );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the entity that should receive the score
//-----------------------------------------------------------------------------
CBasePlayer *CBaseTFPlayer::GetScorer( void )
{
	return this;
}

//-----------------------------------------------------------------------------
// Purpose: Return the entity that should get assistance credit
//-----------------------------------------------------------------------------
CBasePlayer *CBaseTFPlayer::GetAssistant( void )
{
	// If I'm in a vehicle, the builder gets credit
	if ( IsInAVehicle() )
	{
		CBaseObject *pObject = dynamic_cast<CBaseObject*>( GetVehicle() );
		if ( pObject )
		{
			CBasePlayer *pBuilder = pObject->GetBuilder();
			if ( pBuilder && pBuilder != this )
				return pBuilder;
		}
	}

	// If I'm boosted, someone's getting the assist
	if ( HasPowerup( POWERUP_BOOST ) && m_hLastBoostEntity.Get() )
	{
		// I may have boosted myself
		if ( m_hLastBoostEntity.Get() != this )
		{
			if ( m_hLastBoostEntity->IsPlayer() )
				return (CBasePlayer*)m_hLastBoostEntity.Get();

			// If it's an object, give the builder the assist (i.e. buff station)
			CBaseObject *pObject = dynamic_cast<CBaseObject*>( m_hLastBoostEntity.Get() );
			if ( pObject )
			{
				CBasePlayer *pBuilder = pObject->GetBuilder();
				if ( pBuilder && pBuilder != this )
					return pBuilder;
			}
		}
	}

	return NULL;
}
