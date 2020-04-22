//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//


#include "render_pch.h"
#include "gl_matsysiface.h"
#include "gl_cvars.h"
#include "enginetrace.h"
#include "r_local.h"
#include "gl_model_private.h"
#include "materialsystem/imesh.h"
#include "cdll_engine_int.h"
#include "cl_main.h"
#include "debugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar r_drawlights(  "r_drawlights", "0", FCVAR_CHEAT );
static ConVar r_drawlightinfo(  "r_drawlightinfo", "0", FCVAR_CHEAT );

static bool s_bActivateLightSprites = false;

//-----------------------------------------------------------------------------
// Should we draw light sprites over visible lights?
//-----------------------------------------------------------------------------
bool ActivateLightSprites( bool bActive )
{
	bool bOldValue = s_bActivateLightSprites;
	s_bActivateLightSprites = bActive;
	return bOldValue;
}


#define LIGHT_MIN_LIGHT_VALUE 0.03f

float ComputeLightRadius( dworldlight_t *pLight, bool bIsHDR )
{
	float flLightRadius = pLight->radius;
	if (flLightRadius == 0.0f)
	{
		// HACKHACK: Usually our designers scale the light intensity by 0.5 in HDR
		// This keeps the behavior of the cutoff radius consistent between LDR and HDR
		float minLightValue = bIsHDR ? (LIGHT_MIN_LIGHT_VALUE * 0.5f) : LIGHT_MIN_LIGHT_VALUE;

		// Compute the light range based on attenuation factors
		float flIntensity = sqrtf( DotProduct( pLight->intensity, pLight->intensity ) );
		if (pLight->quadratic_attn == 0.0f)
		{
			if (pLight->linear_attn == 0.0f)
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (flIntensity / minLightValue - pLight->constant_attn) / pLight->linear_attn;
			}
		}
		else
		{
			float a = pLight->quadratic_attn;
			float b = pLight->linear_attn;
			float c = pLight->constant_attn - flIntensity / minLightValue;
			float discrim = b * b - 4 * a * c;
			if (discrim < 0.0f)
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (-b + sqrtf(discrim)) / (2.0f * a);
				if (flLightRadius < 0)
					flLightRadius = 0;
			}
		}
	}

	return flLightRadius;
}


