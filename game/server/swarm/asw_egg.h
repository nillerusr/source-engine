#ifndef _INCLUDED_ASW_EGG_H
#define _INCLUDED_ASW_EGG_H
#pragma once

#include "asw_shareddefs.h"

class CASW_Parasite;

class CASW_Egg : public CBaseFlex
{
public:
	DECLARE_CLASS( CASW_Egg, CBaseFlex );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void Precache();

	CASW_Egg();
	virtual ~CASW_Egg();
	void	Spawn( void );
	Class_T		Classify( void ) { return (Class_T) CLASS_ASW_EGG; }
	void	AnimThink( void );
	void	SetupParasiteThink();
// 	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );
// 	int UpdateTransmitState();
	virtual bool CreateVPhysics();
	virtual void ReachedEndOfSequence();

	void Open(CBaseEntity* pOther);
	void Hatch(CBaseEntity* pOther);
	void EggTouch(CBaseEntity* pOther);
	void CheckEggSize();
	void SpawnEffects(int flags);
	void ResetEgg();
	
	bool m_bOpen, m_bHatched, m_bOpening;
	float m_fHatchTime, m_fEggResetTime;
	string_t m_sParasiteClass;
	bool m_bStoredEggSize;
	Vector m_vecStartSurroundMins, m_vecStartSurroundMaxs;
	bool m_bMadeParasiteVisible;
	bool m_bFixedJumpDirection;
	float m_fNextMarineCheckTime;
	bool m_bSmallOpenRadius;
	CNetworkVar( float, m_fEggAwake );	// controls green lines on the outside

	void ParasiteDied(CASW_Parasite* pParasite);
	
	CASW_Parasite* GetParasite();
	EHANDLE m_hParasite;

	// damage related
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon = NULL );
	virtual void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );
	virtual void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual void Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr );
	
	EHANDLE m_hBurner;
	EHANDLE m_hBurnerWeapon;
	CNetworkVar(bool, m_bOnFire);

	static float s_fNextSpottedChatterTime;
	bool m_bSkipEggChatter;

	// i/o
	COutputEvent m_OnOpened;
	COutputEvent m_OnHatched;
	COutputEvent m_OnDestroyed;
	COutputEvent m_OnEggReset;
	void InputOpen( inputdata_t &inputdata );
	void InputHatch( inputdata_t &inputdata );
};


#endif /* _INCLUDED_ASW_EGG_H */