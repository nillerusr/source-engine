//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_ROCKET_H
#define GRENADE_ROCKET_H
#ifdef _WIN32
#pragma once
#endif

#define ROCKET_VELOCITY			1000

//====================================================================================
// Purpose: ROCKET LAUNCHER SENTRYGUN'S ROCKETS
//====================================================================================
class CGrenadeRocket : public CBaseAnimating
{
	DECLARE_CLASS( CGrenadeRocket, CBaseAnimating );
public:

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CGrenadeRocket();

	void Spawn( void );
	void Precache( void );
	void MissileTouch( CBaseEntity *pOther );
	void LockOnto( CBaseEntity *pTarget );
	void FollowThink( void );

	// Damage accessors.
	virtual float GetDamage(void)
	{
		return m_flDamage;
	}
	
	virtual void SetDamage(float flDamage)
	{
		m_flDamage = flDamage;
	}

	virtual int GetDamageType() const
	{
		return DMG_BLAST;
	}

	static CGrenadeRocket *CGrenadeRocket::Create( const Vector &vecOrigin, const Vector &vecAngles, edict_t *pentOwner, CBaseEntity *pRealOwner );

public:
	EHANDLE		m_hLockTarget;
	EHANDLE		m_hOwner;
	EHANDLE		m_pRealOwner;
	float		m_flDamage;
};

#endif // GRENADE_ROCKET_H
