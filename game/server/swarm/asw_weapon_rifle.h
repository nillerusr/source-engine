#ifndef _INCLUDED_ASW_WEAPON_RIFLE_H
#define _INCLUDED_ASW_WEAPON_RIFLE_H
#pragma once

#include "asw_weapon.h"
#include "basegrenade_shared.h"
#include "npc_combine.h"
#include "asw_shareddefs.h"

class CASW_Weapon_Rifle : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Rifle, CASW_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Weapon_Rifle();
	virtual ~CASW_Weapon_Rifle();	

	void	Precache( void );
	
	void	SecondaryAttack( void );
	virtual bool SupportsBayonet() { return true; }

	const char *GetTracerType( void ) { return "ASWTracer"; }

	float	GetFireRate( void );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	
	void	DoImpactEffect( trace_t &tr, int nDamageType );

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}

	virtual const float GetAutoAimAmount() { return AUTOAIM_2DEGREES * 0.3f; }
	virtual bool ShouldFlareAutoaim() { return true; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_RIFLE; }
};


#endif /* _INCLUDED_ASW_WEAPON_RIFLE_H */
