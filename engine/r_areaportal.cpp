//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "render_pch.h"
#include "client.h"
#include "debug_leafvis.h"
#include "con_nprint.h"
#include "tier0/fasttimer.h"
#include "r_areaportal.h"
#include "cmodel_engine.h"
#include "con_nprint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar r_ClipAreaPortals( "r_ClipAreaPortals", "1", FCVAR_CHEAT );
ConVar r_DrawPortals( "r_DrawPortals", "0", FCVAR_CHEAT );

CUtlVector<CPortalRect> g_PortalRects;
static bool				g_bViewerInSolidSpace = false;

			  

// ------------------------------------------------------------------------------------ //
// Classes.
// ------------------------------------------------------------------------------------ //

#define MAX_PORTAL_VERTS	32


class CAreaCullInfo
{
public:
	CAreaCullInfo()
	{
		m_GlobalCounter = 0;
		memset( &m_Frustum, 0, sizeof( m_Frustum ) );
		memset( &m_Rect, 0, sizeof( m_Rect ) );
	}
	Frustum_t		m_Frustum;
	CPortalRect		m_Rect;
	unsigned short	m_GlobalCounter; // Used to tell if it's been touched yet this frame.
};



// ------------------------------------------------------------------------------------ //
// Globals.
// ------------------------------------------------------------------------------------ //

// Visible areas from the client DLL + occluded areas using area portals.
unsigned char g_RenderAreaBits[32];

// Used to prevent it from coming back into portals while flowing through them.
static unsigned char g_AreaStack[32];

// Frustums for each area for the current frame. Used to cull out leaves.
static CUtlVector<CAreaCullInfo> g_AreaCullInfo;

// List of areas marked visible this frame.
static unsigned short g_VisibleAreas[MAX_MAP_AREAS];
static int g_nVisibleAreas;

// Tied to CAreaCullInfo::m_GlobalCounter.
static unsigned short g_GlobalCounter = 1;


// A copy of the current view setup, but possibly nobbled a bit.
static CViewSetup g_viewSetup;
static CPortalRect g_viewWindow;
// Maps from world space to normalised (-1,1) screen coords.
static VMatrix g_ScreenFromWorldProjection;


// ------------------------------------------------------------------------------------ //
// Functions.
// ------------------------------------------------------------------------------------ //
void R_Areaportal_LevelInit()
{
	g_AreaCullInfo.SetCount( host_state.worldbrush->m_nAreas );
}

void R_Areaportal_LevelShutdown()
{
	g_AreaCullInfo.Purge();
	g_PortalRects.Purge();
}

static inline void R_SetBit( unsigned char *pBits, int bit )
{
	pBits[bit>>3] |= (1 << (bit&7));
}

static inline void R_ClearBit( unsigned char *pBits, int bit )
{
	pBits[bit>>3] &= ~(1 << (bit&7));
}

static inline unsigned char R_TestBit( unsigned char *pBits, int bit )
{
	return pBits[bit>>3] & (1 << (bit&7));
}

struct portalclip_t
{
	portalclip_t()
	{
		lists[0] = v0;
		lists[1] = v1;
	}
	Vector v0[MAX_PORTAL_VERTS];
	Vector v1[MAX_PORTAL_VERTS];
	Vector *lists[2];
};

