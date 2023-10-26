#ifndef _INCLUDED_ASW_WEAPON_SENTRY_H
#define _INCLUDED_ASW_WEAPON_SENTRY_H
#pragma once

#ifdef CLIENT_DLL
#include "c_asw_weapon.h"
#define CASW_Weapon C_ASW_Weapon
#define CASW_Weapon_Sentry C_ASW_Weapon_Sentry
#define CASW_Weapon_Sentry_Flamer C_ASW_Weapon_Sentry_Flamer
#define CASW_Weapon_Sentry_Cannon C_ASW_Weapon_Sentry_Cannon
#define CASW_Weapon_Sentry_Freeze C_ASW_Weapon_Sentry_Freeze
#define CASW_Marine C_ASW_Marine
#else
#include "asw_weapon.h"
#include "npc_combine.h"
#endif

#include "basegrenade_shared.h"
#include "asw_shareddefs.h"

class CASW_Weapon_Sentry : public CASW_Weapon
{
public:
	DECLARE_CLASS( CASW_Weapon_Sentry, CASW_Weapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Sentry();
	virtual ~CASW_Weapon_Sentry();
	void Precache();
	
	Activity	GetPrimaryAttackActivity( void );

	virtual void	PrimaryAttack();
	virtual bool	OffhandActivate();
	virtual void	Drop( const Vector &vecVelocity );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	bool			FindValidSentrySpot( void );
	void			DeploySentry();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	void	SetSentryAmmo( int nAmmo ) { m_nSentryAmmo = nAmmo; }

	virtual const char* GetPickupClass() { return "asw_pickup_sentry"; }

#else
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void UpdateOnRemove();
	virtual void ClientThink();
	void DestroySentryBuildDisplay( CASW_Marine *pMarine );
#endif

	virtual bool IsOffensiveWeapon() { return false; }

protected:
	int m_iSentryMunitionType;
	Vector m_vecValidSentrySpot;
	QAngle m_angValidSentryFacing;
	EHANDLE m_hValidSentryParent;

	CNetworkVar(bool, m_bDisplayValid);

#ifndef CLIENT_DLL
	int m_nSentryAmmo;
#else
	float m_flNextDeployCheckThink;
	bool m_bDisplayActive;
	EHANDLE m_hOwningMarine; // need to store this so we can destroy the effect on the marine
#endif

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SENTRY_GUN_CASE; }
};


class CASW_Weapon_Sentry_Flamer : public CASW_Weapon_Sentry
{
public:
	DECLARE_CLASS( CASW_Weapon_Sentry_Flamer, CASW_Weapon_Sentry );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Sentry_Flamer();

#ifndef CLIENT_DLL
	virtual const char* GetPickupClass() { return "asw_pickup_sentry_flamer"; }
#endif

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SENTRY_FLAMER_CASE; }
};

class CASW_Weapon_Sentry_Cannon : public CASW_Weapon_Sentry
{
public:
	DECLARE_CLASS( CASW_Weapon_Sentry_Cannon, CASW_Weapon_Sentry );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Sentry_Cannon();

#ifndef CLIENT_DLL
	virtual const char* GetPickupClass() { return "asw_pickup_sentry_cannon"; }
#endif

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SENTRY_CANNON_CASE; }
};

class CASW_Weapon_Sentry_Freeze : public CASW_Weapon_Sentry
{
public:
	DECLARE_CLASS( CASW_Weapon_Sentry_Freeze, CASW_Weapon_Sentry );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CASW_Weapon_Sentry_Freeze();

#ifndef CLIENT_DLL
	virtual const char* GetPickupClass() { return "asw_pickup_sentry_freeze"; }
#endif

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_SENTRY_FREEZE_CASE; }
};

#endif /* _INCLUDED_ASW_WEAPON_SENTRY_H */
