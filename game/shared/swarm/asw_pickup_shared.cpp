#include "cbase.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_pickup.h"
#include "c_asw_pickup_weapon.h"
#include "c_asw_ammo.h"
#define CASW_Marine C_ASW_Marine
#define CASW_Pickup_Weapon C_ASW_Pickup_Weapon
#define CASW_Weapon C_ASW_Weapon
#define CASW_Ammo C_ASW_Ammo
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "asw_pickup.h"
#include "asw_pickup_weapon.h"
#include "asw_ammo.h"
#endif
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool CASW_Ammo::AllowedToPickup(CASW_Marine *pMarine)
{
	if (!pMarine)
		return false;

	if (!ASWGameRules()->MarineCanPickupAmmo(pMarine, this))
	{
		if (!ASWGameRules()->MarineHasRoomInAmmoBag(pMarine, m_iAmmoIndex))
			return false;
	}

	return true;
}

bool CASW_Pickup_Weapon::AllowedToPickup(CASW_Marine *pMarine)
{
	if (!pMarine || !ASWGameRules() || !pMarine->GetMarineResource())
		return false;

	// check if we're swapping for an existing item
	int index = pMarine->GetWeaponPositionForPickup(GetWeaponClass());	
	CASW_Weapon* pWeapon = pMarine->GetASWWeapon(index);
	const char* szSwappingClass = pWeapon ? pWeapon->GetClassname() : "";
	
	// first check if the gamerules will allow it
	bool bAllowed = ASWGameRules()->MarineCanPickup(pMarine->GetMarineResource(), GetWeaponClass(), szSwappingClass);

#ifdef CLIENT_DLL
	m_bSwappingWeapon = ( pWeapon != NULL );
#endif

	return bAllowed;
}