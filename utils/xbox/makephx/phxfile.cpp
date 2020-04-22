//========= Copyright Valve Corporation, All rights reserved. ============//
#include "stdafx.h"
#include "simplify.h"

extern IPhysicsCollision *physcollision;

vcollide_t *ConvertVCollideToPHX( const vcollide_t *pCollideIn, const simplifyparams_t &params, int *pSize, bool bStoreSurfaceprops, bool bStoreSolidNames )
{
	Assert( !pCollideIn->isPacked );
	vcollide_t *pCollideOut = new vcollide_t;
	pCollideOut->solids = new CPhysCollide *[pCollideIn->solidCount];
	pCollideOut->solidCount = pCollideIn->solidCount;
	*pSize = 0;

	for ( int i = 0; i < pCollideIn->solidCount; i++ )
	{
		Assert( pCollideIn->solids[i] );
		pCollideOut->solids[i] = SimplifyCollide( pCollideIn->solids[i], i, params );
		Assert( pCollideOut->solids[i] );
		int collideSize = physcollision->CollideSize( pCollideOut->solids[i] );
		*pSize += collideSize;
	}
	int packedTextSize;
	pCollideOut->pKeyValues = (char *)physcollision->PackVCollideText( pCollideIn->pKeyValues, &packedTextSize, bStoreSolidNames, bStoreSurfaceprops );
	pCollideOut->isPacked = true;
	pCollideOut->descSize = packedTextSize;
	*pSize += packedTextSize;

	return pCollideOut;
}


void DestroyPHX( vcollide_t *pCollide )
{
	for ( int i = 0; i < pCollide->solidCount; i++ )
	{
		physcollision->DestroyCollide( pCollide->solids[i] );
	}
	delete[] pCollide->solids;
	physcollision->DestroyVCollideText( pCollide->pKeyValues );
	delete pCollide;
}
