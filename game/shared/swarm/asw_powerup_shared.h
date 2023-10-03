#ifndef _INCLUDED_ASW_POWERUP_H
#define _INCLUDED_ASW_POWERUP_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_asw_pickup.h"
#define CASW_Powerup C_ASW_Powerup
#else
#include "asw_pickup.h"
#endif

class CASW_Powerup : public CASW_Pickup
{
	DECLARE_CLASS( CASW_Powerup, CASW_Pickup );
	DECLARE_NETWORKCLASS();
public:
	CASW_Powerup(void);
	virtual ~CASW_Powerup(void);

#ifndef CLIENT_DLL
	virtual void Spawn();
	virtual void Precache();
	//virtual void ItemTouch( CBaseEntity *pOther );
	virtual void PickupPowerup( CASW_Marine *pMarine );

	virtual bool IsUsable(CBaseEntity *pUser) { return true; }
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );
	virtual void SetIsSpawnFlipping( float flMaxPickupDelay );

	virtual const char* GetPowerupModelName( void ) { return "models/swarm/Ammo/ammorailgun.mdl"; }
	virtual const char* GetPickupSoundName( void ) { return "ASW_Powerup.Pickup_Generic"; }
	virtual const char* GetIdleParticleName( void ) { return "powerup_freeze_bullets"; }
	virtual const char* GetPickupParticleName( void ) { return "powerup_pickup_generic"; }
#else
	virtual int GetUseIconTextureID() { BaseClass::GetUseIconTextureID(); return s_nUseIconTakePowerup_FireB; }
#endif

	virtual bool AllowedToPickup(CASW_Marine *pMarine) { return true; }

	float m_flCanPickupMaxTime;
	bool m_bSpawnDelayDenyPickup : 1;
	int m_nPowerupType;
	float m_flPowerupDuration;

};

class CASW_Powerup_Bullets : public CASW_Powerup
{
public:
	DECLARE_CLASS( CASW_Powerup_Bullets, CASW_Powerup );
	DECLARE_NETWORKCLASS();

#ifndef CLIENT_DLL
	virtual void ActivateUseIcon( CASW_Marine* pMarine, int nHoldType );
#endif

	virtual bool AllowedToPickup(CASW_Marine *pMarine);
};

class CASW_Powerup_Freeze_Bullets : public CASW_Powerup_Bullets
{
public:
	DECLARE_CLASS( CASW_Powerup_Freeze_Bullets, CASW_Powerup_Bullets );
	DECLARE_NETWORKCLASS();

	CASW_Powerup_Freeze_Bullets();

#ifndef CLIENT_DLL
	//virtual const char* GetPickupSoundName( void ) { return "ASW_Powerup.Pickup_Generic"; }
	virtual const char* GetIdleParticleName( void ) { return "powerup_freeze_bullets"; }
	virtual const char* GetPickupParticleName( void ) { return "powerup_pickup_generic"; }
#else
	virtual int GetUseIconTextureID() { BaseClass::GetUseIconTextureID(); return s_nUseIconTakePowerup_FreezeB; }
#endif
};

class CASW_Powerup_Fire_Bullets : public CASW_Powerup_Bullets
{
public:
	DECLARE_CLASS( CASW_Powerup_Fire_Bullets, CASW_Powerup_Bullets );
	DECLARE_NETWORKCLASS();

	CASW_Powerup_Fire_Bullets();

#ifndef CLIENT_DLL
	//virtual const char* GetPickupSoundName( void ) { return "ASW_BuffGrenade.GrenadeActivate"; }
	virtual const char* GetIdleParticleName( void ) { return "powerup_fire_bullets"; }
	virtual const char* GetPickupParticleName( void ) { return "powerup_pickup_generic"; }
#else
	virtual int GetUseIconTextureID() { BaseClass::GetUseIconTextureID(); return s_nUseIconTakePowerup_FireB; }
#endif
};

class CASW_Powerup_Electric_Bullets : public CASW_Powerup_Bullets
{
public:
	DECLARE_CLASS( CASW_Powerup_Electric_Bullets, CASW_Powerup_Bullets );
	DECLARE_NETWORKCLASS();

	CASW_Powerup_Electric_Bullets();

#ifndef CLIENT_DLL
	//virtual const char* GetPickupSoundName( void ) { return "ASW_BuffGrenade.GrenadeActivate"; }
	virtual const char* GetIdleParticleName( void ) { return "powerup_electric_bullets"; }
	virtual const char* GetPickupParticleName( void ) { return "powerup_pickup_generic"; }
#else
	virtual int GetUseIconTextureID() { BaseClass::GetUseIconTextureID(); return s_nUseIconTakePowerup_ElectricB; }
#endif
};

class CASW_Powerup_Chemical_Bullets : public CASW_Powerup_Bullets
{
public:
	DECLARE_CLASS( CASW_Powerup_Chemical_Bullets, CASW_Powerup_Bullets );
	DECLARE_NETWORKCLASS();

	CASW_Powerup_Chemical_Bullets();

#ifndef CLIENT_DLL
	//virtual const char* GetPickupSoundName( void ) { return "ASW_BuffGrenade.GrenadeActivate"; }
	virtual const char* GetIdleParticleName( void ) { return "powerup_chemical_bullets"; }
	virtual const char* GetPickupParticleName( void ) { return "powerup_pickup_generic"; }
#else
	virtual int GetUseIconTextureID() { BaseClass::GetUseIconTextureID(); return s_nUseIconTakePowerup_ChemicalB; }
#endif
};

class CASW_Powerup_Explosive_Bullets : public CASW_Powerup_Bullets
{
public:
	DECLARE_CLASS( CASW_Powerup_Explosive_Bullets, CASW_Powerup_Bullets );
	DECLARE_NETWORKCLASS();

	CASW_Powerup_Explosive_Bullets();

#ifndef CLIENT_DLL
	//virtual const char* GetPickupSoundName( void ) { return "ASW_BuffGrenade.GrenadeActivate"; }
	virtual const char* GetIdleParticleName( void ) { return "powerup_explosive_bullets"; }
	virtual const char* GetPickupParticleName( void ) { return "powerup_pickup_generic"; }
#else
	virtual int GetUseIconTextureID() { BaseClass::GetUseIconTextureID(); return s_nUseIconTakePowerup_ExplodeB; }
#endif
};

class CASW_Powerup_Increased_Speed : public CASW_Powerup
{
public:
	DECLARE_CLASS( CASW_Powerup_Increased_Speed, CASW_Powerup );
	DECLARE_NETWORKCLASS();

	CASW_Powerup_Increased_Speed();

#ifndef CLIENT_DLL
	//virtual const char* GetPickupSoundName( void ) { return "ASW_BuffGrenade.GrenadeActivate"; }
	virtual const char* GetIdleParticleName( void ) { return "powerup_increased_speed"; }
	virtual const char* GetPickupParticleName( void ) { return "powerup_pickup_generic"; }
#else
	virtual int GetUseIconTextureID() { BaseClass::GetUseIconTextureID(); return s_nUseIconTakePowerup_Speed; }
#endif
};

#endif // _INCLUDED_ASW_POWERUP_H