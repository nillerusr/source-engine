#ifndef _INCLUDED_ASW_WEAPON_MEDKIT_H
#define _INCLUDED_ASW_WEAPON_MEDKIT_H
#pragma once

#include "asw_weapon_medical_satchel_shared.h"
#ifdef CLIENT_DLL
#define CASW_Weapon_Medkit C_ASW_Weapon_Medkit
#else
#include "asw_weapon.h"
#endif

#include "basegrenade_shared.h"

class CASW_Weapon_Medkit : public CASW_Weapon_Medical_Satchel
{
public:
	DECLARE_CLASS( CASW_Weapon_Medkit, CASW_Weapon_Medical_Satchel );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Medkit();
	virtual ~CASW_Weapon_Medkit();

	virtual bool OffhandActivate();
	virtual void SelfHeal();

	virtual int GetHealAmount();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();

	virtual const char* GetPickupClass() { return "asw_pickup_medkit"; }
#endif

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_MEDKIT; }
};


#endif /* _INCLUDED_ASW_WEAPON_MEDKIT_H */
