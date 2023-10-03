#ifndef _ASW_WEAPON_HORNET_BARRAGE_H
#define _ASW_WEAPON_HORNET_BARRAGE_H
#pragma once

#include "asw_weapon_shared.h"
#include "asw_shareddefs.h"

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Hornet_Barrage C_ASW_Weapon_Hornet_Barrage
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#endif

class CASW_Weapon_Hornet_Barrage : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Hornet_Barrage, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Hornet_Barrage();

	virtual void PrimaryAttack();
	virtual void ItemPostFrame();
	virtual void Precache();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();

	virtual const char* GetPickupClass() { return "asw_pickup_mines"; }	
#else
	
#endif
	int GetRocketsToFire() { return m_iRocketsToFire.Get(); }
	float GetNextLaunchTime() { return m_flNextLaunchTime.Get(); }
	virtual void FireRocket();
	virtual bool IsOffensiveWeapon() { return false; }
	virtual bool OffhandActivate();
	virtual bool WantsOffhandPostFrame() { return m_iRocketsToFire.Get() > 0; }
	virtual const QAngle& GetRocketAngle();
	virtual void SetRocketsToFire();
	virtual float GetRocketFireInterval();
	virtual const Vector& GetRocketFiringPosition();

	CNetworkVar( float, m_flNextLaunchTime );
	CNetworkVar( int, m_iRocketsToFire );
	CNetworkVar( float, m_flFireInterval );

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_HORNET_BARRAGE; }
};


#endif /* _INCLUDED_ASW_WEAPON_HORNET_BARRAGE_H */
