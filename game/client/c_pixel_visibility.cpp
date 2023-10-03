//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "cbase.h"
#include "c_pixel_visibility.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "precache_register.h"
#include "view.h"
#include "viewrender.h"
#include "utlmultilist.h"
#include "vprof.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static void PixelvisDrawChanged( IConVar *pPixelvisVar, const char *pOld, float flOldValue );

ConVar r_pixelvisibility_partial( "r_pixelvisibility_partial", "1" );
ConVar r_dopixelvisibility( "r_dopixelvisibility", "1" );
ConVar r_drawpixelvisibility( "r_drawpixelvisibility", "0", 0, "Show the occlusion proxies", PixelvisDrawChanged );
ConVar r_pixelvisibility_spew( "r_pixelvisibility_spew", "0" );

extern ConVar building_cubemaps;

#ifndef _X360
const float MIN_PROXY_PIXELS = 5.0f;
#else
const float MIN_PROXY_PIXELS = 25.0f;
#endif


extern view_id_t CurrentViewID();

struct OcclusionHandleViewIDPair_t
{
	OcclusionQueryObjectHandle_t hOcclusionHandle;
	int iViewID;
	int iLastFrameRendered;
};

struct OcclusionQueryHiddenData_t
{
	COcclusionQuerySet *pOwner;
	CUtlVector<OcclusionHandleViewIDPair_t> occlusionHandles[MAX_SPLITSCREEN_PLAYERS];
};

static CUtlVector<OcclusionQueryHiddenData_t> s_OcclusionQueries;
static inline int FindQueryHandlePairIndex( OcclusionQueryHiddenData_t *pData, int iViewID, int iSplitScreenSlot )
{
	int iPairCount = pData->occlusionHandles[iSplitScreenSlot].Count();
	OcclusionHandleViewIDPair_t *pPairs = pData->occlusionHandles[iSplitScreenSlot].Base();
	
	for( int i = 0; i != iPairCount; ++i )
	{
		if( pPairs[i].iViewID == iViewID )
			return i;
	}

	return pData->occlusionHandles[iSplitScreenSlot].InvalidIndex();
}


