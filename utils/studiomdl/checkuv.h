//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================


#pragma once


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CCheckUVCmd
{
public:
	int m_nOptGutterTexWidth;	// Width of texture for gutter check
	int m_nOptGutterTexHeight;	// Height of texture for gutter check
	int m_nOptGutterMin;		// Minimum number of pixels between polygon islands in UV space

	enum CheckMask_t
	{
		CHECK_UV_FLAG_NORMALIZED =	( 1 << 0 ),
		CHECK_UV_FLAG_OVERLAP =		( 1 << 1 ),
		CHECK_UV_FLAG_INVERSE =		( 1 << 2 ),
		CHECK_UV_FLAG_GUTTER =		( 1 << 3 ),
		CHECK_UV_ALL_FLAGS = ( CHECK_UV_FLAG_NORMALIZED | CHECK_UV_FLAG_OVERLAP | CHECK_UV_FLAG_INVERSE | CHECK_UV_FLAG_GUTTER )
	};

	int m_nOptChecks;

	CCheckUVCmd();

	void Clear();

	bool DoCheck( CheckMask_t eCheckMask ) const	{ return ( m_nOptChecks & eCheckMask ) == eCheckMask; }
	bool DoAnyCheck() const							{ return ( m_nOptChecks & CHECK_UV_ALL_FLAGS ) != 0; }
	void SetCheck( CheckMask_t eCheckMask )			{ m_nOptChecks |= ( CHECK_UV_ALL_FLAGS & eCheckMask ); }
	void ClearCheck( CheckMask_t eCheckMask )		{ m_nOptChecks &= ( CHECK_UV_ALL_FLAGS & ~eCheckMask ); }

	bool CheckUVs( const struct s_source_t *const *pSourceList, int nSourceCount ) const;

	// Check that all UVs are in the [0, 1] range
	bool CheckNormalized( const struct s_source_t *pSource ) const;
	// Check that all polygons in UV do not overlap
	bool CheckOverlap( const struct s_source_t *pSource ) const;
	// Check that all polygons in UV have the correct winding, i.e. the cross
	// product of edge AB x BC points the right direction
	bool CheckInverse( const struct s_source_t *pSource ) const;
	// Check that the distance between edges in UV islands is a minimum number of pixels for a given texture size
	bool CheckGutter( const struct s_source_t *pSource ) const;

	// Returns barycentric coordinates Vector( u, v, w ) for point vP with respect to triangle ( vA, vB, vC )
	static Vector Barycentric( const Vector2D &vP, const Vector2D &vA, const Vector2D &vB, const Vector2D &vC );
	static Vector Barycentric( const Vector &vP, const Vector &vA, const Vector &vB, const Vector &vC );

	static int FindMeshIndex( const struct s_source_t *pSource, int nFaceIndex );
	static void PrintVertex( const struct s_vertexinfo_t &v, const char *pszPrefix = "       " );
	static void PrintVertex( const struct s_vertexinfo_t &v, const struct s_texture_t &t, const char *pszPrefix = "       " );
	static void PrintFace( const s_source_t *pSource, const int nMesh, const int nFace, const char *pszPrefix = "       " );
};