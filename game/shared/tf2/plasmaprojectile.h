//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PLASMAPROJECTILE_H
#define PLASMAPROJECTILE_H
#ifdef _WIN32
#pragma once
#endif

#include "predictable_entity.h"

#include "baseparticleentity.h"
#include "plasmaprojectile_shared.h"

#if !defined( CLIENT_DLL )
#include "iscorer.h"
#else
#include "particle_prototype.h"
#include "particles_simple.h"
#include "particle_util.h"
#include "c_baseplayer.h"
#include "fx_sparks.h"
#endif

#if defined( CLIENT_DLL )
#define CBasePlasmaProjectile C_BasePlasmaProjectile
#endif

#define MAX_HISTORY			5
#define GUIDED_FADE_TIME	0.25f
#define	GUIDED_WIDTH		3

struct PositionHistory_t
{
	DECLARE_PREDICTABLE();

	Vector	m_Position;
	float	m_Time;
};

// ------------------------------------------------------------------------ //
// CBasePlasmaProjectile
// ------------------------------------------------------------------------ //
class CBasePlasmaProjectile : public CBaseParticleEntity
#if !defined( CLIENT_DLL )
, public IScorer
#endif
{
	DECLARE_CLASS( CBasePlasmaProjectile, CBaseParticleEntity );
public:
	CBasePlasmaProjectile();
	~CBasePlasmaProjectile();

	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS();

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	virtual bool ProjectileHitShield( CBaseEntity *pOther, trace_t& tr );
	virtual void HandleShieldImpact( CBaseEntity *pOther, trace_t& tr );

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Activate( void );

	virtual void MissileTouch( CBaseEntity *pOther );
	virtual float GetDamage( void );
	virtual void SetDamage( float flDamage );
	virtual void SetMaxRange( float flRange );
	virtual void SetExplosive( float flRadius );
	virtual void PerformCustomPhysics( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity );

	// Purpose: Returns the type of damage that this entity inflicts.
	int GetDamageType() const
	{
		return m_DamageType;
	}

	virtual float	GetSize( void ) { return 6.0; };

	// FIXME!!!! Override the think of the baseparticle Think functions
	virtual void Think( void ) { CBaseEntity::Think(); }

	void		SetupProjectile( const Vector &vecOrigin, const Vector &vecForward, int damageType, CBaseEntity *pOwner = NULL );
	static CBasePlasmaProjectile *Create( const Vector &vecOrigin, const Vector &vecForward, int damageType, CBaseEntity *pOwner );
	static CBasePlasmaProjectile *CreatePredicted( const Vector &vecOrigin, const Vector &vecForward, const Vector& gunOffset, int damageType, CBasePlayer *pOwner );

	void		RecalculatePositions( Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity );

// IScorer
public:
	// Return the entity that should receive the score
	virtual CBasePlayer *GetScorer( void );
	// Return the entity that should get assistance credit
	virtual CBasePlayer *GetAssistant( void ) { return NULL; };
	
protected:
	void		Detonate( void );

	// A derived class should return true here so that weapon sounds, etc, can
	//  apply the proper filter
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwnerEntity() &&
			GetOwnerEntity() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}

	virtual	void	OnDataChanged(DataUpdateType_t updateType);
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);
	virtual bool	SimulateAndRender(Particle *pParticle, ParticleDraw *pDraw, float &sortKey);

	// Add the position to the history 
	void		AddPositionToHistory( const Vector& org, float flSimTime );
	void		ResetPositionHistories( const Vector& org );
	// Adjustments for shots straight out of local player's eyes
	void		RemapPosition( Vector &vecStart, float curtime, Vector& outpos );

	// Scale
	virtual float UpdateScale( SimpleParticle *pParticle, float timeDelta )
	{
		return (float)pParticle->m_uchStartSize + RandomInt( -2,2 );
	}

	// Alpha
	virtual float UpdateAlpha( SimpleParticle *pParticle, float timeDelta )
	{
		return (pParticle->m_uchStartAlpha + RandomInt( -50, 0 ) ) / 255.0f;
	}
	virtual	float	UpdateRoll( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;

		return pParticle->m_flRoll;
	}
	virtual Vector	UpdateColor( SimpleParticle *pParticle, float timeDelta )
	{
		static Vector	cColor;

		cColor[0] = pParticle->m_uchColor[0] / 255.0f;
		cColor[1] = pParticle->m_uchColor[1] / 255.0f;
		cColor[2] = pParticle->m_uchColor[2] / 255.0f;

		return cColor;
	}

	// Should this object cast shadows?
	virtual ShadowType_t	ShadowCastType() { return SHADOWS_NONE; }
	
	virtual void	ClientThink( void );
	virtual bool	OnPredictedEntityRemove( bool isbeingremoved, C_BaseEntity *predicted );

protected:
	SimpleParticle	*m_pHeadParticle;
	TrailParticle	*m_pTrailParticle;
	CParticleMgr	*m_pParticleMgr;
	float			m_flNextSparkEffect;
#endif
public:
	EHANDLE		m_hOwner;

protected:
	CNetworkVarEmbedded( CPlasmaProjectileShared, m_Shared );

	Vector		m_vecGunOriginOffset;

	CNetworkVar( float, m_flPower );

	// Explosive radius
	float		m_flExplosiveRadius;

	// Maximum range
	float		m_flMaxRange;

	float		m_flDamage;
	int			m_DamageType;

	Vector		m_vecTargetOffset;

	PositionHistory_t	m_pPreviousPositions[MAX_HISTORY];

private:
	CBasePlasmaProjectile( const CBasePlasmaProjectile & );
};

#if defined( CLIENT_DLL )
#define CPowerPlasmaProjectile C_PowerPlasmaProjectile
#endif

// ------------------------------------------------------------------------ //
// Plasma projectile that has a concept of variable power
// ------------------------------------------------------------------------ //
class CPowerPlasmaProjectile : public CBasePlasmaProjectile
{
	DECLARE_CLASS( CPowerPlasmaProjectile, CBasePlasmaProjectile );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CPowerPlasmaProjectile();

	void	SetPower( float flPower ) { m_flPower = flPower; };
	static CPowerPlasmaProjectile* Create( const Vector &vecOrigin, const Vector &vecForward, int damageType, CBaseEntity *pOwner );
	static CPowerPlasmaProjectile* CreatePredicted( const Vector &vecOrigin, const Vector &vecForward, const Vector& gunOffset, int damageType, CBasePlayer *pOwner );

	virtual float	GetSize( void );

	// A derived class should return true here so that weapon sounds, etc, can
	//  apply the proper filter
	virtual bool			IsPredicted( void ) const
	{ 
		return true;
	}

#if defined( CLIENT_DLL )
	virtual bool	ShouldPredict( void )
	{
		if ( GetOwnerEntity() &&
			GetOwnerEntity() == C_BasePlayer::GetLocalPlayer() )
			return true;

		return BaseClass::ShouldPredict();
	}
#endif

private:
	CPowerPlasmaProjectile( const CPowerPlasmaProjectile & );

};

#endif // PLASMAPROJECTILE_H
