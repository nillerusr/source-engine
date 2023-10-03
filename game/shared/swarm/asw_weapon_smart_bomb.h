#ifndef _ASW_WEAPON_SMART_BOMB_H
#define _ASW_WEAPON_SMART_BOMB_H
#pragma once

#include "asw_weapon_hornet_barrage.h"
#ifdef CLIENT_DLL
#define CASW_Weapon_Smart_Bomb C_ASW_Weapon_Smart_Bomb
#else
#include "asw_weapon.h"
#endif

class CASW_Weapon_Smart_Bomb : public CASW_Weapon_Hornet_Barrage
{
public:
	DECLARE_CLASS( CASW_Weapon_Smart_Bomb, CASW_Weapon_Hornet_Barrage );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Smart_Bomb();

	virtual const QAngle& GetRocketAngle();
	virtual void SetRocketsToFire();
	virtual float GetRocketFireInterval();
	virtual const Vector& GetRocketFiringPosition();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#else
	
#endif

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SMART_BOMB; }
};


#endif /* _INCLUDED_ASW_WEAPON_SMART_BOMB_H */
