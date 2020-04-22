//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Revision: $
// $NoKeywords: $
//
// This file contains a little interface to deal with pooled vertex buffer allocations
// (which is used to allow multiple meshes to own sub-ranges within a single vertex buffer)
//
//=============================================================================//

#ifndef IPOOLEDVBALLOCATOR_H
#define IPOOLEDVBALLOCATOR_H

//-----------------------------------------------------------------------------
// Pooled VB allocator abstract base class
//-----------------------------------------------------------------------------
abstract_class IPooledVBAllocator
{
public:

	virtual ~IPooledVBAllocator() {};

	// Allocate the shared vertex buffer
	virtual bool			Init( VertexFormat_t format, int numVerts ) = 0;
	// Free the shared vertex buffer (after Deallocate is called for all sub-allocs)
	virtual void			Clear() = 0;

	// Get the shared mesh (vertex buffer) from which sub-allocations are made
	virtual IMesh			*GetSharedMesh() = 0;

	// Get a pointer to the start of the vertex buffer data
	virtual void			*GetVertexBufferBase() = 0;
	virtual int				GetNumVertsAllocated() = 0; 

	// Allocate a sub-range of 'numVerts' from free space in the shared vertex buffer
	// (returns the byte offset from the start of the VB to the new allocation)
	virtual int				Allocate( int numVerts ) = 0;
	virtual void			Deallocate( int offset, int numVerts ) = 0;
};


#endif	// IPOOLEDVBALLOCATOR_H
