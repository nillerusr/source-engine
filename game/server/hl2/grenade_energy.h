//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot by mortar synth
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	GRENADEENERGY_H
#define	GRENADEENERGY_H

#include "basegrenade_shared.h"
#if 1
#include "Sprite.h"
#include "SpriteTrail.h"
#endif

class CGrenadeEnergy : public CBaseGrenade
{
public:
	DECLARE_CLASS( CGrenadeEnergy, CBaseGrenade );

#if 1
	static void Shoot( CBaseEntity* pOwner, const Vector &vStart, const Vector &vVelocity, QAngle &vShootAng );
#else
	static void Shoot( CBaseEntity* pOwner, const Vector &vStart, Vector vVelocity );
#endif

public:
	void		Spawn( void );
	void		Precache( void );
	void		Animate( void );
	void 		GrenadeEnergyTouch( CBaseEntity *pOther );
	void		Event_Killed( const CTakeDamageInfo &info );

	int			m_flMaxFrame;
	int			m_nEnergySprite;
	float		m_flLaunchTime;		// When was this thing launched

#if 1
	float		m_fDangerRadius;

	virtual void Detonate( void );

private:
	CSprite			*m_pFragSprite;
	CSpriteTrail	*m_pFragTrail;
#else
	void EXPORT				Detonate(void);
#endif

	DECLARE_DATADESC();
};

#endif	//GRENADEENERGY_H
