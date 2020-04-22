//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Heightfield class
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "heightfield.h"
#include "materialsystem/imaterial.h"
#include "legion.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "tier2/tier2.h"
#include "tier2/utlstreambuffer.h"
#include "bitmap/bitmap.h"
#include "bitmap/psd.h"
#include "tier1/KeyValues.h"


//-----------------------------------------------------------------------------
// Utility macro
//-----------------------------------------------------------------------------
#define HEIGHT( _x, _y )	m_pHeightField[ ( (_y) << m_nPowX ) + (_x) ]
#define ROW( _y )			&m_pHeightField[ (_y) << m_nPowX ]


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CHeightField::CHeightField( int nPowX, int nPowY, int nPowScale )
{
	m_nPowX = nPowX;
	m_nPowY = nPowY;
	m_nPowScale = nPowScale;
	m_nWidth = ( 1 << nPowX );
	m_nHeight = ( 1 << nPowY );
	m_nScale = ( 1 << nPowScale );
	m_flOOScale = 1.0f / m_nScale;
	m_pHeightField = (float*)malloc( m_nWidth * m_nHeight * sizeof(float) );
	memset( m_pHeightField, 0, m_nWidth * m_nHeight * sizeof(float) );

	KeyValues *pKeyValues = new KeyValues( "Wireframe" );
	pKeyValues->SetInt( "$nocull", 1 );
	m_Material.Init( "__Temp", pKeyValues );
}

CHeightField::~CHeightField()
{
	free( m_pHeightField );
}


//-----------------------------------------------------------------------------
// Bilinearly filters a sample out of a bitmap at a particular (x,y)
// NOTE: x,y are not normalized and are expected to go in the range of (0->w-1, 0->h-1)
//-----------------------------------------------------------------------------
float BilerpBitmap( Bitmap_t &bitmap, float x, float y )
{
	Assert( bitmap.m_ImageFormat == IMAGE_FORMAT_RGBA8888 );

    float w = (float)bitmap.m_nWidth;
    float h = (float)bitmap.m_nHeight;

    // Clamp to a valid range
	x = clamp( x, 0, w - 1.0f );
	y = clamp( y, 0, h - 1.0f );

    // pick bilerp coordinates
	int i0 = (int)floor( x );
	int i1 = i0 + 1;
	int j0 = (int)floor( y );
	int j1 = j0 + 1;
	if ( i1 >= bitmap.m_nWidth )
	{
		i1 = bitmap.m_nWidth - 1;
	}
	if ( j1 >= bitmap.m_nHeight )
	{
		j1 = bitmap.m_nHeight - 1;
	}

	float fx = x - i0;
	float fy = y - j0;

	RGBA8888_t* pPixel00 = (RGBA8888_t*)bitmap.GetPixel( i0, j0 );
	RGBA8888_t* pPixel10 = (RGBA8888_t*)bitmap.GetPixel( i1, j0 );
	RGBA8888_t* pPixel01 = (RGBA8888_t*)bitmap.GetPixel( i0, j1 );
	RGBA8888_t* pPixel11 = (RGBA8888_t*)bitmap.GetPixel( i1, j1 );

	float v00 = pPixel00->r / 255.0f;
    float v10 = pPixel10->r / 255.0f;
    float v01 = pPixel01->r / 255.0f;
    float v11 = pPixel11->r / 255.0f;

    // do the bilerp
    return (1-fx)*(1-fy)*v00 + fx*(1-fy)*v10 + (1-fx)*fy*v01 + fx*fy*v11;
}


//-----------------------------------------------------------------------------
// Loads the heightfield from a file
//-----------------------------------------------------------------------------
bool CHeightField::LoadHeightFromFile( const char *pFileName )
{
	Bitmap_t bitmap;
	CUtlStreamBuffer buf( pFileName, "GAME", CUtlBuffer::READ_ONLY );
	if ( IsPSDFile( buf ) )
	{
		if ( !PSDReadFileRGBA8888( buf, bitmap ) )
			return false;
	}

    // map from height field into map, ensuring corner pixel centers line up
    // hfx -> mapx:  0 -> 0.5, hfw-1 -> mapw-0.5
    // x (mapw - 1)/(hfw - 1) + 0.5
    // mapx -> worldx: 0 -> 0, mapw -> worldw
    float fx = (float)( bitmap.m_nWidth - 1) / (float)( m_nWidth - 1 );
    float fy = (float)( bitmap.m_nHeight - 1) / (float)( m_nHeight - 1 );

    for( int i = 0; i < m_nHeight; ++i ) 
	{
		float *pRow = ROW( i );
        for( int j = 0; j < m_nWidth; ++j, ++pRow ) 
		{
            *pRow = 50.0f * BilerpBitmap( bitmap, i * fx, j * fy );
        }
    }
	return true;
}


