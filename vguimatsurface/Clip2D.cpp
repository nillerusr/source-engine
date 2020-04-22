//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains 2D clipping routines
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#include <vgui/ISurface.h>
#include "Clip2D.h"
#include "tier0/dbg.h"
#include "utlvector.h"
#if defined( _X360 )
#include "materialsystem/imaterialsystem.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Stretch texture to fit window ( before scissoring )
//-----------------------------------------------------------------------------
static bool	g_bStretchTexture = false;


//-----------------------------------------------------------------------------
// Max # of vertices for clipping
//-----------------------------------------------------------------------------
enum
{
	VGUI_VERTEX_TEMP_COUNT = 48,
};

//-----------------------------------------------------------------------------
// For simulated scissor tests...
//-----------------------------------------------------------------------------
struct ScissorRect_t
{
	int left;
	int top;
	int right;
	int bottom;
};

static ScissorRect_t g_ScissorRect;
static bool	g_bScissor = false;
static bool g_bFullScreenScissor = false;

//-----------------------------------------------------------------------------
// Enable/disable scissoring...
//-----------------------------------------------------------------------------
void EnableScissor( bool enable )
{
	g_bScissor = enable;
}


void SetScissorRect( int left, int top, int right, int bottom )
{
	// Check for a valid rectangle...
	Assert( left <= right );
	Assert( top <= bottom );

	if ( g_ScissorRect.left == left && g_ScissorRect.right == right &&
		 g_ScissorRect.top == top && g_ScissorRect.bottom == bottom )
		 return;

	g_ScissorRect.left   = left;
	g_ScissorRect.top    = top;
	g_ScissorRect.right  = right;
	g_ScissorRect.bottom = bottom;

#if defined( _X360 )
	// no reason to waste cpu on full screen scissor, gpu does it
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int vx, vy, vw, vh;
	pRenderContext->GetViewport( vx, vy, vw, vh );
	g_bFullScreenScissor = (left <= vx && top <= vy && right >= vw && bottom >= vh );
#endif
}

void GetScissorRect( int &left, int &top, int &right, int &bottom, bool &enabled )
{
	left = g_ScissorRect.left;
	top = g_ScissorRect.top;
	right = g_ScissorRect.right;
	bottom = g_ScissorRect.bottom;
	enabled = g_bScissor;
}


//-----------------------------------------------------------------------------
// Used to clip the shadow decals
//-----------------------------------------------------------------------------
struct PolygonClipState_t
{
	int m_CurrVert;
	int	m_TempCount;
	int	m_ClipCount;
	vgui::Vertex_t	m_pTempVertices[VGUI_VERTEX_TEMP_COUNT];
	vgui::Vertex_t*	m_ppClipVertices[2][VGUI_VERTEX_TEMP_COUNT];
};


//-----------------------------------------------------------------------------
// Clipping methods for 2D
//-----------------------------------------------------------------------------
class CClipTop
{
public:
	static inline bool Inside( vgui::Vertex_t const& vert )					
	{ 
		return vert.m_Position.y >= g_ScissorRect.top;
	}
	static inline float Clip( const Vector2D& one, const Vector2D& two )	
	{ 
		return (g_ScissorRect.top - one.y) / (two.y - one.y);
	}
};

class CClipLeft
{
public:
	static inline bool Inside( vgui::Vertex_t const& vert )					
	{ 
		return vert.m_Position.x >= g_ScissorRect.left;
	}
	static inline float Clip( const Vector2D& one, const Vector2D& two )	
	{ 
		return (one.x - g_ScissorRect.left) / (one.x - two.x);
	}
};

class CClipRight
{
public:
	static inline bool Inside( vgui::Vertex_t const& vert )					
	{
		return vert.m_Position.x < g_ScissorRect.right;
	}
	static inline float Clip( const Vector2D& one, const Vector2D& two )	
	{
		return (g_ScissorRect.right - one.x) / (two.x - one.x);
	}
};

class CClipBottom
{
public:
	static inline bool Inside( vgui::Vertex_t const& vert )					
	{
		return vert.m_Position.y < g_ScissorRect.bottom;
	}
	static inline float Clip( const Vector2D& one, const Vector2D& two )	
	{
		return (one.y - g_ScissorRect.bottom) / (one.y - two.y);
	}
};

template <class Clipper>
static inline void Intersect( const vgui::Vertex_t& start, const vgui::Vertex_t& end, vgui::Vertex_t* pOut, Clipper& clipper )
{
	// Clip to the scissor rectangle
	float t = Clipper::Clip( start.m_Position, end.m_Position );
	Vector2DLerp( start.m_Position, end.m_Position, t, pOut->m_Position );
	Vector2DLerp( start.m_TexCoord, end.m_TexCoord, t, pOut->m_TexCoord );
}


