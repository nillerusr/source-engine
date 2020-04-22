//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basegrenade_shared.h"
#include "minimap_trace.h"
#include "particles_simple.h"

//-----------------------------------------------------------------------------
// Purpose: Client side entity for the ferry target items
//-----------------------------------------------------------------------------
class C_LimpetMine : public C_BaseGrenade
{
	DECLARE_CLASS( C_LimpetMine, C_BaseGrenade );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_MINIMAP_PANEL();

	C_LimpetMine();

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

public:
	C_LimpetMine( const C_LimpetMine & );

private:
	bool	m_bLive;
};

IMPLEMENT_CLIENTCLASS_DT(C_LimpetMine, DT_LimpetMine, CLimpetMine)
	RecvPropInt(RECVINFO(m_bLive)),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_LimpetMine::C_LimpetMine( void )
{
	CONSTRUCT_MINIMAP_PANEL( "minimap_limpet", MINIMAP_OBJECTS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_LimpetMine::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// Only think when live
	if ( m_bLive )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Spawn effects if I'm live
//-----------------------------------------------------------------------------
void C_LimpetMine::ClientThink( void )
{
	if ( !InLocalTeam() )
		return;

	Vector up;
	GetVectors( NULL, NULL, &up );
	up *= 8.0f;
	Vector vecOrg = GetAbsOrigin() + up;

	// Make a single sprite
	CSmartPtr<CSimpleEmitter> pSmokeEmitter = CSimpleEmitter::Create( "C_LimpetMine::Effect" );
	pSmokeEmitter->SetSortOrigin( vecOrg );
	PMaterialHandle	hSphereMaterial = g_Mat_DustPuff[0];
	SimpleParticle *pParticle = (SimpleParticle *) pSmokeEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, vecOrg );
	if ( pParticle == NULL )
		return;

	pParticle->m_flLifetime	= 0.0f;
	pParticle->m_flDieTime	= 0.1f;
	pParticle->m_uchStartSize	= RandomInt(4,6);
	pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
	pParticle->m_vecVelocity = vec3_origin;
	pParticle->m_uchStartAlpha = 255;
	pParticle->m_uchEndAlpha = 255;
	pParticle->m_flRoll	= random->RandomFloat( 180, 360 );
	pParticle->m_flRollDelta = random->RandomFloat( -1, 1 );

	if ( InLocalTeam() )
	{
		pParticle->m_uchColor[0] = 0;
		pParticle->m_uchColor[1] = 255;
		pParticle->m_uchColor[2] = 0;
	}
	else
	{
		pParticle->m_uchColor[0] = 255;
		pParticle->m_uchColor[1] = 50;
		pParticle->m_uchColor[2] = 50;
	}
}


