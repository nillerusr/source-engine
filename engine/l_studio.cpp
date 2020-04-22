//========= Copyright Valve Corporation, All rights reserved. ============//
//
// models are the only shared resource between a client and server running
// on the same machine.
//===========================================================================//

#include "render_pch.h"
#include "client.h"
#include "gl_model_private.h"
#include "studio.h"
#include "phyfile.h"
#include "cdll_int.h"
#include "istudiorender.h"
#include "client_class.h"
#include "float.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/ivballoctracker.h"
#include "modelloader.h"
#include "lightcache.h"
#include "studio_internal.h"
#include "cdll_engine_int.h"
#include "vphysics_interface.h"
#include "utllinkedlist.h"
#include "studio.h"
#include "icliententitylist.h"
#include "engine/ivmodelrender.h"
#include "optimize.h"
#include "icliententity.h"
#include "sys_dll.h"
#include "debugoverlay.h"
#include "enginetrace.h"
#include "l_studio.h"
#include "filesystem_engine.h"
#include "ModelInfo.h"
#include "cl_main.h"
#include "tier0/vprof.h"
#include "r_decal.h"
#include "vstdlib/random.h"
#include "datacache/idatacache.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/itexture.h"
#include "IHammer.h"
#if defined( _WIN32 ) && !defined( _X360 )
#include <xmmintrin.h>
#endif
#include "staticpropmgr.h"
#include "materialsystem/hardwaretexels.h"
#include "materialsystem/hardwareverts.h"
#include "tier1/callqueue.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "tier1/UtlSortVector.h"
#include "tier1/lzmaDecoder.h"
#include "ipooledvballocator.h"
#include "shaderapi/ishaderapi.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// #define VISUALIZE_TIME_AVERAGE 1

extern ConVar r_flashlight_version2;

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void R_StudioInitLightingCache( void );
float Engine_WorldLightDistanceFalloff( const dworldlight_t *wl, const Vector& delta, bool bNoRadiusCheck = false );
void SetRootLOD_f( IConVar *var, const char *pOldString, float flOldValue );
void r_lod_f( IConVar *var, const char *pOldValue, float flOldValue );
void FlushLOD_f();

class CColorMeshData;
static void CreateLightmapsFromData(CColorMeshData* _colorMeshData);


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