//-----------------------------------------------------------------------------
// Clips a line segment to a single plane
//-----------------------------------------------------------------------------
template< class Clipper >
bool ClipLineToPlane( Clipper &clipper, const vgui::Vertex_t *pInVerts, vgui::Vertex_t* pOutVerts )
{
	bool startInside = Clipper::Inside( pInVerts[0] );
	bool endInside = Clipper::Inside( pInVerts[1] );

	// Cull
	if (!startInside && !endInside)
		return false;

	if (startInside && endInside)
	{
		pOutVerts[0] = pInVerts[0];
		pOutVerts[1] = pInVerts[1];
	}
	else
	{
		int inIndex = startInside ? 0 : 1;
		pOutVerts[inIndex] = pInVerts[inIndex];
		Intersect( pInVerts[0], pInVerts[1], &pOutVerts[1 - inIndex], clipper ); 
	}

	return true;
}


//-----------------------------------------------------------------------------
// Clips a line segment to the current scissor rectangle
//-----------------------------------------------------------------------------
bool ClipLine( const vgui::Vertex_t *pInVerts, vgui::Vertex_t* pOutVerts )
{
	if ( g_bScissor && !g_bFullScreenScissor )
	{
		// Clippers...
		CClipTop top;
		CClipBottom bottom;
		CClipLeft left;
		CClipRight right;

		// Sutherland-hodgman clip, not particularly efficient but that's ok for now
		vgui::Vertex_t tempVerts[2];
		if (!ClipLineToPlane( top, pInVerts, tempVerts ))
			return false;
		if (!ClipLineToPlane( bottom, tempVerts, pOutVerts ))
			return false;
		if (!ClipLineToPlane( left, pOutVerts, tempVerts ))
			return false;
		if (!ClipLineToPlane( right, tempVerts, pOutVerts ))
			return false;

		return true;
	}
	else
	{
		pOutVerts[0] = pInVerts[0];
		pOutVerts[1] = pInVerts[1];
		return true;
	}
}


//-----------------------------------------------------------------------------
// Methods associated with clipping 2D polygons
//-----------------------------------------------------------------------------
struct ScreenClipState_t
{
	int m_iCurrVert;
	int	m_iTempCount;
	int	m_iClipCount;
	CUtlVector<vgui::Vertex_t>	m_pTempVertices;
	CUtlVector<vgui::Vertex_t*>	m_ppClipVertices[2];
};


template <class Clipper>
static void ScreenClip( ScreenClipState_t& clip, Clipper& clipper )
{
	if (clip.m_iClipCount < 3)
		return;

	// Ye Olde Sutherland-Hodgman clipping algorithm
	int numOutVerts = 0;
	vgui::Vertex_t** pSrcVert = clip.m_ppClipVertices[clip.m_iCurrVert].Base();
	vgui::Vertex_t** pDestVert = clip.m_ppClipVertices[!clip.m_iCurrVert].Base();

	int numVerts = clip.m_iClipCount;
	vgui::Vertex_t* pStart = pSrcVert[numVerts-1];
	bool startInside = Clipper::Inside( *pStart );
	for (int i = 0; i < numVerts; ++i)
	{
		vgui::Vertex_t* pEnd = pSrcVert[i];
		bool endInside = Clipper::Inside( *pEnd );
		if (endInside)
		{
			if (!startInside)
			{
				// Started outside, ended inside, need to clip the edge
				Assert( clip.m_iTempCount <= clip.m_pTempVertices.Count() ); 

				// Allocate a new clipped vertex 
				pDestVert[numOutVerts] = &clip.m_pTempVertices[clip.m_iTempCount++];

				// Clip the edge to the clip plane
				Intersect( *pStart, *pEnd, pDestVert[numOutVerts], clipper ); 
				++numOutVerts;
			}
			pDestVert[numOutVerts++] = pEnd;
		}
		else
		{
			if (startInside)
			{
				// Started inside, ended outside, need to clip the edge
				Assert( clip.m_iTempCount <= clip.m_pTempVertices.Count() ); 

				// Allocate a new clipped vertex 
				pDestVert[numOutVerts] = &clip.m_pTempVertices[clip.m_iTempCount++];

				// Clip the edge to the clip plane
				Intersect( *pStart, *pEnd, pDestVert[numOutVerts], clipper ); 
				++numOutVerts;
			}
		}
		pStart = pEnd;
		startInside = endInside;
	}

	// Switch source lists
	clip.m_iCurrVert = 1 - clip.m_iCurrVert;
	clip.m_iClipCount = numOutVerts;
}


