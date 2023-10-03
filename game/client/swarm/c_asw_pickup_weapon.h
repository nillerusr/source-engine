#ifndef _DEFINED_C_ASW_PICKUP_WEAPON_H
#define _DEFINED_C_ASW_PICKUP_WEAPON_H

#include "c_asw_pickup.h"
#include "utldict.h"

class C_ASW_Pickup_Weapon : public C_ASW_Pickup
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon, C_ASW_Pickup );
	DECLARE_CLIENTCLASS();

	C_ASW_Pickup_Weapon();
	virtual bool AllowedToPickup(C_ASW_Marine *pMarine);
	virtual bool GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser);
	virtual const char* GetWeaponClass() { return "asw_weapon_rifle"; }
	virtual int GetUseIconTextureID() { return m_nUseIconTextureID; }
	virtual void GetUseIconText( wchar_t *unicode, int unicodeBufferSizeInBytes );

	virtual void InitPickup();

	CNetworkVar(int, m_iBulletsInGun);
	CNetworkVar(int, m_iClips);
	CNetworkVar(int, m_iSecondary);

	int m_nUseIconTextureID;
	bool m_bWideIcon;
	bool m_bSwappingWeapon;
};

extern CUtlDict< int, int > g_WeaponUseIcons;


class C_ASW_Pickup_Weapon_Rifle : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Rifle, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_rifle"; }
	C_ASW_Pickup_Weapon_Rifle();
};

class C_ASW_Pickup_Weapon_PRifle : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_PRifle, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_prifle"; }
	C_ASW_Pickup_Weapon_PRifle();
};

class C_ASW_Pickup_Weapon_Autogun : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Autogun, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_autogun"; }
	C_ASW_Pickup_Weapon_Autogun();
};

class C_ASW_Pickup_Weapon_Shotgun : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Shotgun, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_shotgun"; }
	C_ASW_Pickup_Weapon_Shotgun();
};

class C_ASW_Pickup_Weapon_Assault_Shotgun : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Assault_Shotgun, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_vindicator"; }
	C_ASW_Pickup_Weapon_Assault_Shotgun();
};

class C_ASW_Pickup_Weapon_Flamer : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Flamer, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_flamer"; }
	C_ASW_Pickup_Weapon_Flamer();
};

class C_ASW_Pickup_Weapon_Railgun : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Railgun, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_railgun"; }
	C_ASW_Pickup_Weapon_Railgun();
};

class C_ASW_Pickup_Weapon_Ricochet : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Ricochet, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_ricochet"; }
	C_ASW_Pickup_Weapon_Ricochet();
};

class C_ASW_Pickup_Weapon_Flechette : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Flechette, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_flechette"; }
	C_ASW_Pickup_Weapon_Flechette();
};

class C_ASW_Pickup_Weapon_FireExtinguisher : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_FireExtinguisher, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_fire_extinguisher"; }
	C_ASW_Pickup_Weapon_FireExtinguisher();
};

class C_ASW_Pickup_Weapon_Pistol : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Pistol, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_pistol"; }
	C_ASW_Pickup_Weapon_Pistol();
};

class C_ASW_Pickup_Weapon_Mining_Laser : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Mining_Laser, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_mining_laser"; }
	C_ASW_Pickup_Weapon_Mining_Laser();
};

class C_ASW_Pickup_Weapon_Chainsaw : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Chainsaw, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_chainsaw"; }
	C_ASW_Pickup_Weapon_Chainsaw();
};

class C_ASW_Pickup_Weapon_PDW : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_PDW, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_pdw"; }
	C_ASW_Pickup_Weapon_PDW();
};

class C_ASW_Pickup_Weapon_Grenades : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Grenades, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_grenades"; }
	C_ASW_Pickup_Weapon_Grenades();
};


#endif /* _DEFINED_C_ASW_PICKUP_WEAPON_H */