ConVar r_drawmodelstatsoverlay( "r_drawmodelstatsoverlay", "0", FCVAR_CHEAT );
ConVar r_drawmodelstatsoverlaydistance( "r_drawmodelstatsoverlaydistance", "500", FCVAR_CHEAT );
ConVar r_drawmodellightorigin( "r_DrawModelLightOrigin", "0", FCVAR_CHEAT );
extern ConVar r_worldlights;
ConVar r_lod( "r_lod", "-1", 0, "", r_lod_f );
static ConVar r_entity( "r_entity", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConVar r_lightaverage( "r_lightaverage", "1", 0, "Activates/deactivate light averaging" );
static ConVar r_lightinterp( "r_lightinterp", "5", FCVAR_CHEAT, "Controls the speed of light interpolation, 0 turns off interpolation" );
static ConVar r_eyeglintlodpixels( "r_eyeglintlodpixels", "20.0", FCVAR_CHEAT, "The number of pixels wide an eyeball has to be before rendering an eyeglint.  Is a floating point value." );
ConVar r_rootlod( "r_rootlod", "0", FCVAR_MATERIAL_SYSTEM_THREAD | FCVAR_ARCHIVE, "Root LOD", true, 0, true, MAX_NUM_LODS-1, SetRootLOD_f );
static ConVar r_decalstaticprops( "r_decalstaticprops", "1", 0, "Decal static props test" );
static ConCommand r_flushlod( "r_flushlod", FlushLOD_f, "Flush and reload LODs." );
ConVar r_debugrandomstaticlighting( "r_debugrandomstaticlighting", "0", FCVAR_CHEAT, "Set to 1 to randomize static lighting for debugging.  Must restart for change to take affect." );
ConVar r_proplightingfromdisk( "r_proplightingfromdisk", "1", FCVAR_CHEAT, "0=Off, 1=On, 2=Show Errors" );
static ConVar r_itemblinkmax( "r_itemblinkmax", ".3", FCVAR_CHEAT );
static ConVar r_itemblinkrate( "r_itemblinkrate", "4.5", FCVAR_CHEAT );
static ConVar r_proplightingpooling( "r_proplightingpooling", "-1.0", FCVAR_CHEAT, "0 - off, 1 - static prop color meshes are allocated from a single shared vertex buffer (on hardware that supports stream offset)" );

//-----------------------------------------------------------------------------
// StudioRender config 
//-----------------------------------------------------------------------------
static ConVar	r_showenvcubemap( "r_showenvcubemap", "0", FCVAR_CHEAT );
static ConVar	r_eyemove		( "r_eyemove", "1", FCVAR_ARCHIVE ); // look around
static ConVar	r_eyeshift_x	( "r_eyeshift_x", "0", FCVAR_ARCHIVE ); // eye X position
static ConVar	r_eyeshift_y	( "r_eyeshift_y", "0", FCVAR_ARCHIVE ); // eye Y position
static ConVar	r_eyeshift_z	( "r_eyeshift_z", "0", FCVAR_ARCHIVE ); // eye Z position
static ConVar	r_eyesize		( "r_eyesize", "0", FCVAR_ARCHIVE ); // adjustment to iris textures
static ConVar	mat_softwareskin( "mat_softwareskin", "0", FCVAR_CHEAT );
static ConVar	r_nohw			( "r_nohw", "0", FCVAR_CHEAT );
static ConVar	r_nosw			( "r_nosw", "0", FCVAR_CHEAT );
static ConVar	r_teeth			( "r_teeth", "1" );
static ConVar	r_drawentities	( "r_drawentities", "1", FCVAR_CHEAT );
static ConVar	r_flex			( "r_flex", "1" );
static ConVar	r_eyes			( "r_eyes", "1" );
static ConVar	r_skin			( "r_skin", "0", FCVAR_CHEAT );
static ConVar	r_modelwireframedecal ( "r_modelwireframedecal", "0", FCVAR_CHEAT );
static ConVar	r_maxmodeldecal ( "r_maxmodeldecal", "50" );

static StudioRenderConfig_t s_StudioRenderConfig;

void UpdateStudioRenderConfig( void )
{
	// This can happen during initialization
	if ( !g_pMaterialSystemConfig || !g_pStudioRender )
		return;

	memset( &s_StudioRenderConfig, 0, sizeof(s_StudioRenderConfig) );

	s_StudioRenderConfig.bEyeMove = !!r_eyemove.GetInt();
	s_StudioRenderConfig.fEyeShiftX = r_eyeshift_x.GetFloat();
	s_StudioRenderConfig.fEyeShiftY = r_eyeshift_y.GetFloat();
	s_StudioRenderConfig.fEyeShiftZ = r_eyeshift_z.GetFloat();
	s_StudioRenderConfig.fEyeSize = r_eyesize.GetFloat();	
	if ( IsPC() && ( mat_softwareskin.GetInt() || ShouldDrawInWireFrameMode() ) )
	{
		s_StudioRenderConfig.bSoftwareSkin = true;
	}
	else
	{
		s_StudioRenderConfig.bSoftwareSkin = false;
	}
	s_StudioRenderConfig.bNoHardware = !!r_nohw.GetInt();
	s_StudioRenderConfig.bNoSoftware = !!r_nosw.GetInt();
	s_StudioRenderConfig.bTeeth = !!r_teeth.GetInt();
	s_StudioRenderConfig.drawEntities = r_drawentities.GetInt();
	s_StudioRenderConfig.bFlex = !!r_flex.GetInt();
	s_StudioRenderConfig.bEyes = !!r_eyes.GetInt();
	s_StudioRenderConfig.bWireframe = ShouldDrawInWireFrameMode();
	s_StudioRenderConfig.bDrawNormals = mat_normals.GetBool();
	s_StudioRenderConfig.skin = r_skin.GetInt();
	s_StudioRenderConfig.maxDecalsPerModel = r_maxmodeldecal.GetInt();
	s_StudioRenderConfig.bWireframeDecals = r_modelwireframedecal.GetInt() != 0;
	
	s_StudioRenderConfig.fullbright = g_pMaterialSystemConfig->nFullbright;
	s_StudioRenderConfig.bSoftwareLighting = g_pMaterialSystemConfig->bSoftwareLighting;

	s_StudioRenderConfig.bShowEnvCubemapOnly = r_showenvcubemap.GetInt() ? true : false;
	s_StudioRenderConfig.fEyeGlintPixelWidthLODThreshold = r_eyeglintlodpixels.GetFloat();

	g_pStudioRender->UpdateConfig( s_StudioRenderConfig );
}

void R_InitStudio( void )
{
#ifndef SWDS
	R_StudioInitLightingCache();
#endif
}

//-----------------------------------------------------------------------------
// Converts world lights to materialsystem lights
//-----------------------------------------------------------------------------

#define MIN_LIGHT_VALUE 0.03f

bool WorldLightToMaterialLight( dworldlight_t* pWorldLight, LightDesc_t& light )
{
	// BAD
	light.m_Attenuation0 = 0.0f;
	light.m_Attenuation1 = 0.0f;
	light.m_Attenuation2 = 0.0f;

	switch(pWorldLight->type)
	{
	case emit_spotlight:
		light.m_Type = MATERIAL_LIGHT_SPOT;
		light.m_Attenuation0 = pWorldLight->constant_attn;
		light.m_Attenuation1 = pWorldLight->linear_attn;
		light.m_Attenuation2 = pWorldLight->quadratic_attn;
		light.m_Theta = 2.0 * acos( pWorldLight->stopdot );
		light.m_Phi = 2.0 * acos( pWorldLight->stopdot2 );
		light.m_ThetaDot = pWorldLight->stopdot;
		light.m_PhiDot = pWorldLight->stopdot2;
		light.m_Falloff = pWorldLight->exponent ? pWorldLight->exponent : 1.0f;
		break;

	case emit_surface:
		// A 180 degree spotlight
		light.m_Type = MATERIAL_LIGHT_SPOT;
		light.m_Attenuation2 = 1.0;
		light.m_Theta = M_PI;
		light.m_Phi = M_PI;
		light.m_ThetaDot = 0.0f;
		light.m_PhiDot = 0.0f;
		light.m_Falloff = 1.0f;
		break;

	case emit_point:
		light.m_Type = MATERIAL_LIGHT_POINT;
		light.m_Attenuation0 = pWorldLight->constant_attn;
		light.m_Attenuation1 = pWorldLight->linear_attn;
		light.m_Attenuation2 = pWorldLight->quadratic_attn;
		break;

	case emit_skylight:
		light.m_Type = MATERIAL_LIGHT_DIRECTIONAL;
		break;

	// NOTE: Can't do quake lights in hardware (x-r factor)
	case emit_quakelight:	// not supported
	case emit_skyambient:	// doesn't factor into local lighting
		// skip these
		return false;
	}

	// No attenuation case..
	if ((light.m_Attenuation0 == 0.0f) && (light.m_Attenuation1 == 0.0f) &&
		(light.m_Attenuation2 == 0.0f))
	{
		light.m_Attenuation0 = 1.0f;
	}

	// renormalize light intensity...
	memcpy( &light.m_Position, &pWorldLight->origin, 3 * sizeof(float) );
	memcpy( &light.m_Direction, &pWorldLight->normal, 3 * sizeof(float) );
	light.m_Color[0] = pWorldLight->intensity[0];
	light.m_Color[1] = pWorldLight->intensity[1];
	light.m_Color[2] = pWorldLight->intensity[2];

	// Make it stop when the lighting gets to min%...
	float intensity = sqrtf( DotProduct( light.m_Color, light.m_Color ) );

	// Compute the light range based on attenuation factors
	if (pWorldLight->radius != 0)
	{
		light.m_Range = pWorldLight->radius;
	}
	else
	{
		// FALLBACK: older lights use this
		if (light.m_Attenuation2 == 0.0f)
		{
			if (light.m_Attenuation1 == 0.0f)
			{
				light.m_Range = sqrtf(FLT_MAX);
			}
			else
			{
				light.m_Range = (intensity / MIN_LIGHT_VALUE - light.m_Attenuation0) / light.m_Attenuation1;
			}
		}
		else
		{
			float a = light.m_Attenuation2;
			float b = light.m_Attenuation1;
			float c = light.m_Attenuation0 - intensity / MIN_LIGHT_VALUE;
			float discrim = b * b - 4 * a * c;
			if (discrim < 0.0f)
				light.m_Range = sqrtf(FLT_MAX);
			else
			{
				light.m_Range = (-b + sqrtf(discrim)) / (2.0f * a);
				if (light.m_Range < 0)
					light.m_Range = 0;
			}
		}
	}
	light.m_Flags = LIGHTTYPE_OPTIMIZATIONFLAGS_DERIVED_VALUES_CALCED;
	if( light.m_Attenuation0 != 0.0f )
	{
		light.m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0;
	}
	if( light.m_Attenuation1 != 0.0f )
	{
		light.m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1;
	}
	if( light.m_Attenuation2 != 0.0f )
	{
		light.m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Sets the hardware lighting state
//-----------------------------------------------------------------------------

static void R_SetNonAmbientLightingState( int numLights, dworldlight_t *locallight[MAXLOCALLIGHTS],
										  int *pNumLightDescs, LightDesc_t *pLightDescs, bool bUpdateStudioRenderLights )
{
	Assert( numLights >= 0 && numLights <= MAXLOCALLIGHTS );

	// convert dworldlight_t's to LightDesc_t's and send 'em down to g_pStudioRender->
	*pNumLightDescs = 0;

	LightDesc_t *pLightDesc;
	for ( int i = 0; i < numLights; i++)
	{
		pLightDesc = &pLightDescs[*pNumLightDescs];
		if (!WorldLightToMaterialLight( locallight[i], *pLightDesc ))
			continue;

		// Apply lightstyle
		float bias = LightStyleValue( locallight[i]->style );

		// Deal with overbrighting + bias
		pLightDesc->m_Color[0] *= bias;
		pLightDesc->m_Color[1] *= bias;
		pLightDesc->m_Color[2] *= bias;

		*pNumLightDescs += 1;
		Assert( *pNumLightDescs <= MAXLOCALLIGHTS );
	}

	if ( bUpdateStudioRenderLights )
	{
		g_pStudioRender->SetLocalLights( *pNumLightDescs, pLightDescs );
	}
}


//-----------------------------------------------------------------------------
// Computes the center of the studio model for illumination purposes
//-----------------------------------------------------------------------------
void R_ComputeLightingOrigin( IClientRenderable *pRenderable, studiohdr_t* pStudioHdr, const matrix3x4_t &matrix, Vector& center )
{
	int nAttachmentIndex = pStudioHdr->IllumPositionAttachmentIndex();
	if ( nAttachmentIndex <= 0 )
	{
		VectorTransform( pStudioHdr->illumposition, matrix, center );
	}
	else
	{
		matrix3x4_t attachment;
		pRenderable->GetAttachment( nAttachmentIndex, attachment );
		VectorTransform( pStudioHdr->illumposition, attachment, center );
	}
}



#if 0
// garymct - leave this in here for now. . we might need this for bumped models
void R_StudioCalculateVirtualLightAndLightCube( Vector& mid, Vector& virtualLightPosition,
											   Vector& virtualLightColor, Vector* lightBoxColor )
{
	int i, j;
	Vector delta;
	float dist2, ratio;
	byte  *pvis;
	float t;
	static ConVar bumpLightBlendRatioMin( "bump_light_blend_ratio_min", "0.00002" );
	static ConVar bumpLightBlendRatioMax( "bump_light_blend_ratio_max", "0.00004" );

	if ( g_pMaterialSystemConfig->nFullbright == 1 )
		return;
	
	VectorClear( virtualLightPosition );
	VectorClear( virtualLightColor );
	for( i = 0; i < 6; i++ )
		{
		VectorClear( lightBoxColor[i] );
	}
	byte pvs[MAX_MAP_LEAFS/8];
	pvis = CM_Vis( pvs, sizeof(pvs), CM_LeafCluster( CM_PointLeafnum( mid ), DVIS_PVS );

	float sumBumpBlendParam = 0;
	for (i = 0; i < host_state.worldbrush->numworldlights; i++)
	{
		dworldlight_t *wl = &host_state.worldbrush->worldlights[i];

		if (wl->cluster < 0)
			continue;

		// only do it if the entity can see into the lights leaf
		if (!BIT_SET( pvis, (wl->cluster)))
			continue;
	
		// hack: for this test, only deal with point light sources.
		if( wl->type != emit_point )
			continue;

		// check distance
		VectorSubtract( wl->origin, mid, delta );
		dist2 = DotProduct( delta, delta );
		
		ratio = R_WorldLightDistanceFalloff( wl, delta );
		
		VectorNormalize( delta );
		
		ratio = ratio * R_WorldLightAngle( wl, wl->normal, delta, delta );

		float bumpBlendParam; // 0.0 = all cube, 1.0 = all bump 
		
		// lerp
		bumpBlendParam = 
			( ratio - bumpLightBlendRatioMin.GetFloat() ) / 
			( bumpLightBlendRatioMax.GetFloat() - bumpLightBlendRatioMin.GetFloat() );
	
		if( bumpBlendParam > 0.0 )
		{
			// Get the bit that goes into the bump light
			sumBumpBlendParam += bumpBlendParam;
			VectorMA( virtualLightPosition, bumpBlendParam, wl->origin, virtualLightPosition );
			VectorMA( virtualLightColor, bumpBlendParam, wl->intensity, virtualLightColor );
		}

		if( bumpBlendParam < 1.0f )
		{
			// Get the bit that goes into the cube
			float cubeBlendParam;
			cubeBlendParam = 1.0f - bumpBlendParam;
			if( cubeBlendParam < 0.0f )
			{
				cubeBlendParam = 0.0f;
			}
			for (j = 0; j < numBoxDir; j++)
			{
				t = DotProduct( r_boxdir[j], delta );
				if (t > 0)
				{
					VectorMA( lightBoxColor[j], ratio * t * cubeBlendParam, wl->intensity, lightBoxColor[j] );
				}
			}
		}
	}
	// Get the final virtual light position and color.
	VectorMultiply( virtualLightPosition, 1.0f / sumBumpBlendParam, virtualLightPosition );
	VectorMultiply( virtualLightColor, 1.0f / sumBumpBlendParam, virtualLightColor );
}
#endif


// TODO: move cone calcs to position
// TODO: cone clipping calc's wont work for boxlight since the player asks for a single point.  Not sure what the volume is.
float Engine_WorldLightDistanceFalloff( const dworldlight_t *wl, const Vector& delta, bool bNoRadiusCheck )
{
	float falloff;

	switch (wl->type)
	{
		case emit_surface:
#if 1
			// Cull out stuff that's too far
			if (wl->radius != 0)
			{
				if ( DotProduct( delta, delta ) > (wl->radius * wl->radius))
					return 0.0f;
			}

			return InvRSquared(delta);
#else
			// 1/r*r
			falloff = DotProduct( delta, delta );
			if (falloff < 1)
				return 1.f;
			else
				return 1.f / falloff;
#endif

			break;

		case emit_skylight:
			return 1.f;
			break;

		case emit_quakelight:
			// X - r;
			falloff = wl->linear_attn - FastSqrt( DotProduct( delta, delta ) );
			if (falloff < 0)
				return 0.f;

			return falloff;
			break;

		case emit_skyambient:
			return 1.f;
			break;

		case emit_point:
		case emit_spotlight:	// directional & positional
			{
				float dist2, dist;

				dist2 = DotProduct( delta, delta );
				dist = FastSqrt( dist2 );

				// Cull out stuff that's too far
				if (!bNoRadiusCheck && (wl->radius != 0) && (dist > wl->radius))
					return 0.f;

				return 1.f / (wl->constant_attn + wl->linear_attn * dist + wl->quadratic_attn * dist2);
			}

			break;
		default:
			// Bug: need to return an error
			break;
	}
	return 1.f;
}

/*
  light_normal (lights normal translated to same space as other normals)
  surface_normal
  light_direction_normal | (light_pos - vertex_pos) |
*/
float Engine_WorldLightAngle( const dworldlight_t *wl, const Vector& lnormal, const Vector& snormal, const Vector& delta )
{
	float dot, dot2, ratio = 0;

	switch (wl->type)
	{
		case emit_surface:
			dot = DotProduct( snormal, delta );
			if (dot < 0)
				return 0;

			dot2 = -DotProduct (delta, lnormal);
			if (dot2 <= ON_EPSILON/10)
				return 0; // behind light surface

			return dot * dot2;

		case emit_point:
			dot = DotProduct( snormal, delta );
			if (dot < 0)
				return 0;
			return dot;

		case emit_spotlight:
//			return 1.0; // !!!
			dot = DotProduct( snormal, delta );
			if (dot < 0)
				return 0;

			dot2 = -DotProduct (delta, lnormal);
			if (dot2 <= wl->stopdot2)
				return 0; // outside light cone

			ratio = dot;
			if (dot2 >= wl->stopdot)
				return ratio;	// inside inner cone

			if ((wl->exponent == 1) || (wl->exponent == 0))
			{
				ratio *= (dot2 - wl->stopdot2) / (wl->stopdot - wl->stopdot2);
			}
			else
			{
				ratio *= pow((dot2 - wl->stopdot2) / (wl->stopdot - wl->stopdot2), wl->exponent );
			}
			return ratio;

		case emit_skylight:
			dot2 = -DotProduct( snormal, lnormal );
			if (dot2 < 0)
				return 0;
			return dot2;

		case emit_quakelight:
			// linear falloff
			dot = DotProduct( snormal, delta );
			if (dot < 0)
				return 0;
			return dot;

		case emit_skyambient:
			// not supported
			return 1;

		default:
			// Bug: need to return an error
			break;
	} 
	return 0;
}

//-----------------------------------------------------------------------------
// Allocator for color mesh vertex buffers (for use with static props only).
// It uses a trivial allocation scheme, which assumes that allocations and
// deallocations are not interleaved (you do all allocs, then all deallocs).
//-----------------------------------------------------------------------------
class CPooledVBAllocator_ColorMesh : public IPooledVBAllocator
{
public:

	CPooledVBAllocator_ColorMesh();
	virtual ~CPooledVBAllocator_ColorMesh();

	// Allocate the shared mesh (vertex buffer)
	virtual bool			Init( VertexFormat_t format, int numVerts );
	// Free the shared mesh (after Deallocate is called for all sub-allocs)
	virtual void			Clear();

	// Get the shared mesh (vertex buffer) from which sub-allocations are made
	virtual IMesh			*GetSharedMesh() { return m_pMesh; }

	// Get a pointer to the start of the vertex buffer data
	virtual void			*GetVertexBufferBase() { return m_pVertexBufferBase; }
	virtual int				GetNumVertsAllocated() { return m_totalVerts; } 

	// Allocate a sub-range of 'numVerts' from free space in the shared vertex buffer
	// (returns the byte offset from the start of the VB to the new allocation)
	virtual int				Allocate( int numVerts );
	// Deallocate an existing allocation
	virtual void			Deallocate( int offset, int numVerts );

private:

	// Assert/warn that the allocator is in a clear/empty state (returns FALSE if not)
	bool					CheckIsClear( void );

	IMesh		*m_pMesh;				// The shared mesh (vertex buffer) from which sub-allocations are made
	void		*m_pVertexBufferBase;	// A pointer to the start of the vertex buffer data
	int			m_totalVerts;			// The number of verts in the shared vertex buffer
	int			m_vertexSize;			// The stride of the shared vertex buffer

	int			m_numAllocations;		// The number of extant allocations
	int			m_numVertsAllocated;	// The number of vertices in extant allocations
	int			m_nextFreeOffset;		// The offset to be returned by the next call to Allocate()
	// (incremented as a simple stack)
	bool		m_bStartedDeallocation;	// This is set when Deallocate() is called for the first time,
	// at which point Allocate() cannot be called again until all
	// extant allocations have been deallocated.
};

struct colormeshparams_t
{
	int					m_nMeshes;
	int					m_nTotalVertexes;
	// Given memory alignment (VBs must be 4-KB aligned on X360, for example), it can be more efficient
	// to allocate many color meshes out of a single shared vertex buffer (using vertex 'stream offset')
	IPooledVBAllocator *m_pPooledVBAllocator;
	int					m_nVertexes[256];
	FileNameHandle_t	m_fnHandle;
};

class CColorMeshData
{
public:
	void DestroyResource()
	{
		g_pFileSystem->AsyncFinish( m_hAsyncControlVertex, true );
		g_pFileSystem->AsyncRelease( m_hAsyncControlVertex );

		g_pFileSystem->AsyncFinish( m_hAsyncControlTexel, true );
		g_pFileSystem->AsyncRelease( m_hAsyncControlTexel );

		// release the array of meshes
		CMatRenderContextPtr pRenderContext( materials );

		for ( int i=0; i<m_nMeshes; i++ )
		{
			if ( m_pMeshInfos[i].m_pPooledVBAllocator )
			{
				// Let the pooling allocator dealloc this sub-range of the shared vertex buffer
				m_pMeshInfos[i].m_pPooledVBAllocator->Deallocate( m_pMeshInfos[i].m_nVertOffsetInBytes, m_pMeshInfos[i].m_nNumVerts );
			}
			else
			{
				// Free this standalone mesh
				pRenderContext->DestroyStaticMesh( m_pMeshInfos[i].m_pMesh );
			}

			if (m_pMeshInfos[i].m_pLightmap)
			{
				m_pMeshInfos[i].m_pLightmap->Release();
				m_pMeshInfos[i].m_pLightmap = NULL;
			}

			if (m_pMeshInfos[i].m_pLightmapData)
			{
				delete [] m_pMeshInfos[i].m_pLightmapData->m_pTexelData;
				delete m_pMeshInfos[i].m_pLightmapData;
			}
		}


		delete [] m_pMeshInfos;
		delete [] m_ppTargets;
		delete this;
	}

	CColorMeshData *GetData()
	{ 
		return this; 
	}

	unsigned int Size()
	{ 
		// TODO: This is wrong because we don't currently account for the size of the textures we create. 
		// However, that data isn't available until way after this query is made, so just live with 
		// this for now I guess?
		return m_nTotalSize; 
	}

	static CColorMeshData *CreateResource( const colormeshparams_t &params )
	{
		CColorMeshData *data = new CColorMeshData;

		data->m_bHasInvalidVB = false;
		data->m_bColorMeshValid = false;
		data->m_bColorTextureValid = false;
		data->m_bColorTextureCreated = false;
		data->m_bNeedsRetry = false;
		data->m_hAsyncControlVertex = NULL;
		data->m_hAsyncControlTexel = NULL;
		data->m_fnHandle = params.m_fnHandle;
		
		data->m_nTotalSize = params.m_nMeshes * sizeof( IMesh* ) + params.m_nTotalVertexes * 4;
		data->m_nMeshes = params.m_nMeshes;
		data->m_pMeshInfos = new ColorMeshInfo_t[params.m_nMeshes];
		Q_memset( data->m_pMeshInfos, 0, params.m_nMeshes*sizeof( ColorMeshInfo_t ) );
		data->m_ppTargets = new unsigned char *[params.m_nMeshes];

		CMeshBuilder meshBuilder;
		CMatRenderContextPtr pRenderContext( materials );
	
		for ( int i=0; i<params.m_nMeshes; i++ )
		{
			VertexFormat_t vertexFormat = VERTEX_SPECULAR;

			data->m_pMeshInfos[i].m_pMesh				= NULL;
			data->m_pMeshInfos[i].m_pPooledVBAllocator	= params.m_pPooledVBAllocator;
			data->m_pMeshInfos[i].m_nVertOffsetInBytes	= 0;
			data->m_pMeshInfos[i].m_nNumVerts			= params.m_nVertexes[i];
			data->m_pMeshInfos[i].m_pLightmapData		= NULL;
			data->m_pMeshInfos[i].m_pLightmap		= NULL;

			if ( params.m_pPooledVBAllocator != NULL )
			{
				// Allocate a portion of a single, shared VB for each color mesh
				data->m_pMeshInfos[i].m_nVertOffsetInBytes = params.m_pPooledVBAllocator->Allocate( params.m_nVertexes[i] );
				
				if ( data->m_pMeshInfos[i].m_nVertOffsetInBytes == -1 )
				{
					// Failed (fall back to regular allocations)
					data->m_pMeshInfos[i].m_pPooledVBAllocator = NULL;
					data->m_pMeshInfos[i].m_nVertOffsetInBytes = 0;
				}
				else
				{
					// Set up the mesh+data pointers
					data->m_pMeshInfos[i].m_pMesh	= params.m_pPooledVBAllocator->GetSharedMesh();
					data->m_ppTargets[i]			= ( (unsigned char *)params.m_pPooledVBAllocator->GetVertexBufferBase() ) + data->m_pMeshInfos[i].m_nVertOffsetInBytes;
				}
			}

			if ( data->m_pMeshInfos[i].m_pMesh == NULL )
			{
				if ( g_VBAllocTracker )
					g_VBAllocTracker->TrackMeshAllocations( "CColorMeshData::CreateResource" );

				// Allocate a standalone VB per color mesh
				data->m_pMeshInfos[i].m_pMesh = pRenderContext->CreateStaticMesh( vertexFormat, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_COLOR );

				if ( g_VBAllocTracker )
					g_VBAllocTracker->TrackMeshAllocations( NULL );
			}

			Assert( data->m_pMeshInfos[i].m_pMesh );
			if ( !data->m_pMeshInfos[i].m_pMesh )
			{
				data->DestroyResource();
				data = NULL;
				break;
			}
		}
		return data;
	}

	static unsigned int EstimatedSize( const colormeshparams_t &params )
	{
		// each vertex is a 4 byte color
		return params.m_nMeshes * sizeof( IMesh* ) + params.m_nTotalVertexes * 4;
	}

	int					m_nMeshes;
	ColorMeshInfo_t		*m_pMeshInfos;
	unsigned char		**m_ppTargets;
	unsigned int		m_nTotalSize;
	FSAsyncControl_t	m_hAsyncControlVertex;
	FSAsyncControl_t	m_hAsyncControlTexel;
	unsigned int		m_bHasInvalidVB : 1;
	unsigned int		m_bColorMeshValid : 1;
	unsigned int		m_bColorTextureValid : 1;		// Whether the texture data is valid, but not necessarily created
	unsigned int		m_bColorTextureCreated : 1;	// Whether the texture data has actually been created.
	unsigned int		m_bNeedsRetry : 1;

	FileNameHandle_t	m_fnHandle;
};

//-----------------------------------------------------------------------------
//
// Implementation of IVModelRender
//
//-----------------------------------------------------------------------------

// UNDONE: Move this to hud export code, subsume previous functions
class CModelRender : public IVModelRender,
					 public CManagedDataCacheClient< CColorMeshData, colormeshparams_t >
{
public:
	// members of the IVModelRender interface
	virtual void ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType = OVERRIDE_NORMAL );
	virtual int DrawModel( 	
					int flags, IClientRenderable *cliententity,
					ModelInstanceHandle_t instance, int entity_index, const model_t *model, 
					const Vector& origin, QAngle const& angles,
					int skin, int body, int hitboxset, 
					const matrix3x4_t* pModelToWorld,
					const matrix3x4_t *pLightingOffset );

	virtual void  SetViewTarget( const CStudioHdr *pStudioHdr, int nBodyIndex, const Vector& target );

	// Creates, destroys instance data to be associated with the model
	virtual ModelInstanceHandle_t CreateInstance( IClientRenderable *pRenderable, LightCacheHandle_t* pHandle );
	virtual void SetStaticLighting( ModelInstanceHandle_t handle, LightCacheHandle_t* pCache );
	virtual LightCacheHandle_t GetStaticLighting( ModelInstanceHandle_t handle );
	virtual void DestroyInstance( ModelInstanceHandle_t handle );
	virtual bool ChangeInstance( ModelInstanceHandle_t handle, IClientRenderable *pRenderable );

	// Creates a decal on a model instance by doing a planar projection
	// along the ray. The material is the decal material, the radius is the
	// radius of the decal to create.
	virtual void AddDecal( ModelInstanceHandle_t handle, Ray_t const& ray, 
		const Vector& decalUp, int decalIndex, int body, bool noPokethru = false, int maxLODToDecal = ADDDECAL_TO_ALL_LODS );
	virtual void AddColoredDecal( ModelInstanceHandle_t handle, Ray_t const& ray, 
		const Vector& decalUp, int decalIndex, int body, Color cColor, bool noPokethru = false, int maxLODToDecal = ADDDECAL_TO_ALL_LODS );
	
	virtual void GetMaterialOverride( IMaterial** ppOutForcedMaterial, OverrideType_t* pOutOverrideType );

	// Removes all the decals on a model instance
	virtual void RemoveAllDecals( ModelInstanceHandle_t handle );

	// Remove all decals from all models
	virtual void RemoveAllDecalsFromAllModels();

	// Shadow rendering (render-to-texture)
	virtual matrix3x4_t* DrawModelShadowSetup( IClientRenderable *pRenderable, int body, int skin, DrawModelInfo_t *pInfo, matrix3x4_t *pBoneToWorld );
	virtual void DrawModelShadow( IClientRenderable *pRenderable, const DrawModelInfo_t &info, matrix3x4_t *pBoneToWorld );

	// Used to allow the shadow mgr to manage a list of shadows per model
	unsigned short& FirstShadowOnModelInstance( ModelInstanceHandle_t handle ) { return m_ModelInstances[handle].m_FirstShadow; }

	// This gets called when overbright, etc gets changed to recompute static prop lighting.
	virtual bool RecomputeStaticLighting( ModelInstanceHandle_t handle );

	// Handlers for alt-tab
	virtual void ReleaseAllStaticPropColorData( void );
	virtual void RestoreAllStaticPropColorData( void );

	// Extended version of drawmodel
	virtual bool DrawModelSetup( ModelRenderInfo_t &pInfo, DrawModelState_t *pState, matrix3x4_t *pBoneToWorld, matrix3x4_t** ppBoneToWorldOut );
	virtual int	DrawModelEx( ModelRenderInfo_t &pInfo );
	virtual int	DrawModelExStaticProp( ModelRenderInfo_t &pInfo );
	virtual int DrawStaticPropArrayFast( StaticPropRenderInfo_t *pProps, int count, bool bShadowDepth );

	// Sets up lighting context for a point in space
	virtual void SetupLighting( const Vector &vecCenter );
	virtual void SuppressEngineLighting( bool bSuppress );

	inline vertexFileHeader_t *CacheVertexData() { return g_pMDLCache->GetVertexData( (MDLHandle_t)(int)m_pStudioHdr->virtualModel&0xffff ); }

	bool Init();
	void Shutdown();

	bool GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen );

	struct staticPropAsyncContext_t
	{
		DataCacheHandle_t	m_ColorMeshHandle;
		CColorMeshData		*m_pColorMeshData;
		int					m_nMeshes;
		unsigned int		m_nRootLOD;
		char				m_szFilenameVertex[MAX_PATH];
		char				m_szFilenameTexel[MAX_PATH];
	};


	void StaticPropColorMeshCallback( void *pContext, const void *pData, int numReadBytes, FSAsyncStatus_t asyncStatus );
	void StaticPropColorTexelCallback(void *pContext, const void *pData, int numReadBytes, FSAsyncStatus_t asyncStatus);


	// 360 holds onto static prop color meshes during same map transitions
	void PurgeCachedStaticPropColorData();
	bool IsStaticPropColorDataCached( const char *pName );
	DataCacheHandle_t GetCachedStaticPropColorData( const char *pName );

	virtual void SetupColorMeshes( int nTotalVerts );

private:
	enum
	{
		CURRENT_LIGHTING_UNINITIALIZED = -999999
	};

	enum ModelInstanceFlags_t
	{
		MODEL_INSTANCE_HAS_STATIC_LIGHTING    = 0x1,
		MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR = 0x2,
		MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD = 0x4,
		MODEL_INSTANCE_HAS_COLOR_DATA		  = 0x8
	};

	struct ModelInstance_t
	{
		IClientRenderable* m_pRenderable;

		// Need to store off the model. When it changes, we lose all instance data..
		model_t* m_pModel;
		StudioDecalHandle_t	m_DecalHandle;

		// Stores off the current lighting state
		LightingState_t m_CurrentLightingState;
		LightingState_t	m_AmbientLightingState;
		Vector m_flLightIntensity[MAXLOCALLIGHTS];
		float m_flLightingTime;

		// First shadow projected onto the model
		unsigned short	m_FirstShadow;
		unsigned short  m_nFlags;

		// Static lighting
		LightCacheHandle_t m_LightCacheHandle;

		// Color mesh managed by cache
		DataCacheHandle_t m_ColorMeshHandle;
	};

	// Sets up the render state for a model
	matrix3x4_t* SetupModelState( IClientRenderable *pRenderable );

	int ComputeLOD( const ModelRenderInfo_t &info, studiohwdata_t *pStudioHWData );

	void DrawModelExecute( const DrawModelState_t &state, const ModelRenderInfo_t &pInfo, matrix3x4_t *pCustomBoneToWorld = NULL );

	void InitColormeshParams( ModelInstance_t &instance, studiohwdata_t *pStudioHWData, colormeshparams_t *pColorMeshParams );
	CColorMeshData *FindOrCreateStaticPropColorData( ModelInstanceHandle_t handle );
	void DestroyStaticPropColorData( ModelInstanceHandle_t handle );
	bool UpdateStaticPropColorData( IHandleEntity *pEnt, ModelInstanceHandle_t handle );
	void ProtectColorDataIfQueued( DataCacheHandle_t );

	void ValidateStaticPropColorData( ModelInstanceHandle_t handle );
	bool LoadStaticPropColorData( IHandleEntity *pProp, DataCacheHandle_t colorMeshHandle, studiohwdata_t *pStudioHWData );

	// Returns true if the model instance is valid
	bool IsModelInstanceValid( ModelInstanceHandle_t handle );
	
	void DebugDrawLightingOrigin( const DrawModelState_t& state, const ModelRenderInfo_t &pInfo );

	LightingState_t *TimeAverageLightingState( ModelInstanceHandle_t handle,
		LightingState_t *pLightingState, int nEntIndex, const Vector *pLightingOrigin );

	// Cause the current lighting state to match the given one
	void SnapCurrentLightingState( ModelInstance_t &inst, LightingState_t *pLightingState );

	// Sets up lighting state for rendering
	void StudioSetupLighting( const DrawModelState_t &state, const Vector& absEntCenter, 
		LightCacheHandle_t* pLightcache, bool bVertexLit, bool bNeedsEnvCubemap, bool &bStaticLighting, 
		DrawModelInfo_t &drawInfo, const ModelRenderInfo_t &pInfo, int drawFlags );

	// Time average the ambient term
	void TimeAverageAmbientLight( LightingState_t &actualLightingState, ModelInstance_t &inst, 
		float flAttenFactor, LightingState_t *pLightingState, const Vector *pLightingOrigin );

	// Old-style computation of vertex lighting
	void ComputeModelVertexLightingOld( mstudiomodel_t *pModel, 
		matrix3x4_t& matrix, const LightingState_t &lightingState, color24 *pLighting,
		bool bUseConstDirLighting, float flConstDirLightAmount );

	// New-style computation of vertex lighting
	void ComputeModelVertexLighting( IHandleEntity *pProp, 
		mstudiomodel_t *pModel, OptimizedModel::ModelLODHeader_t *pVtxLOD,
		matrix3x4_t& matrix, Vector4D *pTempMem, color24 *pLighting );

	// Internal Decal
	void AddDecalInternal( ModelInstanceHandle_t handle, Ray_t const& ray, const Vector& decalUp, int decalIndex, int body, bool bUseColor, Color cColor, bool noPokeThru, int maxLODToDecal);

	// Model instance data
	CUtlLinkedList< ModelInstance_t, ModelInstanceHandle_t > m_ModelInstances; 

	// current active model
	studiohdr_t *m_pStudioHdr;

	bool m_bSuppressEngineLighting;

	CUtlDict< DataCacheHandle_t, int > m_CachedStaticPropColorData;
	CThreadFastMutex m_CachedStaticPropMutex;

	// Allocator for static prop color mesh vertex buffers (all are pooled into one VB)
	CPooledVBAllocator_ColorMesh	m_colorMeshVBAllocator;
};


static CModelRender s_ModelRender;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CModelRender, IVModelRender, VENGINE_HUDMODEL_INTERFACE_VERSION, s_ModelRender );
IVModelRender* modelrender = &s_ModelRender;

//-----------------------------------------------------------------------------
// Resource loading for static prop lighting
//-----------------------------------------------------------------------------
class CResourcePreloadPropLighting : public CResourcePreload
{
	virtual bool CreateResource( const char *pName )
	{
		if ( !r_proplightingfromdisk.GetBool() )
		{
			// do nothing, not an error
			return true;
		}

		char szBasename[MAX_PATH];
		char szFilename[MAX_PATH];
		V_FileBase( pName, szBasename, sizeof( szBasename ) );
		V_snprintf( szFilename, sizeof( szFilename ), "%s%s.vhv", szBasename, GetPlatformExt() );

		// static props have the same name across maps
		// can check if loading the same map and early out if data present
		if ( g_pQueuedLoader->IsSameMapLoading() && s_ModelRender.IsStaticPropColorDataCached( szFilename ) )
		{
			// same map is loading, all disk prop lighting was left in the cache
			// otherwise the pre-purge operation below will do the cleanup
			return true;
		}

		// create an anonymous job to get the lighting data in memory, claim during static prop instancing
		LoaderJob_t loaderJob;
		loaderJob.m_pFilename = szFilename;
		loaderJob.m_pPathID = "GAME";
		loaderJob.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
		g_pQueuedLoader->AddJob( &loaderJob );
		return true;
	}

	//-----------------------------------------------------------------------------
	// Pre purge operation before i/o commences
	//-----------------------------------------------------------------------------
	virtual void PurgeUnreferencedResources()
	{
		if ( g_pQueuedLoader->IsSameMapLoading() )
		{
			// do nothing, same map is loading, correct disk prop lighting will still be in data cache
			return;
		}

		// Map is different, need to purge any existing disk prop lighting
		// before anonymous i/o commences, otherwise 2x memory usage
		s_ModelRender.PurgeCachedStaticPropColorData();
	}

	virtual void PurgeAll()
	{
		s_ModelRender.PurgeCachedStaticPropColorData();
	}
};
static CResourcePreloadPropLighting s_ResourcePreloadPropLighting;

//-----------------------------------------------------------------------------
// Init, shutdown studiorender
//-----------------------------------------------------------------------------
void InitStudioRender( void )
{
	UpdateStudioRenderConfig();
	s_ModelRender.Init();
}

void ShutdownStudioRender( void )
{
	s_ModelRender.Shutdown();
}

//-----------------------------------------------------------------------------
// Hook needed for shadows to work
//-----------------------------------------------------------------------------
unsigned short& FirstShadowOnModelInstance( ModelInstanceHandle_t handle )
{
	return s_ModelRender.FirstShadowOnModelInstance( handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void R_RemoveAllDecalsFromAllModels()
{
	s_ModelRender.RemoveAllDecalsFromAllModels();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CModelRender::Init()
{
	// start a managed section in the cache
	CCacheClientBaseClass::Init( g_pDataCache, "ColorMesh" );

	if ( IsX360() )
	{
		g_pQueuedLoader->InstallLoader( RESOURCEPRELOAD_STATICPROPLIGHTING, &s_ResourcePreloadPropLighting );
	}

	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CModelRender::Shutdown()
{
	// end the managed section
	CCacheClientBaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Used by the client to allow it to set lighting state instead of this code
//-----------------------------------------------------------------------------
void CModelRender::SuppressEngineLighting( bool bSuppress )
{
	m_bSuppressEngineLighting = bSuppress;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CModelRender::GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen )
{
	CColorMeshData *pColorMeshData = (CColorMeshData *)pItem;
	g_pFileSystem->String( pColorMeshData->m_fnHandle, pDest, nMaxLen );
	return true;
}


//-----------------------------------------------------------------------------
// Cause the current lighting state to match the given one
//-----------------------------------------------------------------------------
void CModelRender::SnapCurrentLightingState( ModelInstance_t &inst, LightingState_t *pLightingState )
{
	inst.m_CurrentLightingState = *pLightingState;
	for ( int i = 0; i < MAXLOCALLIGHTS; ++i )
	{
		if ( i < pLightingState->numlights )
		{
			inst.m_flLightIntensity[i] = pLightingState->locallight[i]->intensity; 
		}
		else
		{
			inst.m_flLightIntensity[i].Init( 0.0f, 0.0f, 0.0f );
		}
	}

#ifndef SWDS
	inst.m_flLightingTime = cl.GetTime();
#endif
}


#define AMBIENT_MAX 8.0f

//-----------------------------------------------------------------------------
// Time average the ambient term
//-----------------------------------------------------------------------------
void CModelRender::TimeAverageAmbientLight( LightingState_t &actualLightingState, 
										   ModelInstance_t &inst, float flAttenFactor, LightingState_t *pLightingState, const Vector *pLightingOrigin )
{
	flAttenFactor = clamp( flAttenFactor, 0.f, 1.f );   // don't need this but alex is a coward
	Vector vecDelta;
	for ( int i = 0; i < 6; ++i )
	{
		VectorSubtract( pLightingState->r_boxcolor[i], inst.m_CurrentLightingState.r_boxcolor[i], vecDelta );
		vecDelta *= flAttenFactor;
		inst.m_CurrentLightingState.r_boxcolor[i] = pLightingState->r_boxcolor[i] - vecDelta;

#if defined( VISUALIZE_TIME_AVERAGE ) && !defined( SWDS )
		if ( pLightingOrigin )
		{
			Vector vecDir = vec3_origin;
			vecDir[ i >> 1 ] = (i & 0x1) ? -1.0f : 1.0f;
			CDebugOverlay::AddLineOverlay( *pLightingOrigin, *pLightingOrigin + vecDir * 20, 
				255 * inst.m_CurrentLightingState.r_boxcolor[i].x, 
				255 * inst.m_CurrentLightingState.r_boxcolor[i].y,
				255 * inst.m_CurrentLightingState.r_boxcolor[i].z, 255, false, 5.0f );

			CDebugOverlay::AddLineOverlay( *pLightingOrigin + Vector(5, 5, 5), *pLightingOrigin + vecDir * 50, 
				255 * pLightingState->r_boxcolor[i].x, 
				255 * pLightingState->r_boxcolor[i].y,
				255 * pLightingState->r_boxcolor[i].z, 255, true, 5.0f );
		}
#endif
		// haven't been able to find this rare bug which results in ambient light getting "stuck"
		// on the viewmodel extremely rarely , presumably with infinities. So, mask the bug
		// (hopefully) and warn by clamping.
#ifndef NDEBUG
		Assert( inst.m_CurrentLightingState.r_boxcolor[i].IsValid() );
		for( int nComp = 0 ; nComp < 3; nComp++ )
		{
			Assert( inst.m_CurrentLightingState.r_boxcolor[i][nComp] >= 0.0 );
			Assert( inst.m_CurrentLightingState.r_boxcolor[i][nComp] <= AMBIENT_MAX );
		}
#endif
		inst.m_CurrentLightingState.r_boxcolor[i].x = clamp( inst.m_CurrentLightingState.r_boxcolor[i].x, 0.f, AMBIENT_MAX );
		inst.m_CurrentLightingState.r_boxcolor[i].y = clamp( inst.m_CurrentLightingState.r_boxcolor[i].y, 0.f, AMBIENT_MAX );
		inst.m_CurrentLightingState.r_boxcolor[i].z = clamp( inst.m_CurrentLightingState.r_boxcolor[i].z, 0.f, AMBIENT_MAX );
	}
	memcpy( &actualLightingState.r_boxcolor, &inst.m_CurrentLightingState.r_boxcolor, sizeof(inst.m_CurrentLightingState.r_boxcolor) );
}




//-----------------------------------------------------------------------------
// Do time averaging of the lighting state to avoid popping...
//-----------------------------------------------------------------------------
LightingState_t *CModelRender::TimeAverageLightingState( ModelInstanceHandle_t handle, LightingState_t *pLightingState, int nEntIndex, const Vector *pLightingOrigin )
{
	if ( r_lightaverage.GetInt() == 0 )
		return pLightingState;

#ifndef SWDS

	float flInterpFactor = r_lightinterp.GetFloat();
	if ( flInterpFactor == 0 )
		return pLightingState;

	if ( handle == MODEL_INSTANCE_INVALID)
		return pLightingState;

	ModelInstance_t &inst = m_ModelInstances[handle];
	if ( inst.m_flLightingTime == CURRENT_LIGHTING_UNINITIALIZED )
	{
		SnapCurrentLightingState( inst, pLightingState );
		return pLightingState;
	}

	float dt = (cl.GetTime() - inst.m_flLightingTime);
	if ( dt <= 0.0f )
	{
		dt = 0.0f;
	}
	else
	{
		inst.m_flLightingTime = cl.GetTime();
	}

	static LightingState_t actualLightingState;
	static dworldlight_t s_WorldLights[MAXLOCALLIGHTS];
	
	// I'm creating the equation v = vf - (vf-vi)e^-at 
	// where vf = this frame's lighting value, vi = current time averaged lighting value
	int i;
	Vector vecDelta;
	float flAttenFactor = exp( -flInterpFactor * dt );
	TimeAverageAmbientLight( actualLightingState, inst, flAttenFactor, pLightingState, pLightingOrigin );

	// Max # of lights...
	int nWorldLights;
	if ( !g_pMaterialSystemConfig->bSoftwareLighting )
	{
		nWorldLights = min( g_pMaterialSystemHardwareConfig->MaxNumLights(), r_worldlights.GetInt() );
	}
	else
	{
		nWorldLights = r_worldlights.GetInt();
	}

	// Create a mapping of identical lights
	int nMatchCount = 0;
	bool pMatch[MAXLOCALLIGHTS];
	Vector pLight[MAXLOCALLIGHTS];
	dworldlight_t *pSourceLight[MAXLOCALLIGHTS];

	memset( pMatch, 0, sizeof(pMatch) );
	for ( i = 0; i < pLightingState->numlights; ++i )
	{
		// By default, assume the light doesn't match an existing light, so blend up from 0
		pLight[i].Init( 0.0f, 0.0f, 0.0f );
		int j;
		for ( j = 0; j < inst.m_CurrentLightingState.numlights; ++j )
		{
			if ( pLightingState->locallight[i] == inst.m_CurrentLightingState.locallight[j] )
			{
				// Ok, we found a matching light, so use the intensity of that light at the moment
				++nMatchCount;
				pMatch[j] = true;
				pLight[i] = inst.m_flLightIntensity[j];
				break;
			}
		}
	}

	// For the lights in the current lighting state, attenuate them toward their actual value
	for ( i = 0; i < pLightingState->numlights; ++i )
	{
		actualLightingState.locallight[i] = &s_WorldLights[i];
		memcpy( &s_WorldLights[i], pLightingState->locallight[i], sizeof(dworldlight_t) );

		// Light already exists? Attenuate to it...
		VectorSubtract( pLightingState->locallight[i]->intensity, pLight[i], vecDelta );
		vecDelta *= flAttenFactor;

		s_WorldLights[i].intensity = pLightingState->locallight[i]->intensity - vecDelta;
		pSourceLight[i] = pLightingState->locallight[i];
	}

	// Ramp down any light we can; we may not be able to ramp them all down
	int nCurrLight = pLightingState->numlights;
	for ( i = 0; i < inst.m_CurrentLightingState.numlights; ++i )
	{
		if ( pMatch[i] )
			continue;

		// Has it faded out to black? Then remove it.
		if ( inst.m_flLightIntensity[i].LengthSqr() < 1 )
			continue;

		if ( nCurrLight >= MAXLOCALLIGHTS )
			break;

		actualLightingState.locallight[nCurrLight] = &s_WorldLights[nCurrLight];
		memcpy( &s_WorldLights[nCurrLight], inst.m_CurrentLightingState.locallight[i], sizeof(dworldlight_t) );

		// Attenuate to black (fade out)
		VectorMultiply( inst.m_flLightIntensity[i], flAttenFactor, vecDelta );

		s_WorldLights[nCurrLight].intensity = vecDelta;
		pSourceLight[nCurrLight] = inst.m_CurrentLightingState.locallight[i];

		if (( nCurrLight >= nWorldLights ) && pLightingOrigin)
		{
			AddWorldLightToAmbientCube( &s_WorldLights[nCurrLight], *pLightingOrigin, actualLightingState.r_boxcolor ); 
		}

		++nCurrLight;
	}

	actualLightingState.numlights = min( nCurrLight, nWorldLights );
	inst.m_CurrentLightingState.numlights = nCurrLight;

	for ( i = 0; i < nCurrLight; ++i )
	{
		inst.m_CurrentLightingState.locallight[i] = pSourceLight[i];
		inst.m_flLightIntensity[i] = s_WorldLights[i].intensity;

#if defined( VISUALIZE_TIME_AVERAGE ) && !defined( SWDS )
		Vector vecColor = pSourceLight[i]->intensity;
		float flMax = max( vecColor.x, vecColor.y );
		flMax = max( flMax, vecColor.z );
		if ( flMax == 0.0f )
		{
			flMax = 1.0f;
		}
		vecColor *= 255.0f / flMax;
		float flRatio = inst.m_flLightIntensity[i].Length() / pSourceLight[i]->intensity.Length();
		vecColor *= flRatio;
		CDebugOverlay::AddLineOverlay( *pLightingOrigin, pSourceLight[i]->origin, 
			vecColor.x, vecColor.y, vecColor.z, 255, false, 5.0f );
#endif
	}

	return &actualLightingState;
#else
	return pLightingState;
#endif
}

// Ambient boost settings
static ConVar r_ambientboost( "r_ambientboost", "1", FCVAR_ARCHIVE, "Set to boost ambient term if it is totally swamped by local lights" );
static ConVar r_ambientmin( "r_ambientmin", "0.3", FCVAR_ARCHIVE, "Threshold above which ambient cube will not boost (i.e. it's already sufficiently bright" );
static ConVar r_ambientfraction( "r_ambientfraction", "0.1", FCVAR_CHEAT, "Fraction of direct lighting that ambient cube must be below to trigger boosting" );
static ConVar r_ambientfactor( "r_ambientfactor", "5", FCVAR_ARCHIVE, "Boost ambient cube by no more than this factor" );
static ConVar r_lightcachemodel ( "r_lightcachemodel", "-1", FCVAR_CHEAT, "" );
static ConVar r_drawlightcache ("r_drawlightcache", "0", FCVAR_CHEAT, "0: off\n1: draw light cache entries\n2: draw rays\n");


//-----------------------------------------------------------------------------
// Sets up lighting state for rendering
//-----------------------------------------------------------------------------
void CModelRender::StudioSetupLighting( const DrawModelState_t &state, const Vector& absEntCenter, 
	LightCacheHandle_t* pLightcache, bool bVertexLit, bool bNeedsEnvCubemap, bool &bStaticLighting, 
	DrawModelInfo_t &drawInfo, const ModelRenderInfo_t &pInfo, int drawFlags )
{
	if ( m_bSuppressEngineLighting )
		return;

#ifndef SWDS
	ITexture *pEnvCubemapTexture = NULL;
	LightingState_t lightingState;

	Vector pSaveLightPos[MAXLOCALLIGHTS];

	Vector *pDebugLightingOrigin = NULL;
	Vector vecDebugLightingOrigin = vec3_origin;

	// Cache off lighting data for rendering decals - only on dx8/dx9.
	LightingState_t lightingDecalState;

	drawInfo.m_bStaticLighting = bStaticLighting && g_pMaterialSystemHardwareConfig->SupportsStaticPlusDynamicLighting();
	drawInfo.m_nLocalLightCount = 0;

	// Compute lighting origin from input
	Vector vLightingOrigin( 0.0f, 0.0f, 0.0f );
	CMatRenderContextPtr pRenderContext( materials );
	if ( pInfo.pLightingOrigin )
	{
		vLightingOrigin = *pInfo.pLightingOrigin;
	}
	else
	{
		vLightingOrigin = absEntCenter;
		if ( pInfo.pLightingOffset )
		{
			VectorTransform( absEntCenter, *pInfo.pLightingOffset, vLightingOrigin );
		}
	}

	// Set the lighting origin state
	pRenderContext->SetLightingOrigin( vLightingOrigin );

	ModelInstance_t *pModelInst = NULL;
	bool bHasDecals = false;
	if ( pInfo.instance != m_ModelInstances.InvalidIndex() )
	{
		pModelInst = &m_ModelInstances[pInfo.instance];
		if ( pModelInst )
		{
			bHasDecals = ( pModelInst->m_DecalHandle != STUDIORENDER_DECAL_INVALID );
		}
	}

	if ( pLightcache )
	{
		// static prop case.
		if ( bStaticLighting )
		{
			if ( g_pMaterialSystemHardwareConfig->SupportsStaticPlusDynamicLighting() )
			{
				LightingState_t *pLightingState = NULL;
				// dx8 and dx9 case. . .hardware can do baked lighting plus other dynamic lighting
				// We already have the static part baked into a color mesh, so just get the dynamic stuff.
				if ( StaticLightCacheAffectedByDynamicLight( *pLightcache ) )
				{
					pLightingState = LightcacheGetStatic( *pLightcache, &pEnvCubemapTexture );
					Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
				}
				else
				{
					pLightingState = LightcacheGetStatic( *pLightcache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
					Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
				}
				lightingState = *pLightingState;
			}
			else
			{
				// dx6 and dx7 case. . .hardware can either do software lighting or baked lighting only.
				if ( StaticLightCacheAffectedByDynamicLight( *pLightcache ) || 
					StaticLightCacheAffectedByAnimatedLightStyle( *pLightcache ) )
				{
					bStaticLighting = false;
				}
				else if ( StaticLightCacheNeedsSwitchableLightUpdate( *pLightcache ) )
				{
					// Need to rebake lighting since a switch has turned off/on.
					UpdateStaticPropColorData( state.m_pRenderable->GetIClientUnknown(), pInfo.instance );
				}
			}
		}

		if ( !bStaticLighting )
		{
			lightingState = *(LightcacheGetStatic( *pLightcache, &pEnvCubemapTexture ));
			Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
		}

		if ( r_decalstaticprops.GetBool() && pModelInst && drawInfo.m_bStaticLighting && bHasDecals )
		{
			for ( int iCube = 0; iCube < 6; ++iCube )
			{
				drawInfo.m_vecAmbientCube[iCube] = pModelInst->m_AmbientLightingState.r_boxcolor[iCube] + lightingState.r_boxcolor[iCube];
			}

			lightingDecalState.CopyLocalLights( pModelInst->m_AmbientLightingState );
			lightingDecalState.AddAllLocalLights( lightingState );

			Assert( lightingDecalState.numlights >= 0 && lightingDecalState.numlights <= MAXLOCALLIGHTS );
		}
	}
	else	// !pLightcache
	{
		vecDebugLightingOrigin = vLightingOrigin;
		pDebugLightingOrigin = &vecDebugLightingOrigin;

		// If we don't have a lightcache entry, but we have bStaticLighting, that means
		// that we are a prop_physics that has fallen asleep.
		if ( bStaticLighting )
		{
			LightcacheGetDynamic_Stats stats;
			pEnvCubemapTexture = LightcacheGetDynamic( vLightingOrigin, lightingState, 
				stats, LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
			Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );

			// Deal with all the dx6/dx7 issues (ie. can't do anything besides either baked lighting
			// or software dynamic lighting.
			if ( !g_pMaterialSystemHardwareConfig->SupportsStaticPlusDynamicLighting() )
			{
				if ( ( stats.m_bHasDLights || stats.m_bHasNonSwitchableLightStyles ) )
				{
					// We either have a light switch, or a dynamic light. . do it in software.
					// We'll reget the cache entry with different flags below.
					bStaticLighting = false;
				}
				else if ( stats.m_bNeedsSwitchableLightStyleUpdate )
				{
					// Need to rebake lighting since a switch has turned off/on.
					UpdateStaticPropColorData( state.m_pRenderable->GetIClientUnknown(), pInfo.instance );
				}
			}
		}

		if ( !bStaticLighting )
		{
			LightcacheGetDynamic_Stats stats;

			// For special r_drawlightcache mode, we only draw models containing the substring set in r_lightcachemodel
			bool bDebugModel = false;
			if( r_drawlightcache.GetInt() == 5 )
			{
				if ( pModelInst && pModelInst->m_pModel && !pModelInst->m_pModel->strName.IsEmpty() )
				{
					const char *szModelName = r_lightcachemodel.GetString();
					bDebugModel = V_stristr( pModelInst->m_pModel->strName, szModelName ) != NULL;
				}
			}
	
			pEnvCubemapTexture = LightcacheGetDynamic( vLightingOrigin, lightingState, stats, 
				LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST, bDebugModel );

			Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
		}
		
		if ( pInfo.pLightingOffset && !pInfo.pLightingOrigin )
		{
			for ( int i = 0; i < lightingState.numlights; ++i )
			{
				pSaveLightPos[i] = lightingState.locallight[i]->origin; 
				VectorITransform( pSaveLightPos[i], *pInfo.pLightingOffset, lightingState.locallight[i]->origin );
			}
		}

		// Cache lighting for decals.
		if ( pModelInst && drawInfo.m_bStaticLighting && bHasDecals )
		{
			// Only do this on dx8/dx9.
			LightcacheGetDynamic_Stats stats;
			LightcacheGetDynamic( vLightingOrigin, lightingDecalState, stats,
				LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST );

			Assert( lightingDecalState.numlights >= 0 && lightingDecalState.numlights <= MAXLOCALLIGHTS);
			
			for ( int iCube = 0; iCube < 6; ++iCube )
			{
				VectorCopy( lightingDecalState.r_boxcolor[iCube], drawInfo.m_vecAmbientCube[iCube] );
			}

			if ( pInfo.pLightingOffset && !pInfo.pLightingOrigin )
			{
				for ( int i = 0; i < lightingDecalState.numlights; ++i )
				{
					pSaveLightPos[i] = lightingDecalState.locallight[i]->origin; 
					VectorITransform( pSaveLightPos[i], *pInfo.pLightingOffset, lightingDecalState.locallight[i]->origin );
				}
			}
		}
	}

	Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
	
	// Do time averaging of the lighting state to avoid popping...
	LightingState_t *pState;
	if ( !bStaticLighting && !pLightcache )
	{
		pState = TimeAverageLightingState( pInfo.instance, &lightingState, pInfo.entity_index, pDebugLightingOrigin );
	}
	else
	{
		pState = &lightingState;
	}

	if ( bNeedsEnvCubemap && pEnvCubemapTexture )
	{
		pRenderContext->BindLocalCubemap( pEnvCubemapTexture );
	}

	if ( g_pMaterialSystemConfig->nFullbright == 1 )
	{
		pRenderContext->SetAmbientLight( 1.0, 1.0, 1.0 );

		static Vector white[6] = 
		{
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
		};

		g_pStudioRender->SetAmbientLightColors( white );

		// Disable all the lights..
		pRenderContext->DisableAllLocalLights();
	}
	else if ( bVertexLit )
	{
		if( drawFlags & STUDIORENDER_DRAW_ITEM_BLINK )
		{
			float add = r_itemblinkmax.GetFloat() * ( FastCos( r_itemblinkrate.GetFloat() * Sys_FloatTime() ) + 1.0f );
			Vector additiveColor( add, add, add );
			static Vector temp[6];
			int i;
			for( i = 0; i < 6; i++ )
			{
				temp[i] = pState->r_boxcolor[i] + additiveColor;
			}
			g_pStudioRender->SetAmbientLightColors( temp );
		}
		else
		{
			// If we have any lights and want to do ambient boost on this model
			if ( (pState->numlights > 0) && (pInfo.pModel->flags & MODELFLAG_STUDIOHDR_AMBIENT_BOOST) && r_ambientboost.GetBool() )
			{
				Vector lumCoeff( 0.3f, 0.59f, 0.11f );
				float avgCubeLuminance = 0.0f;
				float minCubeLuminance = FLT_MAX;
				float maxCubeLuminance = 0.0f;

				// Compute average luminance of ambient cube
				for( int i = 0; i < 6; i++ )
				{
					float luminance = DotProduct( pState->r_boxcolor[i], lumCoeff );	// compute luminance
					minCubeLuminance = fpmin(minCubeLuminance, luminance);				// min luminance
					maxCubeLuminance = fpmax(maxCubeLuminance, luminance);				// max luminance
					avgCubeLuminance += luminance;										// accumulate luminance
				}
				avgCubeLuminance /= 6.0f;												// average luminance

				// Compute the amount of direct light reaching the center of the model (attenuated by distance)
				float fDirectLight = 0.0f;
				for( int i = 0; i < pState->numlights; i++ )
				{
					Vector vLight = pState->locallight[i]->origin - vLightingOrigin;
					float d2 = DotProduct( vLight, vLight );
					float d = sqrtf( d2 );
					float fAtten = 1.0f;

					float denom = pState->locallight[i]->constant_attn +
								pState->locallight[i]->linear_attn * d +
								pState->locallight[i]->quadratic_attn * d2;

					if ( denom > 0.00001f )
					{
						fAtten = 1.0f / denom;
					}

					Vector vLit = pState->locallight[i]->intensity * fAtten;
					fDirectLight += DotProduct( vLit, lumCoeff );
				}

				// If ambient cube is sufficiently dim in absolute terms and ambient cube is swamped by direct lights
				if ( avgCubeLuminance < r_ambientmin.GetFloat() && (avgCubeLuminance < (fDirectLight * r_ambientfraction.GetFloat())) )	
				{
					Vector vFinalAmbientCube[6];
					float fBoostFactor =  min( (fDirectLight * r_ambientfraction.GetFloat()) / maxCubeLuminance, 5.f ); // boost no more than a certain factor
					for( int i = 0; i < 6; i++ )
					{
						vFinalAmbientCube[i] = pState->r_boxcolor[i] * fBoostFactor;
					}
					g_pStudioRender->SetAmbientLightColors( vFinalAmbientCube );		// Boost
				}
				else
				{
					g_pStudioRender->SetAmbientLightColors( pState->r_boxcolor );		// No Boost
				}
			}
			else // Don't bother with ambient boost, just use the ambient cube as is
			{
				g_pStudioRender->SetAmbientLightColors( pState->r_boxcolor );			// No Boost
			}
		}

		pRenderContext->SetAmbientLight( 0.0, 0.0, 0.0 );
		R_SetNonAmbientLightingState( pState->numlights, pState->locallight,
			                          &drawInfo.m_nLocalLightCount, &drawInfo.m_LocalLightDescs[0], true );

		// Cache lighting for decals.
		if( pModelInst && drawInfo.m_bStaticLighting && bHasDecals )
		{
			R_SetNonAmbientLightingState( lightingDecalState.numlights, lightingDecalState.locallight,
				                          &drawInfo.m_nLocalLightCount, &drawInfo.m_LocalLightDescs[0], false );
		}
	}

	if ( pInfo.pLightingOffset && !pInfo.pLightingOrigin )
	{
		for ( int i = 0; i < lightingState.numlights; ++i )
		{
			lightingState.locallight[i]->origin = pSaveLightPos[i];
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Uses this material instead of the one the model was compiled with
//-----------------------------------------------------------------------------

// FIXME: a duplicate of what's in CEngineTool::GetLightingConditions
int GetLightingConditions( const Vector &vecLightingOrigin, Vector *pColors, int nMaxLocalLights, LightDesc_t *pLocalLights, ITexture *&pEnvCubemapTexture )
{
#ifndef SWDS
	LightcacheGetDynamic_Stats stats;
	LightingState_t state;
	pEnvCubemapTexture = NULL;
	pEnvCubemapTexture = LightcacheGetDynamic( vecLightingOrigin, state, stats );
	Assert( state.numlights >= 0 && state.numlights <= MAXLOCALLIGHTS );
	memcpy( pColors, state.r_boxcolor, sizeof(state.r_boxcolor) );

	int nLightCount = 0;
	for ( int i = 0; i < state.numlights; ++i )
	{
		LightDesc_t *pLightDesc = &pLocalLights[nLightCount];
		if (!WorldLightToMaterialLight( state.locallight[i], *pLightDesc ))
			continue;

		// Apply lightstyle
		float bias = LightStyleValue( state.locallight[i]->style );

		// Deal with overbrighting + bias
		pLightDesc->m_Color[0] *= bias;
		pLightDesc->m_Color[1] *= bias;
		pLightDesc->m_Color[2] *= bias;

		if ( ++nLightCount >= nMaxLocalLights )
			break;
	}
	return nLightCount;
#endif
	return 0;
}

// FIXME: a duplicate of what's in CCDmeMdlRenderable<T>::SetUpLighting and CDmeEmitter::SetUpLighting
void CModelRender::SetupLighting( const Vector &vecCenter )
{
#ifndef SWDS
	// Set up lighting conditions
	Vector vecAmbient[6];
	Vector4D vecAmbient4D[6];
	LightDesc_t desc[2];
	ITexture *pEnvCubemapTexture = NULL;
	int nLightCount = GetLightingConditions( vecCenter, vecAmbient, 2, desc, pEnvCubemapTexture );
	int nMaxLights = g_pMaterialSystemHardwareConfig->MaxNumLights();
	if( nLightCount > nMaxLights )
	{
		nLightCount = nMaxLights;
	}

	int i;
	for( i = 0; i < 6; i++ )
	{
		VectorCopy( vecAmbient[i], vecAmbient4D[i].AsVector3D() );
		vecAmbient4D[i][3] = 1.0f;
	}
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetAmbientLightCube( vecAmbient4D );

	if ( pEnvCubemapTexture )
	{
		pRenderContext->BindLocalCubemap( pEnvCubemapTexture );
	}

	for( i = 0; i < nLightCount; i++ )
	{
		LightDesc_t *pLight = &desc[i];
		pLight->m_Flags = 0;
		if( pLight->m_Attenuation0 != 0.0f )
		{
			pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0;
		}
		if( pLight->m_Attenuation1 != 0.0f )
		{
			pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1;
		}
		if( pLight->m_Attenuation2 != 0.0f )
		{
			pLight->m_Flags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2;
		}

		pRenderContext->SetLight( i, desc[i] );
	}

	for( ; i < nMaxLights; i++ )
	{
		LightDesc_t disableDesc;
		disableDesc.m_Type = MATERIAL_LIGHT_DISABLE;
		pRenderContext->SetLight( i, disableDesc );
	}
#endif
}



//-----------------------------------------------------------------------------
// Uses this material instead of the one the model was compiled with
//-----------------------------------------------------------------------------
void CModelRender::ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	g_pStudioRender->ForcedMaterialOverride( newMaterial, nOverrideType );
}


//-----------------------------------------------------------------------------
// Sets up the render state for a model
//-----------------------------------------------------------------------------
matrix3x4_t* CModelRender::SetupModelState( IClientRenderable *pRenderable )
{
	const model_t *pModel = pRenderable->GetModel();
	if ( !pModel )
		return NULL;

	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( const_cast<model_t*>(pModel) );
	if ( pStudioHdr->numbodyparts == 0 )
		return NULL;

	matrix3x4_t *pBoneMatrices = NULL;
#ifndef SWDS
	// Set up skinning state
	Assert ( pRenderable );
	{
		int nBoneCount = pStudioHdr->numbones;
		pBoneMatrices = g_pStudioRender->LockBoneMatrices( pStudioHdr->numbones );
		pRenderable->SetupBones( pBoneMatrices, nBoneCount, BONE_USED_BY_ANYTHING, cl.GetTime() ); // hack hack
		g_pStudioRender->UnlockBoneMatrices();
	}
#endif

	return pBoneMatrices;
}


struct ModelDebugOverlayData_t
{
	DrawModelInfo_t m_ModelInfo;
	DrawModelResults_t m_ModelResults;
	Vector m_Origin;

	ModelDebugOverlayData_t() {}

private:
	ModelDebugOverlayData_t( const ModelDebugOverlayData_t &vOther );
};

static CUtlVector<ModelDebugOverlayData_t> s_SavedModelInfo;

void DrawModelDebugOverlay( const DrawModelInfo_t& info, const DrawModelResults_t &results, const Vector &origin, float r = 1.0f, float g = 1.0f, float b = 1.0f )
{
#ifndef SWDS
	float alpha = 1;
	if( r_drawmodelstatsoverlaydistance.GetFloat() == 1 )
	{
		alpha = 1.f - clamp( CurrentViewOrigin().DistTo( origin ) / r_drawmodelstatsoverlaydistance.GetFloat(), 0.f, 1.f );
	}
	else
	{
		float flDistance = CurrentViewOrigin().DistTo( origin );

		// The view model keeps throwing up its data and it looks like garbage, so I am trying to get rid of it.
		if ( flDistance < 36.0f )
			return;

		if ( flDistance > r_drawmodelstatsoverlaydistance.GetFloat() )
			return;
	}

	Assert( info.m_pStudioHdr );
	Assert( info.m_pStudioHdr->pszName() );
	Assert( info.m_pHardwareData );
	float duration = 0.0f;
	int lineOffset = 0;
	if( !info.m_pStudioHdr || !info.m_pStudioHdr->pszName() || !info.m_pHardwareData )
	{
		CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, "This model has problems. . see a programmer." );
		return;
	}

	char buf[1024];
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, info.m_pStudioHdr->pszName() );
	Q_snprintf( buf, sizeof( buf ), "lod: %d/%d\n", results.m_nLODUsed+1, ( int )info.m_pHardwareData->m_NumLODs );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );
	Q_snprintf( buf, sizeof( buf ), "tris: %d\n",  results.m_ActualTriCount );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );
	Q_snprintf( buf, sizeof( buf ), "hardware bones: %d\n",  results.m_NumHardwareBones );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		
	Q_snprintf( buf, sizeof( buf ), "num batches: %d\n",  results.m_NumBatches );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		
	Q_snprintf( buf, sizeof( buf ), "has shadow lod: %s\n", ( info.m_pStudioHdr->flags & STUDIOHDR_FLAGS_HASSHADOWLOD ) ? "true" : "false" );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		
	Q_snprintf( buf, sizeof( buf ), "num materials: %d\n", results.m_NumMaterials );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		

	int i;
	for( i = 0; i < results.m_Materials.Count(); i++ )
	{
		IMaterial *pMaterial = results.m_Materials[i];
		if( pMaterial )
		{
			int numPasses = pMaterial->GetNumPasses();
			Q_snprintf( buf, sizeof( buf ), "\t%s (%d %s)\n", results.m_Materials[i]->GetName(), numPasses, numPasses > 1 ? "passes" : "pass" );
			CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		
		}
	}
	if( results.m_Materials.Count() > results.m_NumMaterials )
	{
		CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, "(Remaining materials not shown)\n" );		
	}
	if( r_drawmodelstatsoverlay.GetInt() == 2 )
	{
		Q_snprintf( buf, sizeof( buf ), "Render Time: %0.1f ms\n", results.m_RenderTime.GetDuration().GetMillisecondsF());
		CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );
	}

	//Q_snprintf( buf, sizeof( buf ), "Render Time: %0.1f ms\n", info.m_pClientEntity 
#endif
}

void AddModelDebugOverlay( const DrawModelInfo_t& info, const DrawModelResults_t &results, const Vector& origin )
{
	ModelDebugOverlayData_t &tmp = s_SavedModelInfo[s_SavedModelInfo.AddToTail()];
	tmp.m_ModelInfo = info;
	tmp.m_ModelResults = results;
	tmp.m_Origin = origin;
}

void ClearSaveModelDebugOverlays( void )
{
	s_SavedModelInfo.RemoveAll();
}

int SavedModelInfo_Compare_f( const void *l, const void *r )
{
	ModelDebugOverlayData_t *left = ( ModelDebugOverlayData_t * )l;
	ModelDebugOverlayData_t *right = ( ModelDebugOverlayData_t * )r;
	return left->m_ModelResults.m_RenderTime.GetDuration().GetSeconds() < right->m_ModelResults.m_RenderTime.GetDuration().GetSeconds();
}

static ConVar r_drawmodelstatsoverlaymin( "r_drawmodelstatsoverlaymin", "0.1", FCVAR_ARCHIVE, "time in milliseconds that a model must take to render before showing an overlay in r_drawmodelstatsoverlay 2" );
static ConVar r_drawmodelstatsoverlaymax( "r_drawmodelstatsoverlaymax", "1.5", FCVAR_ARCHIVE, "time in milliseconds beyond which a model overlay is fully red in r_drawmodelstatsoverlay 2" );

void DrawSavedModelDebugOverlays( void )
{
	if( s_SavedModelInfo.Count() == 0 )
	{
		return;
	}
	float min = r_drawmodelstatsoverlaymin.GetFloat();
	float max = r_drawmodelstatsoverlaymax.GetFloat();
	float ooRange = 1.0f / ( max - min );

	int i;
	for( i = 0; i < s_SavedModelInfo.Count(); i++ )
	{
		float r, g, b;
		float t = s_SavedModelInfo[i].m_ModelResults.m_RenderTime.GetDuration().GetMillisecondsF();
		if( t > min )
		{
			if( t >= max )
			{
				r = 1.0f; g = 0.0f; b = 0.0f;
			}
			else
			{
				r = ( t - min ) * ooRange;
				g = 1.0f - r;
				b = 0.0f;
			}
			DrawModelDebugOverlay( s_SavedModelInfo[i].m_ModelInfo, s_SavedModelInfo[i].m_ModelResults, s_SavedModelInfo[i].m_Origin, r, g, b );
		}
	}
	ClearSaveModelDebugOverlays();
}

void CModelRender::DebugDrawLightingOrigin( const DrawModelState_t& state, const ModelRenderInfo_t &pInfo )
{
#ifndef SWDS
	// determine light origin in world space
	Vector illumPosition;
	Vector lightOrigin;
	if ( pInfo.pLightingOrigin )
	{
		illumPosition = *pInfo.pLightingOrigin;
		lightOrigin = illumPosition;
	}
	else
	{
		R_ComputeLightingOrigin( state.m_pRenderable, state.m_pStudioHdr, *state.m_pModelToWorld, illumPosition );
		lightOrigin = illumPosition;
		if ( pInfo.pLightingOffset )
		{
			VectorTransform( illumPosition, *pInfo.pLightingOffset, lightOrigin );
		}
	}

	// draw z planar cross at lighting origin
	Vector pt0;
	Vector pt1;
	pt0    = lightOrigin;
	pt1    = lightOrigin;
	pt0.x -= 4;
	pt1.x += 4;
	CDebugOverlay::AddLineOverlay( pt0, pt1, 0, 255, 0, 255, true, 0.0f );
	pt0    = lightOrigin;
	pt1    = lightOrigin;
	pt0.y -= 4;
	pt1.y += 4;
	CDebugOverlay::AddLineOverlay( pt0, pt1, 0, 255, 0, 255, true, 0.0f );

	// draw lines from the light origin to the hull boundaries to identify model
	Vector pt;
	pt0.x = state.m_pStudioHdr->hull_min.x;
	pt0.y = state.m_pStudioHdr->hull_min.y;
	pt0.z = state.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_min.x;
	pt0.y = state.m_pStudioHdr->hull_max.y;
	pt0.z = state.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_max.x;
	pt0.y = state.m_pStudioHdr->hull_max.y;
	pt0.z = state.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_max.x;
	pt0.y = state.m_pStudioHdr->hull_min.y;
	pt0.z = state.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );

	pt0.x = state.m_pStudioHdr->hull_min.x;
	pt0.y = state.m_pStudioHdr->hull_min.y;
	pt0.z = state.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_min.x;
	pt0.y = state.m_pStudioHdr->hull_max.y;
	pt0.z = state.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_max.x;
	pt0.y = state.m_pStudioHdr->hull_max.y;
	pt0.z = state.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_max.x;
	pt0.y = state.m_pStudioHdr->hull_min.y;
	pt0.z = state.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );	
#endif
}
//-----------------------------------------------------------------------------
// Actually renders the model
//-----------------------------------------------------------------------------
void CModelRender::DrawModelExecute( const DrawModelState_t &state, const ModelRenderInfo_t &pInfo, matrix3x4_t *pBoneToWorld )
{
#ifndef SWDS
	bool bShadowDepth = (pInfo.flags & STUDIO_SHADOWDEPTHTEXTURE) != 0;
	bool bSSAODepth = ( pInfo.flags & STUDIO_SSAODEPTHTEXTURE ) != 0;

	// Bail if we're rendering into shadow depth map and this model doesn't cast shadows
	if ( bShadowDepth && ( ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_DO_NOT_CAST_SHADOWS ) != 0 ) )
		return;

	// Shadow state...
	g_pShadowMgr->SetModelShadowState( pInfo.instance );

	if ( g_bTextMode )
		return;

	// Sets up flexes
	float *pFlexWeights = NULL;
	float *pFlexDelayedWeights = NULL;
	int nFlexCount = state.m_pStudioHdr->numflexdesc;
	if ( nFlexCount > 0 )
	{
		// Does setup for flexes
		Assert( pBoneToWorld );
		bool bUsesDelayedWeights = state.m_pRenderable->UsesFlexDelayedWeights();
		g_pStudioRender->LockFlexWeights( nFlexCount, &pFlexWeights, bUsesDelayedWeights ? &pFlexDelayedWeights : NULL );
		state.m_pRenderable->SetupWeights( pBoneToWorld, nFlexCount, pFlexWeights, pFlexDelayedWeights );
		g_pStudioRender->UnlockFlexWeights();
	}

	// OPTIMIZE: Try to precompute part of this mess once a frame at the very least.
	bool bUsesBumpmapping = ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80 ) && ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_USES_BUMPMAPPING );

	bool bStaticLighting = ( state.m_drawFlags & STUDIORENDER_DRAW_STATIC_LIGHTING ) &&
								( state.m_pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) && 
								( !bUsesBumpmapping ) && 
								( pInfo.instance != MODEL_INSTANCE_INVALID ) &&
								g_pMaterialSystemHardwareConfig->SupportsColorOnSecondStream();

	bool bVertexLit = ( pInfo.pModel->flags & MODELFLAG_VERTEXLIT ) != 0;

	bool bNeedsEnvCubemap = r_showenvcubemap.GetInt() || ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP );
	
	if ( r_drawmodellightorigin.GetBool() && !bShadowDepth && !bSSAODepth )
	{
		DebugDrawLightingOrigin( state, pInfo );
	}

	ColorMeshInfo_t *pColorMeshes = NULL;
	DataCacheHandle_t hColorMeshData = DC_INVALID_HANDLE;
	if ( bStaticLighting )
	{
		// have static lighting, get from cache
		hColorMeshData = m_ModelInstances[pInfo.instance].m_ColorMeshHandle;
		CColorMeshData *pColorMeshData = CacheGet( hColorMeshData );
		if ( !pColorMeshData || pColorMeshData->m_bNeedsRetry )
		{
			// color meshes are not present, try to re-establish
			if ( RecomputeStaticLighting( pInfo.instance ) )
			{
				pColorMeshData = CacheGet( hColorMeshData );
			}
			else if ( !pColorMeshData || !pColorMeshData->m_bNeedsRetry )
			{
				// can't draw
				return;
			}
		}

		if ( pColorMeshData && ( pColorMeshData->m_bColorMeshValid || pColorMeshData->m_bColorTextureValid ) )
		{
			pColorMeshes = pColorMeshData->m_pMeshInfos;
			if (pColorMeshData->m_bColorTextureValid && !pColorMeshData->m_bColorTextureCreated)
			{
				CreateLightmapsFromData(pColorMeshData);
			}
		}
		else
		{
			// failed, draw without static lighting
			bStaticLighting = false;
		}
	}

	DrawModelInfo_t info;
	info.m_bStaticLighting = false;

	// get lighting from ambient light sources and radiosity bounces
	// also set up the env_cubemap from the light cache if necessary.
	if ( ( bVertexLit || bNeedsEnvCubemap ) && !bShadowDepth && !bSSAODepth )
	{
		// See if we're using static lighting
		LightCacheHandle_t* pLightCache = NULL;
		if ( pInfo.instance != MODEL_INSTANCE_INVALID )
		{
			if ( ( m_ModelInstances[pInfo.instance].m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING ) && m_ModelInstances[pInfo.instance].m_LightCacheHandle )
			{
				pLightCache = &m_ModelInstances[pInfo.instance].m_LightCacheHandle;
			}
		}

		// Choose the lighting origin
		Vector entOrigin;
		R_ComputeLightingOrigin( state.m_pRenderable, state.m_pStudioHdr, *state.m_pModelToWorld, entOrigin );

		// Set up lighting based on the lighting origin
		StudioSetupLighting( state, entOrigin, pLightCache, bVertexLit, bNeedsEnvCubemap, bStaticLighting, info, pInfo, state.m_drawFlags );
	}

	// Set up the camera state
	g_pStudioRender->SetViewState( CurrentViewOrigin(), CurrentViewRight(), CurrentViewUp(), CurrentViewForward() );

	// Color + alpha modulation
	g_pStudioRender->SetColorModulation( r_colormod );
	g_pStudioRender->SetAlphaModulation( r_blend );

	Assert( modelloader->IsLoaded( pInfo.pModel ) );
	info.m_pStudioHdr = state.m_pStudioHdr;
	info.m_pHardwareData = state.m_pStudioHWData;
	info.m_Skin = pInfo.skin;
	info.m_Body = pInfo.body;
	info.m_HitboxSet = pInfo.hitboxset;
	info.m_pClientEntity = (void*)state.m_pRenderable;
	info.m_Lod = state.m_lod;
	info.m_pColorMeshes = pColorMeshes;

	// Don't do decals if shadow depth mapping...
	info.m_Decals = ( bShadowDepth || bSSAODepth ) ? STUDIORENDER_DECAL_INVALID : state.m_decals;

	// Get perf stats if we are going to use them.
	int overlayVal = r_drawmodelstatsoverlay.GetInt();
	int drawFlags = state.m_drawFlags;

	if ( bShadowDepth )
	{
		drawFlags |= STUDIORENDER_DRAW_OPAQUE_ONLY;
		drawFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE;
	}

	if ( bSSAODepth == true )
	{
		drawFlags |= STUDIORENDER_DRAW_OPAQUE_ONLY;
		drawFlags |= STUDIORENDER_SSAODEPTHTEXTURE;
	}

	if ( overlayVal && !bShadowDepth && !bSSAODepth )
	{
		drawFlags |= STUDIORENDER_DRAW_GET_PERF_STATS;
	}

	if ( ( pInfo.flags & STUDIO_GENERATE_STATS ) != 0 )
	{
		drawFlags |= STUDIORENDER_GENERATE_STATS;
	}

	DrawModelResults_t results;
	g_pStudioRender->DrawModel( &results, info, pBoneToWorld, pFlexWeights, 
		pFlexDelayedWeights, pInfo.origin, drawFlags );
	info.m_Lod = results.m_nLODUsed;

	if ( overlayVal && !bShadowDepth && !bSSAODepth )
	{
		if ( overlayVal != 2 )
		{
			DrawModelDebugOverlay( info, results, pInfo.origin );
		}
		else
		{
			AddModelDebugOverlay( info, results, pInfo.origin );
		}
	}

	if ( pColorMeshes)
	{
		ProtectColorDataIfQueued( hColorMeshData );
	}

