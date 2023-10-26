#ifndef _DEFINED_C_ASW_PICKUP_EQUIPMENT_H
#define _DEFINED_C_ASW_PICKUP_EQUIPMENT_H

#include "c_asw_pickup_weapon.h"

class C_ASW_Pickup_Weapon_Medkit : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Medkit, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_medkit"; }
	C_ASW_Pickup_Weapon_Medkit();
};

class C_ASW_Pickup_Weapon_Sentry : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Sentry, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_sentry"; }
	C_ASW_Pickup_Weapon_Sentry();
};

class C_ASW_Pickup_Weapon_Sentry_Flamer : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Sentry_Flamer, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_sentry_flamer"; }
	C_ASW_Pickup_Weapon_Sentry_Flamer();
};

class C_ASW_Pickup_Weapon_Sentry_Cannon : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Sentry_Cannon, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_sentry_cannon"; }
	C_ASW_Pickup_Weapon_Sentry_Cannon();
};

class C_ASW_Pickup_Weapon_Sentry_Freeze : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Sentry_Freeze, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_sentry_freeze"; }
	C_ASW_Pickup_Weapon_Sentry_Freeze();
};

class C_ASW_Pickup_Weapon_Tesla_Trap : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Tesla_Trap, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_tesla_trap"; }
	C_ASW_Pickup_Weapon_Tesla_Trap();
};

class C_ASW_Pickup_Weapon_Ammo_Bag : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Ammo_Bag, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_ammo_bag"; }
	C_ASW_Pickup_Weapon_Ammo_Bag();
};

class C_ASW_Pickup_Weapon_Ammo_Satchel : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Ammo_Satchel, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_ammo_satchel"; }
	C_ASW_Pickup_Weapon_Ammo_Satchel();
};

class C_ASW_Pickup_Weapon_Medical_Satchel : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Medical_Satchel, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_medical_satchel"; }
	C_ASW_Pickup_Weapon_Medical_Satchel();
};

class C_ASW_Pickup_Weapon_Stim : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Stim, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_stim"; }
	C_ASW_Pickup_Weapon_Stim();
};

class C_ASW_Pickup_Weapon_Flares : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Flares, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_flares"; }
	C_ASW_Pickup_Weapon_Flares();
};

class C_ASW_Pickup_Weapon_Mines : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Mines, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_mines"; }
	C_ASW_Pickup_Weapon_Mines();
};

class C_ASW_Pickup_Weapon_T75 : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_T75, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_t75"; }
	C_ASW_Pickup_Weapon_T75();
};

class C_ASW_Pickup_Weapon_Heal_Grenade : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Heal_Grenade, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_heal_grenade"; }
	C_ASW_Pickup_Weapon_Heal_Grenade();
};

class C_ASW_Pickup_Weapon_Buff_Grenade : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Buff_Grenade, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_buff_grenade"; }
	C_ASW_Pickup_Weapon_Buff_Grenade();
};

class C_ASW_Pickup_Hornet_Barrage : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Hornet_Barrage, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_hornet_barrage"; }
	C_ASW_Pickup_Hornet_Barrage();
};

class C_ASW_Pickup_Weapon_Flashlight : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Flashlight, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_flashlight"; }
	C_ASW_Pickup_Weapon_Flashlight();
};

class C_ASW_Pickup_Weapon_Welder : public C_ASW_Pickup_Weapon
{
public:
	DECLARE_CLASS( C_ASW_Pickup_Weapon_Welder, C_ASW_Pickup_Weapon );
	DECLARE_CLIENTCLASS();

	virtual const char* GetWeaponClass() { return "asw_weapon_welder"; }
	C_ASW_Pickup_Weapon_Welder();
};

#endif /* _DEFINED_C_ASW_PICKUP_EQUIPMENT_H */