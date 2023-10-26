#ifndef _DEFINED_ASW_FLARE_PROJECTILE_H
#define _DEFINED_ASW_FLARE_PROJECTILE_H
#pragma once

class CSprite;
class CSpriteTrail;

class CASW_Flare_Projectile : public CBaseCombatCharacter
{
	DECLARE_CLASS( CASW_Flare_Projectile, CBaseCombatCharacter );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Flare_Projectile();
	virtual ~CASW_Flare_Projectile();

public:
	void	Spawn( void );
	void	Precache( void );

	//void	SetVelocity( const Vector &velocity, const AngularImpulse &angVelocity );
	//void	VPhysicsUpdate( IPhysicsObject *pPhysics );
	unsigned int PhysicsSolidMaskForEntity() const;
	void	FlareTouch( CBaseEntity *pOther );
	void	FlareBurnTouch( CBaseEntity *pOther );
	void LayFlat();

	const Vector& GetEffectOrigin();

	static CASW_Flare_Projectile* Flare_Projectile_Create( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner);

	float GetDuration() { return m_flTimeBurnOut; }
	void SetDuration( float fDuration ) { m_flTimeBurnOut = fDuration; }

protected:
	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSpriteTrail>	m_pGlowTrail;

	float	m_flDamage;
	bool	m_inSolid;	

public:
	int		Restore( IRestore &restore );

	void	Start( float lifeTime );
	void	Die( float fadeTime );
	void	Launch( const Vector &direction, float speed );

	Class_T Classify( void );

	void	FlareThink( void );

	int			m_nBounces;			// how many times has this flare bounced?
	CNetworkVar( float, m_flTimeBurnOut );	// when will the flare burn out?
	CNetworkVar( float, m_flScale );
	float		m_flDuration;
	float		m_flNextDamage;
	
	bool		m_bFading;
	CNetworkVar( bool, m_bLight );
	CNetworkVar( bool, m_bSmoke );

	virtual void DrawDebugGeometryOverlays();	// visualise the autoaim radius
		
	//
	CASW_Flare_Projectile* m_pNextFlare;		// next flare in the linked list of live flares
};

extern CASW_Flare_Projectile* g_pHeadFlare;	// access to a linked list of live flares

#endif // _DEFINED_ASW_FLARE_PROJECTILE_H
