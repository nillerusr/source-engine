//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Effects played when objects are building
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
#include "tf_shareddefs.h"
#include "c_impact_effects.h"
#include "fx.h"
#include "iviewrender_beams.h"
#include "view.h"
#include "IEffects.h"
#include "c_tracer.h"

CLIENTEFFECT_REGISTER_BEGIN( PrecacheTF2EffectBuild )
CLIENTEFFECT_MATERIAL( "effects/blood" )
CLIENTEFFECT_MATERIAL( "effects/human_build_warp" )
CLIENTEFFECT_MATERIAL( "effects/tesla_glow_noz" )
CLIENTEFFECT_MATERIAL( "particle/particle_smokegrenade" )
CLIENTEFFECT_MATERIAL( "effects/human_tracers/human_sparksprite_A1" )
CLIENTEFFECT_MATERIAL( "effects/human_tracers/human_sparktracer_A_" )
CLIENTEFFECT_MATERIAL( "effects/alien_tracers/alien_pbtracer_A_" )
CLIENTEFFECT_MATERIAL( "effects/alien_tracers/alien_pbsprite_A1" )
CLIENTEFFECT_REGISTER_END()


//-----------------------------------------------------------------------------
// Purpose: Build impact
//-----------------------------------------------------------------------------
void FX_BuildImpact( const Vector &origin, const QAngle &vecAngles, const Vector &vecNormal, float flScale, bool bGround = false, CBaseEntity *pIgnore = NULL )
{
	Vector	offset;
	float	spread = 0.1f;
	
	CSmartPtr<CDustParticle> pSimple = CDustParticle::Create( "dust" );
	pSimple->SetSortOrigin( origin );

	SimpleParticle	*pParticle;

	Vector color( 0.35, 0.35, 0.35 );
	float  colorRamp;

	// If we're hitting the ground, try and get the ground color
	if ( bGround )
	{
		trace_t	tr;
		UTIL_TraceLine( origin, origin + Vector(0,0,-32), MASK_SHOT, pIgnore, COLLISION_GROUP_NONE, &tr );
		GetColorForSurface( &tr, &color );
	}

	int iNumPuffs = 8;
	for ( int i = 0; i < iNumPuffs; i++ )
	{
		QAngle vecTemp = vecAngles;
		vecTemp[YAW] += (360 / iNumPuffs) * i;
		Vector vecForward;
		AngleVectors( vecTemp, &vecForward );

		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), g_Mat_DustPuff[0], origin );

		if ( pParticle != NULL )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= RandomFloat( 0.5f, 1.0f );

			pParticle->m_vecVelocity.Random( -spread, spread );
			pParticle->m_vecVelocity += ( vecForward * RandomFloat( 1.0f, 6.0f ) );
			
			VectorNormalize( pParticle->m_vecVelocity );

			float	fForce = RandomFloat( 500, 750 );

			// scaled
			pParticle->m_vecVelocity *= fForce * flScale;
			
			colorRamp = RandomFloat( 0.75f, 1.25f );
			pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
			pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
			pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
			
			// scaled
			pParticle->m_uchStartSize	= flScale * RandomInt( 15, 20 );

			// scaled
			pParticle->m_uchEndSize		= flScale * pParticle->m_uchStartSize * 4;
			
			pParticle->m_uchStartAlpha	= RandomInt( 32, 255 );
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= RandomFloat( -8.0f, 8.0f );
		}
	}			
}

//-----------------------------------------------------------------------------
// Purpose: Large dust impact
//-----------------------------------------------------------------------------
void BuildImpactCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;
	Vector vecNormal = data.m_vNormal;

	FX_BuildImpact( vecOrigin, vecAngles, vecNormal, 1 );

	// Randomly play sparks
	if ( RandomFloat() > 0.35 )
	{
		// Angle them up
		vecAngles[PITCH] = -90;
		Vector vecForward;
		AngleVectors( vecAngles, &vecForward );

		// Sparks
		FX_Sparks( vecOrigin, 2, 4, vecForward, 2.5, 48, 64 );
	}
}

DECLARE_CLIENT_EFFECT( "BuildImpact", BuildImpactCallback );

