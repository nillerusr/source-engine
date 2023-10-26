#ifndef _INCLUDED_ASW_GRENADE_CLUSTER_H
#define _INCLUDED_ASW_GRENADE_CLUSTER_H
#pragma once

#include "asw_grenade_vindicator.h"

class CASW_Grenade_Cluster : public CASW_Grenade_Vindicator
{
public:
	DECLARE_CLASS( CASW_Grenade_Cluster, CASW_Grenade_Vindicator );
	DECLARE_DATADESC();
				
	virtual void Spawn();
	virtual void DoExplosion();
	virtual void Detonate();
	virtual void Precache();
	virtual void CheckNearbyDrones();
	virtual void SetFuseLength(float fSeconds);
	static CASW_Grenade_Cluster *Cluster_Grenade_Create( float flDamage, float fRadius, int iClusters, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon );	
	virtual float GetEarliestAOEDetonationTime();
	virtual void SetClusters(int iClusters, bool bMaster = false);

	float m_fDetonateTime;
	float m_fEarliestAOEDetonationTime;
	Class_T m_CreatorWeaponClass;

	// Classification
	virtual Class_T Classify( void ) { return (Class_T)CLASS_ASW_GRENADE_CLUSER; }
};

#endif // _INCLUDED_ASW_GRENADE_CLUSTER_H