// Transforms and clips the portal's verts to the view frustum. Returns false
// if the verts lie outside the frustum.
static inline bool GetPortalScreenExtents( dareaportal_t *pPortal, 
	portalclip_t * RESTRICT clip, CPortalRect &portalRect , float *pReflectionWaterHeight )
{
	portalRect.left = portalRect.bottom = 1e24;
	portalRect.right = portalRect.top   = -1e24;
	bool bValidExtents = false;
	worldbrushdata_t *pBrushData = host_state.worldbrush;
	
	int nStartVerts = min( (int)pPortal->m_nClipPortalVerts, MAX_PORTAL_VERTS );

	// NOTE: We need two passes to deal with reflection. We need to compute
	// the screen extents for both the reflected + non-reflected area portals
	// and make bounds that surrounds them both.
	int nPassCount = ( pReflectionWaterHeight != NULL ) ? 2 : 1;
	for ( int j = 0; j < nPassCount; ++j )
	{
		int i;
		for( i=0; i < nStartVerts; i++ )
		{
			clip->v0[i] = pBrushData->m_pClipPortalVerts[pPortal->m_FirstClipPortalVert+i];

			// 2nd pass is to compute the reflected areaportal position
			if ( j == 1 )
			{
				clip->v0[i].z = 2.0f * ( *pReflectionWaterHeight ) - clip->v0[i].z;
			}
		}

		int iCurList = 0;
		bool bAllClipped = false;
		for( int iPlane=0; iPlane < 4; iPlane++ )
		{
			const cplane_t *pPlane = g_Frustum.GetPlane(iPlane);

			Vector *pIn = clip->lists[iCurList];
			Vector *pOut = clip->lists[!iCurList];

			int nOutVerts = 0;
			int iPrev = nStartVerts - 1;
			float flPrevDot = pPlane->normal.Dot( pIn[iPrev] ) - pPlane->dist;
			for( int iCur=0; iCur < nStartVerts; iCur++ )
			{
				float flCurDot = pPlane->normal.Dot( pIn[iCur] ) - pPlane->dist;

				if( (flCurDot > 0) != (flPrevDot > 0) )
				{
					if( nOutVerts < MAX_PORTAL_VERTS )
					{
						// Add the vert at the intersection.
						float t = flPrevDot / (flPrevDot - flCurDot);
						VectorLerp( pIn[iPrev], pIn[iCur], t, pOut[nOutVerts] );

						++nOutVerts;
					}
				}

				// Add this vert?
				if( flCurDot > 0 )
				{
					if( nOutVerts < MAX_PORTAL_VERTS )
					{
						pOut[nOutVerts] = pIn[iCur];
						++nOutVerts;
					}
				}

				flPrevDot = flCurDot;
				iPrev = iCur;
			}

			if( nOutVerts == 0 )
			{
				// If they're all behind, then this portal is clipped out.
				bAllClipped = true;
				break;
			}

			nStartVerts = nOutVerts;
			iCurList = !iCurList;
		}

		if ( bAllClipped )
			continue;

		// Project all the verts and figure out the screen extents.
		Vector screenPos;
		Assert( iCurList == 0 );
		for( i=0; i < nStartVerts; i++ )
		{
			Vector &point = clip->v0[i];

			g_EngineRenderer->ClipTransformWithProjection ( g_ScreenFromWorldProjection, point, &screenPos );

			portalRect.left   = fpmin( screenPos.x, portalRect.left );
			portalRect.bottom = fpmin( screenPos.y, portalRect.bottom );
			portalRect.top    = fpmax( screenPos.y, portalRect.top );
			portalRect.right  = fpmax( screenPos.x, portalRect.right );
		}
		bValidExtents = true;
	}

	if ( !bValidExtents )
	{
		portalRect.left = portalRect.bottom = 0;
		portalRect.right = portalRect.top   = 0;
	}

	return bValidExtents;
}


// Fill in the intersection between the two rectangles.
inline bool GetRectIntersection( CPortalRect const *pRect1, CPortalRect const *pRect2, CPortalRect *pOut )
{
	pOut->left  = fpmax( pRect1->left, pRect2->left );
	pOut->right = fpmin( pRect1->right, pRect2->right );
	if( pOut->left >= pOut->right )
		return false;

	pOut->bottom = fpmax( pRect1->bottom, pRect2->bottom );
	pOut->top    = fpmin( pRect1->top, pRect2->top );
	if( pOut->bottom >= pOut->top )
		return false;

	return true;
}

