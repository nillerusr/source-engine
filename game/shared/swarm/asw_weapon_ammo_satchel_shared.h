#ifndef _INCLUDED_ASW_WEAPON_AMMO_SATCHEL_H
#define _INCLUDED_ASW_WEAPON_AMMO_SATCHEL_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon_Ammo_Satchel C_ASW_Weapon_Ammo_Satchel
#define CASW_Weapon C_ASW_Weapon
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "asw_shareddefs.h"

#define AMMO_SATCHEL_DEFAULT_DROP_COUNT 3

class CASW_Weapon_Ammo_Satchel : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Ammo_Satchel, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Ammo_Satchel();
	virtual ~CASW_Weapon_Ammo_Satchel();
	void Precache();
	
	Activity	GetPrimaryAttackActivity( void );

	virtual void	PrimaryAttack();
	virtual bool	OffhandActivate();
	void	DeployAmmoDrop();

	virtual bool			UsesClipsForAmmo1( void ) const { return false; }

#ifndef CLIENT_DLL
	DECLARE_DATADESC();

	int		CapabilitiesGet( void ) { return 0; }

	virtual const char* GetPickupClass() { return "asw_pickup_ammo_satchel"; }
	void	SetAmmoDrops( int nAmmoDrops ) { m_iClip1 = nAmmoDrops; }
#endif
	//int m_nAmmoDrops;
	float m_fLastAmmoDropTime;

	virtual bool IsOffensiveWeapon() { return false; }

	virtual Class_T Classify( void ) { return (Class_T)CLASS_ASW_AMMO_SATCHEL; }
};

#endif /* _INCLUDED_ASW_WEAPON_AMMO_SATCHEL_H */
