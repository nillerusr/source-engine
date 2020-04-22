//========= Copyright Valve Corporation, All rights reserved. ============//
#include "stdafx.h"
#include "simplify.h"
extern IPhysicsCollision *physcollision;

extern bool g_bQuiet;

const float DIST_EPSILON = 1.0f / 32.0f;
// this is the list of candidate planes that will be added one by one to the convex hull
// until none of the surface lies outside the tolerance
struct planetest_t
{
	Vector	normal;
	float	dist;
	int		inUse;
	float	bestDist;
	void Init( int axis, float sign, float _dist, bool _inUse = false )
	{
		memset( this, 0, sizeof(*this) );
		normal[axis] = sign;
		dist = sign*_dist;
		inUse = _inUse;
		bestDist = -1;
	}
	void Init( const Vector &a, const Vector &b, const Vector &c, bool _inUse = false )
	{
		Vector e0 = b-a;
		Vector e1 = c-a;
		normal = CrossProduct( e1, e0 );
		VectorNormalize( normal );
		dist = DotProduct( normal, a );
		inUse = _inUse;
		bestDist = -1;
	}
};

CPhysConvex *ConvertPlaneListToConvex( CUtlVector<planetest_t> &list )
{
	float temp[4 * 2048];
	struct listplane_t
	{
		float plane[4];
	};

	int planeCount = 0;
	listplane_t *pList = (listplane_t *)temp;
	for ( int i = 0; i < list.Count(); i++ )
	{
		if ( list[i].inUse )
		{
			list[i].normal.CopyToArray( pList[planeCount].plane );
			pList[planeCount].plane[3] = list[i].dist;
			planeCount++;
		}
	}

	return physcollision->ConvexFromPlanes( temp, planeCount, 0.25f );
}

Vector BoxSupport( const Vector &dir, const Vector &mins, const Vector &maxs )
{
	Vector out;
	for ( int i = 0; i < 3; i++ )
	{
		out[i] = (dir[i] >= 0) ? maxs[i] : mins[i];
	}
	return out;
}

struct convexoptimize_t
{
	CUtlVector<planetest_t> list;
	float		targetTolerance;

	void InitPlanes( CPhysCollide *pCollide, bool addAABBToSimplifiedHull )
	{
		Vector mins, maxs;
		physcollision->CollideGetAABB( &mins, &maxs, pCollide, vec3_origin, vec3_angle );
		if ( !addAABBToSimplifiedHull )
		{
			mins -= Vector(targetTolerance,targetTolerance,targetTolerance);
			maxs += Vector(targetTolerance,targetTolerance,targetTolerance);
		}
		int i;
		for ( i = 0; i < 3; i++ )
		{
			planetest_t &elem = list[list.AddToTail()];
			elem.Init( i, 1.0f, maxs[i], true );
			planetest_t &elem2 = list[list.AddToTail()];
			elem2.Init( i, -1.0f, mins[i], true );
		}
		ICollisionQuery *pQuery = physcollision->CreateQueryModel( pCollide );
		Vector triVerts[3];
		for ( i = 0; i < pQuery->TriangleCount(0); i++ )
		{
			pQuery->GetTriangleVerts( 0, i, triVerts );
			planetest_t &elem = list[list.AddToTail()];
			elem.Init( triVerts[0], triVerts[1], triVerts[2], false );
			elem.bestDist = DotProduct( elem.normal, BoxSupport(elem.normal, mins, maxs) ) - elem.dist;
		}
		physcollision->DestroyQueryModel( pQuery );
	}

	CPhysConvex *ConvertToConvex()
	{
		return ::ConvertPlaneListToConvex( list );
	}

	int FindBestPlane( float dist )
	{
		int best = -1;
		for ( int i = 6; i < list.Count(); i++ )
		{
			if ( list[i].inUse )
				continue;
			if ( dist >= list[i].bestDist )
				continue;
			dist = list[i].bestDist;
			best = i;
		}
		return best;
	}