#endif
}

//-----------------------------------------------------------------------------
// Main entry point for model rendering in the engine
//-----------------------------------------------------------------------------
int CModelRender::DrawModel( 	
	int flags,
	IClientRenderable *pRenderable,
	ModelInstanceHandle_t instance,
	int entity_index, 
	const model_t *pModel, 
	const Vector &origin,
	const QAngle &angles,
	int skin,
	int body,
	int hitboxset,
	const matrix3x4_t* pModelToWorld,
	const matrix3x4_t *pLightingOffset )
{
	ModelRenderInfo_t sInfo;
	sInfo.flags = flags;
	sInfo.pRenderable = pRenderable;
	sInfo.instance = instance;
	sInfo.entity_index = entity_index;
	sInfo.pModel = pModel;
	sInfo.origin = origin;
	sInfo.angles = angles;
	sInfo.skin = skin;
	sInfo.body = body;
	sInfo.hitboxset = hitboxset;
	sInfo.pModelToWorld = pModelToWorld;
	sInfo.pLightingOffset = pLightingOffset;

	if ( (r_entity.GetInt() == -1) || (r_entity.GetInt() == entity_index) )
	{
		return DrawModelEx( sInfo );
	}

	return 0;
}

int	CModelRender::ComputeLOD( const ModelRenderInfo_t &info, studiohwdata_t *pStudioHWData )
{
	int lod = r_lod.GetInt();
	// FIXME!!!  This calc should be in studiorender, not here!!!!!  But since the bone setup
	// is done here, and we need the bone mask, we'll do it here for now.
	if ( lod == -1 )
	{
		CMatRenderContextPtr pRenderContext( materials );
		float screenSize = pRenderContext->ComputePixelWidthOfSphere(info.pRenderable->GetRenderOrigin(), 0.5f );
		float metric = pStudioHWData->LODMetric(screenSize);
		lod = pStudioHWData->GetLODForMetric(metric);
	}
	else
	{
		if ( ( info.flags & STUDIOHDR_FLAGS_HASSHADOWLOD ) && ( lod > pStudioHWData->m_NumLODs - 2 ) )
		{
			lod = pStudioHWData->m_NumLODs - 2;
		}
		else if ( lod > pStudioHWData->m_NumLODs - 1 )
		{
			lod = pStudioHWData->m_NumLODs - 1;
		}
		else if( lod < 0 )
		{
			lod = 0;
		}
	}

	if ( lod < 0 )
	{
		lod = 0;
	}
	else if ( lod >= pStudioHWData->m_NumLODs )
	{
		lod = pStudioHWData->m_NumLODs - 1;
	}

	// clamp to root lod
	if (lod < pStudioHWData->m_RootLOD)
	{
		lod = pStudioHWData->m_RootLOD;
	}

	Assert( lod >= 0 && lod < pStudioHWData->m_NumLODs );
	return lod;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pInfo - 
//-----------------------------------------------------------------------------
bool CModelRender::DrawModelSetup( ModelRenderInfo_t &pInfo, DrawModelState_t *pState, matrix3x4_t *pCustomBoneToWorld, matrix3x4_t** ppBoneToWorldOut )
{
	*ppBoneToWorldOut = NULL;

#ifdef SWDS
	return false;
#endif

#if _DEBUG
	if ( (char*)pInfo.pRenderable < (char*)1024 )
	{
		Error( "CModelRender::DrawModel: pRenderable == 0x%p", pInfo.pRenderable );
	}
#endif

	// Can only deal with studio models
	Assert( pInfo.pModel->type == mod_studio );
	Assert( modelloader->IsLoaded( pInfo.pModel ) );

	DrawModelState_t &state = *pState;
	state.m_pStudioHdr = g_pMDLCache->GetStudioHdr( pInfo.pModel->studio );
	state.m_pRenderable = pInfo.pRenderable;

	// Quick exit if we're just supposed to draw a specific model...
	if ( (r_entity.GetInt() != -1) && (r_entity.GetInt() != pInfo.entity_index) )
		return false;

	// quick exit
	if (state.m_pStudioHdr->numbodyparts == 0)
		return false;

	if ( !pInfo.pModelToWorld )
	{
		Assert( 0 );
		return false;
	}

	state.m_pModelToWorld = pInfo.pModelToWorld;

	Assert ( pInfo.pRenderable );

	state.m_pStudioHWData = g_pMDLCache->GetHardwareData( pInfo.pModel->studio );
	if ( !state.m_pStudioHWData )
		return false;

	state.m_lod = ComputeLOD( pInfo, state.m_pStudioHWData );
	
	int boneMask = BONE_USED_BY_VERTEX_AT_LOD( state.m_lod );
	// Why isn't this always set?!?

	if ( ( pInfo.flags & STUDIO_RENDER ) == 0 )
	{
		// no rendering, just force a bone setup.  Don't copy the bones
		bool bOk = pInfo.pRenderable->SetupBones( NULL, MAXSTUDIOBONES, boneMask, cl.GetTime() );
		return bOk;
	}

	int nBoneCount = state.m_pStudioHdr->numbones;
	matrix3x4_t *pBoneToWorld = pCustomBoneToWorld;
	if ( !pCustomBoneToWorld )
	{
		pBoneToWorld = g_pStudioRender->LockBoneMatrices( nBoneCount );
	}
	const bool bOk = pInfo.pRenderable->SetupBones( pBoneToWorld, nBoneCount, boneMask, cl.GetTime() );
	if ( !pCustomBoneToWorld )
	{
		g_pStudioRender->UnlockBoneMatrices();
	}
	if ( !bOk )
		return false;

	*ppBoneToWorldOut = pBoneToWorld;

	// Convert the instance to a decal handle.
	state.m_decals = STUDIORENDER_DECAL_INVALID;
	if (pInfo.instance != MODEL_INSTANCE_INVALID)
	{
		state.m_decals = m_ModelInstances[pInfo.instance].m_DecalHandle;
	}

	state.m_drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL;
	if ( pInfo.flags & STUDIO_TWOPASS )
	{
		if (pInfo.flags & STUDIO_TRANSPARENCY)
		{
			state.m_drawFlags = STUDIORENDER_DRAW_TRANSLUCENT_ONLY; 
		}
		else
		{
			state.m_drawFlags = STUDIORENDER_DRAW_OPAQUE_ONLY; 
		}
	}
	if ( pInfo.flags & STUDIO_STATIC_LIGHTING )
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_STATIC_LIGHTING;
	}
	
	if( pInfo.flags & STUDIO_ITEM_BLINK )
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_ITEM_BLINK;
	}

	if ( pInfo.flags & STUDIO_WIREFRAME )
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_WIREFRAME;
	}

	if ( pInfo.flags & STUDIO_NOSHADOWS )
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_NO_SHADOWS;
	}

	if ( r_drawmodelstatsoverlay.GetInt() == 2)
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_ACCURATETIME;
	}

	if ( pInfo.flags & STUDIO_SHADOWDEPTHTEXTURE )
	{
		state.m_drawFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE;
	}

	return true;
}

