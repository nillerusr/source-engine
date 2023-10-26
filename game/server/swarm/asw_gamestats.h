//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//=============================================================================//
#ifndef ASW_GAMESTATS_H
#define ASW_GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "GameStats.h"
#include "asw_gamestats_shared.h"
#include "GameEventListener.h"
#include "utlvector.h"
#include "asw_player_shared.h"

// Forward declarations
class CASW_Player;
class CASW_Marine;
struct FailAdviceMessageStatus_t;
class CASW_BurnInfo;

//=============================================================================
//
// Class for tracking per-campaign server side stats
//
class CASWCampaignStatTracker
{
public:
	// Events
	void Event_LevelInit( void ) { }
	void Event_LevelShutdown( float flElapsed ) { }
	void Event_MarineKilled( CASW_Marine *pMarine, const CTakeDamageInfo &info ) { }
	void Event_MarineTookDamage( CASW_Marine *pMarine, const CTakeDamageInfo &info ) { }
	void Event_MarineBreadcrumb( CASW_Marine *pMarine ) { }
	void Event_MarineReloading( CASW_Marine *pMarine, const char *szWeaponClass ) { }
	void Event_MarineTookPickup( CASW_Marine *pMarine, const char *szPickupClass, const char *szDroppedClass ) { }
	void Event_AlienSpawned( CBaseEntity *pAlien ) { }
	void Event_AlienKilled( CBaseEntity *pAlien, const CTakeDamageInfo &info ) { }
	void Event_MissionStarted() { }
	void Event_MissionComplete( bool bSuccess ) { }

};
//=============================================================================


//=============================================================================
//
// Class for tracking per-mission server side stats
//
class CASWMissionStatTracker
{
public:
	CASWMissionStatTracker() { }

	// Events
	void Event_LevelInit( void ) { }
	void Event_LevelShutdown( float flElapsed ) { }
	void Event_MarineKilled( CASW_Marine *pMarine, const CTakeDamageInfo &info ) { }
	void Event_MarineTookDamage( CASW_Marine *pMarine, const CTakeDamageInfo &info ) { }
	void Event_MarineBreadcrumb( CASW_Marine *pMarine ) { }
	void Event_MarineReloading( CASW_Marine *pMarine, CBaseEntity *pWeapon ) { }
	void Event_MarineTookPickup( CASW_Marine *pMarine, CBaseEntity *pPickupClass, CBaseEntity *pDroppedClass ) { }
	void Event_AlienSpawned( CBaseEntity *pAlien ) { }
	void Event_AlienKilled( CBaseEntity *pAlien, const CTakeDamageInfo &info ) { }
	void Event_AlienTookDamage( CBaseEntity *pAlien, const CTakeDamageInfo &info ) { }
	void Event_EntityBurned( CBaseEntity *pEntity, float fTotalDamage, CBaseEntity *pBurningWeapon = NULL ) { }
	void Event_MissionStarted( void ) { }
	void Event_MissionComplete(  bool bSuccess, int iFinalFailAdvice, const FailAdviceMessageStatus_t *pFailStatus  ) { }
	void Event_MarineHealed( CASW_Marine *pMarine, int amount, CBaseEntity* pHealingWeapon = NULL ) { }
	void Event_MarineWeaponFired( const CBaseEntity *pWeapon, const CASW_Marine *pMarine, int nShotsFired, bool bIsSecondary ) { }
	void Event_MarinesSpawned( void ) { }
	void OnSessionStart( void ) { }
	void OnSessionEnd( void ) { }

	// Send all of the accumulated stats for the current mission
	virtual void SubmitGameStats( KeyValues *pKV ) { }

};
//=============================================================================

//=============================================================================
//
// Infested Game Stats Class
//
class CASWGameStats : public CBaseGameStats, public CGameEventListener, public CAutoGameSystem
{
public:

	// Constructor/Destructor.
	CASWGameStats( void ) { }
	~CASWGameStats( void ) { }

	virtual void Clear( void ) { }
	virtual bool UseOldFormat() { return false; }
	virtual bool AddDataForSend( KeyValues *pKV, StatSendType_t sendType ) { return false; }
	virtual bool Init( void ) { return true; }

	// Events. Forwarded to Mission Tracker and Campaign Tracker as needed.
	virtual void Event_LevelInit( void ) { }
	virtual void Event_LevelShutdown( float flElapsed ) { }

	void Event_MarineKilled( CASW_Marine *pMarine, const CTakeDamageInfo &info ) { }
	void Event_MarineTookDamage( CASW_Marine *pMarine, const CTakeDamageInfo &info ) { }
	void Event_MarineBreadcrumb( CASW_Marine *pMarine ) { }
	void Event_MarineReloading( CASW_Marine *pMarine, CBaseEntity *pWeapon ) { }
	void Event_MarineTookPickup( CASW_Marine *pMarine, CBaseEntity *pPickupClass, CBaseEntity *pDroppedClass ) { }
	void Event_AlienSpawned( CBaseEntity *pAlien ) { }
	void Event_AlienKilled( CBaseEntity *pAlien, const CTakeDamageInfo &info ) { }
	void Event_AlienTookDamage( CBaseEntity *pAlien, const CTakeDamageInfo &info ) { }
	void Event_EntityBurned( CBaseEntity *pEntity, float fTotalDamage, CBaseEntity *pBurningWeapon = NULL ) { }
	void Event_MissionStarted( void ) { }
	void Event_MissionComplete( bool bSuccess, int iFinalFailAdvice, const FailAdviceMessageStatus_t *pFailStatus ) { }
	void Event_MarineHealed( CASW_Marine *pMarine, int amount, CBaseEntity *pHealingWeapon = NULL ) { }
	void Event_MarineWeaponFired( const CBaseEntity *pWeapon, const CASW_Marine *pMarine, int nShotsFired, bool bIsSecondary = false ) { }
	void Event_MarinesSpawned( void ) { }

	void OnSessionStart( void ) { }
	void OnSessionEnd( void ) { }

	virtual void FireGameEvent( IGameEvent * event ) { }

	void PrintGamestatsMemoryUsage( void ) { }
};

extern CASWGameStats CASW_GameStats;

#endif // ASW_GAMESTATS_H
