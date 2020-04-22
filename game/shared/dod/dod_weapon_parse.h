//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DOD_WEAPON_PARSE_H
#define DOD_WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_parse.h"
#include "networkvar.h"

#define WPN_TYPE_MELEE			(1<<0)
#define WPN_TYPE_GRENADE		(1<<1)			
//#define WPN_TYPE_GRENADE_LIVE	(1<<2)	//exploding grenades, unused
#define WPN_TYPE_PISTOL			(1<<3)			
#define WPN_TYPE_RIFLE			(1<<4)			
#define WPN_TYPE_SNIPER			(1<<5)		
#define WPN_TYPE_SUBMG			(1<<6)			
#define WPN_TYPE_MG				(1<<7)	//mg42, 30cal
#define WPN_TYPE_BAZOOKA		(1<<8)
#define WPN_TYPE_BANDAGE		(1<<9)
#define WPN_TYPE_SIDEARM		(1<<10)	//carbine - secondary weapons
#define WPN_TYPE_RIFLEGRENADE	(1<<11)
#define WPN_TYPE_BOMB			(1<<12)
#define WPN_TYPE_UNKNOWN		(1<<13)
#define WPN_TYPE_CAMERA			(1<<12)

#define WPN_MASK_GUN	( WPN_TYPE_PISTOL | WPN_TYPE_RIFLE | WPN_TYPE_SNIPER | WPN_TYPE_SUBMG | WPN_TYPE_MG | WPN_TYPE_SIDEARM )

//--------------------------------------------------------------------------------------------------------
class CDODWeaponInfo : public FileWeaponInfo_t
{
public:
	DECLARE_CLASS_GAMEROOT( CDODWeaponInfo, FileWeaponInfo_t );
	
	CDODWeaponInfo();
	
	virtual void Parse( ::KeyValues *pKeyValuesData, const char *szWeaponName );

	int		m_iDamage;
	int		m_flPenetration;
	int		m_iBulletsPerShot;
	int		m_iMuzzleFlashType;
	float   m_flMuzzleFlashScale;

	bool	m_bCanDrop;

	float	m_flRecoil;

	float	m_flRange;
	float	m_flRangeModifier;

	float	m_flAccuracy;
	float	m_flSecondaryAccuracy;
	float	m_flAccuracyMovePenalty;

	float	m_flFireDelay;
	float	m_flSecondaryFireDelay;

	int		m_iCrosshairMinDistance;
	int		m_iCrosshairDeltaDistance;

	int		m_WeaponType;

	float	m_flBotAudibleRange;

	char	m_szReloadModel[MAX_WEAPON_STRING];
	char	m_szDeployedModel[MAX_WEAPON_STRING];
	char	m_szDeployedReloadModel[MAX_WEAPON_STRING];
	char	m_szProneDeployedReloadModel[MAX_WEAPON_STRING];

	//timers
	float	m_flTimeToIdleAfterFire;	//wait this long until idling after fire
	float	m_flIdleInterval;			//wait this long after idling to idle again

	//ammo
	int		m_iDefaultAmmoClips;
	int		m_iAmmoPickupClips;

	int		m_iHudClipHeight;
	int		m_iHudClipBaseHeight;
	int		m_iHudClipBulletHeight;

	int		m_iTracerType;

	float	m_flViewModelFOV;

	int		m_iAltWpnCriteria;

	Vector	m_vecViewNormalOffset;
	Vector	m_vecViewProneOffset;
	Vector  m_vecIronSightOffset;

	int		m_iDefaultTeam;
};


#endif // DOD_WEAPON_PARSE_H