int	CModelRender::DrawModelEx( ModelRenderInfo_t &pInfo )
{
#ifndef SWDS
	DrawModelState_t state;

	matrix3x4_t tmpmat;
	if ( !pInfo.pModelToWorld )
	{
		pInfo.pModelToWorld = &tmpmat;

		// Turns the origin + angles into a matrix
		AngleMatrix( pInfo.angles, pInfo.origin, tmpmat );
	}

	matrix3x4_t *pBoneToWorld;
	if ( !DrawModelSetup( pInfo, &state, NULL, &pBoneToWorld ) )
		return 0;

	if ( pInfo.flags & STUDIO_RENDER )
	{
		DrawModelExecute( state, pInfo, pBoneToWorld );
	}

	return 1;
#else
	return 0;
#endif
}

int	CModelRender::DrawModelExStaticProp( ModelRenderInfo_t &pInfo )
{
#ifndef SWDS
	bool bShadowDepth = ( pInfo.flags & STUDIO_SHADOWDEPTHTEXTURE ) != 0;

#if _DEBUG
	if ( (char*)pInfo.pRenderable < (char*)1024 )
	{
		Error( "CModelRender::DrawModel: pRenderable == %p", pInfo.pRenderable );
	}	

	// Can only deal with studio models
	if ( pInfo.pModel->type != mod_studio )
		return 0;
#endif
	Assert( modelloader->IsLoaded( pInfo.pModel ) );

	DrawModelState_t state;
	state.m_pStudioHdr = g_pMDLCache->GetStudioHdr( pInfo.pModel->studio );
	state.m_pRenderable = pInfo.pRenderable;

	// quick exit
	if ( state.m_pStudioHdr->numbodyparts == 0 || g_bTextMode )
		return 1;

	state.m_pStudioHWData = g_pMDLCache->GetHardwareData( pInfo.pModel->studio );
	if ( !state.m_pStudioHWData )
		return 0;

	Assert( pInfo.pModelToWorld );
	state.m_pModelToWorld = pInfo.pModelToWorld;
	Assert ( pInfo.pRenderable );

	int lod = ComputeLOD( pInfo, state.m_pStudioHWData );
	// int boneMask = BONE_USED_BY_VERTEX_AT_LOD( lod );
	// Why isn't this always set?!?
	if ( !(pInfo.flags & STUDIO_RENDER) )
		return 0;

	// Convert the instance to a decal handle.
	StudioDecalHandle_t decalHandle = STUDIORENDER_DECAL_INVALID;
	if ( (pInfo.instance != MODEL_INSTANCE_INVALID) && !(pInfo.flags & STUDIO_SHADOWDEPTHTEXTURE) )
	{
		decalHandle = m_ModelInstances[pInfo.instance].m_DecalHandle;
	}

	int drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL;
	if ( pInfo.flags & STUDIO_TWOPASS )
	{
		if ( pInfo.flags & STUDIO_TRANSPARENCY )
		{
			drawFlags = STUDIORENDER_DRAW_TRANSLUCENT_ONLY; 
		}
		else
		{
			drawFlags = STUDIORENDER_DRAW_OPAQUE_ONLY; 
		}
	}

	if ( pInfo.flags & STUDIO_STATIC_LIGHTING )
	{
		drawFlags |= STUDIORENDER_DRAW_STATIC_LIGHTING;
	}

	if ( pInfo.flags & STUDIO_WIREFRAME )
	{
		drawFlags |= STUDIORENDER_DRAW_WIREFRAME;
	}

	if ( pInfo.flags & STUDIO_GENERATE_STATS )
	{
		drawFlags |= STUDIORENDER_GENERATE_STATS;
	}

	// Shadow state...
	g_pShadowMgr->SetModelShadowState( pInfo.instance );

	// OPTIMIZE: Try to precompute part of this mess once a frame at the very least.
	bool bUsesBumpmapping = ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 80 ) && ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_USES_BUMPMAPPING );

	bool bStaticLighting = (( drawFlags & STUDIORENDER_DRAW_STATIC_LIGHTING ) &&
		( state.m_pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) && 
		( !bUsesBumpmapping ) && 
		( pInfo.instance != MODEL_INSTANCE_INVALID ) &&
		g_pMaterialSystemHardwareConfig->SupportsColorOnSecondStream() );

	bool bVertexLit = ( pInfo.pModel->flags & MODELFLAG_VERTEXLIT ) != 0;
	bool bNeedsEnvCubemap = r_showenvcubemap.GetInt() || ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP );

	if ( r_drawmodellightorigin.GetBool() )
	{
		DebugDrawLightingOrigin( state, pInfo );
	}

	ColorMeshInfo_t *pColorMeshes = NULL;
	DataCacheHandle_t hColorMeshData = DC_INVALID_HANDLE;
	if ( bStaticLighting )
	{
		// have static lighting, get from cache
		hColorMeshData = m_ModelInstances[pInfo.instance].m_ColorMeshHandle;
		CColorMeshData *pColorMeshData = CacheGet( hColorMeshData );
		if ( !pColorMeshData || pColorMeshData->m_bNeedsRetry )
		{
			// color meshes are not present, try to re-establish
			if ( RecomputeStaticLighting( pInfo.instance ) )
			{
				pColorMeshData = CacheGet( hColorMeshData );
			}
			else if ( !pColorMeshData || !pColorMeshData->m_bNeedsRetry )
			{
				// can't draw
				return 0;
			}
		}

		if ( pColorMeshData && ( pColorMeshData->m_bColorMeshValid || pColorMeshData->m_bColorTextureValid ) )
		{
			pColorMeshes = pColorMeshData->m_pMeshInfos;
			if (pColorMeshData->m_bColorTextureValid && !pColorMeshData->m_bColorTextureCreated)
			{
				CreateLightmapsFromData(pColorMeshData);
			}
		}
		else
		{
			// failed, draw without static lighting
			bStaticLighting = false;
		}
	}

	DrawModelInfo_t info;
	info.m_bStaticLighting = false;

	// Get lighting from ambient light sources and radiosity bounces
	// also set up the env_cubemap from the light cache if necessary.
	// Don't bother if we're rendering to shadow depth texture
	if ( ( bVertexLit || bNeedsEnvCubemap ) && !bShadowDepth )
	{
		// See if we're using static lighting
		LightCacheHandle_t* pLightCache = NULL;
		if ( pInfo.instance != MODEL_INSTANCE_INVALID )
		{
			if ( ( m_ModelInstances[pInfo.instance].m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING ) && m_ModelInstances[pInfo.instance].m_LightCacheHandle )
			{
				pLightCache = &m_ModelInstances[pInfo.instance].m_LightCacheHandle;
			}
		}

		// Choose the lighting origin
		Vector entOrigin;
		if ( !pLightCache )
		{
			R_ComputeLightingOrigin( state.m_pRenderable, state.m_pStudioHdr, *state.m_pModelToWorld, entOrigin );
		}

		// Set up lighting based on the lighting origin
		StudioSetupLighting( state, entOrigin, pLightCache, bVertexLit, bNeedsEnvCubemap, bStaticLighting, info, pInfo, drawFlags );
	}

	Assert( modelloader->IsLoaded( pInfo.pModel ) );
	info.m_pStudioHdr = state.m_pStudioHdr;
	info.m_pHardwareData = state.m_pStudioHWData;
	info.m_Decals = decalHandle;
	info.m_Skin = pInfo.skin;
	info.m_Body = pInfo.body;
	info.m_HitboxSet = pInfo.hitboxset;
	info.m_pClientEntity = (void*)state.m_pRenderable;
	info.m_Lod = lod;
	info.m_pColorMeshes = pColorMeshes;

	if ( bShadowDepth )
	{
		drawFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE;
	}

