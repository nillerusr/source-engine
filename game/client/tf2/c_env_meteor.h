//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_ENV_METEOR_H
#define C_ENV_METEOR_H
#pragma once

#include "utlvector.h"
#include "env_meteor_shared.h"
#include "baseparticleentity.h"
#include "c_effect_shootingstar.h"

//=============================================================================
//
// Client-side Meteor Factory Class
//
class C_MeteorFactory : public IMeteorFactory
{
public:

	void CreateMeteor( int nID, int iType, const Vector &vecPosition, 
		               const Vector &vecDirection, float flSpeed, float flStartTime,
					   float flDamageRadius,
					   const Vector &vecTriggerMins, const Vector &vecTriggerMaxs );
};

//=============================================================================
//
// Meteor Spawner Class
//
class C_EnvMeteorSpawner : public C_BaseEntity
{
public:

	DECLARE_CLASS( C_EnvMeteorSpawner, C_BaseEntity );

	DECLARE_CLIENTCLASS();

	C_EnvMeteorSpawner();

	// Will more than likely be used for meteor input(s) later!
//	void ReceiveMessage( const char *msgname, int length, void *data );   

	//-------------------------------------------------------------------------
	// Networking
	//-------------------------------------------------------------------------
	void	OnDataChanged( DataUpdateType_t updateType );

	//-------------------------------------------------------------------------
	// Think
	//-------------------------------------------------------------------------
	void	ClientThink( void );

private:

	C_MeteorFactory				m_Factory;
	CEnvMeteorSpawnerShared		m_SpawnerShared;
	bool						m_fDisabled;
};


//=============================================================================
//
// Meteor Tail Class - Effect
//
class C_EnvMeteorHead
{
public:

	C_EnvMeteorHead();
	~C_EnvMeteorHead();

	void	Start( const Vector &vecOrigin, const Vector &vecDirection );
	void    Destroy( void );

	void	MeteorHeadThink( const Vector &vecOrigin, float flTime );

	void	SetSmokeEmission( bool bEmit ) { m_bEmitSmoke = bEmit; }
	bool	EmitSmoke( void ) { return m_bEmitSmoke; }

	void	SetParticleScale( float flScale ) { m_flParticleScale = flScale; }

	bool				m_bInitThink;

private:

	Vector				m_vecPos;
	Vector				m_vecPrevPos;
	Vector				m_vecDirection;

	float				m_flParticleScale;

	CSmartPtr<CSimpleEmitter>	m_pSmokeEmitter;
	float				m_flSmokeSpawnInterval;
	float				m_flSmokeSpawnRadius;
	PMaterialHandle		m_hSmokeMaterial;
	float				m_flSmokeLifetime;			// How long do the particles live?
	bool				m_bEmitSmoke;

	PMaterialHandle		m_hFlareMaterial;
};

//=============================================================================
//
// Meteor Tail Class - Effect
//
class C_EnvMeteorTail : public C_BaseParticleEntity
{
public:

	DECLARE_CLASS( C_EnvMeteorTail, C_BaseParticleEntity );

	C_EnvMeteorTail();
	~C_EnvMeteorTail();

	void	Start( const Vector &vecOrigin, const Vector &vecDirection, float flSpeed );
	void	Destroy( void );
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );

//protected:

	void	DrawFragment( ParticleDraw* pDraw, const Vector &vecStart, const Vector &vecDelta, 
						  const Vector4D &vecStartColor, const Vector4D &vecEndColor, 
						  float flStartV, float flEndV );

	CParticleMgr		*m_pParticleMgr;
	Particle			*m_pParticle;

	PMaterialHandle		m_TailMaterialHandle;

	// Properties.
	float				m_flFadeTime;
	float				m_flWidth;
	float				m_flSpeed;
	Vector				m_vecDirection;

private:
	C_EnvMeteorTail( const C_EnvMeteorTail & );
};

//=============================================================================
//
// Meteor Class (Client-side only!)
//
class C_EnvMeteor :  public C_BaseAnimating
{
public:

	DECLARE_CLASS( C_EnvMeteor, C_BaseAnimating );

	//-------------------------------------------------------------------------
	// Initialization/Destruction
	//-------------------------------------------------------------------------
	C_EnvMeteor();
	~C_EnvMeteor();
	static C_EnvMeteor	   *Create( int nID, int iMeteorType, const Vector &vecOrigin, 
		                            const Vector &vecDirection, float flSpeed, float flStartTime,
									float flDamageRadius,
								    const Vector &vecTriggerMins, const Vector &vecTriggerMaxs );
	static void				Destroy( C_EnvMeteor *pMeteor );
	
	//-------------------------------------------------------------------------
	// Think
	//-------------------------------------------------------------------------
	void					ClientThink( void );
	void					SkyboxThink( float flTime );
	void					WorldThink( float flTime );
	void					WorldToSkyboxThink( float flTime );
	void					SkyboxToWorldThink( float flTime );

	void					SetTravelDirection( const Vector &vecDir ) { m_vecTravelDir = vecDir; }

private:
	C_EnvMeteor( const C_EnvMeteor & );

	CEnvMeteorShared			m_Meteor;

	// Effects
	Vector						m_vecTravelDir;
	C_EnvMeteorHead				m_HeadEffect;
	C_EnvMeteorTail				m_TailEffect;
};

#endif // C_ENV_METEOR_H
