//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "IOcclusionSystem.h"
#include "mathlib/vector.h"
#include "UtlSortVector.h"
#include "utllinkedlist.h"
#include "utlvector.h"
#include "collisionutils.h"
#include "filesystem.h"
#include "gl_model_private.h"
#include "gl_matsysiface.h"
#include "client.h"
#include "gl_shader.h"
#include "materialsystem/imesh.h"
#include "tier0/vprof.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Uncomment this if you want to get a whole bunch of paranoid error checking
// #define DEBUG_OCCLUSION_SYSTEM 1


//-----------------------------------------------------------------------------
// Used to visualizes what the occlusion system is doing.
//-----------------------------------------------------------------------------
#ifdef _X360
#define DEFAULT_MIN_OCCLUDER_AREA  70.0f
#else
#define DEFAULT_MIN_OCCLUDER_AREA  5.0f
#endif
#define DEFAULT_MAX_OCCLUDEE_AREA  5.0f


ConVar r_visocclusion( "r_visocclusion", "0", FCVAR_CHEAT, "Activate/deactivate wireframe rendering of what the occlusion system is doing." );
ConVar r_occlusion( "r_occlusion", "1", 0, "Activate/deactivate the occlusion system." );
static ConVar r_occludermincount( "r_occludermincount", "0", 0, "At least this many occluders will be used, no matter how big they are." );
static ConVar r_occlusionspew( "r_occlusionspew", "0", FCVAR_CHEAT, "Activate/deactivates spew about what the occlusion system is doing." );
ConVar r_occluderminarea( "r_occluderminarea", "0", 0, "Prevents this occluder from being used if it takes up less than X% of the screen. 0 means use whatever the level said to use." );
ConVar r_occludeemaxarea( "r_occludeemaxarea", "0", 0, "Prevents occlusion testing for entities that take up more than X% of the screen. 0 means use whatever the level said to use." );

#ifdef DEBUG_OCCLUSION_SYSTEM

static ConVar r_occtest( "r_occtest", "0" );

// Set this in the debugger to activate debugging spew
bool s_bSpew = false;

#endif // DEBUG_OCCLUSION_SYSTEM


//-----------------------------------------------------------------------------
// Visualization
//-----------------------------------------------------------------------------
struct EdgeVisualizationInfo_t
{
	Vector m_vecPoint[2];
	unsigned char m_pColor[4];
};

//-----------------------------------------------------------------------------
// Queued up rendering
//-----------------------------------------------------------------------------
static CUtlVector<EdgeVisualizationInfo_t> g_EdgeVisualization;


//-----------------------------------------------------------------------------
//
// Edge list that's fast to iterate over, fast to insert into
//
//-----------------------------------------------------------------------------
class CWingedEdgeList
{
public:
	struct WingedEdge_t
	{
		Vector m_vecPosition;		// of the upper point in y, measured in screen space
		Vector m_vecPositionEnd;	// of the lower point in y, measured in screen space
		float m_flDxDy;				// Change in x per unit in y.
		float m_flOODy;
		float m_flX;
		short m_nLeaveSurfID;		// Unique index of the surface this is a part of
		short m_nEnterSurfID;		// Unique index of the surface this is a part of
		WingedEdge_t *m_pPrevActiveEdge;
		WingedEdge_t *m_pNextActiveEdge;
	};

public:
	CWingedEdgeList();

	// Clears out the edge list
	void Clear();

	// Iteration
	int EdgeCount() const;
	WingedEdge_t &WingedEdge( int i );

	// Adds an edge
	int AddEdge( );
	int AddEdge( const Vector &vecStartVert, const Vector &vecEndVert, int nLeaveSurfID, int nEnterSurfID );

	// Adds a surface
	int AddSurface( const cplane_t &plane );

	// Does this edge list occlude another winged edge list?
	bool IsOccludingEdgeList( CWingedEdgeList &testList );

	// Queues up stuff to visualize
	void QueueVisualization( unsigned char *pColor );

	// Renders the winged edge list
	void Visualize( unsigned char *pColor );

	// Checks consistency of the edge list...
	void CheckConsistency();

private:
	struct Surface_t
	{
		cplane_t m_Plane;			// measured in projection space
	};

private:
	// Active edges...
	WingedEdge_t *FirstActiveEdge( );
	WingedEdge_t *LastActiveEdge( );
	bool AtListEnd( const WingedEdge_t* pEdge ) const;
	bool AtListStart( const WingedEdge_t* pEdge ) const;
	void LinkActiveEdgeAfter( WingedEdge_t *pPrevEdge, WingedEdge_t *pInsertEdge );
	void UnlinkActiveEdge( WingedEdge_t *pEdge );

	// Used to insert an edge into the active edge list
	bool IsEdgeXGreater( const WingedEdge_t *pEdge1, const WingedEdge_t *pEdge2 );

	// Clears the active edge list
	void ResetActiveEdgeList();

	// Spew active edge list
	void SpewActiveEdgeList( float y, bool bHex = false );

	// Inserts an edge into the active edge list, sorted by X
	void InsertActiveEdge( WingedEdge_t *pPrevEdge, WingedEdge_t *pInsertEdge );

	// Returns true if this active edge list occludes another active edge list
	bool IsOccludingActiveEdgeList( CWingedEdgeList &testList, float y );

	// Advances the X values of the active edge list, with no reordering
	bool AdvanceActiveEdgeList( float flCurrY );

	// Advance the active edge list until a particular X value is reached.
	WingedEdge_t *AdvanceActiveEdgeListToX( WingedEdge_t *pEdge, float x );

	// Returns the z value of a surface given and x,y coordinate
	float ComputeZValue( const Surface_t *pSurface, float x, float y ) const;

	// Returns the next time in Y the edge list will undergo a change
	float NextDiscontinuity() const;

private:
	// Active Edge list...
	WingedEdge_t m_StartTerminal;
	WingedEdge_t m_EndTerminal;

	// Back surface...
	Surface_t m_BackSurface;

	// Next discontinuity..
	float m_flNextDiscontinuity;
	int m_nCurrentEdgeIndex;

	CUtlVector< WingedEdge_t > m_WingedEdges;
	CUtlVector< Surface_t > m_Surfaces;
};



//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWingedEdgeList::CWingedEdgeList() : m_WingedEdges( 0, 64 )
{
	m_StartTerminal.m_vecPosition.Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );
	m_StartTerminal.m_vecPositionEnd.Init( -FLT_MAX, FLT_MAX, -FLT_MAX );
	m_StartTerminal.m_nLeaveSurfID = -1;
	m_StartTerminal.m_nEnterSurfID = -1;
	m_StartTerminal.m_pPrevActiveEdge = NULL;
	m_StartTerminal.m_pNextActiveEdge = NULL;
	m_StartTerminal.m_flDxDy = 0.0f;
	m_StartTerminal.m_flOODy = 0.0f;
	m_StartTerminal.m_flX = -FLT_MAX;

	m_EndTerminal.m_vecPosition.Init( FLT_MAX, -FLT_MAX, -FLT_MAX );
	m_EndTerminal.m_vecPositionEnd.Init( FLT_MAX, FLT_MAX, -FLT_MAX );
	m_EndTerminal.m_nLeaveSurfID = -1;
	m_EndTerminal.m_nEnterSurfID = -1;
	m_EndTerminal.m_pPrevActiveEdge = NULL;
	m_EndTerminal.m_pNextActiveEdge = NULL;
	m_EndTerminal.m_flDxDy = 0.0f;
	m_EndTerminal.m_flOODy = 0.0f;
	m_EndTerminal.m_flX = FLT_MAX;

	m_BackSurface.m_Plane.normal.Init( 0, 0, 1 );
	m_BackSurface.m_Plane.dist = FLT_MAX;
}


//-----------------------------------------------------------------------------
// Renders the winged edge list for debugging
//-----------------------------------------------------------------------------
void CWingedEdgeList::Clear()
{
	m_WingedEdges.RemoveAll();
	m_Surfaces.RemoveAll();
}


//-----------------------------------------------------------------------------
// Iterate over the winged edges
//-----------------------------------------------------------------------------
inline int CWingedEdgeList::EdgeCount() const 
{ 
	return m_WingedEdges.Count(); 
}

inline CWingedEdgeList::WingedEdge_t &CWingedEdgeList::WingedEdge( int i )
{
	return m_WingedEdges[i];
}


//-----------------------------------------------------------------------------
// Adds new edges
//-----------------------------------------------------------------------------
inline int CWingedEdgeList::AddEdge( )
{
	int i = m_WingedEdges.AddToTail();

	WingedEdge_t &newEdge = m_WingedEdges[i];
	newEdge.m_pPrevActiveEdge = NULL;
	newEdge.m_pNextActiveEdge = NULL;

	return i;
}

int CWingedEdgeList::AddEdge( const Vector &vecStartVert, const Vector &vecEndVert, int nLeaveSurfID, int nEnterSurfID )
{
	// This is true if we've clipped to the near clip plane
	Assert( (vecStartVert.z >= 0.0) && (vecEndVert.z >= 0.0) );

	// Don't bother adding edges with dy == 0
	float dy;
	dy = vecEndVert.y - vecStartVert.y;
	if (dy == 0.0f)
		return -1;

	int i = m_WingedEdges.AddToTail();
	WingedEdge_t &newEdge = m_WingedEdges[i];

	newEdge.m_flOODy = 1.0f / dy;
	newEdge.m_nLeaveSurfID = nLeaveSurfID;
	newEdge.m_nEnterSurfID = nEnterSurfID;
	newEdge.m_vecPosition = vecStartVert;
	newEdge.m_vecPositionEnd = vecEndVert;
	newEdge.m_pPrevActiveEdge = NULL;
	newEdge.m_pNextActiveEdge = NULL;
	newEdge.m_flDxDy = (vecEndVert.x - vecStartVert.x) * newEdge.m_flOODy;

	return i;
}


//-----------------------------------------------------------------------------
// Adds new surfaces
//-----------------------------------------------------------------------------
int CWingedEdgeList::AddSurface( const cplane_t &plane )
{
	int i = m_Surfaces.AddToTail();
	m_Surfaces[i].m_Plane = plane;
	return i;
}


//-----------------------------------------------------------------------------
// Active edges...
//-----------------------------------------------------------------------------
inline CWingedEdgeList::WingedEdge_t *CWingedEdgeList::FirstActiveEdge( )
{ 
	return m_StartTerminal.m_pNextActiveEdge; 
}

inline CWingedEdgeList::WingedEdge_t *CWingedEdgeList::LastActiveEdge( )
{ 
	return m_EndTerminal.m_pPrevActiveEdge; 
}

inline bool CWingedEdgeList::AtListEnd( const WingedEdge_t* pEdge ) const
{ 
	return pEdge == &m_EndTerminal; 
}

inline bool CWingedEdgeList::AtListStart( const WingedEdge_t* pEdge ) const
{ 
	return pEdge == &m_StartTerminal; 
}

inline void CWingedEdgeList::LinkActiveEdgeAfter( WingedEdge_t *pPrevEdge, WingedEdge_t *pInsertEdge )
{
	pInsertEdge->m_pNextActiveEdge = pPrevEdge->m_pNextActiveEdge;
	pInsertEdge->m_pPrevActiveEdge = pPrevEdge;
	pInsertEdge->m_pNextActiveEdge->m_pPrevActiveEdge = pInsertEdge;
	pPrevEdge->m_pNextActiveEdge = pInsertEdge;
}

inline void CWingedEdgeList::UnlinkActiveEdge( WingedEdge_t *pEdge )
{
	pEdge->m_pPrevActiveEdge->m_pNextActiveEdge = pEdge->m_pNextActiveEdge;
	pEdge->m_pNextActiveEdge->m_pPrevActiveEdge = pEdge->m_pPrevActiveEdge;

#ifdef _DEBUG
	pEdge->m_pPrevActiveEdge = pEdge->m_pNextActiveEdge = NULL;
#endif
}


//-----------------------------------------------------------------------------
// Checks consistency of the edge list...
//-----------------------------------------------------------------------------
void CWingedEdgeList::CheckConsistency()
{
	float flLastY = -FLT_MAX;
	float flLastX = -FLT_MAX;
	float flLastDxDy = 0;

	int nEdgeCount = EdgeCount();
	for ( int i = 0; i < nEdgeCount; ++i )
	{
		WingedEdge_t *pEdge = &WingedEdge(i);
		Assert( pEdge->m_vecPosition.y >= flLastY );
		if ( pEdge->m_vecPosition.y == flLastY )
		{
			Assert( pEdge->m_vecPosition.x >= flLastX );
			if ( pEdge->m_vecPosition.x == flLastX )
			{
				Assert( pEdge->m_flDxDy >= flLastDxDy );
			}
		}

		flLastX = pEdge->m_vecPosition.x;
		flLastY = pEdge->m_vecPosition.y;
		flLastDxDy = pEdge->m_flDxDy;
	}

	ResetActiveEdgeList();
	float flCurrentY = NextDiscontinuity();
	AdvanceActiveEdgeList( flCurrentY );
	while ( flCurrentY != FLT_MAX )
	{
		// Make sure all edges have correct Xs + enter + leave surfaces..
		int nCurrentSurfID = -1;
		float flX = -FLT_MAX;
		WingedEdge_t *pCurEdge = FirstActiveEdge();
		while ( !AtListEnd( pCurEdge ) )
		{
			Assert( pCurEdge->m_nLeaveSurfID == nCurrentSurfID );
			Assert( pCurEdge->m_flX >= flX );
			Assert( pCurEdge->m_nLeaveSurfID != pCurEdge->m_nEnterSurfID );
			nCurrentSurfID = pCurEdge->m_nEnterSurfID;
			flX = pCurEdge->m_flX;
			pCurEdge = pCurEdge->m_pNextActiveEdge;
		}
//		Assert( nCurrentSurfID == -1 );

		flCurrentY = NextDiscontinuity();
		AdvanceActiveEdgeList( flCurrentY );
	}
}


