#ifndef _DEFINED_ASW_BUFFGREN_PROJECTILE_H
#define _DEFINED_ASW_BUFFGREN_PROJECTILE_H
#pragma once

#include "asw_aoegrenade_projectile.h"

class CASW_Skill_Details;

class CASW_BuffGrenade_Projectile : public CASW_AOEGrenade_Projectile
{
	DECLARE_CLASS( CASW_BuffGrenade_Projectile, CASW_AOEGrenade_Projectile );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:

	CASW_BuffGrenade_Projectile();

	void Precache( void );

	static CASW_BuffGrenade_Projectile* Grenade_Projectile_Create( const Vector &position, const QAngle &angles, const Vector &velocity,
		const AngularImpulse &angVelocity, CBaseEntity *pOwner,
		float flRadius, float flDuration );

	virtual float GetGrenadeGravity( void );

	virtual bool ShouldTouchEntity( CBaseEntity *pEntity );

	virtual void StartAOE( CBaseEntity *pEntity );
	virtual bool StopAOE( CBaseEntity *pEntity );

	int GetBuffedMarineCount() { return m_hBuffedMarines.Count(); }

protected:
	// if this buff grenade was deployed by a marine, these describe the skill used
	CASW_Skill_Details *m_pSkill;
	int m_iSkillPoints;

	CUtlVector<EHANDLE> m_hBuffedMarines;
};

#endif // _DEFINED_ASW_BUFFGREN_PROJECTILE_H
