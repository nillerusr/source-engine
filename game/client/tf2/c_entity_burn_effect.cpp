//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_entity_burn_effect.h"


#define NUM_BURN_PARTICLES_PER_SEC	50



IMPLEMENT_CLIENTCLASS_DT( C_EntityBurnEffect, DT_EntityBurnEffect, CEntityBurnEffect )
	RecvPropInt( RECVINFO( m_hBurningEntity ) )
END_RECV_TABLE()



C_EntityBurnEffect::C_EntityBurnEffect()
{
	m_pEmitter = CSimpleEmitter::Create( "Entity burn effect" );
	if ( m_pEmitter.IsValid() )
	{
		m_hFireMaterial = m_pEmitter->GetPMaterial( "particle/fire" );
	}
	else
	{
		m_hFireMaterial = INVALID_MATERIAL_HANDLE;
	}
	m_Timer.Init( NUM_BURN_PARTICLES_PER_SEC );
}


void C_EntityBurnEffect::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}


void C_EntityBurnEffect::ClientThink()
{
	if ( !m_pEmitter.IsValid() || IsDormant() )
		return;

	// Add some burning particles to our target entity.
	C_BaseEntity *pEnt = ClientEntityList().GetBaseEntity( m_hBurningEntity );
	if ( !pEnt )
		return;

	float dt = gpGlobals->frametime;
	while ( m_Timer.NextEvent( dt ) )
	{
		Vector vDims = (pEnt->WorldAlignMaxs() - pEnt->WorldAlignMins()) * 0.5f;
		Vector vCenter = pEnt->GetAbsOrigin() + pEnt->WorldAlignMins() + vDims;
		Vector vPos = vCenter + vDims * RandomVector( -0.7, 0.7 );

		float flLifetime = 1;
		float flRadius = 3;
		unsigned char uchColor[4] = { 255, 100, 0, 100 };

		SimpleParticle *pParticle = m_pEmitter->AddSimpleParticle( m_hFireMaterial, vPos, flLifetime, flRadius );
		if ( pParticle )
		{
			pParticle->m_uchColor[0] = uchColor[0];
			pParticle->m_uchColor[1] = uchColor[1];
			pParticle->m_uchColor[2] = uchColor[2];

			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartAlpha = uchColor[3];

			pParticle->m_vecVelocity.x = RandomFloat( -2, 2 );
			pParticle->m_vecVelocity.y = RandomFloat( -2, 2 );
			pParticle->m_vecVelocity.z = RandomFloat( 3, 29 );

			// Pick up some velocity from the burning guy running around.
			pParticle->m_vecVelocity += pEnt->GetAbsVelocity() * 0.6;
		}
	}
}

