#ifndef _ASW_AOEGRENADE_PROJECTILE_H
#define _ASW_AOEGRENADE_PROJECTILE_H
#pragma once

class CSprite;
class CSpriteTrail;

class CASW_AOEGrenade_Projectile : public CBaseCombatCharacter
{
	DECLARE_CLASS( CASW_AOEGrenade_Projectile, CBaseCombatCharacter );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_AOEGrenade_Projectile();
	virtual ~CASW_AOEGrenade_Projectile();

public:
	void	Spawn( void );
	void	Precache( void );

	unsigned int PhysicsSolidMaskForEntity() const;
	void AOEGrenadeTouch( CBaseEntity *pOther );
	void LayFlat();

	const Vector& GetEffectOrigin();

	float GetDuration() { return m_flTimeBurnOut; }
	void SetDuration( float fDuration ) { m_flTimeBurnOut = fDuration; }

	virtual float GetDoAOEDelayTime( void ) { return 1.0f; }

protected:
	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSpriteTrail>	m_pGlowTrail;

	bool	m_inSolid;

public:
	int		Restore( IRestore &restore );

	virtual float GetGrenadeGravity( void ) { return 1000.0f; }
	virtual float GetBurnDuration( void ) { return m_flDuration; }
	virtual float GetEffectRadius( void ) { return m_flRadius.Get(); }

	void	AOEGrenadeThink( void );


	void	GiveEffectToEntitesInRadius( void );

	virtual void OnBurnout( void ) {}

	virtual bool ShouldTouchEntity( CBaseEntity *pEntity ) { return true; }
	virtual void StartTouch( CBaseEntity *pEntity );
	virtual void EndTouch( CBaseEntity *pEntity );

	virtual void StartAOE( CBaseEntity *pEntity );
	virtual void DoAOE( CBaseEntity *pEntity ) {}
	virtual bool StopAOE( CBaseEntity *pEntity );
	void AddAOETarget( CBaseEntity *pEntity );
	void RemoveAOETarget( CBaseEntity *pEntity );
	bool IsAOETarget( CBaseEntity *pEntity );

	int			m_nBounces;			// how many times has this aoegrenade bounced?
	CNetworkVar( float, m_flTimeBurnOut );	// when will the aoegrenade burn out?
	CNetworkVar( float, m_flScale );
	CNetworkVar( bool, m_bSettled );
	float		m_flDuration;
	float		m_flNextDamage;
	float		m_flTimePulse; // How often does the buff pulse?
	float		m_flLastDoAOE;
	CNetworkVar( float, m_flRadius );
	
	bool		m_bFading;

	CHandle< EHANDLE >	m_hFiringWeapon;

	CUtlVector< EHANDLE >	m_hAOETargets;

	virtual void DrawDebugGeometryOverlays();	// visualise the autoaim radius

private:
	EHANDLE m_hTouchTrigger;
	// Entities currently being touched by this trigger
	CUtlVector< EHANDLE >	m_hTouchingEntities;
};

#endif // _ASW_AOEGRENADE_PROJECTILE_H
