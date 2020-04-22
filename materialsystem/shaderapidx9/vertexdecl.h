//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef VERTEXDECL_H
#define VERTEXDECL_H

#ifdef _WIN32
#pragma once
#endif

#include "locald3dtypes.h"
#include "materialsystem/imaterial.h"


//-----------------------------------------------------------------------------
// Gets the declspec associated with a vertex format
//-----------------------------------------------------------------------------
IDirect3DVertexDeclaration9 *FindOrCreateVertexDecl( VertexFormat_t fmt, bool bStaticLit, bool bUsingFlex, bool bUsingMorph );

//-----------------------------------------------------------------------------
// Clears out all declspecs
//-----------------------------------------------------------------------------
void ReleaseAllVertexDecl( );


#endif // VERTEXDECL_H 

