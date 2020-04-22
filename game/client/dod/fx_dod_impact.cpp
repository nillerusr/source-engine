//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//
#include "cbase.h"
#include "fx_impact.h"
#include "engine/IEngineSound.h"

#include "decals.h"
#include "IEffects.h"
#include "fx.h"
#include "c_impact_effects.h"
#include "fx_fleck.h"

//-----------------------------------------------------------------------------
// This does the actual debris flecks
//-----------------------------------------------------------------------------
#define	FLECK_MIN_SPEED		64.0f
#define	FLECK_MAX_SPEED		128.0f
#define	FLECK_GRAVITY		800.0f
#define	FLECK_DAMPEN		0.3f
#define	FLECK_ANGULAR_SPRAY	0.6f

static void FX_Flecks( const Vector& origin, trace_t *trace, char materialType, int iScale )
{
	Vector	color;
	GetColorForSurface( trace, &color );

	Vector	spawnOffset	= trace->endpos + ( trace->plane.normal * 1.0f );

	CSmartPtr<CFleckParticles> fleckEmitter = CFleckParticles::Create( "FX_DebrisFlecks", spawnOffset, Vector(5,5,5) );

	if ( !fleckEmitter )
		return;

	// Handle increased scale
	float flMaxSpeed = FLECK_MAX_SPEED * iScale;
	float flAngularSpray = MAX( 0.2, FLECK_ANGULAR_SPRAY - ( (float)iScale * 0.2f) ); // More power makes the spray more controlled

	// Setup our collision information
	fleckEmitter->m_ParticleCollision.Setup( spawnOffset, &trace->plane.normal, flAngularSpray, FLECK_MIN_SPEED, flMaxSpeed, FLECK_GRAVITY, FLECK_DAMPEN );

	PMaterialHandle	*hMaterial;

	switch ( materialType )
	{
	case CHAR_TEX_WOOD:
		hMaterial = g_Mat_Fleck_Wood;
		break;

	case CHAR_TEX_CONCRETE:
	case CHAR_TEX_TILE:
	default:
		hMaterial = g_Mat_Fleck_Cement;
		break;
	}

	Vector	dir, end;

	float	colorRamp;

	int	numFlecks = random->RandomInt( 4, 16 ) * iScale;

	FleckParticle	*pFleckParticle;

	//Dump out flecks
	int i;
	for ( i = 0; i < numFlecks; i++ )
	{
		pFleckParticle = (FleckParticle *) fleckEmitter->AddParticle( sizeof(FleckParticle), hMaterial[random->RandomInt(0,1)], spawnOffset );

		if ( pFleckParticle == NULL )
			break;

		pFleckParticle->m_flLifetime	= 0.0f;
		pFleckParticle->m_flDieTime		= 3.0f;

		dir[0] = trace->plane.normal[0] + random->RandomFloat( -flAngularSpray, flAngularSpray );
		dir[1] = trace->plane.normal[1] + random->RandomFloat( -flAngularSpray, flAngularSpray );
		dir[2] = trace->plane.normal[2] + random->RandomFloat( -flAngularSpray, flAngularSpray );

		pFleckParticle->m_uchSize		= random->RandomInt( 1, 2 );

		pFleckParticle->m_vecVelocity	= dir * ( random->RandomFloat( FLECK_MIN_SPEED, flMaxSpeed) * ( 3 - pFleckParticle->m_uchSize ) );

		pFleckParticle->m_flRoll		= random->RandomFloat( 0, 360 );
		pFleckParticle->m_flRollDelta	= random->RandomFloat( 0, 360 );

		colorRamp = random->RandomFloat( 0.75f, 1.25f );

		pFleckParticle->m_uchColor[0] = MIN( 1.0f, color[0]*colorRamp )*255.0f;
		pFleckParticle->m_uchColor[1] = MIN( 1.0f, color[1]*colorRamp )*255.0f;
		pFleckParticle->m_uchColor[2] = MIN( 1.0f, color[2]*colorRamp )*255.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Perform custom effects based on the Decal index
//-----------------------------------------------------------------------------
void DOD_PerformCustomEffects( const Vector &vecOrigin, trace_t &tr, const Vector &shotDir, int iMaterial, float flScale )
{
	// Throw out the effect if any of these are true
	if ( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) )
		return;

	int iScale = (int)flScale;
	if ( iScale < 1 )
		iScale = 1;

	// Cement and wood have dust and flecks
	if ( ( iMaterial == CHAR_TEX_CONCRETE ) ||
		( iMaterial == CHAR_TEX_TILE ) ||
		( iMaterial == CHAR_TEX_WOOD ) )
	{
		FX_Flecks( vecOrigin, &tr, iMaterial, iScale );
		FX_DustImpact( vecOrigin, &tr, flScale );
	}
	else if ( ( iMaterial == CHAR_TEX_DIRT ) || ( iMaterial == CHAR_TEX_SAND ) )
	{
		FX_DustImpact( vecOrigin, &tr, flScale );
	}
	else if ( ( iMaterial == CHAR_TEX_METAL ) || ( iMaterial == CHAR_TEX_VENT ) )
	{
		Vector	reflect;
		float	dot = shotDir.Dot( tr.plane.normal );
		reflect = shotDir + ( tr.plane.normal * ( dot*-2.0f ) );

		reflect[0] += random->RandomFloat( -0.2f, 0.2f );
		reflect[1] += random->RandomFloat( -0.2f, 0.2f );
		reflect[2] += random->RandomFloat( -0.2f, 0.2f );

		FX_MetalSpark( vecOrigin, reflect, tr.plane.normal, iScale );
	}
	else if ( iMaterial == CHAR_TEX_COMPUTER )
	{
		Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );

		g_pEffects->Sparks( offset );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle weapon impacts
//-----------------------------------------------------------------------------
void ImpactCallback( const CEffectData &data )
{
	trace_t tr;
	Vector vecOrigin, vecStart, vecShotDir;
	int iMaterial, iDamageType, iHitbox;
	short nSurfaceProp;

	C_BaseEntity *pEntity = ParseImpactData( data, &vecOrigin, &vecStart, &vecShotDir, nSurfaceProp, iMaterial, iDamageType, iHitbox );

	if ( !pEntity )
		return;

	// If we hit, perform our custom effects and play the sound
	if ( Impact( vecOrigin, vecStart, iMaterial, iDamageType, iHitbox, pEntity, tr ) )
	{
		float flScale = 0.75f;
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

		if ( pPlayer )
		{
			float flDistSqr = (vecOrigin - pPlayer->GetAbsOrigin()).LengthSqr();

			flScale = RemapValClamped( flDistSqr, (400*400), (1000*1000), 0.8, 1.2 );
		}		

		// Check for custom effects based on the Decal index
		DOD_PerformCustomEffects( vecOrigin, tr, vecShotDir, iMaterial, flScale );
		PlayImpactSound( pEntity, tr, vecOrigin, nSurfaceProp );

		//Play a ricochet sound some of the time
		if( random->RandomInt(1,10) <= 3 && (iDamageType == DMG_BULLET) )
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Bounce.Shrapnel", &vecOrigin );
		}
	}
}

DECLARE_CLIENT_EFFECT( "Impact", ImpactCallback );
