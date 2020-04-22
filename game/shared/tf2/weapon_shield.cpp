//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:			The Escort's Shield weapon
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "weapon_shield.h"
#include "in_buttons.h"
#include <float.h>
#include "mathlib/mathlib.h"
#include "engine/IEngineSound.h"

#if defined( CLIENT_DLL )
#include "c_shield.h"
#else
#include "tf_shield.h"
#endif

//-----------------------------------------------------------------------------
// Shield WEAPON
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( weapon_shield, CWeaponShield );
PRECACHE_WEAPON_REGISTER(weapon_shield);

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponShield, DT_WeaponShield )

BEGIN_NETWORK_TABLE(CWeaponShield, DT_WeaponShield)
#ifdef CLIENT_DLL
	RecvPropEHandle (RECVINFO(m_hDeployedShield)),
#else
	SendPropEHandle (SENDINFO(m_hDeployedShield)),
#endif
END_NETWORK_TABLE()


BEGIN_PREDICTION_DATA( CWeaponShield  )

	DEFINE_FIELD( m_bIsDeployed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bShieldPositionLocked, FIELD_BOOLEAN ),

END_PREDICTION_DATA()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponShield::CWeaponShield( void )
{
	SetPredictionEligible( true );
	m_bIsDeployed = false;
}

void CWeaponShield::UpdateOnRemove( void )
{
#ifdef GAME_DLL
	if ( m_hDeployedShield.Get() )
	{
		m_hDeployedShield.Get()->Remove( );
		m_hDeployedShield.Set( NULL );
	}
#endif

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
// Purpose: Given the distance the hit occurred at, return the damage done
//-----------------------------------------------------------------------------
float CWeaponShield::GetDamage( float flDistance, int iLocation )
{
	// Can't injure at all over this distance
	return 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CWeaponShield::GetFireRate( void )
{	
	return 1.0; 
}


//-----------------------------------------------------------------------------
// Deploy the shield! 
//-----------------------------------------------------------------------------
bool CWeaponShield::Deploy( )
{
	if ( !BaseClass::Deploy() )
		return false;

	// NOTE: the underlying system can cause Deploy to be called twice in a row
	if ( m_bIsDeployed )
		return true;

	// We gotta make the actual shield effect....
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if ( !pPlayer )
		return false;

#ifdef GAME_DLL
	Assert( !m_hDeployedShield.Get().IsValid() ); 
	m_hDeployedShield = CreateMobileShield( pPlayer );
#endif

	SetShieldPositionLocked( false );
	m_bIsDeployed = true;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Remove the shield
//-----------------------------------------------------------------------------
bool CWeaponShield::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifdef GAME_DLL
	if ( m_hDeployedShield.Get() )
	{
		m_hDeployedShield.Get()->Remove( );
		m_hDeployedShield.Set( NULL );
	}
#endif

	m_bIsDeployed = false;

	return BaseClass::Holster( pSwitchingTo );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponShield::ItemPostFrame( void )
{
	CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
	if (pPlayer == NULL)
		return;

	// Disabled
	if ( ComputeEMPFireState() )
	{
		// Handle deployment & pick-up (firing is handled on the client)
		if ( pPlayer->m_nButtons & IN_ATTACK2 )
		{
			if ( gpGlobals->curtime >= m_flNextPrimaryAttack) 
			{
				PrimaryAttack();
			}
		}
	}

	WeaponIdle();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponShield::PrimaryAttack( void )
{
	CShield *shield = m_hDeployedShield.Get();
	if ( shield )
	{
		SetShieldPositionLocked( !m_bShieldPositionLocked );

#ifdef GAME_DLL
		if ( m_bShieldPositionLocked )
		{
			ClientPrint( ToBasePlayer(GetOwner()), HUD_PRINTCENTER, "Projected Shield Locked" );
		}
		else
		{
			ClientPrint( ToBasePlayer(GetOwner()), HUD_PRINTCENTER, "Projected Shield Tracking" );
		}
#endif
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + 0.25f;
}


//-----------------------------------------------------------------------------
// Lock the projected shield
//-----------------------------------------------------------------------------
void CWeaponShield::SetShieldPositionLocked( bool bLocked )
{
	m_bShieldPositionLocked = bLocked; 
	if ( m_hDeployedShield.Get() )
	{
		if ( bLocked && m_hDeployedShield.Get()->IsAlwaysOrienting() )
		{
			CBaseTFPlayer *pPlayer = ToBaseTFPlayer( GetOwner() );
			m_hDeployedShield.Get()->SetCenterAngles( pPlayer->EyeAngles() );
		}
		m_hDeployedShield.Get()->SetAlwaysOrient( !bLocked );
	}
}


//-----------------------------------------------------------------------------
// Idle processing
//-----------------------------------------------------------------------------
void CWeaponShield::WeaponIdle( void )
{
	bool isEmped = IsOwnerEMPed();

	if (m_hDeployedShield.Get())
	{
		m_hDeployedShield.Get()->SetEMPed( isEmped ); 
	}
}
