//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "precache_register.h"
#include "FX_Sparks.h"
#include "iefx.h"
#include "c_te_effect_dispatch.h"
#include "particles_ez.h"
#include "decals.h"
#include "engine/IEngineSound.h"
#include "fx_quad.h"
#include "tier0/vprof.h"
#include "fx.h"
#include "fx_water.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectSplash )
PRECACHE( MATERIAL, "effects/splash1" )
PRECACHE( MATERIAL, "effects/splash2" )
PRECACHE( MATERIAL, "effects/splash4" )
PRECACHE( MATERIAL, "effects/slime1" )
PRECACHE_REGISTER_END()


#define	SPLASH_MIN_SPEED	50.0f
#define	SPLASH_MAX_SPEED	100.0f

ConVar	cl_show_splashes( "cl_show_splashes", "1" );

static Vector s_vecSlimeColor( 46.0f/255.0f, 90.0f/255.0f, 36.0f/255.0f );

// Each channel does not contribute to the luminosity equally, as represented here
#define	RED_CHANNEL_CONTRIBUTION	0.30f
#define GREEN_CHANNEL_CONTRIBUTION	0.59f
#define	BLUE_CHANNEL_CONTRIBUTION	0.11f

//-----------------------------------------------------------------------------
// Purpose: Returns a normalized tint and luminosity for a specified color
// Input  : &color - normalized input color to extract information from
//			*tint - normalized tint of that color
//			*luminosity - normalized luminosity of that color
//-----------------------------------------------------------------------------
void UTIL_GetNormalizedColorTintAndLuminosity( const Vector &color, Vector *tint, float *luminosity )
{
	// Give luminosity if requested
	if ( luminosity != NULL )
	{
		// Each channel contributes differently than the others
		*luminosity =	( color.x * RED_CHANNEL_CONTRIBUTION ) +
						( color.y * GREEN_CHANNEL_CONTRIBUTION ) +
						( color.z * BLUE_CHANNEL_CONTRIBUTION );
	}

	// Give tint if requested
	if ( tint != NULL )
	{
		if ( color == vec3_origin )
		{
			*tint = vec3_origin;
		}
		else
		{
			float maxComponent = MAX( color.x, MAX( color.y, color.z ) );
			*tint = color / maxComponent;
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//			scale - 
//-----------------------------------------------------------------------------
void FX_WaterRipple( const Vector &origin, float scale, Vector *pColor, float flLifetime, float flAlpha )
{
	VPROF_BUDGET( "FX_WaterRipple", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	trace_t	tr;

	Vector	color = pColor ? *pColor : Vector( 0.8f, 0.8f, 0.75f );

	Vector startPos = origin + Vector(0,0,8);
	Vector endPos = origin + Vector(0,0,-64);

	UTIL_TraceLine( startPos, endPos, MASK_WATER, NULL, COLLISION_GROUP_NONE, &tr );
	
	if ( tr.fraction < 1.0f )
	{
		QAngle vecAngles;
		// we flip the z and the x to match the orientation of how the impact particles are authored
		// all impact particles are authored with the effect going "up" (0, 0, 1)
		VectorAngles( Vector( tr.plane.normal.z, tr.plane.normal.y, tr.plane.normal.x ), vecAngles );
		DispatchParticleEffect( "water_splash_02_surface2", tr.endpos, vecAngles, NULL );
	}
}

PRECACHE_REGISTER_BEGIN( SHARED_SYSTEM, FX_WaterRipple )
	PRECACHE( PARTICLE_SYSTEM, "water_splash_02_surface2" )
	//PRECACHE( MATERIAL, "effects/splashwake1" )
PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//-----------------------------------------------------------------------------
void FX_GunshotSplashVisuals( const Vector &origin, const Vector &normal, float scale )
{
	VPROF_BUDGET( "FX_GunshotSplash", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
  	if ( cl_show_splashes.GetBool() == false )
		return;

	QAngle vecAngles;
	// we flip the z and the x to match the orientation of how the impact particles are authored
	// all impact particles are authored with the effect going "up" (0, 0, 1)
	VectorAngles( Vector( normal.z, normal.y, normal.x ), vecAngles );
	if ( scale < 4.0f )
	{
		DispatchParticleEffect( "water_splash_01", origin, vecAngles );
	}
	else if ( scale < 8.0f )
	{
		DispatchParticleEffect( "water_splash_02", origin, vecAngles );
	}
	else
	{
		DispatchParticleEffect( "water_splash_03", origin, vecAngles );
	}
}

void FX_GunshotSplashSound( const Vector &origin, const Vector &normal, float scale )
{
	//Play a sound
	CLocalPlayerFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName =  "Physics.WaterSplash";
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_NORM;
	ep.m_pOrigin = &origin;


	C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
}

PRECACHE_REGISTER_BEGIN( SHARED_SYSTEM, FX_GunshotSplash )
	PRECACHE( PARTICLE_SYSTEM, "water_splash_01" )
	PRECACHE( PARTICLE_SYSTEM, "water_splash_02" )
	PRECACHE( PARTICLE_SYSTEM, "water_splash_03" )
	//PRECACHE( MATERIAL, "effects/splash2" )
	PRECACHE( GAMESOUND, "Physics.WaterSplash" )
PRECACHE_REGISTER_END()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//-----------------------------------------------------------------------------
void FX_GunshotSplash( const Vector &origin, const Vector &normal, float scale )
{
	VPROF_BUDGET( "FX_GunshotSplash", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	if ( cl_show_splashes.GetBool() == false )
		return;

	FX_GunshotSplashVisuals( origin, normal, scale );
	FX_GunshotSplashSound( origin, normal, scale );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//			scale - 
//			*pColor - 
//-----------------------------------------------------------------------------
void FX_GunshotSlimeSplash( const Vector &origin, const Vector &normal, float scale )
{
	if ( cl_show_splashes.GetBool() == false )
		return;

	VPROF_BUDGET( "FX_GunshotSlimeSplash", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
	QAngle vecAngles;
	// we flip the z and the x to match the orientation of how the impact particles are authored
	// all impact particles are authored with the effect going "up" (0, 0, 1)
	VectorAngles( Vector( normal.z, normal.y, normal.x ), vecAngles );
	if ( scale < 2.0f )
	{
		DispatchParticleEffect( "slime_splash_01", origin, vecAngles );
	}
	else if ( scale < 4.0f )
	{
		DispatchParticleEffect( "slime_splash_02", origin, vecAngles );
	}
	else
	{
		DispatchParticleEffect( "slime_splash_03", origin, vecAngles );
	}

	//Play a sound
	CLocalPlayerFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName =  "Physics.WaterSplash";
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_NORM;
	ep.m_pOrigin = &origin;

	C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
}

PRECACHE_REGISTER_BEGIN( SHARED_SYSTEM, FX_GunshotSlimeSplash )
	PRECACHE( PARTICLE_SYSTEM, "slime_splash_01" )
	PRECACHE( PARTICLE_SYSTEM, "slime_splash_02" )
	PRECACHE( PARTICLE_SYSTEM, "slime_splash_03" )
	PRECACHE( GAMESOUND, "Physics.WaterSplash" )
PRECACHE_REGISTER_END()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SplashCallback( const CEffectData &data )
{
	Vector	normal;

	AngleVectors( data.m_vAngles, &normal );

	if ( data.m_fFlags & FX_WATER_IN_SLIME )
	{
		FX_GunshotSlimeSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
	else
	{
		FX_GunshotSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( watersplash, SplashCallback )
	PRECACHE( SHARED, "FX_GunshotSlimeSplash" )
	PRECACHE( SHARED, "FX_GunshotSplash" )
DECLARE_CLIENT_EFFECT_END()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SplashQuietCallback( const CEffectData &data )
{
	Vector	normal;

	AngleVectors( data.m_vAngles, &normal );

	if ( data.m_fFlags & FX_WATER_IN_SLIME )
	{
		FX_GunshotSlimeSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
	else
	{
		FX_GunshotSplashVisuals( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( watersplashquiet, SplashQuietCallback )
DECLARE_CLIENT_EFFECT_END()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void GunshotSplashCallback( const CEffectData &data )
{
	if ( data.m_fFlags & FX_WATER_IN_SLIME )
	{
		FX_GunshotSlimeSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
	else
	{
		FX_GunshotSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( gunshotsplash, GunshotSplashCallback )
	PRECACHE( SHARED, "FX_GunshotSlimeSplash" )
	PRECACHE( SHARED, "FX_GunshotSplash" )
DECLARE_CLIENT_EFFECT_END()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void RippleCallback( const CEffectData &data )
{
	float	flScale = data.m_flScale / 8.0f;

	Vector	color;
	float	luminosity;
	
	// Get our lighting information
	FX_GetSplashLighting( data.m_vOrigin + ( Vector(0,0,1) * 4.0f ), &color, &luminosity );

	FX_WaterRipple( data.m_vOrigin, flScale, &color, 1.5f, luminosity );
}

DECLARE_CLIENT_EFFECT_BEGIN( waterripple, RippleCallback )
	PRECACHE( SHARED, "FX_WaterRipple" )
DECLARE_CLIENT_EFFECT_END()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDebugName - 
// Output : WaterDebrisEffect*
//-----------------------------------------------------------------------------
WaterDebrisEffect* WaterDebrisEffect::Create( const char *pDebugName )
{
	return new WaterDebrisEffect( pDebugName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float WaterDebrisEffect::UpdateAlpha( const SimpleParticle *pParticle )
{
	return ( ((float)pParticle->m_uchStartAlpha/255.0f) * sin( M_PI * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float CSplashParticle::UpdateRoll( SimpleParticle *pParticle, float timeDelta )
{
	pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
	
	pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -4.0f );

	//Cap the minimum roll
	if ( fabs( pParticle->m_flRollDelta ) < 0.5f )
	{
		pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.5f : -0.5f;
	}

	return pParticle->m_flRoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
void CSplashParticle::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
	//Decellerate
	static float dtime;
	static float decay;

	if ( dtime != timeDelta )
	{
		dtime = timeDelta;
		float expected = 3.0f;
		decay = exp( log( 0.0001f ) * dtime / expected );
	}

	pParticle->m_vecVelocity *= decay;
	pParticle->m_vecVelocity[2] -= ( 800.0f * timeDelta );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
// Output : float
//-----------------------------------------------------------------------------
float CSplashParticle::UpdateAlpha( const SimpleParticle *pParticle )
{
	if ( m_bUseClipHeight )
	{
		float flAlpha = pParticle->m_uchStartAlpha / 255.0f;

		return  flAlpha * RemapValClamped(pParticle->m_Pos.z,
								m_flClipHeight,
								m_flClipHeight - ( UpdateScale( pParticle ) * 0.5f ),
								1.0f,
								0.0f );
	}

	return (pParticle->m_uchStartAlpha/255.0f) + ( (float)(pParticle->m_uchEndAlpha/255.0f) - (float)(pParticle->m_uchStartAlpha/255.0f) ) * (pParticle->m_flLifetime / pParticle->m_flDieTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &clipPlane - 
//-----------------------------------------------------------------------------
void CSplashParticle::SetClipHeight( float flClipHeight )
{
	m_bUseClipHeight = true;
	m_flClipHeight = flClipHeight;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pIterator - 
//-----------------------------------------------------------------------------
void CSplashParticle::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();

	SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
	
	while ( pParticle )
	{
		//Update velocity
		UpdateVelocity( pParticle, timeDelta );
		pParticle->m_Pos += pParticle->m_vecVelocity * timeDelta;

		// Clip by height if requested
		if ( m_bUseClipHeight )
		{
			// See if we're below, and therefore need to clip
			if ( pParticle->m_Pos.z + UpdateScale( pParticle ) < m_flClipHeight )
			{
				pIterator->RemoveParticle( pParticle );
				pParticle = (SimpleParticle*)pIterator->GetNext();
				continue;
			}
		}

		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;
		UpdateRoll( pParticle, timeDelta );

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			pIterator->RemoveParticle( pParticle );

		pParticle = (SimpleParticle*)pIterator->GetNext();
	}
}
