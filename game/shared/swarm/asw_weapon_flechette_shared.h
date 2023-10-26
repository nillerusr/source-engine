#ifndef _INCLUDED_ASW_WEAPON_FLECHETTE_H
#define _INCLUDED_ASW_WEAPON_FLECHETTE_H
#pragma once

#ifdef CLIENT_DLL
	#define CASW_Weapon_Flechette C_ASW_Weapon_Flechette
	#define CASW_Weapon_Rifle C_ASW_Weapon_Rifle
	#include "c_asw_weapon_rifle.h"
#else
	#include "npc_combine.h"
	#include "asw_weapon_rifle.h"
#endif

class CASW_Weapon_Flechette : public CASW_Weapon_Rifle
{
public:
	DECLARE_CLASS( CASW_Weapon_Flechette, CASW_Weapon_Rifle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Flechette();
	virtual ~CASW_Weapon_Flechette();
	void Precache();

	virtual void PrimaryAttack();
	virtual void SecondaryAttack();

	virtual float GetFireRate( void );

	virtual const float GetAutoAimAmount() { return 0.26f; }
	virtual bool ShouldFlareAutoaim() { return true; }

	#ifndef CLIENT_DLL

		virtual const char* GetPickupClass() { return "asw_pickup_flechette"; }
	#else
		virtual bool HasSecondaryExplosive( void ) const { return false; }
	#endif
	
	// aiming grenades at the ground
	virtual bool SupportsGroundShooting() { return false; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_FLECHETTE; }
};

#endif /* _INCLUDED_ASW_WEAPON_FLECHETTE_H */
