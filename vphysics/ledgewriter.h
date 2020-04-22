//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef LEDGEWRITER_H
#define LEDGEWRITER_H
#ifdef _WIN32
#pragma once
#endif

#include "vphysics/virtualmesh.h"

// 2-sided triangle ledge rep
struct triangleledge_t
{
	IVP_Compact_Ledge		ledge;
	IVP_Compact_Triangle	faces[2];
};

// minimum footprint convex hull rep.  Assume 8-bits of index space per edge/triangle/vert
// NOTE: EACH ELEMENT OF THESE STRUCTS MUST BE 8-bits OR YOU HAVE TO WRITE SWAPPING CODE FOR 
// THE X360 IMPLEMENTATION.  THERE IS NO SUCH CODE AS OF NOW.
#pragma pack(1)
struct packedtriangle_t
{
	byte e0;	// only bytes allowed see above
	byte e1;
	byte e2;
	byte opposite;
};

struct packededge_t
{
	byte v0;	// only bytes allowed see above
	byte v1;
};

struct packedhull_t
{
	byte triangleCount; // only bytes allowed see above
	byte vtriCount;
	byte edgeCount;
	byte vedgeCount;
	byte baseVert;
	inline size_t DataSize() const
	{
		return (sizeof(packedtriangle_t) * triangleCount) + (sizeof(packededge_t)*edgeCount);
	}
};

struct virtualmeshhull_t
{
	byte		hullCount; // only bytes allowed see above
	byte		pad[3];
	
	inline const packedhull_t *GetPackedHull( int hullIndex ) const
	{
		Assert(hullIndex<hullCount);
		return ((const packedhull_t *)(this+1)) + hullIndex;
	}
	inline const packedtriangle_t *GetPackedTriangles( int hullIndex ) const
	{
		const packedhull_t *pHull = GetPackedHull(0);
		// the first triangle is immediately following the memory for the packed hulls
		const byte *pStart = reinterpret_cast<const byte *>(GetPackedHull(0) + hullCount);
		for ( int i = 0; i < hullIndex; i++ )
		{
			pStart += pHull[i].DataSize();
		}
		return reinterpret_cast<const packedtriangle_t *>(pStart);
	}
	inline const packededge_t *GetPackedEdges( int hullIndex ) const
	{
		return reinterpret_cast<const packededge_t *>(GetPackedTriangles(hullIndex) + GetPackedHull(hullIndex)->triangleCount);
	}
	inline size_t TotalSize() const
	{
		size_t size = sizeof(*this) + sizeof(packedhull_t) * hullCount;
		for ( int i = 0; i < hullCount; i++ )
		{
			size += GetPackedHull(i)->DataSize();
		}
		return size;

	}
};

#pragma pack()
// end
// NOTE: EACH ELEMENT OF THE ABOVE STRUCTS MUST BE 8-bits OR YOU HAVE TO WRITE SWAPPING CODE FOR 
// THE X360 IMPLEMENTATION.  THERE IS NO SUCH CODE AS OF NOW.


// These statics are grouped in a class so they can be friends of IVP_Compact_Ledge and access its private data
class CVPhysicsVirtualMeshWriter
{
public:
	// init a 2-sided triangle ledge
	static void InitTwoSidedTriangleLege( triangleledge_t *pOut, const IVP_Compact_Poly_Point *pPoints, int v0, int v1, int v2, int materialIndex );

	static virtualmeshhull_t *CreatePackedHullFromLedges( const virtualmeshlist_t &list, const IVP_Compact_Ledge **pLedges, int ledgeCount );
	static void UnpackCompactLedgeFromHull( IVP_Compact_Ledge *pLedge, int materialIndex, const IVP_Compact_Poly_Point *pPointList, const virtualmeshhull_t *pHullHeader, int hullIndex, bool isVirtualLedge );
	static void DestroyPackedHull( virtualmeshhull_t *pHull );
	static bool LedgeCanBePacked(const IVP_Compact_Ledge *pLedge, const virtualmeshlist_t &list);
	static unsigned int UnpackLedgeListFromHull( byte *pOut, virtualmeshhull_t *pHull, IVP_Compact_Poly_Point *pPoints );
};

#endif // LEDGEWRITER_H
