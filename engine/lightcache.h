//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LIGHTCACHE_H
#define LIGHTCACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"

#define MAXLOCALLIGHTS 4

class IHandleEntity;
struct dworldlight_t;												   
FORWARD_DECLARE_HANDLE( LightCacheHandle_t ); 

struct LightingState_t;

typedef Vector AmbientCube_t[6];

struct LightingState_t
{
	Vector		r_boxcolor[6];		// ambient, and lights that aren't in locallight[]
	int			numlights;
	dworldlight_t *locallight[MAXLOCALLIGHTS];
	void ZeroLightingState( void )
	{
		int i;
		for( i = 0; i < 6; i++ )
		{
			r_boxcolor[i].Init();
		}
		numlights = 0;
	}

	bool HasAmbientColors()
	{
		for ( int i = 0; i < 6; i++ )
		{
			if ( !r_boxcolor[i].IsZero(1e-4f) )
				return true;
		}
		return false;
	}

	void AddLocalLight( dworldlight_t *pLocalLight )
	{
		if ( numlights >= MAXLOCALLIGHTS )
			return;
		for ( int i = 0; i < numlights; i++ )
		{
			if ( locallight[i] == pLocalLight )
				return;
		}
		locallight[numlights] = pLocalLight;
		numlights++;
	}

	void AddAllLocalLights( const LightingState_t &src )
	{
		for ( int i = 0; i < src.numlights; i++ )
		{
			AddLocalLight( src.locallight[i] );
		}
	}

	void CopyLocalLights( const LightingState_t &src )
	{
		numlights = src.numlights;
		for ( int i = 0; i < src.numlights; i++ )
		{
			locallight[i] = src.locallight[i];
		}
	}

	LightingState_t()
	{
		ZeroLightingState();
	}
};

enum
{
	LIGHTCACHEFLAGS_STATIC						= 0x1,
	LIGHTCACHEFLAGS_DYNAMIC						= 0x2,
	LIGHTCACHEFLAGS_LIGHTSTYLE					= 0x4,
	LIGHTCACHEFLAGS_ALLOWFAST					= 0x8,
};

class ITexture;


// Called each frame to check for reinitializing the lightcache based on cvar changes.
void R_StudioCheckReinitLightingCache();


// static prop version
LightingState_t *LightcacheGetStatic( LightCacheHandle_t cache, ITexture **pEnvCubemap,
				    unsigned int flags = ( LIGHTCACHEFLAGS_STATIC | 
						                   LIGHTCACHEFLAGS_DYNAMIC | 
					                       LIGHTCACHEFLAGS_LIGHTSTYLE ) );
// dynamic prop version
struct LightcacheGetDynamic_Stats
{
	bool m_bHasNonSwitchableLightStyles;
	bool m_bHasSwitchableLightStyles;
	bool m_bHasDLights;
	bool m_bNeedsSwitchableLightStyleUpdate;
};
ITexture *LightcacheGetDynamic( const Vector& origin, LightingState_t& lightingState, 
							   LightcacheGetDynamic_Stats &stats,
							   unsigned int flags = ( LIGHTCACHEFLAGS_STATIC | 
						                              LIGHTCACHEFLAGS_DYNAMIC | 
					                                  LIGHTCACHEFLAGS_LIGHTSTYLE ), bool bDebugModel=false );

// Reset the light cache.
void R_StudioInitLightingCache( void );

// force recomputation for static lighting cache entries
void InvalidateStaticLightingCache(void);

// Compute the comtribution of D- and E- lights at a point + normal
void ComputeDynamicLighting( const Vector& pt, const Vector* pNormal, Vector& color );

// Computes an average color (of sorts) at a particular point + optional normal
void ComputeLighting( const Vector& pt, const Vector* pNormal, bool bClamp, Vector& color, Vector *pBoxColors );

// Finds ambient lights
dworldlight_t* FindAmbientLight();

// Precache lighting
LightCacheHandle_t CreateStaticLightingCache( const Vector& origin, const Vector& mins, const Vector& maxs );
void ClearStaticLightingCache();

// Computes the static vertex lighting term from a large number of spherical samples
bool ComputeVertexLightingFromSphericalSamples( const Vector& vecVertex, 
	const Vector &vecNormal, IHandleEntity *pIgnoreEnt, Vector *pLinearColor );

bool StaticLightCacheAffectedByDynamicLight( LightCacheHandle_t handle );
bool StaticLightCacheAffectedByAnimatedLightStyle( LightCacheHandle_t handle );
bool StaticLightCacheNeedsSwitchableLightUpdate( LightCacheHandle_t handle );

//-----------------------------------------------------------------------------
// Adds a world light to the ambient cube
//-----------------------------------------------------------------------------
void AddWorldLightToAmbientCube( dworldlight_t* pWorldLight, const Vector &vecLightingOrigin, AmbientCube_t &ambientCube );

void InitDLightGlobals( int nMapVersion );

// This is different for shipped HL2. . . 
extern float g_flMinLightingValue;

#endif // LIGHTCACHE_H
