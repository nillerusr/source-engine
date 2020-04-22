//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef SIMPLIFY_H
#define SIMPLIFY_H
#pragma once

struct simplifyparams_t
{
	float	tolerance;
	bool	addAABBToSimplifiedHull;
	bool	forceSingleConvex;
	bool	mergeConvexElements;
	float	mergeConvexTolerance;

	void Defaults()
	{
		tolerance = 1.0f;
		addAABBToSimplifiedHull = false;
		forceSingleConvex = false;
		mergeConvexElements = false;
		mergeConvexTolerance = 0.f;
	}
};

extern CPhysCollide *SimplifyCollide( CPhysCollide *pCollideIn, int index, const simplifyparams_t &params );

#endif // SIMPLIFY_H
