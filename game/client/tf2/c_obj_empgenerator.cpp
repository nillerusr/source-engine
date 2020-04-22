//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "c_baseobject.h"
#include "particles_simple.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectEMPGenerator : public C_BaseObject
{
	DECLARE_CLASS( C_ObjectEMPGenerator, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();

	C_ObjectEMPGenerator();

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual void	ClientThink( void );

private:
	CSmartPtr<CSimpleEmitter>	m_pEmitter;
	PMaterialHandle				m_hParticleMaterial;
	TimedEvent					m_ParticleEvent;

private:
	C_ObjectEMPGenerator( const C_ObjectEMPGenerator & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectEMPGenerator, DT_ObjectEMPGenerator, CObjectEMPGenerator)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectEMPGenerator::C_ObjectEMPGenerator()
{
	m_ParticleEvent.Init( 300 ); 
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectEMPGenerator::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_pEmitter = CSimpleEmitter::Create( "C_ObjectEMPGenerator" );
		m_hParticleMaterial = m_pEmitter->GetPMaterial( "sprites/chargeball" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectEMPGenerator::ClientThink( void )
{
	// Add particles at the target.
	float flCur = gpGlobals->frametime;
	while ( m_ParticleEvent.NextEvent( flCur ) )
	{
		Vector vPos = WorldSpaceCenter( );
		Vector vOffset = RandomVector( -1, 1 );
		VectorNormalize( vOffset );
		vPos += vOffset * RandomFloat( 0, 50 );
		
		SimpleParticle *pParticle = m_pEmitter->AddSimpleParticle( m_hParticleMaterial, vPos );
		if ( pParticle )
		{
			// Move the points along the path.
			pParticle->m_vecVelocity.Init();
			pParticle->m_flRoll = 0;
			pParticle->m_flRollDelta = 0;
			pParticle->m_flDieTime = 0.4f;
			pParticle->m_flLifetime = 0;
			pParticle->m_uchColor[0] = 255; 
			pParticle->m_uchColor[1] = 255;
			pParticle->m_uchColor[2] = 255;
			pParticle->m_uchStartAlpha = 32;
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = 6;
			pParticle->m_uchEndSize = 4;
			pParticle->m_iFlags = 0;
		}
	}
}





