//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERAPI_GLOBAL_H
#define SHADERAPI_GLOBAL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/threadtools.h"

//-----------------------------------------------------------------------------
// Use this to fill in structures with the current board state
//-----------------------------------------------------------------------------
#ifdef _DEBUG
#define DEBUG_BOARD_STATE 0
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IShaderUtil;
class CShaderDeviceBase;
class CShaderDeviceMgrBase;
class CShaderAPIBase;
class IShaderShadow;


//-----------------------------------------------------------------------------
// Global interfaces
//-----------------------------------------------------------------------------
extern IShaderUtil* g_pShaderUtil;
inline IShaderUtil* ShaderUtil()
{
	return g_pShaderUtil;
}

extern CShaderDeviceBase *g_pShaderDevice;
extern CShaderDeviceMgrBase *g_pShaderDeviceMgr;
extern CShaderAPIBase *g_pShaderAPI;
extern IShaderShadow *g_pShaderShadow;


//-----------------------------------------------------------------------------
// Memory debugging 
//-----------------------------------------------------------------------------
#define MEM_ALLOC_D3D_CREDIT()	MEM_ALLOC_CREDIT_("D3D:" __FILE__)
#define BEGIN_D3D_ALLOCATION()	MemAlloc_PushAllocDbgInfo("D3D:" __FILE__, __LINE__)
#define END_D3D_ALLOCATION()	MemAlloc_PopAllocDbgInfo()


//-----------------------------------------------------------------------------
// Threading
//-----------------------------------------------------------------------------
extern bool g_bUseShaderMutex;

//#define USE_SHADER_DISALLOW 1
//#define STRICT_MT_SHADERAPI 1

#if defined(_DEBUG)
#if !defined(STRICT_MT_SHADERAPI)
#define UNCONDITIONAL_MT_SHADERAPI 1
#endif
#else
#if !defined(STRICT_MT_SHADERAPI) && !defined(UNCONDITIONAL_MT_SHADERAPI)
#define ST_SHADERAPI 1
#endif
#endif


#if defined(ST_SHADERAPI)
typedef CThreadNullMutex CShaderMutex;
#elif defined(STRICT_MT_SHADERAPI)
typedef CThreadConditionalMutex<CThreadTerminalMutex<CThreadFastMutex>, &g_bUseShaderMutex> CShaderMutex;
#elif defined(UNCONDITIONAL_MT_SHADERAPI)
typedef CThreadFastMutex CShaderMutex;
#else
typedef CThreadConditionalMutex<CThreadFastMutex, &g_bUseShaderMutex> CShaderMutex;
#endif

extern CShaderMutex g_ShaderMutex;

extern bool g_bShaderAccessDisallowed;

#ifdef USE_SHADER_DISALLOW
#define TestShaderPermission() do { if ( (!g_bUseShaderMutex || g_ShaderMutex.GetDepth() == 0) && g_bShaderAccessDisallowed ) { ExecuteOnce( DebuggerBreakIfDebugging() ); } } while (0)
#define LOCK_SHADERAPI() TestShaderPermission(); AUTO_LOCK_( CShaderMutex, g_ShaderMutex )
#define LockShaderMutex() TestShaderPermission(); g_ShaderMutex.Lock();
#define UnlockShaderMutex() TestShaderPermission(); g_ShaderMutex.Unlock();
#else
#define TestShaderPermission() ((void)0)
#define LOCK_SHADERAPI() ((void)0)
#define LockShaderMutex() ((void)0)
#define UnlockShaderMutex() ((void)0)
#endif


#endif // SHADERAPI_GLOBAL_H