#ifdef _DEBUG
	Vector tmp;
	MatrixGetColumn( *pInfo.pModelToWorld, 3, &tmp );
	Assert( VectorsAreEqual( pInfo.origin, tmp, 1e-3 ) );
#endif

	g_pStudioRender->DrawModelStaticProp( info, *pInfo.pModelToWorld, drawFlags );

	if ( pColorMeshes)
	{
		ProtectColorDataIfQueued( hColorMeshData );
	}

	return 1;
#else
	return 0;
#endif
}

struct robject_t
{
	const matrix3x4_t	*pMatrix;
	IClientRenderable	*pRenderable;
	ColorMeshInfo_t		*pColorMeshes;
	ITexture			*pEnvCubeMap;
	Vector				*pLightingOrigin;
	short				modelIndex;
	short				lod;
	ModelInstanceHandle_t instance;
	short				skin;
	short				lightIndex;
	short				pad0;
};

struct rmodel_t
{
	const model_t *			pModel;
	studiohdr_t*			pStudioHdr;
	studiohwdata_t*			pStudioHWData;
	float					maxArea;
	short					lodStart;
	byte					lodCount;
	byte					bVertexLit : 1;
	byte					bNeedsCubemap : 1;
	byte					bStaticLighting : 1;
};

class CRobjectLess
{
public:
	bool Less( const robject_t& lhs, const robject_t& rhs, void *pContext )
	{
		rmodel_t *pModels = static_cast<rmodel_t *>(pContext);
		if ( lhs.modelIndex == rhs.modelIndex )
		{
			if ( lhs.skin != rhs.skin )
				return lhs.skin < rhs.skin;
			return lhs.lod < rhs.lod;
		}
		if ( pModels[lhs.modelIndex].maxArea == pModels[rhs.modelIndex].maxArea )
			return lhs.modelIndex < rhs.modelIndex;
		return pModels[lhs.modelIndex].maxArea > pModels[rhs.modelIndex].maxArea;
	}
};

struct rdecalmodel_t
{
	short			objectIndex;
	short			lightIndex;
};
/*
// ----------------------------------------
// not yet implemented

struct rlod_t
{
	short groupCount;
	short groupStart;
};

struct rgroup_t
{
	IMesh	*pMesh;
	short	batchCount;
	short	batchStart;
	short	colorMeshIndex;
	short	pad0;
};

struct rbatch_t
{
	IMaterial		*pMaterial;
	short			primitiveType;
	short			pad0;
	unsigned short	indexOffset;
	unsigned short	indexCount;
};
// ----------------------------------------
*/

inline int FindModel( const CUtlVector<rmodel_t> &list, const model_t *pModel )
{
	for ( int j = list.Count(); --j >= 0 ; )
	{
		if ( list[j].pModel == pModel )
			return j;
	}
	return -1;
}


// NOTE: UNDONE: This is a work in progress of a new static prop rendering pipeline
// UNDONE: Expose drawing commands from studiorender and draw here
// UNDONE: Build a similar pipeline for non-static prop models
// UNDONE: Split this into several functions in a sub-object
ConVar r_staticprop_lod("r_staticprop_lod", "-1");
int CModelRender::DrawStaticPropArrayFast( StaticPropRenderInfo_t *pProps, int count, bool bShadowDepth )
{
#ifndef SWDS
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
	CMatRenderContextPtr pRenderContext( materials );
	const int MAX_OBJECTS = 1024;
	CUtlSortVector<robject_t, CRobjectLess> objectList(0, MAX_OBJECTS);
	CUtlVector<rmodel_t> modelList(0,256);
	CUtlVector<short> lightObjects(0,256);
	CUtlVector<short> shadowObjects(0,64);
	CUtlVector<rdecalmodel_t> decalObjects(0,64);
	CUtlVector<LightingState_t> lightStates(0,256);
	bool bForceCubemap = r_showenvcubemap.GetBool();
	int drawnCount = 0;
	int forcedLodSetting = r_lod.GetInt();
	if ( r_staticprop_lod.GetInt() >= 0 )
	{
		forcedLodSetting = r_staticprop_lod.GetInt();
	}

	// build list of objects and unique models
	for ( int i = 0; i < count; i++ )
	{
		drawnCount++;
		// UNDONE: This is a perf hit in some scenes!  Use a hash?
		int modelIndex = FindModel( modelList, pProps[i].pModel );
		if ( modelIndex < 0 )
		{
			modelIndex = modelList.AddToTail();
			modelList[modelIndex].pModel = pProps[i].pModel;
			modelList[modelIndex].pStudioHWData = g_pMDLCache->GetHardwareData( modelList[modelIndex].pModel->studio );
		}
		if ( modelList[modelIndex].pStudioHWData )
		{
			robject_t obj;
			obj.pMatrix = pProps[i].pModelToWorld;
			obj.pRenderable = pProps[i].pRenderable;
			obj.modelIndex = modelIndex;
			obj.instance = pProps[i].instance;
			obj.skin = pProps[i].skin;
			obj.lod = 0;
			obj.pColorMeshes = NULL;
			obj.pEnvCubeMap = NULL;
			obj.lightIndex = -1;
			obj.pLightingOrigin = pProps[i].pLightingOrigin;
			objectList.InsertNoSort(obj);
		}
	}

	// process list of unique models
	int lodStart = 0;
	for ( int i = 0; i < modelList.Count(); i++ )
	{
		const model_t *pModel = modelList[i].pModel;
		Assert( modelloader->IsLoaded( pModel ) );
		unsigned int flags = pModel->flags;
		modelList[i].pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );
		modelList[i].maxArea = 1.0f;
		modelList[i].lodStart = lodStart;
		modelList[i].lodCount = modelList[i].pStudioHWData ? modelList[i].pStudioHWData->m_NumLODs : 0;
		bool bBumpMapped = (flags & MODELFLAG_STUDIOHDR_USES_BUMPMAPPING) != 0;
		modelList[i].bStaticLighting = (( modelList[i].pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) != 0) && !bBumpMapped;
		modelList[i].bVertexLit = ( flags & MODELFLAG_VERTEXLIT ) != 0;
		modelList[i].bNeedsCubemap = ( flags & MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP ) != 0;

		lodStart += modelList[i].lodCount;
	}

	// -1 is automatic lod
	if ( forcedLodSetting < 0 )
	{
		// compute the lod of each object
		for ( int i = 0; i < objectList.Count(); i++ )
		{
			Vector org;
			MatrixGetColumn( *objectList[i].pMatrix, 3, org );
			float screenSize = pRenderContext->ComputePixelWidthOfSphere(org, 0.5f );
			const rmodel_t &model = modelList[objectList[i].modelIndex];
			float metric = model.pStudioHWData->LODMetric(screenSize);
			objectList[i].lod = model.pStudioHWData->GetLODForMetric(metric);
			if ( objectList[i].lod < model.pStudioHWData->m_RootLOD )
			{
				objectList[i].lod = model.pStudioHWData->m_RootLOD;
			}
			modelList[objectList[i].modelIndex].maxArea = max(modelList[objectList[i].modelIndex].maxArea, screenSize);
		}
	}
	else
	{
		// force the lod of each object
		for ( int i = 0; i < objectList.Count(); i++ )
		{
			const rmodel_t &model = modelList[objectList[i].modelIndex];
			objectList[i].lod = clamp(forcedLodSetting, model.pStudioHWData->m_RootLOD, model.lodCount-1);
		}
	}
	// UNDONE: Don't sort if rendering transparent objects - for now this isn't called in the transparent case
	// sort by model, then by lod
	objectList.SetLessContext( static_cast<void *>(modelList.Base()) );
	objectList.RedoSort(true);

	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
	
	// now build out the lighting states
	if ( !bShadowDepth )
	{
		for ( int i = 0; i < objectList.Count(); i++ )
		{
			robject_t &obj = objectList[i];
			rmodel_t &model = modelList[obj.modelIndex];
			bool bStaticLighting = (model.bStaticLighting && obj.instance != MODEL_INSTANCE_INVALID);
			bool bVertexLit = model.bVertexLit;
			bool bNeedsEnvCubemap = bForceCubemap || model.bNeedsCubemap;
			bool bHasDecals = ( m_ModelInstances[obj.instance].m_DecalHandle != STUDIORENDER_DECAL_INVALID ) ? true : false;
			LightingState_t *pDecalLightState = NULL;
			if ( bHasDecals )
			{
				rdecalmodel_t decalModel;
				decalModel.lightIndex = lightStates.AddToTail();
				pDecalLightState = &lightStates[decalModel.lightIndex];
				decalModel.objectIndex = i;
				decalObjects.AddToTail( decalModel );
			}
			// for now we skip models that have shadows - will update later to include them in a post-pass
			if ( g_pShadowMgr->ModelHasShadows( obj.instance ) )
			{
				shadowObjects.AddToTail(i);
			}

			// get the static lighting from the cache
			DataCacheHandle_t hColorMeshData = DC_INVALID_HANDLE;
			if ( bStaticLighting )
			{
				// have static lighting, get from cache
				hColorMeshData = m_ModelInstances[obj.instance].m_ColorMeshHandle;
				CColorMeshData *pColorMeshData = CacheGet( hColorMeshData );
				if ( !pColorMeshData || pColorMeshData->m_bNeedsRetry )
				{
					// color meshes are not present, try to re-establish
					
					if ( UpdateStaticPropColorData( obj.pRenderable->GetIClientUnknown(), obj.instance ) )
					{
						pColorMeshData = CacheGet( hColorMeshData );
					}
					else if ( !pColorMeshData || !pColorMeshData->m_bNeedsRetry )
					{
						// can't draw
						continue;
					}
				}

				if ( pColorMeshData && ( pColorMeshData->m_bColorMeshValid || pColorMeshData->m_bColorTextureValid ) )
				{
					obj.pColorMeshes = pColorMeshData->m_pMeshInfos;
					if (pColorMeshData->m_bColorTextureValid && !pColorMeshData->m_bColorTextureCreated)
					{
						CreateLightmapsFromData(pColorMeshData);
					}

					if ( pCallQueue )
					{
						if ( CacheLock( hColorMeshData ) ) // CacheCreate above will call functions that won't take place until later. If color mesh isn't used right away, it could get dumped
						{
							pCallQueue->QueueCall( this, &CModelRender::CacheUnlock, hColorMeshData );
						}
					}
				}
				else
				{
					// failed, draw without static lighting
					bStaticLighting = false;
				}
			}

			// Get lighting from ambient light sources and radiosity bounces
			// also set up the env_cubemap from the light cache if necessary.
			if ( ( bVertexLit || bNeedsEnvCubemap ) )
			{
				// See if we're using static lighting
				LightCacheHandle_t* pLightCache = NULL;
				ITexture *pEnvCubemapTexture = NULL;
				if ( obj.instance != MODEL_INSTANCE_INVALID )
				{
					if ( ( m_ModelInstances[obj.instance].m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING ) && m_ModelInstances[obj.instance].m_LightCacheHandle )
					{
						pLightCache = &m_ModelInstances[obj.instance].m_LightCacheHandle;
					}
				}

				Assert(pLightCache);
				LightingState_t lightingState;
				LightingState_t *pState = &lightingState;
				if ( pLightCache )
				{
					// dx8 and dx9 case. . .hardware can do baked lighting plus other dynamic lighting
					// We already have the static part baked into a color mesh, so just get the dynamic stuff.
					if ( !bStaticLighting || StaticLightCacheAffectedByDynamicLight( *pLightCache ) )
					{
						pState = LightcacheGetStatic( *pLightCache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
						Assert( pState->numlights >= 0 && pState->numlights <= MAXLOCALLIGHTS );
					}
					else
					{
						pState = LightcacheGetStatic( *pLightCache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
						Assert( pState->numlights >= 0 && pState->numlights <= MAXLOCALLIGHTS );
					}
					if ( bHasDecals )
					{
						for ( int iCube = 0; iCube < 6; ++iCube )
						{
							pDecalLightState->r_boxcolor[iCube] = m_ModelInstances[obj.instance].m_AmbientLightingState.r_boxcolor[iCube] + pState->r_boxcolor[iCube];
						}
						pDecalLightState->CopyLocalLights( m_ModelInstances[obj.instance].m_AmbientLightingState );
						pDecalLightState->AddAllLocalLights( *pState );
					}
				}
				else	// !pLightcache
				{
					// UNDONE: is it possible to end up here in the static prop case?
					Vector vLightingOrigin = *obj.pLightingOrigin;
					int lightCacheFlags = bStaticLighting ? (LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE)
						: (LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST);
					LightcacheGetDynamic_Stats stats;
					pEnvCubemapTexture = LightcacheGetDynamic( vLightingOrigin, lightingState, 
						stats, lightCacheFlags, false );
					Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
					pState = &lightingState;

					if ( bHasDecals )
					{
						LightcacheGetDynamic_Stats tempStats;
						LightcacheGetDynamic( vLightingOrigin, *pDecalLightState, tempStats,
							LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST );
					}
				}

				if ( bNeedsEnvCubemap && pEnvCubemapTexture )
				{
					obj.pEnvCubeMap = pEnvCubemapTexture;
				}

				if ( bVertexLit )
				{
					// if we have any real lighting state we need to save it for this object
					if ( pState->numlights || pState->HasAmbientColors() )
					{
						obj.lightIndex = lightStates.AddToTail(*pState);
						lightObjects.AddToTail( i );
					}
				}
			}
		}
	}
	// now render the baked lighting props with no lighting state
	float color[3];
	color[0] = color[1] = color[2] = 1.0f;
	g_pStudioRender->SetColorModulation(color);
	g_pStudioRender->SetAlphaModulation(1.0f);
	g_pStudioRender->SetViewState( CurrentViewOrigin(), CurrentViewRight(), CurrentViewUp(), CurrentViewForward() );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	g_pStudioRender->ClearAllShadows();
	pRenderContext->DisableAllLocalLights();
	DrawModelInfo_t info;
	for ( int i = 0; i < 6; i++ )
		info.m_vecAmbientCube[i].Init();
	g_pStudioRender->SetAmbientLightColors( info.m_vecAmbientCube );
	pRenderContext->SetAmbientLight( 0.0, 0.0, 0.0 );
	info.m_nLocalLightCount = 0;
	info.m_bStaticLighting = false;

	int drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL | STUDIORENDER_DRAW_STATIC_LIGHTING;
	if (bShadowDepth)
		drawFlags |= STUDIO_SHADOWDEPTHTEXTURE;
	info.m_Decals = STUDIORENDER_DECAL_INVALID;
	info.m_Body = 0;
	info.m_HitboxSet = 0;
	for ( int i = 0; i < objectList.Count(); i++ )
	{
		robject_t &obj = objectList[i];
		if ( obj.lightIndex >= 0 )
			continue;
		rmodel_t &model = modelList[obj.modelIndex];
		if ( obj.pEnvCubeMap )
		{
			pRenderContext->BindLocalCubemap( obj.pEnvCubeMap );
		}

		info.m_pStudioHdr = model.pStudioHdr;
		info.m_pHardwareData = model.pStudioHWData;
		info.m_Skin = obj.skin;
		info.m_pClientEntity = static_cast<void*>(obj.pRenderable);
		info.m_Lod = obj.lod;
		info.m_pColorMeshes = obj.pColorMeshes;
		g_pStudioRender->DrawModelStaticProp( info, *obj.pMatrix, drawFlags );
	}

	// now render the vertex lit props
	int				nLocalLightCount = 0;
	LightDesc_t		localLightDescs[4];
	drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL | STUDIORENDER_DRAW_STATIC_LIGHTING;
	if ( lightObjects.Count() )
	{
		for ( int i = 0; i < lightObjects.Count(); i++ )
		{
			robject_t &obj = objectList[lightObjects[i]];
			rmodel_t &model = modelList[obj.modelIndex];

			if ( obj.pEnvCubeMap )
			{
				pRenderContext->BindLocalCubemap( obj.pEnvCubeMap );
			}

			LightingState_t *pState = &lightStates[obj.lightIndex];
			g_pStudioRender->SetAmbientLightColors( pState->r_boxcolor );
			pRenderContext->SetLightingOrigin( *obj.pLightingOrigin );
			R_SetNonAmbientLightingState( pState->numlights, pState->locallight, &nLocalLightCount, localLightDescs, true );
			info.m_pStudioHdr = model.pStudioHdr;
			info.m_pHardwareData = model.pStudioHWData;
			info.m_Skin = obj.skin;
			info.m_pClientEntity = static_cast<void*>(obj.pRenderable);
			info.m_Lod = obj.lod;
			info.m_pColorMeshes = obj.pColorMeshes;
			g_pStudioRender->DrawModelStaticProp( info, *obj.pMatrix, drawFlags );
		}
	}

	if ( !IsX360() && ( r_flashlight_version2.GetInt() == 0 ) && shadowObjects.Count() )
	{
		drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL;
		for ( int i = 0; i < shadowObjects.Count(); i++ )
		{
			// draw just the shadows!
			robject_t &obj = objectList[shadowObjects[i]];
			rmodel_t &model = modelList[obj.modelIndex];
			g_pShadowMgr->SetModelShadowState( obj.instance );

			info.m_pStudioHdr = model.pStudioHdr;
			info.m_pHardwareData = model.pStudioHWData;
			info.m_Skin = obj.skin;
			info.m_pClientEntity = static_cast<void*>(obj.pRenderable);
			info.m_Lod = obj.lod;
			info.m_pColorMeshes = obj.pColorMeshes;
			g_pStudioRender->DrawStaticPropShadows( info, *obj.pMatrix, drawFlags );
		}
		g_pStudioRender->ClearAllShadows();
	}

	for ( int i = 0; i < decalObjects.Count(); i++ )
	{
		// draw just the decals!
		robject_t &obj = objectList[decalObjects[i].objectIndex];
		rmodel_t &model = modelList[obj.modelIndex];
		LightingState_t *pState = &lightStates[decalObjects[i].lightIndex];
		g_pStudioRender->SetAmbientLightColors( pState->r_boxcolor );
		pRenderContext->SetLightingOrigin( *obj.pLightingOrigin );
		R_SetNonAmbientLightingState( pState->numlights, pState->locallight, &nLocalLightCount, localLightDescs, true );
		info.m_pStudioHdr = model.pStudioHdr;
		info.m_pHardwareData = model.pStudioHWData;
		info.m_Decals = m_ModelInstances[obj.instance].m_DecalHandle;
		info.m_Skin = obj.skin;
		info.m_pClientEntity = static_cast<void*>(obj.pRenderable);
		info.m_Lod = obj.lod;
		info.m_pColorMeshes = obj.pColorMeshes;
		g_pStudioRender->DrawStaticPropDecals( info, *obj.pMatrix );
	}

	// Restore the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
	return drawnCount;
#else // SWDS
	return 0;
#endif // SWDS
}

//-----------------------------------------------------------------------------
// Shadow rendering
//-----------------------------------------------------------------------------
matrix3x4_t* CModelRender::DrawModelShadowSetup( IClientRenderable *pRenderable, int body, int skin, DrawModelInfo_t *pInfo, matrix3x4_t *pCustomBoneToWorld )
{
#ifndef SWDS
	DrawModelInfo_t &info = *pInfo;
	static ConVar r_shadowlod("r_shadowlod", "-1");
	static ConVar r_shadowlodbias("r_shadowlodbias", "2");

	model_t const* pModel = pRenderable->GetModel();
	if ( !pModel )
		return NULL;

	// FIXME: Make brush shadows work
	if ( pModel->type != mod_studio )
		return NULL;

	Assert( modelloader->IsLoaded( pModel ) && ( pModel->type == mod_studio ) );

	info.m_pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );
	info.m_pColorMeshes = NULL;

	// quick exit
	if (info.m_pStudioHdr->numbodyparts == 0)
		return NULL;

	Assert ( pRenderable );
	info.m_pHardwareData = g_pMDLCache->GetHardwareData( pModel->studio );
	if ( !info.m_pHardwareData )
		return NULL;

	info.m_Decals = STUDIORENDER_DECAL_INVALID;
	info.m_Skin = skin;
	info.m_Body = body;
	info.m_pClientEntity = (void*)pRenderable;
	info.m_HitboxSet = 0;

	info.m_Lod = r_shadowlod.GetInt();
	// If the .mdl has a shadowlod, force the use of that one instead
	if ( info.m_pStudioHdr->flags & STUDIOHDR_FLAGS_HASSHADOWLOD )
	{
		info.m_Lod = info.m_pHardwareData->m_NumLODs-1;
	}
	else if ( info.m_Lod == USESHADOWLOD )
	{
		int lastlod = info.m_pHardwareData->m_NumLODs - 1;
		info.m_Lod = lastlod;
	}
	else if ( info.m_Lod < 0 )
	{
		CMatRenderContextPtr pRenderContext( materials );
		// Compute the shadow LOD...
		float factor = r_shadowlodbias.GetFloat() > 0.0f ? 1.0f / r_shadowlodbias.GetFloat() : 1.0f;
		float screenSize = factor * pRenderContext->ComputePixelWidthOfSphere( pRenderable->GetRenderOrigin(), 0.5f );
		info.m_Lod = g_pStudioRender->ComputeModelLod( info.m_pHardwareData, screenSize ); 
		info.m_Lod = info.m_pHardwareData->m_NumLODs-2;
		if ( info.m_Lod < 0 )
		{
			info.m_Lod = 0;
		}
	}

	// clamp to root lod
	if (info.m_Lod < info.m_pHardwareData->m_RootLOD)
	{
		info.m_Lod = info.m_pHardwareData->m_RootLOD;
	}

	matrix3x4_t *pBoneToWorld = pCustomBoneToWorld;
	if ( !pBoneToWorld )
	{
		pBoneToWorld = g_pStudioRender->LockBoneMatrices( info.m_pStudioHdr->numbones );
	}
	const bool bOk = pRenderable->SetupBones( pBoneToWorld, info.m_pStudioHdr->numbones, BONE_USED_BY_VERTEX_AT_LOD(info.m_Lod), cl.GetTime() );
	g_pStudioRender->UnlockBoneMatrices();
	if ( !bOk )
		return NULL;
	return pBoneToWorld;
#else
	return NULL;
#endif
}

