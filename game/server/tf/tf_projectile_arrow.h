//========= Copyright Valve Corporation, All rights reserved. ============//
//
// TF Arrow Projectile
//
//=============================================================================
#ifndef TF_PROJECTILE_ARROW_H
#define TF_PROJECTILE_ARROW_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_player.h"
#include "tf_weaponbase_rocket.h"
#include "iscorer.h"

class CTFProjectile_Arrow : public CTFBaseRocket, public IScorer
{
public:

	DECLARE_CLASS( CTFProjectile_Arrow, CTFBaseRocket );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

	CTFProjectile_Arrow();
	~CTFProjectile_Arrow();

	// Creation.
	static CTFProjectile_Arrow *Create( const Vector &vecOrigin, const QAngle &vecAngles, const float fSpeed, const float fGravity, ProjectileType_t projectileType, CBaseEntity *pOwner = NULL, CBaseEntity *pScorer = NULL );	
	virtual void	InitArrow( const QAngle &vecAngles, const float fSpeed, const float fGravity, ProjectileType_t projectileType, CBaseEntity *pOwner = NULL, CBaseEntity *pScorer = NULL );	
	virtual void	Spawn();
	virtual void	Precache();
	virtual int		GetWeaponID( void ) const			{ return m_iWeaponId; }
	virtual int		GetProjectileType ( void ) const OVERRIDE;

	virtual bool	StrikeTarget( mstudiobbox_t *pBox, CBaseEntity *pOther );
	virtual void	OnArrowImpact( mstudiobbox_t *pBox, CBaseEntity *pOther, CBaseEntity *pAttacker );
	virtual bool	OnArrowImpactObject( CBaseEntity *pOther );
	bool			PositionArrowOnBone( mstudiobbox_t *pBox, CBaseAnimating *pOtherAnim );
	void			GetBoneAttachmentInfo( mstudiobbox_t *pBox, CBaseAnimating *pOtherAnim, Vector &bonePosition, QAngle &boneAngles, int &boneIndexAttached, int &physicsBoneIndex );

	void			ImpactThink( void );
	void			BuildingHealingArrow( CBaseEntity *pOther );

	int				GetArrowSkin() const;

	// IScorer interface
	virtual CBasePlayer *GetScorer( void );
	virtual CBasePlayer *GetAssistant( void ) { return NULL; }

	void			SetScorer( CBaseEntity *pScorer );
	void			SetCritical( bool bCritical ) { m_bCritical = bCritical; }

	virtual float	GetDamage();
	virtual bool	CanHeadshot();

	virtual void	OnArrowMissAllPlayers( void );
	virtual void	ArrowTouch( CBaseEntity *pOther );
	virtual void	CheckSkyboxImpact( CBaseEntity *pOther );
	bool			CheckRagdollPinned( const Vector &start, const Vector &vel, int boneIndexAttached, int physicsBoneIndex, CBaseEntity *pOther, int iHitGroup, int iVictim );
	virtual void	AdjustDamageDirection( const CTakeDamageInfo &info, Vector &dir, CBaseEntity *pEnt );
	void			ImpactSound( const char *pszSoundName, bool bLoudForAttacker = false );
	virtual void	BreakArrow();
	virtual void	ImpactTeamPlayer( CTFPlayer *pOther ) {}

	void			FadeOut( int iTime );
	void			RemoveThink();

	virtual const char * GetTrailParticleName( void );
	void			CreateTrail( void );
	void			RemoveTrail( void );

	virtual void	IncrementDeflected( void );
	
	virtual void	Deflected( CBaseEntity *pDeflectedBy, Vector &vecDir );
	
	virtual bool	ShouldNotDetonate( void ) { return true; }
	bool			IsAlight() { return m_bArrowAlight; }
	void			SetArrowAlight( bool bAlight ) { m_bArrowAlight = bAlight; }
	virtual bool	IsDeflectable() OVERRIDE { return true; }
	void			SetPenetrate( bool bPenetrate = false ) { m_bPenetrate = bPenetrate; SetSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER ); }
	bool			CanPenetrate() const { return m_bPenetrate; }
	virtual bool	IsDestroyable( void ) OVERRIDE { return false; }
	virtual bool	IsBreakable( void ) const { return true; }
	
	void SetApplyMilkOnHit() { m_bApplyMilkOnHit = true; }