static void R_FlowThroughArea( int area, const Vector &vecVisOrigin, const CPortalRect *pClipRect, 
	const VisOverrideData_t* pVisData, float *pReflectionWaterHeight )
{
#ifndef SWDS
	// Update this area's frustum.
	if( g_AreaCullInfo[area].m_GlobalCounter != g_GlobalCounter )
	{
		g_VisibleAreas[g_nVisibleAreas] = area;
		++g_nVisibleAreas;

		g_AreaCullInfo[area].m_GlobalCounter = g_GlobalCounter;
		g_AreaCullInfo[area].m_Rect = *pClipRect;
	}
	else
	{
		// Expand the areaportal's rectangle to include the new cliprect.
		CPortalRect *pFrustumRect = &g_AreaCullInfo[area].m_Rect;
		pFrustumRect->left   = fpmin( pFrustumRect->left, pClipRect->left );
		pFrustumRect->bottom = fpmin( pFrustumRect->bottom, pClipRect->bottom );
		pFrustumRect->top    = fpmax( pFrustumRect->top, pClipRect->top );
		pFrustumRect->right  = fpmax( pFrustumRect->right, pClipRect->right );
	}
	
	// Mark this area as visible.
	R_SetBit( g_RenderAreaBits, area );

	// Set that we're in this area on the stack.
	R_SetBit( g_AreaStack, area );

	worldbrushdata_t *pBrushData = host_state.worldbrush;

	Assert( area < host_state.worldbrush->m_nAreas );
	darea_t *pArea = &host_state.worldbrush->m_pAreas[area];
	// temp buffer for clipping
	portalclip_t clipTmp;

	// Check all areas that connect to this area.
	for( int iAreaPortal=0; iAreaPortal < pArea->numareaportals; iAreaPortal++ )
	{
		Assert( pArea->firstareaportal + iAreaPortal < pBrushData->m_nAreaPortals );
		dareaportal_t *pAreaPortal = &pBrushData->m_pAreaPortals[ pArea->firstareaportal + iAreaPortal ];

		// Don't flow back into a portal on the stack.
		if( R_TestBit( g_AreaStack, pAreaPortal->otherarea ) )
			continue;

		// If this portal is closed, don't go through it.
		if ( !R_TestBit( cl.m_chAreaPortalBits, pAreaPortal->m_PortalKey ) )
			continue;

		// Make sure the viewer is on the right side of the portal to see through it.
		cplane_t *pPlane = &pBrushData->planes[ pAreaPortal->planenum ];
		// Use the specified vis origin to test backface culling, or the main view if none was specified
		float flDist = pPlane->normal.Dot( vecVisOrigin ) - pPlane->dist;
		if( flDist < -0.1f )
			continue;

		// If the client doesn't want this area visible, don't try to flow into it.
		if( !R_TestBit( cl.m_chAreaBits, pAreaPortal->otherarea ) )
			continue;

		CPortalRect portalRect;
		bool portalVis = true;

		// don't try to clip portals if the viewer is practically in the plane
		float fDistTolerance = (pVisData)?(pVisData->m_fDistToAreaPortalTolerance):(0.1f);
		if ( flDist > fDistTolerance )
		{
			portalVis = GetPortalScreenExtents( pAreaPortal, &clipTmp, portalRect, pReflectionWaterHeight );
		}
		else
		{
			portalRect.left = -1;
			portalRect.top = 1;
			portalRect.right = 1;
			portalRect.bottom = -1;	// note top/bottom reversed!
			//portalVis=true - not needed, default
		}
		if( portalVis )
		{
			CPortalRect intersection;
			if( GetRectIntersection( &portalRect, pClipRect, &intersection ) )
			{
#ifdef USE_CONVARS
				if( r_DrawPortals.GetInt() )
				{
					g_PortalRects.AddToTail( intersection );
				}
#endif

				// Ok, we can see into this area.
				R_FlowThroughArea( pAreaPortal->otherarea, vecVisOrigin, &intersection, pVisData, pReflectionWaterHeight );
			}
		}
	}
	
	// Mark that we're leaving this area.
	R_ClearBit( g_AreaStack, area );
#endif
}


static void IncrementGlobalCounter()
{
	if( g_GlobalCounter == 0xFFFF )
	{
		for( int i=0; i < g_AreaCullInfo.Count(); i++ )
			g_AreaCullInfo[i].m_GlobalCounter = 0;
	
		g_GlobalCounter = 1;
	}
	else
	{
		g_GlobalCounter++;
	}
}


