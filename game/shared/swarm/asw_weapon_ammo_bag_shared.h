#ifndef _INCLUDED_ASW_WEAPON_AMMO_BAG_H
#define _INCLUDED_ASW_WEAPON_AMMO_BAG_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Ammo_Bag C_ASW_Weapon_Ammo_Bag
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "asw_shareddefs.h"
#include "basegrenade_shared.h"

// ammo bag stores clips for a number of different weapons
//   holding marine can reload from the bag
//   holding marine can give ammo clips to nearby marines

class CASW_Weapon_Ammo_Bag : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Ammo_Bag, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Ammo_Bag();
	
	Activity	GetPrimaryAttackActivity( void );

	void	PrimaryAttack();

	void ThrowAmmo();

	#ifndef CLIENT_DLL
		DECLARE_DATADESC();

		int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

		virtual const char* GetPickupClass() { return "asw_pickup_ammo_bag"; }
		void GiveClipTo(CASW_Marine *pTargetMarine, int iAmmoType, bool bSuppressSound=false);
		virtual void OnMarineDamage(const CTakeDamageInfo &info);	// marine was hurt when holding this weapon
		float m_fBurnDamage;

		virtual int AddAmmo(int iBullets, int iAmmoIndex);
		virtual bool DropAmmoPickup(int iAmmoType);
	#else
		virtual void MouseOverEntity(C_BaseEntity *pEnt, Vector vecWorldCursor);
	#endif

	virtual bool IsOffensiveWeapon() { return false; }

	// these differ because of server latency 
	static float GetAIMaxAmmoGiveDistance() { return 50.0f; }
	static float GetPlayerMaxAmmoGiveDistance() { return 300.0f; }

	virtual bool PrimaryAmmoLoaded( void );
	bool CanGiveAmmoToWeapon(CASW_Weapon *pWeapon);
	bool BagHasAmmo(int iAmmoType);
	int NumClipsForWeapon(CASW_Weapon *pWeapon);
	int FindBagIndexForAmmoType(int iAmmoType);
	int SelectAmmoTypeToGive(CASW_Marine *pOtherMarine);
	bool HasRoomForAmmo(int iAmmoType);

	virtual int AmmoCount(int index);
	virtual int MaxAmmoCount(int index);

	CNetworkArray( int, m_AmmoCount, ASW_AMMO_BAG_SLOTS );
	int m_MaxAmmoCount[ASW_AMMO_BAG_SLOTS];

	void DebugContents();

	int m_iAmmoType[ASW_AMMO_BAG_SLOTS];
	char m_szWeaponClass[ASW_AMMO_BAG_SLOTS][64];
	char m_szAmmoClass[ASW_AMMO_BAG_SLOTS][64];

	enum {
		ASW_BAG_SLOT_RIFLE = 0,
		ASW_BAG_SLOT_AUTOGUN,
		ASW_BAG_SLOT_SHOTGUN,
		ASW_BAG_SLOT_ASSAULT_SHOTGUN,		
		ASW_BAG_SLOT_FLAMER,
		//ASW_BAG_SLOT_RAILGUN,
		ASW_BAG_SLOT_PDW,
		ASW_BAG_SLOT_PISTOL
	};

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_AMMO_BAG; }
};


#endif /* _INCLUDED_ASW_WEAPON_AMMO_BAG_H */
