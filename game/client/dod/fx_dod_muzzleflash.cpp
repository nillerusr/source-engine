//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "particles_simple.h"
#include "particles_localspace.h"
#include "c_te_effect_dispatch.h"
#include "clienteffectprecachesystem.h"

#include "fx.h"
#include "r_efx.h"
#include "dlight.h"
#include "dod_shareddefs.h"

// Precache our effects
CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffect_DOD_MuzzleFlash )
CLIENTEFFECT_MATERIAL( "sprites/effects/muzzleflash1" )
CLIENTEFFECT_MATERIAL( "sprites/effects/muzzleflash2" )
CLIENTEFFECT_REGISTER_END()

ConVar cl_muzzleflash_dlight_1st( "cl_muzzleflash_dlight_1st", "1" );

void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay, int nLightIndex = LIGHT_INDEX_TE_DYNAMIC );

void DOD_MuzzleFlashCallback( const CEffectData &data )
{
	CSmartPtr<CLocalSpaceEmitter> pEmitter = 
		CLocalSpaceEmitter::Create( "DOD_MuzzleFlash", data.m_hEntity, data.m_nAttachmentIndex, 0 );

	if ( !pEmitter )
		return;

	// SetBBox() manually on the particle system so it doesn't have to be recalculated more than once.
	Vector vCenter( 0.0f, 0.0f, 0.0f );
	C_BaseEntity *pEnt = data.GetEntity();
	if ( pEnt )
	{
		vCenter = pEnt->WorldSpaceCenter();
	}
	else
	{
		IClientRenderable *pRenderable = data.GetRenderable( );
		if ( pRenderable )
		{
			Vector vecMins, vecMaxs;
			pRenderable->GetRenderBoundsWorldspace( vecMins, vecMaxs );
			VectorAdd( vecMins, vecMaxs, vCenter );
			vCenter *= 0.5f;
		}
	}

	Assert( pEmitter );
	pEmitter->GetBinding().SetBBox( vCenter - Vector( 10, 10, 10 ), vCenter + Vector( 10, 10, 10 ) );

	// haxors - make the clip much shorter so the alpha is not 
	// changed based on large clip distances
	pEmitter->SetNearClip( 0, 5 );

	Vector vFlashOffset = vec3_origin;
	Vector vForward(1,0,0);
	int i;

	if( data.m_nHitBox == DOD_MUZZLEFLASH_MG )	//Machine gun
	{
		SimpleParticle *pParticle = (SimpleParticle *)pEmitter->AddParticle( sizeof( SimpleParticle ),
															g_Mat_SMG_Muzzleflash[0],
															vFlashOffset );
		Assert( pParticle );
		if( pParticle )
		{	
			pParticle->m_flLifetime		= 0.0f;
			pParticle->m_flDieTime		= 0.1f;

			pParticle->m_vecVelocity = Vector(0,0,0);

			pParticle->m_uchColor[0]	= 255;
			pParticle->m_uchColor[1]	= 255;
			pParticle->m_uchColor[2]	= 255;

			pParticle->m_uchStartAlpha	= 210.0f;
			pParticle->m_uchEndAlpha	= 0;

			pParticle->m_uchStartSize	= random->RandomFloat( 120, 130 ) * data.m_flMagnitude;
			pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
			pParticle->m_flRoll			= 0;
			pParticle->m_flRollDelta	= 0.0f;
		}
	}
	else
	{
		for( i=0;i<3;i++ )
		{
			//move the three sprites around

			vFlashOffset = vForward * ( (2-i)*6 + 10 );

			SimpleParticle *pParticle = (SimpleParticle *)pEmitter->AddParticle( sizeof( SimpleParticle ),
																g_Mat_SMG_Muzzleflash[1],
																vFlashOffset );
			Assert( pParticle );
			if( pParticle )
			{
				pParticle->m_flLifetime		= 0.0f;
				pParticle->m_flDieTime		= 0.1f;

				pParticle->m_vecVelocity = vec3_origin;

				pParticle->m_uchColor[0]	= 255;
				pParticle->m_uchColor[1]	= 255;
				pParticle->m_uchColor[2]	= 255;

				pParticle->m_uchStartAlpha	= 100.0f;
				pParticle->m_uchEndAlpha	= 30;

				pParticle->m_uchStartSize	= ( 15.0 + 20.0*i ) * data.m_flMagnitude;
				pParticle->m_uchEndSize		= pParticle->m_uchStartSize;
				pParticle->m_flRoll			= random->RandomInt( 0, 360 );
				pParticle->m_flRollDelta	= 0.0f;
			}
		}
	}

	// dynamic light temporary entity for the muzzle flash
	if ( cl_muzzleflash_dlight_1st.GetBool() )
	{
		CPVSFilter filter(pEmitter->GetSortOrigin());
		TE_DynamicLight( filter, 0.0, &(pEmitter->GetSortOrigin()), 255, 192, 64, 5, 70, 0.05, 768 );
	}
}

DECLARE_CLIENT_EFFECT( "DOD_MuzzleFlash", DOD_MuzzleFlashCallback );