void CModelRender::DrawModelShadow( IClientRenderable *pRenderable, const DrawModelInfo_t &info, matrix3x4_t *pBoneToWorld )
{
#ifndef SWDS
	// Needed because we don't call SetupWeights
	g_pStudioRender->SetEyeViewTarget( info.m_pStudioHdr, info.m_Body, vec3_origin );

	// Color + alpha modulation
	Vector white( 1, 1, 1 );
	g_pStudioRender->SetColorModulation( white.Base() );
	g_pStudioRender->SetAlphaModulation( 1.0f );

	if ((info.m_pStudioHdr->flags & STUDIOHDR_FLAGS_USE_SHADOWLOD_MATERIALS) == 0)
	{
		g_pStudioRender->ForcedMaterialOverride( g_pMaterialShadowBuild, OVERRIDE_BUILD_SHADOWS );
	}

	g_pStudioRender->DrawModel( NULL, info, pBoneToWorld, NULL, NULL, pRenderable->GetRenderOrigin(),
		STUDIORENDER_DRAW_NO_SHADOWS | STUDIORENDER_DRAW_ENTIRE_MODEL | STUDIORENDER_DRAW_NO_FLEXES );
	g_pStudioRender->ForcedMaterialOverride( 0 );
#endif
}

void  CModelRender::SetViewTarget( const CStudioHdr *pStudioHdr, int nBodyIndex, const Vector& target )
{
	g_pStudioRender->SetEyeViewTarget( pStudioHdr->GetRenderHdr(), nBodyIndex, target );
}

