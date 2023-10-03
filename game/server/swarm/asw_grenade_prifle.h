#ifndef _DEFINED_ASW_GRENADE_PRIFLE_H
#define _DEFINED_ASW_GRENADE_PRIFLE_H
#pragma once

#ifdef CLIENT_DLL
#define CBaseEntity C_BaseEntity
#endif

#include "asw_rifle_grenade.h"

class CASW_Grenade_PRifle : public CASW_Rifle_Grenade
{
public:
	DECLARE_CLASS( CASW_Grenade_PRifle, CASW_Rifle_Grenade );
					
	virtual void Detonate();
	virtual void Spawn();
	virtual void Precache( void );
	virtual void CreateEffects();
	static CASW_Grenade_PRifle *PRifle_Grenade_Create( float flDamage, float fRadius, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon );	

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_GRENADE_PRIFLE; }
};


#endif // _DEFINED_ASW_GRENADE_PRIFLE_H
