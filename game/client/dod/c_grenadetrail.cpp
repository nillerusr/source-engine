//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "c_grenadetrail.h"
#include "fx.h"
//#include "engine/ivdebugoverlay.h"
//#include "engine/IEngineSound.h"
//#include "c_te_effect_dispatch.h"
//#include "glow_overlay.h"
//#include "fx_explosion.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CSmokeParticle : public CSimpleEmitter
{
public:
	
	CSmokeParticle( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	//Create
	static CSmokeParticle *Create( const char *pDebugName )
	{
		return new CSmokeParticle( pDebugName );
	}

	//Alpha
	virtual float UpdateAlpha( const SimpleParticle *pParticle )
	{
		return ( ((float)pParticle->m_uchStartAlpha/255.0f) * sin( M_PI * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
	}

	//Color
	virtual Vector UpdateColor( const SimpleParticle *pParticle )
	{
		Vector	color;

		float	tLifetime = pParticle->m_flLifetime / pParticle->m_flDieTime;
		float	ramp = 1.0f - tLifetime;

		Vector endcolor(75, 75, 75);

		color[0] = ( (float) pParticle->m_uchColor[0] * ramp ) / 255.0f + ( 1-ramp) * endcolor[0];
		color[1] = ( (float) pParticle->m_uchColor[1] * ramp ) / 255.0f + ( 1-ramp) * endcolor[1];
		color[2] = ( (float) pParticle->m_uchColor[2] * ramp ) / 255.0f + ( 1-ramp) * endcolor[2];

		return color;
	}

	//Roll
	virtual	float UpdateRoll( SimpleParticle *pParticle, float timeDelta )
	{
		pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
		
		pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -8.0f );

		//Cap the minimum roll
		if ( fabs( pParticle->m_flRollDelta ) < 0.5f )
		{
			pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.5f : -0.5f;
		}

		return pParticle->m_flRoll;
	}

private:
	CSmokeParticle( const CSmokeParticle & );
};

// Datatable.. this can have all the smoketrail parameters when we need it to.
IMPLEMENT_CLIENTCLASS_DT(C_GrenadeTrail, DT_GrenadeTrail, CGrenadeTrail)
	RecvPropFloat(RECVINFO(m_SpawnRate)),
	RecvPropFloat(RECVINFO(m_ParticleLifetime)),
	RecvPropFloat(RECVINFO(m_StopEmitTime)),
	RecvPropInt(RECVINFO(m_bEmit)),
	RecvPropInt(RECVINFO(m_nAttachment)),
END_RECV_TABLE()

// ------------------------------------------------------------------------- //
// ParticleMovieExplosion
// ------------------------------------------------------------------------- //
C_GrenadeTrail::C_GrenadeTrail()
{
	m_MaterialHandle[0] = NULL;
	m_MaterialHandle[1] = NULL;

	// things that we will change
	m_SpawnRate = 10;
	m_ParticleLifetime = 5;
	m_bEmit = true;
	m_nAttachment	= -1;
	m_StopEmitTime = 0;	// No end time

	// invariants
	m_ParticleSpawn.Init(10);
	m_StartColor.Init(0.65, 0.65, 0.65);
	m_MinSpeed = 2;
	m_MaxSpeed = 6;
	m_MinDirectedSpeed = m_MaxDirectedSpeed = 0;
	m_StartSize = 2;
	m_EndSize = 6;
	m_SpawnRadius = 2;
	m_VelocityOffset.Init();
	m_Opacity = 0.3f;

	m_pSmokeEmitter = NULL;
	m_pParticleMgr	= NULL;
}

C_GrenadeTrail::~C_GrenadeTrail()
{
	if ( m_pParticleMgr )
	{
		m_pParticleMgr->RemoveEffect( &m_ParticleEffect );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_GrenadeTrail::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	C_BaseEntity *pEnt = pAttachedTo->GetBaseEntity();
	if (pEnt && (m_nAttachment > 0))
	{
		pEnt->GetAttachment( m_nAttachment, *pAbsOrigin, *pAbsAngles );
	}
	else
	{
		BaseClass::GetAimEntOrigin( pAttachedTo, pAbsOrigin, pAbsAngles );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bEmit - 
//-----------------------------------------------------------------------------
void C_GrenadeTrail::SetEmit(bool bEmit)
{
	m_bEmit = bEmit;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rate - 
//-----------------------------------------------------------------------------
void C_GrenadeTrail::SetSpawnRate(float rate)
{
	m_SpawnRate = rate;
	m_ParticleSpawn.Init(rate);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_GrenadeTrail::OnDataChanged(DataUpdateType_t updateType)
{
	C_BaseEntity::OnDataChanged(updateType);

	if ( updateType == DATA_UPDATE_CREATED )
	{
		Start( ParticleMgr(), NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticleMgr - 
//			*pArgs - 
//-----------------------------------------------------------------------------
void C_GrenadeTrail::Start( CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs )
{
	if(!pParticleMgr->AddEffect( &m_ParticleEffect, this ))
		return;

	m_pParticleMgr	= pParticleMgr;
	m_pSmokeEmitter = CSmokeParticle::Create("smokeTrail");
	
	if ( !m_pSmokeEmitter )
	{
		Assert( false );
		return;
	}

	m_pSmokeEmitter->SetSortOrigin( GetAbsOrigin() );
	m_pSmokeEmitter->SetNearClip( 64.0f, 128.0f );

	m_MaterialHandle[0] = g_Mat_DustPuff[0];
	m_MaterialHandle[1] = g_Mat_DustPuff[1];
	
	m_ParticleSpawn.Init( m_SpawnRate );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : fTimeDelta - 
//-----------------------------------------------------------------------------
void C_GrenadeTrail::Update( float fTimeDelta )
{
	if ( !m_pSmokeEmitter )
		return;

	// Grenades thrown out of the PVS should not draw particles at the world origin
	if ( IsDormant() )
		return;

	Vector	offsetColor;

	// Add new particles
	if ( !m_bEmit )
		return;

	if ( ( m_StopEmitTime != 0 ) && ( m_StopEmitTime <= gpGlobals->curtime ) )
		return;

	float tempDelta = fTimeDelta;

	SimpleParticle	*pParticle;
	Vector			offset;

	Vector vecOrigin;
	VectorMA( GetAbsOrigin(), -fTimeDelta, GetAbsVelocity(), vecOrigin );

	Vector vecForward;
	GetVectors( &vecForward, NULL, NULL );

	while( m_ParticleSpawn.NextEvent( tempDelta ) )
	{
		float fldt = fTimeDelta - tempDelta;

		offset.Random( -m_SpawnRadius, m_SpawnRadius );
		offset += vecOrigin;
		VectorMA( offset, fldt, GetAbsVelocity(), offset );

		pParticle = (SimpleParticle *) m_pSmokeEmitter->AddParticle( sizeof( SimpleParticle ), m_MaterialHandle[random->RandomInt(0,1)], offset );

		if ( pParticle == NULL )
			continue;

		pParticle->m_flLifetime		= 0.0f;
		pParticle->m_flDieTime		= m_ParticleLifetime;

		pParticle->m_iFlags = 0;	// no wind!

		pParticle->m_vecVelocity.Random( -1.0f, 1.0f );
		pParticle->m_vecVelocity *= random->RandomFloat( m_MinSpeed, m_MaxSpeed );
		
		float flDirectedVel = random->RandomFloat( m_MinDirectedSpeed, m_MaxDirectedSpeed );
		VectorMA( pParticle->m_vecVelocity, flDirectedVel, vecForward, pParticle->m_vecVelocity );

		pParticle->m_vecVelocity[2] += 15;

		offsetColor = m_StartColor;
		float flMaxVal = MAX( m_StartColor[0], m_StartColor[1] );
		if ( flMaxVal < m_StartColor[2] )
		{
			flMaxVal = m_StartColor[2];
		}
		offsetColor /= flMaxVal;

		offsetColor *= random->RandomFloat( -0.2f, 0.2f );
		offsetColor += m_StartColor;

		offsetColor[0] = clamp( offsetColor[0], 0.0f, 1.0f );
		offsetColor[1] = clamp( offsetColor[1], 0.0f, 1.0f );
		offsetColor[2] = clamp( offsetColor[2], 0.0f, 1.0f );

		pParticle->m_uchColor[0]	= offsetColor[0]*255.0f;
		pParticle->m_uchColor[1]	= offsetColor[1]*255.0f;
		pParticle->m_uchColor[2]	= offsetColor[2]*255.0f;
		
		pParticle->m_uchStartSize	= m_StartSize;
		pParticle->m_uchEndSize		= m_EndSize;
		
		float alpha = random->RandomFloat( m_Opacity*0.75f, m_Opacity*1.25f );
		alpha = clamp( alpha, 0.0f, 1.0f );

		pParticle->m_uchStartAlpha	= alpha * 255; 
		pParticle->m_uchEndAlpha	= 0;
			
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -1.0f, 1.0f );
    }
}

void C_GrenadeTrail::RenderParticles( CParticleRenderIterator *pIterator )
{
}


void C_GrenadeTrail::SimulateParticles( CParticleSimulateIterator *pIterator )
{
}
