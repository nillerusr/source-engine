#ifndef _INCLUDED_ASW_GRENADE_FREEZE_H
#define _INCLUDED_ASW_GRENADE_FREEZE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_grenade_cluster.h"

class CASW_Grenade_Freeze : public CASW_Grenade_Cluster
{
public:
	DECLARE_CLASS( CASW_Grenade_Freeze, CASW_Grenade_Cluster );
	DECLARE_DATADESC();

	virtual void Precache();
	virtual void DoExplosion();
	virtual void CreateEffects();

	static CASW_Grenade_Freeze *Freeze_Grenade_Create( float flDamage, float flFreezeAmount, float fRadius, int iClusters, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pCreatorWeapon );

	float m_flFreezeAmount;
};

#endif // _INCLUDED_ASW_GRENADE_FREEZE_H