//-----------------------------------------------------------------------------
// Purpose: Small dust impact
//-----------------------------------------------------------------------------
void BuildImpactSmallCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;
	Vector vecNormal = data.m_vNormal;

	FX_BuildImpact( vecOrigin, vecAngles, vecNormal, 0.65 );
}

DECLARE_CLIENT_EFFECT( "BuildImpactSmall", BuildImpactSmallCallback );

//-----------------------------------------------------------------------------
// Purpose: Large dust impact to be used only when something's landed on the ground
//-----------------------------------------------------------------------------
void BuildImpactGroundCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;
	Vector vecNormal = data.m_vNormal;
	int iEntIndex = data.m_nEntIndex;

	C_BaseEntity *pEntity = ClientEntityList().GetEnt( iEntIndex );
	FX_BuildImpact( vecOrigin, vecAngles, vecNormal, 1, true, pEntity );
}

DECLARE_CLIENT_EFFECT( "BuildImpactGround", BuildImpactGroundCallback );


//-----------------------------------------------------------------------------
// Purpose: Create a tesla effect between two points
//-----------------------------------------------------------------------------
void TF2_FX_BuildTesla( C_BaseEntity *pEntity, Vector &vecOrigin, Vector &vecEnd )
{
	BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMTESLA;
	beamInfo.m_vecStart = vecOrigin;
	beamInfo.m_vecEnd = vecEnd;
	beamInfo.m_pszModelName = "sprites/physbeam.vmt";
	beamInfo.m_flHaloScale = 0.0;
	beamInfo.m_flLife = RandomFloat( 0.3, 0.55 );
	beamInfo.m_flWidth = 5.0;
	beamInfo.m_flEndWidth = 1;
	beamInfo.m_flFadeLength = 0.3;
	beamInfo.m_flAmplitude = 16;
	beamInfo.m_flBrightness = 200.0;
	beamInfo.m_flSpeed = 0.0;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 1.0;
	beamInfo.m_flRed = 255.0;
	beamInfo.m_flGreen = 255.0;
	beamInfo.m_flBlue = 255.0;
	beamInfo.m_nSegments = 20;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE;
	
	beams->CreateBeamPoints( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Tesla effect
//-----------------------------------------------------------------------------
void TF2_BuildTeslaCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;
	int iEntIndex = data.m_nEntIndex;
	C_BaseEntity *pEntity = ClientEntityList().GetEnt( iEntIndex );

	// Send out beams around us
	int iNumBeamsAround = 4;
	int iNumRandomBeams = 2;
	int iTotalBeams = iNumBeamsAround + iNumRandomBeams;
	float flYawOffset = RandomFloat(0,360);
	for ( int i = 0; i < iTotalBeams; i++ )
	{
		// Make a couple of tries at it
		int iTries = -1;
		Vector vecForward;
		trace_t tr;
		do
		{
			iTries++;

			// Some beams are deliberatly aimed around the point, the rest are random.
			if ( i < iNumBeamsAround )
			{
				QAngle vecTemp = vecAngles;
				vecTemp[YAW] += anglemod( flYawOffset + ((360 / iTotalBeams) * i) );
				AngleVectors( vecTemp, &vecForward );

				// Randomly angle it up or down
				vecForward.z = RandomFloat( -1, 1 );
			}
			else
			{
				vecForward = RandomVector( -1, 1 );
			}

			UTIL_TraceLine( vecOrigin, vecOrigin + (vecForward * 192), MASK_SHOT, pEntity, COLLISION_GROUP_NONE, &tr );
		} while ( tr.fraction >= 1.0 && iTries < 3 );

		Vector vecEnd = tr.endpos - (vecForward * 8);

		// Only spark & glow if we hit something
		if ( tr.fraction < 1.0 )
		{
			if ( !EffectOccluded( tr.endpos ) )
			{
				// Move it towards the camera
				Vector vecFlash = tr.endpos;
				Vector vecForward;
				AngleVectors( MainViewAngles(), &vecForward );
				vecFlash -= (vecForward * 8);

				g_pEffects->EnergySplash( vecFlash, -vecForward, false );

				// End glow
				CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "dust" );
				pSimple->SetSortOrigin( vecFlash );
				SimpleParticle *pParticle;
				pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/tesla_glow_noz" ), vecFlash );
				if ( pParticle != NULL )
				{
					pParticle->m_flLifetime = 0.0f;
					pParticle->m_flDieTime	= RandomFloat( 0.5, 1 );
					pParticle->m_vecVelocity = vec3_origin;
					Vector color( 1,1,1 );
					float  colorRamp = RandomFloat( 0.75f, 1.25f );
					pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
					pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
					pParticle->m_uchStartSize	= RandomFloat( 6,13 );
					pParticle->m_uchEndSize		= pParticle->m_uchStartSize - 2;
					pParticle->m_uchStartAlpha	= 255;
					pParticle->m_uchEndAlpha	= 10;
					pParticle->m_flRoll			= RandomFloat( 0,360 );
					pParticle->m_flRollDelta	= 0;
				}
			}
		}

		// Build the tesla
		TF2_FX_BuildTesla( pEntity, vecOrigin, tr.endpos );
	}
}

