#include "cbase.h"
#include "asw_weapon_flashlight_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "asw_gamerules.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Flashlight, DT_ASW_Weapon_Flashlight )

BEGIN_NETWORK_TABLE( CASW_Weapon_Flashlight, DT_ASW_Weapon_Flashlight )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Flashlight )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_flashlight, CASW_Weapon_Flashlight );
PRECACHE_WEAPON_REGISTER(asw_weapon_flashlight);

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Flashlight )
	
END_DATADESC()

#endif /* not client */

CASW_Weapon_Flashlight::CASW_Weapon_Flashlight()
{

}


CASW_Weapon_Flashlight::~CASW_Weapon_Flashlight()
{

}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CASW_Weapon_Flashlight::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

#ifndef CLIENT_DLL

void CASW_Weapon_Flashlight::MarineDropped(CASW_Marine* pMarine)
{
	pMarine->FlashlightTurnOff();

	BaseClass::MarineDropped(pMarine);
}

void CASW_Weapon_Flashlight::Equip( CBaseCombatCharacter *pOwner )
{
	BaseClass::Equip(pOwner);
	CASW_Marine* pMarine = CASW_Marine::AsMarine( pOwner );
	if (pMarine)
	{
		pMarine->FlashlightTurnOn();
	}
}

#endif // not client

void CASW_Weapon_Flashlight::PrimaryAttack( void )
{
	// do nothing
}

void CASW_Weapon_Flashlight::Precache()
{
	BaseClass::Precache();
}