//-----------------------------------------------------------------------------
// Returns the z value of a surface given and x,y coordinate
//-----------------------------------------------------------------------------
inline float CWingedEdgeList::ComputeZValue( const Surface_t *pSurface, float x, float y ) const
{
	const cplane_t &plane = pSurface->m_Plane;
	Assert( plane.normal.z == 1.0f );
	return plane.dist - plane.normal.x * x - plane.normal.y * y; 
}


//-----------------------------------------------------------------------------
// Used to insert an edge into the active edge list, sorted by X
// If Xs match, sort by Dx/Dy
//-----------------------------------------------------------------------------
inline bool CWingedEdgeList::IsEdgeXGreater( const WingedEdge_t *pEdge1, const WingedEdge_t *pEdge2 )
{
	float flDelta = pEdge1->m_flX - pEdge2->m_flX;
	if ( flDelta > 0 )
		return true;

	if ( flDelta < 0 )
		return false;

	// NOTE: Using > instead of >= means coincident edges won't continually swap places
	return pEdge1->m_flDxDy > pEdge2->m_flDxDy;
}


//-----------------------------------------------------------------------------
// Inserts an edge into the active edge list, sorted by X
//-----------------------------------------------------------------------------
inline void CWingedEdgeList::InsertActiveEdge( WingedEdge_t *pPrevEdge, WingedEdge_t *pInsertEdge )
{
	while( !AtListStart(pPrevEdge) && IsEdgeXGreater( pPrevEdge, pInsertEdge ) )
	{
		pPrevEdge = pPrevEdge->m_pPrevActiveEdge;
	}
	LinkActiveEdgeAfter( pPrevEdge, pInsertEdge );
}


//-----------------------------------------------------------------------------
// Clears the active edge list
//-----------------------------------------------------------------------------
void CWingedEdgeList::ResetActiveEdgeList()
{
	// This shouldn't be called unless we're about to do active edge checking
	Assert( EdgeCount() );

	m_nCurrentEdgeIndex = 0;

	// Don't bother with edges below the screen edge
	m_flNextDiscontinuity = WingedEdge( 0 ).m_vecPosition.y;
	m_flNextDiscontinuity = max( m_flNextDiscontinuity, -1.0f );

	m_StartTerminal.m_pNextActiveEdge = &m_EndTerminal;
	m_EndTerminal.m_pPrevActiveEdge = &m_StartTerminal;
	Assert( m_StartTerminal.m_pPrevActiveEdge == NULL );
	Assert( m_EndTerminal.m_pNextActiveEdge == NULL );
}


//-----------------------------------------------------------------------------
// Spew active edge list
//-----------------------------------------------------------------------------
void CWingedEdgeList::SpewActiveEdgeList( float y, bool bHex )
{
	WingedEdge_t *pEdge = FirstActiveEdge();
	Msg( "%.3f : ", y );
	while ( !AtListEnd( pEdge ) )
	{
		if (!bHex)
		{
			Msg( "(%d %.3f [%d/%d]) ", (int)(pEdge - m_WingedEdges.Base()), pEdge->m_flX, pEdge->m_nLeaveSurfID, pEdge->m_nEnterSurfID );
		}
		else
		{
			Msg( "(%d %X [%d/%d]) ", (int)(pEdge - m_WingedEdges.Base()), *(int*)&pEdge->m_flX, pEdge->m_nLeaveSurfID, pEdge->m_nEnterSurfID );
		}
		pEdge = pEdge->m_pNextActiveEdge;
	}
	Msg( "\n" );
}


//-----------------------------------------------------------------------------
// Returns the next time in Y the edge list will undergo a change
//-----------------------------------------------------------------------------
inline float CWingedEdgeList::NextDiscontinuity() const
{
	return m_flNextDiscontinuity;
}


//-----------------------------------------------------------------------------
// Advances the X values of the active edge list, with no reordering
//-----------------------------------------------------------------------------
bool CWingedEdgeList::AdvanceActiveEdgeList( float flCurrY )
{
	// Reordering is unnecessary because the winged edges are guaranteed to be non-overlapping
	m_flNextDiscontinuity = FLT_MAX;

	// Advance all edges until the current Y; we don't need to re-order *any* edges.
	WingedEdge_t *pCurEdge;
	WingedEdge_t *pNextEdge;
	for ( pCurEdge = FirstActiveEdge(); !AtListEnd( pCurEdge ); pCurEdge = pNextEdge )
	{
		pNextEdge = pCurEdge->m_pNextActiveEdge;

		if ( pCurEdge->m_vecPositionEnd.y <= flCurrY )
		{
			UnlinkActiveEdge( pCurEdge );
			continue;
		}

		pCurEdge->m_flX = pCurEdge->m_vecPosition.x + (flCurrY - pCurEdge->m_vecPosition.y) * pCurEdge->m_flDxDy;
		if ( pCurEdge->m_vecPositionEnd.y < m_flNextDiscontinuity )
		{
			m_flNextDiscontinuity = pCurEdge->m_vecPositionEnd.y;
		}
	}

	int nEdgeCount = EdgeCount();
	if ( m_nCurrentEdgeIndex == nEdgeCount )
		return (m_flNextDiscontinuity != FLT_MAX);

	pCurEdge = &WingedEdge( m_nCurrentEdgeIndex );

	// Add new edges, computing the x + z coordinates at the requested y value
	while ( pCurEdge->m_vecPosition.y <= flCurrY )
	{
		// This is necessary because of our initial skip up to y == -1.0f
		if ( pCurEdge->m_vecPositionEnd.y > flCurrY )
		{
			float flDy = flCurrY - pCurEdge->m_vecPosition.y; 
			pCurEdge->m_flX = pCurEdge->m_vecPosition.x + flDy * pCurEdge->m_flDxDy;
			if ( pCurEdge->m_vecPositionEnd.y < m_flNextDiscontinuity )
			{
				m_flNextDiscontinuity = pCurEdge->m_vecPositionEnd.y;
			}

			// Now re-insert in the list, sorted by X
			InsertActiveEdge( LastActiveEdge(), pCurEdge );
		}

		if ( ++m_nCurrentEdgeIndex == nEdgeCount )
			return (m_flNextDiscontinuity != FLT_MAX);

		pCurEdge = &WingedEdge( m_nCurrentEdgeIndex );
	}

	// The next edge in y will also present a discontinuity
	if ( pCurEdge->m_vecPosition.y < m_flNextDiscontinuity )
	{
		m_flNextDiscontinuity = pCurEdge->m_vecPosition.y;
	}

	return (m_flNextDiscontinuity != FLT_MAX);
}


//-----------------------------------------------------------------------------
// Advance the active edge list until a particular X value is reached.
//-----------------------------------------------------------------------------
inline CWingedEdgeList::WingedEdge_t *CWingedEdgeList::AdvanceActiveEdgeListToX( WingedEdge_t *pEdge, float x )
{
	// <= is necessary because we always want to point *after* the edge
	while( pEdge->m_flX <= x )
	{
		pEdge = pEdge->m_pNextActiveEdge;
	}
	return pEdge;
}


//-----------------------------------------------------------------------------
// Returns true if this active edge list occludes another active edge list
//-----------------------------------------------------------------------------
bool CWingedEdgeList::IsOccludingActiveEdgeList( CWingedEdgeList &testList, float y )
{
	WingedEdge_t *pTestEdge = testList.FirstActiveEdge();

	// If the occludee is off screen, it's occluded
	if ( pTestEdge->m_flX >= 1.0f )
		return true;

	pTestEdge = AdvanceActiveEdgeListToX( pTestEdge, -1.0f );

	// If all occludee edges have x values <= -1.0f, it's occluded
	if ( testList.AtListEnd( pTestEdge ) )
		return true;

	// Start at the first edge whose x value is <= -1.0f 
	// if the occludee goes off the left side of the screen.
	float flNextTestX = pTestEdge->m_flX;
	if ( !testList.AtListStart( pTestEdge->m_pPrevActiveEdge ) )
	{
		// In this case, we should be on a span crossing from x <= -1.0f to x > 1.0f.
		// Do the first occlusion test at x = -1.0f.
		Assert( pTestEdge->m_flX > -1.0f );
		pTestEdge = pTestEdge->m_pPrevActiveEdge;
		Assert( pTestEdge->m_flX <= -1.0f );
		flNextTestX = -1.0f;
	}

	WingedEdge_t *pOccluderEdge = FirstActiveEdge();
	pOccluderEdge = AdvanceActiveEdgeListToX( pOccluderEdge, flNextTestX );

	Surface_t *pTestSurf = (pTestEdge->m_nEnterSurfID >= 0) ? &testList.m_Surfaces[pTestEdge->m_nEnterSurfID] : &m_BackSurface;

	// Use the leave surface because we know the occluder has been advanced *beyond* the test surf X.
	Surface_t *pOccluderSurf = (pOccluderEdge->m_nLeaveSurfID >= 0) ? &m_Surfaces[pOccluderEdge->m_nLeaveSurfID] : &m_BackSurface;

	float flCurrentX = flNextTestX;
	float flNextOccluderX = pOccluderEdge->m_flX;
	flNextTestX = pTestEdge->m_pNextActiveEdge->m_flX;

	while ( true )
	{
		// Is the occludee in front of the occluder? No dice!
		float flTestOOz = ComputeZValue( pTestSurf, flCurrentX, y );
		float flOccluderOOz = ComputeZValue( pOccluderSurf, flCurrentX, y );
		if ( flTestOOz < flOccluderOOz )
			return false;

		// We're done if there's no more occludees
		if ( flNextTestX == FLT_MAX )
			return true;

		// We're done if there's no more occluders
		if ( flNextOccluderX == FLT_MAX )
			return false;

		if ( flNextTestX <= flNextOccluderX )
		{
			flCurrentX = flNextTestX;
			pTestEdge = pTestEdge->m_pNextActiveEdge;
			if ( pTestEdge->m_nEnterSurfID >= 0 )
			{
				pTestSurf = &testList.m_Surfaces[pTestEdge->m_nEnterSurfID];
			}
			else
			{
				pTestSurf = (pTestEdge->m_nLeaveSurfID >= 0) ? &testList.m_Surfaces[pTestEdge->m_nLeaveSurfID] : &m_BackSurface;
			}
			flNextTestX = pTestEdge->m_pNextActiveEdge->m_flX;
		}
		else
		{
			flCurrentX = flNextOccluderX;
			pOccluderEdge = pOccluderEdge->m_pNextActiveEdge;
			pOccluderSurf = (pOccluderEdge->m_nLeaveSurfID >= 0) ? &m_Surfaces[pOccluderEdge->m_nLeaveSurfID] : &m_BackSurface;
			flNextOccluderX = pOccluderEdge->m_flX;
		}
	}
}


//-----------------------------------------------------------------------------
// Does this edge list occlude another winged edge list?
//-----------------------------------------------------------------------------
bool CWingedEdgeList::IsOccludingEdgeList( CWingedEdgeList &testList )
{
#ifdef DEBUG_OCCLUSION_SYSTEM
	testList.CheckConsistency();
	CheckConsistency();
#endif

	// Did all the edges get culled for some reason? Then it's occluded
	if ( testList.EdgeCount() == 0 )
		return true;

	testList.ResetActiveEdgeList();
	ResetActiveEdgeList();

	// What we're going to do is look for the first discontinuities we can find
	// in both edge lists. Then, at each discontinuity, we must check the 
	// active edge lists against each other and see if the occluders always
	// block the occludees...
	float flCurrentY = testList.NextDiscontinuity();

	// The edge list for the occluder must completely obscure the occludee...
	// If, then, the first occluder edge starts *below* the first occludee edge, it doesn't occlude.
	if ( flCurrentY < NextDiscontinuity() )
		return false;

	// If we start outside the screen bounds, then it's occluded!
	if ( flCurrentY >= 1.0f )
		return true;

	testList.AdvanceActiveEdgeList( flCurrentY );
	AdvanceActiveEdgeList( flCurrentY );

	while ( true )
	{
		if ( !IsOccludingActiveEdgeList( testList, flCurrentY ) )
			return false;

		// If we got outside the screen bounds, then it's occluded!
		if ( flCurrentY >= 1.0f )
			return true;

		float flTestY = testList.NextDiscontinuity();
		float flOccluderY = NextDiscontinuity();
		flCurrentY = min( flTestY, flOccluderY );

		// NOTE: This check here is to help occlusion @ the top of the screen
		// We cut the occluders off at y = 1.0 + epsilon, which means there's
		// not necessarily a discontinuity at y == 1.0. We need to create a discontinuity
		// there so that the occluder edges are still being used.
		if ( flCurrentY > 1.0f )
		{
			flCurrentY = 1.0f;
		}

		// If the occludee list is empty, then it's occluded! 
		if ( !testList.AdvanceActiveEdgeList( flCurrentY ) )
			return true;
		
		// If the occluder list is empty, then the occludee is not occluded!
		if ( !AdvanceActiveEdgeList( flCurrentY ) )
			return false;
	}
}


//-----------------------------------------------------------------------------
// Queues up stuff to visualize
//-----------------------------------------------------------------------------
void CWingedEdgeList::QueueVisualization( unsigned char *pColor )
{
#ifndef SWDS
	if ( !r_visocclusion.GetInt() )
		return;

	int nFirst = g_EdgeVisualization.AddMultipleToTail( m_WingedEdges.Count() );
	for ( int i = m_WingedEdges.Count(); --i >= 0; )
	{
		WingedEdge_t *pEdge = &m_WingedEdges[i];
		EdgeVisualizationInfo_t &info = g_EdgeVisualization[nFirst + i];
		info.m_vecPoint[0] = pEdge->m_vecPosition;
		info.m_vecPoint[1] = pEdge->m_vecPositionEnd;
		*(int*)(info.m_pColor) = *(int*)pColor;
	}
#endif
}