void CModelRender::InitColormeshParams( ModelInstance_t &instance, studiohwdata_t *pStudioHWData, colormeshparams_t *pColorMeshParams )
{
	pColorMeshParams->m_nMeshes = 0;
	pColorMeshParams->m_nTotalVertexes = 0;
	pColorMeshParams->m_pPooledVBAllocator = NULL;

	if ( ( instance.m_nFlags & MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR ) &&
		g_pMaterialSystemHardwareConfig->SupportsStreamOffset() &&
		 ( r_proplightingpooling.GetInt() == 1 ) )
	{
		// Color meshes can be allocated in a shared pool for static props
		// (saves memory on X360 due to 4-KB VB alignment)
		pColorMeshParams->m_pPooledVBAllocator = (IPooledVBAllocator *)&m_colorMeshVBAllocator;
	}

	for ( int lodID = pStudioHWData->m_RootLOD; lodID < pStudioHWData->m_NumLODs; lodID++ )
	{
		studioloddata_t *pLOD = &pStudioHWData->m_pLODs[lodID];
		for ( int meshID = 0; meshID < pStudioHWData->m_NumStudioMeshes; meshID++ )
		{
			studiomeshdata_t *pMesh = &pLOD->m_pMeshData[meshID];
			for ( int groupID = 0; groupID < pMesh->m_NumGroup; groupID++ )
			{
				pColorMeshParams->m_nVertexes[pColorMeshParams->m_nMeshes++] = pMesh->m_pMeshGroup[groupID].m_NumVertices;
				Assert( pColorMeshParams->m_nMeshes <= ARRAYSIZE( pColorMeshParams->m_nVertexes ) );

				pColorMeshParams->m_nTotalVertexes += pMesh->m_pMeshGroup[groupID].m_NumVertices;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Allocates the static prop color data meshes
//-----------------------------------------------------------------------------
// FIXME? : Move this to StudioRender?
CColorMeshData *CModelRender::FindOrCreateStaticPropColorData( ModelInstanceHandle_t handle )
{
	if ( handle == MODEL_INSTANCE_INVALID || !g_pMaterialSystemHardwareConfig->SupportsColorOnSecondStream() )
	{
		// the card can't support it
		return NULL;
	}

	ModelInstance_t& instance = m_ModelInstances[handle];
	CColorMeshData *pColorMeshData = CacheGet( instance.m_ColorMeshHandle );
	if ( pColorMeshData )
	{
		// found in cache
		return pColorMeshData;
	}

	if ( !instance.m_pModel )
	{
		return NULL;
	}

	Assert( modelloader->IsLoaded( instance.m_pModel ) && ( instance.m_pModel->type == mod_studio ) );
	studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( instance.m_pModel->studio );
	Assert( pStudioHWData );
	if ( !pStudioHWData )
		return NULL;

	colormeshparams_t params;
	InitColormeshParams( instance, pStudioHWData, &params );
	if ( params.m_nMeshes <= 0 )
	{
		// nothing to create
		return NULL;
	}

	// create the meshes
	params.m_fnHandle = instance.m_pModel->fnHandle;
	instance.m_ColorMeshHandle = CacheCreate( params );
	ProtectColorDataIfQueued( instance.m_ColorMeshHandle );
	pColorMeshData = CacheGet( instance.m_ColorMeshHandle );

	return pColorMeshData;
}

//-----------------------------------------------------------------------------
// Allocates the static prop color data meshes
//-----------------------------------------------------------------------------
// FIXME? : Move this to StudioRender?
void CModelRender::ProtectColorDataIfQueued( DataCacheHandle_t hColorMesh )
{
	if ( hColorMesh != DC_INVALID_HANDLE)
	{
		CMatRenderContextPtr pRenderContext( materials );
		ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
		if ( pCallQueue )
		{
			if ( CacheLock( hColorMesh ) ) // CacheCreate above will call functions that won't take place until later. If color mesh isn't used right away, it could get dumped
			{
				pCallQueue->QueueCall( this, &CModelRender::CacheUnlock, hColorMesh );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Old-style computation of vertex lighting ( Currently In Use )
//-----------------------------------------------------------------------------
void CModelRender::ComputeModelVertexLightingOld( mstudiomodel_t *pModel, 
	matrix3x4_t& matrix, const LightingState_t &lightingState, color24 *pLighting,
	bool bUseConstDirLighting, float flConstDirLightAmount )
{
	Vector			worldPos, worldNormal, destColor;
	int				nNumLightDesc;
	LightDesc_t		lightDesc[MAXLOCALLIGHTS];
	LightingState_t *pLightingState;

	pLightingState = (LightingState_t*)&lightingState;

	// build the lighting descriptors
	R_SetNonAmbientLightingState( pLightingState->numlights, pLightingState->locallight, &nNumLightDesc, lightDesc, false );

	const thinModelVertices_t		*thinVertData	= NULL;
	const mstudio_modelvertexdata_t	*vertData		= pModel->GetVertexData();
	mstudiovertex_t					*pFatVerts		= NULL;
	if ( vertData )
	{
		pFatVerts = vertData->Vertex( 0 );
	}
	else
	{
		thinVertData = pModel->GetThinVertexData();
		if ( !thinVertData )
			return;
	}

	bool bHasSSE = MathLib_SSEEnabled();

	// light all vertexes
	for ( int i = 0; i < pModel->numvertices; ++i )
	{
		if ( vertData )
		{
#ifdef _WIN32
			if (bHasSSE)
			{
				// hint the next vertex
				// data is loaded with one extra vertex for read past
#if !defined( _X360 ) // X360TBD
				_mm_prefetch( (char*)&pFatVerts[i+1], _MM_HINT_T0 );
#endif
		}
#endif

			VectorTransform( pFatVerts[i].m_vecPosition, matrix, worldPos );
			VectorRotate( pFatVerts[i].m_vecNormal, matrix, worldNormal );
		}
		else
		{
			Vector position;
			Vector normal;
			thinVertData->GetModelPosition(	pModel, i, &position );
			thinVertData->GetModelNormal( pModel, i, &normal );
			VectorTransform( position, matrix, worldPos );
			VectorRotate( normal, matrix, worldNormal );
		}

		if ( bUseConstDirLighting )
		{
			g_pStudioRender->ComputeLightingConstDirectional( pLightingState->r_boxcolor,
				nNumLightDesc, lightDesc, worldPos, worldNormal, destColor, flConstDirLightAmount );
		}
		else
		{
            g_pStudioRender->ComputeLighting( pLightingState->r_boxcolor,
				nNumLightDesc, lightDesc, worldPos, worldNormal, destColor );
		}
		
		// to gamma space
		destColor[0] = LinearToVertexLight( destColor[0] );
		destColor[1] = LinearToVertexLight( destColor[1] );
		destColor[2] = LinearToVertexLight( destColor[2] );

		Assert( (destColor[0] >= 0.0f) && (destColor[0] <= 1.0f) );
		Assert( (destColor[1] >= 0.0f) && (destColor[1] <= 1.0f) );
		Assert( (destColor[2] >= 0.0f) && (destColor[2] <= 1.0f) );

		pLighting[i].r = FastFToC(destColor[0]);
		pLighting[i].g = FastFToC(destColor[1]);
		pLighting[i].b = FastFToC(destColor[2]);
	}
}


//-----------------------------------------------------------------------------
// New-style computation of vertex lighting ( Not Used Yet )
//-----------------------------------------------------------------------------
void CModelRender::ComputeModelVertexLighting( IHandleEntity *pProp, 
	mstudiomodel_t *pModel, OptimizedModel::ModelLODHeader_t *pVtxLOD,
	matrix3x4_t& matrix, Vector4D *pTempMem, color24 *pLighting )
{
#ifndef SWDS
	if ( IsX360() )
		return;

	int i;
	unsigned char *pInSolid = (unsigned char*)stackalloc( ((pModel->numvertices + 7) >> 3) * sizeof(unsigned char) );
	Vector worldPos, worldNormal;

	const mstudio_modelvertexdata_t *vertData = pModel->GetVertexData();
	Assert( vertData );
	if ( !vertData )
		return;

	for ( i = 0; i < pModel->numvertices; ++i )
	{
		const Vector &pos = *vertData->Position( i );
		const Vector &normal = *vertData->Normal( i );
		VectorTransform( pos, matrix, worldPos );
		VectorRotate( normal, matrix, worldNormal );
		bool bNonSolid = ComputeVertexLightingFromSphericalSamples( worldPos, worldNormal, pProp, &(pTempMem[i].AsVector3D()) );
		
		int nByte = i >> 3;
		int nBit = i & 0x7;

		if ( bNonSolid )
		{
			pTempMem[i].w = 1.0f;
			pInSolid[ nByte ] &= ~(1 << nBit);
		}
		else
		{
			pTempMem[i].Init( );
			pInSolid[ nByte ] |= (1 << nBit);
		}
	}

	// Must iterate over each triangle to average out the colors for those
	// vertices in solid.
	// Iterate over all the meshes....
	for (int meshID = 0; meshID < pModel->nummeshes; ++meshID)
	{
		Assert( pModel->nummeshes == pVtxLOD->numMeshes );
		mstudiomesh_t* pMesh = pModel->pMesh(meshID);
		OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(meshID);

		// Iterate over all strip groups.
		for( int stripGroupID = 0; stripGroupID < pVtxMesh->numStripGroups; ++stripGroupID )
		{
			OptimizedModel::StripGroupHeader_t* pStripGroup = pVtxMesh->pStripGroup(stripGroupID);
			
			// Iterate over all indices
			Assert( pStripGroup->numIndices % 3 == 0 );
			for (i = 0; i < pStripGroup->numIndices; i += 3)
			{
				unsigned short nIndex1 = *pStripGroup->pIndex( i );
				unsigned short nIndex2 = *pStripGroup->pIndex( i + 1 );
				unsigned short nIndex3 = *pStripGroup->pIndex( i + 2 );

				int v[3];
				v[0] = pStripGroup->pVertex( nIndex1 )->origMeshVertID + pMesh->vertexoffset;
				v[1] = pStripGroup->pVertex( nIndex2 )->origMeshVertID + pMesh->vertexoffset;
				v[2] = pStripGroup->pVertex( nIndex3 )->origMeshVertID + pMesh->vertexoffset;

				Assert( v[0] < pModel->numvertices );
				Assert( v[1] < pModel->numvertices );
				Assert( v[2] < pModel->numvertices );

				bool bSolid[3];
				bSolid[0] = ( pInSolid[ v[0] >> 3 ] & ( 1 << ( v[0] & 0x7 ) ) ) != 0;
				bSolid[1] = ( pInSolid[ v[1] >> 3 ] & ( 1 << ( v[1] & 0x7 ) ) ) != 0;
				bSolid[2] = ( pInSolid[ v[2] >> 3 ] & ( 1 << ( v[2] & 0x7 ) ) ) != 0;

				int nValidCount = 0;
				int nAverage[3];
				if ( !bSolid[0] ) { nAverage[nValidCount++] = v[0]; }
				if ( !bSolid[1] ) { nAverage[nValidCount++] = v[1]; }
				if ( !bSolid[2] ) { nAverage[nValidCount++] = v[2]; }

				if ( nValidCount == 3 )
					continue;

				Vector vecAverage( 0, 0, 0 );
				for ( int j = 0; j < nValidCount; ++j )
				{
					vecAverage += pTempMem[nAverage[j]].AsVector3D();
				}

				if (nValidCount != 0)
				{
					vecAverage /= nValidCount;
				}

				if ( bSolid[0] ) { pTempMem[ v[0] ].AsVector3D() += vecAverage; pTempMem[ v[0] ].w += 1.0f; }
				if ( bSolid[1] ) { pTempMem[ v[1] ].AsVector3D() += vecAverage; pTempMem[ v[1] ].w += 1.0f; }
				if ( bSolid[2] ) { pTempMem[ v[2] ].AsVector3D() += vecAverage; pTempMem[ v[2] ].w += 1.0f; }
			}
		}
	}

	Vector destColor;
	for ( i = 0; i < pModel->numvertices; ++i )
	{
		if ( pTempMem[i].w != 0.0f )
		{
			pTempMem[i] /= pTempMem[i].w;
		}

		destColor[0] = LinearToVertexLight( pTempMem[i][0] );
		destColor[1] = LinearToVertexLight( pTempMem[i][1] );
		destColor[2] = LinearToVertexLight( pTempMem[i][2] );

		ColorClampTruncate( destColor );

		pLighting[i].r = FastFToC(destColor[0]);
		pLighting[i].g = FastFToC(destColor[1]);
		pLighting[i].b = FastFToC(destColor[2]);
	}
#endif
}

//-----------------------------------------------------------------------------
// Sanity check and setup the compiled color mesh for an optimal async load
// during runtime.
//-----------------------------------------------------------------------------
void CModelRender::ValidateStaticPropColorData( ModelInstanceHandle_t handle )
{
	if ( !r_proplightingfromdisk.GetBool() )
	{
		return;
	}

	ModelInstance_t *pInstance = &m_ModelInstances[handle];
	IHandleEntity* pProp = pInstance->m_pRenderable->GetIClientUnknown();

	if ( !g_pMaterialSystemHardwareConfig->SupportsColorOnSecondStream() || !StaticPropMgr()->IsStaticProp( pProp ) )
	{
		// can't support it or not a static prop
		return;
	}

	if ( !g_bLoadedMapHasBakedPropLighting || StaticPropMgr()->PropHasBakedLightingDisabled( pProp ) )
	{
		return;
	}

	MEM_ALLOC_CREDIT();

	// fetch the header
	CUtlBuffer utlBuf;
	char fileName[MAX_PATH];
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE || g_bBakedPropLightingNoSeparateHDR )
	{
		Q_snprintf( fileName, sizeof( fileName ), "sp_%d%s.vhv", StaticPropMgr()->GetStaticPropIndex( pProp ), GetPlatformExt() );
	}
	else
	{	
		Q_snprintf( fileName, sizeof( fileName ), "sp_hdr_%d%s.vhv", StaticPropMgr()->GetStaticPropIndex( pProp ), GetPlatformExt() );
	}

	if ( IsX360()  )
	{
		DataCacheHandle_t hColorMesh = GetCachedStaticPropColorData( fileName );
		if ( hColorMesh != DC_INVALID_HANDLE )
		{
			// already have it
			pInstance->m_ColorMeshHandle = hColorMesh;
			pInstance->m_nFlags &= ~MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD;
			pInstance->m_nFlags |= MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR;
			return;
		}
	}

	if ( !g_pFileSystem->ReadFile( fileName, "GAME", utlBuf, sizeof( HardwareVerts::FileHeader_t ), 0 ) )
	{
		// not available
		return;
	}

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( pInstance->m_pModel->studio );

	HardwareVerts::FileHeader_t *pVhvHdr = (HardwareVerts::FileHeader_t *)utlBuf.Base();
	if ( pVhvHdr->m_nVersion != VHV_VERSION || 
		pVhvHdr->m_nChecksum != (unsigned int)pStudioHdr->checksum || 
		pVhvHdr->m_nVertexSize != 4 )
	{
		// out of sync
		// mark for debug visualization
		pInstance->m_nFlags |= MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD;
		return;
	}

	// async callback can safely stream data into targets
	pInstance->m_nFlags &= ~MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD;
	pInstance->m_nFlags |= MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR;
}

//-----------------------------------------------------------------------------
// Async loader callback
// Called from async i/o thread - must spend minimal cycles in this context
//-----------------------------------------------------------------------------
void CModelRender::StaticPropColorMeshCallback( void *pContext, const void *pData, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	// get our preserved data
	Assert( pContext );
	staticPropAsyncContext_t *pStaticPropContext = (staticPropAsyncContext_t *)pContext;

	HardwareVerts::FileHeader_t *pVhvHdr;
	byte *pOriginalData = NULL;
	int numLightingComponents = 1;

	if ( asyncStatus != FSASYNC_OK )
	{
		// any i/o error
		goto cleanUp;
	}

	if ( IsX360() )
	{
		// only the 360 has compressed VHV data
		// the compressed data is after the header
		byte *pCompressedData = (byte *)pData + sizeof( HardwareVerts::FileHeader_t );
		if ( CLZMA::IsCompressed( pCompressedData ) )
		{
			// create a buffer that matches the original
			int actualSize = CLZMA::GetActualSize( pCompressedData );
			pOriginalData = (byte *)malloc( sizeof( HardwareVerts::FileHeader_t ) + actualSize );

			// place the header, then uncompress directly after it
			V_memcpy( pOriginalData, pData, sizeof( HardwareVerts::FileHeader_t ) );
			int outputLength = CLZMA::Uncompress( pCompressedData, pOriginalData + sizeof( HardwareVerts::FileHeader_t ) );
			if ( outputLength != actualSize )
			{
				goto cleanUp;
			}
			pData = pOriginalData;
		}
	}

	pVhvHdr = (HardwareVerts::FileHeader_t *)pData;

	int startMesh;
	for ( startMesh=0; startMesh<pVhvHdr->m_nMeshes; startMesh++ )
	{
		// skip past higher detail lod meshes that must be ignored
		// find first mesh that matches desired lod
		if ( pVhvHdr->pMesh( startMesh )->m_nLod == pStaticPropContext->m_nRootLOD )
		{
			break;
		}
	}

	int meshID;
	for ( meshID = startMesh; meshID<pVhvHdr->m_nMeshes; meshID++ )
	{
		int numVertexes = pVhvHdr->pMesh( meshID )->m_nVertexes;
		if ( numVertexes != pStaticPropContext->m_pColorMeshData->m_pMeshInfos[meshID-startMesh].m_nNumVerts )
		{
			// meshes are out of sync, discard data
			break;
		}

		int nID = meshID-startMesh;

		unsigned char *pIn = (unsigned char *) pVhvHdr->pVertexBase( meshID );
		unsigned char *pOut = NULL;
			
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pStaticPropContext->m_pColorMeshData->m_pMeshInfos[ nID ].m_pMesh, MATERIAL_HETEROGENOUS, numVertexes, 0 );
		if ( numLightingComponents > 1 )
		{
			pOut = reinterpret_cast< unsigned char * >( const_cast< float * >( meshBuilder.Normal() ) );
		}
		else
		{
			pOut = meshBuilder.Specular();
		}

#ifdef DX_TO_GL_ABSTRACTION
		// OPENGL_SWAP_COLORS
		for ( int i=0; i < (numVertexes * numLightingComponents ); i++ )
		{
			unsigned char red = *pIn++;
			unsigned char green = *pIn++;
			unsigned char blue = *pIn++;
			*pOut++ = blue;
			*pOut++ = green;
			*pOut++ = red;
			*pOut++ = *pIn++; // Alpha goes straight across
		}
#else
		V_memcpy( pOut, pIn, numVertexes * 4 * numLightingComponents );
#endif
		meshBuilder.End();
	}
cleanUp:
	if ( IsX360() )
	{
		AUTO_LOCK( m_CachedStaticPropMutex );
		// track the color mesh's datacache handle so that we can find it long after the model instance's are gone
		// the static prop filenames are guaranteed uniquely decorated
		m_CachedStaticPropColorData.Insert( pStaticPropContext->m_szFilenameVertex, pStaticPropContext->m_ColorMeshHandle );

		// No support for lightmap textures on X360. 
	}

	// mark as completed in single atomic operation
	pStaticPropContext->m_pColorMeshData->m_bColorMeshValid = true;
	CacheUnlock( pStaticPropContext->m_ColorMeshHandle );
	delete pStaticPropContext;

	if ( pOriginalData )
	{
		free( pOriginalData );
	}
}

//-----------------------------------------------------------------------------
// Async loader callback
// Called from async i/o thread - must spend minimal cycles in this context
//-----------------------------------------------------------------------------
void CModelRender::StaticPropColorTexelCallback(void *pContext, const void *pData, int numReadBytes, FSAsyncStatus_t asyncStatus)
{
	// get our preserved data
	Assert(pContext);
	staticPropAsyncContext_t *pStaticPropContext = (staticPropAsyncContext_t *)pContext;

	HardwareTexels::FileHeader_t *pVhtHdr;

	// This needs to be above the goto or clang complains "goto into protected scope."
	bool anyTextures = false;

	if (asyncStatus != FSASYNC_OK)
	{
		// any i/o error
		goto cleanUp;
	}

	pVhtHdr = (HardwareTexels::FileHeader_t *)pData;

	int startMesh;
	for (startMesh = 0; startMesh < pVhtHdr->m_nMeshes; startMesh++)
	{
		// skip past higher detail lod meshes that must be ignored
		// find first mesh that matches desired lod
		if (pVhtHdr->pMesh(startMesh)->m_nLod == pStaticPropContext->m_nRootLOD)
		{
			break;
		}
	}



	int meshID;
	for ( meshID = startMesh; meshID < pVhtHdr->m_nMeshes; meshID++ )
	{
		const HardwareTexels::MeshHeader_t* pMeshData = pVhtHdr->pMesh( meshID );

		// We can't create the real texture here because that's just how the material system works.
		// So instead, squirrel away what we need for later.
		ColorTexelsInfo_t* newCTI = new ColorTexelsInfo_t;
		newCTI->m_nWidth = pMeshData->m_nWidth;
		newCTI->m_nHeight = pMeshData->m_nHeight;
		newCTI->m_nMipmapCount = ImageLoader::GetNumMipMapLevels( newCTI->m_nWidth, newCTI->m_nHeight );
		newCTI->m_ImageFormat = ( ImageFormat ) pVhtHdr->m_nTexelFormat;
		newCTI->m_nByteCount = pVhtHdr->pMesh( meshID )->m_nBytes;
		newCTI->m_pTexelData = new byte[ newCTI->m_nByteCount ];
		Q_memcpy( newCTI->m_pTexelData, pVhtHdr->pTexelBase( meshID ), newCTI->m_nByteCount );

		pStaticPropContext->m_pColorMeshData->m_pMeshInfos[ meshID - startMesh ].m_pLightmapData = newCTI;
		Assert( pStaticPropContext->m_pColorMeshData->m_pMeshInfos[ meshID - startMesh ].m_pLightmap == NULL );
		anyTextures = true;
	}

	// This only gets set if we actually have texel data. Otherwise, it remains false.
	pStaticPropContext->m_pColorMeshData->m_bColorTextureValid = anyTextures;

cleanUp:
	// mark as completed in single atomic operation
	CacheUnlock( pStaticPropContext->m_ColorMeshHandle );
	delete pStaticPropContext;
}

//-----------------------------------------------------------------------------
// Async loader callback
// Called from async i/o thread - must spend minimal cycles in this context
//-----------------------------------------------------------------------------
static void StaticPropColorMeshCallback( const FileAsyncRequest_t &request, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	s_ModelRender.StaticPropColorMeshCallback( request.pContext, request.pData, numReadBytes, asyncStatus );
}

//-----------------------------------------------------------------------------
// Async loader callback
// Called from async i/o thread - must spend minimal cycles in this context
//-----------------------------------------------------------------------------
static void StaticPropColorTexelCallback( const FileAsyncRequest_t &request, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	s_ModelRender.StaticPropColorTexelCallback( request.pContext, request.pData, numReadBytes, asyncStatus );
}


//-----------------------------------------------------------------------------
// Queued loader callback
// Called from async i/o thread - must spend minimal cycles in this context
//-----------------------------------------------------------------------------
static void QueuedLoaderCallback_PropLighting( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
{
	// translate error
	FSAsyncStatus_t asyncStatus = ( loaderError == LOADERERROR_NONE ? FSASYNC_OK : FSASYNC_ERR_READING );

	// mimic async i/o completion
	s_ModelRender.StaticPropColorMeshCallback( pContext, pData, nSize, asyncStatus );
}

//-----------------------------------------------------------------------------
// Loads the serialized static prop color data.
// Returns false if legacy path should be used.
//-----------------------------------------------------------------------------
bool CModelRender::LoadStaticPropColorData( IHandleEntity *pProp, DataCacheHandle_t colorMeshHandle, studiohwdata_t *pStudioHWData )
{
	if ( !g_bLoadedMapHasBakedPropLighting || !r_proplightingfromdisk.GetBool() )
	{
		return false;
	}

	// lock the mesh memory during async transfer
	// the color meshes should already have low quality data to be used during rendering
	CColorMeshData *pColorMeshData = CacheLock( colorMeshHandle );
	if ( !pColorMeshData )
	{
		return false;
	}

	if ( pColorMeshData->m_hAsyncControlVertex || pColorMeshData->m_hAsyncControlTexel )
	{
		// load in progress, ignore additional request 
		// or already loaded, ignore until discarded from cache
		CacheUnlock( colorMeshHandle );
		return true;
	}

	// each static prop has its own compiled color mesh
	char fileName[MAX_PATH];
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE || g_bBakedPropLightingNoSeparateHDR )
	{
        Q_snprintf( fileName, sizeof( fileName ), "sp_%d%s.vhv", StaticPropMgr()->GetStaticPropIndex( pProp ), GetPlatformExt() );
	}
	else
	{
        Q_snprintf( fileName, sizeof( fileName ), "sp_hdr_%d%s.vhv", StaticPropMgr()->GetStaticPropIndex( pProp ), GetPlatformExt() );
	}

	// mark as invalid, async callback will set upon completion
	// prevents rendering during async transfer into locked mesh, otherwise d3drip
	pColorMeshData->m_bColorMeshValid = false;
	pColorMeshData->m_bColorTextureValid = false;
	pColorMeshData->m_bColorTextureCreated = false;


	// async load high quality lighting from file
	// can't optimal async yet, because need flat ppColorMesh[], so use callback to distribute
	// create our private context of data for the callback
	staticPropAsyncContext_t *pContextVertex = new staticPropAsyncContext_t;
	pContextVertex->m_nRootLOD = pStudioHWData->m_RootLOD;
	pContextVertex->m_nMeshes = pColorMeshData->m_nMeshes;
	pContextVertex->m_ColorMeshHandle = colorMeshHandle;
	pContextVertex->m_pColorMeshData = pColorMeshData;
	V_strncpy( pContextVertex->m_szFilenameVertex, fileName, sizeof( pContextVertex->m_szFilenameVertex ) );

	if ( IsX360() && g_pQueuedLoader->IsMapLoading() )
	{
		if ( !g_pQueuedLoader->ClaimAnonymousJob( fileName, QueuedLoaderCallback_PropLighting, (void *)pContextVertex ) )
		{
			// not there as expected
			// as a less optimal fallback during loading, issue as a standard queued loader job
			LoaderJob_t loaderJob;
			loaderJob.m_pFilename = fileName;
			loaderJob.m_pPathID = "GAME";
			loaderJob.m_pCallback = QueuedLoaderCallback_PropLighting;
			loaderJob.m_pContext = (void *)pContextVertex;
			loaderJob.m_Priority = LOADERPRIORITY_BEFOREPLAY;
			g_pQueuedLoader->AddJob( &loaderJob );
		}
		return true;
	}

	// async load the file
	FileAsyncRequest_t fileRequest;
	fileRequest.pContext = (void *)pContextVertex;
	fileRequest.pfnCallback = ::StaticPropColorMeshCallback;
	fileRequest.pData = NULL;
	fileRequest.pszFilename = fileName;
	fileRequest.nOffset = 0;
	fileRequest.flags = 0; // FSASYNC_FLAGS_SYNC;
	fileRequest.nBytes = 0;
	fileRequest.priority = -1;
	fileRequest.pszPathID = "GAME";

	// This must be done before sending pContextVertex down
	staticPropAsyncContext_t* pContextTexel = new staticPropAsyncContext_t( *pContextVertex );
	
	// queue vertex data for async load
	{
		MEM_ALLOC_CREDIT();
		g_pFileSystem->AsyncRead( fileRequest, &pColorMeshData->m_hAsyncControlVertex );
	}

	Q_snprintf( fileName, sizeof( fileName ), "texelslighting_%d.ppl", StaticPropMgr()->GetStaticPropIndex( pProp ) );
	V_strncpy( pContextTexel->m_szFilenameTexel, fileName, sizeof( pContextTexel->m_szFilenameTexel ) );

	
	// We are already locked, but we will unlock twice--so lock once more for the texel processing. 
	CacheLock( colorMeshHandle );

	// queue texel data for async load
	fileRequest.pContext = pContextTexel;
	fileRequest.pfnCallback = ::StaticPropColorTexelCallback;
	fileRequest.pData = NULL;
	fileRequest.pszFilename = fileName; // This doesn't need to happen, but included for clarity.

	{
		MEM_ALLOC_CREDIT();
		g_pFileSystem->AsyncRead( fileRequest, &pColorMeshData->m_hAsyncControlTexel );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Computes the static prop color data.
// Data calculation may be delayed if data is disk based.
// Returns FALSE if data not available or error. For retry polling pattern.
// Resturns TRUE if operation succesful or in progress (succeeds later).
//-----------------------------------------------------------------------------
bool CModelRender::UpdateStaticPropColorData( IHandleEntity *pProp, ModelInstanceHandle_t handle )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

#ifndef SWDS
	// find or allocate color meshes
	CColorMeshData *pColorMeshData = FindOrCreateStaticPropColorData( handle );
	if ( !pColorMeshData )
	{
		return false;
	}

	// HACK: on PC, VB creation can fail due to device loss
	if ( IsPC() && pColorMeshData->m_bHasInvalidVB )
	{
		// Don't retry until color data is flushed by device restore
		pColorMeshData->m_bColorMeshValid = false;
		pColorMeshData->m_bNeedsRetry = false;
		return false;
	}

	unsigned char debugColor[3] = {0};
	bool bDebugColor = false;
	if ( r_debugrandomstaticlighting.GetBool() )
	{
		// randomize with bright colors, skip black and white
		// purposely not deterministic to catch bugs with excessive re-baking (i.e. disco)
		Vector fRandomColor;
		int nColor = RandomInt(1,6);
		fRandomColor.x = (nColor>>2) & 1;
		fRandomColor.y = (nColor>>1) & 1;
		fRandomColor.z = nColor & 1;
		VectorNormalize( fRandomColor );
		debugColor[0] = fRandomColor[0] * 255.0f;
		debugColor[1] = fRandomColor[1] * 255.0f;
		debugColor[2] = fRandomColor[2] * 255.0f;
		bDebugColor = true;
	}

	// FIXME? : Move this to StudioRender?
	ModelInstance_t &inst = m_ModelInstances[handle];
	Assert( inst.m_pModel );
	Assert( modelloader->IsLoaded( inst.m_pModel ) && ( inst.m_pModel->type == mod_studio ) );

	if ( r_proplightingfromdisk.GetInt() == 2 )
	{
		// This visualization debug mode is strictly to debug which static prop models have valid disk
		// based lighting. There should be no red models, only green or yellow. Yellow models denote the legacy
		// lower quality runtime baked lighting.
		if ( inst.m_nFlags & MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD )
		{
			// prop was compiled for static prop lighting, but out of sync
			// bad disk data for model, show as red
			debugColor[0] = 255.0f;
			debugColor[1] = 0;
			debugColor[2] = 0;
		}
		else if ( inst.m_nFlags & MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR )
		{
			// valid disk data, show as green
			debugColor[0] = 0;
			debugColor[1] = 255.0f;
			debugColor[2] = 0;
		}
		else
		{
			// no disk based data, using runtime method, show as yellow
			// identifies a prop that wasn't compiled for static prop lighting
			debugColor[0] = 255.0f;
			debugColor[1] = 255.0f;
			debugColor[2] = 0;
		}
		bDebugColor = true;
	}

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( inst.m_pModel->studio );
	studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( inst.m_pModel->studio );
	Assert( pStudioHdr && pStudioHWData );

	if ( !bDebugColor && ( inst.m_nFlags & MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR ) )
	{
		// start an async load on available higher quality disc based data
		if ( LoadStaticPropColorData( pProp, inst.m_ColorMeshHandle, pStudioHWData ) )
		{
			// async in progress, operation expected to succeed
			// async callback handles finalization
			return true;
		}
	}
	
	// lighting calculation path
	// calculation may abort due to lack of async requested data, caller should retry 
	pColorMeshData->m_bColorMeshValid = false;
	pColorMeshData->m_bColorTextureValid = false;
	pColorMeshData->m_bColorTextureCreated = false;
	pColorMeshData->m_bNeedsRetry = true;

	if ( !bDebugColor )
	{
		// vertexes must be available for lighting calculation
		vertexFileHeader_t *pVertexHdr = g_pMDLCache->GetVertexData( (MDLHandle_t)(int)pStudioHdr->virtualModel&0xffff );
		if ( !pVertexHdr )
		{
			// data not available yet
			return false;
		}
	}

	inst.m_nFlags |= MODEL_INSTANCE_HAS_COLOR_DATA;

	// calculate lighting, set for access to verts
	m_pStudioHdr = pStudioHdr;

	// Sets the model transform state in g_pStudioRender
	matrix3x4_t matrix;
	AngleMatrix( inst.m_pRenderable->GetRenderAngles(), inst.m_pRenderable->GetRenderOrigin(), matrix );
	
	// Get static lighting only!!  We'll add dynamic and lightstyles in in the vertex shader. . .
	unsigned int lightCacheFlags = LIGHTCACHEFLAGS_STATIC;
	if ( !g_pMaterialSystemHardwareConfig->SupportsStaticPlusDynamicLighting() )
	{
		// . . . unless we can't do anything but static or dynamic simulaneously. . then
		// we'll bake the lightstyle info here.
		lightCacheFlags |= LIGHTCACHEFLAGS_LIGHTSTYLE;
	}

	LightingState_t lightingState;
	if ( (inst.m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING) && inst.m_LightCacheHandle )
	{
		lightingState = *(LightcacheGetStatic( inst.m_LightCacheHandle, NULL, lightCacheFlags ));
	}
	else
	{
		// Choose the lighting origin
		Vector entOrigin;
		R_ComputeLightingOrigin( inst.m_pRenderable, pStudioHdr, matrix, entOrigin );
		LightcacheGetDynamic_Stats stats;
		LightcacheGetDynamic( entOrigin, lightingState, stats, lightCacheFlags );
	}

	// See if the studiohdr wants to use constant directional light, ie
	// the surface normal plays no part in determining light intensity
	bool bUseConstDirLighting = false;
	float flConstDirLightingAmount = 0.0;
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_CONSTANT_DIRECTIONAL_LIGHT_DOT )
	{
		bUseConstDirLighting = true;
		flConstDirLightingAmount =  (float)( pStudioHdr->constdirectionallightdot ) / 255.0;
	}

	CUtlMemory< color24 > tmpLightingMem; 
	
	// Iterate over every body part...
	for ( int bodyPartID = 0; bodyPartID < pStudioHdr->numbodyparts; ++bodyPartID )
	{
		mstudiobodyparts_t* pBodyPart = pStudioHdr->pBodypart( bodyPartID );

		// Iterate over every submodel...
		for ( int modelID = 0; modelID < pBodyPart->nummodels; ++modelID )
		{
			mstudiomodel_t* pModel = pBodyPart->pModel(modelID);

			if ( pModel->numvertices == 0 )
				continue;

			// Make sure we've got enough space allocated
			tmpLightingMem.EnsureCapacity( pModel->numvertices );

			if ( !bDebugColor )
			{
				// Compute lighting for each unique vertex in the model exactly once
				ComputeModelVertexLightingOld( pModel, matrix, lightingState, tmpLightingMem.Base(), bUseConstDirLighting, flConstDirLightingAmount );
			}
			else
			{
				for ( int i=0; i<pModel->numvertices; i++ )
				{
					tmpLightingMem[i].r = debugColor[0];
					tmpLightingMem[i].g = debugColor[1];
					tmpLightingMem[i].b = debugColor[2];
				}
			}

			// distribute the lighting results to the mesh's vertexes
			for ( int lodID = pStudioHWData->m_RootLOD; lodID < pStudioHWData->m_NumLODs; ++lodID )
			{
				studioloddata_t *pStudioLODData = &pStudioHWData->m_pLODs[lodID];
				studiomeshdata_t *pStudioMeshData = pStudioLODData->m_pMeshData;

				// Iterate over all the meshes....
				for ( int meshID = 0; meshID < pModel->nummeshes; ++meshID)
				{
					mstudiomesh_t* pMesh = pModel->pMesh( meshID );

					// Iterate over all strip groups.
					for ( int stripGroupID = 0; stripGroupID < pStudioMeshData[pMesh->meshid].m_NumGroup; ++stripGroupID )
					{
						studiomeshgroup_t* pMeshGroup = &pStudioMeshData[pMesh->meshid].m_pMeshGroup[stripGroupID];
						ColorMeshInfo_t* pColorMeshInfo = &pColorMeshData->m_pMeshInfos[pMeshGroup->m_ColorMeshID];

						CMeshBuilder meshBuilder;
						meshBuilder.Begin( pColorMeshInfo->m_pMesh, MATERIAL_HETEROGENOUS, pMeshGroup->m_NumVertices, 0 );
						
						if ( !meshBuilder.VertexSize() )
						{
							meshBuilder.End();
							return false;		// Aborting processing, since something was wrong with D3D
						}

						// We need to account for the stream offset used by pool-allocated (static-lit) color meshes:
						int streamOffset = pColorMeshInfo->m_nVertOffsetInBytes / meshBuilder.VertexSize();
						meshBuilder.AdvanceVertices( streamOffset );

						// Iterate over all vertices
						for ( int i = 0; i < pMeshGroup->m_NumVertices; ++i)
						{
							int nVertIndex = pMesh->vertexoffset + pMeshGroup->m_pGroupIndexToMeshIndex[i];
							Assert( nVertIndex < pModel->numvertices );
							meshBuilder.Specular3ub( tmpLightingMem[nVertIndex].r, tmpLightingMem[nVertIndex].g, tmpLightingMem[nVertIndex].b );
							meshBuilder.AdvanceVertex();
						}

						meshBuilder.End();
					}
				}
			}
		}
	}
	
	pColorMeshData->m_bColorMeshValid = true;
	pColorMeshData->m_bNeedsRetry = false;
#endif
	
	return true;
}


//-----------------------------------------------------------------------------
// FIXME? : Move this to StudioRender?
//-----------------------------------------------------------------------------
void CModelRender::DestroyStaticPropColorData( ModelInstanceHandle_t handle )
{
#ifndef SWDS
	if ( handle == MODEL_INSTANCE_INVALID )
		return;

	if ( m_ModelInstances[handle].m_ColorMeshHandle != DC_INVALID_HANDLE )
	{
		CacheRemove( m_ModelInstances[handle].m_ColorMeshHandle );
		m_ModelInstances[handle].m_ColorMeshHandle = DC_INVALID_HANDLE;
	}
#endif
}


void CModelRender::ReleaseAllStaticPropColorData( void )
{
	FOR_EACH_LL( m_ModelInstances, i )
	{
		DestroyStaticPropColorData( i );
	}
	if ( IsX360() )
	{
		PurgeCachedStaticPropColorData();
	}
}


void CModelRender::RestoreAllStaticPropColorData( void )
{
#if !defined( SWDS )
	if ( !host_state.worldmodel )
		return;

	// invalidate all static lighting cache data
	InvalidateStaticLightingCache();

	// rebake
	FOR_EACH_LL( m_ModelInstances, i )
	{
		UpdateStaticPropColorData( m_ModelInstances[i].m_pRenderable->GetIClientUnknown(), i );
	}
#endif
}

void RestoreAllStaticPropColorData( void )
{
	s_ModelRender.RestoreAllStaticPropColorData();
}


//-----------------------------------------------------------------------------
// Creates, destroys instance data to be associated with the model
//-----------------------------------------------------------------------------
ModelInstanceHandle_t CModelRender::CreateInstance( IClientRenderable *pRenderable, LightCacheHandle_t *pCache )
{
	Assert( pRenderable );

	// ensure all components are available
	model_t *pModel = (model_t*)pRenderable->GetModel();

	// We're ok, allocate a new instance handle
	ModelInstanceHandle_t handle = m_ModelInstances.AddToTail();
	ModelInstance_t& instance = m_ModelInstances[handle];

	instance.m_pRenderable = pRenderable;
	instance.m_DecalHandle = STUDIORENDER_DECAL_INVALID;
	instance.m_pModel = (model_t*)pModel;
	instance.m_ColorMeshHandle = DC_INVALID_HANDLE;
	instance.m_flLightingTime = CURRENT_LIGHTING_UNINITIALIZED;
	instance.m_nFlags = 0;
	instance.m_LightCacheHandle = 0;

	instance.m_AmbientLightingState.ZeroLightingState();
	for ( int i = 0; i < 6; ++i )
	{
		// To catch errors with uninitialized m_AmbientLightingState...
		// force to pure red
		instance.m_AmbientLightingState.r_boxcolor[i].x = 1.0;
	}

#ifndef SWDS
	instance.m_FirstShadow = g_pShadowMgr->InvalidShadowIndex();
#endif

	// Static props use baked lighting for performance reasons
	if ( pCache )
	{
		SetStaticLighting( handle, pCache );

		// validate static color meshes once, now at load/create time
		ValidateStaticPropColorData( handle );
	
		// 360 persists the color meshes across same map loads
		if ( !IsX360() || instance.m_ColorMeshHandle == DC_INVALID_HANDLE )
		{
			// builds out color meshes or loads disk colors, now at load/create time
			RecomputeStaticLighting( handle );
		}
		else
			if ( r_decalstaticprops.GetBool() && instance.m_LightCacheHandle )
			{
				instance.m_AmbientLightingState = *(LightcacheGetStatic( *pCache, NULL, LIGHTCACHEFLAGS_STATIC ));
			}
	}
	
	return handle;
}


//-----------------------------------------------------------------------------
// Assigns static lighting to the model instance
//-----------------------------------------------------------------------------
void CModelRender::SetStaticLighting( ModelInstanceHandle_t handle, LightCacheHandle_t *pCache )
{
	// FIXME: If we make static lighting available for client-side props,
	// we must clean up the lightcache handles as the model instances are removed.
	// At the moment, since only the static prop manager uses this, it cleans up all LightCacheHandles 
	// at level shutdown.

	// The reason I moved the lightcache handles into here is because this place needs
	// to know about lighting overrides when restoring meshes for alt-tab reasons
	// It was a real pain to do this from within the static prop mgr, where the
	// lightcache handle used to reside
	if (handle != MODEL_INSTANCE_INVALID)
	{
		ModelInstance_t& instance = m_ModelInstances[handle];
		if ( pCache )
		{
			instance.m_LightCacheHandle = *pCache;
			instance.m_nFlags |= MODEL_INSTANCE_HAS_STATIC_LIGHTING;
		}
		else
		{
			instance.m_LightCacheHandle = 0;
			instance.m_nFlags &= ~MODEL_INSTANCE_HAS_STATIC_LIGHTING;
		}
	}
}

LightCacheHandle_t CModelRender::GetStaticLighting( ModelInstanceHandle_t handle )
{
	if (handle != MODEL_INSTANCE_INVALID)
	{
		ModelInstance_t& instance = m_ModelInstances[handle];
		if ( instance.m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING )
			return instance.m_LightCacheHandle;
		return 0;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// This gets called when overbright, etc gets changed to recompute static prop lighting.
// Returns FALSE if needed async data not available to complete computation or an error (don't draw).
// Returns TRUE if operation succeeded or computation skipped (ok to draw).
// Callers use this to track state in a retry pattern, so the expensive computation
// only happens once as needed or can continue to be polled until success.
//-----------------------------------------------------------------------------
bool CModelRender::RecomputeStaticLighting( ModelInstanceHandle_t handle )
{
#ifndef SWDS
	if ( handle == MODEL_INSTANCE_INVALID )
	{
		return false;
	}

	if ( !g_pMaterialSystemHardwareConfig->SupportsColorOnSecondStream() )
	{
		// static lighting not supported, but callers can proceed
		return true;
	}

	ModelInstance_t& instance = m_ModelInstances[handle];
	Assert( modelloader->IsLoaded( instance.m_pModel ) && ( instance.m_pModel->type == mod_studio ) );

	// get data, possibly delayed due to async
	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( instance.m_pModel->studio );
	if ( !pStudioHdr )
	{
		// data not available
		return false;
	}

	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP )
	{
		// get data, possibly delayed due to async
		studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( instance.m_pModel->studio );
		if ( !pStudioHWData )
		{
			// data not available
			return false;
		}

		if ( r_decalstaticprops.GetBool() && instance.m_LightCacheHandle )
		{
			instance.m_AmbientLightingState = *(LightcacheGetStatic( instance.m_LightCacheHandle, NULL, LIGHTCACHEFLAGS_STATIC ));
		}

		return UpdateStaticPropColorData( instance.m_pRenderable->GetIClientUnknown(), handle );
	}

#endif
	// success
	return true;
}

void CModelRender::PurgeCachedStaticPropColorData( void )
{
	// valid for 360 only
	Assert( IsX360() );
	if ( IsPC() )
	{
		return;
	}

	// flush all the color mesh data
	GetCacheSection()->Flush( true, true );
	DataCacheStatus_t status;
	GetCacheSection()->GetStatus( &status );
	if ( status.nBytes )
	{
		DevWarning( "CModelRender: ColorMesh %d bytes failed to flush!\n", status.nBytes );
	}

	m_colorMeshVBAllocator.Clear();
	m_CachedStaticPropColorData.Purge();
}

bool CModelRender::IsStaticPropColorDataCached( const char *pName )
{
	// valid for 360 only
	Assert( IsX360() );
	if ( IsPC() )
	{
		return false;
	}

	DataCacheHandle_t hColorMesh = DC_INVALID_HANDLE;
	{
		AUTO_LOCK( m_CachedStaticPropMutex );
		int iIndex = m_CachedStaticPropColorData.Find( pName );
		if ( m_CachedStaticPropColorData.IsValidIndex( iIndex ) )
		{
			hColorMesh = m_CachedStaticPropColorData[iIndex];
		}
	}

	CColorMeshData *pColorMeshData = CacheGetNoTouch( hColorMesh );
	if ( pColorMeshData )
	{
		// color mesh data is in cache
		return true;
	}

	return false;
}

DataCacheHandle_t CModelRender::GetCachedStaticPropColorData( const char *pName )
{
	// valid for 360 only
	Assert( IsX360() );
	if ( IsPC() )
	{
		return DC_INVALID_HANDLE;
	}

	DataCacheHandle_t hColorMesh = DC_INVALID_HANDLE;
	{
		AUTO_LOCK( m_CachedStaticPropMutex );
		int iIndex = m_CachedStaticPropColorData.Find( pName );
		if ( m_CachedStaticPropColorData.IsValidIndex( iIndex ) )
		{
			hColorMesh = m_CachedStaticPropColorData[iIndex];
		}
	}

	return hColorMesh;
}

void CModelRender::SetupColorMeshes( int nTotalVerts )
{
	Assert( IsX360() );
	if ( IsPC() )
	{
		return;
	}

	if ( !g_pQueuedLoader->IsMapLoading() )
	{
		// oops, the queued loader didn't run which does the pre-purge cleanup
		// do the cleanup now
		PurgeCachedStaticPropColorData();
	}

	// Set up the appropriate default value for color mesh pooling
	if ( r_proplightingpooling.GetInt() == -1 )
	{
		// This is useful on X360 because VBs are 4-KB aligned, so using a shared VB saves tons of memory
		r_proplightingpooling.SetValue( true );
	}

	if ( r_proplightingpooling.GetInt() == 1 )
	{
		if ( m_colorMeshVBAllocator.GetNumVertsAllocated() == 0 )
		{
			if ( nTotalVerts )
			{
				// Allocate a mesh (vertex buffer) big enough to accommodate all static prop color meshes
				// (which are allocated inside CModelRender::FindOrCreateStaticPropColorData() ):
				m_colorMeshVBAllocator.Init( VERTEX_SPECULAR, nTotalVerts );
			}
		}
		else
		{
			// already allocated
			// 360 keeps the color meshes during same map loads
			// vb allocator already allocated, needs to match
			Assert( m_colorMeshVBAllocator.GetNumVertsAllocated() == nTotalVerts );
		}
	}
}

void CModelRender::DestroyInstance( ModelInstanceHandle_t handle )
{
	if ( handle == MODEL_INSTANCE_INVALID )
		return;

	g_pStudioRender->DestroyDecalList( m_ModelInstances[handle].m_DecalHandle );
#ifndef SWDS
	g_pShadowMgr->RemoveAllShadowsFromModel( handle );
#endif

	// 360 holds onto static prop disk color data only, to avoid redundant work during same map load
	// can only persist props with disk based lighting
	// check for dvd mode as a reasonable assurance that the queued loader will be responsible for a possible purge
	// if the queued loader doesn't run, the purge will get caught later than intended
	bool bPersistLighting = IsX360() && 
		( m_ModelInstances[handle].m_nFlags & MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR ) && 
		( g_pFullFileSystem->GetDVDMode() == DVDMODE_STRICT );
	if ( !bPersistLighting )
	{
		DestroyStaticPropColorData( handle );
	}

	m_ModelInstances.Remove( handle );
}

bool CModelRender::ChangeInstance( ModelInstanceHandle_t handle, IClientRenderable *pRenderable )
{
	if ( handle == MODEL_INSTANCE_INVALID || !pRenderable )
		return false;

	ModelInstance_t& instance = m_ModelInstances[handle];

	if ( instance.m_pModel != pRenderable->GetModel() )
	{
		DevMsg("MoveInstanceHandle: models are different!\n");
		return false;
	}

	// ok, models are the same, change renderable pointer
	instance.m_pRenderable = pRenderable;

	return true;
}


//-----------------------------------------------------------------------------
// It's not valid if the model index changed + we have non-zero instance data
//-----------------------------------------------------------------------------
bool CModelRender::IsModelInstanceValid( ModelInstanceHandle_t handle )
{
	if ( handle == MODEL_INSTANCE_INVALID )
		return false;

	ModelInstance_t& inst = m_ModelInstances[handle];
	if ( inst.m_DecalHandle == STUDIORENDER_DECAL_INVALID )
		return false;

	model_t const* pModel = inst.m_pRenderable->GetModel();
	return inst.m_pModel == pModel;
}


//-----------------------------------------------------------------------------
// Creates a decal on a model instance by doing a planar projection
//-----------------------------------------------------------------------------
void CModelRender::AddDecal( ModelInstanceHandle_t handle, Ray_t const& ray,
	const Vector& decalUp, int decalIndex, int body, bool noPokeThru, int maxLODToDecal )
{
	Color cColorTemp;
	AddDecalInternal( handle, ray, decalUp, decalIndex, body, false, cColorTemp, noPokeThru, maxLODToDecal );
}

//-----------------------------------------------------------------------------
void CModelRender::AddColoredDecal( ModelInstanceHandle_t handle, Ray_t const& ray,
	const Vector& decalUp, int decalIndex, int body, Color cColor, bool noPokeThru, int maxLODToDecal )
{
	AddDecalInternal( handle, ray, decalUp, decalIndex, body, true, cColor, noPokeThru, maxLODToDecal );
}

//-----------------------------------------------------------------------------
void CModelRender::GetMaterialOverride( IMaterial** ppOutForcedMaterial, OverrideType_t* pOutOverrideType )
{
	g_pStudioRender->GetMaterialOverride( ppOutForcedMaterial, pOutOverrideType );
}

//-----------------------------------------------------------------------------
void CModelRender::AddDecalInternal( ModelInstanceHandle_t handle, Ray_t const& ray, 
	const Vector& decalUp, int decalIndex, int body, bool bUseColor, Color cColor, bool noPokeThru, int maxLODToDecal)
{
	if (handle == MODEL_INSTANCE_INVALID)
		return;

	// Get the decal material + radius
	IMaterial* pDecalMaterial;
	float w, h;
	R_DecalGetMaterialAndSize( decalIndex, pDecalMaterial, w, h );
	if ( !pDecalMaterial )
	{
		DevWarning("Bad decal index %d\n", decalIndex );
		return;
	}
	w *= 0.5f;
	h *= 0.5f;

	// FIXME: For now, don't render fading decals on props...
	bool found = false;
	pDecalMaterial->FindVar( "$decalFadeDuration", &found, false );
	if ( found )
		return;

	if ( bUseColor )
	{
		IMaterialVar *pColor = pDecalMaterial->FindVar( "$color2", &found, false );
		if ( found )
		{
			// expects a 0..1 value.  Input is 0 to 255
			pColor->SetVecValue( cColor.r() / 255.0f, cColor.g() / 255.0f, cColor.b() / 255.0f );
		}
	}
	
	// FIXME: Pass w and h into AddDecal
	float radius = (w > h) ? w : h;

	ModelInstance_t& inst = m_ModelInstances[handle];
	if (!IsModelInstanceValid(handle))
	{
		g_pStudioRender->DestroyDecalList(inst.m_DecalHandle);
		inst.m_DecalHandle = STUDIORENDER_DECAL_INVALID;
	}

	Assert( modelloader->IsLoaded( inst.m_pModel ) && ( inst.m_pModel->type == mod_studio ) );
	if ( inst.m_DecalHandle == STUDIORENDER_DECAL_INVALID )
	{
		studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( inst.m_pModel->studio );
		inst.m_DecalHandle = g_pStudioRender->CreateDecalList( pStudioHWData );
	}

	matrix3x4_t *pBoneToWorld = SetupModelState( inst.m_pRenderable );
	g_pStudioRender->AddDecal( inst.m_DecalHandle, g_pMDLCache->GetStudioHdr( inst.m_pModel->studio ),
		pBoneToWorld, ray, decalUp, pDecalMaterial, radius, body, noPokeThru, maxLODToDecal );
}

//-----------------------------------------------------------------------------
// Purpose: Removes all the decals on a model instance
//-----------------------------------------------------------------------------
void CModelRender::RemoveAllDecals( ModelInstanceHandle_t handle )
{
	if (handle == MODEL_INSTANCE_INVALID)
		return;

	ModelInstance_t& inst = m_ModelInstances[handle];
	if (!IsModelInstanceValid(handle))
		return;

	g_pStudioRender->DestroyDecalList( inst.m_DecalHandle );
	inst.m_DecalHandle = STUDIORENDER_DECAL_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelRender::RemoveAllDecalsFromAllModels()
{
	for ( ModelInstanceHandle_t i = m_ModelInstances.Head(); 
		i != m_ModelInstances.InvalidIndex(); 
		i = m_ModelInstances.Next( i ) )
	{
		RemoveAllDecals( i );
	}
}

const vertexFileHeader_t * mstudiomodel_t::CacheVertexData( void *pModelData )
{
	// make requested data resident
	Assert( pModelData == NULL );
	return s_ModelRender.CacheVertexData();
}

bool CheckVarRange_r_rootlod()
{
	return CheckVarRange_Generic( &r_rootlod, 0, 2 );
}

bool CheckVarRange_r_lod()
{
	return CheckVarRange_Generic( &r_lod, -1, 2 );
}


// Convar callback to change lod 
//-----------------------------------------------------------------------------
void r_lod_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	CheckVarRange_r_lod();
}

//-----------------------------------------------------------------------------
// Convar callback to change root lod 
//-----------------------------------------------------------------------------
void SetRootLOD_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	// Make sure the variable is in range.
	if ( CheckVarRange_r_rootlod() )
		return;	// was called recursively.

	ConVarRef var( pConVar );
	UpdateStudioRenderConfig();
	if ( !g_LostVideoMemory && Q_strcmp( var.GetString(), pOldString ) )
	{
		// reload only the necessary models to desired lod
		modelloader->Studio_ReloadModels( IModelLoader::RELOAD_LOD_CHANGED );
	}
}

//-----------------------------------------------------------------------------
// Discard and reload (rebuild, rebake, etc) models to the current lod
//-----------------------------------------------------------------------------
void FlushLOD_f()
{
	UpdateStudioRenderConfig();
	if ( !g_LostVideoMemory )
	{
		// force a full discard and rebuild of all loaded models
		modelloader->Studio_ReloadModels( IModelLoader::RELOAD_EVERYTHING );
	}
}

//-----------------------------------------------------------------------------
//
// CPooledVBAllocator_ColorMesh implementation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// CPooledVBAllocator_ColorMesh constructor
//-----------------------------------------------------------------------------
CPooledVBAllocator_ColorMesh::CPooledVBAllocator_ColorMesh()
: m_pMesh( NULL )
{
	Clear();
}

//-----------------------------------------------------------------------------
// CPooledVBAllocator_ColorMesh destructor
//  - Clear should have been called
//-----------------------------------------------------------------------------
CPooledVBAllocator_ColorMesh::~CPooledVBAllocator_ColorMesh()
{
	CheckIsClear();

	// Clean up, if it hadn't been done already
	Clear();
}

//-----------------------------------------------------------------------------
// Init
//  - Allocate the internal shared mesh (vertex buffer)
//-----------------------------------------------------------------------------
bool CPooledVBAllocator_ColorMesh::Init( VertexFormat_t format, int numVerts )
{
	if ( !CheckIsClear() )
		return false;

	if ( g_VBAllocTracker )
		g_VBAllocTracker->TrackMeshAllocations( "CPooledVBAllocator_ColorMesh::Init" );

	CMatRenderContextPtr pRenderContext( materials );
	m_pMesh = pRenderContext->CreateStaticMesh( format, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_COLOR );
	if ( m_pMesh )
	{
		// Build out the underlying vertex buffer
		CMeshBuilder meshBuilder;
		int numIndices = 0;
		meshBuilder.Begin( m_pMesh, MATERIAL_HETEROGENOUS, numVerts, numIndices );
		{
			m_pVertexBufferBase	= meshBuilder.Specular();
			m_totalVerts		= numVerts;
			m_vertexSize		= meshBuilder.VertexSize();
			// Probably good to catch any change to vertex size... there may be assumptions based on it:
			Assert( m_vertexSize == 4 );
			// Start at the bottom of the VB and work your way up like a simple stack
			m_nextFreeOffset	= 0;
		}
		meshBuilder.End();
	}

	if ( g_VBAllocTracker )
		g_VBAllocTracker->TrackMeshAllocations( NULL );

	return ( m_pMesh != NULL );
}

//-----------------------------------------------------------------------------
// Clear
//  - frees the shared mesh (vertex buffer), resets member variables
//-----------------------------------------------------------------------------
void CPooledVBAllocator_ColorMesh::Clear( void )
{
	if ( m_pMesh != NULL )
	{
		if ( m_numAllocations > 0 )
		{
			Warning( "ERROR: CPooledVBAllocator_ColorMesh::Clear should not be called until all allocations released!" );
			Assert( m_numAllocations == 0 );
		}
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->DestroyStaticMesh( m_pMesh );
		m_pMesh = NULL;
	}

	m_pVertexBufferBase		= NULL;
	m_totalVerts			= 0;
	m_vertexSize			= 0;

	m_numAllocations		= 0;
	m_numVertsAllocated		= 0;
	m_nextFreeOffset		= -1;
	m_bStartedDeallocation	= false;
}

//-----------------------------------------------------------------------------
// CheckIsClear
//  - assert/warn if the allocator isn't in a clear state
//    (no extant allocations, no internal mesh)
//-----------------------------------------------------------------------------
bool CPooledVBAllocator_ColorMesh::CheckIsClear( void )
{
	if ( m_pMesh )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh's internal mesh (vertex buffer) should have been freed!" );
		Assert( m_pMesh == NULL );
		return false;
	}

	if ( m_numAllocations > 0 )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh has unfreed allocations!" );
		Assert( m_numAllocations == 0 );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Allocate
//  - Allocate a sub-range of 'numVerts' from free space in the shared vertex buffer
//    (returns the byte offset from the start of the VB to the new allocation)
//  - returns -1 on failure
//-----------------------------------------------------------------------------
int CPooledVBAllocator_ColorMesh::Allocate( int numVerts )
{
	if ( m_pMesh == NULL )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Allocate cannot be called before Init (expect a crash)" );
		Assert( m_pMesh );
		return -1;
	}

	// Once we start deallocating, we have to keep going until everything has been freed
	if ( m_bStartedDeallocation )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Allocate being called after some (but not all) calls to Deallocate have been called - invalid! (expect visual artifacts)" );
		Assert( !m_bStartedDeallocation );
		return -1;
	}

	if ( numVerts > ( m_totalVerts - m_numVertsAllocated ) )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Allocate failing - not enough space left in the vertex buffer!" );
		Assert( numVerts <= ( m_totalVerts - m_numVertsAllocated ) );
		return -1;
	}

	int result = m_nextFreeOffset;

	m_numAllocations	+= 1;
	m_numVertsAllocated	+= numVerts;
	m_nextFreeOffset	+= numVerts*m_vertexSize;

	return result;
}

//-----------------------------------------------------------------------------
// Deallocate
//  - Deallocate an existing allocation
//-----------------------------------------------------------------------------
void CPooledVBAllocator_ColorMesh::Deallocate( int offset, int numVerts )
{
	if ( m_pMesh == NULL )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Deallocate cannot be called before Init" );
		Assert( m_pMesh != NULL );
		return;
	}

	if ( m_numAllocations == 0 )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Deallocate called too many times! (bug in calling code)" );
		Assert( m_numAllocations > 0 );
		return;
	}

	if ( numVerts > m_numVertsAllocated )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Deallocate called with too many verts, trying to free more than were allocated (bug in calling code)" );
		Assert( numVerts <= m_numVertsAllocated );
		numVerts = m_numVertsAllocated; // Hack (avoid counters ever going below zero)
	}

	// Now all extant allocations must be freed before we make any new allocations
	m_bStartedDeallocation = true;

	m_numAllocations	-= 1;
	m_numVertsAllocated	-= numVerts;
	m_nextFreeOffset	 = 0; // (we shouldn't be returning this until everything's free, at which point 0 is valid)

	// Are we empty?
	if ( m_numAllocations == 0 )
	{
		if ( m_numVertsAllocated != 0 )
		{
			Warning( "ERROR: CPooledVBAllocator_ColorMesh::Deallocate, after all allocations have been freed too few verts total have been deallocated (bug in calling code)" );
			Assert( m_numVertsAllocated == 0 );
		}

		// We can start allocating again, now
		m_bStartedDeallocation = false;
	}
}

