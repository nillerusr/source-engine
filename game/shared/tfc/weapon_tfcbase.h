//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_TFCBASE_H
#define WEAPON_TFCBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "tfc_playeranimstate.h"
#include "tfc_weapon_parse.h"

#if defined( CLIENT_DLL )
	#define CWeaponTFCBase C_WeaponTFCBase
#endif

class CTFCPlayer;


// Given an ammo type (like from a weapon's GetPrimaryAmmoType()), this compares it
// against the ammo name you specify.
// MIKETODO: this should use indexing instead of searching and strcmp()'ing all the time.
bool IsAmmoType( int iAmmoType, const char *pAmmoName );


typedef enum
{
	WEAPON_NONE = 0,

	// Melee
	WEAPON_CROWBAR,
	WEAPON_SPANNER,		// Engineer's wrench.
	WEAPON_KNIFE,
	WEAPON_MEDIKIT,

	// Vector weapons
	WEAPON_MINIGUN,

	// Shotguns
	WEAPON_SHOTGUN,
	WEAPON_SUPER_SHOTGUN,

	WEAPON_NAILGUN,
	WEAPON_SUPER_NAILGUN,

	WEAPON_MAX,			// number of weapons weapon index

} TFCWeaponID;


//Class Heirarchy for tfc weapons

/*

  CWeaponTFCBase
	|
	|--> CTFCCrowbar
	|		|
	|		|--> CTFCKnife
	|		|--> CTFCMedikit
	|		|--> CTFCSpanner
	|		|--> CTFCMedikit
	|
	|--> CTFCMinigun
	|
	|--> CTFCShotgun
	|		|
	|		|--> CTFCSuperShotgun
	|
	|--> CTFCNailgun
	|		|
	|		|--> CTFCSuperNailgun
					
*/
class CWeaponTFCBase : public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponTFCBase, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponTFCBase();

	virtual void Precache();
	virtual bool IsPredicted() const;

	CTFCPlayer* GetPlayerOwner() const;

	// Get TFC-specific weapon data.
	CTFCWeaponInfo const	&GetTFCWpnData() const;

	// Get specific TFC weapon ID (ie: WEAPON_AK47, etc)
	virtual TFCWeaponID GetWeaponID( void ) const;

	// return true if this weapon is an instance of the given weapon type (ie: "IsA" WEAPON_GLOCK)
	bool IsA( TFCWeaponID id ) const;

	// return true if this weapon has a silencer equipped
	virtual bool IsSilenced( void ) const;


#ifdef CLIENT_DLL

#else

	DECLARE_DATADESC();

	virtual void Spawn();
	virtual bool DefaultReload( int iClipSize1, int iClipSize2, int iActivity );
	void SendReloadSoundEvent();
	
	virtual Vector GetSoundEmissionOrigin() const;

#endif


private:
	
	CWeaponTFCBase( const CWeaponTFCBase & );
};


#endif // WEAPON_TFCBASE_H