DECLARE_CLIENT_EFFECT( "TF2BuildTesla", TF2_BuildTeslaCallback );

//-----------------------------------------------------------------------------
// Purpose: WarpParticle emitter 
//			This is a particle that scales up to its max size in WARPEMMITER_MIDPOINT 
//			of it's lifetime, the drops back to its initial size by the end of its life.
//			Alpha scales the same way.
//-----------------------------------------------------------------------------
#define WARPEMMITER_MIDPOINT	0.6

class CWarpParticleEmitter : public CSimpleEmitter
{
public:
	
	CWarpParticleEmitter( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	
	//Create
	static CWarpParticleEmitter *Create( const char *pDebugName="dust" )
	{
		return new CWarpParticleEmitter( pDebugName );
	}

	// Scale
	virtual float UpdateScale( const SimpleParticle *pParticle )
	{
		float	tLifeTime = pParticle->m_flLifetime / pParticle->m_flDieTime;

		// Ramp up for the first 75% of my life, then reduce for the rest
		if ( tLifeTime < WARPEMMITER_MIDPOINT )
		{
			tLifeTime = RemapVal( tLifeTime, 0, WARPEMMITER_MIDPOINT, 0, 1.0 );
			return (float)pParticle->m_uchStartSize + ( (float)pParticle->m_uchEndSize - (float)pParticle->m_uchStartSize ) * tLifeTime;
		}

		tLifeTime -= WARPEMMITER_MIDPOINT;
		tLifeTime = RemapVal( tLifeTime, 0, 1 - WARPEMMITER_MIDPOINT, 0, 1.0 );
		return (float)pParticle->m_uchEndSize - ( (float)pParticle->m_uchEndSize - (float)pParticle->m_uchStartSize ) * tLifeTime;
	}

	//Alpha
	virtual float UpdateAlpha( const SimpleParticle *pParticle )
	{
		float tLifeTime = pParticle->m_flLifetime / pParticle->m_flDieTime;
		float flAlpha = 0;

		// Ramp up for the first 75% of my life, then reduce for the rest
		if ( tLifeTime < WARPEMMITER_MIDPOINT )
		{
			tLifeTime = RemapVal( tLifeTime, 0, WARPEMMITER_MIDPOINT, 0, 1.0 );
			flAlpha = (float)pParticle->m_uchStartAlpha + ( (float)pParticle->m_uchEndAlpha - (float)pParticle->m_uchStartAlpha ) * tLifeTime;
		}
		else
		{
			tLifeTime -= WARPEMMITER_MIDPOINT;
			tLifeTime = RemapVal( tLifeTime, 0, 1 - WARPEMMITER_MIDPOINT, 0, 1.0 );
			flAlpha = (float)pParticle->m_uchEndAlpha - ( (float)pParticle->m_uchEndAlpha - (float)pParticle->m_uchStartAlpha ) * tLifeTime;
		}

		flAlpha = flAlpha / 255;
		return flAlpha;
	}

private:
	CWarpParticleEmitter( const CWarpParticleEmitter & ); // not defined, not accessible
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void FX_BuildWarp( Vector &vecOrigin, QAngle &vecAngles, float flScale )
{
	if ( EffectOccluded( vecOrigin ) )
		return;

	CSmartPtr<CSimpleEmitter> pSimple = CSimpleEmitter::Create( "dust" );
	pSimple->SetSortOrigin( vecOrigin );

	SimpleParticle *pParticle;

	Vector color( 1, 1, 1 );
	float  colorRamp;

	// Big flash
	pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), pSimple->GetPMaterial( "effects/blueflare2" ), vecOrigin );
	if ( pParticle != NULL )
	{
		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime	= 0.5;
		pParticle->m_vecVelocity = vec3_origin;
		colorRamp = RandomFloat( 0.75f, 1.25f );
		pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
		pParticle->m_uchStartSize	= RandomFloat( 10,15 );
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 8 * flScale;
		pParticle->m_uchStartAlpha	= 48;
		pParticle->m_uchEndAlpha	= 0;
		pParticle->m_flRoll			= 0;
		pParticle->m_flRollDelta	= 0;
	}

