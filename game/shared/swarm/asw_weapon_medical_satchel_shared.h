#ifndef _INCLUDED_ASW_WEAPON_MEDICAL_SATCHEL_H
#define _INCLUDED_ASW_WEAPON_MEDICAL_SATCHEL_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Medical_Satchel C_ASW_Weapon_Medical_Satchel
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

class CASW_Weapon_Medical_Satchel : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Medical_Satchel, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Medical_Satchel();
	
	Activity	GetPrimaryAttackActivity( void );

	void	PrimaryAttack();

	virtual int GetHealAmount();

	bool GiveHealth();

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		void	GiveHealthTo( CASW_Marine *pTarget );
		int		GetTotalMeds();	// returns total med charges for carrying marine (checks for carrying 2 med satchels)
		bool	MedSatchelEmpty();	// returns true if there are no primary and no secondary charges in the medsatchel
		virtual bool Holster( CBaseCombatWeapon *pSwitchingTo );
		virtual float GetInfestationCureAmount();
		void Drop( const Vector &vecVelocity );

		virtual const char* GetPickupClass() { return "asw_pickup_medical_satchel"; }		
	#else
		virtual void MouseOverEntity(C_BaseEntity *pEnt, Vector vecWorldCursor);
	#endif

	virtual bool IsOffensiveWeapon() { return false; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_MEDICAL_SATCHEL; }
};


#endif /* _INCLUDED_ASW_WEAPON_MEDICAL_SATCHEL_H */