//-----------------------------------------------------------------------------
// CreateLightmapsFromData
//  - Creates Lightmap Textures from data that was squirreled away during ASYNC load.
//  This is necessary because the material system doesn't like us creating things from ASYNC loaders.
//-----------------------------------------------------------------------------
static void CreateLightmapsFromData(CColorMeshData* _colorMeshData)
{
	Assert(_colorMeshData->m_bColorTextureValid);
	Assert(!_colorMeshData->m_bColorTextureCreated);

	for (int mesh = 0; mesh < _colorMeshData->m_nMeshes; ++mesh)
	{
		ColorMeshInfo_t* meshInfo = &_colorMeshData->m_pMeshInfos[mesh];

		// Ensure that we haven't somehow already messed with these.
		Assert(meshInfo->m_pLightmapData);
		Assert(!meshInfo->m_pLightmap);

		ColorTexelsInfo_t* cti = meshInfo->m_pLightmapData;

		Assert(cti->m_pTexelData);
		
		meshInfo->m_pLightmap = g_pMaterialSystem->CreateTextureFromBits(cti->m_nWidth, cti->m_nHeight, cti->m_nMipmapCount, cti->m_ImageFormat, cti->m_nByteCount, cti->m_pTexelData);

		// If this triggers, we need to figure out if it's reasonable to fail. If it is, then we should figure out how to signal back
		// that we shouldn't try to create this again (probably by clearing _colorMeshData->m_bColoTextureValid)
		Assert(meshInfo->m_pLightmap);

		// Cleanup after ourselves.
		delete [] cti->m_pTexelData;
		delete cti;

		meshInfo->m_pLightmapData = NULL;
	}

	_colorMeshData->m_bColorTextureCreated = true;
}
