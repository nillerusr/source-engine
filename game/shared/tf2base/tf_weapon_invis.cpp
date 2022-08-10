//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_invis.h"
#include "in_buttons.h"

#if !defined( CLIENT_DLL )
	#include "vguiscreen.h"
	#include "tf_player.h"
#else
	#include "c_tf_player.h"
#endif

extern ConVar tf_spy_invis_unstealth_time;

//=============================================================================
//
// TFWeaponBase Melee tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFWeaponInvis, DT_TFWeaponInvis )

BEGIN_NETWORK_TABLE( CTFWeaponInvis, DT_TFWeaponInvis )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFWeaponInvis )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_invis, CTFWeaponInvis );
PRECACHE_WEAPON_REGISTER( tf_weapon_invis );

// Server specific.
#if !defined( CLIENT_DLL ) 
	BEGIN_DATADESC( CTFWeaponInvis )
	END_DATADESC()
#endif

//-----------------------------------------------------------------------------
// Purpose: Use the offhand view model
//-----------------------------------------------------------------------------
void CTFWeaponInvis::Spawn( void )
{
	BaseClass::Spawn();

	SetViewModelIndex( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Clear out the view model when we hide
//-----------------------------------------------------------------------------
void CTFWeaponInvis::HideThink( void )
{ 
	SetWeaponVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: Show/hide weapon and corresponding view model if any
// Input  : visible - 
//-----------------------------------------------------------------------------
void CTFWeaponInvis::SetWeaponVisible( bool visible )
{
	CBaseViewModel *vm = NULL;

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner )
	{
		vm = pOwner->GetViewModel( m_nViewModelIndex );
	}

	if ( visible )
	{
		RemoveEffects( EF_NODRAW );
		if ( vm )
		{
			vm->RemoveEffects( EF_NODRAW );
		}
	}
	else
	{
		AddEffects( EF_NODRAW );
		if ( vm )
		{
			vm->AddEffects( EF_NODRAW );
		}
	}
}

bool CTFWeaponInvis::Deploy( void )
{
	bool b = BaseClass::Deploy();

	SetWeaponIdleTime( gpGlobals->curtime + 1.5 );

	return b;
}

bool CTFWeaponInvis::Holster( CBaseCombatWeapon *pSwitchingTo )
{ 
	bool bHolster = BaseClass::Holster( pSwitchingTo );

	// far in the future
	SetWeaponIdleTime( gpGlobals->curtime + 10 );

	return bHolster;
}

void CTFWeaponInvis::SecondaryAttack( void )
{
	// do nothing
}

void CTFWeaponInvis::ItemBusyFrame( void )
{
	// do nothing
}

#ifndef CLIENT_DLL

void CTFWeaponInvis::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "pda_panel_spy_invis";
}

#endif