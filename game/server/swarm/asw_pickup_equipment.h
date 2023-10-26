#ifndef _DEFINED_ASW_PICKUP_EQUIPMENT_H
#define _DEFINED_ASW_PICKUP_EQUIPMENT_H

#include "asw_pickup_weapon.h"
#include "asw_shareddefs.h"

class CASW_Player;
class CASW_Weapon;
class CASW_Marine;

class CASW_Pickup_Weapon_Medkit : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Medkit, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_medkit"; }
};

class CASW_Pickup_Weapon_Sentry : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Sentry, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_sentry"; }
};

class CASW_Pickup_Weapon_Sentry_Flamer : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Sentry_Flamer, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_sentry_flamer"; }
};

class CASW_Pickup_Weapon_Sentry_Cannon : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Sentry_Cannon, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_sentry_cannon"; }
};

class CASW_Pickup_Weapon_Sentry_Freeze : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Sentry_Freeze, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_sentry_freeze"; }
};

class CASW_Pickup_Weapon_Tesla_Trap : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Tesla_Trap, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_tesla_trap"; }
};

class CASW_Pickup_Weapon_Ammo_Bag : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Ammo_Bag, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Pickup_Weapon_Ammo_Bag();
	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_ammo_bag"; }
	virtual void InitFrom(CASW_Marine* pMarine, CASW_Weapon* pWeapon);
	virtual void InitWeapon(CASW_Marine* pMarine, CASW_Weapon* pWeapon);

	int m_iAmmoCount[ASW_AMMO_BAG_SLOTS];
};

class CASW_Pickup_Weapon_Ammo_Satchel : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Ammo_Satchel, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Pickup_Weapon_Ammo_Satchel();
	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_ammo_satchel"; }
	virtual void InitFrom(CASW_Marine* pMarine, CASW_Weapon* pWeapon);
	virtual void InitWeapon(CASW_Marine* pMarine, CASW_Weapon* pWeapon);

	//int m_nAmmoDrops;
};

class CASW_Pickup_Weapon_Medical_Satchel : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Medical_Satchel, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_medical_satchel"; }
};

class CASW_Pickup_Weapon_Stim : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Stim, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_stim"; }
};

class CASW_Pickup_Weapon_Flares : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Flares, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_flares"; }
};

class CASW_Pickup_Weapon_T75 : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_T75, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_t75"; }
};

class CASW_Pickup_Weapon_Mines : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Mines, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_mines"; }
};

class CASW_Pickup_Weapon_Heal_Grenade : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Heal_Grenade, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_heal_grenade"; }
};

class CASW_Pickup_Weapon_Buff_Grenade : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Buff_Grenade, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_buff_grenade"; }
};

class CASW_Pickup_Hornet_Barrage : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Hornet_Barrage, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_hornet_barrage"; }
};

class CASW_Pickup_Weapon_Flashlight : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Flashlight, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_flashlight"; }
};

class CASW_Pickup_Weapon_Welder : public CASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( CASW_Pickup_Weapon_Welder, CASW_Pickup_Weapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual const char* GetWeaponClass() { return "asw_weapon_welder"; }
};

#endif /* _DEFINED_ASW_PICKUP_EQUIPMENT_H */