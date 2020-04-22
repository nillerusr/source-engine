//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Commando Player Class
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_class_commando.h"
#include "tf_vehicle_teleport_station.h"
#include "EntityList.h"
#include "basecombatweapon.h"
#include "weapon_builder.h"
#include "tf_obj.h"
#include "tf_obj_rallyflag.h"
#include "tf_team.h"
#include "order_assist.h"
#include "engine/IEngineSound.h"
#include "weapon_twohandedcontainer.h"
#include "weapon_combatshield.h"

ConVar tf_knockdowntime( "tf_knockdowntime", "3", FCVAR_NONE, "Length of time knocked-down players remain on the ground." );

// Adrenalin
ConVar	class_commando_speed( "class_commando_speed","200", FCVAR_NONE, "Commando movement speed." );
ConVar	class_commando_rush_length( "class_commando_rush_length","10", FCVAR_NONE, "Commando's adrenalin rush length in seconds." );
ConVar	class_commando_rush_recharge( "class_commando_rush_recharge","60", FCVAR_NONE, "Commando's adrenalin rush recharge time in seconds." );

ConVar	class_commando_battlecry_radius( "class_commando_battlecry_radius","512", FCVAR_NONE, "Commando's battlecry radius." );
ConVar	class_commando_battlecry_length( "class_commando_battlecry_length","10", FCVAR_NONE, "Length of adrenalin rush given by the Commando's battlecry in seconds." );

