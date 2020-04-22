//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#ifndef SHADERDLL_GLOBAL_H
#define SHADERDLL_GLOBAL_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IShaderSystem;


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
inline IShaderSystem *GetShaderSystem()
{
	extern IShaderSystem* g_pSLShaderSystem;
	return g_pSLShaderSystem;
}


#endif	// SHADERDLL_GLOBAL_H