static void R_SetupGlobalFrustum()
{
#ifndef SWDS

	// Copy the current view away so that we can play with it if needed.
	g_viewSetup = g_EngineRenderer->ViewGetCurrent();

	if( g_viewSetup.m_bOrtho )
	{
		g_viewWindow.right	= g_viewSetup.m_OrthoRight;
		g_viewWindow.left		= g_viewSetup.m_OrthoLeft;
		g_viewWindow.top		= g_viewSetup.m_OrthoTop;
		g_viewWindow.bottom	= g_viewSetup.m_OrthoBottom;
	}
	else
	{
		// Assuming a view plane distance of 1, figure out the boundaries of a window
		// the view would project into given the FOV.
		float xFOV = g_EngineRenderer->GetFov() * 0.5f;
		float yFOV = g_EngineRenderer->GetFovY() * 0.5f;

		g_viewWindow.right	= tan( DEG2RAD( xFOV ) );
		g_viewWindow.left	= -g_viewWindow.right;
		g_viewWindow.top	= tan( DEG2RAD( yFOV ) );
		g_viewWindow.bottom	= -g_viewWindow.top;

		if ( g_viewSetup.m_bOffCenter )
		{
			// How did this ever work?
			Assert ( !"test m_bOffCenter frustums with area portals" );
		}
		else if ( g_viewSetup.m_bViewToProjectionOverride )
		{
			// ...this has been tested and works!
		}

		// Rather than try to deal with crazy projection matrices (shear, trapezoid, etc), take whatever FOV we're given,
		// assume it's conservative, and then construct a matching projection matrix for it. Then use that consistent
		// hallucination throughout, rather than refer back to the engine's actual proj. matrix for anything.
		VMatrix matrixView;
		VMatrix matrixProjection;
		VMatrix matrixWorldToScreen;
		g_viewSetup.m_bViewToProjectionOverride = false;

		ComputeViewMatrices (
			&matrixView, 
			&matrixProjection,
			&matrixWorldToScreen,
			g_viewSetup );

		g_ScreenFromWorldProjection = matrixWorldToScreen;
	}

#endif //#ifndef SWDS
}

ConVar r_snapportal( "r_snapportal", "-1" );
extern void CSGFrustum( Frustum_t &frustum );

