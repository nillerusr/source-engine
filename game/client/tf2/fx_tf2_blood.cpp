//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A blood spray effect to expose successful hits.
//
//=============================================================================//

#include "cbase.h"
#include "clienteffectprecachesystem.h"
#include "fx_sparks.h"
#include "iefx.h"
#include "c_te_effect_dispatch.h"
#include "particles_ez.h"
#include "decals.h"
#include "engine/IEngineSound.h"
#include "fx_quad.h"
#include "engine/ivdebugoverlay.h"
#include "shareddefs.h"
#include "fx_blood.h"
#include "tf_shareddefs.h"
#include "view.h"
#include "c_basetfplayer.h"

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectTF2BloodSpray )
CLIENTEFFECT_MATERIAL( "effects/blood_gore" )
CLIENTEFFECT_MATERIAL( "effects/blood_drop" )
CLIENTEFFECT_MATERIAL( "effects/blood_puff" )
CLIENTEFFECT_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			normal - 
//			scale - 
//-----------------------------------------------------------------------------
void FX_TF2_BloodSpray( const Vector &origin, const Vector &normal, float scale, unsigned char r, unsigned char g, unsigned char b, int flags )
{
	if ( UTIL_IsLowViolence() )
		return;

	//debugoverlay->AddLineOverlay( origin, origin + normal * 72, 255, 255, 255, true, 10 ); 

	float spread = 0.2f;
	Vector color = Vector( r / 255.0f, g / 255.0f, b / 255.0f );
	float colorRamp;
	Vector offset;
	int i;

	Vector offDir;
	Vector right;
	Vector up;
	Vector vecNormal = normal;

	// Get the distance to the view
	float flDistance = (origin - MainViewOrigin()).Length();
	float flLODDistance = 0.25 * (flDistance / 512);
	float flDistanceScale = 1.0 + flLODDistance;

	if (vecNormal != Vector(0, 0, 1) )
	{
		right = vecNormal.Cross( Vector(0, 0, 1) );
		up = right.Cross( vecNormal );
	}
	else
	{
		right = Vector(0, 0, 1);
		up = right.Cross( vecNormal );
	}

	// If the normal's too close to being along the view, push it out
	Vector vecForward, vecRight;
	AngleVectors( MainViewAngles(), &vecForward, &vecRight, NULL );
	float flDot = DotProduct( vecNormal, vecForward );
	if ( fabs(flDot) > 0.5 )
	{
		float flPush = random->RandomFloat(0.5, 1.5) + flLODDistance;
		float flRightDot = DotProduct( vecNormal, vecRight );
		// If we're up close, randomly move it around. If we're at a distance, always push it to the side
		// Up close, this can move it back towards the view, but the random chance still looks better
		if ( ( flDistance >= 512 && flRightDot > 0 ) || ( flDistance < 512 && RandomFloat(0,1) > 0.5 ) )
		{
			// Turn it to the right
			vecNormal += (vecRight * flPush);
		}
		else
		{
			// Turn it to the left
			vecNormal -= (vecRight * flPush);
		}
	}

	//
	// Dump out drops
	//
	// Don't bother with these over midrange distance
	if (flags & FX_BLOODSPRAY_DROPS && ( flDistance < 1500 ) )
	{
		TrailParticle *tParticle;

		CSmartPtr<CTrailParticles> pTrailEmitter = CTrailParticles::Create( "blooddrops" );
		if ( !pTrailEmitter )
			return;

		pTrailEmitter->SetSortOrigin( origin );

		// Partial gravity on blood drops.
		pTrailEmitter->SetGravity( 600.0 ); 
		
		// Enable simple collisions with nearby surfaces.
		pTrailEmitter->Setup(origin, &vecNormal, 1, 10, 100, 600, 0.2, 0 );

		PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_drop" );

		//
		// Long stringy drops of blood.
		//
		for ( i = 0; i < 7; i++ )
		{
			// Originate from within a circle 'scale' inches in diameter.
			offset = origin;
			offset += right * random->RandomFloat( -0.5f, 0.5f ) * scale;
			offset += up * random->RandomFloat( -0.5f, 0.5f ) * scale;

			tParticle = (TrailParticle *) pTrailEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

			if ( tParticle == NULL )
				break;

			tParticle->m_flLifetime	= 0.0f;

			offDir = vecNormal + RandomVector( -0.3f, 0.3f );

			tParticle->m_vecVelocity = offDir * random->RandomFloat( 4.0f * scale, 40.0f * scale );
			tParticle->m_vecVelocity[2] += random->RandomFloat( 4.0f, 16.0f ) * scale;

			tParticle->m_flWidth		= random->RandomFloat( 0.5f, 1.0f ) * scale * (flDistanceScale * 1.25);
			tParticle->m_flLength		= random->RandomFloat( 0.02f, 0.03f ) * scale * (flDistanceScale * 1.25);
			tParticle->m_flDieTime		= random->RandomFloat( 0.5f, 1.0f );

			// Ramp up the brightness as it gets farther away
			colorRamp = random->RandomFloat( 0.5f, 0.75f ) + flLODDistance;
			FloatToColor32( tParticle->m_color, MIN( 1.0, color[0] * colorRamp ), MIN( 1.0, color[1] * colorRamp ), MIN( 1.0, color[2] * colorRamp ), 1.0f );
		}

		//
		// Shorter droplets.
		//
		// Only do these at short range
		if ( flDistance < 512 )
		{
			for ( i = 0; i < 10; i++ )
			{
				// Originate from within a circle 'scale' inches in diameter.
				offset = origin;
				offset += right * random->RandomFloat( -0.5f, 0.5f ) * scale;
				offset += up * random->RandomFloat( -0.5f, 0.5f ) * scale;

				tParticle = (TrailParticle *) pTrailEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

				if ( tParticle == NULL )
					break;

				tParticle->m_flLifetime	= 0.0f;

				offDir = vecNormal + RandomVector( -1.0f, 1.0f );
				offDir[2] += random->RandomFloat(0, 1.0f);

				tParticle->m_vecVelocity = offDir * random->RandomFloat( 2.0f * scale, 25.0f * scale );
				tParticle->m_vecVelocity[2] += random->RandomFloat( 4.0f, 16.0f ) * scale;

				tParticle->m_flWidth		= random->RandomFloat( 0.25f, 0.375f ) * scale * flDistanceScale;
				tParticle->m_flLength		= random->RandomFloat( 0.0025f, 0.005f ) * scale * flDistanceScale;
				tParticle->m_flDieTime		= random->RandomFloat( 0.5f, 1.0f );

				colorRamp = random->RandomFloat( 0.5f, 0.75f ) + flLODDistance;
				FloatToColor32( tParticle->m_color, MIN( 1.0, color[0] * colorRamp ), MIN( 1.0, color[1] * colorRamp ), MIN( 1.0, color[2] * colorRamp ), 1.0f );
			}
		}
	}

	if ((flags & FX_BLOODSPRAY_GORE) || (flags & FX_BLOODSPRAY_CLOUD))
	{
		CSmartPtr<CBloodSprayEmitter> pSimple = CBloodSprayEmitter::Create( "bloodgore" );
		if ( !pSimple )
			return;

		pSimple->SetSortOrigin( origin );
		pSimple->SetGravity( 0 );

		PMaterialHandle	hMaterial;

		//
		// Tight blossom of blood at the center.
		//
		if (flags & FX_BLOODSPRAY_GORE)
		{
			hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_gore" );

			SimpleParticle *pParticle;

			for ( i = 0; i < 6; i++ )
			{
				// Originate from within a circle 'scale' inches in diameter.
				offset = origin + ( 0.5 * scale * vecNormal );
				offset += right * random->RandomFloat( -0.5f, 0.5f ) * scale;
				offset += up * random->RandomFloat( -0.5f, 0.5f ) * scale;

				pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, offset );

				if ( pParticle != NULL )
				{
					pParticle->m_flLifetime = 0.0f;
					pParticle->m_flDieTime	= 0.3f;

					spread = 0.2f;
					pParticle->m_vecVelocity.Random( -spread, spread );
					pParticle->m_vecVelocity += vecNormal * random->RandomInt( 10, 100 );
					//VectorNormalize( pParticle->m_vecVelocity );

					colorRamp = random->RandomFloat( 0.75f, 1.0f ) + flLODDistance;

					pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
					
					pParticle->m_uchStartSize	= random->RandomFloat( scale, scale * 4 ) * flDistanceScale;
					pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 2 * flDistanceScale;
					
					pParticle->m_uchStartAlpha	= random->RandomInt( 200, 255 );
					pParticle->m_uchEndAlpha	= 0;
					
					pParticle->m_flRoll			= random->RandomInt( 0, 360 );
					pParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
				}
			}
		}

		//
		// Diffuse cloud just in front of the exit wound.
		//
		if (flags & FX_BLOODSPRAY_CLOUD)
		{
			hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_puff" );

			SimpleParticle *pParticle;

			for ( i = 0; i < 6; i++ )
			{
				// Originate from within a circle '2 * scale' inches in diameter.
				offset = origin + ( scale * vecNormal * 0.5 );
				offset += right * random->RandomFloat( -1, 1 ) * scale;
				offset += up * random->RandomFloat( -1, 1 ) * scale;

				pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, offset );

				if ( pParticle != NULL )
				{
					pParticle->m_flLifetime = 0.0f;
					pParticle->m_flDieTime	= random->RandomFloat( 0.5f, 0.8f);

					spread = 0.5f;
					pParticle->m_vecVelocity.Random( -spread, spread );
					pParticle->m_vecVelocity += vecNormal * random->RandomInt( 100, 200 );

					colorRamp = random->RandomFloat( 0.5f, 0.75f ) + flLODDistance;

					pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
					
					pParticle->m_uchStartSize	= random->RandomFloat( scale * 0.5, scale ) * flDistanceScale;
					pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 4 * flDistanceScale;
					
					pParticle->m_uchStartAlpha	= random->RandomInt( 80, 128 );
					pParticle->m_uchEndAlpha	= 0;
					
					pParticle->m_flRoll			= random->RandomInt( 0, 360 );
					pParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
				}
			}
		}
	}

	// TODO: Play a sound?
	//CLocalPlayerFilter filter;
	//C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, CHAN_VOICE, "Physics.WaterSplash", 1.0, ATTN_NORM, 0, 100, &origin );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bloodtype - 
