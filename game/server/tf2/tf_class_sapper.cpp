//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Sapper Player Class
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_class_sapper.h"
#include "basecombatweapon.h"
#include "weapon_builder.h"
#include "tf_team.h"
#include "entitylist.h"
#include "tf_obj.h"
#include "weapon_mortar.h"
#include "orders.h"
#include "engine/IEngineSound.h"
#include "weapon_twohandedcontainer.h"
#include "weapon_combatshield.h"
#include "tf_vehicle_teleport_station.h"
#define EMP_RADIUS_NORMAL	( 512.0f )
#define EMP_RADIUS_MEDIUM	( 1024.0f )
#define EMP_RADIUS_HUGE		( 4096.0f )

#define EMP_RECHARGETIME_NORMAL	( 20.0f )
#define EMP_RECHARGETIME_FAST	( 15.0f )

#define EMP_EFFECTTIME_NORMAL	( 10.0f )
#define EMP_EFFECTTIME_LONG		( 15.0f )

ConVar	class_sapper_speed( "class_sapper_speed","225", FCVAR_NONE, "Sapper movement speed" );
ConVar	class_sapper_boost_amount( "class_sapper_boost_amount","35", FCVAR_NONE, "Amount of energy drained for the Sapper to get a boost." );
ConVar	class_sapper_boost_time( "class_sapper_boost_time","15", FCVAR_NONE, "Sapper's boost time after draining a full charge of boost." );
ConVar	class_sapper_boost_rate( "class_sapper_boost_rate","1", FCVAR_NONE, "Sapper's boost rate on his self-boost." );
ConVar	class_sapper_damage_modifier( "class_sapper_damage_modifier", "1.5", 0, "Scales the damage a Sapper does while he's self-boosted." );

