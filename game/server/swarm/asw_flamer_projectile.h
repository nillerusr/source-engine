#ifndef _DEFINED_ASW_FLAMER_PROJECTILE_H
#define _DEFINED_ASW_FLAMER_PROJECTILE_H
#pragma once

#include "asw_shareddefs.h"

#ifdef CLIENT_DLL
#define CBaseEntity C_BaseEntity
#endif

class CSprite;
class CSpriteTrail;

class CASW_Flamer_Projectile : public CBaseCombatCharacter
{
	DECLARE_CLASS( CASW_Flamer_Projectile, CBaseCombatCharacter );

#if !defined( CLIENT_DLL )
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
#endif
					
	CASW_Flamer_Projectile();
	virtual ~CASW_Flamer_Projectile( void );

public:
	void	Spawn( void );
	virtual void UpdateOnRemove();
	void	OnRestore( void );
	void	Precache( void );
	bool	CreateVPhysics( void );
	void	CreateEffects( void );
	void	KillEffects();
	void	CollideThink();
	void FlameHit( CBaseEntity *pOther, const Vector &vecHitPos, bool bOnlyHurtUnignited );
	//void	SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity );
	//void	VPhysicsUpdate( IPhysicsObject *pPhysics );
	unsigned int PhysicsSolidMaskForEntity() const;
	void	ProjectileTouch( CBaseEntity *pOther );
	virtual void PhysicsSimulate( void );
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo );
	
	void SetHurtIgnited( bool bHurtIgnited ) { m_bHurtIgnited = bHurtIgnited; }

	static CASW_Flamer_Projectile *Flamer_Projectile_Create( float flDamage, const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, CBaseEntity *pEntityToCreditForTheDamage = NULL, CBaseEntity *pCreatorWeapon = NULL );

	// Classification
	virtual Class_T Classify( void ) { return (Class_T)CLASS_ASW_FLAMER_PROJECTILE; }
	EHANDLE					m_pGetsCreditedForDamage; /// if the owner is (eg) a turret, set this to the marine so the damage is credited to him.
	EHANDLE					m_hCreatorWeapon; // The weapon that created this projectile
protected:
	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSpriteTrail>	m_pGlowTrail;
	CBaseEntity* m_pLastHitEnt;

	float		m_flDamage;
	bool		m_bHurtIgnited;
	bool		m_inSolid;
	float		m_fDieTime;
	Vector		m_vecOldPos;
};

#endif // _DEFINED_ASW_FLAMER_PROJECTILE_H
