#ifndef _INCLUDED_ASW_FIRE_H
#define _INCLUDED_ASW_FIRE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"

class CASW_BurnInfo
{
public:
	CASW_BurnInfo(CBaseEntity *pEntity, CBaseEntity *pAttacker, float fDieTime, float fBurnInterval, float fDamagePerInterval, CBaseEntity *pDamagingWeapon = NULL );

	EHANDLE m_hEntity;
	EHANDLE m_hAttacker;
	EHANDLE m_hDamagingWeapon;
	float m_fNextBurnTime;
	float m_fDieTime;
	float m_fBurnInterval;
	float m_fDamagePerInterval;
};

class CASW_Burning : public CBaseEntity
{
public:
	DECLARE_CLASS( CASW_Burning, CBaseEntity );
	DECLARE_DATADESC();

	CASW_Burning();
	virtual ~CASW_Burning();

	void Spawn();
	void Reset();
	void FireThink();
	void SetNextThinkTime();
	void Precache();

	void BurnEntity(CBaseEntity *pEntity, CBaseEntity *pAttacker, float fFireDuration, float fBurnInterval, float fDamagePerInterval, CBaseEntity *pDamagingWeapon = NULL );
	void Extinguish(CBaseEntity *pEntity);

	//void OnEntityExtinguished(CBaseEntity *pEntity);

	CUtlVector<CASW_BurnInfo*> m_Burning;

	// Classification
	virtual Class_T		Classify( void ) { return (Class_T) CLASS_ASW_BURNING; }
};

CASW_Burning* ASWBurning();

#endif // _INCLUDED_ASW_FIRE_H