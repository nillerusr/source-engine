//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_bottle.h"
#include "decals.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
// Server specific.
#else
#include "tf_player.h"
#endif

//=============================================================================
//
// Weapon Bottle tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFBottle, DT_TFWeaponBottle )

BEGIN_NETWORK_TABLE( CTFBottle, DT_TFWeaponBottle )
#if defined( CLIENT_DLL )
	RecvPropBool( RECVINFO( m_bBroken ) )
#else
	SendPropBool( SENDINFO( m_bBroken ) )
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFBottle )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_bottle, CTFBottle );
PRECACHE_WEAPON_REGISTER( tf_weapon_bottle );

#define TF_BOTTLE_SWITCHGROUP 1
#define TF_BOTTLE_NOTBROKEN 0
#define TF_BOTTLE_BROKEN 1

//=============================================================================
//
// Weapon Bottle functions.
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFBottle::CTFBottle()
{
}

void CTFBottle::WeaponReset( void )
{
	BaseClass::WeaponReset();

	m_bBroken = false;
}

bool CTFBottle::DefaultDeploy( char *szViewModel, char *szWeaponModel, int iActivity, char *szAnimExt )
{
	bool bRet = BaseClass::DefaultDeploy( szViewModel, szWeaponModel, iActivity, szAnimExt );

	if ( bRet )
	{
		SwitchBodyGroups();
	}

	return bRet;
}

void CTFBottle::SwitchBodyGroups( void )
{
	int iState = 0;

	if ( m_bBroken == true )
	{
		iState = 1;
	}

	SetBodygroup( TF_BOTTLE_SWITCHGROUP, iState );

	CTFPlayer *pTFPlayer = ToTFPlayer( GetOwner() );

	if ( pTFPlayer && pTFPlayer->GetActiveWeapon() == this )
	{
		if ( pTFPlayer->GetViewModel() )
		{
			pTFPlayer->GetViewModel()->SetBodygroup( TF_BOTTLE_SWITCHGROUP, iState );
		}
	}
}

void CTFBottle::Smack( void )
{
	BaseClass::Smack();

	if ( ConnectedHit() && IsCurrentAttackACritical() )
	{
		m_bBroken = true;
		SwitchBodyGroups();
	}
}
