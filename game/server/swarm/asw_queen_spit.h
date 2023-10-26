#ifndef _INCLUDED_ASW_QUEEN_SPIT_H
#define _INCLUDED_ASW_QUEEN_SPIT_H
#pragma once

class CSprite;
class CSpriteTrail;

class CASW_Queen_Spit : public CBaseCombatCharacter
{
public:
	DECLARE_CLASS( CASW_Queen_Spit, CBaseCombatCharacter );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
					
	virtual ~CASW_Queen_Spit( void );

	void	Spawn( void );
	void	OnRestore( void );
	void	Precache( void );
	bool	CreateVPhysics( void );
	virtual void	CreateEffects( void );
	virtual void	KillEffects();

	virtual void Detonate();
	
	unsigned int PhysicsSolidMaskForEntity() const;
	void	SpitTouch( CBaseEntity *pOther );

	static CASW_Queen_Spit *Queen_Spit_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner );	

protected:

	float	m_flDamage;
	float	m_DmgRadius;
	bool	m_inSolid;	

	int   m_nSpitBurstSprite;
};


#endif // _INCLUDED_ASW_QUEEN_SPIT_H
