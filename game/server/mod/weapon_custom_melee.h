//=//=============================================================================//
//
// Purpose: Melee weapon for custom NPCs. Its model can be overridden.
//
//	Author: 1upD
//
//=============================================================================//
#include "cbase.h"
#include "basehlcombatweapon_shared.h"
#include "weapon_crowbar.h"


class CWeaponCustomMelee : public CWeaponCrowbar
{
	DECLARE_CLASS(CWeaponCustomMelee, CWeaponCrowbar);

	const char *GetWorldModel() const { return m_iszWeaponModelName.ToCStr(); }
	virtual void Drop(const Vector &vecVelocity);
public:
	string_t m_iszWeaponModelName;

	DECLARE_DATADESC();
};

void CWeaponCustomMelee::Drop(const Vector &vecVelocity)
{
	BaseClass::Drop(vecVelocity);
	AddSpawnFlags(SF_WEAPON_NO_PLAYER_PICKUP);
}

LINK_ENTITY_TO_CLASS(weapon_custommelee, CWeaponCustomMelee);

BEGIN_DATADESC(CWeaponCustomMelee)
	DEFINE_FIELD(m_iszWeaponModelName, FIELD_STRING)
END_DATADESC()
