//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HL1_ENTS_H
#define HL1_ENTS_H
#ifdef _WIN32
#pragma once
#endif


/**********************
	    Pendulum
/**********************/

class CPendulum : public CBaseToggle
{
	DECLARE_CLASS( CPendulum, CBaseToggle );
public:
	void	Spawn ( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Swing( void );
	void	PendulumUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Stop( void );
	void	Touch( CBaseEntity *pOther );
	void	RopeTouch ( CBaseEntity *pOther );// this touch func makes the pendulum a rope
	void	Blocked( CBaseEntity *pOther );

	// Input handlers.
	void	InputActivate( inputdata_t &inputdata );

	DECLARE_DATADESC();
	
	float	m_flAccel;			// Acceleration
	float	m_flTime;
	float	m_flDamp;
	float	m_flMaxSpeed;
	float	m_flDampSpeed;
	QAngle	m_vCenter;
	QAngle	m_vStart;
	float   m_flBlockDamage;

	EHANDLE		m_hEnemy;
};

class CHL1Gib : public CBaseEntity
{
	DECLARE_CLASS( CHL1Gib, CBaseEntity );

public:
	void Spawn( const char *szGibModel );
	void BounceGibTouch ( CBaseEntity *pOther );
	void StickyGibTouch ( CBaseEntity *pOther );
	void WaitTillLand( void );
	void LimitVelocity( void );

	virtual int	ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }
	static	void SpawnHeadGib( CBaseEntity *pVictim );
	static	void SpawnRandomGibs( CBaseEntity *pVictim, int cGibs, int human );
	static  void SpawnStickyGibs( CBaseEntity *pVictim, Vector vecOrigin, int cGibs );

	int		m_bloodColor;
	int		m_cBloodDecals;
	int		m_material;
	float	m_lifeTime;

	DECLARE_DATADESC();
};


#endif // HL1_ENTS_H 