//-----------------------------------------------------------------------------
// Clips a polygon to the screen area
//-----------------------------------------------------------------------------
int ClipPolygon( int iCount, vgui::Vertex_t *pVerts, int iTranslateX, int iTranslateY, vgui::Vertex_t ***pppOutVertex ) 
{
	static ScreenClipState_t clip;

	// Allocate enough room in the clip state...
	// Having no reallocations during clipping 
	clip.m_pTempVertices.EnsureCount( iCount * 4 );
	clip.m_ppClipVertices[0].EnsureCount( iCount * 4 );
	clip.m_ppClipVertices[1].EnsureCount( iCount * 4 );

	// Copy the initial verts in...
	for (int i = 0; i < iCount; ++i)
	{
		// NOTE: This only works because we EnsuredCount above
		clip.m_pTempVertices[i] = pVerts[i];
		clip.m_pTempVertices[i].m_Position.x += iTranslateX;
		clip.m_pTempVertices[i].m_Position.y += iTranslateY;
		clip.m_ppClipVertices[0][i] = &clip.m_pTempVertices[i];
	}

	if ( !g_bScissor || g_bFullScreenScissor )
	{
		Assert(pppOutVertex);
		*pppOutVertex = clip.m_ppClipVertices[0].Base();
		return iCount;
	}

	clip.m_iClipCount = iCount;
	clip.m_iTempCount = iCount;
	clip.m_iCurrVert = 0;

	// Clippers...
	CClipTop top;
	CClipBottom bottom;
	CClipLeft left;
	CClipRight right;

	// Sutherland-hodgman clip
	ScreenClip( clip, top );
	ScreenClip( clip, bottom );
	ScreenClip( clip, left );
	ScreenClip( clip, right );

	if (clip.m_iClipCount < 3)
		return 0;

	// Return a pointer to the array of clipped vertices...
	Assert(pppOutVertex);
	*pppOutVertex = clip.m_ppClipVertices[clip.m_iCurrVert].Base();
	return clip.m_iClipCount;
}


//-----------------------------------------------------------------------------
// Purpose: Used for clipping, produces an interpolated texture coordinate
//-----------------------------------------------------------------------------
inline float InterpTCoord(float val, float mins, float maxs, float tMin, float tMax)
{
	float flPercent;
	if (mins != maxs)
		flPercent = (float)(val - mins) / (maxs - mins);
	else
		flPercent = 0.5f;
	return tMin + (tMax - tMin) * flPercent;
}


//-----------------------------------------------------------------------------
// Purpose: Does a scissor clip of the input rectangle.  
// Returns false if it is completely clipped off.
//-----------------------------------------------------------------------------
bool ClipRect( const vgui::Vertex_t &inUL, const vgui::Vertex_t &inLR, 
			   vgui::Vertex_t *pOutUL, vgui::Vertex_t *pOutLR )
{
	// Check for a valid rectangle...
//	Assert( inUL.m_Position.x <= inLR.m_Position.x );
//	Assert( inUL.m_Position.y <= inLR.m_Position.y );
 
	if ( IsX360() && ( !g_bScissor || g_bFullScreenScissor || 
		( inUL.m_Position.x >= g_ScissorRect.left && inLR.m_Position.x <= g_ScissorRect.right && inUL.m_Position.y >= g_ScissorRect.top && inLR.m_Position.y <= g_ScissorRect.bottom ) ) )
	{
		// clipping is not needed
		// either full screen, and hw will do it or rect is inscribed, and operation is meaningless
		*pOutUL = inUL;
		*pOutLR = inLR;
		return true;
	}

	if ( g_bScissor )
	{
		// Pick whichever left side is larger
		if (g_ScissorRect.left > inUL.m_Position.x)
			pOutUL->m_Position.x = g_ScissorRect.left;
		else
			pOutUL->m_Position.x = inUL.m_Position.x;

		// Pick whichever right side is smaller
		if (g_ScissorRect.right <= inLR.m_Position.x)
			pOutLR->m_Position.x = g_ScissorRect.right;
		else
			pOutLR->m_Position.x = inLR.m_Position.x;

		// Pick whichever top side is larger
		if (g_ScissorRect.top > inUL.m_Position.y)
			pOutUL->m_Position.y = g_ScissorRect.top;
		else
			pOutUL->m_Position.y = inUL.m_Position.y;

		// Pick whichever bottom side is smaller
		if (g_ScissorRect.bottom <= inLR.m_Position.y)
			pOutLR->m_Position.y = g_ScissorRect.bottom;
		else
			pOutLR->m_Position.y = inLR.m_Position.y;

		// Check for non-intersecting
		if ( (pOutUL->m_Position.x > pOutLR->m_Position.x) ||
			 (pOutUL->m_Position.y > pOutLR->m_Position.y) )
		{
			return false;
		}

		if ( !g_bStretchTexture )
		{
			pOutUL->m_TexCoord.x = InterpTCoord(pOutUL->m_Position.x, 
				inUL.m_Position.x, inLR.m_Position.x, inUL.m_TexCoord.x, inLR.m_TexCoord.x);
			pOutLR->m_TexCoord.x = InterpTCoord(pOutLR->m_Position.x,  
				inUL.m_Position.x, inLR.m_Position.x, inUL.m_TexCoord.x, inLR.m_TexCoord.x);

			pOutUL->m_TexCoord.y = InterpTCoord(pOutUL->m_Position.y, 
				inUL.m_Position.y, inLR.m_Position.y, inUL.m_TexCoord.y, inLR.m_TexCoord.y);
			pOutLR->m_TexCoord.y = InterpTCoord(pOutLR->m_Position.y,  
				inUL.m_Position.y, inLR.m_Position.y, inUL.m_TexCoord.y, inLR.m_TexCoord.y);
		}
		else
		{
			// FIXME, this isn't right
			pOutUL->m_TexCoord = inUL.m_TexCoord;
			pOutLR->m_TexCoord = inLR.m_TexCoord;
		}
	}
	else
	{
		*pOutUL = inUL;
		*pOutLR = inLR;
	}

	return true;
}

