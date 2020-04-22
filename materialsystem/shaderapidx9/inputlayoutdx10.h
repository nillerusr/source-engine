//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef INPUTLAYOUTDX10_H
#define INPUTLAYOUTDX10_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imaterial.h"


//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
struct ID3D10InputLayout;
struct ID3D10ShaderReflection;


//-----------------------------------------------------------------------------
// Gets the input layout associated with a vertex format
// FIXME: Note that we'll need to change this from a VertexFormat_t
//-----------------------------------------------------------------------------
ID3D10InputLayout *CreateInputLayout( VertexFormat_t fmt, 
	 ID3D10ShaderReflection* pReflection, const void *pByteCode, size_t nByteCodeLen );


#endif // INPUTLAYOUTDX10_H 