	bool AddBestPlane()
	{
		convertconvexparams_t params;
		params.Defaults();
		CPhysConvex *pConvex = ConvertPlaneListToConvex( list );
		CPhysCollide *pCurrentCollide = physcollision->ConvertConvexToCollideParams( &pConvex, 1, params );
		int bestIndex = -1;
		float bestDist = 0;
		while ( true )
		{
			if ( bestIndex >= 0 )
			{
				list[bestIndex].inUse = true;
			}
			int test = FindBestPlane( bestDist );
			if ( test < 0 )
				break;
			if ( bestIndex >= 0 )
			{
				list[bestIndex].inUse = false;
			}
			Vector dir = list[test].normal;
			Vector point = physcollision->CollideGetExtent( pCurrentCollide, vec3_origin, vec3_angle, dir );
			float before = DotProduct( dir, point );
			list[test].inUse = true;
			pConvex = ConvertToConvex();
			list[test].inUse = false;
			CPhysCollide *pCollide = physcollision->ConvertConvexToCollideParams( &pConvex, 1, params );
			Vector p2 = physcollision->CollideGetExtent( pCollide, vec3_origin, vec3_angle, dir );
			physcollision->DestroyCollide( pCollide );
			float after = DotProduct( dir, p2 );
			list[test].bestDist = fabs(before-after);
			if ( list[test].bestDist > bestDist )
			{
				bestDist = list[test].bestDist;
				bestIndex = test;
			}
		}
		physcollision->DestroyCollide( pCurrentCollide );

		if ( bestIndex >= 0 && bestDist >= targetTolerance )
		{
			list[bestIndex].inUse = true;
			return true;
		}

		return false;
	}
};

CPhysConvex *SimplifyConvexFromVerts( Vector **verts, int vertCount, bool addAABBToSimplifiedHull, float tolerance, int index )
{
	CPhysConvex *pConvex = physcollision->ConvexFromVerts( verts, vertCount );
	float targetVolume = physcollision->ConvexVolume( pConvex );
	// can't simplify this polyhedron
	if ( vertCount <= 8 )
		return pConvex;

	convexoptimize_t opt;
	memset( &opt, 0, sizeof(opt));
	opt.targetTolerance = tolerance;
	convertconvexparams_t params;
	params.Defaults();
	CPhysCollide *pRef = physcollision->ConvertConvexToCollideParams( &pConvex, 1, params );
	opt.InitPlanes( pRef, addAABBToSimplifiedHull );
	physcollision->DestroyCollide( pRef );

	// Simplify until you hit the tolerance
	int i;
	for ( i = 0; i < vertCount; i++ )
	{
		if ( !opt.AddBestPlane() )
			break;
	}

	// Create the output shape
	pConvex = opt.ConvertToConvex();
	float currentVolume = physcollision->ConvexVolume( pConvex );
	//Msg("%d iterations, for convex %d\n", i, index );

	return pConvex;
}

inline int AddVert( Vector **ppVerts, int vertCount, const Vector &newVert )
{
	for ( int i = 0; i < vertCount; i++ )
	{
		if ( fabs(ppVerts[i]->x - newVert.x) < DIST_EPSILON && 
			fabs(ppVerts[i]->y - newVert.y) < DIST_EPSILON && 
			fabs(ppVerts[i]->z - newVert.z) < DIST_EPSILON )
			return vertCount;
	}
	*ppVerts[vertCount] = newVert;
	return vertCount+1;
}

void BuildSingleConvex( CPhysConvex **convexListOut, ICollisionQuery *pQuery, Vector **ppVerts, const simplifyparams_t &params )
{
	int vertCount = 0;
	for ( int i = 0; i < pQuery->ConvexCount(); i++ )
	{
		Vector v[3];
		for ( int j = 0; j < pQuery->TriangleCount(i); j++ )
		{
			pQuery->GetTriangleVerts( i, j, v );
			vertCount = AddVert( ppVerts, vertCount, v[0] );
			vertCount = AddVert( ppVerts, vertCount, v[1] );
			vertCount = AddVert( ppVerts, vertCount, v[2] );
		}
	}
	convexListOut[0] = SimplifyConvexFromVerts( ppVerts, vertCount, params.addAABBToSimplifiedHull, params.tolerance, 0 );
	physcollision->SetConvexGameData( convexListOut[0], pQuery->GetGameData( 0 ) );
}

