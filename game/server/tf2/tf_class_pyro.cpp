//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basecombatweapon.h"
#include "tf_player.h"
#include "tf_class_pyro.h"
#include "weapon_twohandedcontainer.h"
#include "weapon_gas_can.h"
#include "weapon_flame_thrower.h"
#include "gasoline_shared.h"
#include "weapon_combatshield.h"

BEGIN_SEND_TABLE_NOBASE( CPlayerClassPyro, DT_PlayerClassPyroData )
END_SEND_TABLE()


CPlayerClassPyro::CPlayerClassPyro( CBaseTFPlayer *pPlayer, TFClass iClass ) : CPlayerClass( pPlayer, iClass )
{
	for (int i = 0; i < MAX_TF_TEAMS; ++i)
	{
		SetClassModel( MAKE_STRING(GetClassModelString(i)), i ); 
	}
}


CPlayerClassPyro::~CPlayerClassPyro()
{
}


void CPlayerClassPyro::SetupSizeData()
{
	// Initially set the player to the base player class standing hull size.
	m_pPlayer->SetCollisionBounds( PYROCLASS_HULL_STAND_MIN, PYROCLASS_HULL_STAND_MAX );
	m_pPlayer->SetViewOffset( PYROCLASS_VIEWOFFSET_STAND );
	m_pPlayer->m_Local.m_flStepSize = PYROCLASS_STEPSIZE;	
}


void CPlayerClassPyro::ClassActivate()
{
	BaseClass::ClassActivate();

	// Setup movement data.
	SetupMoveData();
	
	memset( &m_ClassData, 0, sizeof( m_ClassData ) );
}


const char *CPlayerClassPyro::GetClassModelString( int nTeam )
{
	if (nTeam == TEAM_HUMANS)
		return "models/player/medic.mdl";
	else
		return "models/player/recon.mdl";
}


void CPlayerClassPyro::CreateClass()
{
	BaseClass::CreateClass();

	// Create our two handed weapon layout
	m_hWpnFlameThrower = static_cast< CWeaponFlameThrower * >( m_pPlayer->GiveNamedItem( "weapon_flame_thrower" ) );
	m_hWpnGasCan = static_cast< CWeaponGasCan * >( m_pPlayer->GiveNamedItem( "weapon_gas_can" ) );
	m_hWpnShield = m_pPlayer->GetCombatShield();
	CWeaponTwoHandedContainer *p = ( CWeaponTwoHandedContainer * )m_pPlayer->Weapon_OwnsThisType( "weapon_twohandedcontainer" );
	if ( !p )
	{
		p = static_cast< CWeaponTwoHandedContainer * >( m_pPlayer->GiveNamedItem( "weapon_twohandedcontainer" ) );
	}
	if ( p && m_hWpnShield.Get() && m_hWpnGasCan.Get() )
	{
		m_hWpnShield->SetReflectViewModelAnimations( true );
		p->SetWeapons( m_hWpnGasCan, m_hWpnShield );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassPyro::SetPlayerHull()
{
	if ( m_pPlayer->GetFlags() & FL_DUCKING )
	{
		m_pPlayer->SetCollisionBounds( PYROCLASS_HULL_DUCK_MIN, PYROCLASS_HULL_DUCK_MAX );
	}
	else
	{
		m_pPlayer->SetCollisionBounds( PYROCLASS_HULL_STAND_MIN, PYROCLASS_HULL_STAND_MAX );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassPyro::GetPlayerHull( bool bDucking, Vector &vecMin, Vector &vecMax )
{
	if ( bDucking )
	{
		VectorCopy( PYROCLASS_HULL_DUCK_MIN, vecMin );
		VectorCopy( PYROCLASS_HULL_DUCK_MAX, vecMax );
	}
	else
	{
		VectorCopy( PYROCLASS_HULL_STAND_MIN, vecMin );
		VectorCopy( PYROCLASS_HULL_STAND_MAX, vecMax );
	}
}


void CPlayerClassPyro::ResetViewOffset()
{
	if ( m_pPlayer )
	{
		m_pPlayer->SetViewOffset( PYROCLASS_VIEWOFFSET_STAND );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerClassPyro::InitVCollision()
{
	CPhysCollide *pStandModel = PhysCreateBbox( PYROCLASS_HULL_STAND_MIN, PYROCLASS_HULL_STAND_MAX );
	CPhysCollide *pCrouchModel = PhysCreateBbox( PYROCLASS_HULL_DUCK_MIN, PYROCLASS_HULL_DUCK_MAX );
	m_pPlayer->SetupVPhysicsShadow( pStandModel, "tfplayer_medic_stand", pCrouchModel, "tfplayer_medic_crouch" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPlayerClassPyro::ResupplyAmmo( float flFraction, ResupplyReason_t reason )
{
	bool bGiven = false;
	
	if ((reason == RESUPPLY_ALL_FROM_STATION) || (reason == RESUPPLY_AMMO_FROM_STATION))
	{
		if (ResupplyAmmoType( 80 * flFraction, PYRO_AMMO_TYPE ))
			bGiven = true;
	}

	// On respawn, resupply base weapon ammo
	if ( reason == RESUPPLY_RESPAWN )
	{
		if ( ResupplyAmmoType( 30, PYRO_AMMO_TYPE ) )
			bGiven = true;
	}

	if ( BaseClass::ResupplyAmmo(flFraction, reason) )
		bGiven = true;

	return bGiven;
}

