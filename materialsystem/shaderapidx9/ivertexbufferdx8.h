//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef IVERTEXBUFFERDX8_H
#define IVERTEXBUFFERDX8_H
#pragma once

#include "IVertexBuffer.h"

abstract_class IVertexBufferDX8 : public IVertexBuffer
{
public:
	// TEMPORARY!
	virtual int Begin( int flags, int numVerts ) = 0;

	// Sets up the renderstate
	virtual void SetRenderState( int stream	) = 0;

	// Gets FVF info
	virtual void ComputeFVFInfo( int flags, int& fvf, int& size ) const = 0;

	// Cleans up the vertex buffers
	virtual void CleanUp() = 0;

	// Flushes the vertex buffers
	virtual void Flush() = 0;
};

#endif // IVERTEXBUFFERDX8_H
