//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "c_ai_basenpc.h"
#include "IEffects.h"
#include "particles_simple.h"

//-----------------------------------------------------------------------------
// Purpose: Client side entity for the NPC Bug hole
//-----------------------------------------------------------------------------
class C_Maker_Bughole : public C_BaseEntity
{
	DECLARE_CLASS( C_Maker_Bughole, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_Maker_Bughole();

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );
	void			SpawnEffect( void );

public:
	C_Maker_Bughole( const C_Maker_Bughole & );

	float		m_flNextEffectTime;
};

IMPLEMENT_CLIENTCLASS_DT(C_Maker_Bughole, DT_Maker_Bughole, CMaker_Bughole)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_Maker_Bughole::C_Maker_Bughole( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_Maker_Bughole::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		m_flNextEffectTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spawn effects if I'm sapping
//-----------------------------------------------------------------------------
void C_Maker_Bughole::ClientThink( void )
{
	if ( m_flNextEffectTime < gpGlobals->curtime )
	{
		SpawnEffect();
		m_flNextEffectTime = gpGlobals->curtime + random->RandomFloat( 0.5, 1.0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Particle effects created when we spawn a chunk
//-----------------------------------------------------------------------------
void C_Maker_Bughole::SpawnEffect( void )
{
	Vector normal = Vector(0,0,-1);
	Vector offset = GetAbsOrigin() + (normal * 16);
	Vector dir;

	// Create a couple of big, floating smoke clouds
	CSmartPtr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "C_Maker_Bughole::SpawnEffect" );
	pSmokeEmitter->SetSortOrigin( offset );
	int iSmokeClouds = random->RandomInt(2,5);
	for ( int i = 0; i < iSmokeClouds; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[1], offset );
		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 2.0f, 3.0f );

		pParticle->m_uchStartSize	= 8;
		pParticle->m_uchEndSize		= 48;

		dir[0] = normal[0] + random->RandomFloat( -0.4f, 0.4f );
		dir[1] = normal[1] + random->RandomFloat( -0.4f, 0.4f );
		dir[2] = normal[2] + random->RandomFloat( 0, 0.6f );
		pParticle->m_vecVelocity = dir * random->RandomFloat( 8.0f, 20.0f )*(i+1);
		pParticle->m_uchStartAlpha	= 255;
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -1, 1 );

		float flColor = random->RandomFloat( 0,64 );
		pParticle->m_uchColor[0] = 100 + flColor;
		pParticle->m_uchColor[1] = 50 + flColor;
		pParticle->m_uchColor[2] = flColor;
	}
}
