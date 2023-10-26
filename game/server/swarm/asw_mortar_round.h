#ifndef _DEFINED_ASW_MORTAR_ROUND_H
#define _DEFINED_ASW_MORTAR_ROUND_H
#pragma once

#include "asw_grenade_vindicator.h"

enum ASW_Mortar_t
{
	ASW_MORTAR_EXPLOSIVE,
	ASW_MORTAR_FREEZE,
};

class CASW_Mortar_Round : public CASW_Grenade_Vindicator
{
public:
	DECLARE_CLASS( CASW_Mortar_Round, CASW_Grenade_Vindicator );

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	static CASW_Mortar_Round *Mortar_Round_Create( float flDamage, float fRadius, int iClusters, const Vector &position, const QAngle &angles, const Vector &velocity, 
												   const AngularImpulse &angVelocity, CBaseEntity *pOwner, ASW_Mortar_t nExplosionType = ASW_MORTAR_EXPLOSIVE );	

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

			void			SetExplosionType( ASW_Mortar_t type ) { m_nGrenadeExplosionType = type; }
			ASW_Mortar_t	GetExplosionType() { return m_nGrenadeExplosionType; }


	float			m_fDetonateTime;
	float			m_fEarliestAOEDetonationTime;
	ASW_Mortar_t	m_nGrenadeExplosionType;
	bool			m_bModelOpening;
};


#endif // _DEFINED_ASW_MORTAR_ROUND_H
