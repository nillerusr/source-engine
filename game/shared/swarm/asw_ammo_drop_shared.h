#ifndef C_ASW_AMMO_DROP_SHARED_H
#define C_ASW_AMMO_DROP_SHARED_H

#define DEFAULT_AMMO_DROP_UNITS 100
#define AMMO_UNITS_MAX 9999

class CASW_Ammo_Drop_Shared
{
private:
	CASW_Ammo_Drop_Shared() {}

	enum {
		ASW_AMMO_DROP_TYPE_RIFLE = 0,
		ASW_AMMO_DROP_TYPE_AUTOGUN,
		ASW_AMMO_DROP_TYPE_SHOTGUN,
		ASW_AMMO_DROP_TYPE_ASSAULT_SHOTGUN,		
		ASW_AMMO_DROP_TYPE_FLAMER,
		ASW_AMMO_DROP_TYPE_RAILGUN,
		ASW_AMMO_DROP_TYPE_PDW,
		ASW_AMMO_DROP_TYPE_PISTOL,
		ASW_AMMO_DROP_TYPE_MINING_LASER,
		ASW_AMMO_DROP_TYPE_TESLA_CANNON,
		ASW_AMMO_DROP_TYPE_GRENADE_LAUNCHER,
		ASW_AMMO_DROP_TYPE_SNIPER,
		ASW_AMMO_DROP_TYPE_COUNT
	};

	static void InitAmmoCosts();

	static bool ms_bAmmoCostInitiated;
	static int ms_iAmmoType[ASW_AMMO_DROP_TYPE_COUNT];
	static int ms_iAmmoUnitCost[ASW_AMMO_DROP_TYPE_COUNT];
	static int ms_iAmmoClips[ASW_AMMO_DROP_TYPE_COUNT];	// how many clips to give per usage

public:
	static int GetAmmoUnitCost( int iAmmoType );
	static int GetAmmoClipsToGive( int iAmmoType );
};

#endif  /* C_ASW_AMMO_DROP_SHARED_H */