private:

	CBaseHandle		m_Scorer;

	float			m_flImpactTime;
	Vector			m_vecImpactNormal;

	float			m_flTrailLife;
	EHANDLE			m_pTrail;

	bool			m_bStruckEnemy;

	CNetworkVar( bool, m_bArrowAlight );
	CNetworkVar( bool, m_bCritical );

	bool				m_bPenetrate;

	CNetworkVar( int, m_iProjectileType );
	int				m_iWeaponId;

	bool			m_bFiredWhileZoomed;

protected:
	CUtlVector< int >	m_HitEntities;
	float			m_flInitTime;

	bool			m_bApplyMilkOnHit;		// For Apothacary's Arrow which can sometimes be special
};

class CTFProjectile_HealingBolt : public CTFProjectile_Arrow
{
public:

	DECLARE_CLASS( CTFProjectile_HealingBolt, CTFProjectile_Arrow );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

	virtual void	InitArrow( const QAngle &vecAngles, const float fSpeed, const float fGravity, ProjectileType_t projectileType, CBaseEntity *pOwner = NULL, CBaseEntity *pScorer = NULL ) OVERRIDE;

	virtual bool CanHeadshot() { return false; }
	virtual void ImpactTeamPlayer( CTFPlayer *pOther );
	
	virtual float GetCollideWithTeammatesDelay() const { return 0.f; }
};

class CTFProjectile_GrapplingHook : public CTFProjectile_Arrow
{
public:

	DECLARE_CLASS( CTFProjectile_GrapplingHook, CTFProjectile_Arrow );
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

	CTFProjectile_GrapplingHook();

	virtual void	Spawn() OVERRIDE;
	virtual void	Precache() OVERRIDE;
	virtual void	UpdateOnRemove() OVERRIDE;
	virtual void	InitArrow( const QAngle &vecAngles, const float fSpeed, const float fGravity, ProjectileType_t projectileType, CBaseEntity *pOwner = NULL, CBaseEntity *pScorer = NULL ) OVERRIDE;
	virtual void	OnArrowImpact( mstudiobbox_t *pBox, CBaseEntity *pOther, CBaseEntity *pAttacker ) OVERRIDE;
	virtual bool	OnArrowImpactObject( CBaseEntity *pOther ) OVERRIDE;
	virtual void	OnArrowMissAllPlayers( void ) OVERRIDE {}
	virtual void	CheckSkyboxImpact( CBaseEntity *pOther ) OVERRIDE;
	
	virtual void	BreakArrow() { /*DO NOTHING*/ }

	virtual bool	IsDeflectable() OVERRIDE { return false; }
	virtual bool	IsBreakable( void ) const OVERRIDE { return false; }

	virtual float GetDamage() OVERRIDE { return 1.f; }
	virtual bool CanHeadshot() OVERRIDE { return false; }
	virtual bool CanCollideWithTeammates() const OVERRIDE { return false; }

	void HookTarget( CBaseEntity *pOther );
	void HookLatchedThink();

private:
	void StartImpactFleshSoundLoop();
	void StopImpactFleshSoundLoop();

	CSoundPatch *m_pImpactFleshSoundLoop;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTraceFilterCollisionArrows : public CTraceFilterEntitiesOnly
{
public:
	DECLARE_CLASS_NOBASE( CTraceFilterCollisionArrows );

	CTraceFilterCollisionArrows( const IHandleEntity *passentity, const IHandleEntity *passentity2 )
		: m_pPassEnt(passentity), m_pPassEnt2(passentity2)
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
			return false;
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( pEntity )
		{
			if ( pEntity == m_pPassEnt2 )
				return false;
			if ( pEntity->GetCollisionGroup() == TF_COLLISIONGROUP_GRENADES )
				return false;
			if ( pEntity->GetCollisionGroup() == TFCOLLISION_GROUP_ROCKETS )
				return false;
			if ( pEntity->GetCollisionGroup() == TFCOLLISION_GROUP_ROCKET_BUT_NOT_WITH_OTHER_ROCKETS )
				return false;
			if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
				return false;
			if ( pEntity->GetCollisionGroup() == TFCOLLISION_GROUP_RESPAWNROOMS )
				return false;
			if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_NONE )
				return false;

			return true;
		}

		return true;
	}

protected:
	const IHandleEntity *m_pPassEnt;
	const IHandleEntity *m_pPassEnt2;
};

#endif	//TF_PROJECTILE_ARROW_H