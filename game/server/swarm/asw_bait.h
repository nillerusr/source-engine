#ifndef _INCLUDED_ASW_BAIT_H
#define _INCLUDED_ASW_BAIT_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"

class CASW_Bait : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CASW_Bait, CBaseCombatCharacter );
	DECLARE_DATADESC();

	CASW_Bait();
	virtual ~CASW_Bait();

public:
	void	Spawn( void );
	void	Precache( void );

	unsigned int PhysicsSolidMaskForEntity() const { return MASK_NPCSOLID; }
	void	BaitTouch( CBaseEntity *pOther );
	void	LayFlat();

	static CASW_Bait* Bait_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner);

	float GetDuration() { return m_flTimeBurnOut; }
	void SetDuration( float fDuration ) { m_flTimeBurnOut = fDuration; }

protected:
	bool	m_inSolid;	

public:
	void	Start( float lifeTime );
	void	Die( float fadeTime );
	void	Launch( const Vector &direction, float speed );

	virtual Class_T Classify( void ) { return (Class_T) CLASS_ASW_BAIT; }

	void	BaitThink( void );

	float m_flTimeBurnOut;
	int m_nBounces;
};

#endif // _INCLUDED_ASW_BAIT_H