float PixelVisibility_DrawProxy( IMatRenderContext *pRenderContext, OcclusionQueryObjectHandle_t queryHandle, Vector origin, float scale, float proxyAspect, IMaterial *pMaterial, bool screenspace )
{
	Vector point;

	// don't expand this with distance to fit pixels or the sprite will poke through
	// only expand the parts perpendicular to the view
	float forwardScale = scale;
	// draw a pyramid of points touching a sphere of radius "scale" at origin
	float pixelsPerUnit = pRenderContext->ComputePixelDiameterOfSphere( origin, 1.0f );
	pixelsPerUnit = MAX( pixelsPerUnit, 1e-4f );
	if ( screenspace )
	{
		// Force this to be the size of a sphere of diameter "scale" at some reference distance (1.0 unit)
		float pixelsPerUnit2 = pRenderContext->ComputePixelDiameterOfSphere( CurrentViewOrigin() + CurrentViewForward()*1.0f, scale*0.5f );
		// force drawing of "scale" pixels
		scale = pixelsPerUnit2 / pixelsPerUnit;
	}
	else
	{
		float pixels = scale * pixelsPerUnit;
		
		// make the radius larger to ensure a minimum screen space size of the proxy geometry
		if ( pixels < MIN_PROXY_PIXELS )
		{
			scale = MIN_PROXY_PIXELS / pixelsPerUnit;
		}
	}

	// collapses the pyramid to a plane - so this could be a quad instead
	Vector dir = origin - CurrentViewOrigin();
	VectorNormalize(dir);
	origin -= dir * forwardScale;
	forwardScale = 0.0f;
	// 

	Vector verts[5];
	const float sqrt2 = 0.707106781f; // sqrt(2) - keeps all vectors the same length from origin
	scale *= sqrt2;
	float scale45x = scale;
	float scale45y = scale / proxyAspect;
	verts[0] = origin - CurrentViewForward() * forwardScale;					  // the apex of the pyramid
	verts[1] = origin + CurrentViewUp() * scale45y - CurrentViewRight() * scale45x; // these four form the base
	verts[2] = origin + CurrentViewUp() * scale45y + CurrentViewRight() * scale45x; // the pyramid is a sprite with a point that
	verts[3] = origin - CurrentViewUp() * scale45y + CurrentViewRight() * scale45x; // pokes back toward the camera through any nearby 
	verts[4] = origin - CurrentViewUp() * scale45y - CurrentViewRight() * scale45x; // geometry

	// get screen coords of edges
	Vector screen[4];
	for ( int i = 0; i < 4; i++ )
	{
		extern int ScreenTransform( const Vector& point, Vector& screen );
		if ( ScreenTransform( verts[i+1], screen[i] ) )
			return -1;
	}

	// compute area and screen-clipped area
	float w = screen[1].x - screen[0].x;
	float h = screen[0].y - screen[3].y;
	float ws = MIN(1.0f, screen[1].x) - MAX(-1.0f, screen[0].x);
	float hs = MIN(1.0f, screen[0].y) - MAX(-1.0f, screen[3].y);
	float area = w*h; // area can be zero when we ALT-TAB
	float areaClipped = ws*hs;
	float ratio = 0.0f;
	if ( area != 0 )
	{
		// compute the ratio of the area not clipped by the frustum to total area
		ratio = areaClipped / area;
		ratio = clamp(ratio, 0.0f, 1.0f);
	}

	pRenderContext->BeginOcclusionQueryDrawing( queryHandle );
	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( false, NULL, NULL, pMaterial );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 4 );
	// draw a pyramid
	for ( int i = 0; i < 4; i++ )
	{
		int a = i+1;
		int b = (a%4)+1;
		meshBuilder.Position3fv( verts[0].Base() );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3fv( verts[a].Base() );
		meshBuilder.AdvanceVertex();
		meshBuilder.Position3fv( verts[b].Base() );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End();
	pMesh->Draw();

	// sprite/quad proxy
#if 0
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	VectorMA (origin, -scale, CurrentViewUp(), point);
	VectorMA (point, -scale, CurrentViewRight(), point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	VectorMA (origin, scale, CurrentViewUp(), point);
	VectorMA (point, -scale, CurrentViewRight(), point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	VectorMA (origin, scale, CurrentViewUp(), point);
	VectorMA (point, scale, CurrentViewRight(), point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();

	VectorMA (origin, -scale, CurrentViewUp(), point);
	VectorMA (point, scale, CurrentViewRight(), point);
	meshBuilder.Position3fv (point.Base());
	meshBuilder.AdvanceVertex();
	
	meshBuilder.End();
	pMesh->Draw();
#endif
	pRenderContext->EndOcclusionQueryDrawing( queryHandle );

	// fraction clipped by frustum
	return ratio;
}

class CPixelVisSet
{
public:
	void Init( const pixelvis_queryparams_t &params );
	void MarkActive();
	bool IsActive();
	CPixelVisSet()
	{
		frameIssued = 0;
		serial = 0;
		queryList = 0xFFFF;
		sizeIsScreenSpace = false;
	}

public:
	float			proxySize;
	float			proxyAspect;
	float			fadeTimeInv;
	unsigned short	queryList;
	unsigned short	serial;
	bool			sizeIsScreenSpace;
private:
	int				frameIssued;
};


void CPixelVisSet::Init( const pixelvis_queryparams_t &params )
{
	Assert( params.bSetup );
	proxySize = params.proxySize;
	proxyAspect = params.proxyAspect;
	if ( params.fadeTime > 0.0f )
	{
		fadeTimeInv = 1.0f / params.fadeTime;
	}
	else
	{
		// fade in over 0.125 seconds
		fadeTimeInv = 1.0f / 0.125f;
	}
	frameIssued = 0;
	sizeIsScreenSpace = params.bSizeInScreenspace;
}

void CPixelVisSet::MarkActive()
{
	frameIssued = gpGlobals->framecount;
}

bool CPixelVisSet::IsActive()
{
	return (gpGlobals->framecount - frameIssued) > 1 ? false : true;
}

class CPixelVisibilityQuery
{
public:
	CPixelVisibilityQuery();
	~CPixelVisibilityQuery();
	bool IsValid();
	bool IsForView( int nPlayerSlot, int viewID );
	bool IsActive();
	float GetFractionVisible( float fadeTimeInv );
	void IssueQuery( IMatRenderContext *pRenderContext, float proxySize, float proxyAspect, IMaterial *pMaterial, bool sizeIsScreenSpace );
	void IssueCountingQuery( IMatRenderContext *pRenderContext, float proxySize, float proxyAspect, IMaterial *pMaterial, bool sizeIsScreenSpace );
	void ResetOcclusionQueries();
	void SetView( int nPlayerSlot, int viewID ) 
	{
		// This is necessary since player slot is stored in 2 bits
		COMPILE_TIME_ASSERT( MAX_SPLITSCREEN_PLAYERS <= 4 );

		Assert( nPlayerSlot >= 0 && nPlayerSlot < MAX_SPLITSCREEN_PLAYERS );
		m_viewID = viewID;	
		m_brightnessTarget = 0.0f;
		m_clipFraction = 1.0f;
		m_frameIssued = -1;
		m_failed = false;
		m_wasQueriedThisFrame = false;
		m_hasValidQueryResults = false;
		m_nPlayerSlot = nPlayerSlot;
	}

public:
	Vector							m_origin;
	int								m_frameIssued;
private:
	float							m_brightnessTarget;
	float							m_clipFraction;
	OcclusionQueryObjectHandle_t	m_queryHandle;
	OcclusionQueryObjectHandle_t	m_queryHandleCount;
	unsigned short					m_wasQueriedThisFrame : 1;
	unsigned short					m_failed : 1;
	unsigned short					m_hasValidQueryResults : 1;
	unsigned short					m_nPlayerSlot : 2;
	unsigned short					m_pad : 11;
	unsigned short					m_viewID;

	friend void PixelVisibility_ShiftVisibilityViews( int nPlayerSlot, int iSourceViewID, int iDestViewID ); //need direct access to private data to make shifting smooth
};

CPixelVisibilityQuery::CPixelVisibilityQuery()
{
	CMatRenderContextPtr pRenderContext( materials );
	SetView( 0, 0xFFFF );
	m_queryHandle = pRenderContext->CreateOcclusionQueryObject();
	m_queryHandleCount = pRenderContext->CreateOcclusionQueryObject();
}

CPixelVisibilityQuery::~CPixelVisibilityQuery()
{
	CMatRenderContextPtr pRenderContext( materials );
	if ( m_queryHandle != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
	{
		pRenderContext->DestroyOcclusionQueryObject( m_queryHandle );
	}
	if ( m_queryHandleCount != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
	{
		pRenderContext->DestroyOcclusionQueryObject( m_queryHandleCount );
	}
}

void CPixelVisibilityQuery::ResetOcclusionQueries()
{
	// NOTE: Since we're keeping the CPixelVisibilityQuery objects around in a pool
	// and not actually deleting them, this means that our material system occlusion queries are
	// not being deleted either. Which means that if a CPixelVisibilityQuery is 
	// put into the free list and then immediately re-used, then we have an opportunity for
	// a bug: What can happen on the first frame of the material system query
	// is that if the query isn't done yet, it will use the last queried value
	// which will happen to be set to the value of the last query done
	// for the previous CPixelVisSet the CPixelVisibilityQuery happened to be associated with
	// which makes queries have an invalid value for the first frame

	// This will mark the occlusion query objects as not ever having been read from before
	CMatRenderContextPtr pRenderContext( materials );
	if ( m_queryHandle != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
	{
		pRenderContext->ResetOcclusionQueryObject( m_queryHandle );
	}
	if ( m_queryHandleCount != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
	{
		pRenderContext->ResetOcclusionQueryObject( m_queryHandleCount );
	}
}

bool CPixelVisibilityQuery::IsValid()
{
	return (m_queryHandle != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE) ? true : false;
}

bool CPixelVisibilityQuery::IsForView( int nPlayerSlot, int viewID ) 
{ 
	return ( m_viewID == viewID ) && ( nPlayerSlot == m_nPlayerSlot ); 
}

bool CPixelVisibilityQuery::IsActive()
{
	return ( gpGlobals->framecount - m_frameIssued ) > 1 ? false : true;
}

float CPixelVisibilityQuery::GetFractionVisible( float fadeTimeInv )
{

	if ( !IsValid() )
		return 0.0f;

	if ( !m_wasQueriedThisFrame )
	{
		CMatRenderContextPtr pRenderContext( materials );
		m_wasQueriedThisFrame = true;
		int pixels = -1;
		int pixelsPossible = -1;
		if ( r_pixelvisibility_partial.GetBool() )
		{
			if ( m_frameIssued != -1 )
			{
				pixelsPossible = pRenderContext->OcclusionQuery_GetNumPixelsRendered( m_queryHandleCount );
				pixels = pRenderContext->OcclusionQuery_GetNumPixelsRendered( m_queryHandle );
			}

			if ( r_pixelvisibility_spew.GetBool() && CurrentViewID() == 0 ) 
			{
				DevMsg( 1, "Pixels visible: %d (qh:%d) Pixels possible: %d (qh:%d) (frame:%d)\n", pixels, m_queryHandle, pixelsPossible, m_queryHandleCount, gpGlobals->framecount );
			}

			if ( pixels < 0 || pixelsPossible < 0 )
			{
				m_failed = ( m_frameIssued >= 0 ) ? true : false;
				return m_brightnessTarget * m_clipFraction;
			}
			m_hasValidQueryResults = true;

			if ( pixelsPossible > 0 )
			{
				float target = (float)pixels / (float)pixelsPossible;
				target = (target >= 0.95f) ? 1.0f : (target < 0.0f) ? 0.0f : target;
				float rate = gpGlobals->frametime * fadeTimeInv;
				m_brightnessTarget = Approach( target, m_brightnessTarget, rate ); // fade in / out
			}
			else
			{
				m_brightnessTarget = 0.0f;
			}
		}
		else
		{
			if ( m_frameIssued != -1 )
			{
				pixels = pRenderContext->OcclusionQuery_GetNumPixelsRendered( m_queryHandle );
			}

			if ( r_pixelvisibility_spew.GetBool() && CurrentViewID() == 0 ) 
			{
				DevMsg( 1, "Pixels visible: %d (qh:%d) (frame:%d)\n", pixels, m_queryHandle, gpGlobals->framecount );
			}

			if ( pixels < 0 )
			{
				m_failed = ( m_frameIssued >= 0 ) ? true : false;
				return m_brightnessTarget * m_clipFraction;
			}
			m_hasValidQueryResults = true;
			if ( m_frameIssued == gpGlobals->framecount-1 )
			{
				float rate = gpGlobals->frametime * fadeTimeInv;
				float target = 0.0f;
				if ( pixels > 0 )
				{
					// fade in slower than you fade out
					rate *= 0.5f;
					target = 1.0f;
				}
				m_brightnessTarget = Approach( target, m_brightnessTarget, rate ); // fade in / out
			}
			else
			{
				m_brightnessTarget = 0.0f;
			}
		}
	}

	return m_brightnessTarget * m_clipFraction;
}

void CPixelVisibilityQuery::IssueQuery( IMatRenderContext *pRenderContext, float proxySize, float proxyAspect, IMaterial *pMaterial, bool sizeIsScreenSpace )
{
	if ( !m_failed )
	{
		Assert( IsValid() );

		if ( r_pixelvisibility_spew.GetBool() && CurrentViewID() == 0 ) 
		{
			DevMsg( 1, "Draw Proxy: qh:%d org:<%d,%d,%d> (frame:%d)\n", m_queryHandle, (int)m_origin[0], (int)m_origin[1], (int)m_origin[2], gpGlobals->framecount );
		}

		m_clipFraction = PixelVisibility_DrawProxy( pRenderContext, m_queryHandle, m_origin, proxySize, proxyAspect, pMaterial, sizeIsScreenSpace );
		if ( m_clipFraction < 0 )
		{
			// NOTE: In this case, the proxy wasn't issued cause it was offscreen
			// can't set the m_frameissued[ slot ] field since that would cause it to get marked as failed
			m_clipFraction = 0;
			m_wasQueriedThisFrame = false;
			m_failed = false;
			return;
		}
	}

	// In split screen we can issue these multiple times
	Assert( m_frameIssued != gpGlobals->framecount);


	m_frameIssued = gpGlobals->framecount;
	m_wasQueriedThisFrame = false;
	m_failed = false;
}

void CPixelVisibilityQuery::IssueCountingQuery( IMatRenderContext *pRenderContext, float proxySize, float proxyAspect, IMaterial *pMaterial, bool sizeIsScreenSpace )
{
	if ( !m_failed )
	{
		Assert( IsValid() );
#if 0
		// this centers it on the screen.
		// This is nice because it makes the glows fade as they get partially clipped by the view frustum
		// But it introduces sub-pixel errors (off by one row/column of pixels) so the glows shimmer
		// UNDONE: Compute an offset center coord that matches sub-pixel coords with the real glow position
		// UNDONE: Or frustum clip the sphere/geometry and fade based on proxy size
		Vector origin = m_origin - CurrentViewOrigin();
		float dot = DotProduct(CurrentViewForward(), origin);
		origin = CurrentViewOrigin() + dot * CurrentViewForward();
#endif
		PixelVisibility_DrawProxy( pRenderContext, m_queryHandleCount, m_origin, proxySize, proxyAspect, pMaterial, sizeIsScreenSpace );
	}
}

//Precache the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheOcclusionProxy )
PRECACHE( MATERIAL, "engine/occlusionproxy" )
PRECACHE( MATERIAL, "engine/occlusionproxy_countdraw" )
PRECACHE_REGISTER_END()

class CPixelVisibilitySystem : public CAutoGameSystem
{
public:
	
	// GameSystem: Level init, shutdown
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPostEntity();

	// locals
	CPixelVisibilitySystem();
	float GetFractionVisible( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle );
	void EndView();
	void EndScene();
	unsigned short FindQueryForView( CPixelVisSet *pSet, int nPlayerSlot, int viewID );
	unsigned short FindOrCreateQueryForView( CPixelVisSet *pSet, int nPlayerSlot, int viewID );

	void DeleteUnusedQueries( CPixelVisSet *pSet, bool bDeleteAll );
	void DeleteUnusedSets( bool bDeleteAll );
	void ShowQueries( bool show );
	unsigned short AllocQuery();
	unsigned short AllocSet();
	void FreeSet( unsigned short node );
	CPixelVisSet *FindOrCreatePixelVisSet( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle );
	bool SupportsOcclusion() { return m_hwCanTestGlows; }
	void DebugInfo()
	{
		Msg("Pixel vis system using %d sets total (%d in free list), %d queries total (%d in free list)\n", 
			m_setList.TotalCount(), m_setList.Count(m_freeSetsList), m_queryList.TotalCount(), m_queryList.Count( m_freeQueriesList ) );
	}

private:
	CUtlMultiList< CPixelVisSet, unsigned short >	m_setList;
	CUtlMultiList<CPixelVisibilityQuery, unsigned short> m_queryList;
	unsigned short m_freeQueriesList;
	unsigned short m_activeSetsList;
	unsigned short m_freeSetsList;
	unsigned short m_pad0;

	IMaterial	*m_pProxyMaterial;
	IMaterial	*m_pDrawMaterial;
	bool		m_hwCanTestGlows;
	bool		m_drawQueries;


	friend void PixelVisibility_ShiftVisibilityViews( int nPlayerSlot, int iSourceViewID, int iDestViewID ); //need direct access to private data to make shifting smooth
};

static CPixelVisibilitySystem g_PixelVisibilitySystem;

CPixelVisibilitySystem::CPixelVisibilitySystem() : CAutoGameSystem( "CPixelVisibilitySystem" )
{
	m_hwCanTestGlows = true;
	m_drawQueries = false;
}
// Level init, shutdown
void CPixelVisibilitySystem::LevelInitPreEntity()
{
	m_hwCanTestGlows = r_dopixelvisibility.GetBool();
	if ( m_hwCanTestGlows )
	{
		CMatRenderContextPtr pRenderContext( materials );

		OcclusionQueryObjectHandle_t query = pRenderContext->CreateOcclusionQueryObject();
		if ( query != INVALID_OCCLUSION_QUERY_OBJECT_HANDLE )
		{
			pRenderContext->DestroyOcclusionQueryObject( query );
		}
		else
		{
			m_hwCanTestGlows = false;
		}
	}

	m_pProxyMaterial = materials->FindMaterial("engine/occlusionproxy", TEXTURE_GROUP_CLIENT_EFFECTS);
	m_pProxyMaterial->IncrementReferenceCount();
	m_pDrawMaterial = materials->FindMaterial("engine/occlusionproxy_countdraw", TEXTURE_GROUP_CLIENT_EFFECTS);
	m_pDrawMaterial->IncrementReferenceCount();
	m_freeQueriesList = m_queryList.CreateList();
	m_activeSetsList = m_setList.CreateList();
	m_freeSetsList = m_setList.CreateList();
}

void CPixelVisibilitySystem::LevelShutdownPostEntity()
{
	m_pProxyMaterial->DecrementReferenceCount();
	m_pProxyMaterial = NULL;
	m_pDrawMaterial->DecrementReferenceCount();
	m_pDrawMaterial = NULL;
	DeleteUnusedSets(true);
	m_setList.Purge();
	m_queryList.Purge();
	m_freeQueriesList = m_queryList.InvalidIndex();
	m_activeSetsList = m_setList.InvalidIndex();
	m_freeSetsList = m_setList.InvalidIndex();
}

float CPixelVisibilitySystem::GetFractionVisible( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle )
{
	if ( !m_hwCanTestGlows || building_cubemaps.GetBool() )
	{
		return GlowSightDistance( params.position, true ) > 0 ? 1.0f : 0.0f;
	}

	if ( CurrentViewID() < 0 )
		return 0.0f;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nPlayerSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CPixelVisSet *pSet = FindOrCreatePixelVisSet( params, queryHandle );
	Assert( pSet );
	unsigned short node = FindOrCreateQueryForView( pSet, nPlayerSlot, CurrentViewID() );
	m_queryList[node].m_origin = params.position;
	float fraction = m_queryList[node].GetFractionVisible( pSet->fadeTimeInv );
	pSet->MarkActive();
	return fraction;
}

void CPixelVisibilitySystem::EndView()
{
	if ( !PixelVisibility_IsAvailable() && CurrentViewID() >= 0 )
		return;
	
	if ( m_setList.Head( m_activeSetsList ) == m_setList.InvalidIndex() )
		return;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nPlayerSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CMatRenderContextPtr pRenderContext( materials );

	IMaterial *pProxy = m_drawQueries ? m_pDrawMaterial : m_pProxyMaterial;
	pRenderContext->Bind( pProxy );

	// BUGBUG: If you draw both queries, the measure query fails for some reason.
	if ( r_pixelvisibility_partial.GetBool() && !m_drawQueries )
	{
		pRenderContext->DepthRange( 0.0f, 0.01f );
		unsigned short node = m_setList.Head( m_activeSetsList );
		while( node != m_setList.InvalidIndex() )
		{
			CPixelVisSet *pSet = &m_setList[node];
			unsigned short queryNode = FindQueryForView( pSet, nPlayerSlot, CurrentViewID() );
			if ( queryNode != m_queryList.InvalidIndex() )
			{
				m_queryList[queryNode].IssueCountingQuery( pRenderContext, pSet->proxySize, pSet->proxyAspect, pProxy, pSet->sizeIsScreenSpace );
			}
			node = m_setList.Next( node );
		}
		pRenderContext->DepthRange( 0.0f, 1.0f );
	}

	{
		unsigned short node = m_setList.Head( m_activeSetsList );
		while( node != m_setList.InvalidIndex() )
		{
			CPixelVisSet *pSet = &m_setList[node];
			unsigned short queryNode = FindQueryForView( pSet, nPlayerSlot, CurrentViewID() );
			if ( queryNode != m_queryList.InvalidIndex() )
			{
				m_queryList[queryNode].IssueQuery( pRenderContext, pSet->proxySize, pSet->proxyAspect, pProxy, pSet->sizeIsScreenSpace );
			}
			node = m_setList.Next( node );
		}
	}
}

void CPixelVisibilitySystem::EndScene()
{
	DeleteUnusedSets(false);
}

unsigned short CPixelVisibilitySystem::FindQueryForView( CPixelVisSet *pSet, int nPlayerSlot, int viewID )
{
	unsigned short node = m_queryList.Head( pSet->queryList );
	while ( node != m_queryList.InvalidIndex() )
	{
		if ( m_queryList[node].IsForView( nPlayerSlot, viewID ) )
			return node;
		node = m_queryList.Next( node );
	}
	return m_queryList.InvalidIndex();
}
unsigned short CPixelVisibilitySystem::FindOrCreateQueryForView( CPixelVisSet *pSet, int nPlayerSlot, int viewID )
{
	unsigned short node = FindQueryForView( pSet, nPlayerSlot, viewID );
	if ( node != m_queryList.InvalidIndex() )
		return node;

	node = AllocQuery();
	m_queryList.LinkToHead( pSet->queryList, node );
	m_queryList[node].SetView( nPlayerSlot, viewID );
	return node;
}


void CPixelVisibilitySystem::DeleteUnusedQueries( CPixelVisSet *pSet, bool bDeleteAll )
{
	unsigned short node = m_queryList.Head( pSet->queryList );
	while ( node != m_queryList.InvalidIndex() )
	{
		unsigned short next = m_queryList.Next( node );
		if ( bDeleteAll || !m_queryList[node].IsActive() )
		{
			m_queryList.Unlink( pSet->queryList, node);
			m_queryList.LinkToHead( m_freeQueriesList, node );
		}
		node = next;
	}
}
void CPixelVisibilitySystem::DeleteUnusedSets( bool bDeleteAll )
{
	unsigned short node = m_setList.Head( m_activeSetsList );
	while ( node != m_setList.InvalidIndex() )
	{
		unsigned short next = m_setList.Next( node );
		CPixelVisSet *pSet = &m_setList[node];
		if ( bDeleteAll || !m_setList[node].IsActive() )
		{
			DeleteUnusedQueries( pSet, true );
		}
		else
		{
			DeleteUnusedQueries( pSet, false );
		}
		if ( m_queryList.Head(pSet->queryList) == m_queryList.InvalidIndex() )
		{
			FreeSet( node );
		}
		node = next;
	}
}

void CPixelVisibilitySystem::ShowQueries( bool show )
{
	m_drawQueries = show;
}

unsigned short CPixelVisibilitySystem::AllocQuery()
{
	unsigned short node = m_queryList.Head(m_freeQueriesList);
	if ( node != m_queryList.InvalidIndex() )
	{
		m_queryList.Unlink( m_freeQueriesList, node );
		m_queryList[node].ResetOcclusionQueries();
	}
	else
	{
		node = m_queryList.Alloc();
	}
	return node;
}

unsigned short CPixelVisibilitySystem::AllocSet()
{
	unsigned short node = m_setList.Head(m_freeSetsList);
	if ( node != m_setList.InvalidIndex() )
	{
		m_setList.Unlink( m_freeSetsList, node );
	}
	else
	{
		node = m_setList.Alloc();
		m_setList[node].queryList = m_queryList.CreateList();
	}
	m_setList.LinkToHead( m_activeSetsList, node );
	return node;
}

void CPixelVisibilitySystem::FreeSet( unsigned short node )
{
	m_setList.Unlink( m_activeSetsList, node );
	m_setList.LinkToHead( m_freeSetsList, node );
	m_setList[node].serial++;
}

CPixelVisSet *CPixelVisibilitySystem::FindOrCreatePixelVisSet( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle )
{
	if ( queryHandle[0] )
	{
		unsigned short handle = queryHandle[0] & 0xFFFF;
		handle--;
		unsigned short serial = queryHandle[0] >> 16;
		if ( m_setList.IsValidIndex(handle) && m_setList[handle].serial == serial )
		{
			return &m_setList[handle];
		}
	}

	unsigned short node = AllocSet();
	m_setList[node].Init( params );
	unsigned int out = m_setList[node].serial;
	unsigned short nodeHandle = node + 1;
	out <<= 16;
	out |= nodeHandle;
	queryHandle[0] = out;
	return &m_setList[node];
}


void PixelvisDrawChanged( IConVar *pPixelvisVar, const char *pOld, float flOldValue )
{
	ConVarRef var( pPixelvisVar );
	g_PixelVisibilitySystem.ShowQueries( var.GetBool() );
}

class CTraceFilterGlow : public CTraceFilterSimple
{
public:
	DECLARE_CLASS( CTraceFilterGlow, CTraceFilterSimple );
	
	CTraceFilterGlow( const IHandleEntity *passentity, int collisionGroup ) : CTraceFilterSimple(passentity, collisionGroup) {}
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		IClientUnknown *pUnk = (IClientUnknown*)pHandleEntity;
		ICollideable *pCollide = pUnk->GetCollideable();
		if ( pCollide->GetSolid() != SOLID_VPHYSICS && pCollide->GetSolid() != SOLID_BSP )
			return false;
		return BaseClass::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};
float GlowSightDistance( const Vector &glowOrigin, bool bShouldTrace )
{
	float dist = (glowOrigin - CurrentViewOrigin()).Length();
	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();
	if ( local )
	{
		dist *= local->GetFOVDistanceAdjustFactor();
	}

	if ( bShouldTrace )
	{
		Vector end = glowOrigin;
		// HACKHACK: trace 4" from destination in case the glow is inside some parent object
		//			allow a little error...
		if ( dist > 4 )
		{
			end -= CurrentViewForward()*4;
		}
		int traceFlags = MASK_OPAQUE|CONTENTS_MONSTER|CONTENTS_DEBRIS;
		
		CTraceFilterGlow filter(NULL, COLLISION_GROUP_NONE);
		trace_t tr;
		UTIL_TraceLine( CurrentViewOrigin(), end, traceFlags, &filter, &tr );
		if ( tr.fraction != 1.0f )
			return -1;
	}

	return dist;
}

void PixelVisibility_EndCurrentView()
{
	g_PixelVisibilitySystem.EndView();
}

void PixelVisibility_EndScene()
{
	g_PixelVisibilitySystem.EndScene();
}

float PixelVisibility_FractionVisible( const pixelvis_queryparams_t &params, pixelvis_handle_t *queryHandle )
{
	if ( !queryHandle )
	{
		return GlowSightDistance( params.position, true ) > 0.0f ? 1.0f : 0.0f;
	}
	else 
	{
		return g_PixelVisibilitySystem.GetFractionVisible( params, queryHandle );
	}
}

bool PixelVisibility_IsAvailable()
{
	return r_dopixelvisibility.GetBool() && g_PixelVisibilitySystem.SupportsOcclusion();
}


//this originally called a class function of CPixelVisibiltySystem to keep the work clean, but that function needed friend access to CPixelVisibilityQuery 
//and I didn't want to make the whole class a friend or shift all the functions and class declarations around in this file
void PixelVisibility_ShiftVisibilityViews( int nPlayerSlot, int iSourceViewID, int iDestViewID )
{
	unsigned short node = g_PixelVisibilitySystem.m_setList.Head( g_PixelVisibilitySystem.m_activeSetsList );
	while ( node != g_PixelVisibilitySystem.m_setList.InvalidIndex() )
	{
		unsigned short next = g_PixelVisibilitySystem.m_setList.Next( node );
		CPixelVisSet *pSet = &g_PixelVisibilitySystem.m_setList[node];

		unsigned short iSourceQueryNode = g_PixelVisibilitySystem.FindQueryForView( pSet, nPlayerSlot, iSourceViewID );
		unsigned short iDestQueryNode = g_PixelVisibilitySystem.FindQueryForView( pSet, nPlayerSlot, iDestViewID );

		if( iDestQueryNode != g_PixelVisibilitySystem.m_queryList.InvalidIndex() )
		{
			//delete the destination if found
			g_PixelVisibilitySystem.m_queryList.Unlink( pSet->queryList, iDestQueryNode );
			g_PixelVisibilitySystem.m_queryList.LinkToHead( g_PixelVisibilitySystem.m_freeQueriesList, iDestQueryNode );

			if ( g_PixelVisibilitySystem.m_queryList.Head(pSet->queryList) == g_PixelVisibilitySystem.m_queryList.InvalidIndex() )
			{
				g_PixelVisibilitySystem.FreeSet( node );
			}
		}

		if( iSourceQueryNode != g_PixelVisibilitySystem.m_queryList.InvalidIndex() )
		{
			//make the source believe it's the destination
			g_PixelVisibilitySystem.m_queryList[iSourceQueryNode].m_viewID = iDestViewID;
		}		

		node = next;
	}

	for( int i = 0; i != s_OcclusionQueries.Count(); ++i )
	{
		int iPairCount = s_OcclusionQueries[i].occlusionHandles[nPlayerSlot].Count();
		OcclusionHandleViewIDPair_t *pPairs = s_OcclusionQueries[i].occlusionHandles[nPlayerSlot].Base();
		int iSourceIndex, iDestIndex, iInvalidIndex;
		iDestIndex = iSourceIndex = iInvalidIndex = s_OcclusionQueries[i].occlusionHandles[nPlayerSlot].InvalidIndex();
		for( int j = 0; j != iPairCount; ++j )
		{
			if( pPairs[j].iViewID == iSourceViewID )
			{
				iSourceIndex = j;
				if( iDestIndex != iInvalidIndex )
					break;
			}

			if( pPairs[j].iViewID == iDestViewID )
			{
				iDestIndex = j;
				if( iSourceIndex != iInvalidIndex )
					break;
			}
		}

		if( iSourceIndex != iInvalidIndex )
		{
			//change view id on source
			pPairs[iSourceIndex].iViewID = iDestViewID;
		}
		
		if( iDestIndex != iInvalidIndex )
		{
			//destroy dest
			materials->GetRenderContext()->DestroyOcclusionQueryObject( pPairs[iDestIndex].hOcclusionHandle );
			s_OcclusionQueries[i].occlusionHandles[nPlayerSlot].FastRemove( iDestIndex );
		}
	}
}



COcclusionQuerySet::COcclusionQuerySet( void )
{	
	OcclusionQueryHiddenData_t &data = s_OcclusionQueries[s_OcclusionQueries.AddToTail()];
	data.pOwner = this;
	m_pManagedData = &data;

	//handle base address shifting
	if( s_OcclusionQueries.Count() > 1 )
	{
		OcclusionQueryHiddenData_t &baseData = s_OcclusionQueries[0];
		if( baseData.pOwner->m_pManagedData != &baseData )
		{
			for( int i = 0; i != s_OcclusionQueries.Count(); ++i )
			{
				s_OcclusionQueries[i].pOwner->m_pManagedData = &s_OcclusionQueries[i];
			}
		}
	}
}

COcclusionQuerySet::~COcclusionQuerySet( void )
{
	int iIndex;
	for( iIndex = 0; iIndex != s_OcclusionQueries.Count(); ++iIndex )
	{
		if( &s_OcclusionQueries[iIndex] == m_pManagedData )
			break;
	}
	if( iIndex != s_OcclusionQueries.Count() )
	{
		//destroy query handles
		{
			CMatRenderContextPtr pRenderContext( materials );
			OcclusionQueryHiddenData_t &data = s_OcclusionQueries[iIndex];
			for( int i = 0; i != MAX_SPLITSCREEN_PLAYERS; ++i )
			{
				for( int j = 0; j != data.occlusionHandles[i].Count(); ++j )
				{
					pRenderContext->DestroyOcclusionQueryObject( data.occlusionHandles[i].Element(j).hOcclusionHandle );
				}
			}
		}

		s_OcclusionQueries.FastRemove( iIndex );
		if( s_OcclusionQueries.Count() != 0 )
		{
			s_OcclusionQueries[iIndex].pOwner->m_pManagedData = &s_OcclusionQueries[iIndex];
			//handle base address shifting
			if( s_OcclusionQueries.Count() > 1 )
			{
				OcclusionQueryHiddenData_t &baseData = s_OcclusionQueries[iIndex == 0 ? 1 : 0];
				if( baseData.pOwner->m_pManagedData != &baseData )
				{
					for( int i = 0; i != s_OcclusionQueries.Count(); ++i )
					{
						s_OcclusionQueries[i].pOwner->m_pManagedData = &s_OcclusionQueries[i];
					}
				}
			}
		}
	}
}



void COcclusionQuerySet::BeginQueryDrawing( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex == pData->occlusionHandles[iSplitScreenSlot].InvalidIndex() )
	{
		//create a new one
		iIndex = pData->occlusionHandles[iSplitScreenSlot].AddToTail();
		OcclusionHandleViewIDPair_t &Entry = pData->occlusionHandles[iSplitScreenSlot].Element(iIndex);
		Entry.iViewID = iViewID;
		Entry.hOcclusionHandle = materials->GetRenderContext()->CreateOcclusionQueryObject();	
		materials->GetRenderContext()->ResetOcclusionQueryObject( Entry.hOcclusionHandle );
	}

	materials->GetRenderContext()->BeginOcclusionQueryDrawing( pData->occlusionHandles[iSplitScreenSlot].Element(iIndex).hOcclusionHandle );
	pData->occlusionHandles[iSplitScreenSlot].Element(iIndex).iLastFrameRendered = gpGlobals->framecount;
}

void COcclusionQuerySet::BeginQueryDrawing( void )
{
	return BeginQueryDrawing( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}



void COcclusionQuerySet::EndQueryDrawing( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex != pData->occlusionHandles[iSplitScreenSlot].InvalidIndex() )
	{
		materials->GetRenderContext()->EndOcclusionQueryDrawing( pData->occlusionHandles[iSplitScreenSlot].Element(iIndex).hOcclusionHandle );
	}
}

void COcclusionQuerySet::EndQueryDrawing( void )
{
	return EndQueryDrawing( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}



int COcclusionQuerySet::QueryNumPixelsRendered( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex != pData->occlusionHandles[iSplitScreenSlot].InvalidIndex() )
	{
		return materials->GetRenderContext()->OcclusionQuery_GetNumPixelsRendered( pData->occlusionHandles[iSplitScreenSlot].Element(iIndex).hOcclusionHandle );
	}

	return 0;
}

int COcclusionQuerySet::QueryNumPixelsRendered( void )
{
	return QueryNumPixelsRendered( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}



float COcclusionQuerySet::QueryPercentageOfScreenRendered( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex != pData->occlusionHandles[iSplitScreenSlot].InvalidIndex() )
	{
		int iX, iY, iWidth, iHeight;
		materials->GetRenderContext()->GetViewport( iX, iY, iWidth, iHeight );
		return ((float)materials->GetRenderContext()->OcclusionQuery_GetNumPixelsRendered( pData->occlusionHandles[iSplitScreenSlot].Element(iIndex).hOcclusionHandle )) / ((float)(iWidth * iHeight));
	}

	return 0.0f;
}

float COcclusionQuerySet::QueryPercentageOfScreenRendered( void )
{
	return QueryPercentageOfScreenRendered( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}

int COcclusionQuerySet::QueryNumPixelsRenderedForAllViewsLastFrame( int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;
	int iMatchFrame = gpGlobals->framecount - 1;
	int iResult = 0;
	CMatRenderContextPtr pRenderContext( materials );
	
	for( int i = 0; i != pData->occlusionHandles[iSplitScreenSlot].Count(); ++i )
	{
		if( pData->occlusionHandles[iSplitScreenSlot].Element(i).iLastFrameRendered == iMatchFrame )
		{
			iResult += pRenderContext->OcclusionQuery_GetNumPixelsRendered( pData->occlusionHandles[iSplitScreenSlot].Element(i).hOcclusionHandle );
		}
	}

	return iResult;
}

int COcclusionQuerySet::QueryNumPixelsRenderedForAllViewsLastFrame( void )
{
	return QueryNumPixelsRenderedForAllViewsLastFrame( GET_ACTIVE_SPLITSCREEN_SLOT() );
}


int COcclusionQuerySet::GetLastFrameDrawn( int iViewID, int iSplitScreenSlot )
{
	OcclusionQueryHiddenData_t *pData = (OcclusionQueryHiddenData_t *)m_pManagedData;

	int iIndex = FindQueryHandlePairIndex( pData, iViewID, iSplitScreenSlot );
	if( iIndex != pData->occlusionHandles[iSplitScreenSlot].InvalidIndex() )
	{
		return pData->occlusionHandles[iSplitScreenSlot].Element(iIndex).iLastFrameRendered;
	}

	return -1;
}

int COcclusionQuerySet::GetLastFrameDrawn( void )
{
	return GetLastFrameDrawn( CurrentViewID(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}



CON_COMMAND( pixelvis_debug, "Dump debug info" )
{
	g_PixelVisibilitySystem.DebugInfo();
}
