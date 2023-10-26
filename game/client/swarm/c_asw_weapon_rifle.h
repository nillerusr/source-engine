#ifndef _INCLUDED_C_ASW_WEAPON_RIFLE_H
#define _INCLUDED_C_ASW_WEAPON_RIFLE_H
#ifdef _WIN32
#pragma once
#endif

#include "c_asw_weapon.h"

class C_ASW_Weapon_Rifle : public C_ASW_Weapon
{
	DECLARE_CLASS( C_ASW_Weapon_Rifle, C_ASW_Weapon );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_ASW_Weapon_Rifle();
	virtual			~C_ASW_Weapon_Rifle();

	float	GetFireRate( void );

	virtual const char* GetTracerEffectName() { return "tracer_rifle"; }	// particle effect name
	virtual const char* GetMuzzleEffectName() { return "muzzle_rifle"; }	// particle effect name

	virtual const float GetAutoAimAmount() { return AUTOAIM_2DEGREES * 0.3f; }
	virtual bool ShouldFlareAutoaim() { return true; }
	virtual void SecondaryAttack();
	virtual bool SupportsBayonet();
	virtual bool HasSecondaryExplosive( void ) const { return true; }
	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone = VECTOR_CONE_3DEGREES;
		return cone;
	}

	virtual bool SupportsGroundShooting() { return true; }

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_RIFLE; }

private:
	C_ASW_Weapon_Rifle( const C_ASW_Weapon_Rifle & ); // not defined, not accessible
};

#endif /* _INCLUDED_C_ASW_WEAPON_RIFLE_H */


