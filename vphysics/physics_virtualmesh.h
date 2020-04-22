//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef PHYSICS_VIRTUALMESH_H
#define PHYSICS_VIRTUALMESH_H
#ifdef _WIN32
#pragma once
#endif


CPhysCollide *CreateVirtualMesh( const virtualmeshparams_t &params );
void DestroyVirtualMesh( CPhysCollide *pSurf );
void DumpVirtualMeshStats();
void VirtualMeshPSI();

#endif // PHYSICS_VIRTUALMESH_H