BEGIN_SEND_TABLE_NOBASE( CPlayerClassSapper, DT_PlayerClassSapperData )
	SendPropFloat( SENDINFO( m_flDrainedEnergy ),	8,	SPROP_UNSIGNED,	0.0f,	1.0f ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CPlayerClassSapper::GetClassModelString( int nTeam )
{
	static const char *string = "models/player/technician.mdl";
	return string;
}

//-----------------------------------------------------------------------------
// Purpose: Sapper
//-----------------------------------------------------------------------------
CPlayerClassSapper::CPlayerClassSapper( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}

	// Setup movement data.
	SetupMoveData();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassSapper::~CPlayerClassSapper()
{
}

//-----------------------------------------------------------------------------
// Purpose: Any objects created/owned by class should be allocated and destroyed here
//-----------------------------------------------------------------------------
void CPlayerClassSapper::ClassActivate( void )
{
	BaseClass::ClassActivate();

	m_bHasMediumRangeEMP = false;
	m_bHasFasterRechargingEMP = false;
	m_bHasHugeRadiusEMP = false;
	m_bHasLongerLastingEMPEffect = false;

	m_vecLastOrigin.Init();
	m_flLastMoveTime = 0.0f;

	memset( &m_ClassData, 0, sizeof( m_ClassData ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSapper::ClassDeactivate( void )
{
	m_DamageModifier.RemoveModifier();
	BaseClass::ClassDeactivate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSapper::CreateClass( void )
{
	BaseClass::CreateClass();

	// Create our two handed weapon layout
	m_hWpnPlasma = static_cast< CBaseTFCombatWeapon * >( m_pPlayer->GiveNamedItem( "weapon_combat_shotgun" ) );
	m_hWpnShield = m_pPlayer->GetCombatShield();
	CWeaponTwoHandedContainer *p = ( CWeaponTwoHandedContainer * )m_pPlayer->Weapon_OwnsThisType( "weapon_twohandedcontainer" );
	if ( !p )
	{
		p = static_cast< CWeaponTwoHandedContainer * >( m_pPlayer->GiveNamedItem( "weapon_twohandedcontainer" ) );
	}
	if ( p && m_hWpnShield.Get() && m_hWpnPlasma.Get() )
	{
		m_hWpnShield->SetReflectViewModelAnimations( true );
		p->SetWeapons( m_hWpnPlasma, m_hWpnShield );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSapper::RespawnClass( void )
{
	BaseClass::RespawnClass();

	// Reset time of last move
	m_vecLastOrigin.Init();
	m_flLastMoveTime = 0.0f;
	m_flDrainedEnergy = 0;
	m_flBoostEndTime = 0;
	m_DamageModifier.SetModifier( class_sapper_damage_modifier.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassSapper::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;
	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_GRENADES_FROM_STATION))
	{
		if (ResupplyAmmoType( 3 * flFraction, "Grenades" ))
			bGiven = true;
	}

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
	}

	// On respawn, resupply base weapon ammo
	if ( reason == RESUPPLY_RESPAWN )
	{
	}

	if ( BaseClass::ResupplyAmmo(flFraction,reason) )
		bGiven = true;
	return bGiven;
}

//-----------------------------------------------------------------------------
// Purpose: Set sapper class specific movement data here.
//-----------------------------------------------------------------------------
void CPlayerClassSapper::SetupMoveData( void )
{
	// Setup Class statistics
	m_flMaxWalkingSpeed = class_sapper_speed.GetFloat();
	m_vecLastOrigin.Init();
	m_flLastMoveTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassSapper::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( SAPPERCLASS_HULL_STAND_MIN, SAPPERCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( SAPPERCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = SAPPERCLASS_STEPSIZE;	
}

//-----------------------------------------------------------------------------
// Purpose: New technology has been gained. Recalculate any class specific technology dependencies.
//-----------------------------------------------------------------------------
void CPlayerClassSapper::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	m_bHasMediumRangeEMP = false;
	m_bHasFasterRechargingEMP = false;
	m_bHasHugeRadiusEMP = false;
	m_bHasLongerLastingEMPEffect = false;

	if ( m_pPlayer->HasNamedTechnology( "emp1" ) )
	{
		m_bHasFasterRechargingEMP = true;
	}

	if ( m_pPlayer->HasNamedTechnology( "emp3" ) )
	{
		m_bHasMediumRangeEMP = true;
	}

	if ( m_pPlayer->HasNamedTechnology( "emp4" ) )
	{
		m_bHasLongerLastingEMPEffect = true;
	}

	if ( m_pPlayer->HasNamedTechnology( "emp5" ) )
	{
		m_bHasHugeRadiusEMP = true;
	}

	BaseClass::GainedNewTechnology( pTechnology );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSapper::ClassThink( void )
{
	if ( m_pPlayer->GetLocalOrigin() != m_vecLastOrigin )
	{
		m_vecLastOrigin = m_pPlayer->GetLocalOrigin();
		m_flLastMoveTime = gpGlobals->curtime;
	}

	// Am I boosting myself?
	if ( m_pPlayer->IsAlive() && (m_flBoostEndTime > gpGlobals->curtime) )
	{
		m_pPlayer->AttemptToPowerup( POWERUP_BOOST, class_sapper_boost_time.GetFloat(), class_sapper_boost_rate.GetFloat() * 0.1, m_pPlayer, &m_DamageModifier );
	}
	else
	{
		m_DamageModifier.RemoveModifier();
	}

	BaseClass::ClassThink();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassSapper::CheckStationaryTime( float time_required )
{
	// Has enough time passed?
	if ( gpGlobals->curtime >= m_flLastMoveTime + time_required )
	{
		return true;
	}

	// Not enough time yet
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSapper::CreatePersonalOrder( void )
{
	if ( CreateInitialOrder() )
		return;

	BaseClass::CreatePersonalOrder();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSapper::SetPlayerHull( void )
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( SAPPERCLASS_HULL_DUCK_MIN, SAPPERCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( SAPPERCLASS_HULL_STAND_MIN, SAPPERCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSapper::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( SAPPERCLASS_VIEWOFFSET_STAND );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSapper::AddDrainedEnergy( float flEnergy )
{
	// Convert to 0->1
	flEnergy = ( flEnergy / class_sapper_boost_amount.GetFloat() ); 
	m_flDrainedEnergy = MIN( 1.0, m_flDrainedEnergy + flEnergy );

	// Did we hit max?
	if ( m_flDrainedEnergy >= 1.0 )
	{
		m_flDrainedEnergy = 0;

		// Boost the player
		m_flBoostEndTime = gpGlobals->curtime + class_sapper_boost_time.GetFloat();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CPlayerClassSapper::GetDrainedEnergy( void )
{
	return m_flDrainedEnergy;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassSapper::DeductDrainedEnergy( float flEnergy )
{
	m_flDrainedEnergy = MAX( 0, m_flDrainedEnergy - flEnergy );
}