void SimplifyConvexElements( CPhysConvex **convexListOut, ICollisionQuery *pQuery, Vector **ppVerts, const simplifyparams_t &params )
{
	for ( int i = 0; i < pQuery->ConvexCount(); i++ )
	{
		int vertCount = 0;
		Vector v[3];
		for ( int j = 0; j < pQuery->TriangleCount(i); j++ )
		{
			pQuery->GetTriangleVerts( i, j, v );
			vertCount = AddVert( ppVerts, vertCount, v[0] );
			vertCount = AddVert( ppVerts, vertCount, v[1] );
			vertCount = AddVert( ppVerts, vertCount, v[2] );
		}
		convexListOut[i] = SimplifyConvexFromVerts( ppVerts, vertCount, params.addAABBToSimplifiedHull, params.tolerance, i );
		physcollision->SetConvexGameData( convexListOut[i], pQuery->GetGameData( i ) );
	}
}

struct mergeconvex_t
{
	byte mergeCount;
	byte list[255];
};

void MergeElems( CUtlVector<mergeconvex_t> &elems, int index0, int index1 )
{
	Assert( index0 < index1 );
	for (int i = 0; i < elems[index1].mergeCount; i++)
	{
		elems[index0].list[i+elems[index0].mergeCount] = elems[index1].list[i];
	}
	elems[index0].mergeCount += elems[index1].mergeCount;
	elems.FastRemove(index1);
}

int VertsForElem( ICollisionQuery *pQuery, Vector **ppVerts, const mergeconvex_t &elems0, int vertCount )
{
	for ( int i = 0; i < elems0.mergeCount; i++ )
	{
		int convexId = elems0.list[i];
		Vector v[3];
		for ( int j = 0; j < pQuery->TriangleCount(convexId); j++ )
		{
			pQuery->GetTriangleVerts( convexId, j, v );
			vertCount = AddVert( ppVerts, vertCount, v[0] );
			vertCount = AddVert( ppVerts, vertCount, v[1] );
			vertCount = AddVert( ppVerts, vertCount, v[2] );
		}
	}
	return vertCount;
}

void PlanesForElem( ICollisionQuery *pQuery, CUtlVector<float> &planes, const mergeconvex_t &elem0 )
{
	for ( int i = 0; i < elem0.mergeCount; i++ )
	{
		int convexId = elem0.list[i];
		Vector v[3];
		for ( int j = 0; j < pQuery->TriangleCount(convexId); j++ )
		{
			pQuery->GetTriangleVerts( convexId, j, v );
			Vector e0 = v[1]-v[0];
			Vector e1 = v[2]-v[0];
			Vector normal = CrossProduct( e1, e0 );
			VectorNormalize( normal );
			float dist = DotProduct( normal, v[0] );
			planes.AddToTail( normal.x );
			planes.AddToTail( normal.y );
			planes.AddToTail( normal.z );
			planes.AddToTail( dist );
		}
	}
}

float ConvexVolumeFromPlanes( CUtlVector<float> &planes )
{
	CPhysConvex *pConvex = planes.Count() ? physcollision->ConvexFromPlanes( planes.Base(), planes.Count()/4, DIST_EPSILON ) : NULL;
	float volume = 0;
	if ( pConvex )
	{
		volume = physcollision->ConvexVolume(pConvex);
		physcollision->ConvexFree(pConvex);
	}
	return volume;
}

float MergedDeltaVolume( ICollisionQuery *pQuery, Vector **ppVerts, const mergeconvex_t &elem0, const mergeconvex_t &elem1 )
{
	// build vert list
	int vertCount = VertsForElem( pQuery, ppVerts, elem0, 0 );
	// merge in next element
	vertCount = VertsForElem( pQuery, ppVerts, elem1, vertCount);
	CPhysConvex *pConvex = physcollision->ConvexFromVerts( ppVerts, vertCount );
	float finalVolume = physcollision->ConvexVolume(pConvex);
	physcollision->ConvexFree(pConvex);

	CUtlVector<float> planes;
	PlanesForElem( pQuery, planes, elem0 );
	float vol0 = ConvexVolumeFromPlanes( planes );
	planes.RemoveAll();
	PlanesForElem( pQuery, planes, elem1 );
	float vol1 = ConvexVolumeFromPlanes( planes );
	PlanesForElem( pQuery, planes, elem0 );

	float volInt = ConvexVolumeFromPlanes( planes );

	return finalVolume - (vol0+vol1-volInt);
}

