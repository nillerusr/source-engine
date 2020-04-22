//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//
// Shader system:
//	The shader system makes a few fundamental assumptions about when 
//	certain types of state get set.
//
// 1) Anything that can potentially affect vertex format must be set up
//		during the shader shadow/snapshot phase
// 2) Anything that we can dynamically mess with (through a material var)
//		should happen in the dynamic/render phase
// 3) In general, we try to cache off expensive state pre-processing in
//		the shader shadow phase (like texture stage pipeline).
//
//=============================================================================//

#ifndef SHADERSYSTEM_H
#define SHADERSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "IShaderSystem.h"
#include "shaderlib/BaseShader.h"
#include "materialsystem/materialsystem_config.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem_global.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IMaterialVar;
class TextureManager_t;
class ITextureInternal;
class ShaderSystem_t;
class IMesh;
class IVertexBuffer;
class IIndexBuffer;
class Vector;
enum MaterialPrimitiveType_t;
enum MaterialPropertyTypes_t;
enum MaterialIndexFormat_t;
enum ShaderParamType_t;


//-----------------------------------------------------------------------------
// for ShaderRenderState_t::m_flags
//-----------------------------------------------------------------------------
enum
{
	// The flags up here are computed from the shaders themselves

/*
	// lighting flags
	SHADER_UNLIT					= 0x0000,
	SHADER_VERTEX_LIT				= 0x0001,
	SHADER_NEEDS_LIGHTMAP			= 0x0002, 
	SHADER_NEEDS_BUMPED_LIGHTMAPS	= 0x0004,
	SHADER_LIGHTING_MASK			= 0x0007,
*/

	// opacity flags
	SHADER_OPACITY_ALPHATEST		= 0x0010,
	SHADER_OPACITY_OPAQUE			= 0x0020,
	SHADER_OPACITY_TRANSLUCENT		= 0x0040,
	SHADER_OPACITY_MASK				= 0x0070,
};


enum
{
	MAX_RENDER_PASSES = 4
};


//-----------------------------------------------------------------------------
// Information for a single render pass
//-----------------------------------------------------------------------------
struct RenderPassList_t
{
	int m_nPassCount;
	StateSnapshot_t	m_Snapshot[MAX_RENDER_PASSES];
	// per material shader-defined state
	CBasePerMaterialContextData *m_pContextData[MAX_RENDER_PASSES];
};

struct ShaderRenderState_t
{
	// These are the same, regardless of whether alpha or color mod is used
	int				m_Flags;	// Can't shrink this to a short
	VertexFormat_t	m_VertexFormat;
	VertexFormat_t	m_VertexUsage;
	MorphFormat_t	m_MorphFormat;

	// List of all snapshots
	RenderPassList_t *m_pSnapshots;


};


//-----------------------------------------------------------------------------
// Used to get the snapshot count
//-----------------------------------------------------------------------------
enum
{
	SNAPSHOT_COUNT_NORMAL = 16,
	SNAPSHOT_COUNT_EDITOR = 32,
};

inline int SnapshotTypeCount()
{
	return MaterialSystem()->CanUseEditorMaterials() ? SNAPSHOT_COUNT_EDITOR : SNAPSHOT_COUNT_NORMAL;
}


//-----------------------------------------------------------------------------
// Utility methods
//-----------------------------------------------------------------------------
inline void SetFlags( IMaterialVar **params, MaterialVarFlags_t _flag )
{
	params[FLAGS]->SetIntValue( params[FLAGS]->GetIntValueFast() | (_flag) );
}

inline void SetFlags2( IMaterialVar **params, MaterialVarFlags2_t _flag )
{
	params[FLAGS2]->SetIntValue( params[FLAGS2]->GetIntValueFast() | (_flag) );
}

inline bool IsFlagSet( IMaterialVar **params, MaterialVarFlags_t _flag )
{
	return ((params[FLAGS]->GetIntValueFast() & (_flag) ) != 0);
}

inline bool IsFlag2Set( IMaterialVar **params, MaterialVarFlags2_t _flag )
{
	return ((params[FLAGS2]->GetIntValueFast() & (_flag) ) != 0);
}



//-----------------------------------------------------------------------------
// Poll params + renderstate
//-----------------------------------------------------------------------------
inline bool	IsTranslucent( const ShaderRenderState_t* pRenderState )
{
	return (pRenderState->m_Flags & SHADER_OPACITY_TRANSLUCENT) != 0;
}

inline bool	IsAlphaTested( ShaderRenderState_t* pRenderState )
{
	return (pRenderState->m_Flags & SHADER_OPACITY_ALPHATEST) != 0;
}


//-----------------------------------------------------------------------------
// The shader system (a singleton)
//-----------------------------------------------------------------------------
abstract_class IShaderSystemInternal : public IShaderInit, public IShaderSystem
{
public:
	// Initialization, shutdown
	virtual void		Init() = 0;
	virtual void		Shutdown() = 0;
	virtual void		ModInit() = 0;
	virtual void		ModShutdown() = 0;

	// Methods related to reading in shader DLLs
	virtual bool		LoadShaderDLL( const char *pFullPath ) = 0;
	virtual void		UnloadShaderDLL( const char *pFullPath ) = 0;

	// Find me a shader!
	virtual IShader*	FindShader( char const* pShaderName ) = 0;

	// returns strings associated with the shader state flags...
	virtual char const* ShaderStateString( int i ) const = 0;
	virtual int ShaderStateCount( ) const = 0;

	// Rendering related methods

	// Create debugging materials
	virtual void CreateDebugMaterials() = 0;

	// Cleans up the debugging materials
	virtual void CleanUpDebugMaterials() = 0;

	// Call the SHADER_PARAM_INIT block of the shaders
	virtual void InitShaderParameters( IShader *pShader, IMaterialVar **params, const char *pMaterialName ) = 0;

	// Call the SHADER_INIT block of the shaders
	virtual void InitShaderInstance( IShader *pShader, IMaterialVar **params, const char *pMaterialName, const char *pTextureGroupName ) = 0;

	// go through each param and make sure it is the right type, load textures, 
	// compute state snapshots and vertex types, etc.
	virtual bool InitRenderState( IShader *pShader, int numParams, IMaterialVar **params, ShaderRenderState_t* pRenderState, char const* pMaterialName ) = 0;

	// When you're done with the shader, be sure to call this to clean up
	virtual void CleanupRenderState( ShaderRenderState_t* pRenderState ) = 0;

	// Draws the shader
	virtual void DrawElements( IShader *pShader, IMaterialVar **params, ShaderRenderState_t* pShaderState, VertexCompressionType_t vertexCompression,
							   uint32 nMaterialVarTimeStamp ) = 0;

	// Used to iterate over all shaders for editing purposes
	virtual int	 ShaderCount() const = 0;
	virtual int  GetShaders( int nFirstShader, int nCount, IShader **ppShaderList ) const = 0;
};


#endif // SHADERSYSTEM_H
