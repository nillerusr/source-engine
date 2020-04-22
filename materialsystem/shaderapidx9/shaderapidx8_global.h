//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERAPIDX8_GLOBAL_H
#define SHADERAPIDX8_GLOBAL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "shaderapi_global.h"
#include "tier2/tier2.h"
#include "shaderdevicedx8.h"


//-----------------------------------------------------------------------------
// Use this to fill in structures with the current board state
//-----------------------------------------------------------------------------

#ifdef _DEBUG
#define DEBUG_BOARD_STATE 0
#endif

#if !defined( _X360 )
IDirect3DDevice9 *Dx9Device();
IDirect3D9 *D3D();
#endif


//-----------------------------------------------------------------------------
// Measures driver allocations
//-----------------------------------------------------------------------------
//#define MEASURE_DRIVER_ALLOCATIONS 1


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IShaderUtil;
class IVertexBufferDX8;
class IShaderShadowDX8;
class IMeshMgr;
class IShaderAPIDX8;
class IFileSystem;
class IShaderManager;


//-----------------------------------------------------------------------------
// A new shader draw flag we need to workaround dx8
//-----------------------------------------------------------------------------
enum
{
	SHADER_HAS_CONSTANT_COLOR = 0x80000000
};

//-----------------------------------------------------------------------------
// The main shader API
//-----------------------------------------------------------------------------
extern IShaderAPIDX8 *g_pShaderAPIDX8;
inline IShaderAPIDX8* ShaderAPI()
{
	return g_pShaderAPIDX8;
}

//-----------------------------------------------------------------------------
// The shader shadow
//-----------------------------------------------------------------------------
IShaderShadowDX8* ShaderShadow();

//-----------------------------------------------------------------------------
// Manager of all vertex + pixel shaders
//-----------------------------------------------------------------------------
inline IShaderManager *ShaderManager()
{
	extern IShaderManager *g_pShaderManager;
	return g_pShaderManager;
}

//-----------------------------------------------------------------------------
// The mesh manager
//-----------------------------------------------------------------------------
IMeshMgr* MeshMgr();

//-----------------------------------------------------------------------------
// The main hardware config interface
//-----------------------------------------------------------------------------
inline IMaterialSystemHardwareConfig* HardwareConfig()
{	
	return g_pMaterialSystemHardwareConfig;
}


#endif // SHADERAPIDX8_GLOBAL_H