int MergeAndSimplifyConvexElements( CPhysConvex **convexListOut, const CPhysCollide *pCollideIn, ICollisionQuery *pQuery, Vector **ppVerts, const simplifyparams_t &params )
{
	Assert( pQuery->ConvexCount() < 256 );
	if ( pQuery->ConvexCount() > 256 )
	{
		SimplifyConvexElements(convexListOut, pQuery, ppVerts, params);
		return pQuery->ConvexCount();
	}

	CUtlVector<mergeconvex_t> elems;
	int i;
	elems.EnsureCount(pQuery->ConvexCount());
	float totalVolume = physcollision->CollideVolume( (CPhysCollide *)pCollideIn );
	for ( i = 0; i < pQuery->ConvexCount(); i++ )
	{
		elems[i].mergeCount = 1;
		elems[i].list[0] = i;
	}
loop:
	for ( i = 0; i < elems.Count(); i++ )
	{
		for ( int j = i+1; j < elems.Count(); j++ )
		{
			float volume = fabs(MergedDeltaVolume( pQuery, ppVerts, elems[i], elems[j] ));
			volume /= totalVolume;
			if ( volume < params.mergeConvexTolerance )
			{
				MergeElems( elems, i, j );
				goto loop;
			}
		}
	}

	for ( i = 0; i < elems.Count(); i++ )
	{
		int vertCount = VertsForElem( pQuery, ppVerts, elems[i], 0 );
		convexListOut[i] = SimplifyConvexFromVerts( ppVerts, vertCount, params.addAABBToSimplifiedHull, params.tolerance, i );
		physcollision->SetConvexGameData( convexListOut[i], pQuery->GetGameData( elems[i].list[0] ) );
	}
	return elems.Count();
}

CPhysCollide *SimplifyCollide( CPhysCollide *pCollideIn, int indexIn, const simplifyparams_t &params )
{
	int sizeIn = physcollision->CollideSize( pCollideIn );
	ICollisionQuery *pQuery = physcollision->CreateQueryModel( pCollideIn );
	int maxVertCount = 0;
	int i;
	for ( i = pQuery->ConvexCount(); --i >= 0; )
	{
		int vertCount = pQuery->TriangleCount(i)*3;
		maxVertCount += vertCount;
	}

	Vector **ppVerts = new Vector *[maxVertCount];
	Vector *verts = new Vector[maxVertCount];
	for ( i = 0; i < maxVertCount; i++ )
	{
		ppVerts[i] = &verts[i];
	}

	int outputConvexCount = params.forceSingleConvex ? 1 : pQuery->ConvexCount();
	CPhysConvex **convexList = new CPhysConvex *[outputConvexCount];
	if ( params.forceSingleConvex )
	{
		BuildSingleConvex( convexList, pQuery, ppVerts, params );
	}
	else if ( params.mergeConvexElements && pQuery->ConvexCount() > 1 )
	{
		outputConvexCount = MergeAndSimplifyConvexElements( convexList, pCollideIn, pQuery, ppVerts, params );
		if ( !g_bQuiet && pQuery->ConvexCount() != outputConvexCount)
		{
			Msg("Simplified %d to %d elements\n", pQuery->ConvexCount(), outputConvexCount );
		}
	}
	else
	{
		SimplifyConvexElements( convexList, pQuery, ppVerts, params );
	}
	convertconvexparams_t params;
	params.Defaults();
	params.buildOuterConvexHull = true;
	params.buildDragAxisAreas = false;

	CPhysCollide *pCollideOut = physcollision->ConvertConvexToCollideParams( convexList, outputConvexCount, params );
	
	// copy the drag axis areas from the source
	Vector dragAxisAreas = physcollision->CollideGetOrthographicAreas( pCollideIn );
	physcollision->CollideSetOrthographicAreas( pCollideOut, dragAxisAreas );

	physcollision->DestroyQueryModel( pQuery );
	delete[] convexList;
	delete[] verts;
	delete[] ppVerts;

	if ( physcollision->CollideSize(pCollideOut) >= sizeIn )
	{
		// make a copy of the input collide
		physcollision->DestroyCollide(pCollideOut);
		char *pBuf = new char[sizeIn];
		physcollision->CollideWrite( pBuf, pCollideIn );
		pCollideOut = physcollision->UnserializeCollide( pBuf, sizeIn, indexIn );
		delete[] pBuf;
	}
	return pCollideOut;
}

