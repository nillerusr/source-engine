//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
//#include "studio.h"
#include "studio.h"
#include "datacache/imdlcache.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "istudiorender.h"
#include "bone_setup.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// FIXME: This trashy glue code is really not acceptable. Figure out a way of making it unnecessary.
//-----------------------------------------------------------------------------
const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const
{
	MDLHandle_t handle = g_pMDLCache->FindMDL( pModelName );
	*cache = (void*)(uintp)handle;
	return g_pMDLCache->GetStudioHdr( handle );
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	return g_pMDLCache->GetVirtualModel( VoidPtrToMDLHandle( VirtualModel() ) );
}

byte *studiohdr_t::GetAnimBlock( int i ) const
{
	return g_pMDLCache->GetAnimBlock( VoidPtrToMDLHandle( VirtualModel() ), i );
}

int studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return g_pMDLCache->GetAutoplayList( VoidPtrToMDLHandle( VirtualModel() ), pOut );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	return g_pMDLCache->GetStudioHdr( VoidPtrToMDLHandle( cache ) );
}