static void R_SetupVisibleAreaFrustums()
{
#ifndef SWDS

	// Now scale the portals as specified in the normalized view frustum (-1,-1,1,1)
	// into our view window and generate planes out of that.
	for( int i=0; i < g_nVisibleAreas; i++ )
	{
		CAreaCullInfo *pInfo = &g_AreaCullInfo[ g_VisibleAreas[i] ];

		CPortalRect portalWindow;
		portalWindow.left    = RemapVal( pInfo->m_Rect.left,   -1, 1, g_viewWindow.left,   g_viewWindow.right );
		portalWindow.right   = RemapVal( pInfo->m_Rect.right,  -1, 1, g_viewWindow.left,   g_viewWindow.right );
		portalWindow.top     = RemapVal( pInfo->m_Rect.top,    -1, 1, g_viewWindow.bottom, g_viewWindow.top );
		portalWindow.bottom  = RemapVal( pInfo->m_Rect.bottom, -1, 1, g_viewWindow.bottom, g_viewWindow.top );
		
		if( g_viewSetup.m_bOrtho )
		{
			// Left and right planes...
			float orgOffset = DotProduct(CurrentViewOrigin(), CurrentViewRight());
			pInfo->m_Frustum.SetPlane( FRUSTUM_LEFT, PLANE_ANYZ, CurrentViewRight(), portalWindow.left + orgOffset );
			pInfo->m_Frustum.SetPlane( FRUSTUM_RIGHT, PLANE_ANYZ, -CurrentViewRight(), -portalWindow.right - orgOffset );

			// Top and bottom planes...
			orgOffset = DotProduct(CurrentViewOrigin(), CurrentViewUp());
			pInfo->m_Frustum.SetPlane( FRUSTUM_TOP, PLANE_ANYZ, CurrentViewUp(), portalWindow.top + orgOffset );
			pInfo->m_Frustum.SetPlane( FRUSTUM_BOTTOM, PLANE_ANYZ, -CurrentViewUp(), -portalWindow.bottom - orgOffset );
		}
		else
		{

			if ( g_viewSetup.m_bOffCenter )
			{
				// How did this ever work?
				Assert ( !"test m_bOffCenter frustums with area portals" );
			}
			else if ( g_viewSetup.m_bViewToProjectionOverride )
			{
				// ...this has been tested and works!
			}

			Vector normal;
			const cplane_t *pTest;

			// right side
			normal = portalWindow.right * CurrentViewForward() - CurrentViewRight();
			VectorNormalize(normal); // OPTIMIZE: This is unnecessary for culling
			pTest = pInfo->m_Frustum.GetPlane( FRUSTUM_RIGHT );
			pInfo->m_Frustum.SetPlane( FRUSTUM_RIGHT, PLANE_ANYZ, normal, DotProduct(normal,CurrentViewOrigin()) );

			// left side
			normal = CurrentViewRight() - portalWindow.left * CurrentViewForward();
			VectorNormalize(normal); // OPTIMIZE: This is unnecessary for culling
			pTest = pInfo->m_Frustum.GetPlane( FRUSTUM_LEFT );
			pInfo->m_Frustum.SetPlane( FRUSTUM_LEFT, PLANE_ANYZ, normal, DotProduct(normal,CurrentViewOrigin()) );

			// top
			normal = portalWindow.top * CurrentViewForward() - CurrentViewUp();
			VectorNormalize(normal); // OPTIMIZE: This is unnecessary for culling
			pTest = pInfo->m_Frustum.GetPlane( FRUSTUM_TOP );
			pInfo->m_Frustum.SetPlane( FRUSTUM_TOP, PLANE_ANYZ, normal, DotProduct(normal,CurrentViewOrigin()) );

			// bottom
			normal = CurrentViewUp() - portalWindow.bottom * CurrentViewForward();
			VectorNormalize(normal); // OPTIMIZE: This is unnecessary for culling
			pTest = pInfo->m_Frustum.GetPlane( FRUSTUM_BOTTOM );
			pInfo->m_Frustum.SetPlane( FRUSTUM_BOTTOM, PLANE_ANYZ, normal, DotProduct(normal,CurrentViewOrigin()) );

			// farz
			pInfo->m_Frustum.SetPlane( FRUSTUM_FARZ, PLANE_ANYZ, -CurrentViewForward(), 
				DotProduct(-CurrentViewForward(), CurrentViewOrigin() + CurrentViewForward()*g_viewSetup.zFar) );
		}

		// DEBUG: Code to visualize the areaportal frustums in 3D
		// Useful for debugging
		extern void CSGFrustum( Frustum_t &frustum );
		if ( r_snapportal.GetInt() >= 0 )
		{
			if ( g_VisibleAreas[i] == r_snapportal.GetInt() )
			{
				pInfo->m_Frustum.SetPlane( FRUSTUM_NEARZ, PLANE_ANYZ, CurrentViewForward(), 
					DotProduct(CurrentViewForward(), CurrentViewOrigin()) );
				pInfo->m_Frustum.SetPlane( FRUSTUM_FARZ, PLANE_ANYZ, -CurrentViewForward(), 
					DotProduct(-CurrentViewForward(), CurrentViewOrigin() + CurrentViewForward()*500) );
				r_snapportal.SetValue( -1 );
				CSGFrustum( pInfo->m_Frustum );
			}
		}
	}
#endif
}




