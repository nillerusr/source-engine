//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The Escort Player Class
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "tf_player.h"
#include "tf_class_escort.h"
#include "basecombatweapon.h"
#include "tf_obj_shieldwall.h"
#include "weapon_builder.h"
#include "tf_shareddefs.h"
#include "tf_team.h"
#include "orders.h"
#include "order_buildshieldwall.h"
#include "order_mortar_attack.h"
#include "weapon_twohandedcontainer.h"
#include "tf_shield.h"
#include "iservervehicle.h"
#include "weapon_combatshield.h"
#include "in_buttons.h"
#include "tf_vehicle_teleport_station.h"
#include "weapon_shield.h"


ConVar	class_escort_speed( "class_escort_speed","200", FCVAR_NONE, "Escort movement speed" );


//=============================================================================
//
// Escort Data Table
//
BEGIN_SEND_TABLE_NOBASE( CPlayerClassEscort, DT_PlayerClassEscortData )
END_SEND_TABLE()



bool OrderCreator_Mortar( CPlayerClassEscort *pClass )
{
	return COrderMortarAttack::CreateOrder( pClass );
}


bool OrderCreator_ShieldWall( CPlayerClassEscort *pClass )
{
	return COrderBuildShieldWall::CreateOrder( pClass );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CPlayerClassEscort::GetClassModelString( int nTeam )
{
	static const char *string = "models/player/alien_escort.mdl";
	return string;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassEscort::CPlayerClassEscort( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerClassEscort::SetupSizeData( void )
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( ESCORTCLASS_HULL_STAND_MIN, ESCORTCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( ESCORTCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = ESCORTCLASS_STEPSIZE;	
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerClassEscort::~CPlayerClassEscort()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassEscort::ClassActivate( void )
{
	BaseClass::ClassActivate();

	// Setup movement data.
	m_hWeaponProjectedShield = NULL;
	SetupMoveData();
	memset( &m_ClassData, 0, sizeof( m_ClassData ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponShield *CPlayerClassEscort::GetProjectedShield( void )
{
	if ( !m_hWeaponProjectedShield )
	{
		m_hWeaponProjectedShield = static_cast< CWeaponShield * >( m_pPlayer->Weapon_OwnsThisType( "weapon_shield" ) );
		if ( !m_hWeaponProjectedShield )
		{
			m_hWeaponProjectedShield = static_cast< CWeaponShield * >( m_pPlayer->GiveNamedItem( "weapon_shield" ) );
		}
	}

	if ( m_hWeaponProjectedShield )
	{
 		m_hWeaponProjectedShield->SetReflectViewModelAnimations( true );
	}

	return m_hWeaponProjectedShield;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassEscort::CreateClass( void )
{
	BaseClass::CreateClass();

	CWeaponTwoHandedContainer *p = ( CWeaponTwoHandedContainer * )m_pPlayer->Weapon_OwnsThisType( "weapon_twohandedcontainer" );
	if ( !p )
	{
		p = static_cast< CWeaponTwoHandedContainer * >( m_pPlayer->GiveNamedItem( "weapon_twohandedcontainer" ) );
	}

	CWeaponShield *pShield = GetProjectedShield();
	if ( p && pShield )
	{
		p->SetWeapons( NULL, pShield );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassEscort::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;
	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
		if ( ResupplyAmmoType( 200 * flFraction, "Bullets" ) )
			bGiven = true;
	}

	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_GRENADES_FROM_STATION))
	{
		if (ResupplyAmmoType( 5 * flFraction, "ShieldGrenades" ))
			bGiven = true;
	}

	// On respawn, resupply base weapon ammo
	if ( reason == RESUPPLY_RESPAWN )
	{
		if ( ResupplyAmmoType( 200, "Bullets" ) )
			bGiven = true;
	}

	if ( BaseClass::ResupplyAmmo(flFraction, reason) )
		bGiven = true;

	return bGiven;
}


//-----------------------------------------------------------------------------
// Purpose: Set escort class specific movement data here.
//-----------------------------------------------------------------------------
void CPlayerClassEscort::SetupMoveData( void )
{
	// Setup Class statistics
	m_flMaxWalkingSpeed = class_escort_speed.GetFloat();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassEscort::SetPlayerHull( void )
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( ESCORTCLASS_HULL_DUCK_MIN, ESCORTCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( ESCORTCLASS_HULL_STAND_MIN, ESCORTCLASS_HULL_STAND_MAX );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassEscort::ResetViewOffset( void )
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( ESCORTCLASS_VIEWOFFSET_STAND );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassEscort::PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
	/*
	switch( iPowerup )
	{
	case POWERUP_BOOST:
		// Increase our shield's energy
		if ( m_hDeployedShield )
		{
			m_hDeployedShield->SetPower( m_hDeployedShield->GetPower() + (flAmount * 10) );
		}
		break;

	case POWERUP_EMP:
		if ( m_hDeployedShield )
		{
			m_hDeployedShield->SetEMPed( true );
		}
		break;

	default:
		break;
	}
	*/

	BaseClass::PowerupStart( iPowerup, flAmount, pAttacker, pDamageModifier );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CPlayerClassEscort::PowerupEnd( int iPowerup )
{
	/*
	switch( iPowerup )
	{
	case POWERUP_EMP:
		if ( m_hDeployedShield )
		{
			m_hDeployedShield->SetEMPed( false );
		}
		break;

	default:
		break;
	}
	*/

	BaseClass::PowerupEnd( iPowerup );
}