//=============================================================================
//
// Commando Data Table
//
BEGIN_SEND_TABLE_NOBASE( CPlayerClassCommando, DT_PlayerClassCommandoData )
	SendPropInt		( SENDINFO_STRUCTELEM( m_ClassData.m_bCanBullRush ), 1, SPROP_UNSIGNED ),
	SendPropInt		( SENDINFO_STRUCTELEM( m_ClassData.m_bBullRush ), 1, SPROP_UNSIGNED ),
	SendPropVector	( SENDINFO_STRUCTELEM( m_ClassData.m_vecBullRushDir ), -1, SPROP_COORD ),
	SendPropVector	( SENDINFO_STRUCTELEM( m_ClassData.m_vecBullRushViewDir ), -1, SPROP_COORD ),
	SendPropVector	( SENDINFO_STRUCTELEM( m_ClassData.m_vecBullRushViewGoalDir ), -1, SPROP_COORD ),
	SendPropFloat	( SENDINFO_STRUCTELEM( m_ClassData.m_flBullRushTime ), 32, SPROP_NOSCALE ),
	SendPropFloat	( SENDINFO_STRUCTELEM( m_ClassData.m_flDoubleTapForwardTime ), 32, SPROP_NOSCALE ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CPlayerClassCommando::GetClassModelString( int nTeam )
{
	if (nTeam == TEAM_HUMANS)
		return "models/player/human_commando.mdl";
	else
		return "models/player/alien_commando.mdl";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassCommando::CPlayerClassCommando( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassCommando::~CPlayerClassCommando()
{
	m_aHitPlayers.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::ClassActivate( void )
{
	BaseClass::ClassActivate();

	// Setup movement data.
	SetupMoveData();

	// Initialize the shared class data.
	m_ClassData.m_bCanBullRush = false;
	m_ClassData.m_bBullRush = false;
	m_ClassData.m_vecBullRushDir.Init();
	m_ClassData.m_vecBullRushViewDir.Init();
	m_ClassData.m_vecBullRushViewGoalDir.Init();
	m_ClassData.m_flBullRushTime = COMMANDO_TIME_INVALID;
	m_ClassData.m_flDoubleTapForwardTime = COMMANDO_TIME_INVALID;

	m_bCanRush = false;
	m_bPersonalRush = false;
	m_bHasBattlecry = false;
	m_bCanBoot = false;
	m_flNextBootCheck = 0.0f;		// Time at which to recheck for the automatic melee attack
	m_bOldBullRush = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::ClassDeactivate( void )
{
	m_hWpnShield = NULL;
	m_hWpnPlasma = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::CreateClass( void )
{
	BaseClass::CreateClass();

	// Create our two handed weapon layout
	m_hWpnShield = m_pPlayer->GetCombatShield();

	CWeaponTwoHandedContainer *p = ( CWeaponTwoHandedContainer * )m_pPlayer->Weapon_OwnsThisType( "weapon_twohandedcontainer" );
	if ( !p )
	{
		p = static_cast< CWeaponTwoHandedContainer * >( m_pPlayer->GiveNamedItem( "weapon_twohandedcontainer" ) );
	}

	if ( p && m_hWpnShield.Get() )
	{
		m_hWpnShield->SetReflectViewModelAnimations( true );
		p->SetWeapons( NULL, m_hWpnShield );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::RespawnClass( void )
{
	BaseClass::RespawnClass();

	m_flNextBootCheck = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Supply the player with Ammo. Return true if some ammo was given.
//-----------------------------------------------------------------------------
bool CPlayerClassCommando::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_GRENADES_FROM_STATION))
	{
		if (ResupplyAmmoType( 3 * flFraction, "Grenades" ))
			bGiven = true;
		if (ResupplyAmmoType( 1, "RallyFlags" ))
			bGiven = true;
		if (ResupplyAmmoType( 3, "Rockets" ))
			bGiven = true;
	}

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
		if (ResupplyAmmoType( 3, "Rockets" ))
			bGiven = true;
	}

	// On respawn, resupply base weapon ammo
	if ( reason == RESUPPLY_RESPAWN )
	{
	}

	if ( BaseClass::ResupplyAmmo(flFraction, reason) )
		bGiven = true;

	return bGiven;
}


//-----------------------------------------------------------------------------
// Purpose: Set commando class specific movement data here.
//-----------------------------------------------------------------------------
void CPlayerClassCommando::SetupMoveData( void )
{
	// Setup Class statistics
	m_flMaxWalkingSpeed = class_commando_speed.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassCommando::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( COMMANDOCLASS_HULL_STAND_MIN, COMMANDOCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( COMMANDOCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = COMMANDOCLASS_STEPSIZE;	
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player's allowed to build another one of the specified objects
//-----------------------------------------------------------------------------
int CPlayerClassCommando::CanBuild( int iObjectType )
{
	if ( iObjectType == OBJ_RALLYFLAG )
	{
		if ( !m_pPlayer->HasNamedTechnology( "com_obj_rallyflag" ) )
			return CB_NOT_RESEARCHED;
	}

	return BaseClass::CanBuild( iObjectType );
}

//-----------------------------------------------------------------------------
// Purpose: Object has been built by this player
//-----------------------------------------------------------------------------
void CPlayerClassCommando::FinishedObject( CBaseObject *pObject )
{
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the Commando's Adrenalin Rush from available technologies
//-----------------------------------------------------------------------------
void CPlayerClassCommando::CalculateRush( void )
{
	// Adrenalin Rush
	if ( m_pPlayer->HasNamedTechnology( "com_adrenalin_rush" ) )
	{
		m_bCanRush = true;
	}
	else
	{
		m_bCanRush = false;
	}

	// Battlecry
	m_bHasBattlecry = m_pPlayer->HasNamedTechnology( "com_adrenalin_battlecry" );

	// Boot
	// ROBIN: Removed for now
	m_bCanBoot = false;//m_pPlayer->HasNamedTechnology( "com_automatic_boot" );

	// Killing Rush
	m_pPlayer->SetRampage( m_pPlayer->HasNamedTechnology( "com_adrenalin_rampage" ) );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the player is bullrushing.
//-----------------------------------------------------------------------------
bool CPlayerClassCommando::InBullRush( void )
{
	return m_ClassData.m_bBullRush;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the player's able to bull rush
//-----------------------------------------------------------------------------
bool CPlayerClassCommando::CanBullRush( void )
{
	return m_ClassData.m_bCanBullRush;
}


//-----------------------------------------------------------------------------
// Should we take damage-based force?
//-----------------------------------------------------------------------------
bool CPlayerClassCommando::ShouldApplyDamageForce( const CTakeDamageInfo &info )
{
	return !InBullRush();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassCommando::BullRushTouch( CBaseEntity *pTouched )
{
	if ( pTouched->IsPlayer() && !pTouched->InSameTeam( m_pPlayer ) )
	{
		// Get the player.
		CBaseTFPlayer *pTFPlayer = ( CBaseTFPlayer* )pTouched;

		// Check to see if we have "touched" this player already this bullrush cycle.
		if ( m_aHitPlayers.Find( pTFPlayer ) != -1 )
			return;

		// Hitting the player now.
		m_aHitPlayers.AddToTail( pTFPlayer );

		// ROBIN: Bullrush now instantly kills again
		float flDamage = 200;
		// Calculate the damage a player takes based on distance(time).
		//float flDamage = 1.0f - ( ( COMMANDO_BULLRUSH_TIME - m_ClassData.m_flBullRushTime ) * ( 1.0f / COMMANDO_BULLRUSH_TIME ) );
		//flDamage *= 115.0f; // max bullrush damage

		const trace_t &tr = m_pPlayer->GetTouchTrace();
		CTakeDamageInfo info( m_pPlayer, m_pPlayer, flDamage, DMG_CLUB, DMG_KILL_BULLRUSH );
		CalculateMeleeDamageForce( &info, (tr.endpos - tr.startpos), tr.endpos );
		pTFPlayer->TakeDamage( info );

		CPASAttenuationFilter filter( m_pPlayer, "Commando.BullRushFlesh" );
		CBaseEntity::EmitSound( filter, m_pPlayer->entindex(), "Commando.BullRushFlesh" );

		pTFPlayer->Touch( m_pPlayer ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: New technology has been gained. Recalculate any class specific technology dependencies.
//-----------------------------------------------------------------------------
void CPlayerClassCommando::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	// Technology handling
	CalculateRush();

	BaseClass::GainedNewTechnology( pTechnology );
}

//-----------------------------------------------------------------------------
// Purpose: Called when we are about to bullrush.
//-----------------------------------------------------------------------------
void CPlayerClassCommando::PreBullRush( void )
{
	// Set the touch function to look for collisions!
	SetClassTouch( m_pPlayer, BullRushTouch );

	// Clear the player hit list.
	m_aHitPlayers.RemoveAll();

	// Start the bull rush sound.
	CPASAttenuationFilter filter( m_pPlayer, "Commando.BullRushScream" );
	filter.MakeReliable();
	CBaseEntity::EmitSound( filter, m_pPlayer->entindex(), "Commando.BullRushScream" );

	// Force the shield down, if it is up.
	CWeaponTwoHandedContainer *pContainer = dynamic_cast<CWeaponTwoHandedContainer*>( m_pPlayer->GetActiveWeapon() );
	if ( pContainer )
	{
		CWeaponCombatShield *pShield = dynamic_cast<CWeaponCombatShield*>( pContainer->GetRightWeapon() );
		if ( pShield )
		{
			pShield->SetShieldUsable( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when we finish bullrushing.
//-----------------------------------------------------------------------------
void CPlayerClassCommando::PostBullRush( void )
{
	SetClassTouch( m_pPlayer, NULL );

	// Force the shield down, if it is up.
	CWeaponTwoHandedContainer *pContainer = dynamic_cast<CWeaponTwoHandedContainer*>( m_pPlayer->GetActiveWeapon() );
	if ( pContainer )
	{
		CWeaponCombatShield *pShield = dynamic_cast<CWeaponCombatShield*>( pContainer->GetRightWeapon() );
		if ( pShield )
		{
			pShield->SetShieldUsable( true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame by postthink
//-----------------------------------------------------------------------------
void CPlayerClassCommando::ClassThink( void )
{
	// Check bullrush
	m_ClassData.m_bCanBullRush = true;

	// Do the init thing here!
	if ( m_bOldBullRush != m_ClassData.m_bBullRush )
	{
		if ( m_ClassData.m_bBullRush )
		{
			PreBullRush();
		}
		else
		{
			PostBullRush();
		}

		m_bOldBullRush = (bool)m_ClassData.m_bBullRush;
	} 

	// Check for melee attack
	if ( m_bCanBoot && m_pPlayer->IsAlive() && m_flNextBootCheck < gpGlobals->curtime )
	{
		m_flNextBootCheck = gpGlobals->curtime + 0.2;

		CBaseEntity *pEntity = NULL;
		Vector vecSrc	 = m_pPlayer->Weapon_ShootPosition( );
		Vector vecDir	 = m_pPlayer->BodyDirection2D( );
		Vector vecTarget = vecSrc + (vecDir * 48);
		for ( CEntitySphereQuery sphere( vecTarget, 16 ); pEntity = sphere.GetCurrentEntity(); sphere.NextEntity() )
		{
			if ( pEntity->IsPlayer() && (pEntity != m_pPlayer) )
			{
				CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pEntity;
				// Target needs to be on the enemy team
				if ( !pPlayer->IsClass( TFCLASS_UNDECIDED ) && pPlayer->IsAlive() && pPlayer->InSameTeam( m_pPlayer ) == false )
				{
					Boot( pPlayer );
					m_flNextBootCheck = gpGlobals->curtime + 1.5;
				}
			}
		}
	}

	BaseClass::ClassThink();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::StartAdrenalinRush( void )
{
	// Am I actually alive?
	if ( !m_pPlayer->IsAlive() )
		return;

	// Do I have rush capability?
	if ( !m_bCanRush )
		return;

	m_bPersonalRush = true;

	// Start adrenalin rushing
	m_pPlayer->AttemptToPowerup( POWERUP_RUSH, class_commando_rush_length.GetFloat() );

	// If I have battlecry, adrenalin up all my nearby teammates
	if ( m_bHasBattlecry )
	{
		// Find nearby teammates
		for ( int i = 0; i < m_pPlayer->GetTFTeam()->GetNumPlayers(); i++ )
		{
			CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)m_pPlayer->GetTFTeam()->GetPlayer(i);
			assert(pPlayer);

			// Is it within range?
			if ( pPlayer != m_pPlayer && (pPlayer->GetAbsOrigin() - m_pPlayer->GetAbsOrigin()).Length() < class_commando_battlecry_radius.GetFloat() )
			{
				// Can I see it?
				trace_t tr;
				UTIL_TraceLine( m_pPlayer->EyePosition(), pPlayer->EyePosition(), MASK_SOLID_BRUSHONLY, m_pPlayer, COLLISION_GROUP_NONE, &tr);
				CBaseEntity *pEntity = tr.m_pEnt;
				if ( (tr.fraction == 1.0) || ( pEntity == pPlayer ) )
				{
					pPlayer->AttemptToPowerup( POWERUP_RUSH, class_commando_battlecry_length.GetFloat() );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Automatic Melee Attack
//-----------------------------------------------------------------------------
void CPlayerClassCommando::Boot( CBaseTFPlayer *pTarget )
{
	CPASAttenuationFilter filter( m_pPlayer, "Commando.BootSwing" );

	CBaseEntity::EmitSound( filter, m_pPlayer->entindex(), "Commando.BootSwing" );
	CBaseEntity::EmitSound( filter, m_pPlayer->entindex(), "Commando.BootHit" );

	// Damage the target
	CTakeDamageInfo info( m_pPlayer, m_pPlayer, 25, DMG_CLUB );
	CalculateMeleeDamageForce( &info, (pTarget->GetAbsOrigin() - m_pPlayer->GetAbsOrigin()), pTarget->GetAbsOrigin() );
	pTarget->TakeDamage( info );

	Vector vecForward;
	AngleVectors( m_pPlayer->GetLocalAngles(), &vecForward );
	// Give it a lot of "in the air"
	vecForward.z = MAX( 0.8, vecForward.z );
	VectorNormalize( vecForward );

	// Knock the target to the ground for a few seconds (use default duration)
	pTarget->KnockDownPlayer( vecForward, 500.0f, tf_knockdowntime.GetFloat() );

}

//-----------------------------------------------------------------------------
// Purpose: Handle custom commands for this playerclass
//-----------------------------------------------------------------------------
bool CPlayerClassCommando::ClientCommand( const char *pcmd )
{
	return BaseClass::ClientCommand( pcmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::SetPlayerHull( void )
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( COMMANDOCLASS_HULL_DUCK_MIN, COMMANDOCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( COMMANDOCLASS_HULL_STAND_MIN, COMMANDOCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax )
{
	if ( bDucking )
	{
		VectorCopy( COMMANDOCLASS_HULL_DUCK_MIN, vecMin );
		VectorCopy( COMMANDOCLASS_HULL_DUCK_MAX, vecMax );
	}
	else
	{
		VectorCopy( COMMANDOCLASS_HULL_STAND_MIN, vecMin );
		VectorCopy( COMMANDOCLASS_HULL_STAND_MAX, vecMax );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::CreatePersonalOrder( void )
{
	if ( CreateInitialOrder() )
		return;

	if ( COrderAssist::CreateOrder( this ) )
		return;

	BaseClass::CreatePersonalOrder();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( COMMANDOCLASS_VIEWOFFSET_STAND );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassCommando::InitVCollision( void )
{
	CPhysCollide *pStandModel = PhysCreateBbox( COMMANDOCLASS_HULL_STAND_MIN, COMMANDOCLASS_HULL_STAND_MAX );
	CPhysCollide *pCrouchModel = PhysCreateBbox( COMMANDOCLASS_HULL_DUCK_MIN, COMMANDOCLASS_HULL_DUCK_MAX );
	m_pPlayer->SetupVPhysicsShadow( pStandModel, "tfplayer_commando_stand", pCrouchModel, "tfplayer_commando_crouch" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassCommando::CanGetInVehicle( void )
{
	if ( InBullRush() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPlayerClassCommando::ClassCostAdjustment( ResupplyBuyType_t nType )
{
	int nCost = 0;
	if ( nType != RESUPPLY_BUY_HEALTH )
	{
			nCost = RESUPPLY_ROCKET_COST;
	}

	return nCost;
}