// Intersection of AABB and a frustum. The frustum may contain 0-32 planes
// (active planes are defined by inClipMask). Returns boolean value indicating
// whether AABB intersects the view frustum or not.
// If AABB intersects the frustum, an output clip mask is returned as well
// (indicating which planes are crossed by the AABB). This information
// can be used to optimize testing of child nodes or objects inside the
// nodes (pass value as 'inClipMask').
inline bool R_CullNodeInternal( mnode_t *pNode, int &nClipMask, const Frustum_t& frustum )
{
	int nOutClipMask = nClipMask & FRUSTUM_CLIP_IN_AREA;	// init outclip mask

	float flCenterDotNormal, flHalfDiagDotAbsNormal;
	if (nClipMask & FRUSTUM_CLIP_RIGHT)
	{
		const cplane_t *pPlane = frustum.GetPlane(FRUSTUM_RIGHT);
		flCenterDotNormal = DotProduct( pNode->m_vecCenter, pPlane->normal ) - pPlane->dist;
		flHalfDiagDotAbsNormal = DotProduct( pNode->m_vecHalfDiagonal, frustum.GetAbsNormal(FRUSTUM_RIGHT) );
		if (flCenterDotNormal + flHalfDiagDotAbsNormal < 0.0f)
			return true;
		if (flCenterDotNormal - flHalfDiagDotAbsNormal < 0.0f)
			nOutClipMask |= FRUSTUM_CLIP_RIGHT;	
	}

	if (nClipMask & FRUSTUM_CLIP_LEFT)
	{
		const cplane_t *pPlane = frustum.GetPlane(FRUSTUM_LEFT);
		flCenterDotNormal = DotProduct( pNode->m_vecCenter, pPlane->normal ) - pPlane->dist;
		flHalfDiagDotAbsNormal = DotProduct( pNode->m_vecHalfDiagonal, frustum.GetAbsNormal(FRUSTUM_LEFT) );
		if (flCenterDotNormal + flHalfDiagDotAbsNormal < 0.0f)
			return true;
		if (flCenterDotNormal - flHalfDiagDotAbsNormal < 0.0f)
			nOutClipMask |= FRUSTUM_CLIP_LEFT;	
	}

	if (nClipMask & FRUSTUM_CLIP_TOP)
	{
		const cplane_t *pPlane = frustum.GetPlane(FRUSTUM_TOP);
		flCenterDotNormal = DotProduct( pNode->m_vecCenter, pPlane->normal ) - pPlane->dist;
		flHalfDiagDotAbsNormal = DotProduct( pNode->m_vecHalfDiagonal, frustum.GetAbsNormal(FRUSTUM_TOP) );
		if (flCenterDotNormal + flHalfDiagDotAbsNormal < 0.0f)
			return true;
		if (flCenterDotNormal - flHalfDiagDotAbsNormal < 0.0f)
			nOutClipMask |= FRUSTUM_CLIP_TOP;	
	}

	if (nClipMask & FRUSTUM_CLIP_BOTTOM)
	{
		const cplane_t *pPlane = frustum.GetPlane(FRUSTUM_BOTTOM);
		flCenterDotNormal = DotProduct( pNode->m_vecCenter, pPlane->normal ) - pPlane->dist;
		flHalfDiagDotAbsNormal = DotProduct( pNode->m_vecHalfDiagonal, frustum.GetAbsNormal(FRUSTUM_BOTTOM) );
		if (flCenterDotNormal + flHalfDiagDotAbsNormal < 0.0f)
			return true;
		if (flCenterDotNormal - flHalfDiagDotAbsNormal < 0.0f)
			nOutClipMask |= FRUSTUM_CLIP_BOTTOM;	
	}

	nClipMask = nOutClipMask;
	return false;						// AABB intersects frustum
}


bool R_CullNode( Frustum_t *pAreaFrustum, mnode_t *pNode, int& nClipMask )
{
	// Now cull to this area's frustum.
	if (( !g_bViewerInSolidSpace ) && ( pNode->area > 0 ))
	{
		// First make sure its whole area is even visible.
		if( !R_IsAreaVisible( pNode->area ) )
			return true;

		// This ConVar access causes a huge perf hit in some cases.
		// If you want to put it back in please cache it's value.
//#ifdef USE_CONVARS
//		if( r_ClipAreaPortals.GetInt() )
//#else
		if( 1 )
//#endif
		{
			if ((nClipMask & FRUSTUM_CLIP_IN_AREA) == 0)
			{
				// In this case, we've never hit this area before and
				// we need to test all planes again because we just changed the frustum
				nClipMask = FRUSTUM_CLIP_IN_AREA | FRUSTUM_CLIP_ALL;
			}

			pAreaFrustum = &g_AreaCullInfo[pNode->area].m_Frustum;
		}
	}

	return R_CullNodeInternal( pNode, nClipMask, *pAreaFrustum );
}