	// Bright light
	// Move it towards the camera
	Vector vecForward;
	AngleVectors( MainViewAngles(), &vecForward );
	vecOrigin -= (vecForward * 8);
	CSmartPtr<CWarpParticleEmitter> pWarpEmitter = CWarpParticleEmitter::Create( "dust" );
	pWarpEmitter->SetSortOrigin( vecOrigin );

	pParticle = (SimpleParticle *) pWarpEmitter->AddParticle( sizeof( SimpleParticle ), pWarpEmitter->GetPMaterial( "effects/human_build_warp" ), vecOrigin );
	if ( pParticle != NULL )
	{
		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime	= 0.4;
		pParticle->m_vecVelocity = vec3_origin;

		colorRamp = RandomFloat( 0.75f, 1.25f );
		pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
		
		pParticle->m_uchStartSize	= RandomInt( 10,13 ) * flScale;
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 9;
		
		pParticle->m_uchStartAlpha	= 32;
		pParticle->m_uchEndAlpha	= 192;
		
		pParticle->m_flRoll			= 0;
		pParticle->m_flRollDelta	= 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Warp effect
//-----------------------------------------------------------------------------
void BuildWarpCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;

	// Warp effect
	FX_BuildWarp( vecOrigin, vecAngles, 2 );
	g_pEffects->EnergySplash( vecOrigin, Vector(0,0,1), false );
}

DECLARE_CLIENT_EFFECT( "BuildWarp", BuildWarpCallback );


//-----------------------------------------------------------------------------
// Purpose: Small Warp effect
//-----------------------------------------------------------------------------
void BuildWarpSmallCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;

	// Warp effect
	FX_BuildWarp( vecOrigin, vecAngles, 1.5 );
	g_pEffects->EnergySplash( vecOrigin, Vector(0,0,1), false );
}

DECLARE_CLIENT_EFFECT( "BuildWarpSmall", BuildWarpSmallCallback );

//-----------------------------------------------------------------------------
// Purpose: Spark effects
//-----------------------------------------------------------------------------
void BuildSparksCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;

	// Angle them up
	vecAngles[PITCH] = -90;
	Vector vecForward;
	AngleVectors( vecAngles, &vecForward );

	// Sparks
	FX_Sparks( vecOrigin, 2, 4, vecForward, 2.5, 48, 64 );
}

DECLARE_CLIENT_EFFECT( "BuildSparks", BuildSparksCallback );

//-----------------------------------------------------------------------------
// Purpose: Red Spark effects
//-----------------------------------------------------------------------------
void BuildSparksRedCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;

	// Angle them up
	vecAngles[PITCH] = -90;
	Vector vecForward;
	AngleVectors( vecAngles, &vecForward );

	// Sparks
	FX_Sparks( vecOrigin, 2, 4, vecForward, 2.5, 48, 64, "effects/spark2" );
}

DECLARE_CLIENT_EFFECT( "BuildSparksRed", BuildSparksRedCallback );

//-----------------------------------------------------------------------------
// Purpose: Electric sparks effect
//-----------------------------------------------------------------------------
void BuildSparksElectricCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	Vector vecNormal = data.m_vNormal;

	// Sparks
	FX_ElectricSpark( vecOrigin, 2, 4, &vecNormal );
}

DECLARE_CLIENT_EFFECT( "BuildSparksElectric", BuildSparksElectricCallback );

//-----------------------------------------------------------------------------
// Purpose: Metal sparks effect
//-----------------------------------------------------------------------------
void BuildSparksMetalCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	Vector vecNormal = data.m_vNormal;

	// Sparks
	FX_MetalSpark( vecOrigin, vecNormal, vecNormal, 2 );
}