//-----------------------------------------------------------------------------
// Renders the winged edge list for debugging
//-----------------------------------------------------------------------------
void CWingedEdgeList::Visualize( unsigned char *pColor )
{
#ifndef SWDS
	if ( !r_visocclusion.GetInt() )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->Bind( g_pMaterialWireframeVertexColorIgnoreZ );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, m_WingedEdges.Count() );

	int i;
	int nCount = m_WingedEdges.Count();
	for ( i = nCount; --i >= 0; )
	{
		WingedEdge_t *pEdge = &m_WingedEdges[i];
		meshBuilder.Position3fv( pEdge->m_vecPosition.Base() );
		meshBuilder.Color4ubv( pColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( pEdge->m_vecPositionEnd.Base() );
#ifdef DEBUG_OCCLUSION_SYSTEM
		meshBuilder.Color4ub( 0, 0, 255, 255 );
#else
		meshBuilder.Color4ubv( pColor );
#endif
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
#endif
}


//-----------------------------------------------------------------------------
// Edge list that's fast to iterate over, fast to insert into
//-----------------------------------------------------------------------------
class CEdgeList
{
public:
	struct Edge_t
	{
		Vector m_vecPosition;		// of the upper point in y, measured in screen space
		Vector m_vecPositionEnd;	// of the lower point in y, measured in screen space
		float m_flDxDy;				// Change in x per unit in y.
		float m_flOODy;
		float m_flX;
		int   m_nSurfID;			// Unique index of the surface this is a part of

		// Active edge list
		Edge_t *m_pPrevActiveEdge;
		Edge_t *m_pNextActiveEdge;
	};

public:
	CEdgeList();

	// Insertion
	void AddEdge( Vector **ppEdgeVertices, int nSurfID );

	// Surface ID management
	int AddSurface( const cplane_t &plane );
	void SetSurfaceArea( int nSurfID, float flArea );

	// Removal
	void RemoveAll();

	// Visualization
	void QueueVisualization( unsigned char *pColor );
	void Visualize( unsigned char *pColor );

	// Access
	int EdgeCount() const;
	int ActualEdgeCount() const;
	const Edge_t &EdgeFromSortIndex( int nSortIndex ) const;
	Edge_t &EdgeFromSortIndex( int nSortIndex );

	// Is the test edge list occluded by this edge list
	bool IsOccludingEdgeList( CEdgeList &testList );

	// Reduces the active occlusion edge list to the bare minimum set of edges
	void ReduceActiveList( CWingedEdgeList &newEdgeList );

	// Removal of small occluders
	void CullSmallOccluders();

private:
	struct Surface_t
	{
		cplane_t m_Plane;		// measured in projection space
		float m_flOOz;
		Surface_t *m_pPrevSurface;
		Surface_t *m_pNextSurface;
		int m_nSurfID;
		float m_flArea;			// Area in screen space
	};

	struct ReduceInfo_t
	{
		short m_hEdge;
		short m_nWingedEdge;
		const Edge_t *m_pEdge;
	};

	enum
	{
		MAX_EDGE_CROSSINGS = 64
	};

	typedef CUtlVector<Edge_t> EdgeList_t;
	 
private:
	// Gets an edge
	const Edge_t &Edge( int nIndex ) const;

	// Active edges...
	const Edge_t *FirstActiveEdge( ) const;
	Edge_t *FirstActiveEdge( );
	const Edge_t *LastActiveEdge( ) const;
	Edge_t *LastActiveEdge( );
	bool AtListEnd( const Edge_t* pEdge ) const;
	bool AtListStart( const Edge_t* pEdge ) const;
	void LinkActiveEdgeAfter( Edge_t *pPrevEdge, Edge_t *pInsertEdge );
	void UnlinkActiveEdge( Edge_t *pEdge );

	// Surface list
	Surface_t* TopSurface();
	bool AtSurfListEnd( const Surface_t* pSurface ) const;
	void CleanupCurrentSurfaceList();

	// Active edge list
	void ResetActiveEdgeList();
	float NextDiscontinuity() const;

	// Clears the current scan line
	float ClearCurrentSurfaceList();

	// Returns the z value of a surface given and x,y coordinate
	float ComputeZValue( const Surface_t *pSurface, float x, float y ) const;

	// Computes a point at a specified y value along an edge
	void ComputePointAlongEdge( const Edge_t *pEdge, int nSurfID, float y, Vector *pPoint ) const;

	// Inserts an edge into the active edge list, sorted by X
	void InsertActiveEdge( Edge_t *pPrevEdge, Edge_t *pInsertEdge );

	// Used to insert an edge into the active edge list
	bool IsEdgeXGreater( const Edge_t *pEdge1, const Edge_t *pEdge2 );

	// Reduces the active edge list into a subset of ones we truly care about
	void ReduceActiveEdgeList( CWingedEdgeList &newEdgeList, float flMinY, float flMaxY );

	// Discovers the first edge crossing discontinuity
	float LocateEdgeCrossingDiscontinuity( float flNextY, float flPrevY, int &nCount, Edge_t **pInfo );

	// Generates a list of surfaces on the current scan line
	void UpdateCurrentSurfaceZValues( float x, float y );

	// Intoruces a single new edge
	void IntroduceSingleActiveEdge( const Edge_t *pEdge, float flCurrY );

	// Returns true if pTestSurf is closer (lower z value)
	bool IsSurfaceBehind( Surface_t *pTestSurf, Surface_t *pSurf );

	// Advances the X values of the active edge list, with no reordering
	void AdvanceActiveEdgeList( float flNextY );

	void IntroduceNewActiveEdges( float y );
	void ReorderActiveEdgeList( int nCount, Edge_t **ppInfo );

	// Debugging spew
	void SpewActiveEdgeList( float y, bool bHex = false );

	// Checks consistency of the edge list...
	void CheckConsistency();

	class EdgeLess
	{
	public:
        bool Less( const unsigned short& src1, const unsigned short& src2, void *pCtx );
	};

	static int __cdecl SurfCompare( const void *elem1, const void *elem2 );

private:
	// Used to sort surfaces by screen area
	static Surface_t *s_pSortSurfaces;

	// List of all edges
	EdgeList_t m_Edges;
	CUtlSortVector<unsigned short, EdgeLess > m_OrigSortIndices;
	CUtlVector<unsigned short> m_SortIndices;
	Edge_t m_StartTerminal;
	Edge_t m_EndTerminal;

	// Surfaces
	CUtlVector< Surface_t > m_Surfaces;
	CUtlVector< int > m_SurfaceSort;
	Surface_t m_StartSurfTerminal;
	Surface_t m_EndSurfTerminal;

	// Active edges
	int m_nCurrentEdgeIndex;
	float m_flNextDiscontinuity;

	// List of edges on the current Y scan-line
	Edge_t *m_pCurrentActiveEdge;

	// Last X on the current scan line
	float m_flLastX;

	// Reduce list
	ReduceInfo_t *m_pNewReduceInfo;
	ReduceInfo_t *m_pPrevReduceInfo;
	int m_nNewReduceCount;
	int m_nPrevReduceCount;
};

		
//-----------------------------------------------------------------------------
// Used to sort the edge list
//-----------------------------------------------------------------------------
bool CEdgeList::EdgeLess::Less( const unsigned short& src1, const unsigned short& src2, void *pCtx )
{
	EdgeList_t *pEdgeList = (EdgeList_t*)pCtx;

	const Edge_t &e1 = pEdgeList->Element(src1);
	const Edge_t &e2 = pEdgeList->Element(src2);

	if ( e1.m_vecPosition.y < e2.m_vecPosition.y )
		return true;

	if ( e1.m_vecPosition.y > e2.m_vecPosition.y )
		return false;

	if ( e1.m_vecPosition.x < e2.m_vecPosition.x )
		return true;

	if ( e1.m_vecPosition.x > e2.m_vecPosition.x )
		return false;

	// This makes it so that if two edges start on the same point,
	// the leftmost edge is always selected
	return ( e1.m_flDxDy <= e2.m_flDxDy );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEdgeList::CEdgeList() : m_Edges( 0, 32 ), m_OrigSortIndices( 0, 32 )
{
	m_OrigSortIndices.SetLessContext( &m_Edges );

	m_StartTerminal.m_vecPosition.Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );
	m_StartTerminal.m_vecPositionEnd.Init( -FLT_MAX, FLT_MAX, -FLT_MAX );
	m_StartTerminal.m_nSurfID = -1;
	m_StartTerminal.m_pPrevActiveEdge = NULL;
	m_StartTerminal.m_pNextActiveEdge = NULL;
	m_StartTerminal.m_flDxDy = 0.0f;
	m_StartTerminal.m_flOODy = 0.0f;
	m_StartTerminal.m_flX = -FLT_MAX;

	m_EndTerminal.m_vecPosition.Init( FLT_MAX, -FLT_MAX, -FLT_MAX );
	m_EndTerminal.m_vecPositionEnd.Init( FLT_MAX, FLT_MAX, -FLT_MAX );
	m_EndTerminal.m_nSurfID = -1;
	m_EndTerminal.m_pPrevActiveEdge = NULL;
	m_EndTerminal.m_pNextActiveEdge = NULL;
	m_EndTerminal.m_flDxDy = 0.0f;
	m_EndTerminal.m_flOODy = 0.0f;
	m_EndTerminal.m_flX = FLT_MAX;

	m_StartSurfTerminal.m_flOOz = -FLT_MAX;
	m_StartSurfTerminal.m_Plane.normal.Init( 0, 0, 1 );
	m_StartSurfTerminal.m_Plane.dist = -FLT_MAX;
	m_StartSurfTerminal.m_nSurfID = -1;
	m_StartSurfTerminal.m_pNextSurface = NULL;
	m_StartSurfTerminal.m_pPrevSurface = NULL;

	m_EndSurfTerminal.m_flOOz = FLT_MAX;
	m_EndSurfTerminal.m_Plane.normal.Init( 0, 0, 1 );
	m_EndSurfTerminal.m_Plane.dist = FLT_MAX;
	m_EndSurfTerminal.m_nSurfID = -1;
	m_EndSurfTerminal.m_pNextSurface = NULL;
	m_EndSurfTerminal.m_pPrevSurface = NULL;
}


//-----------------------------------------------------------------------------
// iteration
//-----------------------------------------------------------------------------
inline int CEdgeList::EdgeCount() const
{
	return m_Edges.Count();
}

inline int CEdgeList::ActualEdgeCount() const
{
	return m_SortIndices.Count();
}

inline const CEdgeList::Edge_t &CEdgeList::EdgeFromSortIndex( int nSortIndex ) const
{
	return m_Edges[ m_SortIndices[nSortIndex] ];
}

inline CEdgeList::Edge_t &CEdgeList::EdgeFromSortIndex( int nSortIndex )
{
	return m_Edges[ m_SortIndices[nSortIndex] ];
}

inline const CEdgeList::Edge_t &CEdgeList::Edge( int nIndex ) const
{
	return m_Edges[ nIndex ];
}


//-----------------------------------------------------------------------------
// Active edges...
//-----------------------------------------------------------------------------
inline const CEdgeList::Edge_t *CEdgeList::FirstActiveEdge( ) const
{ 
	return m_StartTerminal.m_pNextActiveEdge; 
}

inline CEdgeList::Edge_t *CEdgeList::FirstActiveEdge( )
{ 
	return m_StartTerminal.m_pNextActiveEdge; 
}

inline const CEdgeList::Edge_t *CEdgeList::LastActiveEdge( ) const
{ 
	return m_EndTerminal.m_pPrevActiveEdge; 
}

inline CEdgeList::Edge_t *CEdgeList::LastActiveEdge( )
{ 
	return m_EndTerminal.m_pPrevActiveEdge; 
}

inline bool CEdgeList::AtListEnd( const Edge_t* pEdge ) const
{ 
	return pEdge == &m_EndTerminal; 
}

inline bool CEdgeList::AtListStart( const Edge_t* pEdge ) const
{ 
	return pEdge == &m_StartTerminal; 
}

inline void CEdgeList::LinkActiveEdgeAfter( Edge_t *pPrevEdge, Edge_t *pInsertEdge )
{
	pInsertEdge->m_pNextActiveEdge = pPrevEdge->m_pNextActiveEdge;
	pInsertEdge->m_pPrevActiveEdge = pPrevEdge;
	pInsertEdge->m_pNextActiveEdge->m_pPrevActiveEdge = pInsertEdge;
	pPrevEdge->m_pNextActiveEdge = pInsertEdge;
}

inline void CEdgeList::UnlinkActiveEdge( Edge_t *pEdge )
{
	pEdge->m_pPrevActiveEdge->m_pNextActiveEdge = pEdge->m_pNextActiveEdge;
	pEdge->m_pNextActiveEdge->m_pPrevActiveEdge = pEdge->m_pPrevActiveEdge;

#ifdef _DEBUG
	pEdge->m_pPrevActiveEdge = pEdge->m_pNextActiveEdge = NULL;
#endif
}


//-----------------------------------------------------------------------------
// Surface list
//-----------------------------------------------------------------------------
inline CEdgeList::Surface_t* CEdgeList::TopSurface()
{
	return m_StartSurfTerminal.m_pNextSurface;
}

inline bool CEdgeList::AtSurfListEnd( const Surface_t* pSurface ) const
{
	return pSurface == &m_EndSurfTerminal;
}

void CEdgeList::CleanupCurrentSurfaceList()
{
	Surface_t *pSurf = TopSurface();
	while ( !AtSurfListEnd(pSurf) )
	{
		Surface_t *pNext = pSurf->m_pNextSurface;
		pSurf->m_pPrevSurface = pSurf->m_pNextSurface = NULL;
		pSurf = pNext;
	}
}

inline void CEdgeList::SetSurfaceArea( int nSurfID, float flArea )
{
	m_Surfaces[nSurfID].m_flArea = flArea;
}


//-----------------------------------------------------------------------------
// Returns the z value of a surface given and x,y coordinate
//-----------------------------------------------------------------------------
inline float CEdgeList::ComputeZValue( const Surface_t *pSurface, float x, float y ) const
{
	const cplane_t &plane = pSurface->m_Plane;
	Assert( plane.normal.z == 1.0f );
	return plane.dist - plane.normal.x * x - plane.normal.y * y; 
}


//-----------------------------------------------------------------------------
// Computes a point at a specified y value along an edge
//-----------------------------------------------------------------------------
inline void CEdgeList::ComputePointAlongEdge( const Edge_t *pEdge, int nSurfID, float y, Vector *pPoint ) const
{
	Assert( (y >= pEdge->m_vecPosition.y) && (y <= pEdge->m_vecPositionEnd.y) );

	float t;
	t = (y - pEdge->m_vecPosition.y) * pEdge->m_flOODy;
	pPoint->x = pEdge->m_vecPosition.x + ( pEdge->m_vecPositionEnd.x - pEdge->m_vecPosition.x ) * t;
	pPoint->y = y;
	pPoint->z = ComputeZValue( &m_Surfaces[nSurfID], pPoint->x, y );
}


//-----------------------------------------------------------------------------
// Surface ID management
//-----------------------------------------------------------------------------
int CEdgeList::AddSurface( const cplane_t &plane )
{
	int nIndex = m_Surfaces.AddToTail();

	Surface_t &surf = m_Surfaces[nIndex];
	surf.m_flOOz = 0.0f;
	surf.m_Plane = plane;
	surf.m_pNextSurface = NULL;
	surf.m_pPrevSurface = NULL;
	surf.m_nSurfID = nIndex;

	m_SurfaceSort.AddToTail(nIndex);

	return nIndex;
}


//-----------------------------------------------------------------------------
// Insertion
//-----------------------------------------------------------------------------
void CEdgeList::AddEdge( Vector **ppEdgeVertices, int nSurfID )
{
	int nMinIndex = ( ppEdgeVertices[0]->y >= ppEdgeVertices[1]->y );

	const Vector &vecStartVert = *(ppEdgeVertices[ nMinIndex ]);
	const Vector &vecEndVert = *(ppEdgeVertices[ 1 - nMinIndex ]);

	// This is true if we've clipped to the near clip plane
	Assert( (vecStartVert.z >= 0.0f) && (vecEndVert.z >= 0.0f) );

	// Don't bother adding edges with dy == 0
	float dy = vecEndVert.y - vecStartVert.y;
	if (dy == 0.0f)
		return;

	int i = m_Edges.AddToTail();
	Edge_t &newEdge = m_Edges[i];

	newEdge.m_flOODy = 1.0f / dy;
	newEdge.m_vecPosition = vecStartVert;
	newEdge.m_vecPositionEnd = vecEndVert;
	newEdge.m_nSurfID = nSurfID;
	newEdge.m_flDxDy = (vecEndVert.x - vecStartVert.x) * newEdge.m_flOODy;
	newEdge.m_pPrevActiveEdge = NULL;
	newEdge.m_pNextActiveEdge = NULL;

	// Insert it into the sorted list
	m_OrigSortIndices.Insert( i );
}


//-----------------------------------------------------------------------------
// Used to sort the surfaces
//-----------------------------------------------------------------------------
CEdgeList::Surface_t *CEdgeList::s_pSortSurfaces = NULL;
int __cdecl CEdgeList::SurfCompare( const void *elem1, const void *elem2 )
{
	int nSurfID1 = *(int*)elem1;
	float flArea1 = s_pSortSurfaces[nSurfID1].m_flArea;

	int nSurfID2 = *(int*)elem2;
	float flArea2 = s_pSortSurfaces[nSurfID2].m_flArea;

	if (flArea1 > flArea2)
		return -1;
	if (flArea1 < flArea2)
		return 1;
	return 0;
}


//-----------------------------------------------------------------------------
// Removal of small occluders
//-----------------------------------------------------------------------------
void CEdgeList::CullSmallOccluders()
{	 
	// Cull out all surfaces with too small of a screen area...
	// Sort the surfaces by screen area, in descending order
	int nSurfCount = m_Surfaces.Count();
	s_pSortSurfaces = m_Surfaces.Base();
	qsort( m_SurfaceSort.Base(), nSurfCount, sizeof(int), SurfCompare );

	// We're going to keep the greater of r_occludermin + All surfaces with a screen area >= r_occluderarea
	int nMinSurfaces = r_occludermincount.GetInt();

	// The *2 here is because surf areas are 2x bigger than actual
	float flMinScreenArea = r_occluderminarea.GetFloat() * 0.02f;
	if ( flMinScreenArea == 0.0f )
	{
		flMinScreenArea = OcclusionSystem()->MinOccluderArea() * 0.02f;
	}

	bool *bUseSurface = (bool*)stackalloc( nSurfCount * sizeof(bool) );
	memset( bUseSurface, 0, nSurfCount * sizeof(bool) );
	
	int i;
	for ( i = 0; i < nSurfCount; ++i )
	{
		int nSurfID = m_SurfaceSort[i];
		if (( m_Surfaces[ nSurfID ].m_flArea < flMinScreenArea ) && (i >= nMinSurfaces ))
			break;
		bUseSurface[nSurfID] = true;
	}

	MEM_ALLOC_CREDIT();

	int nEdgeCount = m_OrigSortIndices.Count();
	m_SortIndices.RemoveAll();
	m_SortIndices.EnsureCapacity( nEdgeCount );
	for( i = 0; i < nEdgeCount; ++i )
	{
		int nEdgeIndex = m_OrigSortIndices[i];
		if ( bUseSurface[ m_Edges[ nEdgeIndex ].m_nSurfID ] )
		{
			m_SortIndices.AddToTail( nEdgeIndex ); 
		}
	}
}


//-----------------------------------------------------------------------------
// Removal
//-----------------------------------------------------------------------------
void CEdgeList::RemoveAll()
{
	m_Edges.RemoveAll();
	m_SortIndices.RemoveAll();
	m_OrigSortIndices.RemoveAll();
	m_Surfaces.RemoveAll();
	m_SurfaceSort.RemoveAll();
}


//-----------------------------------------------------------------------------
// Active edge list
//-----------------------------------------------------------------------------
void CEdgeList::ResetActiveEdgeList()
{
	// This shouldn't be called unless we're about to do active edge checking
	Assert( ActualEdgeCount() );

	m_nCurrentEdgeIndex = 0;
	m_flNextDiscontinuity = EdgeFromSortIndex( 0 ).m_vecPosition.y;

	m_StartTerminal.m_pNextActiveEdge = &m_EndTerminal;
	m_EndTerminal.m_pPrevActiveEdge = &m_StartTerminal;
	Assert( m_StartTerminal.m_pPrevActiveEdge == NULL );
	Assert( m_EndTerminal.m_pNextActiveEdge == NULL );
}


//-----------------------------------------------------------------------------
// Returns the next time in Y the edge list will undergo a change
//-----------------------------------------------------------------------------
inline float CEdgeList::NextDiscontinuity() const
{
	return m_flNextDiscontinuity;
}


//-----------------------------------------------------------------------------
// Used to insert an edge into the active edge list, sorted by X
// If Xs match, sort by Dx/Dy
//-----------------------------------------------------------------------------
inline bool CEdgeList::IsEdgeXGreater( const Edge_t *pEdge1, const Edge_t *pEdge2 )
{
	float flDelta = pEdge1->m_flX - pEdge2->m_flX;
	if ( flDelta > 0 )
		return true;

	if ( flDelta < 0 )
		return false;

	// NOTE: Using > instead of >= means coincident edges won't continually swap places
	return pEdge1->m_flDxDy > pEdge2->m_flDxDy;
}


//-----------------------------------------------------------------------------
// Inserts an edge into the active edge list, sorted by X
//-----------------------------------------------------------------------------
inline void CEdgeList::InsertActiveEdge( Edge_t *pPrevEdge, Edge_t *pInsertEdge )
{
	while( !AtListStart(pPrevEdge) && IsEdgeXGreater( pPrevEdge, pInsertEdge ) )
	{
		pPrevEdge = pPrevEdge->m_pPrevActiveEdge;
	}
	LinkActiveEdgeAfter( pPrevEdge, pInsertEdge );
}


//-----------------------------------------------------------------------------
// Clears the current scan line
//-----------------------------------------------------------------------------
float CEdgeList::ClearCurrentSurfaceList()
{
	m_pCurrentActiveEdge = FirstActiveEdge();
	m_flLastX = m_pCurrentActiveEdge->m_flX;
	m_StartSurfTerminal.m_pNextSurface = &m_EndSurfTerminal;
	m_EndSurfTerminal.m_pPrevSurface = &m_StartSurfTerminal;
	return m_flLastX;
}


//-----------------------------------------------------------------------------
// Generates a list of surfaces on the current scan line
//-----------------------------------------------------------------------------
inline void CEdgeList::UpdateCurrentSurfaceZValues( float x, float y )
{
	// Update the z values of all active surfaces
	for ( Surface_t *pSurf = TopSurface(); !AtSurfListEnd( pSurf ); pSurf = pSurf->m_pNextSurface )
	{
		// NOTE: As long as we assume no interpenetrating surfaces,
		// we don't need to re-sort by ooz here.
		pSurf->m_flOOz = ComputeZValue( pSurf, x, y );
	}
}


//-----------------------------------------------------------------------------
// Returns true if pTestSurf is closer (lower z value)
//-----------------------------------------------------------------------------
inline bool CEdgeList::IsSurfaceBehind( Surface_t *pTestSurf, Surface_t *pSurf )
{
	if ( pTestSurf->m_flOOz - pSurf->m_flOOz <= -1e-6 )
		return true;
	if ( pTestSurf->m_flOOz - pSurf->m_flOOz >= 1e-6 )
		return false;

	// If they're nearly equal, then the thing that's approaching the screen
	// more quickly as we ascend in y is closer
	return ( pTestSurf->m_Plane.normal.y >= pSurf->m_Plane.normal.y );
}


//-----------------------------------------------------------------------------
// Introduces a single new edge
//-----------------------------------------------------------------------------
void CEdgeList::IntroduceSingleActiveEdge( const Edge_t *pEdge, float flCurrY )
{
	Surface_t *pCurrentSurf = &m_Surfaces[ pEdge->m_nSurfID ];
	if ( !pCurrentSurf->m_pNextSurface )
	{
		pCurrentSurf->m_flOOz = ComputeZValue( pCurrentSurf, pEdge->m_flX, flCurrY );

		// Determine where to insert the surface into the surface list...
		// Insert it so that the surface list is sorted by OOz
		Surface_t *pNextSurface = TopSurface();
		while( IsSurfaceBehind( pNextSurface, pCurrentSurf ) )
		{
			pNextSurface = pNextSurface->m_pNextSurface;
		}
		pCurrentSurf->m_pNextSurface = pNextSurface;
		pCurrentSurf->m_pPrevSurface = pNextSurface->m_pPrevSurface;
		pNextSurface->m_pPrevSurface = pCurrentSurf;
		pCurrentSurf->m_pPrevSurface->m_pNextSurface = pCurrentSurf;
	}
	else
	{
		// This means this edge is associated with a surface
		// already in the current surface list
		// In this case, simply remove the surface from the surface list
		pCurrentSurf->m_pNextSurface->m_pPrevSurface = pCurrentSurf->m_pPrevSurface;
		pCurrentSurf->m_pPrevSurface->m_pNextSurface = pCurrentSurf->m_pNextSurface;
		pCurrentSurf->m_pPrevSurface = pCurrentSurf->m_pNextSurface = NULL;
	}
}


//-----------------------------------------------------------------------------
// Reduces the active occlusion edge list to the bare minimum set of edges
//-----------------------------------------------------------------------------
void CEdgeList::IntroduceNewActiveEdges( float y )
{
	int nEdgeCount = ActualEdgeCount();
	if ( m_nCurrentEdgeIndex == nEdgeCount )
		return;

	Edge_t *pCurEdge = &EdgeFromSortIndex( m_nCurrentEdgeIndex );

	// Add new edges, computing the x + z coordinates at the requested y value
	while ( pCurEdge->m_vecPosition.y <= y )
	{
		// This is necessary because of our initial skip up to y == -1.0f
		if (pCurEdge->m_vecPositionEnd.y > y)
		{
			float flDy = y - pCurEdge->m_vecPosition.y; 
			pCurEdge->m_flX = pCurEdge->m_vecPosition.x + flDy * pCurEdge->m_flDxDy;
			if ( pCurEdge->m_vecPositionEnd.y < m_flNextDiscontinuity )
			{
				m_flNextDiscontinuity = pCurEdge->m_vecPositionEnd.y;
			}

			// Now re-insert in the list, sorted by X
			InsertActiveEdge( LastActiveEdge(), pCurEdge );
		}

		if ( ++m_nCurrentEdgeIndex == nEdgeCount )
			return;

		pCurEdge = &EdgeFromSortIndex( m_nCurrentEdgeIndex );
	}

	// The next edge in y will also present a discontinuity
	if ( pCurEdge->m_vecPosition.y < m_flNextDiscontinuity )
	{
		m_flNextDiscontinuity = pCurEdge->m_vecPosition.y;
	}
}


//-----------------------------------------------------------------------------
// Reduces the active edge list into a subset of ones we truly care about
//-----------------------------------------------------------------------------
void CEdgeList::ReduceActiveEdgeList( CWingedEdgeList &wingedEdgeList, float flMinY, float flMaxY )
{
	// Surface lists should be empty
	int i;

#ifdef DEBUG_OCCLUSION_SYSTEM
	for ( i = m_Surfaces.Count(); --i >= 0; )
	{
		Assert( m_Surfaces[i].m_pNextSurface == NULL );
	}
#endif

	int nLeaveSurfID = -1;
	const Edge_t *pCurEdge = FirstActiveEdge();
	const Edge_t *pNextEdge;

	// NOTE: This algorithm depends on the fact that the active edge
	// list is not only sorted by ascending X, but also because edges
	// that land on the same X value are sorted by ascending dy/dx
	float flPrevX = pCurEdge->m_flX; 

	for ( ; !AtListEnd( pCurEdge ); pCurEdge = pNextEdge ) 
	{
		if ( pCurEdge->m_flX != flPrevX )
		{
			UpdateCurrentSurfaceZValues( pCurEdge->m_flX, flMinY );
		}

		IntroduceSingleActiveEdge( pCurEdge, flMinY );

		flPrevX = pCurEdge->m_flX;

		// If we have coincident edges, we have to introduce them at the same time...
		pNextEdge = pCurEdge->m_pNextActiveEdge;
		if ( (flPrevX == pNextEdge->m_flX) && (pCurEdge->m_flDxDy == pNextEdge->m_flDxDy) )
			continue;

		// If there's more than one overlapping surface at this point,
		// we can eliminate some edges.
		int nEnterSurfID = TopSurface()->m_nSurfID;

		// No change in the top surface? No edges needed...
		if ( nLeaveSurfID == nEnterSurfID )
			continue;

		Assert( ( nLeaveSurfID != -1 ) || ( nEnterSurfID != -1 )  );

		int nEdgeSurfID = ( nEnterSurfID != -1 ) ? nEnterSurfID : nLeaveSurfID;

		// Seam up edges...
		for ( i = m_nPrevReduceCount; --i >= 0; )
		{
			CWingedEdgeList::WingedEdge_t &testEdge = wingedEdgeList.WingedEdge( m_pPrevReduceInfo[i].m_nWingedEdge );
			if (( testEdge.m_nLeaveSurfID != nLeaveSurfID ) || ( testEdge.m_nEnterSurfID != nEnterSurfID ))
				continue;

			if ( ( testEdge.m_flDxDy != pCurEdge->m_flDxDy) || ( fabs( testEdge.m_vecPositionEnd.x - pCurEdge->m_flX ) >= 1e-3 ) )
				continue;

			ComputePointAlongEdge( m_pPrevReduceInfo[i].m_pEdge, nEdgeSurfID, flMaxY, &testEdge.m_vecPositionEnd );
			
			// Don't try to seam up edges that end on this line...
			if ( pCurEdge->m_vecPositionEnd.y > flMaxY )
			{
				ReduceInfo_t *pNewEdge = &m_pNewReduceInfo[ m_nNewReduceCount ];
				++m_nNewReduceCount;
				pNewEdge->m_pEdge = m_pPrevReduceInfo[i].m_pEdge;
				pNewEdge->m_nWingedEdge = m_pPrevReduceInfo[i].m_nWingedEdge;
			}
			break;
		}

		// This edge didn't exist on the previous y discontinuity line
		// We'll need to make a new one
		if ( i < 0 )
		{
			i = wingedEdgeList.AddEdge();
			CWingedEdgeList::WingedEdge_t &newWingedEdge = wingedEdgeList.WingedEdge(i);
			newWingedEdge.m_nLeaveSurfID = nLeaveSurfID;
			newWingedEdge.m_nEnterSurfID = nEnterSurfID;
			newWingedEdge.m_flDxDy = pCurEdge->m_flDxDy;
			ComputePointAlongEdge( pCurEdge, nEdgeSurfID, flMinY, &newWingedEdge.m_vecPosition ); 
			ComputePointAlongEdge( pCurEdge, nEdgeSurfID, flMaxY, &newWingedEdge.m_vecPositionEnd ); 
			
			// Enforce sort order...
			// Required because we're computing the x position here, which can introduce error.
			if ( i != 0 )
			{
				CWingedEdgeList::WingedEdge_t &prevWingedEdge = wingedEdgeList.WingedEdge(i - 1);
				if ( newWingedEdge.m_vecPosition.y == prevWingedEdge.m_vecPosition.y )
				{
					if ( newWingedEdge.m_vecPosition.x < prevWingedEdge.m_vecPosition.x ) 
					{
						newWingedEdge.m_vecPosition.x = prevWingedEdge.m_vecPosition.x;
					}
				}
			}

			// Don't try to seam up edges that end on this line...
			if ( pCurEdge->m_vecPositionEnd.y > flMaxY )
			{
				ReduceInfo_t *pNewEdge = &m_pNewReduceInfo[ m_nNewReduceCount ];
				++m_nNewReduceCount;
				pNewEdge->m_pEdge = pCurEdge;
				pNewEdge->m_nWingedEdge = i;
			}

#ifdef DEBUG_OCCLUSION_SYSTEM
			wingedEdgeList.CheckConsistency();
#endif
		}

		nLeaveSurfID = nEnterSurfID;
	}

	Assert( nLeaveSurfID == -1 );

//		Msg("\n");

}


//-----------------------------------------------------------------------------
// Discovers the first edge crossing discontinuity
//-----------------------------------------------------------------------------
float CEdgeList::LocateEdgeCrossingDiscontinuity( float flNextY, float flPrevY, int &nCount, Edge_t **ppInfo )
{
	nCount = 0;
	float flCurrX = -FLT_MAX;
	float flNextX = -FLT_MAX;
	float flCurrY = flNextY;

	Vector2D vecDelta, vecIntersection;
	
	Edge_t *pCurEdge;
	for ( pCurEdge = FirstActiveEdge(); !AtListEnd(pCurEdge); flCurrX = flNextX, pCurEdge = pCurEdge->m_pNextActiveEdge )
	{
		// Don't take into account edges that end on the current line
		Assert( pCurEdge->m_vecPositionEnd.y >= flCurrY );

		flNextX = pCurEdge->m_vecPosition.x + (flCurrY - pCurEdge->m_vecPosition.y) * pCurEdge->m_flDxDy;

		// Look for an X-crossing... This check helps for nearly co-linear lines
		// NOTE: You might think this would crash since it could dereference a NULL
		// pointer the first time through the loop, but it never hits that check since the
		// first X test is guaranteed to pass
		Edge_t *pPrevEdge = pCurEdge->m_pPrevActiveEdge;
		if ( ( flNextX > flCurrX ) || ( pPrevEdge->m_flDxDy <= pCurEdge->m_flDxDy ) )
			continue;

		// This test is necessary to not capture edges that meet at a point...
		if ( pPrevEdge->m_vecPositionEnd == pCurEdge->m_vecPositionEnd )
			continue;

		Assert( pPrevEdge->m_flDxDy != pCurEdge->m_flDxDy );

		// Found one! Let's find the intersection of these two
		// edges and up the Y discontinuity to that point.
		// We'll solve this by doing an intersection of point + plane in 2D...
		// For the line, we'll use the previous line where
		// P = Pop + D * t, Pop = prevEdge.m_vecPosition, D = [dx dy] = [(dx/dy) 1]
		// For the plane, we'll use the current line where
		// N * P = d
		// Normal is perpendicular to the line, therefore N = [-dy dx] = [-1 (dx/dy)]
		// d = DotProduct( N, edge.m_vecPosition ) = N dot Pon
		// So, the t that solve the equation is given by t = (d - N dot Pop) / (N dot D)
		// Or, t = (N dot Pon - N dot Pop) / (N dot D)
		// t = (N dot (Pon - Pop)) / (N dot D)

		float flDenominator = 1.0f / (-pPrevEdge->m_flDxDy + pCurEdge->m_flDxDy);
		Vector2DSubtract( pCurEdge->m_vecPosition.AsVector2D(), pPrevEdge->m_vecPosition.AsVector2D(), vecDelta );
		float flNumerator = - vecDelta.x + pCurEdge->m_flDxDy * vecDelta.y;
		float t = flNumerator * flDenominator;
		float flYCrossing = pPrevEdge->m_vecPosition.y + t;
		
		// Precision errors...
		// NOTE: The optimizer unfortunately causes this test to not return ==
		// if the bitpattern of flYCrossing and flNextY are the exact same, because it's
		// doing the test with the 80bit fp registers. flYCrossing is still sitting in the register
		// from the computation on the line above, but flNextY isn't. Therefore it returns not equal.
		// That's why I have to do the explicit bitpattern check.
		if ( ( flYCrossing >= flNextY ) || ( *(int*)&flYCrossing == *(int*)&flNextY ) )
			continue;

		if ( flYCrossing < flPrevY )
		{
			flYCrossing = flPrevY;
		}

		// If we advanced in Y, then reset the edge crossings
		if ( flCurrY != flYCrossing )
		{
			flCurrY	= flYCrossing;
			nCount = 0;
		}
		
		Assert( nCount < MAX_EDGE_CROSSINGS );
		flNextX = pPrevEdge->m_vecPosition.x + t * pPrevEdge->m_flDxDy;
		ppInfo[nCount++] = pCurEdge;
	}
	return flCurrY;
}


//-----------------------------------------------------------------------------
// Advances the X values of the active edge list, with no reordering
//-----------------------------------------------------------------------------
void CEdgeList::AdvanceActiveEdgeList( float flCurrY )
{
	m_flNextDiscontinuity = FLT_MAX;

	// Advance all edges until the current Y; we don't need to re-order *any* edges.
	Edge_t *pCurEdge;
	Edge_t *pNextEdge;
	float flPrevX = -FLT_MAX;
	for ( pCurEdge = FirstActiveEdge(); !AtListEnd( pCurEdge ); pCurEdge = pNextEdge )
	{
		pNextEdge = pCurEdge->m_pNextActiveEdge;

		if ( pCurEdge->m_vecPositionEnd.y <= flCurrY )
		{
			UnlinkActiveEdge( pCurEdge );
			continue;
		}

		pCurEdge->m_flX = pCurEdge->m_vecPosition.x + (flCurrY - pCurEdge->m_vecPosition.y) * pCurEdge->m_flDxDy;

		// Eliminate precision errors by guaranteeing sort ordering...
		if ( pCurEdge->m_flX < flPrevX )
		{
			pCurEdge->m_flX = flPrevX;
		}
		else
		{
			flPrevX = pCurEdge->m_flX;
		}

		if ( pCurEdge->m_vecPositionEnd.y < m_flNextDiscontinuity )
		{
			m_flNextDiscontinuity = pCurEdge->m_vecPositionEnd.y;
		}
	}
}


//-----------------------------------------------------------------------------
// Reorders the active edge list based on where edge crossings occur
//-----------------------------------------------------------------------------
void CEdgeList::ReorderActiveEdgeList( int nCount, Edge_t **ppCrossings )
{
	int nCurCrossing = 0;
	while ( nCurCrossing < nCount )
	{
		// Re-order the list where the edge crossing occurred.
		// For all edges that passed through the exact same point, we need only
		// reverse the order of those edges. At the same time, slam the X value of each
		// crossing edge to reduce precision errors

		Edge_t *pCurCrossing = ppCrossings[nCurCrossing++];
		Edge_t *pFirstCrossing = pCurCrossing->m_pPrevActiveEdge;

		// First, bring shared (or nearly shared) edges into the crossing list...
		while ( pFirstCrossing->m_pPrevActiveEdge->m_flX == pFirstCrossing->m_flX )
		{
			pFirstCrossing = pFirstCrossing->m_pPrevActiveEdge;
		}

		// Find the last crossing...
		Edge_t *pLastCrossing = pCurCrossing->m_pNextActiveEdge;
		Edge_t *pPrevCrossing = pCurCrossing;
		while ( true )
		{
			if ( (nCurCrossing < nCount) && (pLastCrossing == ppCrossings[nCurCrossing]) )
			{
				pPrevCrossing = pLastCrossing;
				pLastCrossing = pLastCrossing->m_pNextActiveEdge;
				++nCurCrossing;
				continue;
			}

			if ( pPrevCrossing->m_flX != pLastCrossing->m_flX )
				break;

			pLastCrossing = pLastCrossing->m_pNextActiveEdge;
		}

		// This should always be true, since there's always an edge at FLT_MAX.
		Assert( pLastCrossing );

		// Slam all x values to be the same to avoid precision errors...
		// This guarantees that this crossing at least will occur
		float flXCrossing = pFirstCrossing->m_flX;
		for ( Edge_t *pCrossing = pFirstCrossing->m_pNextActiveEdge; pCrossing != pLastCrossing; pCrossing = pCrossing->m_pNextActiveEdge )
		{
			pCrossing->m_flX = flXCrossing;
		}
	}

	// Now re-insert everything to take into account other edges which may well have 
	// crossed on this line
	Edge_t *pEdge;
	Edge_t *pNextEdge;
	for( pEdge = FirstActiveEdge(); !AtListEnd(pEdge); pEdge = pNextEdge )
	{
		pNextEdge = pEdge->m_pNextActiveEdge;
		Edge_t *pPrevEdge = pEdge->m_pPrevActiveEdge;
		if ( pPrevEdge->m_flX == pEdge->m_flX )
		{
			UnlinkActiveEdge( pEdge );
			InsertActiveEdge( pPrevEdge, pEdge );
		}
	}
}


//-----------------------------------------------------------------------------
// Reduces the active occlusion edge list to the bare minimum set of edges
//-----------------------------------------------------------------------------
void CEdgeList::SpewActiveEdgeList( float y, bool bHex)
{
	Edge_t *pEdge = FirstActiveEdge();
	Msg( "%.3f : ", y );
	while ( !AtListEnd( pEdge ) )
	{
		if (!bHex)
		{
			Msg( "(%d %.3f [%d]) ", (int)(pEdge - m_Edges.Base()), pEdge->m_flX, pEdge->m_nSurfID );
		}
		else
		{
			Msg( "(%d %X [%d]) ", (int)(pEdge - m_Edges.Base()), *(int*)&pEdge->m_flX, pEdge->m_nSurfID );
		}
		pEdge = pEdge->m_pNextActiveEdge;
	}
	Msg( "\n" );
}


//-----------------------------------------------------------------------------
// Checks consistency of the edge list...
//-----------------------------------------------------------------------------
void CEdgeList::CheckConsistency()
{
	Edge_t *pEdge = FirstActiveEdge();
	while( !AtListEnd( pEdge ) )
	{
		Edge_t *pPrevEdge = pEdge->m_pPrevActiveEdge;
		Assert( pEdge->m_flX >= pPrevEdge->m_flX );
		if ( pEdge->m_flX == pPrevEdge->m_flX )
		{
			// End point check necessary because of precision errors
			Assert( (pEdge->m_flDxDy >= pPrevEdge->m_flDxDy) || (pEdge->m_vecPositionEnd == pPrevEdge->m_vecPositionEnd) );
		}

		pEdge = pEdge->m_pNextActiveEdge;
	}
}


//-----------------------------------------------------------------------------
// Reduces the active occlusion edge list to the bare minimum set of edges
//-----------------------------------------------------------------------------
void CEdgeList::ReduceActiveList( CWingedEdgeList &newEdgeList )
{
	int nEdgeCount = ActualEdgeCount();
	if ( nEdgeCount == 0 )
		return;

	// Copy the surfaces over
	int nCount = m_Surfaces.Count();
//	newEdgeList.m_Surfaces.EnsureCapacity( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		newEdgeList.AddSurface( m_Surfaces[i].m_Plane );
	}

	Edge_t *pEdgeCrossings[MAX_EDGE_CROSSINGS];
	ReduceInfo_t *pBuf[2];
	pBuf[0] = (ReduceInfo_t*)stackalloc( nEdgeCount * sizeof(ReduceInfo_t) );
	pBuf[1] = (ReduceInfo_t*)stackalloc( nEdgeCount * sizeof(ReduceInfo_t) );
	m_nPrevReduceCount = m_nNewReduceCount = 0;
	int nIndex = 0;

	ResetActiveEdgeList();
	ClearCurrentSurfaceList();

	// We can skip everything up to y = -1.0f; since that's offscreen
	float flPrevY = NextDiscontinuity();
	flPrevY = fpmax( -1.0f, flPrevY );

	m_flNextDiscontinuity = FLT_MAX;
	IntroduceNewActiveEdges( flPrevY );

	int nEdgeCrossingCount = 0;
	bool bDone = false;
	while( !bDone )
	{
		// Don't immediately progress to the next discontinuity if there are edge crossings.
		float flNextY = LocateEdgeCrossingDiscontinuity( NextDiscontinuity(), flPrevY, nEdgeCrossingCount, pEdgeCrossings );

#ifdef  DEBUG_OCCLUSION_SYSTEM
		if ( s_bSpew )
		{
			// Debugging spew
			SpewActiveEdgeList( flPrevY );
		}
#endif

		// Reduce the active edge list
		m_pNewReduceInfo = pBuf[1 - nIndex];
		m_pPrevReduceInfo = pBuf[nIndex];
		m_nPrevReduceCount = m_nNewReduceCount;
		m_nNewReduceCount = 0;

		// Add a small epsilon so we occlude things on the top edge at y = 1.0
		if (flNextY >= 1.001f)
		{
			flNextY = 1.001f;
			bDone = true;
		}

		ReduceActiveEdgeList( newEdgeList, flPrevY, flNextY );
		flPrevY = flNextY;

		// Advance the active edge list, with no resorting necessary!!
		AdvanceActiveEdgeList( flNextY );

		// If we had an edge crossing, re-order the edges. Otherwise introduce new active edges
		if ( !nEdgeCrossingCount )
		{
			IntroduceNewActiveEdges( flNextY );

			// Keep advancing the active edge list until it's got no more discontinuities
			if ( NextDiscontinuity() == FLT_MAX )
				return;
		}
		else
		{
			ReorderActiveEdgeList( nEdgeCrossingCount, pEdgeCrossings );
			
			// The next edge in y will also present a discontinuity
			if ( m_nCurrentEdgeIndex < nEdgeCount )
			{
				float flNextEdgeY = EdgeFromSortIndex( m_nCurrentEdgeIndex ).m_vecPosition.y;
				if ( flNextEdgeY < m_flNextDiscontinuity )
				{
					m_flNextDiscontinuity = flNextEdgeY;
				}
			}
		}

#ifdef DEBUG_OCCLUSION_SYSTEM
		CheckConsistency();
#endif

		nIndex = 1 - nIndex;
	}
}


