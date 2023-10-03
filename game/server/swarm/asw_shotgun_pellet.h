#ifndef _DEFINED_ASW_SHOTGUN_PELLET_H
#define _DEFINED_ASW_SHOTGUN_PELLET_H
#pragma once

#ifdef CLIENT_DLL
#define CBaseEntity C_BaseEntity
#endif

class CSprite;
class CSpriteTrail;

class CASW_Shotgun_Pellet : public CBaseCombatCharacter
{
	DECLARE_CLASS( CASW_Shotgun_Pellet, CBaseCombatCharacter );

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif
					
	virtual ~CASW_Shotgun_Pellet( void );

public:
	void	Spawn( void );
	void	OnRestore( void );
	void	Precache( void );
	bool	CreateVPhysics( void );
	void	CreateEffects( void );
	void	KillEffects();
	//void	SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity );
	//void	VPhysicsUpdate( IPhysicsObject *pPhysics );
	unsigned int PhysicsSolidMaskForEntity() const;
	void	PelletTouch( CBaseEntity *pOther );
	void	PelletHit( CBaseEntity *pOther );
	void	SetPelletDamage(float damage) { m_flDamage = damage; }

	static CASW_Shotgun_Pellet *Shotgun_Pellet_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float damage );

protected:
	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSpriteTrail>	m_pGlowTrail;
	CBaseEntity* GetLastHit();
	EHANDLE m_hLastHit;

	float	m_flDamage;
	bool	m_inSolid;	
};


#endif // _DEFINED_ASW_SHOTGUN_PELLET_H
