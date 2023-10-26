#ifndef _DEFINED_ASW_BOOMER_BLOB_H
#define _DEFINED_ASW_BOOMER_BLOB_H
#pragma once

#include "asw_grenade_vindicator.h"

extern CUtlVector<CBaseEntity*> g_aExplosiveProjectiles;

class CASW_Boomer_Blob : public CASW_Grenade_Vindicator
{
public:
	DECLARE_CLASS( CASW_Boomer_Blob, CASW_Grenade_Vindicator );

	CASW_Boomer_Blob();
	virtual ~CASW_Boomer_Blob();

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	static CASW_Boomer_Blob *Boomer_Blob_Create( float flDamage, float fRadius, int iClusters, const Vector &position, const QAngle &angles, const Vector &velocity, 
												 const AngularImpulse &angVelocity, CBaseEntity *pOwner );	

	IMPLEMENT_AUTO_LIST_GET();

	virtual void	Spawn( );
	virtual void	Detonate( );
	virtual void	Precache( );
	virtual void	SetFuseLength( float fSeconds );
	virtual void	SetClusters(int iClusters, bool bMaster = false);
	virtual void	CreateEffects();

private:
			void			Touch( CBaseEntity *pOther );
			void			CheckNearbyTargets( );
			float			GetEarliestAOEDetonationTime();
			void			DoExplosion( );

	float			m_fDetonateTime;
	float			m_fEarliestAOEDetonationTime;
	bool			m_bModelOpening;
};


#endif // _DEFINED_ASW_BOOMER_BLOB_H