//-----------------------------------------------------------------------------
// Used to debug the occlusion system
//-----------------------------------------------------------------------------
void CEdgeList::QueueVisualization( unsigned char *pColor )
{
#ifndef SWDS
	if ( !r_visocclusion.GetInt() )
		return;

	int nFirst = g_EdgeVisualization.AddMultipleToTail( m_Edges.Count() );
	for ( int i = m_Edges.Count(); --i >= 0; )
	{
		EdgeVisualizationInfo_t &info = g_EdgeVisualization[nFirst + i];
		info.m_vecPoint[0] = m_Edges[i].m_vecPosition;
		info.m_vecPoint[1] = m_Edges[i].m_vecPositionEnd;
		*(int*)(info.m_pColor) = *(int*)pColor;
	}
#endif
}


void CEdgeList::Visualize( unsigned char *pColor )
{
#ifndef SWDS
	if ( !r_visocclusion.GetInt() )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->Bind( g_pMaterialWireframeVertexColorIgnoreZ );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, m_Edges.Count() );

	int i;
	for ( i = m_Edges.Count(); --i >= 0; )
	{
		meshBuilder.Position3fv( m_Edges[i].m_vecPosition.Base() );
		meshBuilder.Color4ubv( pColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( m_Edges[i].m_vecPositionEnd.Base() );
#ifdef DEBUG_OCCLUSION_SYSTEM
		meshBuilder.Color4ub( 0, 0, 255, 255 );
#else
		meshBuilder.Color4ubv( pColor );
#endif
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
#endif
}


//-----------------------------------------------------------------------------
// Implementation of IOcclusionSystem
//-----------------------------------------------------------------------------
class COcclusionSystem : public IOcclusionSystem
{
public:
	COcclusionSystem();
	~COcclusionSystem();

	// Inherited from IOcclusionSystem
	virtual void ActivateOccluder( int nOccluderIndex, bool bActive );
	virtual void SetView( const Vector &vecCameraPos, float flFOV, const VMatrix &worldToCamera, const VMatrix &cameraToProjection, const VPlane &nearClipPlane );
	virtual bool IsOccluded( const Vector &vecAbsMins, const Vector &vecAbsMaxs );
	virtual void SetOcclusionParameters( float flMaxOccludeeArea, float flMinOccluderArea );
	virtual float MinOccluderArea() const;
	virtual void DrawDebugOverlays();

private:
	struct AxisAlignedPlane_t
	{
		int m_nAxis;
		float m_flSign;
		float m_flDist;
	};

	// Recomputes the edge list for occluders
	void RecomputeOccluderEdgeList();

	// Is the point inside the near plane?
	bool IsPointInsideNearPlane( const Vector &vecPos ) const;
	void IntersectWithNearPlane( const Vector &vecStart, const Vector &vecEnd, Vector &outPos ) const;

	// Clips a polygon to the near clip plane
	int ClipPolygonToNearPlane( Vector **ppVertices, int nVertexCount, Vector **ppOutVerts, bool *pClipped ) const;

	// Project world-space verts + add into the edge list
	void AddPolygonToEdgeList( CEdgeList &edgeList, Vector **ppPolygon, int nCount, int nSurfID, bool bClipped );

	// Computes the plane equation of a polygon in screen space from a camera-space plane
	void ComputeScreenSpacePlane( const cplane_t &cameraSpacePlane, cplane_t *pScreenSpacePlane );

	// Used to clip the screen space polygons to the screen
	void ResetClipTempVerts();
	int ClipPolygonToAxisAlignedPlane( Vector **ppVertices, int nVertexCount, 
								const AxisAlignedPlane_t &plane, Vector **ppOutVerts ) const;

	// Is the point within an axis-aligned plane?
	bool IsPointInsideAAPlane( const Vector &vecPos, const AxisAlignedPlane_t &plane ) const;
	void IntersectWithAAPlane( const Vector &vecStart, const Vector &vecEnd, const AxisAlignedPlane_t &plane, Vector &outPos ) const;

	// Stitches up clipped vertices
	void StitchClippedVertices( Vector *pVertices, int nCount );

private:
	// Per-frame information
	bool m_bEdgeListDirty;
	VMatrix m_WorldToProjection;
	VMatrix m_WorldToCamera;
	float m_flXProjScale;
	float m_flYProjScale;
	float m_flProjDistScale;
	float m_flProjDistOffset;
	Vector m_vecCameraPosition;		// in world space
	cplane_t m_NearClipPlane;
	float m_flNearPlaneDist;
	float m_flFOVFactor;
	CEdgeList m_EdgeList;
	CWingedEdgeList m_WingedEdgeList;
	CUtlVector< Vector > m_ClippedVerts;

	float m_flMaxOccludeeArea;
	float m_flMinOccluderArea;

	// Stats
	int m_nTests;
	int m_nOccluded;
};

static COcclusionSystem g_OcclusionSystem;



//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
IOcclusionSystem *OcclusionSystem()
{
	return &g_OcclusionSystem;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
COcclusionSystem::COcclusionSystem() : m_ClippedVerts( 0, 64 )
{
	m_bEdgeListDirty = false;
	m_nTests = 0;
	m_nOccluded = 0;
	m_flMinOccluderArea = DEFAULT_MIN_OCCLUDER_AREA;
	m_flMaxOccludeeArea = DEFAULT_MAX_OCCLUDEE_AREA;
}

COcclusionSystem::~COcclusionSystem()
{
}


//-----------------------------------------------------------------------------
// Occlusion parameters?
//-----------------------------------------------------------------------------
void COcclusionSystem::SetOcclusionParameters( float flMaxOccludeeArea, float flMinOccluderArea )
{
	m_flMaxOccludeeArea = (flMaxOccludeeArea ? flMaxOccludeeArea : DEFAULT_MAX_OCCLUDEE_AREA) * 0.01f;
	m_flMinOccluderArea = (flMinOccluderArea ? flMinOccluderArea : DEFAULT_MIN_OCCLUDER_AREA);
}

float COcclusionSystem::MinOccluderArea() const
{
	return m_flMinOccluderArea;
}


//-----------------------------------------------------------------------------
// Is the point within the near plane?
//-----------------------------------------------------------------------------
inline bool COcclusionSystem::IsPointInsideNearPlane( const Vector &vecPos ) const
{
	return DotProduct( vecPos, m_NearClipPlane.normal ) >= m_NearClipPlane.dist;
}

inline void COcclusionSystem::IntersectWithNearPlane( const Vector &vecStart, const Vector &vecEnd, Vector &outPos ) const
{
	Vector vecDir;
	VectorSubtract( vecEnd, vecStart, vecDir );
	float t = IntersectRayWithPlane( vecStart, vecDir, m_NearClipPlane.normal, m_NearClipPlane.dist );
	VectorLerp( vecStart, vecEnd, t, outPos );
}


//-----------------------------------------------------------------------------
// Clips a surface to the near clip plane
// FIXME: This blows: a *third* S-H clipper in the engine! All because the
// vertex formats are different owing to different goals of the 3 clippers
//-----------------------------------------------------------------------------
static Vector s_TempVertMemory[256];

int COcclusionSystem::ClipPolygonToNearPlane( Vector **ppVertices, int nVertexCount, Vector **ppOutVerts, bool *pClipped ) const
{
	*pClipped = false;

	if ( nVertexCount < 3 )
		return 0;

	// Ye Olde Sutherland-Hodgman clipping algorithm
	int nOutVertCount = 0;
	int nNewVertCount = 0;

	Vector* pStart = ppVertices[ nVertexCount - 1 ];
	bool bStartInside = IsPointInsideNearPlane( *pStart );
	for ( int i = 0; i < nVertexCount; ++i )
	{
		Vector* pEnd = ppVertices[ i ];
		bool bEndInside = IsPointInsideNearPlane( *pEnd );
		if (bEndInside)
		{
			if (!bStartInside)
			{
				// Started outside, ended inside, need to clip the edge
				ppOutVerts[nOutVertCount] = &s_TempVertMemory[ nNewVertCount++ ];
				IntersectWithNearPlane( *pStart, *pEnd, *ppOutVerts[nOutVertCount] ); 
				++nOutVertCount;
				*pClipped = true;
			}
			ppOutVerts[nOutVertCount++] = pEnd;
		}
		else
		{
			if (bStartInside)
			{
				// Started inside, ended outside, need to clip the edge
				ppOutVerts[nOutVertCount] = &s_TempVertMemory[ nNewVertCount++ ];
				IntersectWithNearPlane( *pStart, *pEnd, *ppOutVerts[nOutVertCount] ); 
				++nOutVertCount;
				*pClipped = true;
			}
		}
		pStart = pEnd;
		bStartInside = bEndInside;
	}

	return nOutVertCount;
}


//-----------------------------------------------------------------------------
// Is the point within an axis-aligned plane?
//-----------------------------------------------------------------------------
inline bool COcclusionSystem::IsPointInsideAAPlane( const Vector &vecPos, const AxisAlignedPlane_t &plane ) const
{
	return vecPos[plane.m_nAxis] * plane.m_flSign >= plane.m_flDist;
}

inline void COcclusionSystem::IntersectWithAAPlane( const Vector &vecStart, const Vector &vecEnd, const AxisAlignedPlane_t &plane, Vector &outPos ) const
{
	float t = IntersectRayWithAAPlane( vecStart, vecEnd, plane.m_nAxis, plane.m_flSign, plane.m_flDist );
	VectorLerp( vecStart, vecEnd, t, outPos );
}


//-----------------------------------------------------------------------------
// Clips a surface to the edges of the screen (axis-aligned planes)
//-----------------------------------------------------------------------------
static int s_nTempVertCount = 0;

void COcclusionSystem::ResetClipTempVerts()
{
	s_nTempVertCount = 0;
}

int COcclusionSystem::ClipPolygonToAxisAlignedPlane( Vector **ppVertices, int nVertexCount, 
							const AxisAlignedPlane_t &plane, Vector **ppOutVerts ) const
{
	// Ye Olde Sutherland-Hodgman clipping algorithm
	int nOutVertCount = 0;

	Vector* pStart = ppVertices[ nVertexCount - 1 ];
	bool bStartInside = IsPointInsideAAPlane( *pStart, plane );
	for ( int i = 0; i < nVertexCount; ++i )
	{
		Vector* pEnd = ppVertices[ i ];
		bool bEndInside = IsPointInsideAAPlane( *pEnd, plane );
		if (bEndInside)
		{
			if (!bStartInside)
			{
				// Started outside, ended inside, need to clip the edge
				ppOutVerts[nOutVertCount] = &s_TempVertMemory[ s_nTempVertCount++ ];
				IntersectWithAAPlane( *pStart, *pEnd, plane, *ppOutVerts[nOutVertCount] ); 
				++nOutVertCount;
			}
			ppOutVerts[nOutVertCount++] = pEnd;
		}
		else
		{
			if (bStartInside)
			{
				// Started inside, ended outside, need to clip the edge
				ppOutVerts[nOutVertCount] = &s_TempVertMemory[ s_nTempVertCount++ ];
				IntersectWithAAPlane( *pStart, *pEnd, plane, *ppOutVerts[nOutVertCount] ); 
				++nOutVertCount;
			}
		}
		pStart = pEnd;
		bStartInside = bEndInside;
	}

	return nOutVertCount;
}


//-----------------------------------------------------------------------------
// Computes the plane equation of a polygon in screen space from a world-space plane
//-----------------------------------------------------------------------------
void COcclusionSystem::ComputeScreenSpacePlane( const cplane_t &cameraSpacePlane, cplane_t *pScreenSpacePlane )
{
	// Here's how this is computed:
	// If the *camera* space plane is Ax+By+Cz = D, 
	// and xs = -(xf) * x/z, ys = -(yf) y/z, zs = - (zc + zf * ooz)
	// Then x = -xs * z / xf, y = -ys * z / yf, ooz = -(zs + zc) / zf
	// So - A * xs * z / xf - B * ys * z / yf + C * z = D
	// - A xs / xf - B ys / yf + C = D * ooz
	// (A/D) xs/xf + (B/D) ys/yf + ooz = (C/D)
	// (A/D) xs/xf + (B/D) ys/yf - (zs + zc) / zf = (C/D)
	// -(A/D) xs/xf - (B/D) ys/yf + (zs + zc) / zf = -(C/D)
	// -zf * (A/D) xs/xf - zf * (B/D) ys/yf + zs = -zf * (C/D) - zc
	// Let A' = -zf/xf*(A/D), B' = -zf/yf*(B/D), D' =  -zf * (C/D) - zc
	// A' xs + B' ys + zs = D'	is the screen space plane equation
	
	float ooD = (cameraSpacePlane.dist != 0) ? (1.0f / cameraSpacePlane.dist) : 0.0f;
	pScreenSpacePlane->normal.x = cameraSpacePlane.normal.x * ooD * m_flXProjScale;
	pScreenSpacePlane->normal.y = cameraSpacePlane.normal.y * ooD * m_flYProjScale;
	pScreenSpacePlane->normal.z = 1;
	pScreenSpacePlane->dist = cameraSpacePlane.normal.z * ooD * m_flProjDistScale + m_flProjDistOffset;
}



//-----------------------------------------------------------------------------
// Stitches up clipped vertices
//-----------------------------------------------------------------------------
void COcclusionSystem::StitchClippedVertices( Vector *pVertices, int nCount )
{
	for ( int i = 0; i < nCount; ++i )
	{
		// Only stitch ones that have been clipped by the near clip plane
		if ( fabs( pVertices[i].z ) > 1e-3 )
			continue;

		int j;
		for ( j = m_ClippedVerts.Count(); --j >= 0; )
		{
			if ( VectorsAreEqual( pVertices[i], m_ClippedVerts[j], 1e-3 ) )
			{
				pVertices[i] = m_ClippedVerts[j];
				break;
			}
		}

		if ( j < 0 )
		{
			MEM_ALLOC_CREDIT();
			// No match found...
			m_ClippedVerts.AddToTail( pVertices[i] );
		}
	}
}

	
//-----------------------------------------------------------------------------
// Project world-space verts + add into the edge list
//-----------------------------------------------------------------------------
void COcclusionSystem::AddPolygonToEdgeList( CEdgeList &edgeList, Vector **ppPolygon, int nCount, int nSurfID, bool bClipped )
{
	// Transform the verts into projection space
	// Transform into projection space (extra logic here is to simply guarantee that we project each vert exactly once)
	int nMaxClipVerts = (nCount * 4);
	int nClipCount, nClipCount1;
	Vector **ppClipVertex = (Vector**)stackalloc( nMaxClipVerts * sizeof(Vector*) );
	Vector **ppClipVertex1 = (Vector**)stackalloc( nMaxClipVerts * sizeof(Vector*) );
	Vector *pVecProjectedVertex = (Vector*)stackalloc( nCount * sizeof(Vector) );

	int k;
	for ( k = 0; k < nCount; ++k )
	{
		Vector3DMultiplyPositionProjective( m_WorldToProjection, *(ppPolygon[k]), pVecProjectedVertex[k] );

		// Clamp needed to avoid precision problems.
//		if ( pVecProjectedVertex[k].z < 0.0f )
//			pVecProjectedVertex[k].z = 0.0f;
		pVecProjectedVertex[k].z *= (pVecProjectedVertex[k].z > 0.0f);
		ppClipVertex[k] = &pVecProjectedVertex[k];
	}

	// Clip vertices to the screen in x,y...
	AxisAlignedPlane_t aaPlane;
	aaPlane.m_nAxis = 0;
	aaPlane.m_flDist = -1;
	aaPlane.m_flSign = -1;
	nClipCount = nCount;

	ResetClipTempVerts();

	nClipCount1 = ClipPolygonToAxisAlignedPlane( ppClipVertex, nClipCount, aaPlane, ppClipVertex1 );
	if ( nClipCount1 < 3 )
		return;
	Assert( nClipCount1 < nMaxClipVerts );

	aaPlane.m_flSign = 1;
	nClipCount = ClipPolygonToAxisAlignedPlane( ppClipVertex1, nClipCount1, aaPlane, ppClipVertex );
	if ( nClipCount < 3 )
		return;
	Assert( nClipCount < nMaxClipVerts );

	aaPlane.m_nAxis = 1;
	nClipCount1 = ClipPolygonToAxisAlignedPlane( ppClipVertex, nClipCount, aaPlane, ppClipVertex1 );
	if ( nClipCount1 < 3 )
		return;
	Assert( nClipCount1 < nMaxClipVerts );

	aaPlane.m_flSign = -1;
	nClipCount = ClipPolygonToAxisAlignedPlane( ppClipVertex1, nClipCount1, aaPlane, ppClipVertex );
	if ( nClipCount < 3 )
		return;
	Assert( nClipCount < nMaxClipVerts );

	// Compute the screen area...
	float flScreenArea = 0.0f;
	int nLastClipVert = nClipCount - 1;
	for ( k = 1; k < nLastClipVert; ++k )
	{
		// Using area times two simply because it's faster...
		float flTriArea = TriArea2DTimesTwo( (*ppClipVertex[0]), (*ppClipVertex[k]), (*ppClipVertex[k+1]) );
		Assert( flTriArea <= 1e-3 );
		if ( flTriArea < 0 )
		{
			flScreenArea += -flTriArea;
		}
	}
	edgeList.SetSurfaceArea( nSurfID, flScreenArea );

	// If there's a clipped vertex, attempt to seam up with other edges...
	if ( bClipped )
	{
		StitchClippedVertices( pVecProjectedVertex, nCount );
	}

	// Add in the edges of the *unclipped* polygon: to avoid precision errors
	Vector *ppEdgeVertices[2];
	int nLastVert = nCount - 1;
	ppEdgeVertices[ 1 ] = &pVecProjectedVertex[ nLastVert ];
	for ( k = 0; k < nLastVert; ++k )
	{
		ppEdgeVertices[ k & 0x1 ] = &pVecProjectedVertex[ k ];
		edgeList.AddEdge( ppEdgeVertices, nSurfID );
	}
	ppEdgeVertices[ nLastVert & 0x1 ] = &pVecProjectedVertex[ nLastVert ];
	edgeList.AddEdge( ppEdgeVertices, nSurfID );
}


//-----------------------------------------------------------------------------
// Recomputes the occluder edge list
//-----------------------------------------------------------------------------
void COcclusionSystem::RecomputeOccluderEdgeList()
{
	if ( !m_bEdgeListDirty )
		return;

	// Tracker 17772:  If building cubemaps can end up calling into here w/o cl.pAreaBits setup yet, oh well.
	if ( !cl.m_bAreaBitsValid && CommandLine()->FindParm( "-buildcubemaps" ) )
		return;
	 
	m_bEdgeListDirty = false;
	m_EdgeList.RemoveAll();
	m_WingedEdgeList.Clear();
	m_ClippedVerts.RemoveAll();

	mvertex_t *pVertices = host_state.worldbrush->vertexes;
	int *pIndices = host_state.worldbrush->occludervertindices;
	doccluderdata_t *pOccluders = host_state.worldbrush->occluders;

	int i, j, k;
	for ( i = host_state.worldbrush->numoccluders ; --i >= 0; )
	{
		if ( pOccluders[i].flags & OCCLUDER_FLAGS_INACTIVE )
			continue;

		// Skip the occluder if it's in a disconnected area
		if ( cl.m_chAreaBits &&
			(cl.m_chAreaBits[pOccluders[i].area >> 3] & (1 << ( pOccluders[i].area & 0x7 )) ) == 0 )
			continue;

		int nSurfID = pOccluders[i].firstpoly;
		int nSurfCount = pOccluders[i].polycount;
		for ( j = 0; j < nSurfCount; ++j, ++nSurfID )
		{
			doccluderpolydata_t *pSurf = &host_state.worldbrush->occluderpolys[nSurfID];

			int nFirstVertexIndex = pSurf->firstvertexindex;
			int nVertexCount = pSurf->vertexcount;

			// If the surface is backfacing, blow it off...
			const cplane_t &surfPlane = host_state.worldbrush->planes[ pSurf->planenum ];
			if ( DotProduct( surfPlane.normal, m_vecCameraPosition ) <= surfPlane.dist )
				continue;

			// Clip to the near plane (has to be done in world space)
			Vector **ppSurfVerts = (Vector**)stackalloc( ( nVertexCount ) * sizeof(Vector*) );
			Vector **ppClipVerts = (Vector**)stackalloc( ( nVertexCount * 2 ) * sizeof(Vector*) );
			for ( k = 0; k < nVertexCount; ++k )
			{
				int nVertIndex = pIndices[nFirstVertexIndex + k];
				ppSurfVerts[k] = &( pVertices[nVertIndex].position );
			}

			bool bClipped;
			int nClipCount = ClipPolygonToNearPlane( ppSurfVerts, nVertexCount, ppClipVerts, &bClipped );
			Assert( nClipCount <= ( nVertexCount * 2 ) );
			if ( nClipCount < 3 )
				continue;

			cplane_t projectionSpacePlane;
			cplane_t cameraSpacePlane;
			MatrixTransformPlane( m_WorldToCamera, surfPlane, cameraSpacePlane );
			ComputeScreenSpacePlane( cameraSpacePlane, &projectionSpacePlane ); 
			int nEdgeSurfID = m_EdgeList.AddSurface( projectionSpacePlane );

			// Transform into projection space (extra logic here is to simply guarantee that we project each vert exactly once)
			AddPolygonToEdgeList( m_EdgeList, ppClipVerts, nClipCount, nEdgeSurfID, bClipped );
		}
	}

	m_EdgeList.CullSmallOccluders();
	m_EdgeList.ReduceActiveList( m_WingedEdgeList ); 
//	Msg("Edge count %d -> %d\n", m_EdgeList.EdgeCount(), m_WingedEdgeList.EdgeCount() );

	// Draw the occluders
	unsigned char color[4] = { 255, 255, 255, 255 };
	m_WingedEdgeList.QueueVisualization( color );
}


//-----------------------------------------------------------------------------
// Occluder list management
//-----------------------------------------------------------------------------
void COcclusionSystem::ActivateOccluder( int nOccluderIndex, bool bActive )
{
	if ( ( nOccluderIndex >= host_state.worldbrush->numoccluders ) || ( nOccluderIndex < 0 ) )
		return;

	if ( bActive )
	{
		host_state.worldbrush->occluders[nOccluderIndex].flags &= ~OCCLUDER_FLAGS_INACTIVE;
	}
	else
	{
		host_state.worldbrush->occluders[nOccluderIndex].flags |= OCCLUDER_FLAGS_INACTIVE;
	}

	m_bEdgeListDirty = true;
}


void COcclusionSystem::SetView( const Vector &vecCameraPos, float flFOV, const VMatrix &worldToCamera, 
			const VMatrix &cameraToProjection, const VPlane &nearClipPlane )
{
	m_vecCameraPosition = vecCameraPos;
	m_WorldToCamera = worldToCamera;

	// See ComputeScreenSpacePlane() for the use of these constants
	m_flXProjScale = -cameraToProjection[2][3] / cameraToProjection[0][0];
	m_flYProjScale = -cameraToProjection[2][3] / cameraToProjection[1][1];
	m_flProjDistScale = -cameraToProjection[2][3];
	m_flProjDistOffset = -cameraToProjection[2][2];
	MatrixMultiply( cameraToProjection, worldToCamera, m_WorldToProjection );
	m_NearClipPlane.normal = nearClipPlane.m_Normal;
	m_NearClipPlane.dist = nearClipPlane.m_Dist;
	m_NearClipPlane.type = 3;
	m_bEdgeListDirty = true;
	m_flNearPlaneDist = -( DotProduct( vecCameraPos, m_NearClipPlane.normal ) - m_NearClipPlane.dist );
	Assert( m_flNearPlaneDist > 0.0f );
	m_flFOVFactor = m_flNearPlaneDist * tan( flFOV * 0.5f * M_PI / 180.0f );
	m_flFOVFactor = m_flNearPlaneDist / m_flFOVFactor;
	m_flFOVFactor *= m_flFOVFactor;

	if ( r_occlusionspew.GetInt() )
	{
		if ( m_nTests )
		{
			float flPercent = 100.0f * ((float)m_nOccluded / (float)m_nTests);
			Msg("Occl %.2f (%d/%d)\n", flPercent, m_nOccluded, m_nTests );
			m_nTests = 0;
			m_nOccluded = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Used to build the quads to test for occlusion
//-----------------------------------------------------------------------------
static int s_pFaceIndices[6][4] =
{
	{ 0, 4, 6, 2 },		// -x
	{ 1, 3, 7, 5 },		// +x
	{ 0, 1, 5, 4 },		// -y
	{ 2, 6, 7, 3 },		// +y
	{ 0, 2, 3, 1 },		// -z
	{ 4, 5, 7, 6 },		// +z
};

static int s_pSourceIndices[8] = 
{
	-1, 0, 0, 1, 0, 1, 2, 3  
};

static int s_pDeltaIndices[8] = 
{
	-1, 0, 1, 1, 2, 2, 2, 2
};

static unsigned char s_VisualizationColor[2][4] = 
{ 
	{ 255, 0, 0, 255 }, 
	{ 0, 255, 0, 255 } 
};

struct EdgeInfo_t
{
	unsigned char m_nVert[2];
	unsigned char m_nFace[2];
	int m_nTestCount;
	int m_nMinVert;
};

// NOTE: The face indices here have to very carefully ordered for the algorithm
// to work. They must be ordered so that vert0 -> vert1 is clockwise
// for the first face listed and vert1 -> vert0 is clockwise for the 2nd face listed
static EdgeInfo_t s_pEdges[12] = 
{
	{ { 0, 1 }, { 2, 4 }, 0, 0 },		// 0: Edge between -y + -z
	{ { 2, 0 }, { 0, 4 }, 0, 0 },		// 1: Edge between -x + -z
	{ { 1, 3 }, { 1, 4 }, 0, 0 },		// 2: Edge between +x + -z
	{ { 3, 2 }, { 3, 4 }, 0, 0 },		// 3: Edge between +y + -z
	{ { 0, 4 }, { 0, 2 }, 0, 0 },		// 4: Edge between -x + -y
	{ { 5, 1 }, { 1, 2 }, 0, 0 },		// 5: Edge between +x + -y
	{ { 6, 2 }, { 0, 3 }, 0, 0 },		// 6: Edge between -x + +y
	{ { 3, 7 }, { 1, 3 }, 0, 0 },		// 7: Edge between +x + +y
	{ { 5, 4 }, { 2, 5 }, 0, 0 },		// 8: Edge between -y + +z
	{ { 4, 6 }, { 0, 5 }, 0, 0 },		// 9: Edge between -x + +z
	{ { 7, 5 }, { 1, 5 }, 0, 0 },		// 10:Edge between +x + +z
	{ { 6, 7 }, { 3, 5 }, 0, 0 },		// 11:Edge between +y + +z
};

static int s_pFaceEdges[6][4] = 
{
	{ 4, 9, 6, 1 },
	{ 2, 7, 10, 5 },
	{ 0, 5, 8, 4 },
	{ 6, 11, 7, 3 },
	{ 1, 3, 2, 0 },
	{ 8, 10, 11, 9 },
};


//-----------------------------------------------------------------------------
// Occlusion checks
//-----------------------------------------------------------------------------
static CWingedEdgeList s_WingedTestEdgeList;

class WingedEdgeLessFunc
{
public:
	bool Less( const int& src1, const int& src2, void *pCtx )
	{
		Vector *pVertices = (Vector*)pCtx;

		EdgeInfo_t *pEdge1 = &s_pEdges[ src1 ];
		EdgeInfo_t *pEdge2 = &s_pEdges[ src2 ];

		Vector *pV1 = &pVertices[ pEdge1->m_nVert[ pEdge1->m_nMinVert ] ];
		Vector *pV2 = &pVertices[ pEdge2->m_nVert[ pEdge2->m_nMinVert ] ];

		if (pV1->y < pV2->y)
			return true;
		if (pV1->y > pV2->y)
			return false;
		if (pV1->x < pV2->x)
			return true;
		if (pV1->x > pV2->x)
			return false;

		// This is the same as the following line:
	//	return (pEdge1->m_flDxDy <= pEdge2->m_flDxDy);
		Vector2D dEdge1, dEdge2;
		Vector2DSubtract( pVertices[ pEdge1->m_nVert[ 1 - pEdge1->m_nMinVert ] ].AsVector2D(), pV1->AsVector2D(), dEdge1 );
		Vector2DSubtract( pVertices[ pEdge2->m_nVert[ 1 - pEdge2->m_nMinVert ] ].AsVector2D(), pV2->AsVector2D(), dEdge2 );
		Assert( dEdge1.y >= 0.0f );
		Assert( dEdge2.y >= 0.0f );

		return dEdge1.x * dEdge2.y <= dEdge1.y * dEdge2.x;
	}
};

bool COcclusionSystem::IsOccluded( const Vector &vecAbsMins, const Vector &vecAbsMaxs )
{
	if ( r_occlusion.GetInt() == 0 )
		return false;

	VPROF_BUDGET( "COcclusionSystem::IsOccluded", VPROF_BUDGETGROUP_OCCLUSION );

	// @MULTICORE (toml 9/11/2006): need to eliminate this mutex
	static CThreadFastMutex mutex;
	AUTO_LOCK( mutex );

	RecomputeOccluderEdgeList();

	// No occluders? Then the edge list isn't occluded
	if ( m_WingedEdgeList.EdgeCount() == 0 )
		return false;

	// Don't occlude things that have large screen area
	// Use a super cheap but inaccurate screen area computation
	Vector vecCenter;
	VectorAdd( vecAbsMaxs, vecAbsMins, vecCenter );
	vecCenter *= 0.5f;

	vecCenter -= m_vecCameraPosition;
	float flDist = DotProduct( m_NearClipPlane.normal, vecCenter );
	if (flDist <= 0.0f)
		return false;

	flDist += m_flNearPlaneDist;

	Vector vecSize;
	VectorSubtract( vecAbsMaxs, vecAbsMins, vecSize );
	float flRadiusSq = DotProduct( vecSize, vecSize ) * 0.25f;

	float flScreenArea = m_flFOVFactor * flRadiusSq / (flDist * flDist);
	float flMaxSize = r_occludeemaxarea.GetFloat() * 0.01f;
	if ( flMaxSize == 0.0f )
	{
		flMaxSize = m_flMaxOccludeeArea;
	}
	if (flScreenArea >= flMaxSize)
		return false;

	// Clear out its state
	s_WingedTestEdgeList.Clear();

	// NOTE: This assumes that frustum culling has already occurred on this object
	// If that were not the case, we'd need to add a little extra into this 
	// (probably a single plane test, which tests if the box is wholly behind the camera )

	// Convert the bbox into a max of 3 quads...
	const Vector *pCornerVert[2] = { &vecAbsMins, &vecAbsMaxs };

	// Compute the 8 box verts, and transform them into projective space...
	// NOTE: We'd want to project them *after* the plane test if there were
	// no frustum culling.
	int i;
	Vector pVecProjectedVertex[8];

	// NOTE: The code immediately below is an optimized version of this loop
	// The optimization takes advantage of the fact that the verts are all
	// axis aligned.
//	Vector vecBoxVertex;
//	for ( i = 0; i < 8; ++i )
//	{
//		vecBoxVertex.x = pCornerVert[ (i & 0x1) ]->x;
//		vecBoxVertex.y = pCornerVert[ (i & 0x2) >> 1 ]->y;
//		vecBoxVertex.z = pCornerVert[ (i & 0x4) >> 2 ]->z;
//		Vector3DMultiplyPositionProjective( m_WorldToProjection, vecBoxVertex, pVecProjectedVertex[ i ] );
//		if ( pVecProjectedVertex[ i ].z <= 0.0f )
//			return false;
//	}

	Vector4D vecProjVert[8];
	Vector4D vecDeltaProj[3];
	Vector4D vecAbsMins4D( vecAbsMins.x, vecAbsMins.y, vecAbsMins.z, 1.0f );
	Vector4DMultiply( m_WorldToProjection, vecAbsMins4D, vecProjVert[0] );
	if ( vecProjVert[0].w <= 0.0f )
		return false;
	float flOOW = 1.0f / vecProjVert[0].w;

	vecDeltaProj[0].Init( vecSize.x * m_WorldToProjection[0][0], vecSize.x * m_WorldToProjection[1][0],	vecSize.x * m_WorldToProjection[2][0], vecSize.x * m_WorldToProjection[3][0] );
	vecDeltaProj[1].Init( vecSize.y * m_WorldToProjection[0][1], vecSize.y * m_WorldToProjection[1][1],	vecSize.y * m_WorldToProjection[2][1], vecSize.y * m_WorldToProjection[3][1] );
	vecDeltaProj[2].Init( vecSize.z * m_WorldToProjection[0][2], vecSize.z * m_WorldToProjection[1][2],	vecSize.z * m_WorldToProjection[2][2], vecSize.z * m_WorldToProjection[3][2] );

	pVecProjectedVertex[0].Init( vecProjVert[0].x * flOOW, vecProjVert[0].y * flOOW, vecProjVert[0].z * flOOW ); 
	if ( pVecProjectedVertex[0].z <= 0.0f )
		return false;

	for ( i = 1; i < 8; ++i )
	{
		int nIndex = s_pSourceIndices[i];
		int nDelta = s_pDeltaIndices[i];
		Vector4DAdd( vecProjVert[nIndex], vecDeltaProj[nDelta], vecProjVert[i] );
		if ( vecProjVert[ i ].w <= 0.0f )
			return false;
		flOOW = 1.0f / vecProjVert[i].w;
		pVecProjectedVertex[ i ].Init( vecProjVert[i].x * flOOW, vecProjVert[i].y * flOOW, vecProjVert[i].z * flOOW ); 
		if ( pVecProjectedVertex[ i ].z <= 0.0f )
			return false;
	}

	// Precompute stuff needed by the loop over faces below
	float pSign[2] = { -1, 1 };
	Vector vecDelta[2];
	VectorSubtract( *pCornerVert[0], m_vecCameraPosition, vecDelta[0] );
	VectorSubtract( m_vecCameraPosition, *pCornerVert[1], vecDelta[1] );

	// Determine which faces + edges are visible...
	++m_nTests;
	int pSurfInd[6];
	for ( i = 0; i < 6; ++i )
	{
		int nDim = ( i >> 1 );
		int nInd = i & 0x1;

		// Try to backface cull each of the 6 box faces
		if ( vecDelta[nInd][nDim] <= 0.0f )
		{
			pSurfInd[i] = -1;
			continue;
		}

		cplane_t cameraSpacePlane, projectionSpacePlane;
		float flSign = pSign[nInd];
		float flPlaneDist = (*pCornerVert[nInd])[ nDim ] * flSign;
		MatrixTransformAxisAlignedPlane( m_WorldToCamera, nDim, flSign, flPlaneDist, cameraSpacePlane );
		ComputeScreenSpacePlane( cameraSpacePlane, &projectionSpacePlane ); 
		int nSurfID = s_WingedTestEdgeList.AddSurface( projectionSpacePlane );
		pSurfInd[i] = nSurfID;

		// Mark edges as being used...
		int *pFaceEdges = s_pFaceEdges[i];
		s_pEdges[ pFaceEdges[0] ].m_nTestCount = m_nTests;
		s_pEdges[ pFaceEdges[1] ].m_nTestCount = m_nTests;
		s_pEdges[ pFaceEdges[2] ].m_nTestCount = m_nTests;
		s_pEdges[ pFaceEdges[3] ].m_nTestCount = m_nTests;
	}

	// Sort edges by minimum Y + dx/dy...
	int pEdgeSort[12];
	CUtlSortVector< int, WingedEdgeLessFunc > edgeSort( pEdgeSort, 12 );
	edgeSort.SetLessContext( pVecProjectedVertex );
	for ( i = 0; i < 12; ++i )
	{
		// Skip non-visible edges
		EdgeInfo_t *pEdge = &s_pEdges[i];
		if ( pEdge->m_nTestCount != m_nTests )
			continue;

		pEdge->m_nMinVert = ( pVecProjectedVertex[ pEdge->m_nVert[0] ].y >= pVecProjectedVertex[ pEdge->m_nVert[1] ].y );
		edgeSort.Insert( i );
	}

	// Now add them into the winged edge list, in sorted order...
	int nEdgeCount = edgeSort.Count();
	for ( i = 0; i < nEdgeCount; ++i )
	{
		EdgeInfo_t *pEdge = &s_pEdges[edgeSort[i]];

		// The enter + leave ids depend entirely on which edge is further up
		// This works because the edges listed in s_pEdges show the edges as they
		// would be visited in *clockwise* order
		const Vector &startVert = pVecProjectedVertex[pEdge->m_nVert[pEdge->m_nMinVert]];
		const Vector &endVert = pVecProjectedVertex[pEdge->m_nVert[1 - pEdge->m_nMinVert]];
		int nLeaveSurfID = pSurfInd[ pEdge->m_nFace[pEdge->m_nMinVert] ];
		int nEnterSurfID = pSurfInd[ pEdge->m_nFace[1 - pEdge->m_nMinVert] ];

		s_WingedTestEdgeList.AddEdge( startVert, endVert, nLeaveSurfID, nEnterSurfID );
	}

#ifdef DEBUG_OCCLUSION_SYSTEM
	s_WingedTestEdgeList.CheckConsistency();
#endif

	// Now let's see if this edge list is occluded or not..
	bool bOccluded = m_WingedEdgeList.IsOccludingEdgeList( s_WingedTestEdgeList );
	if (bOccluded)
	{
		++m_nOccluded;
	}

	s_WingedTestEdgeList.QueueVisualization( s_VisualizationColor[bOccluded] );

	return bOccluded;
}


//-----------------------------------------------------------------------------
// Used to debug the occlusion system
//-----------------------------------------------------------------------------
void VisualizeQueuedEdges( )
{
#ifndef SWDS
	if ( !g_EdgeVisualization.Count() )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->Bind( g_pMaterialWireframeVertexColorIgnoreZ );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, g_EdgeVisualization.Count() );

	int i;
	for ( i = g_EdgeVisualization.Count(); --i >= 0; )
	{
		EdgeVisualizationInfo_t &info = g_EdgeVisualization[i];

		meshBuilder.Position3fv( info.m_vecPoint[0].Base() );
		meshBuilder.Color4ubv( info.m_pColor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv( info.m_vecPoint[1].Base() );
#ifdef DEBUG_OCCLUSION_SYSTEM
		meshBuilder.Color4ub( 0, 0, 255, 255 );
#else
		meshBuilder.Color4ubv( info.m_pColor );
#endif
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	g_EdgeVisualization.RemoveAll();
#endif
}


//-----------------------------------------------------------------------------
// Render debugging overlay
//-----------------------------------------------------------------------------
void COcclusionSystem::DrawDebugOverlays()
{
	// Draw the occludees
	VisualizeQueuedEdges();
}