DECLARE_CLIENT_EFFECT( "BuildSparksMetal", BuildSparksMetalCallback );

//-----------------------------------------------------------------------------
// Purpose: Metal scrape effect
//-----------------------------------------------------------------------------
void BuildMetalScrapeCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	Vector vecNormal = data.m_vNormal;

	// Sparks
	FX_MetalScrape( vecOrigin, vecNormal );
}

DECLARE_CLIENT_EFFECT( "BuildMetalScrape", BuildMetalScrapeCallback );

//-----------------------------------------------------------------------------
// Purpose: Warp effect that looks like it's sucking things to it
//-----------------------------------------------------------------------------
void FX_BuildWarpSuck( Vector &vecOrigin, QAngle &vecAngles, float flScale )
{
	CSmartPtr<CTrailParticles> pEmitter = CTrailParticles::Create( "BuildWarpSuck" );
	PMaterialHandle	hParticleMaterial = pEmitter->GetPMaterial( "effects/bluespark" );
	pEmitter->Setup( (Vector &) vecOrigin, 
						NULL, 
						0.0, 
						0, 
						64, 
						0, 
						0, 
						bitsPARTICLE_TRAIL_VELOCITY_DAMPEN | bitsPARTICLE_TRAIL_FADE );

	// Add particles
	int iNumParticles = 60;
	for ( int i = 0; i < iNumParticles; i++ )
	{
		Vector vOffset = RandomVector( -1, 1 );
		VectorNormalize( vOffset );
		float flDistance = RandomFloat( 16, 64 ) * flScale;
		Vector vPos = vecOrigin + (vOffset * flDistance);
		
		TrailParticle *pParticle = (TrailParticle *) pEmitter->AddParticle( sizeof(TrailParticle), hParticleMaterial, vPos );
		if ( pParticle )
		{
			float flSpeed = RandomFloat(8,16) * (flScale * flScale * flScale);
			pParticle->m_vecVelocity = vOffset * -flSpeed;
			pParticle->m_flDieTime = MIN( 3, (flDistance / flSpeed) + RandomFloat(0.0, 0.2) );
			pParticle->m_flLifetime = 0;
			pParticle->m_flWidth = RandomFloat( 2, 3 ) * flScale;
			pParticle->m_flLength = RandomFloat( 1, 2 ) * flScale;

			Color32Init( pParticle->m_color, 255, 255, 255, 255 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Warp effect that looks like it's sucking things to it
//-----------------------------------------------------------------------------
void BuildWarpSuckCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;

	FX_BuildWarpSuck( vecOrigin, vecAngles, 1.0 );
}

DECLARE_CLIENT_EFFECT( "BuildWarpSuck", BuildWarpSuckCallback );

//-----------------------------------------------------------------------------
// Purpose: Bigger Warp effect that looks like it's sucking things to it
//-----------------------------------------------------------------------------
void BuildWarpSuckBigCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;

	FX_BuildWarpSuck( vecOrigin, vecAngles, 2.0 );
}

DECLARE_CLIENT_EFFECT( "BuildWarpSuckBig", BuildWarpSuckBigCallback );

//-----------------------------------------------------------------------------
// Purpose: GasSpurt Emitter
//			This is an emitter that keeps spitting out particles for its lifetime
//			It won't create particles if it's not in view, so it's best when short lived
//-----------------------------------------------------------------------------
class CGasSpurtEmitter : public CSimpleEmitter
{
	typedef CSimpleEmitter BaseClass;
public:
	CGasSpurtEmitter( const char *pDebugName ) : CSimpleEmitter( pDebugName ) 
	{
		m_flDeathTime = 0;
		m_flLastParticleSpawnTime = 0;
	}
	
	// Create
	static CGasSpurtEmitter *Create( const char *pDebugName="gasspurt" )
	{
		return new CGasSpurtEmitter( pDebugName );
	}

	void SetLifeTime( float flTime )
	{
		m_flDeathTime = gpGlobals->curtime + flTime;
	}

	void SetSpurtAngle( QAngle &vecAngles )
	{
		AngleVectors( vecAngles, &m_vecSpurtForward );
	}

	void SetSpurtColor( const Vector4D &pColor )
	{
		for ( int i = 0; i <= 3; i++ )
		{
			m_SpurtColor[i] = pColor[i];
		}
	}

	void SetSpawnRate( float flRate )
	{
		m_flSpawnRate = flRate;
	}

	void CreateSpurtParticles( void )
	{
		SimpleParticle *pParticle;

		// Smoke
		int numParticles = RandomInt( 1,2 );
		for ( int i = 0; i < numParticles; i++ )
		{
			pParticle = (SimpleParticle *) AddParticle( sizeof( SimpleParticle ), g_Mat_DustPuff[0], m_vSortOrigin );			
			if ( pParticle == NULL )
				break;

			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime = RandomFloat( 0.5, 1.0 );

			// Random velocity around the angles forward
			Vector vecVelocity;
			vecVelocity.Random( -0.1f, 0.1f );
			vecVelocity += m_vecSpurtForward;
			VectorNormalize( vecVelocity );
			vecVelocity	*= RandomFloat( 16.0f, 64.0f );
			pParticle->m_vecVelocity = vecVelocity;

			// Randomize the color a little
			int color[3][2];
			for( int i = 0; i < 3; ++i )
			{
				color[i][0] = MAX( 0, m_SpurtColor[i] - 64 );
				color[i][1] = MIN( 255, m_SpurtColor[i] + 64 );
			}
			pParticle->m_uchColor[0] = random->RandomInt( color[0][0], color[0][1] );
			pParticle->m_uchColor[1] = random->RandomInt( color[1][0], color[1][1] );
			pParticle->m_uchColor[2] = random->RandomInt( color[2][0], color[2][1] );

			pParticle->m_uchStartAlpha = m_SpurtColor[3];
			pParticle->m_uchEndAlpha = 0;
			pParticle->m_uchStartSize = RandomInt( 1, 2 );
			pParticle->m_uchEndSize = pParticle->m_uchStartSize*3;
			pParticle->m_flRoll	= RandomFloat( 0, 360 );
			pParticle->m_flRollDelta = RandomFloat( -4.0f, 4.0f );
		}			

		m_flLastParticleSpawnTime = gpGlobals->curtime + m_flSpawnRate;
	}

	virtual void SimulateParticles( CParticleSimulateIterator *pIterator )
	{
		Particle *pParticle = (Particle*)pIterator->GetFirst();
		while ( pParticle )
		{
			// If our lifetime isn't up, create more particles
			if ( m_flDeathTime > gpGlobals->curtime )
			{
				if ( m_flLastParticleSpawnTime <= gpGlobals->curtime )
				{
					CreateSpurtParticles();
				}
			}

			pParticle = (Particle*)pIterator->GetNext();
		}

		BaseClass::SimulateParticles( pIterator );
	}


private:
	float		m_flDeathTime;
	float		m_flLastParticleSpawnTime;
	float		m_flSpawnRate;
	Vector		m_vecSpurtForward;
	Vector4D	m_SpurtColor;

	CGasSpurtEmitter( const CGasSpurtEmitter & ); // not defined, not accessible
};

//-----------------------------------------------------------------------------
// Purpose: Small hose gas spurt
//-----------------------------------------------------------------------------
void FX_BuildGasSpurt( Vector &vecOrigin, QAngle &vecAngles, float flLifeTime, const Vector4D &pColor )
{
	CSmartPtr<CGasSpurtEmitter> pSimple = CGasSpurtEmitter::Create( "FX_Smoke" );
	pSimple->SetSortOrigin( vecOrigin );
	pSimple->SetLifeTime( flLifeTime );
	pSimple->SetSpurtAngle( vecAngles );
	pSimple->SetSpurtColor( pColor );
	pSimple->SetSpawnRate( 0.03 );
	pSimple->CreateSpurtParticles();
}

//-----------------------------------------------------------------------------
// Purpose: Green hose gas spurt
//-----------------------------------------------------------------------------
void GasGreenCallback( const CEffectData &data )
{
	Vector vecOrigin = data.m_vOrigin;
	QAngle vecAngles = data.m_vAngles;

	Vector4D color( 50,192,50,255 );
	FX_BuildGasSpurt( vecOrigin, vecAngles, 1.0, color );
}

DECLARE_CLIENT_EFFECT( "GasGreen", GasGreenCallback );
