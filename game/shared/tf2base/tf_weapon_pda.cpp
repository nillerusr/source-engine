//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "tf_weapon_pda.h"
#include "in_buttons.h"

// Server specific.
#if !defined( CLIENT_DLL )
#include "tf_player.h"
#include "vguiscreen.h"
// Client specific.
#else
#include "c_tf_player.h"
#include <igameevents.h>
#endif

//=============================================================================
//
// TFWeaponBase Melee tables.
//
IMPLEMENT_NETWORKCLASS_ALIASED( TFWeaponPDA, DT_TFWeaponPDA )

BEGIN_NETWORK_TABLE( CTFWeaponPDA, DT_TFWeaponPDA )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFWeaponPDA )
END_PREDICTION_DATA()

// Server specific.
#if !defined( CLIENT_DLL ) 
BEGIN_DATADESC( CTFWeaponPDA )
END_DATADESC()
#endif

CTFWeaponPDA::CTFWeaponPDA()
{
}

void CTFWeaponPDA::Spawn()
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: cancel menu
//-----------------------------------------------------------------------------
void CTFWeaponPDA::PrimaryAttack( void )
{
	CTFPlayer *pOwner = ToTFPlayer( GetOwner() );
	if ( !pOwner )
	{
		return;
	}

	pOwner->SelectLastItem();
}

//-----------------------------------------------------------------------------
// Purpose: toggle invis
//-----------------------------------------------------------------------------
void CTFWeaponPDA::SecondaryAttack( void )
{
	// semi-auto behaviour
	if ( m_bInAttack2 )
		return;

	// Get the player owning the weapon.
	CTFPlayer *pPlayer = ToTFPlayer( GetPlayerOwner() );
	if ( !pPlayer )
		return;

	pPlayer->DoClassSpecialSkill();

	m_bInAttack2 = true;

	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
}

#if !defined( CLIENT_DLL )

	void CTFWeaponPDA::Precache()
	{
		BaseClass::Precache();
		PrecacheVGuiScreen( GetPanelName() );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Gets info about the control panels
	//-----------------------------------------------------------------------------
	void CTFWeaponPDA::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
	{
		pPanelName = GetPanelName();
	}

#else

	float CTFWeaponPDA::CalcViewmodelBob( void )
	{
		// no bob
		return BaseClass::CalcViewmodelBob();
	}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CTFWeaponPDA::ShouldShowControlPanels( void )
{
	return true;
}

//==============================

IMPLEMENT_NETWORKCLASS_ALIASED( TFWeaponPDA_Engineer_Build, DT_TFWeaponPDA_Engineer_Build )

BEGIN_NETWORK_TABLE( CTFWeaponPDA_Engineer_Build, DT_TFWeaponPDA_Engineer_Build )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFWeaponPDA_Engineer_Build )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_pda_engineer_build, CTFWeaponPDA_Engineer_Build );
PRECACHE_WEAPON_REGISTER( tf_weapon_pda_engineer_build );

//==============================

IMPLEMENT_NETWORKCLASS_ALIASED( TFWeaponPDA_Engineer_Destroy, DT_TFWeaponPDA_Engineer_Destroy )

BEGIN_NETWORK_TABLE( CTFWeaponPDA_Engineer_Destroy, DT_TFWeaponPDA_Engineer_Destroy )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFWeaponPDA_Engineer_Destroy )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_pda_engineer_destroy, CTFWeaponPDA_Engineer_Destroy );
PRECACHE_WEAPON_REGISTER( tf_weapon_pda_engineer_destroy );

//==============================

IMPLEMENT_NETWORKCLASS_ALIASED( TFWeaponPDA_Spy, DT_TFWeaponPDA_Spy )

BEGIN_NETWORK_TABLE( CTFWeaponPDA_Spy, DT_TFWeaponPDA_Spy )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CTFWeaponPDA_Spy )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( tf_weapon_pda_spy, CTFWeaponPDA_Spy );
PRECACHE_WEAPON_REGISTER( tf_weapon_pda_spy );

#ifdef CLIENT_DLL

bool CTFWeaponPDA_Spy::Deploy( void )
{
	bool bDeploy = BaseClass::Deploy();

	if ( bDeploy )
	{
		// let the spy pda menu know to reset
		IGameEvent *event = gameeventmanager->CreateEvent( "spy_pda_reset" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}

	return bDeploy;
}

#endif