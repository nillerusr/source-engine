//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_EFFECTS_H
#define C_EFFECTS_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "precipitation_shared.h"

// Draw rain effects.
void DrawPrecipitation();

//-----------------------------------------------------------------------------
// Precipitation particle type
//-----------------------------------------------------------------------------

class CPrecipitationParticle
{
public:
	Vector	m_Pos;
	Vector	m_Velocity;
	float	m_SpawnTime;				// Note: Tweak with this to change lifetime
	float	m_Mass;
	float	m_Ramp;

	float	m_flMaxLifetime;
	int		m_nSplitScreenPlayerSlot;
};

class CClient_Precipitation;
static CUtlVector<CClient_Precipitation*> g_Precipitations;

//===========
// Snow fall
//===========
class CSnowFallManager;
static CSnowFallManager *s_pSnowFallMgr[ MAX_SPLITSCREEN_PLAYERS ];
bool SnowFallManagerCreate( CClient_Precipitation *pSnowEntity );
void SnowFallManagerDestroy( void );

class AshDebrisEffect : public CSimpleEmitter
{
public:
	AshDebrisEffect( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}

	static AshDebrisEffect* Create( const char *pDebugName );

	virtual float UpdateAlpha( const SimpleParticle *pParticle );
	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta );

private:
	AshDebrisEffect( const AshDebrisEffect & );
};

//-----------------------------------------------------------------------------
// Precipitation blocker entity
//-----------------------------------------------------------------------------
class C_PrecipitationBlocker : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_PrecipitationBlocker, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_PrecipitationBlocker();
	virtual ~C_PrecipitationBlocker();
};

//-----------------------------------------------------------------------------
// Precipitation base entity
//-----------------------------------------------------------------------------
class CClient_Precipitation : public C_BaseEntity
{
	class CPrecipitationEffect;
	friend class CClient_Precipitation::CPrecipitationEffect;

public:
	DECLARE_CLASS( CClient_Precipitation, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	CClient_Precipitation();
	virtual ~CClient_Precipitation();

	// Inherited from C_BaseEntity
	virtual void Precache( );
	virtual RenderableTranslucencyType_t ComputeTranslucencyType() { return RENDERABLE_IS_TRANSLUCENT; }

	void Render();

	// Computes where we're gonna emit
	bool ComputeEmissionArea( Vector& origin, Vector2D& size, C_BaseCombatCharacter *pCharacter );


private:

	// Creates a single particle
	CPrecipitationParticle* CreateParticle();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void ClientThink();

	void Simulate( float dt );

	// Renders the particle
	void RenderParticle( CPrecipitationParticle* pParticle, CMeshBuilder &mb );

	void CreateWaterSplashes();

	// Emits the actual particles
	void EmitParticles( float fTimeDelta );

	// Gets the tracer width and speed
	float GetWidth() const;
	float GetLength() const;
	float GetSpeed() const;

	// Gets the remaining lifetime of the particle
	float GetRemainingLifetime( CPrecipitationParticle* pParticle ) const;

	// Computes the wind vector
	static void ComputeWindVector( );

	// simulation methods
	bool SimulateRain( CPrecipitationParticle* pParticle, float dt );
	bool SimulateSnow( CPrecipitationParticle* pParticle, float dt );

	void CreateParticlePrecip( void );
	void InitializeParticlePrecip( void );
	void DispatchOuterParticlePrecip( int nSlot, C_BasePlayer *pPlayer, Vector vForward );
	void DispatchInnerParticlePrecip( int nSlot, C_BasePlayer *pPlayer, Vector vForward );
	void DestroyOuterParticlePrecip( int nSlot );
	void DestroyInnerParticlePrecip( int nSlot );

	void UpdateParticlePrecip( C_BasePlayer *pPlayer, int nSlot );
	float GetDensity() { return m_flDensity; }

private:
	void CreateAshParticle( void );
	void CreateRainOrSnowParticle( const Vector &vSpawnPosition, const Vector &vEndPosition, const Vector &vVelocity );	// TERROR: adding end pos for lifetime calcs

	// Information helpful in creating and rendering particles
	IMaterial		*m_MatHandle;	// material used 

	float			m_Color[4];		// precip color
	float			m_Lifetime;		// Precip lifetime
	float			m_InitialRamp;	// Initial ramp value
	float			m_Speed;		// Precip speed
	float			m_Width;		// Tracer width
	float			m_Remainder;	// particles we should render next time
	PrecipitationType_t	m_nPrecipType;			// Precip type
	float			m_flHalfScreenWidth;	// Precalculated each frame.

	float			m_flDensity;

#ifdef INFESTED_DLL
	int m_nSnowDustAmount;
#endif

	// Some state used in rendering and simulation
	// Used to modify the rain density and wind from the console
	static ConVar s_raindensity;
	static ConVar s_rainwidth;
	static ConVar s_rainlength;
	static ConVar s_rainspeed;

	static Vector s_WindVector;			// Stores the wind speed vector

	CUtlLinkedList<CPrecipitationParticle> m_Particles;
	CUtlVector<Vector> m_Splashes;

	struct AshSplit_t
	{
		AshSplit_t() : m_bActiveAshEmitter( false ), m_vAshSpawnOrigin( 0, 0, 0 ), m_iAshCount( 0 )
		{
		}

		CSmartPtr<AshDebrisEffect>		m_pAshEmitter;
		TimedEvent						m_tAshParticleTimer;
		TimedEvent						m_tAshParticleTraceTimer;
		bool							m_bActiveAshEmitter;
		Vector							m_vAshSpawnOrigin;

		int								m_iAshCount;
	};

protected:
	AshSplit_t						m_Ash[ MAX_SPLITSCREEN_PLAYERS ];

	float							m_flParticleInnerDist;	//The distance at which to start drawing the inner system
	char							*m_pParticleInnerNearDef; //Name of the first inner system
	char							*m_pParticleInnerFarDef;  //Name of the second inner system
	char							*m_pParticleOuterDef;     //Name of the outer system
	HPARTICLEFFECT					m_pParticlePrecipInnerNear[ MAX_SPLITSCREEN_PLAYERS ];
	HPARTICLEFFECT					m_pParticlePrecipInnerFar[ MAX_SPLITSCREEN_PLAYERS ];
	HPARTICLEFFECT					m_pParticlePrecipOuter[ MAX_SPLITSCREEN_PLAYERS ];
	TimedEvent						m_tParticlePrecipTraceTimer[ MAX_SPLITSCREEN_PLAYERS ];
	bool							m_bActiveParticlePrecipEmitter[ MAX_SPLITSCREEN_PLAYERS ];
	bool							m_bParticlePrecipInitialized;

private:
	CClient_Precipitation( const CClient_Precipitation & ); // not defined, not accessible
};


#endif // C_EFFECTS_H