static void DrawLightSprite( dworldlight_t *pLight, float angleAttenFactor )
{
	Vector lightToEye;
	lightToEye = CurrentViewOrigin() - pLight->origin;
	VectorNormalize( lightToEye );
	Vector up( 0.0f, 0.0f, 1.0f );
	Vector right;
	CrossProduct( up, lightToEye, right );
	VectorNormalize( right );
	CrossProduct( lightToEye, right, up );
	VectorNormalize( up );

/*
	up *= dist;
	right *= dist;

	up *= ( 1.0f / 5.0f );
	right *= ( 1.0f / 5.0f );

	up *= 1.0f / sqrt( pLight->constant_attn + dist * pLight->linear_attn + dist * dist * pLight->quadratic_attn );
	right *= 1.0f / sqrt( pLight->constant_attn + dist * pLight->linear_attn + dist * dist * pLight->quadratic_attn );
*/

	//	float distFactor = 1.0f / ( pLight->constant_attn + dist * pLight->linear_attn + dist * dist * pLight->quadratic_attn );
	//float distFactor = 1.0f;
	
	Vector color = pLight->intensity;
	VectorNormalize( color );
	color *= angleAttenFactor;

	color[0] = pow( color[0], 1.0f / 2.2f );
	color[1] = pow( color[1], 1.0f / 2.2f );
	color[2] = pow( color[2], 1.0f / 2.2f );
	
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( g_pMaterialLightSprite );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	float radius = 16.0f;
	Vector p;
	
	ColorClamp( color );
	
	p = pLight->origin + right * radius + up * radius;
	meshBuilder.TexCoord2f( 0, 1.0f, 1.0f );
	meshBuilder.Color3fv( color.Base() );
	meshBuilder.Position3fv( p.Base() );
	meshBuilder.AdvanceVertex();

	p = pLight->origin + right * -radius + up * radius;
	meshBuilder.TexCoord2f( 0, 0.0f, 1.0f );
	meshBuilder.Color3fv( color.Base() );
	meshBuilder.Position3fv( p.Base() );
	meshBuilder.AdvanceVertex();

	p = pLight->origin + right * -radius + up * -radius;
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.Color3fv( color.Base() );
	meshBuilder.Position3fv( p.Base() );
	meshBuilder.AdvanceVertex();

	p = pLight->origin + right * radius + up * -radius;
	meshBuilder.TexCoord2f( 0, 1.0f, 0.0f );
	meshBuilder.Color3fv( color.Base() );
	meshBuilder.Position3fv( p.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

#define POINT_THETA_GRID 8
#define POINT_PHI_GRID 8

static void DrawPointLight( const Vector &vecOrigin, float flLightRadius )
{
	int nVertCount = POINT_THETA_GRID * (POINT_PHI_GRID + 1);
	int nIndexCount = 8 * POINT_THETA_GRID * POINT_PHI_GRID;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( g_materialWorldWireframeZBuffer );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, nVertCount, nIndexCount );

	float dTheta = 360.0f / POINT_THETA_GRID;
	float dPhi = 180.0f / POINT_PHI_GRID;

	Vector pt;
	int i;
	float flPhi = 0;
	for ( i = 0; i <= POINT_PHI_GRID; ++i )
	{
		float flSinPhi = sin(DEG2RAD(flPhi));
		float flCosPhi = cos(DEG2RAD(flPhi));
		float flTheta = 0;
		for ( int j = 0; j < POINT_THETA_GRID; ++j )
		{
			pt = vecOrigin;
			pt.x += flLightRadius * cos(DEG2RAD(flTheta)) * flSinPhi;
			pt.y += flLightRadius * sin(DEG2RAD(flTheta)) * flSinPhi;
			pt.z += flLightRadius * flCosPhi;

			meshBuilder.Position3fv( pt.Base() );
			meshBuilder.AdvanceVertex();

			flTheta += dTheta;
		}

		flPhi += dPhi;
	}

	for ( i = 0; i < POINT_THETA_GRID; ++i )
	{
		for ( int j = 0; j < POINT_PHI_GRID; ++j )
		{
			int nNextIndex = (j != POINT_PHI_GRID - 1) ? j + 1 : 0;

			meshBuilder.Index( i * POINT_PHI_GRID + j );
			meshBuilder.AdvanceIndex();
			meshBuilder.Index( (i + 1) * POINT_PHI_GRID + j );
			meshBuilder.AdvanceIndex();

			meshBuilder.Index( (i + 1) * POINT_PHI_GRID + j );
			meshBuilder.AdvanceIndex();
			meshBuilder.Index( (i + 1) * POINT_PHI_GRID + nNextIndex );
			meshBuilder.AdvanceIndex();

			meshBuilder.Index( (i + 1) * POINT_PHI_GRID + nNextIndex );
			meshBuilder.AdvanceIndex();
			meshBuilder.Index( i * POINT_PHI_GRID + nNextIndex );
			meshBuilder.AdvanceIndex();

			meshBuilder.Index( i * POINT_PHI_GRID + nNextIndex );
			meshBuilder.AdvanceIndex();
			meshBuilder.Index( i * POINT_PHI_GRID + j );
			meshBuilder.AdvanceIndex();
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Draws the spot light
//-----------------------------------------------------------------------------
#define SPOT_GRID_LINE_COUNT 20
#define SPOT_GRID_LINE_DISTANCE 50
#define SPOT_RADIAL_GRID 8

void DrawSpotLight( dworldlight_t *pLight )
{
	float flLightRadius = ComputeLightRadius( pLight, false );

	int nGridLines = (int)(flLightRadius / SPOT_GRID_LINE_DISTANCE) + 1;
	int nVertCount = SPOT_RADIAL_GRID * (nGridLines + 1);
	int nIndexCount = 8 * SPOT_RADIAL_GRID * nGridLines;

	// Compute a basis perpendicular to the normal
	Vector xaxis, yaxis;
	int nMinIndex = fabs(pLight->normal[0]) < fabs(pLight->normal[1]) ? 0 : 1;
	nMinIndex = fabs(pLight->normal[nMinIndex]) < fabs(pLight->normal[2]) ? nMinIndex : 2;
	Vector perp = vec3_origin;
	perp[nMinIndex] = 1.0f;
	CrossProduct( perp, pLight->normal, xaxis );
	VectorNormalize( xaxis );
	CrossProduct( pLight->normal, xaxis, yaxis ); 

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->Bind( g_materialWorldWireframeZBuffer );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, nVertCount, nIndexCount );

	float flAngle = acos(pLight->stopdot2);
	float flTanAngle = tan(flAngle);
	float dTheta = 360.0f / SPOT_RADIAL_GRID;
	float flDist = 0.0f;

	int i;
	for ( i = 0; i <= nGridLines; ++i )
	{
		Vector pt, vecCenter;
		VectorMA( pLight->origin, flDist, pLight->normal, vecCenter );
		
		float flRadius = flDist * flTanAngle;

		float flTempAngle = 0;
		for ( int j = 0; j < SPOT_RADIAL_GRID; ++j )
		{
			float flSin = sin( DEG2RAD( flTempAngle ) );
			float flCos = cos( DEG2RAD( flTempAngle ) );
			VectorMA( vecCenter, flRadius * flCos, xaxis, pt );
			VectorMA( pt, flRadius * flSin, yaxis, pt );

			meshBuilder.Position3fv( pt.Base() );
			meshBuilder.AdvanceVertex();

			flTempAngle += dTheta;
		}

		flDist += SPOT_GRID_LINE_DISTANCE;
	}

	for ( i = 0; i < nGridLines; ++i )
	{
		for ( int j = 0; j < SPOT_RADIAL_GRID; ++j )
		{
			int nNextIndex = (j != SPOT_RADIAL_GRID - 1) ? j + 1 : 0;

			meshBuilder.Index( i * SPOT_RADIAL_GRID + j );
			meshBuilder.AdvanceIndex();
			meshBuilder.Index( (i + 1) * SPOT_RADIAL_GRID + j );
			meshBuilder.AdvanceIndex();

			meshBuilder.Index( (i + 1) * SPOT_RADIAL_GRID + j );
			meshBuilder.AdvanceIndex();
			meshBuilder.Index( (i + 1) * SPOT_RADIAL_GRID + nNextIndex );
			meshBuilder.AdvanceIndex();

			meshBuilder.Index( (i + 1) * SPOT_RADIAL_GRID + nNextIndex );
			meshBuilder.AdvanceIndex();
			meshBuilder.Index( i * SPOT_RADIAL_GRID + nNextIndex );
			meshBuilder.AdvanceIndex();

			meshBuilder.Index( i * SPOT_RADIAL_GRID + nNextIndex );
			meshBuilder.AdvanceIndex();
			meshBuilder.Index( i * SPOT_RADIAL_GRID + j );
			meshBuilder.AdvanceIndex();
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Draws sprites over all visible lights
// NOTE: This is used to render env-cubemaps
//-----------------------------------------------------------------------------
void DrawLightSprites( void )
{
	if (!s_bActivateLightSprites)
		return;

	int i;	
	for (i = 0; i < host_state.worldbrush->numworldlights; i++)
	{
		dworldlight_t *pLight = &host_state.worldbrush->worldlights[i];
		trace_t tr;
		CTraceFilterWorldAndPropsOnly traceFilter;
		Ray_t ray;
		ray.Init( CurrentViewOrigin(), pLight->origin );
		g_pEngineTraceClient->TraceRay( ray, MASK_OPAQUE, &traceFilter, &tr );
		if( tr.fraction < 1.0f )
			continue;

		float angleAttenFactor = 0.0f;
		Vector lightToEye;
		lightToEye = CurrentViewOrigin() - pLight->origin;
		VectorNormalize( lightToEye );
		switch( pLight->type )
		{
		case emit_point:
			angleAttenFactor = 1.0f;
			break;
		case emit_spotlight:
			continue;
			break;
		case emit_surface:
			// garymcthack - don't do surface lights
			continue;
			if( DotProduct( lightToEye, pLight->normal ) < 0.0f )
			{
				continue;
			}
			angleAttenFactor = 1.0f;
			break;
		case emit_skylight:
		case emit_skyambient:
			continue;
		default:
			assert( 0 );
			continue;
		}
		DrawLightSprite( pLight, angleAttenFactor );
	}
}


//-----------------------------------------------------------------------------
// Draws debugging information for the lights
//-----------------------------------------------------------------------------
void DrawLightDebuggingInfo( void )
{
	int		i;
	char	buf[256];
	int		lineOffset;

	int nLight = r_drawlights.GetInt();

	if ( r_drawlightinfo.GetBool() )
	{
		for (i = 0; i < host_state.worldbrush->numworldlights; i++)
		{	
			dworldlight_t *pLight = &host_state.worldbrush->worldlights[i];

			lineOffset = 0;
			Q_snprintf( buf, sizeof( buf ), "light:  %d\n", i+1 );
			CDebugOverlay::AddTextOverlay( pLight->origin, lineOffset++, 0, buf );	
			Q_snprintf( buf, sizeof( buf ), "origin: <%d, %d, %d>\n", (int)pLight->origin[0], (int)pLight->origin[1], (int)pLight->origin[2] );
			CDebugOverlay::AddTextOverlay( pLight->origin, lineOffset++, 0, buf );	

			if (!nLight)
			{
				// avoid a double debug draw
				DrawLightSprite( pLight, 1.0f );
			}
		}
	}

	if (!nLight)
		return;

	for (i = 0; i < host_state.worldbrush->numworldlights; i++)
	{
		if ((nLight > 0) && (i != nLight-1))
			continue;

		dworldlight_t *pLight = &host_state.worldbrush->worldlights[i];
		Vector lightToEye;
		float angleAttenFactor = 0.0f;
		switch( pLight->type )
		{
		case emit_point:
			angleAttenFactor = 1.0f;
			DrawPointLight( pLight->origin, ComputeLightRadius( pLight, false ) );
			break;
		case emit_spotlight:
			angleAttenFactor = 1.0f;
			DrawSpotLight( pLight );
			break;
		case emit_surface:
			// garymcthack - don't do surface lights
			continue;
			lightToEye = CurrentViewOrigin() - pLight->origin;
			VectorNormalize( lightToEye );
			if( DotProduct( lightToEye, pLight->normal ) < 0.0f )
			{
				continue;
			}
			angleAttenFactor = 1.0f;
			break;
		case emit_skylight:
		case emit_skyambient:
			continue;
		default:
			assert( 0 );
			continue;
		}
		DrawLightSprite( pLight, angleAttenFactor );
	}

	int	lnum;
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		// If the light's not active, then continue
		if ( (r_dlightactive & (1 << lnum)) == 0 )
			continue;

		DrawPointLight( cl_dlights[lnum].origin, cl_dlights[lnum].GetRadius() );
	}
}