//-----------------------------------------------------------------------------
// Returns the height at a particular point
//-----------------------------------------------------------------------------
float CHeightField::GetHeight( float x, float y )
{
	x *= m_flOOScale;
	y *= m_flOOScale;

	int gx = (int)floor( x );
	int gy = (int)floor( y );
	x -= gx;
	y -= gy;

	// Check for out of range
	if ( gx < -1 || gy < -1 || gx >= m_nWidth || gy >= m_nHeight ) 
		return 0.0f;

	float h00 = ( gx >= 0 && gy >= 0 )					? HEIGHT(gx  , gy  ) : 0.0f;
	float h01 = ( gx < (m_nWidth-1) && gy >= 0 )		? HEIGHT(gx+1, gy  ) : 0.0f;
	float h10 = ( gx >= 0 && gy < (m_nHeight-1) )		? HEIGHT(gx  , gy+1) : 0.0f;
	float h11 = ( gx<(m_nWidth-1) && gy<(m_nHeight-1) )	? HEIGHT(gx+1, gy+1) : 0.0f;

	// This fixup accounts for the triangularization of the mesh
	if (x > y) 
	{
		h10 = h00 + h11 - h01;
	} 
	else 
	{
		h01 = h00 + h11 - h10;
	}

	// Bilinear filter
	float h0 = h00 + ( h01 - h00 ) * x;
	float h1 = h10 + ( h11 - h10 ) * x;
	float h = h0 + (h1-h0)*y;

	return h;
}


//-----------------------------------------------------------------------------
// Returns the height + slope at a particular point
//-----------------------------------------------------------------------------
float CHeightField::GetHeightAndSlope( float x, float y, float *dx, float *dy )
{
	x *= m_flOOScale;
	y *= m_flOOScale;

	int gx = (int)floor(x);
	int gy = (int)floor(y);
	x -= gx;
	y -= gy;

	if ( gx < -1 || gy < -1 || gx >= m_nWidth || gy >= m_nHeight )
	{
		*dx = 0;
		*dy = 0;
		return 0.0f;
	}

	float h00 = ( gx >= 0 && gy >= 0 )					? HEIGHT(gx  , gy  ) : 0.0f;
	float h01 = ( gx < (m_nWidth-1) && gy >= 0 )		? HEIGHT(gx+1, gy  ) : 0.0f;
	float h10 = ( gx >= 0 && gy < (m_nHeight-1) )		? HEIGHT(gx  , gy+1) : 0.0f;
	float h11 = ( gx<(m_nWidth-1) && gy<(m_nHeight-1) )	? HEIGHT(gx+1, gy+1) : 0.0f;

	if (x > y) 
	{
		h10 = h00 + h11 - h01;
	} 
	else 
	{
		h01 = h00 + h11 - h10;
	}

	*dx = ( h01 - h00 ) * m_flOOScale;
	*dy = ( h10 - h00 ) * m_flOOScale;

	// Bilinear filter
	float h0 = h00 + ( h01 - h00 ) * x;
	float h1 = h10 + ( h11 - h10 ) * x;
	float h = h0 + ( h1 - h0 )* y;

	return h;
}


//-----------------------------------------------------------------------------
// Draws the height field
//-----------------------------------------------------------------------------
void CHeightField::Draw( )
{
	int nVertexCount = m_nWidth * m_nHeight;
	int nIndexCount = 6 * ( m_nWidth - 1 ) * ( m_nHeight - 1 );

	float flOOTexWidth = 1.0f / m_Material->GetMappingWidth();
	float flOOTexHeight = 1.0f / m_Material->GetMappingHeight();
	float iu = 0.5f  * flOOTexWidth;
	float iv = 1.0f - ( 0.5f * flOOTexHeight );
	float du = ( 1.0f - flOOTexWidth ) / ( m_nWidth - 1 );
	float dv = -( 1.0f - flOOTexHeight ) / ( m_nHeight - 1 );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( m_Material );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nVertexCount, nIndexCount );

	// Deal with vertices
	float v = iv;
	float y = 0.0f;
	for ( int i = 0; i < m_nHeight; ++i, y += m_nScale, v += dv )
	{
		float u = iu;
		float x = 0.0f;
		for ( int j = 0; j < m_nWidth; ++j, x += m_nScale, u += du )
		{
			meshBuilder.Position3f( x, y, HEIGHT( j, i ) );
			meshBuilder.TexCoord2f( 0, u, v );
			meshBuilder.AdvanceVertex();
		}
	}

	// Deal with indices
	for ( int i = 0; i < (m_nHeight - 1); ++i )
	{
		int nRow0 = m_nWidth * i; 
		int nRow1 = nRow0 + m_nWidth; 
		for ( int j = 0; j < (m_nWidth - 1); ++j )
		{
			meshBuilder.FastIndex( nRow0+j );
			meshBuilder.FastIndex( nRow0+j+1 );
			meshBuilder.FastIndex( nRow1+j+1 );

			meshBuilder.FastIndex( nRow0+j );
			meshBuilder.FastIndex( nRow1+j+1 );
			meshBuilder.FastIndex( nRow1+j );
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}
