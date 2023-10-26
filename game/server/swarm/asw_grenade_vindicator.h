#ifndef _DEFINED_ASW_GRENADE_VINDICATOR_H
#define _DEFINED_ASW_GRENADE_VINDICATOR_H
#pragma once

#ifdef CLIENT_DLL
#define CBaseEntity C_BaseEntity
#endif

#include "asw_rifle_grenade.h"

class CASW_Grenade_Vindicator : public CASW_Rifle_Grenade
{
public:
	DECLARE_CLASS( CASW_Grenade_Vindicator, CASW_Rifle_Grenade );

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif
					
	CASW_Grenade_Vindicator();
	virtual ~CASW_Grenade_Vindicator( void );
	void Spawn();
	void Precache();
	
	virtual void Detonate();
	virtual	bool		AllowedToIgnite( void ) { return true; }
	void VGrenadeTouch( CBaseEntity *pOther );
	static CASW_Grenade_Vindicator *Vindicator_Grenade_Create( float flDamage, float fRadius, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon );	
	virtual void CreateEffects();
	virtual void KillEffects();
	virtual float GetEarliestTouchDetonationTime();

	virtual void SetFuseLength(float fSeconds);
	virtual void SetClusters(int iClusters, bool bMaster = false) { m_iClusters = iClusters; m_bMaster = bMaster;}
	virtual int OnTakeDamage_Dying( const CTakeDamageInfo &info );
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void BurntAlien(CBaseEntity *pAlien);
	void SetExplodeOnWorldContact( bool bExplode ) { m_bExplodeOnWorldContact = bExplode; }		// if set, grenade will explode on the first thing it touches

	int m_iClusters;
	bool m_bMaster;
	float m_fEarliestTouchDetonationTime;

	EHANDLE m_hFirer;

	bool m_bKicked;
	bool m_bExplodeOnWorldContact;

	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSpriteTrail>	m_pGlowTrail;

	// Classification
	virtual Class_T Classify( void ) { return (Class_T)CLASS_ASW_GRENADE_VINDICATOR; }
};


#endif // _DEFINED_ASW_GRENADE_VINDICATOR_H
