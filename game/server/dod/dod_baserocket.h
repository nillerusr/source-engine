//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_BASEROCKET_H
#define DOD_BASEROCKET_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "baseanimating.h"
#include "smoke_trail.h"
#include "weapon_dodbase.h"

class RocketTrail;
 
//================================================
// CDODBaseRocket	
//================================================
class CDODBaseRocket : public CBaseAnimating
{
	DECLARE_CLASS( CDODBaseRocket, CBaseAnimating );

public:
	CDODBaseRocket();
	~CDODBaseRocket();
	
	void	Spawn( void );
	void	Precache( void );
	void	RocketTouch( CBaseEntity *pOther );
	void	Explode( void );
	void	Fire( void );
	
	virtual float	GetDamage() { return m_flDamage; }
	virtual void	SetDamage(float flDamage) { m_flDamage = flDamage; }

	unsigned int PhysicsSolidMaskForEntity( void ) const;

	static CDODBaseRocket *Create( const char *szClassname, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner );	

	void SetupInitialTransmittedGrenadeVelocity( const Vector &velocity );

	virtual DODWeaponID GetEmitterWeaponID() { return WEAPON_NONE; Assert(0); }

protected:
	virtual void DoExplosion( trace_t *pTrace );

	void FlyThink( void );

	float					m_flDamage;

	CNetworkVector( m_vInitialVelocity );

private:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	float m_flCollideWithTeammatesTime;
	bool m_bCollideWithTeammates;
};

#endif // DOD_BASEROCKET_H