//			r - 
//			g - 
//			b - 
//-----------------------------------------------------------------------------
void GetBloodColorForTeam( int iTeam, unsigned char &r, unsigned char &g, unsigned char &b )
{
	if ( iTeam == TEAM_ALIENS )
	{
		r = 0;
		g = 255;
		b = 0;
	}
	else if ( iTeam == TEAM_HUMANS )
	{
		// Humans
		r = 255;
		g = 0;
		b = 0;
	}
	else 
	{
		// NPCs?
		r = 200;
		g = 0;
		b = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Intercepts the blood spray message.
//-----------------------------------------------------------------------------
void TF2BloodSprayCallback( const CEffectData &data )
{
	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;

	// Get the entity we've hit
	C_BaseEntity *pEntity = ClientEntityList().GetEnt( data.m_nEntIndex );
	if ( pEntity )
	{
		GetBloodColorForTeam( pEntity->GetTeamNumber(), r, g, b );
		if ( pEntity->IsPlayer() )
		{
			C_BaseTFPlayer *pPlayer = (C_BaseTFPlayer *)pEntity;
			pPlayer->PainSound();
		}
	}
	else
	{
		GetBloodColorForTeam( 0, r, g, b );
	}
	FX_TF2_BloodSpray( data.m_vOrigin, data.m_vNormal, data.m_flScale, r, g, b, data.m_fFlags );
}

DECLARE_CLIENT_EFFECT( "tf2blood", TF2BloodSprayCallback );
