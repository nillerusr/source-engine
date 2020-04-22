//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "particles_simple.h"

//-----------------------------------------------------------------------------
// Purpose: Client side entity for the antipersonnel grenades
//-----------------------------------------------------------------------------
class C_GrenadeRocket : public C_BaseAnimating
{
	DECLARE_CLASS( C_GrenadeRocket, C_BaseAnimating );
public:
	DECLARE_CLIENTCLASS();

	C_GrenadeRocket();

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

public:
	C_GrenadeRocket( const C_GrenadeRocket & );
};

IMPLEMENT_CLIENTCLASS_DT(C_GrenadeRocket, DT_GrenadeRocket, CGrenadeRocket)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_GrenadeRocket::C_GrenadeRocket( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_GrenadeRocket::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Only think when sapping
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Spawn effects if I'm sapping
//-----------------------------------------------------------------------------
void C_GrenadeRocket::ClientThink( void )
{
	// Fire smoke puffs out the side
	CSmartPtr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "C_GrenadeRocket::Effect" );
	pSmokeEmitter->SetSortOrigin( GetAbsOrigin() );
	int iSmokeClouds = random->RandomInt(1,2);
	for ( int i = 0; i < iSmokeClouds; i++ )
	{
		SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), g_Mat_DustPuff[1], GetAbsOrigin() );
		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 0.1f, 0.3f );

		pParticle->m_uchStartSize	= 10;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize + 2;

		pParticle->m_vecVelocity = GetAbsVelocity();
		pParticle->m_uchStartAlpha = 255;
		pParticle->m_uchEndAlpha = 64;
		pParticle->m_flRoll	= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta = random->RandomFloat( -1, 1 );

		pParticle->m_uchColor[0] = 50;
		pParticle->m_uchColor[1] = 250;
		pParticle->m_uchColor[2] = 50;
	}
}