static ConVar r_portalscloseall( "r_portalscloseall", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConVar r_portalsopenall( "r_portalsopenall", "0", FCVAR_CHEAT, "Open all portals" );
static ConVar r_ShowViewerArea( "r_ShowViewerArea", "0" );

void R_SetupAreaBits( int iForceViewLeaf /* = -1 */, const VisOverrideData_t* pVisData /* = NULL */, float *pWaterReflectionHeight /* = NULL */ )
{
	IncrementGlobalCounter();

	g_bViewerInSolidSpace = false;

	// Clear the visible area bits.
	memset( g_RenderAreaBits, 0, sizeof( g_RenderAreaBits ) );
	memset( g_AreaStack, 0, sizeof( g_AreaStack ) );

	// Our initial clip rect is the whole screen.
	CPortalRect rect;
	rect.left = rect.bottom = -1;
	rect.top = rect.right = 1;

	// Flow through areas starting at the one we're in.
	int leaf = iForceViewLeaf;
	// If view point override wasn't specified, use the current view origin
	if ( iForceViewLeaf == -1  ) 
	{
		leaf = CM_PointLeafnum( g_EngineRenderer->ViewOrigin() );
	}

	if( r_portalscloseall.GetBool() )
	{
		if ( cl.m_bAreaBitsValid )
		{
			// Clear the visible area bits.
			memset( g_RenderAreaBits, 0, sizeof( g_RenderAreaBits ) );
			int area = host_state.worldbrush->leafs[leaf].area;
			R_SetBit( g_RenderAreaBits, area );
			
			g_VisibleAreas[0] = area;
			g_nVisibleAreas = 1;

			g_AreaCullInfo[area].m_GlobalCounter = g_GlobalCounter;
			g_AreaCullInfo[area].m_Rect = rect;
			
			R_SetupVisibleAreaFrustums();
		}
		else
		{
			g_bViewerInSolidSpace = true;
		}

		return;
	}

#if defined( REPLAY_ENABLED )
	if ( host_state.worldbrush->leafs[leaf].contents & CONTENTS_SOLID ||
		 cl.ishltv || cl.isreplay || !cl.m_bAreaBitsValid || r_portalsopenall.GetBool()  )
#else
	if ( host_state.worldbrush->leafs[leaf].contents & CONTENTS_SOLID ||
		 cl.ishltv || !cl.m_bAreaBitsValid || r_portalsopenall.GetBool()  )
#endif
	{
		// Draw everything if we're in solid space or if r_portalsopenall is true (used for building cubemaps)
		g_bViewerInSolidSpace = true;

		if ( r_ShowViewerArea.GetInt() )
			Con_NPrintf( 3, "Viewer area: (solid space)" );
	}
	else
	{
		int area = host_state.worldbrush->leafs[leaf].area;
		
		if ( r_ShowViewerArea.GetInt() )
			Con_NPrintf( 3, "Viewer area: %d", area );

		g_nVisibleAreas = 0;		
		Vector vecVisOrigin = (pVisData)?(pVisData->m_vecVisOrigin):(g_EngineRenderer->ViewOrigin());
		R_SetupGlobalFrustum();
		R_FlowThroughArea ( area, vecVisOrigin, &rect, pVisData, pWaterReflectionHeight );
		R_SetupVisibleAreaFrustums();
	}
}


const Frustum_t* GetAreaFrustum( int area )
{
	if ( g_AreaCullInfo[area].m_GlobalCounter == g_GlobalCounter )
		return &g_AreaCullInfo[area].m_Frustum;
	else
		return &g_Frustum;
}



