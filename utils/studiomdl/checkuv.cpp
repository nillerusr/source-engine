//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
// Function which do validation tests on UVs values at the s_source_t level
//
//=============================================================================


#include "tier1/fmtstr.h"
#include "tier1/utlmap.h"


#include "studiomdl.h"
#include "checkuv.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CCheckUVCmd::CCheckUVCmd()
{
	Clear();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCheckUVCmd::Clear()
{
	ClearCheck( CHECK_UV_ALL_FLAGS );
	m_nOptGutterTexWidth = 512;
	m_nOptGutterTexHeight = 512;
	m_nOptGutterMin = 5;
}


//-----------------------------------------------------------------------------
// Dumps an s_soruce_t as an OBJ file
//-----------------------------------------------------------------------------
static void WriteOBJ( const char *pszFilename, const s_source_t *pSource )
{
	FILE *pFile = fopen( pszFilename, "w" );

	fprintf( pFile, "#\n" );
	fprintf( pFile, "# s_source_t: %s\n", pSource->filename );
	fprintf( pFile, "# Bone Count: %d\n", pSource->numbones );
	
	for ( int i = 0; i < pSource->numbones; ++i )
	{
		if ( pSource->localBone[i].parent >= 0 )
		{
			fprintf( pFile, "# Bone %3d: %s Parent %3d: %s\n", i, pSource->localBone[i].name, pSource->localBone[i].parent, pSource->localBone[pSource->localBone[i].parent].name );
		}
		else
		{
			fprintf( pFile, "# Bone %3d: %s\n", i, pSource->localBone[i].name );
		}
	}

	fprintf( pFile, "# Mesh Count:   %d\n", pSource->nummeshes );
	fprintf( pFile, "# Vertex Count: %d\n", pSource->numvertices );
	fprintf( pFile, "# Face Count:   %d\n", pSource->numfaces );

	fprintf( pFile, "#\n" );
	fprintf( pFile, "# positions\n" );
	fprintf( pFile, "#\n" );

	for ( int i = 0; i < pSource->numvertices; ++i )
	{
		const s_vertexinfo_t &v = pSource->vertex[i];
		fprintf( pFile, "v %.4f %.4f %.4f\n", v.position.x, v.position.y, v.position.z );
	}

	fprintf( pFile, "#\n" );
	fprintf( pFile, "# texture coordinates\n" );
	fprintf( pFile, "#\n" );

	for ( int i = 0; i < pSource->numvertices; ++i )
	{
		const s_vertexinfo_t &v = pSource->vertex[i];
		fprintf( pFile, "vt %.4f %.4f\n", v.texcoord.x, v.texcoord.y );
	}

	fprintf( pFile, "#\n" );
	fprintf( pFile, "# normals\n" );
	fprintf( pFile, "#\n" );

	for ( int i = 0; i < pSource->numvertices; ++i )
	{
		const s_vertexinfo_t &v = pSource->vertex[i];
		fprintf( pFile, "vn %.4f %.4f %.4f\n", v.normal.x, v.normal.y, v.normal.z );
	}

	for ( int i = 0; i < pSource->nummeshes; ++i )
	{
		const s_mesh_t &m = pSource->mesh[i];
		const s_texture_t &t = g_texture[pSource->meshindex[i]];

		fprintf( pFile, "#\n" );
		fprintf( pFile, "# mesh %d - %s\n", i, t.name );
		fprintf( pFile, "# Face Count:   %d\n", m.numfaces );
		fprintf( pFile, "#\n" );
		fprintf( pFile, "usemtl %s\n", t.name );

		for ( int j = 0; j < m.numfaces; ++j )
		{
			const s_face_t &f = pSource->face[m.faceoffset + j];
			fprintf( pFile, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
				f.a, f.a, f.a,
				f.b, f.b, f.b,
				f.c, f.c, f.c );
		}
	}

	fclose( pFile );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CCheckUVCmd::CheckUVs( const s_source_t *const *pSourceList, int nSourceCount ) const
{
	if ( !DoAnyCheck() || nSourceCount <= 0 )
		return true;

	bool bRet = true;

	for ( int i = 0; i < nSourceCount; ++i )
	{
		const s_source_t *pSource = pSourceList[i];

		bRet &= CheckNormalized( pSource );
		bRet &= CheckOverlap( pSource );
		bRet &= CheckInverse( pSource );
		bRet &= CheckGutter( pSource );
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Check that all UVs are in the [0, 1] range
//-----------------------------------------------------------------------------
bool CCheckUVCmd::CheckNormalized( const struct s_source_t *pSource ) const
{
	if ( !DoCheck( CHECK_UV_FLAG_NORMALIZED ) )
		return true;

	CUtlRBTree< int > badVertexIndices( CDefOps< int >::LessFunc );

	for ( int i = 0; i < pSource->numvertices; ++i )
	{
		const s_vertexinfo_t &v = pSource->vertex[i];

		if (
			v.texcoord.x < 0.0f || v.texcoord.x > 1.0f ||
			v.texcoord.y < 0.0f || v.texcoord.y > 1.0f )
		{
			badVertexIndices.InsertIfNotFound( i );
		}
	}

	if ( badVertexIndices.Count() <= 0 )
		return true;

	Msg( "Error! %s\n", pSource->filename );
	Msg( "       UVs outside of [0, 1] range\n" );

	for ( int i = 0; i < pSource->nummeshes; ++i )
	{
		const s_mesh_t &m = pSource->mesh[i];
		const s_texture_t &t = g_texture[pSource->meshindex[i]];

		CUtlRBTree< int > badMeshVertexIndices( CDefOps< int >::LessFunc );

		for ( int j = 0; j < m.numfaces; ++j )
		{
			const s_face_t &f = pSource->face[m.faceoffset + j];

			if ( badVertexIndices.HasElement( f.a ) )
			{
				badMeshVertexIndices.InsertIfNotFound( f.a );
			}

			if ( badVertexIndices.HasElement( f.b ) )
			{
				badMeshVertexIndices.InsertIfNotFound( f.b );
			}

			if ( badVertexIndices.HasElement( f.c ) )
			{
				badMeshVertexIndices.InsertIfNotFound( f.c );
			}
		}

		for ( auto vIt = badMeshVertexIndices.FirstInorder(); badMeshVertexIndices.IsValidIndex( vIt ); vIt = badMeshVertexIndices.NextInorder( vIt ) )
		{
			PrintVertex( pSource->vertex[badMeshVertexIndices.Element( vIt )], t );
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Check that all polygons in UV do not overlap
//-----------------------------------------------------------------------------
bool CCheckUVCmd::CheckOverlap( const struct s_source_t *pSource ) const
{
	if ( !DoCheck( CHECK_UV_FLAG_OVERLAP ) )
		return true;

	bool bRet = true;

	CUtlVector< CUtlVector< int > > faceOverlapMap;
	faceOverlapMap.SetCount( pSource->numfaces );

	for ( int i = 0; i < pSource->numfaces; ++i )
	{
		const s_face_t &fA = pSource->face[i];

		const Vector2D &tAA = pSource->vertex[fA.a].texcoord;
		const Vector2D &tAB = pSource->vertex[fA.b].texcoord;
		const Vector2D &tAC = pSource->vertex[fA.c].texcoord;

		for ( int j = i + 1; j < pSource->numfaces; ++j )
		{
			const s_face_t &fB = pSource->face[j];

			const Vector2D tB[] = {
				pSource->vertex[fB.a].texcoord,
				pSource->vertex[fB.b].texcoord,
				pSource->vertex[fB.c].texcoord };

			for ( int k = 0; k < ARRAYSIZE( tB ); ++k )
			{
				const Vector vCheck = Barycentric( tB[k], tAA, tAB, tAC );

				if ( vCheck.x > 0.0f && vCheck.y > 0.0f && vCheck.z > 0.0f )
				{
					if ( bRet )
					{
						Msg( "Error! %s\n", pSource->filename );
						Msg( "       Overlapping UV faces\n" );

						bRet = false;
					}

					faceOverlapMap[i].AddToTail( j );

					break;
				}
			}
		}
	}

	for ( int i = 0; i < faceOverlapMap.Count(); ++i )
	{
		const CUtlVector< int > &overlapList = faceOverlapMap[i];
		
		if ( overlapList.IsEmpty() )
			continue;;

		const int nFaceA = i;
		const int nMeshA = FindMeshIndex( pSource, nFaceA );
		PrintFace( pSource, nMeshA, nFaceA );
		Msg( "       Overlaps\n" );

		for ( int j = 0; j < overlapList.Count(); ++j )
		{
			const int nFaceB = overlapList[j];
			const int nMeshB = FindMeshIndex( pSource, nFaceB );
			PrintFace( pSource, nMeshB, nFaceB, "                " );
		}
	}

	return bRet;
}


//-----------------------------------------------------------------------------
// Check that all polygons in UV have the correct winding, i.e. the cross
// product of edge AB x BC points the right direction
//-----------------------------------------------------------------------------
bool CCheckUVCmd::CheckInverse( const struct s_source_t *pSource ) const
{
	if ( !DoCheck( CHECK_UV_FLAG_INVERSE ) )
		return true;

	bool bRetVal = true;

	for ( int i = 0; i < pSource->nummeshes; ++i )
	{
		const s_mesh_t &m = pSource->mesh[i];

		for ( int j = 0; j < m.numfaces; ++j )
		{
			const int nFaceIndex = m.faceoffset + j;

			const s_face_t &f = pSource->face[nFaceIndex];

			const Vector2D &tA = pSource->vertex[f.a].texcoord;
			const Vector2D &tB = pSource->vertex[f.b].texcoord;
			const Vector2D &tC = pSource->vertex[f.c].texcoord;

			const Vector vA( tA.x, tA.y, 0.0f );
			const Vector vB( tB.x, tB.y, 0.0f );
			const Vector vC( tC.x, tC.y, 0.0f );

			const Vector vAB = vB - vA;
			const Vector vBC = vC - vB;

			const Vector vUVNormal = CrossProduct( vAB, vBC );

			const float flDot = DotProduct( vUVNormal, Vector( 0.0f, 0.0f, 1.0f ) );

			if ( flDot < 0.0f )
			{
				if ( bRetVal )
				{
					Msg( "Error! %s\n", pSource->filename );
					Msg( "       Inverse UV faces\n" );

					bRetVal = false;
				}

				PrintFace( pSource, i, nFaceIndex );
			}
		}
	}

	return bRetVal;
}


//-----------------------------------------------------------------------------
// Check that the distance between edges in UV islands is a minimum number of pixels for a given texture size
//-----------------------------------------------------------------------------
bool CCheckUVCmd::CheckGutter( const struct s_source_t *pSource ) const
{
	if ( !DoCheck( CHECK_UV_FLAG_GUTTER ) )
		return true;

	// TODO: Implement me!

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
Vector CCheckUVCmd::Barycentric( const Vector2D &vP, const Vector2D &vA, const Vector2D &vB, const Vector2D &vC )
{
	const Vector2D v0 = vB - vA;
	const Vector2D v1 = vC - vA;
	const Vector2D v2 = vP - vA;

	const float d00 = DotProduct2D( v0, v0 );
	const float d01 = DotProduct2D( v0, v1 );
	const float d11 = DotProduct2D( v1, v1 );
	const float d20 = DotProduct2D( v2, v0 );
	const float d21 = DotProduct2D( v2, v1 );

	const float flDenom = d00 * d11 - d01 * d01;

	const float flV = ( d11 * d20 - d01 * d21 ) / flDenom;
	const float flW = ( d00 * d21 - d01 * d20 ) / flDenom;
	const float flU = 1.0f - flV - flW;

	return Vector( flV, flW, flU );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
Vector CCheckUVCmd::Barycentric( const Vector &vP, const Vector &vA, const Vector &vB, const Vector &vC )
{
	const Vector v0 = vB - vA;
	const Vector v1 = vC - vA;
	const Vector v2 = vP - vA;

	const float d00 = DotProduct( v0, v0 );
	const float d01 = DotProduct( v0, v1 );
	const float d11 = DotProduct( v1, v1 );
	const float d20 = DotProduct( v2, v0 );
	const float d21 = DotProduct( v2, v1 );

	const float flDenom = d00 * d11 - d01 * d01;

	const float flV = ( d11 * d20 - d01 * d21 ) / flDenom;
	const float flW = ( d00 * d21 - d01 * d20 ) / flDenom;
	const float flU = 1.0f - flV - flW;

	return Vector( flV, flW, flU );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CCheckUVCmd::FindMeshIndex( const struct s_source_t *pSource, int nFaceIndex )
{
	for ( int i = 1; i < pSource->nummeshes; ++i )
	{
		if ( nFaceIndex <= pSource->mesh[i].faceoffset )
			return i;
	}

	return 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCheckUVCmd::PrintVertex( const s_vertexinfo_t &v, const char *pszPrefix /* = "       " */ )
{
	Msg( "%sP: %8.4f %8.4f %8.4f T: %8.4f %8.4f\n",
		pszPrefix,
		v.position.x, v.position.y, v.position.z,
		v.texcoord.x, v.texcoord.y );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCheckUVCmd::PrintVertex( const s_vertexinfo_t &v, const s_texture_t &t, const char *pszPrefix /* = "       " */ )
{
	Msg( "%sP: %8.4f %8.4f %8.4f T: %8.4f %8.4f M: %s\n",
		pszPrefix,
		v.position.x, v.position.y, v.position.z,
		v.texcoord.x, v.texcoord.y,
		t.name );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CCheckUVCmd::PrintFace( const s_source_t *pSource, const int nMesh, const int nFace, const char *pszPrefix /* = "       " */ )
{
	const s_texture_t &t = g_texture[pSource->meshindex[nMesh]];
	const s_face_t &f = pSource->face[nFace];

	Msg( "%sF: %4d %s\n", pszPrefix, nFace, t.name );

	PrintVertex( pSource->vertex[f.a], pszPrefix );
	PrintVertex( pSource->vertex[f.b], pszPrefix );
	PrintVertex( pSource->vertex[f.c], pszPrefix );
}