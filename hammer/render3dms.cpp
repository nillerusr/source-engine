//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include <math.h>
#include <mmsystem.h>
#include "Camera.h"
#include "CullTreeNode.h"
#include "MapDefs.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapInstance.h"
#include "MapWorld.h"
#include "Render3DMS.h"
#include "SSolid.h"
#include "MapStudioModel.h"
#include "Material.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "TextureSystem.h"
#include "ToolInterface.h"
#include "StudioModel.h"
#include "ibsplighting.h"
#include "MapDisp.h"
#include "ToolManager.h"
#include "mapview.h"
#include "hammer.h"
#include "IStudioRender.h"
#include <renderparm.h>
#include "materialsystem/itexture.h"
#include "maplightcone.h"
#include "map_utils.h"
#include "bitmap/float_bm.h"
#include "lpreview_thread.h"
#include "hammer.h"
#include "mainfrm.h"
#include "mathlib/halton.h"
#include "Manifest.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#define NUM_MIPLEVELS			4

#define CROSSHAIR_DIST_HORIZONTAL		5
#define CROSSHAIR_DIST_VERTICAL			6

#define TEXTURE_AXIS_LENGTH				10	// Texture axis length in world units


// dvs: experiment!
//extern int g_nClipPoints;
//extern Vector g_ClipPoints[4];

//
// Debugging / diagnostic stuff.
//
static bool g_bDrawWireFrameSelection = true;
static bool g_bShowStatistics = false;
static bool g_bUseCullTree = true;
static bool g_bRenderCullBoxes = false;

int g_nBitmapGenerationCounter = 1;

//-----------------------------------------------------------------------------
// Purpose: Callback comparison function for sorting objects clicked on while
//			in selection mode.
// Input  : pHit1 - First hit to compare.
//			pHit2 - Second hit to compare.
// Output : Sorts by increasing depth value. Returns -1, 0, or 1 per qsort spec.
//-----------------------------------------------------------------------------
static int _CompareHits(const void *pHit1, const void *pHit2)
{
	if (((HitInfo_t *)pHit1)->nDepth < ((HitInfo_t *)pHit2)->nDepth)
	{
		return(-1);
	}

	if (((HitInfo_t *)pHit1)->nDepth > ((HitInfo_t *)pHit2)->nDepth)
	{
		return(1);
	}

	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: Callback comparison function for sorting objects clicked on while
//			in selection mode. The reverse sort is used for cards that return
//			depth values in reverse (larger numbers are closer to the camera).
// Input  : pHit1 - First hit to compare.
//			pHit2 - Second hit to compare.
// Output : Sorts by decreasing depth value. Returns -1, 0, or 1 per qsort spec.
//-----------------------------------------------------------------------------
static int _CompareHitsReverse(const void *pHit1, const void *pHit2)
{
	if (((HitInfo_t *)pHit1)->nDepth > ((HitInfo_t *)pHit2)->nDepth)
	{
		return(-1);
	}

	if (((HitInfo_t *)pHit1)->nDepth < ((HitInfo_t *)pHit2)->nDepth)
	{
		return(1);
	}

	return(0);
}

static bool TranslucentObjectsLessFunc( TranslucentObjects_t const&a, TranslucentObjects_t const&b )
{
	return (a.depth < b.depth);
}


bool GetRequiredMaterial( const char *pName, IMaterial* &pMaterial )
{
	pMaterial = NULL;
	IEditorTexture *pTex = g_Textures.FindActiveTexture( pName );
	if ( pTex )
		pMaterial = pTex->GetMaterial();

	if ( pMaterial )
	{
		return true;
	}
	else
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "Missing material '%s'. Go to Tools | Options | Game Configurations and verify that your game directory is correct.", pName );
		MessageBox( NULL, str, "FATAL ERROR", MB_OK );
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Calculates lighting for a given face.
// Input  : Normal - vector that is normal to the face being lit.
// Output : Returns a number from [0.2, 1.0]
//-----------------------------------------------------------------------------
float CRender3D::LightPlane(Vector& Normal)
{
	static Vector Light( 1.0f, 2.0f, 3.0f );
	static bool bFirst = true;

	if (bFirst)
	{
		VectorNormalize(Light);
		bFirst = false;
	}

	float fShade = 0.65f + (0.35f * DotProduct(Normal, Light));

	return(fShade);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRender3D::CRender3D(void) :
	CRender()
{
	memset(&m_WinData, 0, sizeof(m_WinData));
	m_WinData.bAllowSoft = true;

	memset(m_FrustumPlanes, 0, sizeof(m_FrustumPlanes));

	m_pDropCamera = new CCamera;
	m_bDroppedCamera = false;
	m_DeferRendering = false;
	m_TranslucentSortRendering = false;

	m_fFrameRate = 0;
	m_nFramesThisSample = 0;
	m_dwTimeLastSample = 0;
	m_dwTimeLastFrame = 0;
	m_fTimeElapsed = 0;

	m_LastLPreviewCameraPos = Vector(1.0e22,1.0e22,1.0e22);
	m_nLastLPreviewWidth = -1;
	m_nLastLPreviewHeight = -1;

	memset(&m_Pick, 0, sizeof(m_Pick));
	m_Pick.bPicking = false;

	memset(&m_RenderState, 0, sizeof(m_RenderState));

	for (int i = 0; i < 2; ++i)
	{
		m_pVertexColor[i] = 0;
	}
	m_bLightingPreview = false;

	m_TranslucentRenderObjects.SetLessFunc( TranslucentObjectsLessFunc );

#ifdef _DEBUG
	m_bRenderFrustum = false;
	m_bRecomputeFrustumRenderGeometry = false;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CRender3D::~CRender3D(void)
{
	if (m_pDropCamera != NULL)
	{
		delete m_pDropCamera;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called before rendering an object that should be hit tested when
//			rendering in selection mode.
// Input  : pObject - Map atom pointer that will be returned from the ObjectsAt
//			routine if this rendered object is positively hit tested.
//-----------------------------------------------------------------------------
void CRender3D::BeginRenderHitTarget(CMapAtom *pObject, unsigned int uHandle)
{
	if ( m_Pick.bPicking == false )
	{
		return;
	}

	if ( ( m_Pick.m_nFlags & FLAG_OBJECTS_AT_RESOLVE_INSTANCES ) == 0 && m_bInstanceRendering && !GetInstanceClass()->IsEditable() )
	{
		pObject = m_CurrentInstanceState.m_pTopInstanceClass; // GetInstanceClass();
		uHandle = 0;
	}

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	pRenderContext->PushSelectionName((unsigned int)pObject);
	pRenderContext->PushSelectionName(uHandle);
}


//-----------------------------------------------------------------------------
// Purpose: Called after rendering an object that should be hit tested when
//			rendering in selection mode.
// Input  : pObject - Map atom pointer that will be returned from the ObjectsAt
//			routine if this rendered object is positively hit tested.
// Input  : pObject - 
//-----------------------------------------------------------------------------
void CRender3D::EndRenderHitTarget(void)
{
	if ( m_Pick.bPicking )
	{
		//
		// Pop the name and the handle from the stack.
		//

		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		pRenderContext->PopSelectionName();
		pRenderContext->PopSelectionName();

		if ((pRenderContext->SelectionMode(true) != 0) && (m_Pick.nNumHits < MAX_PICK_HITS))
		{
			if (m_Pick.uSelectionBuffer[0] == 2)
			{
				m_Pick.Hits[m_Pick.nNumHits].pObject = (CMapClass *)m_Pick.uSelectionBuffer[3];
				m_Pick.Hits[m_Pick.nNumHits].uData = m_Pick.uSelectionBuffer[4];
				m_Pick.Hits[m_Pick.nNumHits].nDepth = m_Pick.uSelectionBuffer[1];
				m_Pick.Hits[m_Pick.nNumHits].m_LocalMatrix = m_LocalMatrix.Head();
				m_Pick.nNumHits++;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
void CRender3D::AddTranslucentDeferredRendering( CMapPoint *pMapPoint )
{
	// object is translucent, render in 2nd batch
	Vector direction = m_pView->GetViewAxis();
	Vector center;	
	pMapPoint->GetOrigin(center);

	TranslucentObjects_t entry;
	if ( m_bInstanceRendering )
	{
		center += GetInstanceOrigin();
		entry.m_InstanceState = m_CurrentInstanceState;
		entry.m_bInstanceSelected = ( m_InstanceSelectionDepth != 0 );
	}
	else
	{
		entry.m_InstanceState.m_pInstanceClass = NULL;
	}

	entry.object = pMapPoint;
	entry.depth = center.Dot( direction );

	m_TranslucentRenderObjects.Insert(entry);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
float CRender3D::GetElapsedTime(void)
{
	return(m_fTimeElapsed);
}


//-----------------------------------------------------------------------------
// Computes us some geometry to render the frustum planes
//-----------------------------------------------------------------------------

void CRender3D::ComputeFrustumRenderGeometry(CCamera *pCamera)
{
#ifdef _DEBUG
	Vector viewPoint;
	pCamera->GetViewPoint(viewPoint);

	// Find lines along each of the plane intersections.
	// We know these lines are perpendicular to both plane normals,
	// so we can take the cross product to find them.
	static int edgeIdx[4][2] =
		{
			{ 0, 2 }, { 0, 3 }, { 1, 3 }, { 1, 2 }
		};

	int i;
	Vector edges[4];
	for ( i = 0; i < 4; ++i)
	{
		CrossProduct( m_FrustumPlanes[edgeIdx[i][0]].AsVector3D(),
					  m_FrustumPlanes[edgeIdx[i][1]].AsVector3D(), edges[i] );
		VectorNormalize( edges[i] );
	}

	// Figure out four near points by intersection lines with the near plane
	// Figure out four far points by intersection with lines against far plane
	for (i = 0; i < 4; ++i)
	{
		float t = (m_FrustumPlanes[4][3] - DotProduct(m_FrustumPlanes[4].AsVector3D(), viewPoint)) /
			DotProduct(m_FrustumPlanes[4].AsVector3D(), edges[i]);
		VectorMA( viewPoint, t, edges[i], m_FrustumRenderPoint[i] );

		/*
		  t = (m_FrustumPlanes[5][3] - DotProduct(m_FrustumPlanes[5], viewPoint)) /
		  DotProduct(m_FrustumPlanes[5], edges[i]);
		  VectorMA( viewPoint, t, edges[i], m_FrustumRenderPoint[i + 4] );
		*/
		if (t < 0)
		{
			edges[i] *= -1;
		}

		VectorMA( m_FrustumRenderPoint[i], 200.0, edges[i], m_FrustumRenderPoint[i + 4] );
	}
#endif
}

//-----------------------------------------------------------------------------
// renders the frustum
//-----------------------------------------------------------------------------

void CRender3D::RenderFrustum( )
{
#ifdef _DEBUG
	static int indices[] = 
		{
			0, 1, 1, 2, 2, 3, 3, 0,	// near square
			4, 5, 5, 6, 6, 7, 7, 4,	// far square
			0, 4, 1, 5, 2, 6, 3, 7	// connections between them
		};

	PushRenderMode( RENDER_MODE_WIREFRAME ); 
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	int numIndices = sizeof(indices) / sizeof(int);
	CMeshBuilder meshBuilder3D;
	meshBuilder3D.Begin( pMesh, MATERIAL_LINES, 8, numIndices );

	for ( int i = 0; i < 8; ++i )
	{
		meshBuilder3D.Position3fv( m_FrustumRenderPoint[i].Base() );
		meshBuilder3D.Color4ub( 255, 255, 255, 255 );
		meshBuilder3D.AdvanceVertex();
	}

	for ( int i = 0; i < numIndices; ++i )
	{
		meshBuilder3D.Index( indices[i] );
		meshBuilder3D.AdvanceIndex();
	}

	meshBuilder3D.End();
	pMesh->Draw();

	PopRenderMode(); 
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Returns the 3D grid spacing, in world units.
//-----------------------------------------------------------------------------
float CRender3D::GetGridDistance(void)
{
	return(m_RenderState.fGridDistance);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the 3D grid spacing, in world units.
//-----------------------------------------------------------------------------
float CRender3D::GetGridSize(void)
{
	return(m_RenderState.fGridSpacing);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwnd - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CRender3D::SetView( CMapView *pView )
{
	if ( !CRender::SetView( pView ) )
		return false;

	HWND hwnd = pView->GetViewWnd()->GetSafeHwnd();
	CMapDoc *pDoc = pView->GetMapDoc();

	Assert(hwnd != NULL);
	Assert(pDoc != NULL);
	Assert(pDoc->GetMapWorld() != NULL);
	  
	if (!MaterialSystemInterface()->AddView( hwnd ))
	{
		return false;
	}

	MaterialSystemInterface()->SetView( hwnd );

	m_WinData.hWnd = hwnd;

	if ((m_WinData.hDC = GetDCEx(m_WinData.hWnd, NULL, DCX_CACHE | DCX_CLIPSIBLINGS)) == NULL)
	{
		ChangeDisplaySettings(NULL, 0);
		MessageBox(NULL, "GetDC on main window failed", "FATAL ERROR", MB_OK);
		return(false);
	}

	// Preload all our stuff (textures, etc) for rendering.
	Preload( pDoc->GetMapWorld() );

	// Store off the three materials we use most often...
	if ( !GetRequiredMaterial( "editor/vertexcolor", m_pVertexColor[0] ) )
	{
		return false;
	}
	
	m_pVertexColor[1] = m_pVertexColor[0];

	return(true);
}


//-----------------------------------------------------------------------------
// Purpose: Determines the visibility of the given axis-aligned bounding box.
// Input  : pBox - Bounding box to evaluate.
// Output : VIS_TOTAL if the box is entirely within the view frustum.
//			VIS_PARTIAL if the box is partially within the view frustum.
//			VIS_NONE if the box is entirely outside the view frustum.
//-----------------------------------------------------------------------------
Visibility_t CRender3D::IsBoxVisible(Vector const &BoxMins, Vector const &BoxMaxs)
{
	Vector NearVertex;
	Vector FarVertex;

	//
	// Build the near and far vertices based on the octant of the plane normal.
	//
	int nInPlanes = 0;
	for ( int i = 0; i < 6; i++ )
	{
		if (m_FrustumPlanes[i][0] > 0)
		{
			NearVertex[0] = BoxMins[0];
			FarVertex[0] = BoxMaxs[0];
		}
		else
		{
			NearVertex[0] = BoxMaxs[0];
			FarVertex[0] = BoxMins[0];
		}

		if (m_FrustumPlanes[i][1] > 0)
		{
			NearVertex[1] = BoxMins[1];
			FarVertex[1] = BoxMaxs[1];
		}
		else
		{
			NearVertex[1] = BoxMaxs[1];
			FarVertex[1] = BoxMins[1];
		}

		if (m_FrustumPlanes[i][2] > 0)
		{
			NearVertex[2] = BoxMins[2];
			FarVertex[2] = BoxMaxs[2];
		}
		else
		{
			NearVertex[2] = BoxMaxs[2];
			FarVertex[2] = BoxMins[2];
		}

		if (DotProduct(m_FrustumPlanes[i].AsVector3D(), NearVertex) >= m_FrustumPlanes[i][3])
		{
			return(VIS_NONE);
		}

		if (DotProduct(m_FrustumPlanes[i].AsVector3D(), FarVertex) < m_FrustumPlanes[i][3])
		{
			nInPlanes++;
		}
	}

	if (nInPlanes == 6)
	{
		return(VIS_TOTAL);
	}
	
	return(VIS_PARTIAL);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eRenderState - 
// Output : Returns true if the render state is enabled, false if it is disabled.
//-----------------------------------------------------------------------------
bool CRender3D::IsEnabled(RenderState_t eRenderState)
{
	switch (eRenderState)
	{
		case RENDER_CENTER_CROSSHAIR:
		{
			return(m_RenderState.bCenterCrosshair);
		}

		case RENDER_GRID:
		{
			return(m_RenderState.bDrawGrid);
		}

		case RENDER_REVERSE_SELECTION:
		{
			return(m_RenderState.bReverseSelection);
		}
	}

	return(false);
}


//-----------------------------------------------------------------------------
// Purpose: Determines whether we are rendering for for selection or not.
// Output : Returns true if we are rendering for selection, false if rendering normally.
//-----------------------------------------------------------------------------
bool CRender3D::IsPicking(void)
{
	return(m_Pick.bPicking);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the map objects within the rectangle whose upper left corner
//			is at the client coordinates (x, y) and whose width and height are
//			fWidth and fHeight.
// Input  : x - Leftmost point in the rectangle, in client coordinates.
//			y - Topmost point in the rectangle, in client coordinates.
//			fWidth - Width of rectangle, in client coordinates.
//			fHeight - Height of rectangle, in client coordinates.
//			pObjects - Pointer to buffer to receive objects intersecting the rectangle.
//			nMaxObjects - Maximum number of object pointers to place in the buffer.
// Output : Returns the number of object pointers placed in the buffer pointed to
//			by 'pObjects'.
//-----------------------------------------------------------------------------
int CRender3D::ObjectsAt( float x, float y, float fWidth, float fHeight, HitInfo_t *pObjects, int nMaxObjects, unsigned int nFlags )
{
	int width, height;

	GetCamera()->GetViewPort(width,height);

	m_Pick.fX = x;
	m_Pick.fY = height - (y + 1);
	m_Pick.fWidth = fWidth;
	m_Pick.fHeight = fHeight;
	m_Pick.pHitsDest = pObjects;
	m_Pick.nMaxHits = min(nMaxObjects, MAX_PICK_HITS);
	m_Pick.nNumHits = 0;

	if (!m_RenderState.bReverseSelection)
	{
		m_Pick.uLastZ = 0xFFFFFFFF;
	}
	else
	{
		m_Pick.uLastZ = 0;
	}

	m_Pick.m_nFlags = nFlags;
	m_Pick.bPicking = true;

	EditorRenderMode_t eOldMode = GetDefaultRenderMode();
	SetDefaultRenderMode( RENDER_MODE_TEXTURED );

	bool bOldLightPreview = IsInLightingPreview();
	SetInLightingPreview( false );

	Render();

	SetDefaultRenderMode( eOldMode );
	SetInLightingPreview( bOldLightPreview );

	m_Pick.bPicking = false;

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	pRenderContext->SelectionMode(false);

	return(m_Pick.nNumHits);
}

static ITexture *SetRenderTargetNamed(int nWhichTarget, char const *pRtName)
{
	CMatRenderContextPtr pRenderContext( materials );
	ITexture *dest_rt=materials->FindTexture(pRtName, TEXTURE_GROUP_RENDER_TARGET );
	pRenderContext->SetRenderTargetEx(nWhichTarget,dest_rt);
	return dest_rt;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRender3D::StartRenderFrame(void)
{
	CRender::StartRenderFrame();

	CCamera *pCamera = GetCamera();

	//
	// Determine the elapsed time since the last frame was rendered.
	//
	DWORD dwTimeNow = timeGetTime();
	if (m_dwTimeLastFrame == 0)
	{
		m_dwTimeLastFrame = dwTimeNow;
	}
	DWORD dwTimeElapsed = dwTimeNow - m_dwTimeLastFrame;
	m_fTimeElapsed = (float)dwTimeElapsed / 1000.0;
	m_dwTimeLastFrame = dwTimeNow;

	//
	// Animate the models based on elapsed time.
	//
	CMapStudioModel::AdvanceAnimation( GetElapsedTime() );

	// We're drawing to this view now
	MaterialSystemInterface()->SetView( m_WinData.hWnd );

	// view materialsystem viewport
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	int width, height;
	pCamera->GetViewPort( width, height );
	if (
		(m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW2) ||
		(m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED)
		)
	{
		AllocateLightingPreviewtextures();

		ITexture *first_rt=SetRenderTargetNamed(0,"_rt_albedo");
		SetRenderTargetNamed(1,"_rt_normal");
		SetRenderTargetNamed(2,"_rt_position");
		SetRenderTargetNamed(3,"_rt_flags");
		int nTargetWidth = min( width, first_rt->GetActualWidth() );
		int nTargetHeight = min( height, first_rt->GetActualHeight() );
		pRenderContext->
			Viewport(0, 0, nTargetWidth, nTargetHeight );
		pRenderContext->ClearColor3ub(0,1,0);
		pRenderContext->ClearBuffers( true, true );
	}
	else
		pRenderContext->Viewport(0, 0, width, height);

	//
	// Setup the camera position, orientation, and FOV.
	//
	//
	// Set up our perspective transformation.
	//

	// if picking, setup extra perspective matrix
	if ( m_Pick.bPicking )
	{
		pRenderContext->MatrixMode(MATERIAL_PROJECTION);
		pRenderContext->LoadIdentity();

		pRenderContext->PickMatrix(m_Pick.fX, m_Pick.fY, m_Pick.fWidth, m_Pick.fHeight);
		pRenderContext->SelectionBuffer(m_Pick.uSelectionBuffer, sizeof(m_Pick.uSelectionBuffer));
		pRenderContext->SelectionMode(true);
		pRenderContext->ClearSelectionNames();

		float aspect = (float)width / (float)height; 

		pRenderContext->PerspectiveX( pCamera->GetFOV(), 
			aspect, pCamera->GetNearClip(), pCamera->GetFarClip() );
	}
	else
	{
		//
		// Clear the frame buffer and Z buffer.
		//
		pRenderContext->ClearColor3ub( 0,0,0 );
		pRenderContext->ClearBuffers( true, true, true );
	}

	//
	// Build the frustum planes for view volume culling.
	//
	CCamera *pTempCamera = NULL;

	if (m_bDroppedCamera)
	{
		pTempCamera = pCamera;
		pCamera = m_pDropCamera;
	}

	pCamera->GetFrustumPlanes( m_FrustumPlanes);

	// For debugging frustum planes
#ifdef _DEBUG
	if (m_bRecomputeFrustumRenderGeometry)
	{
		ComputeFrustumRenderGeometry( pCamera );
		m_bRecomputeFrustumRenderGeometry = false;
	}
#endif

	if (m_bDroppedCamera)
	{
		pCamera = pTempCamera;
	}

	//
	// Cache per-frame information from the doc.
	//
	m_RenderState.fGridSpacing = m_pView->GetMapDoc()->GetGridSpacing();
	m_RenderState.fGridDistance = m_RenderState.fGridSpacing * 10;
	if (m_RenderState.fGridDistance > 2048)
	{
		m_RenderState.fGridDistance = 2048;
	}
	else if (m_RenderState.fGridDistance < 64)
	{
		m_RenderState.fGridDistance = 64;
	}

	// We do bizarro reverse culling in WC
	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	Assert( m_TranslucentRenderObjects.Count() == 0 );
}


static void SetNamedMaterialVar(IMaterial *pMat, char const *pVName, float fValue)
{
	IMaterialVar *pVar = pMat->FindVar( pVName, NULL );
	pVar->SetFloatValue( fValue );
}

class CLightPreview_Light
{
public:
	LightDesc_t m_Light;
	float m_flDistanceToEye;
};

bool CompareLightPreview_Lights(CLightPreview_Light const &a, CLightPreview_Light const &b)
{
	return (a.m_flDistanceToEye > b.m_flDistanceToEye);
}

#define MAX_PREVIEW_LIGHTS 10								// max # of lights to process.


void CRender3D::SendShadowTriangles( void )
{
	static int LastSendTimeStamp=-1;
	if ( GetUpdateCounter( EVTYPE_FACE_CHANGED ) != LastSendTimeStamp )
	{
		LastSendTimeStamp = GetUpdateCounter( EVTYPE_FACE_CHANGED );
		CUtlVector<Vector> *tri_list=new CUtlVector<Vector>;
		CMapDoc *pDoc = m_pView->GetMapDoc();
		CMapWorld *pWorld = pDoc->GetMapWorld();
		
		if ( !pWorld )
			return;
		
		if (g_pLPreviewOutputBitmap)
			delete g_pLPreviewOutputBitmap;
		g_pLPreviewOutputBitmap = NULL;
		EnumChildrenPos_t pos;
		CMapClass *pChild = pWorld->GetFirstDescendent( pos );
		while ( pChild )
		{
			if (pChild->IsVisible())
				pChild->AddShadowingTriangles( *tri_list );
			pChild = pWorld->GetNextDescendent( pos );
		}
		if ( tri_list->Count() )
		{
			MessageToLPreview msg( LPREVIEW_MSG_GEOM_DATA );
			msg.m_pShadowTriangleList = tri_list;
			g_HammerToLPreviewMsgQueue.QueueMessage( msg );
		}
		else
			delete tri_list;
	}
	
}


static bool LightForString( char const *pLight, Vector& intensity )
{
	double r, g, b, scaler;

	VectorFill( intensity, 0 );

	// scanf into doubles, then assign, so it is vec_t size independent
	r = g = b = scaler = 0;
	double r_hdr,g_hdr,b_hdr,scaler_hdr;
	int argCnt = sscanf ( pLight, "%lf %lf %lf %lf %lf %lf %lf %lf", 
						  &r, &g, &b, &scaler, &r_hdr,&g_hdr,&b_hdr,&scaler_hdr );
	
	// This is a special case for HDR lights.  If we have a vector of [-1, -1, -1, 1], then we
	// need to fall back to the non-HDR lighting since the HDR lighting hasn't been defined
	// for this light source.
	if( ( argCnt == 3 && r == -1.0f && g == -1.0f && b == -1.0f ) ||
		( argCnt == 4 && r == -1.0f && g == -1.0f && b == -1.0f && scaler == 1.0f ) )
	{
		intensity.Init( -1.0f, -1.0f, -1.0f );
		return true;
	}
	
	if (argCnt==8) 											// 2 4-tuples
	{
		if (g_bHDR)
		{
			r=r_hdr;
			g=g_hdr;
			b=b_hdr;
			scaler=scaler_hdr;
		}
		argCnt=4;
	}
	
	intensity[0] = pow( r / 255.0, 2.2 ) * 255;				// convert to linear
	
	switch( argCnt)
	{
		case 1:
			// The R,G,B values are all equal.
			intensity[1] = intensity[2] = intensity[0]; 
			break;
			
		case 3:
		case 4:
			// Save the other two G,B values.
			intensity[1] = pow( g / 255.0, 2.2 ) * 255;
			intensity[2] = pow( b / 255.0, 2.2 ) * 255;
			
			// Did we also get an "intensity" scaler value too?
			if ( argCnt == 4 )
			{
				// Scale the normalized 0-255 R,G,B values by the intensity scaler
				VectorScale( intensity, scaler / 255.0, intensity );
			}
			break;

		default:
			printf("unknown light specifier type - %s\n",pLight);
			return false;
	}
	// change light to 0..1
	intensity *= (1.0/255);
	return true;
}

// ugly code copied from vrad and munged. Should move into a lib
static bool LightForKey (CMapEntity *ent, char *key, Vector& intensity )
{
	char const *pLight = ent->GetKeyValue( key );

	return LightForString( pLight, intensity );
}

static void GetVectorForKey( CMapEntity *e, char const *kname, Vector *out )
{
	Vector ret(-1,-1,-1);
	char const *pk = e->GetKeyValue( kname );
	if ( pk )
	{
		sscanf( pk, "%f %f %f", &(ret.x), &(ret.y), &(ret.z) );
	}
	*out=ret;
}
static float GetFloatForKey( CMapEntity *e, char const *kname)
{
	char const *pk = e->GetKeyValue( kname );
	if ( pk )
		return atof( pk );
	else
		return 0.0;
}

static void SetLightFalloffParams( CMapEntity *e, CLightingPreviewLightDescription &l)
{
	float d50=GetFloatForKey(e,"_fifty_percent_distance");
	if (d50)
	{
		float d0=GetFloatForKey(e,"_zero_percent_distance");
		l.SetupNewStyleAttenuation( d50, d0 );
	}
	else
	{
		float c = GetFloatForKey (e, "_constant_attn");
		float b = GetFloatForKey (e, "_linear_attn");
		float a = GetFloatForKey (e, "_quadratic_attn");
		
		l.SetupOldStyleAttenuation( a, b, c );
	}
}

static bool ParseLightAmbient( CMapEntity *e, CLightingPreviewLightDescription &out )
{
	if( LightForKey( e, "_ambient", out.m_Color ) == 0 )
		return false;
	return true;

}

static bool ParseLightGeneric( CMapEntity *e, CLightingPreviewLightDescription &out )
{
	// returns false if it doesn't like the light

	// get intensity
	if( g_bHDR )
	{
		if( LightForKey( e, "_lightHDR", out.m_Color ) == 0 ||
			( out.m_Color.x == -1.0f && 
			  out.m_Color.y == -1.0f && 
			  out.m_Color.z == -1.0f ) )
		{
			LightForKey( e, "_light", out.m_Color );
		}
	}
	else
	{
		LightForKey( e, "_light", out.m_Color );
	}
	
	// handle spot falloffs
	if ( out.m_Type == MATERIAL_LIGHT_SPOT )
	{
		out.m_Theta=GetFloatForKey(e, "_inner_cone");
		out.m_Theta *= (M_PI/180.0);
		out.m_Phi=GetFloatForKey(e,"_cone");
		out.m_Phi *= (M_PI/180.0);
		out.m_Falloff=GetFloatForKey(e,"_exponent");
	}


	// check angle, targets
#if 0														// !!bug!!
	target = e->m_KeyValues.GetValue( "target");
	if (target[0])
	{	// point towards target
		entity_t		*e2;
		char	        *target;
		e2 = FindTargetEntity (target);
		if (!e2)
			Warning("WARNING: light at (%i %i %i) has missing target\n",
					(int)dl->light.origin[0], (int)dl->light.origin[1], (int)dl->light.origin[2]);
		else
		{
			Vector dest;
			GetVectorForKey (e2, "origin", &dest);
			VectorSubtract (dest, dl->light.origin, dl->light.normal);
			VectorNormalize (dl->light.normal);
		}
	}
	else
#endif
	{	
		// point down angle
		Vector angles;
		GetVectorForKey( e, "angles", &angles );
		float pitch = GetFloatForKey( e,"pitch");
		float angle = GetFloatForKey( e,"angle" );
		SetupLightNormalFromProps( QAngle( angles.x, angles.y, angles.z ), angle, pitch, 
								   out.m_Direction );
	}
	if ( out.m_Type == MATERIAL_LIGHT_DIRECTIONAL )
	{
		out.m_Range = 0;
		out.m_Attenuation2 = out.m_Attenuation1 = out.m_Attenuation0 = 0;
		out.m_Direction *= -1;
	}
	else
		SetLightFalloffParams( e, out );
	return true;
}

// when there are multiple lighting environments, we are supposed to ignore but the first
static bool s_bAddedLightEnvironmentAlready;


static void AddEntityLightToLightList( 
	CMapEntity *e,
	CUtlVector<CLightingPreviewLightDescription> &listout )
{
	char const *pszClassName=e->GetClassName();
	if (pszClassName)
	{
		CLightingPreviewLightDescription new_l;
		new_l.Init( e->m_nObjectID );
		e->GetOrigin( new_l.m_Position );
		new_l.m_Range = 0;

		if ( (! s_bAddedLightEnvironmentAlready ) &&
			 (! stricmp( pszClassName, "light_environment" ) ))
		{
			// lets add the sun to the list!
			new_l.m_Type = MATERIAL_LIGHT_DIRECTIONAL;
			if ( ParseLightGeneric(e,new_l) )
			{
				new_l.m_Position = new_l.m_Direction * 100000;
				new_l.RecalculateDerivedValues();
				listout.AddToTail( new_l );
				s_bAddedLightEnvironmentAlready = true;
			}
			// now, add the ambient sphere. We will approximate as "N" directional lights
			if ( ParseLightAmbient( e, new_l ) )
			{
				DirectionalSampler_t sampler;
				Vector color = new_l.m_Color;
				for( int i = 0; i < 160; i++)
				{
					new_l.Init( 0x80000000 | i );			// special id for ambient
					new_l.m_Type = MATERIAL_LIGHT_DIRECTIONAL;
					Vector dir = sampler.NextValue();
					new_l.m_Direction = dir;
					new_l.m_Position = new_l.m_Direction * 100000;
					new_l.m_Color = color * ( 1.0 / 160.0 );
					new_l.RecalculateDerivedValues();
					listout.AddToTail( new_l );
				}
			}
		}
		else if ( (! stricmp( pszClassName, "light" ) ))
		{
			// add point light to list
			new_l.m_Type = MATERIAL_LIGHT_POINT;
			if ( ParseLightGeneric(e,new_l) )
			{
				new_l.RecalculateDerivedValues();
				listout.AddToTail( new_l );
			}
		}
		else if ( (! stricmp( pszClassName, "light_spot" ) ))
		{
			// add point light to list
			new_l.m_Type = MATERIAL_LIGHT_SPOT;
			if ( ParseLightGeneric(e,new_l) )
			{
				new_l.RecalculateDerivedValues();
				listout.AddToTail( new_l );
			}
		}

	}
}


void CRender3D::BuildLightList( CUtlVector<CLightingPreviewLightDescription> *pList ) const
{
	CMapDoc *pDoc = m_pView->GetMapDoc();
	CMapWorld *pWorld = pDoc->GetMapWorld();
	
	if ( !pWorld )
		return;
	
	EnumChildrenPos_t pos;
	CMapClass *pChild = pWorld->GetFirstDescendent( pos );
	while ( pChild )
	{
		CMapEntity *pLightEntity=dynamic_cast<CMapEntity*>( pChild );
		if (pLightEntity && (pLightEntity->m_EntityTypeFlags & ENTITY_FLAG_IS_LIGHT ) &&
			(pLightEntity->IsVisible()) )
			AddEntityLightToLightList( pLightEntity, *pList );
		pChild = pWorld->GetNextDescendent( pos );
	}
}

void CRender3D::SendLightList( void )
{
	// send light list to lighting preview thread in priority order

	static int LastSendTimeStamp=-1;
	s_bAddedLightEnvironmentAlready = false;
	if ( GetUpdateCounter( EVTYPE_LIGHTING_CHANGED ) != LastSendTimeStamp )
	{
		LastSendTimeStamp = GetUpdateCounter( EVTYPE_LIGHTING_CHANGED );
		if (g_pLPreviewOutputBitmap)
			delete g_pLPreviewOutputBitmap;
		g_pLPreviewOutputBitmap = NULL;
		// now, get list of lights
		CUtlVector<CLightingPreviewLightDescription> *pList=new CUtlVector<CLightingPreviewLightDescription>;
		BuildLightList( pList );
		MessageToLPreview Msg( LPREVIEW_MSG_LIGHT_DATA );
		Msg.m_pLightList = pList;								// thread deletes
		CCamera *pCamera = GetCamera();
		pCamera->GetViewPoint( Msg.m_EyePosition );
	
		g_HammerToLPreviewMsgQueue.QueueMessage( Msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRender3D::EndRenderFrame(void)
{
	CRender::EndRenderFrame();

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	if (m_Pick.bPicking)
	{
		pRenderContext->Flush();

		//
		// Some OpenGL drivers, such as the ATI Rage Fury Max, return selection buffer Z values
		// in reverse order. For these cards, we must reverse the selection order.
		//
		if (m_Pick.nNumHits > 1)
		{
			if (!m_RenderState.bReverseSelection)
			{
				qsort(m_Pick.Hits, m_Pick.nNumHits, sizeof(m_Pick.Hits[0]), _CompareHits);
			}
			else
			{
				qsort(m_Pick.Hits, m_Pick.nNumHits, sizeof(m_Pick.Hits[0]), _CompareHitsReverse);
			}
		}

		//
		// Copy the requested number of nearest hits into the destination buffer.
		//
		int nHitsToCopy = min(m_Pick.nNumHits, m_Pick.nMaxHits);
		if (nHitsToCopy != 0)
		{
			memcpy(m_Pick.pHitsDest, m_Pick.Hits, sizeof(m_Pick.Hits[0]) * nHitsToCopy);
		}
	}

	//
	// Copy the GL buffer contents to our window's device context unless we're in pick mode.
	//
	if (!m_Pick.bPicking)
	{
		if (
			(m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW2) ||
			(m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED)
			)
		{
			pRenderContext->Flush();
			pRenderContext->SetRenderTarget( NULL );
			pRenderContext->SetRenderTargetEx( 1,NULL );
			pRenderContext->SetRenderTargetEx( 2,NULL );
			pRenderContext->SetRenderTargetEx( 3,NULL );


			ITexture *pRT = SetRenderTargetNamed(0,"_rt_accbuf_0");
			pRenderContext->ClearColor3ub(0,0,0);
			pRenderContext->ClearBuffers( true, true );

			CCamera *pCamera = GetCamera();
			int width, height;
			pCamera->GetViewPort( width, height );

			int nTargetWidth = min( width, pRT->GetActualWidth() );
			int nTargetHeight = min( height, pRT->GetActualHeight() );

			bool view_changed = false;

			Vector new_vp;
			pCamera->GetViewPoint( new_vp );

			if ( (pCamera->GetYaw() != m_fLastLPreviewAngles[0] ) ||
				 (pCamera->GetPitch() != m_fLastLPreviewAngles[1] ) ||
				 (pCamera->GetRoll() != m_fLastLPreviewAngles[2] ) ||
				 (m_nLastLPreviewHeight != height ) ||
				 (m_nLastLPreviewWidth != width ) ||
				 ( new_vp != m_LastLPreviewCameraPos ) ||
				 (pCamera->GetZoom() != m_fLastLPreviewZoom ) )
				view_changed = true;
			if (m_pView->m_bUpdateView && (m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED))
			{

				static bool did_dump=false;
				static float Last_SendTime=0;
				// now, lets create floatbms with the deferred rendering data, so we can pass it to the lpreview thread
				float newtime=Plat_FloatTime();
				if (( n_gbufs_queued < 1 ) && ( newtime-Last_SendTime > 1.0) )
				{
					SendShadowTriangles();
					SendLightList();							// send light list to render thread
					if ( view_changed )
					{
						m_fLastLPreviewAngles[0] = pCamera->GetYaw();
						m_fLastLPreviewAngles[1] = pCamera->GetPitch();
						m_fLastLPreviewAngles[2] = pCamera->GetRoll();
						m_LastLPreviewCameraPos = new_vp;
						m_fLastLPreviewZoom = pCamera->GetZoom();
						m_nLastLPreviewHeight = height;
						m_nLastLPreviewWidth = width;


						g_nBitmapGenerationCounter++;
						Last_SendTime=newtime;
						if (g_pLPreviewOutputBitmap)
							delete g_pLPreviewOutputBitmap;
						g_pLPreviewOutputBitmap = NULL;
						static char const *rts_to_transmit[]={"_rt_albedo","_rt_normal","_rt_position",
															  "_rt_flags" };
						MessageToLPreview Msg(LPREVIEW_MSG_G_BUFFERS);
						for(int i=0; i < NELEMS( rts_to_transmit ); i++)
						{
							SetRenderTargetNamed(0,rts_to_transmit[i]);
							FloatBitMap_t *fbm = new FloatBitMap_t( nTargetWidth, nTargetHeight );
							Msg.m_pDefferedRenderingBMs[i]=fbm;
							pRenderContext->ReadPixels(0, 0, nTargetWidth, nTargetHeight, (uint8 *) &(fbm->Pixel(0,0,0)),
												  IMAGE_FORMAT_RGBA32323232F);
							if ( (i==0) && (! did_dump) )
							{
								fbm->WriteTGAFile("albedo.tga");
							}
							if ( (i==1) && (! did_dump) )
							{
								fbm->WriteTGAFile("normal.tga");
							}
						}
						pRenderContext->SetRenderTarget( NULL );
						did_dump = true;
						n_gbufs_queued++;
						pCamera->GetViewPoint( Msg.m_EyePosition );
						Msg.m_nBitmapGenerationCounter=g_nBitmapGenerationCounter;
						g_HammerToLPreviewMsgQueue.QueueMessage( Msg );
					}
				}
			}			
			
			// only update non-ray traced lpreview if we have no ray traced one or if the scene has changed
			if (m_pView->m_bUpdateView || (m_eCurrentRenderMode != RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) || 
				(! g_pLPreviewOutputBitmap) )
			{
				SetRenderTargetNamed(0,"_rt_accbuf_0");
				pRenderContext->ClearColor3ub(0,0,0);
				MaterialSystemInterface()->ClearBuffers( true, true );

				
				pRenderContext->Viewport(0, 0, nTargetWidth, nTargetHeight );
				pRenderContext->ClearColor3ub(0,0,0);
				pRenderContext->ClearBuffers( true, true );
				// now, copy albedo to screen
				ITexture *dest_rt=materials->FindTexture("_rt_albedo", TEXTURE_GROUP_RENDER_TARGET );
				int xl,yl,dest_width,dest_height;
				pRenderContext->GetViewport( xl,yl,dest_width,dest_height);

				CMapDoc *pDoc = m_pView->GetMapDoc();
				CMapWorld *pWorld = pDoc->GetMapWorld();
			
				if ( !pWorld )
					return;
			
				// now, get list of lights
				CUtlVector<CLightingPreviewLightDescription> lightList;
				BuildLightList( &lightList );

				CUtlPriorityQueue<CLightPreview_Light> light_queue( 0, 0, CompareLightPreview_Lights);

				Vector eye_pnt;
				pCamera->GetViewPoint(eye_pnt);
				// now, add lights in priority order
				for( int i = 0; i < lightList.Count(); i++ )
				{
					LightDesc_t *pLight = &lightList[i];
					if (
						( pLight->m_Type == MATERIAL_LIGHT_SPOT ) ||
						( pLight->m_Type == MATERIAL_LIGHT_POINT ) )
					{
						Vector lpnt;
						CLightPreview_Light tmplight;
						tmplight.m_Light = *pLight;
						tmplight.m_flDistanceToEye = pLight->m_Position.DistTo( eye_pnt );
						light_queue.Insert(tmplight);
					}
				}
				if ( light_queue.Count() == 0 )
				{
					// no lights for gpu preview? lets add a fake one
					CLightPreview_Light tmplight;
					tmplight.m_Light.m_Type = MATERIAL_LIGHT_POINT;
					tmplight.m_Light.m_Color = Vector( 10, 10, 10 );
					tmplight.m_Light.m_Position = Vector( 0, 0, 30000 );
					tmplight.m_Light.m_Range = 1.0e20;
					tmplight.m_Light.m_Attenuation0 = 1.0;
					tmplight.m_Light.m_Attenuation1 = 0.0;
					tmplight.m_Light.m_Attenuation2 = 0.0;
					tmplight.m_flDistanceToEye = 1;
					light_queue.Insert(tmplight);
				}
				// because of no blend support on ati, we have to ping pong. This needs an nvidia-specifc
				// path for perf
				IMaterial *add_0_to_1=materials->FindMaterial("editor/addlight0",
															  TEXTURE_GROUP_OTHER,true);
				IMaterial *add_1_to_0=materials->FindMaterial("editor/addlight1",
															  TEXTURE_GROUP_OTHER,true);
				
				IMaterial *sample_last=materials->FindMaterial("editor/sample_result_0",
															   TEXTURE_GROUP_OTHER,true);
				IMaterial *sample_other=materials->FindMaterial("editor/sample_result_1",
																TEXTURE_GROUP_OTHER,true);

				ITexture *dest_rt_current=materials->FindTexture("_rt_accbuf_1", TEXTURE_GROUP_RENDER_TARGET );
				ITexture *dest_rt_other=materials->FindTexture("_rt_accbuf_0", TEXTURE_GROUP_RENDER_TARGET );
				pRenderContext->SetRenderTarget(dest_rt_other);
				pRenderContext->ClearColor3ub(0,0,0);
				pRenderContext->ClearBuffers( true, true );
				int nlights=min(MAX_PREVIEW_LIGHTS,light_queue.Count());
				for(int i=0;i<nlights;i++)
				{
					IMaterial *src_mat=add_0_to_1;
					LightDesc_t light = light_queue.ElementAtHead().m_Light;
					light.RecalculateDerivedValues();
					light_queue.RemoveAtHead();
					Vector lpnt = light.m_Position;
					SetNamedMaterialVar(src_mat,"$C0_X", lpnt.x);
					SetNamedMaterialVar(src_mat,"$C0_Y", lpnt.y );
					SetNamedMaterialVar(src_mat,"$C0_Z", lpnt.z );
					// now, get the facing direction.
					Vector spot_dir = light.m_Direction;
					SetNamedMaterialVar(src_mat,"$C1_X", spot_dir.x );
					SetNamedMaterialVar(src_mat,"$C1_Y", spot_dir.y );
					SetNamedMaterialVar(src_mat,"$C1_Z", spot_dir.z );
					
					// now, handle cone angle
					if ( light.m_Type == MATERIAL_LIGHT_POINT )
					{
						// model point as a spot with infinite inner radius
						SetNamedMaterialVar(src_mat, "$C0_W", 0.5 );
						SetNamedMaterialVar(src_mat, "$C1_W", 1.0e10 );
					}
					else
					{
						SetNamedMaterialVar(src_mat, "$C0_W", light.m_ThetaDot );
						SetNamedMaterialVar(src_mat, "$C1_W", light.m_PhiDot );
					}

					SetNamedMaterialVar( src_mat, "$C2_X", light.m_Attenuation2 );
					SetNamedMaterialVar( src_mat, "$C2_Y", light.m_Attenuation1 );
					SetNamedMaterialVar( src_mat, "$C2_Z", light.m_Attenuation0 );
					SetNamedMaterialVar( src_mat, "$C2_W", 1.0 );
				
					Vector color_intens = light.m_Color;
					SetNamedMaterialVar(src_mat, "$C3_X", color_intens.x);
					SetNamedMaterialVar(src_mat, "$C3_Y", color_intens.y);
					SetNamedMaterialVar(src_mat, "$C3_Z", color_intens.z);
				
					pRenderContext->SetRenderTarget(dest_rt_current);
					pRenderContext->DrawScreenSpaceRectangle(
						src_mat, 0, 0, nTargetWidth, nTargetHeight,
						0,0,
						nTargetWidth - 1, nTargetHeight -1,
						dest_rt->GetActualWidth(),
						dest_rt->GetActualHeight());
					V_swap(dest_rt_current,dest_rt_other);
					V_swap(sample_last,sample_other);
					V_swap(add_0_to_1,add_1_to_0);
				}
				pRenderContext->SetRenderTarget(NULL);
				pRenderContext->DrawScreenSpaceRectangle(
					sample_last, xl, yl, dest_width, dest_height,
					0,0,
					nTargetWidth, nTargetHeight,
					dest_rt->GetActualWidth(),
					dest_rt->GetActualHeight());
			
			}
		}
		MaterialSystemInterface()->SwapBuffers();

		if ( (m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) &&
			 g_pLPreviewOutputBitmap )
		{
			// blit it
			BITMAPINFOHEADER mybmh;
			mybmh.biHeight=-g_pLPreviewOutputBitmap->Height();
			mybmh.biSize=sizeof(BITMAPINFOHEADER);
			// now, set up bitmapheader struct for StretchDIB
			mybmh.biWidth=g_pLPreviewOutputBitmap->Width();
			mybmh.biPlanes=1;
			mybmh.biBitCount=32;
			mybmh.biCompression=BI_RGB;
			mybmh.biSizeImage=g_pLPreviewOutputBitmap->Width()*g_pLPreviewOutputBitmap->Height();

			RECT wrect;
			memset(&wrect,0,sizeof(wrect));
  
			CCamera *pCamera = GetCamera();
			int width, height;
			pCamera->GetViewPort( width, height );
// 			StretchDIBits(
// 				m_WinData.hDC,0,0,width,height,
// 				0,0,g_pLPreviewOutputBitmap->m_nWidth, g_pLPreviewOutputBitmap->m_nHeight,
// 				g_pLPreviewOutputBitmap->m_pBits, (BITMAPINFO *) &mybmh,
// 				DIB_RGB_COLORS, SRCCOPY);

			// remember that we blitted it
			m_pView->m_nLastRaytracedBitmapRenderTimeStamp = 
				GetUpdateCounter( EVTYPE_BITMAP_RECEIVED_FROM_LPREVIEW );
		}

		if (g_bShowStatistics)
		{
			//
			// Calculate frame rate.
			//
			if (m_dwTimeLastSample != 0)
			{
				DWORD dwTimeNow = timeGetTime();
				DWORD dwTimeElapsed = dwTimeNow - m_dwTimeLastSample;
				if ((dwTimeElapsed > 1000) && (m_nFramesThisSample > 0))
				{
					float fTimeElapsed = (float)dwTimeElapsed / 1000.0;
					m_fFrameRate = m_nFramesThisSample / fTimeElapsed;
					m_nFramesThisSample = 0;
					m_dwTimeLastSample = dwTimeNow;
				}
			}
			else
			{
				m_dwTimeLastSample = timeGetTime();
			}
		
			m_nFramesThisSample++;

			//
			// Display the frame rate and camera position.
			//
			char szText[100];
			Vector ViewPoint;
			GetCamera()->GetViewPoint(ViewPoint);
			int nLen = sprintf(szText, "FPS=%3.2f Pos=[%.f %.f %.f]", m_fFrameRate, ViewPoint[0], ViewPoint[1], ViewPoint[2]);
			TextOut(m_WinData.hDC, 2, 18, szText, nLen);
		}
	}
}


void CRender3D::PushInstanceData( CMapInstance *pInstanceClass, Vector &InstanceOrigin, QAngle &InstanceAngles )
{
	__super::PushInstanceData( pInstanceClass, InstanceOrigin, InstanceAngles );

	if ( m_bInstanceRendering )
	{
		CMapFace::PushFaceQueue();
	}
}

void CRender3D::PopInstanceData( void )
{
	if ( m_bInstanceRendering )
	{
		CMapFace::PopFaceQueue();
	}

	__super::PopInstanceData();
}

//-----------------------------------------------------------------------------
// Renders the world axes 
//-----------------------------------------------------------------------------
void CRender3D::RenderWorldAxes()
{
	// Render the world axes.
	PushRenderMode( RENDER_MODE_WIREFRAME );

	CMeshBuilder meshBuilder3D;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );
	meshBuilder3D.Begin( pMesh, MATERIAL_LINES, 3 );

	meshBuilder3D.Color3ub(255, 0, 0);
	meshBuilder3D.Position3f(0, 0, 0);
	meshBuilder3D.AdvanceVertex();

	meshBuilder3D.Color3ub(255, 0, 0);
	meshBuilder3D.Position3f(100, 0, 0);
	meshBuilder3D.AdvanceVertex();

	meshBuilder3D.Color3ub(0, 255, 0);
	meshBuilder3D.Position3f(0, 0, 0);
	meshBuilder3D.AdvanceVertex();

	meshBuilder3D.Color3ub(0, 255, 0);
	meshBuilder3D.Position3f(0, 100, 0);
	meshBuilder3D.AdvanceVertex();
	
	meshBuilder3D.Color3ub(0, 0, 255);
	meshBuilder3D.Position3f(0, 0, 0);
	meshBuilder3D.AdvanceVertex();

	meshBuilder3D.Color3ub(0, 0, 255);
	meshBuilder3D.Position3f(0, 0, 100);
	meshBuilder3D.AdvanceVertex();

	meshBuilder3D.End();
	pMesh->Draw();

	PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: this will handle all translucent rendering, including local transforms for instance items
// Input  : none
// Output : none
//-----------------------------------------------------------------------------
void CRender3D::RenderTranslucentObjects( void )
{
	CMatRenderContextPtr	pRenderContext( MaterialSystemInterface() );
	bool					bAddedTransform = false;
	CMapInstance			*pInstanceClass = NULL;
	TInstanceState			SaveInstanceState = m_CurrentInstanceState;

	m_bInstanceRendering = false;

	// render translucent objects after all opaque objects
	while ( m_TranslucentRenderObjects.Count() > 0 )
	{
		TranslucentObjects_t current = m_TranslucentRenderObjects.ElementAtHead();
		m_TranslucentRenderObjects.RemoveAtHead();	

		if ( current.m_InstanceState.m_pInstanceClass )
		{
			if ( pInstanceClass != current.m_InstanceState.m_pInstanceClass || !m_bInstanceRendering || current.m_InstanceState.m_InstanceMatrix != m_CurrentInstanceState.m_InstanceMatrix  )
			{
				if ( bAddedTransform )
				{
					EndLocalTransfrom();
				}
				bAddedTransform = true;
				BeginLocalTransfrom( current.m_InstanceState.m_InstanceRenderMatrix, false );
				m_CurrentInstanceState = current.m_InstanceState;
				pInstanceClass = m_CurrentInstanceState.m_pInstanceClass;
				m_bInstanceRendering = true;
	
				if ( pInstanceClass->IsEditable() )
				{
					SetInstanceRendering( INSTANCE_STATE_OFF );
				}
				else
				{
					SetInstanceRendering( current.m_bInstanceSelected ? INSTANCE_STACE_SELECTED : INSTANCE_STATE_ON );
				}
			}
		}
		else
		{
			if ( m_bInstanceRendering )
			{
				if ( bAddedTransform )
				{
					EndLocalTransfrom();
					bAddedTransform = false;
				}

				SetInstanceRendering( INSTANCE_STATE_OFF );
				m_bInstanceRendering = false;
			}
		}

		current.object->Render3D( this );
	}

	m_bInstanceRendering = false;
	if ( bAddedTransform )
	{
		EndLocalTransfrom();
	}

	m_CurrentInstanceState = SaveInstanceState;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRender3D::Render(void)
{
	CMapDoc		*pDoc = m_pView->GetMapDoc();
	CMapWorld	*pMapWorld = pDoc->GetMapWorld();
	CManifest	*pManifest = pDoc->GetManifest();

	bool view_changed = false;
	
	CCamera *pCamera = GetCamera();
	Vector new_vp;
	pCamera->GetViewPoint( new_vp );
	int width, height;
	pCamera->GetViewPort( width, height );

	if ( GetMainWnd()->m_pLightingPreviewOutputWindow)
	{
		SendLightList();									// nop if nothing changed
		SendShadowTriangles();								// nop if nothing changed
	}

	if ( (pCamera->GetYaw() != m_fLastLPreviewAngles[0] ) ||
		 (pCamera->GetPitch() != m_fLastLPreviewAngles[1] ) ||
		 (pCamera->GetRoll() != m_fLastLPreviewAngles[2] ) ||
		 (m_nLastLPreviewHeight != height ) ||
		 (m_nLastLPreviewWidth != width ) ||
		 ( new_vp != m_LastLPreviewCameraPos ) ||
		 (pCamera->GetZoom() != m_fLastLPreviewZoom ) )
		view_changed = true;

	if ( (m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) &&
		 g_pLPreviewOutputBitmap &&
		 (! view_changed ) )
	{
		// blit it
		BITMAPINFOHEADER mybmh;
		mybmh.biHeight=-g_pLPreviewOutputBitmap->Height();
		mybmh.biSize=sizeof(BITMAPINFOHEADER);
		// now, set up bitmapheader struct for StretchDIB
		mybmh.biWidth=g_pLPreviewOutputBitmap->Width();
		mybmh.biPlanes=1;
		mybmh.biBitCount=32;
		mybmh.biCompression=BI_RGB;
		mybmh.biSizeImage=g_pLPreviewOutputBitmap->Width()*g_pLPreviewOutputBitmap->Height();

		RECT wrect;
		memset(&wrect,0,sizeof(wrect));
  
		pCamera->GetViewPort( width, height );
// 		StretchDIBits(
// 			m_WinData.hDC,0,0,width,height,
// 			0,0,g_pLPreviewOutputBitmap->m_nWidth, g_pLPreviewOutputBitmap->m_nHeight,
// 			g_pLPreviewOutputBitmap->m_pBits, (BITMAPINFO *) &mybmh,
// 			DIB_RGB_COLORS, SRCCOPY);
		m_pView->m_nLastRaytracedBitmapRenderTimeStamp = 
			GetUpdateCounter( EVTYPE_BITMAP_RECEIVED_FROM_LPREVIEW );
//		return;
	}

	StartRenderFrame();
	
	if (
		( m_eCurrentRenderMode != RENDER_MODE_LIGHT_PREVIEW2 ) &&
		( m_eCurrentRenderMode != RENDER_MODE_LIGHT_PREVIEW_RAYTRACED )
		)
		RenderWorldAxes();

	//
	// Deferred rendering lets us sort everything here by material.
	//
	if (!IsPicking())
	{
		m_DeferRendering = true;
	}

	m_TranslucentSortRendering = true;

// 	if (IsInLightingPreview())
// 	{
// 		// Lighting preview?
// 		IBSPLighting *pBSPLighting = pDoc->GetBSPLighting();
// 		if (pBSPLighting)
// 		{
// 			pBSPLighting->Draw();
// 		}
// 	}

	//
	// Render the world using octree culling.
	//

	PrepareInstanceStencil();

	if ( pManifest )
	{
 		pMapWorld = pManifest->GetManifestWorld();
	}

	if (g_bUseCullTree)
	{
		RenderTree( pMapWorld );
	}
	//
	// Render the world without octree culling.
	//
	else
	{
		RenderMapClass( pMapWorld );
	}

	if ( m_DeferRendering )
	{
		m_DeferRendering = false;

		// An optimization... render tree doesn't actually render anythung
		// This here will do the rendering, sorted by material by pass
		CMapFace::RenderOpaqueFaces(this);
	}

	RenderTranslucentObjects();

	DrawInstanceStencil();

	m_TranslucentSortRendering = false;
	pDoc->RenderDocument( this );

	RenderTool();

	RenderPointsAndPortals();

    
#ifdef _DEBUG
	if (m_bRenderFrustum)
	{
		RenderFrustum();
	}
#endif

	//
	// Render any 2D elements that overlay the 3D view, like a center crosshair.
	//
	RenderOverlayElements();

	EndRenderFrame();

	// Purge any translucent detail objects that were added AFTER the translucent rendering loop
	if ( m_TranslucentRenderObjects.Count() )
		m_TranslucentRenderObjects.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: render an arrow of a given color at a given position (start and end)
//          in world space
// Input  : vStartPt - the arrow starting point
//          vEndPt - the arrow ending point (the head of the arrow)
//          chRed, chGree, chBlue - the arrow color
//-----------------------------------------------------------------------------
void CRender3D::RenderArrow( Vector const &vStartPt, Vector const &vEndPt, 
							   unsigned char chRed, unsigned char chGreen, unsigned char chBlue )
{
	//
	// render the stick portion of the arrow
	//

	// set to a flat shaded render mode
	PushRenderMode( RENDER_MODE_FLAT );
	SetDrawColor( chRed, chGreen, chBlue );
	DrawLine( vStartPt, vEndPt );
	PopRenderMode();

	//
	// render the tip of the arrow
	//
	Vector coneAxis = vEndPt - vStartPt;
	float length = VectorNormalize( coneAxis );
	float length8 = length * 0.125;
	length -= length8;
	
	Vector vBasePt;
	vBasePt = vStartPt + coneAxis * length;

	RenderCone( vBasePt, vEndPt, ( length8 * 0.333 ), 6, chRed, chGreen, chBlue );
}


//-----------------------------------------------------------------------------
// Purpose: Renders a box in flat shaded or wireframe depending on our render mode.
// Input  : chRed - 
//			chGreen - 
//			chBlue - 
//-----------------------------------------------------------------------------
void CRender3D::RenderBox(const Vector &Mins, const Vector &Maxs,
							unsigned char chRed, unsigned char chGreen, unsigned char chBlue, SelectionState_t eBoxSelectionState)
{
	Vector FacePoints[8];

	PointsFromBox( Mins, Maxs, FacePoints );

	int nFaces[6][4] =
		{
			{ 0, 2, 3, 1 },
			{ 0, 1, 5, 4 },
			{ 4, 5, 7, 6 },
			{ 2, 6, 7, 3 },
			{ 1, 3, 7, 5 },
			{ 0, 4, 6, 2 }
		};

	EditorRenderMode_t eRenderModeThisPass;
	int nPasses;

	if ((eBoxSelectionState != SELECT_NONE) && (GetDefaultRenderMode() != RENDER_MODE_WIREFRAME))
	{
		nPasses = 2;
	}
	else
	{
		nPasses = 1;
	}

	for (int nPass = 1; nPass <= nPasses; nPass++)
	{
		if (nPass == 1)
		{
			eRenderModeThisPass = GetDefaultRenderMode();

			// There's no texture for a bounding box.
			if ((eRenderModeThisPass == RENDER_MODE_TEXTURED) ||
				(eRenderModeThisPass == RENDER_MODE_TEXTURED_SHADED) ||
				(eRenderModeThisPass == RENDER_MODE_LIGHT_PREVIEW2) ||
				(eRenderModeThisPass == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) ||
				(eRenderModeThisPass == RENDER_MODE_LIGHTMAP_GRID))
			{
				eRenderModeThisPass = RENDER_MODE_FLAT;
			}

			PushRenderMode(eRenderModeThisPass);
		}
		else
		{
			eRenderModeThisPass = RENDER_MODE_WIREFRAME;
			PushRenderMode(eRenderModeThisPass);
		}

		for (int nFace = 0; nFace < 6; nFace++)
		{
			Vector Edge1, Edge2, Normal;
			int nP1, nP2, nP3, nP4;

			nP1 = nFaces[nFace][0];
			nP2 = nFaces[nFace][1];
			nP3 = nFaces[nFace][2];
			nP4 = nFaces[nFace][3];

			VectorSubtract(FacePoints[nP4], FacePoints[nP1], Edge1);
			VectorSubtract(FacePoints[nP2], FacePoints[nP1], Edge2);
			CrossProduct(Edge1, Edge2, Normal);
			VectorNormalize(Normal);

			//
			// If we are rendering using one of the lit modes, calculate lighting.
			// 
			unsigned char color[3];

			assert( (eRenderModeThisPass != RENDER_MODE_TEXTURED) &&
					(eRenderModeThisPass != RENDER_MODE_TEXTURED_SHADED) && 
					(eRenderModeThisPass != RENDER_MODE_LIGHT_PREVIEW2) && 
					(eRenderModeThisPass != RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) && 
					(eRenderModeThisPass != RENDER_MODE_LIGHTMAP_GRID) ); 
			if ((eRenderModeThisPass == RENDER_MODE_FLAT))
			{
				float fShade = LightPlane(Normal);

				//
				// For flat and textured mode use the face color with lighting.
				//
				if (eBoxSelectionState != SELECT_NONE)
				{
					color[0] = SELECT_FACE_RED * fShade;
					color[1] = SELECT_FACE_GREEN * fShade;
					color[2] = SELECT_FACE_BLUE * fShade;
				}
				else
				{
					color[0] = chRed * fShade;
					color[1] = chGreen * fShade;
					color[2] = chBlue * fShade;
				}
			}
			//
			// For wireframe mode use the face color without lighting.
			//
			else
			{
				if (eBoxSelectionState != SELECT_NONE)
				{
					color[0] = SELECT_FACE_RED;
					color[1] = SELECT_FACE_GREEN;
					color[2] = SELECT_FACE_BLUE;
				}
				else
				{
					color[0] = chRed;
					color[1] = chGreen;
					color[2] = chBlue;
				}
			}

			//
			// Draw the face.
			//
			bool wireframe = (eRenderModeThisPass == RENDER_MODE_WIREFRAME);

			CMeshBuilder meshBuilder3D;
			CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
			IMesh* pMesh = pRenderContext->GetDynamicMesh();
			meshBuilder3D.DrawQuad( pMesh, FacePoints[nP1].Base(), FacePoints[nP2].Base(),
								  FacePoints[nP3].Base(), FacePoints[nP4].Base(), color, wireframe );
		}

		PopRenderMode();
	}
}


//-----------------------------------------------------------------------------
// Purpose: render a cone of a given color at a given position in world space
// Intput : vBasePt - the start point of the cone (the base point)
//          vTipPt - the end point of the cone (the peak)
//          fRadius - the radius (at the base) of the cone
//          nSlices - the number of slices (segments) making up the cone
//          chRed, chGreen, chBlue - the cone color
//-----------------------------------------------------------------------------
void CRender3D::RenderCone( Vector const &vBasePt, Vector const &vTipPt, float fRadius, int nSlices,
		                      unsigned char chRed, unsigned char chGreen, unsigned char chBlue )
{
	// get the angle between slices (in radians)
	float sliceAngle = ( 2 * M_PI ) / ( float )nSlices;

	//
	// allocate ALIGNED!!!!!!! vectors for cone base
	//
	int size = nSlices * sizeof( Vector );
	size += 16 + sizeof( Vector* );
	byte *ptr = ( byte* )_alloca( size );
	long data = ( long )ptr;
	
	data += 16 + sizeof( Vector* ) - 1;
	data &= -16;

	(( void** )data)[-1] = ptr;

	Vector *pPts = ( Vector* )data;
	if( !pPts )
		return;

	//
	// calculate the cone's base points in a local space (x,y plane)
	//
	for( int i = 0; i < nSlices; i++ )
	{
		pPts[i].x = fRadius * cos( ( sliceAngle * -i ) );
		pPts[i].y = fRadius * sin( ( sliceAngle * -i ) );
		pPts[i].z = 0.0f;
	}

	//
	// get cone tip in local space
	//
	Vector coneAxis = vTipPt - vBasePt;
	float length = coneAxis.Length();
	Vector tipPt( 0.0f, 0.0f, length );

	//
	// create cone faces
	//
	CMapFaceList m_Faces;
	Vector ptList[3];
	
	// triangulate the base
	for( int i = 0; i < ( nSlices - 2 ); i++ )
	{	
		ptList[0] = pPts[0];
		ptList[1] = pPts[i+1];
		ptList[2] = pPts[i+2];

		// add face to list
		CMapFace *pFace = new CMapFace;
		if( !pFace )
			return;
		pFace->SetRenderColor( chRed, chGreen, chBlue );
		pFace->CreateFace( ptList, 3 );
		pFace->RenderUnlit( true );
		m_Faces.AddToTail( pFace );
	}

	// triangulate the sides
	for( int i = 0; i < nSlices; i++ )
	{
		ptList[0] = pPts[i];
		ptList[1] = tipPt;
		ptList[2] = pPts[(i+1)%nSlices];

		// add face to list
		CMapFace *pFace = new CMapFace;
		if( !pFace )
			return;
		pFace->SetRenderColor( chRed, chGreen, chBlue );
		pFace->CreateFace( ptList, 3 );
		pFace->RenderUnlit( true );
		m_Faces.AddToTail( pFace );
	}

	//
	// rotate base points into world space as they are being rendered
	//
	VectorNormalize( coneAxis );
	QAngle rotAngles;
	VectorAngles( coneAxis, rotAngles );
	rotAngles[PITCH] += 90;

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	pRenderContext->MatrixMode( MATERIAL_MODEL ); 
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->Translate( vBasePt.x, vBasePt.y, vBasePt.z );

	pRenderContext->Rotate( rotAngles[YAW], 0, 0, 1 );
	pRenderContext->Rotate( rotAngles[PITCH], 0, 1, 0 );
	pRenderContext->Rotate( rotAngles[ROLL], 1, 0, 0 );

	// set to a flat shaded render mode
	PushRenderMode( RENDER_MODE_FLAT );

	for ( int i = 0; i < m_Faces.Count(); i++ )
	{
		CMapFace *pFace = m_Faces.Element( i );
		if( !pFace )
			continue;
		pFace->Render3D( this );
	}

	pRenderContext->PopMatrix();

	// set back to default render mode
	PopRenderMode();

	//
	// delete the faces in the list
	//
	for ( int i = 0; i < m_Faces.Count(); i++ )
	{
		CMapFace *pFace = m_Faces.Element( i );
		delete pFace;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : vCenter - 
//			flRadius - 
//			nTheta - Number of vertical slices in the sphere.
//			nPhi - Number of horizontal slices in the sphere.
//			chRed - 
//			chGreen - 
//			chBlue - 
//-----------------------------------------------------------------------------
void CRender3D::RenderSphere(Vector const &vCenter, float flRadius, int nTheta, int nPhi,
							   unsigned char chRed, unsigned char chGreen, unsigned char chBlue )
{
	PushRenderMode( RENDER_MODE_EXTERN );

	int nTriangles =  2 * nTheta * ( nPhi - 1 ); // Two extra degenerate triangles per row (except the last one)
	int nIndices = 2 * ( nTheta + 1 ) * ( nPhi - 1 );

	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	pRenderContext->Bind( m_pVertexColor[0] );

	CMeshBuilder meshBuilder3D;
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder3D.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, nTriangles, nIndices );

	//
	// Build the index buffer.
	//
	int i, j;
	for ( i = 0; i < nPhi; ++i )
	{
		for ( j = 0; j < nTheta; ++j )
		{
			float u = j / ( float )( nTheta - 1 );
			float v = i / ( float )( nPhi - 1 );
			float theta = 2.0f * M_PI * u;
			float phi = M_PI * v;

			Vector vecPos;
			vecPos.x = flRadius * sin(phi) * cos(theta);
			vecPos.y = flRadius * sin(phi) * sin(theta); 
			vecPos.z = flRadius * cos(phi);

			Vector vecNormal = vecPos;
			VectorNormalize(vecNormal);

			float flScale = LightPlane(vecNormal);

			unsigned char red = chRed * flScale;
			unsigned char green = chGreen * flScale;
			unsigned char blue = chBlue * flScale;

			vecPos += vCenter;

			meshBuilder3D.Position3f( vecPos.x, vecPos.y, vecPos.z );
			meshBuilder3D.Color3ub( red, green, blue );
			meshBuilder3D.AdvanceVertex();
		}
	}

	//
	// Emit the triangle strips.
	//
	int idx = 0;
	for ( i = 0; i < nPhi - 1; ++i )
	{
		for ( j = 0; j < nTheta; ++j )
		{
			idx = nTheta * i + j;

			meshBuilder3D.Index( idx + nTheta );
			meshBuilder3D.AdvanceIndex();

			meshBuilder3D.Index( idx );
			meshBuilder3D.AdvanceIndex();
		}

		//
		// Emit a degenerate triangle to skip to the next row without
		// a connecting triangle.
		//
		if ( i < nPhi - 2 )
		{
			meshBuilder3D.Index( idx );
			meshBuilder3D.AdvanceIndex();

			meshBuilder3D.Index( idx + nTheta + 1 );
			meshBuilder3D.AdvanceIndex();
		}
	}

	meshBuilder3D.End();
	pMesh->Draw();
	
	PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRender3D::RenderWireframeSphere(Vector const &vCenter, float flRadius, int nTheta, int nPhi,
							            unsigned char chRed, unsigned char chGreen, unsigned char chBlue )
{
	PushRenderMode(RENDER_MODE_WIREFRAME);

	// Make one more coordinate because (u,v) is discontinuous.
	++nTheta;

	int nVertices = nPhi * nTheta; 
	int nIndices = ( nTheta - 1 ) * 4 * ( nPhi - 1 );

	CMeshBuilder meshBuilder3D;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder3D.Begin( pMesh, MATERIAL_LINES, nVertices, nIndices );

	for ( int i = 0; i < nPhi; ++i )
	{
		for ( int j = 0; j < nTheta; ++j )
		{
			float u = j / ( float )( nTheta - 1 );
			float v = i / ( float )( nPhi - 1 );
			float theta = 2.0f * M_PI * u;
			float phi = M_PI * v;
			meshBuilder3D.Position3f( vCenter.x + ( flRadius * sin(phi) * cos(theta) ),
				                    vCenter.y + ( flRadius * sin(phi) * sin(theta) ), 
									vCenter.z + ( flRadius * cos(phi) ) );
			meshBuilder3D.Color3ub( chRed, chGreen, chBlue );
			meshBuilder3D.AdvanceVertex();
		}
	}

	for ( int i = 0; i < nPhi - 1; ++i )
	{
		for ( int j = 0; j < nTheta - 1; ++j )
		{
			int idx = nTheta * i + j;

			meshBuilder3D.Index( idx );
			meshBuilder3D.AdvanceIndex();

			meshBuilder3D.Index( idx + nTheta );
			meshBuilder3D.AdvanceIndex();

			meshBuilder3D.Index( idx );
			meshBuilder3D.AdvanceIndex();

			meshBuilder3D.Index( idx + 1 );
			meshBuilder3D.AdvanceIndex();
		}
	}

	meshBuilder3D.End();
	pMesh->Draw();

	PopRenderMode();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDrawDC - 
//-----------------------------------------------------------------------------
void CRender3D::RenderPointsAndPortals(void)
{
	CMapDoc *pDoc = m_pView->GetMapDoc();

	if ( pDoc->m_PFPoints.Count() )
	{
		PushRenderMode(RENDER_MODE_WIREFRAME);

		int nPFPoints = pDoc->m_PFPoints.Count();
		Vector* pPFPoints = pDoc->m_PFPoints.Base();
		CMeshBuilder meshBuilder3D;
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		IMesh* pMesh = pRenderContext->GetDynamicMesh( );
		meshBuilder3D.Begin( pMesh, MATERIAL_LINE_STRIP, nPFPoints - 1 );

		for (int i = 0; i < nPFPoints; i++)
		{
			meshBuilder3D.Position3f(pPFPoints[i][0], pPFPoints[i][1], pPFPoints[i][2]);
			meshBuilder3D.Color3ub(255, 0, 0);
			meshBuilder3D.AdvanceVertex();
		}

		meshBuilder3D.End();
		pMesh->Draw();
		PopRenderMode();
	}

	// draw any portal file that was loaded
	if ( pDoc->m_pPortalFile )
	{
		PushRenderMode(RENDER_MODE_FLAT_NOCULL);

		// each vert makes and edge and thus a quad
		int totalQuads = pDoc->m_pPortalFile->totalVerts;
		int nMaxVerts;
		int nMaxIndices;
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		pRenderContext->GetMaxToRender( pRenderContext->GetDynamicMesh( ), false, &nMaxVerts, &nMaxIndices );

		int portalIndex = 0;
		int baseVert = 0;
		while ( totalQuads > 0 )
		{
			int quadLimit = totalQuads;
			int quadOut = 0;
			IMesh* pMesh = pRenderContext->GetDynamicMesh( );
			if ( (quadLimit * 4) > nMaxVerts )
			{
				quadLimit = nMaxVerts / 4;
			}
			if ( (quadLimit * 6) > nMaxIndices )
			{
				quadLimit = nMaxIndices / 6;
			}
			CMeshBuilder meshBuilder3D;
			meshBuilder3D.Begin( pMesh, MATERIAL_QUADS, quadLimit );

			const float edgeWidth = 2.0f;
			for (; portalIndex < pDoc->m_pPortalFile->vertCount.Count(); portalIndex++)
			{
				int vertCount = pDoc->m_pPortalFile->vertCount[portalIndex];

				if ( (quadOut + vertCount) > quadLimit )
					break;
				quadOut += vertCount;
				// compute a face normal
				Vector e0 = pDoc->m_pPortalFile->verts[baseVert+1] - pDoc->m_pPortalFile->verts[baseVert];
				Vector e1 = pDoc->m_pPortalFile->verts[baseVert+2] - pDoc->m_pPortalFile->verts[baseVert];
				Vector normal = CrossProduct( e1, e0 );
				VectorNormalize(normal);
				for ( int j = 0; j < vertCount; j++ )
				{
					int v0 = baseVert + j;
					int v1 = baseVert + ((j+1) % vertCount);
					// compute the direction in the plane of the face to extrude the edge toward the
					// face interior, use that to make a wide line with a quad
					e0 = pDoc->m_pPortalFile->verts[v1] - pDoc->m_pPortalFile->verts[v0];
					Vector dir = CrossProduct( e0, normal );
					VectorNormalize(dir);
					dir *= edgeWidth;
					meshBuilder3D.Position3fv( pDoc->m_pPortalFile->verts[v0].Base() );
					meshBuilder3D.Color3ub(0, 0, 255);
					meshBuilder3D.AdvanceVertex();
					meshBuilder3D.Position3fv( pDoc->m_pPortalFile->verts[v1].Base() );
					meshBuilder3D.Color3ub(0, 0, 255);
					meshBuilder3D.AdvanceVertex();
					meshBuilder3D.Position3fv( (pDoc->m_pPortalFile->verts[v1] + dir).Base() );
					meshBuilder3D.Color3ub(0, 0, 255);
					meshBuilder3D.AdvanceVertex();
					meshBuilder3D.Position3fv( (pDoc->m_pPortalFile->verts[v0] + dir).Base() );
					meshBuilder3D.Color3ub(0, 0, 255);
					meshBuilder3D.AdvanceVertex();
				}
				baseVert += vertCount;
			}

			meshBuilder3D.End();
			pMesh->Draw();
			totalQuads -= quadOut;
		}
		PopRenderMode();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draws a wireframe box using the given color.
// Input  : pfMins - Pointer to the box minima in all 3 dimensions.
//			pfMins - Pointer to the box maxima in all 3 dimensions.
//			chRed, chGreen, chBlue - Red, green, and blue color compnents for the box.
//-----------------------------------------------------------------------------
void CRender3D::RenderWireframeBox(const Vector &Mins, const Vector &Maxs,
									 unsigned char chRed, unsigned char chGreen, unsigned char chBlue)
{
	//
	// Draw the box bottom, top, and one corner edge.
	//

	PushRenderMode( RENDER_MODE_WIREFRAME );
	SetDrawColor( chRed, chGreen, chBlue );
	DrawBox( Mins, Maxs );
	PopRenderMode();
}


//-----------------------------------------------------------------------------
// Purpose: Renders this object (and all of its children) if it is visible and
//			has not already been rendered this frame.
// Input  : pMapClass - Pointer to the object to be rendered.
//-----------------------------------------------------------------------------
void CRender3D::RenderMapClass(CMapClass *pMapClass)
{
	Assert(pMapClass != NULL);

	if ((pMapClass != NULL) && (pMapClass->GetRenderFrame() != m_nFrameCount))
	{
		if (pMapClass->IsVisible())
		{
			//
			// Render this object's culling box if it is enabled.
			//
			if (g_bRenderCullBoxes)
			{
				Vector mins,maxs;
				pMapClass->GetCullBox(mins, maxs);

				RenderWireframeBox(mins, maxs, 255, 0, 0);
			}

			bool should_appear=true;
			if (m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW2)
				should_appear &= pMapClass->ShouldAppearInLightingPreview();

			if (m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED)
				should_appear &= pMapClass->ShouldAppearInLightingPreview();
//				should_appear &= pMapClass->ShouldAppearInRaytracedLightingPreview();

			if ( should_appear == true && m_Pick.bPicking == true && ( m_Pick.m_nFlags & FLAG_OBJECTS_AT_ONLY_SOLIDS ) != 0 )
			{
				if ( pMapClass->IsSolid() == false )
				{
					should_appear = false;
				}
			}


			if ( should_appear )
			{
				//
				// If we should render this object after all the other objects,
				// just add it to a list of objects to render last. Otherwise, render it now.
				//
				if (!pMapClass->ShouldRenderLast())
				{
					pMapClass->Render3D(this);
				}
				else
				{
					if (
						(m_eCurrentRenderMode != RENDER_MODE_LIGHT_PREVIEW2) &&
						(m_eCurrentRenderMode != RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) )
					{
						AddTranslucentDeferredRendering( pMapClass );
					}
				}
			}
			//
			// Render this object's children.
			//
			const CMapObjectList *pChildren = pMapClass->GetChildren();
			
			FOR_EACH_OBJ( *pChildren, pos )
			{
				Vector vecMins,vecMaxs;
				
				CMapClass *pChild = pChildren->Element(pos);

				pChild->GetCullBox(vecMins, vecMaxs);

				if (IsBoxVisible(vecMins, vecMaxs) != VIS_NONE )
				{
					RenderMapClass(pChild);
				}
			}
		}

		//
		// Consider this object as handled for this frame.
		//
		pMapClass->SetRenderFrame(m_nFrameCount);
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will render an instance map at the specific offset and rotation
// Input  : pInstanceClass - the map class of the func_instance
//			pMapClass - the map class of the world spawn of the instance
//			InstanceOrigin - the translation offset
//			InstanceAngles - the axis rotation
// Output : none
//-----------------------------------------------------------------------------
void CRender3D::RenderInstanceMapClass( CMapInstance *pInstanceClass, CMapClass *pMapClass, Vector &InstanceOrigin, QAngle &InstanceAngles )
{
	if ( !pInstanceClass->IsInstanceVisible() )
	{
		return;
	}

	PushInstanceData( pInstanceClass, InstanceOrigin, InstanceAngles );

	m_nInstanceCount++;

	RenderInstanceMapClass_r( pMapClass );

	if ( m_DeferRendering )
	{
		CMapFace::RenderOpaqueFaces(this);
	}

	if ( m_TranslucentSortRendering == false )
	{	// translucent objects will do their own transforms
		RenderTranslucentObjects();
	}

	PopInstanceData();
}


//-----------------------------------------------------------------------------
// Purpose: this function will recursively render an instance and all of its children
// Input  : pObject - the object to be rendered
// Output : none
//-----------------------------------------------------------------------------
void CRender3D::RenderInstanceMapClass_r(CMapClass *pMapClass)
{
	Assert(pMapClass != NULL);

	if ( ( pMapClass != NULL ) && ( pMapClass->GetRenderFrame() != m_nInstanceCount ) )
	{
		if (pMapClass->IsVisible())
		{
			//
			// Render this object's culling box if it is enabled.
			//
			if (g_bRenderCullBoxes)
			{
				Vector vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs;
				pMapClass->GetCullBox( vecMins, vecMaxs );
				TransformInstanceAABB( vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs );

				RenderWireframeBox( vecExpandedMins, vecExpandedMaxs, 255, 0, 0 );
			}

			bool should_appear=true;
			if (m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW2)
				should_appear &= pMapClass->ShouldAppearInLightingPreview();

			if (m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED)
				should_appear &= pMapClass->ShouldAppearInLightingPreview();
			//				should_appear &= pMapClass->ShouldAppearInRaytracedLightingPreview();

			if ( should_appear )
			{
				//
				// If we should render this object after all the other objects,
				// just add it to a list of objects to render last. Otherwise, render it now.
				//
				if (!pMapClass->ShouldRenderLast())
				{
					pMapClass->Render3D(this);
				}
				else
				{
					if (
						(m_eCurrentRenderMode != RENDER_MODE_LIGHT_PREVIEW2) &&
						(m_eCurrentRenderMode != RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) )
					{
						AddTranslucentDeferredRendering( pMapClass );
					}
				}
			}
			//
			// Render this object's children.
			//
			const CMapObjectList *pChildren = pMapClass->GetChildren();

			FOR_EACH_OBJ( *pChildren, pos )
			{
				Vector vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs;

				CMapClass *pChild = pChildren->Element( pos );

				pChild->GetCullBox( vecMins, vecMaxs );
				TransformInstanceAABB( vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs );

				if (IsBoxVisible( vecExpandedMins, vecExpandedMaxs ) != VIS_NONE )
				{
					RenderInstanceMapClass_r( pChild );
				}
			}
		}

		//
		// Consider this object as handled for this instance frame.
		//
		pMapClass->SetRenderFrame( m_nInstanceCount );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Prepares all objects in this node for rendering.
// Input  : pParent - 
//-----------------------------------------------------------------------------
void CRender3D::Preload(CMapClass *pParent)
{
	Assert(pParent != NULL);

	if (pParent != NULL)
	{
		//
		// Preload this object's children.
		//
		const CMapObjectList *pChildren = pParent->GetChildren();
		FOR_EACH_OBJ( *pChildren, pos )
		{
			pChildren->Element(pos)->RenderPreload(this, true);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Renders all objects in this node if this node is visible.
// Input  : pNode - The node to render.
//			bForce - If true, don't check for visibility, just render the node
//				and all of its children.
//-----------------------------------------------------------------------------
void CRender3D::RenderNode(CCullTreeNode *pNode, bool bForce )
{
	//
	// Render all child nodes first.
	//
	CCullTreeNode *pChild;
	int nChildren = pNode->GetChildCount();
	if (nChildren != 0)
	{
		for (int nChild = 0; nChild < nChildren; nChild++)
		{
			pChild = pNode->GetCullTreeChild(nChild);
			Assert(pChild != NULL);

			if (pChild != NULL)
			{
				//
				// Only bother checking nodes with children or objects.
				//
				if ((pChild->GetChildCount() != 0) || (pChild->GetObjectCount() != 0))
				{
					bool bForceThisChild = bForce;
					Visibility_t eVis = VIS_NONE;

					if (!bForceThisChild)
					{
						Vector vecMins;
						Vector vecMaxs;
						pChild->GetBounds(vecMins, vecMaxs);
						eVis = IsBoxVisible(vecMins, vecMaxs);
						if (eVis == VIS_TOTAL)
						{
							bForceThisChild = true;
						}
					}

					if ((bForceThisChild) || (eVis != VIS_NONE))
					{
						RenderNode(pChild, bForceThisChild);
					}
				}
			}
		}
	}
	else
	{
		//
		// Now render the contents of this node.
		//
		CMapClass *pObject;
		int nObjects = pNode->GetObjectCount();
		for (int nObject = 0; nObject < nObjects; nObject++)
		{
			pObject = pNode->GetCullTreeObject(nObject);
			Assert(pObject != NULL);

			Vector vecMins;
			Vector vecMaxs;
			pObject->GetCullBox(vecMins, vecMaxs);
			if (IsBoxVisible(vecMins, vecMaxs) != VIS_NONE)
			{
				RenderMapClass(pObject);
			}
		}
	}
}


void CRender3D::RenderCrossHair()
{
	int width, height;

	GetCamera()->GetViewPort( width, height );

	int nCenterX = width / 2;
	int nCenterY = height / 2;

	Assert( IsInClientSpace() );

	// Render the world axes
	PushRenderMode( RENDER_MODE_FLAT_NOZ );

	SetDrawColor(0,0,0);
   
	DrawLine(	Vector(nCenterX - CROSSHAIR_DIST_HORIZONTAL, nCenterY - 1, 0),
				Vector(nCenterX + CROSSHAIR_DIST_HORIZONTAL + 1, nCenterY - 1, 0) );

	DrawLine(	Vector(nCenterX - CROSSHAIR_DIST_HORIZONTAL, nCenterY + 1, 0),
				Vector(nCenterX + CROSSHAIR_DIST_HORIZONTAL + 1, nCenterY + 1, 0) );

	DrawLine(	Vector(nCenterX - 1, nCenterY - CROSSHAIR_DIST_VERTICAL, 0),
				Vector(nCenterX - 1, nCenterY + CROSSHAIR_DIST_VERTICAL, 0) );

	DrawLine(	Vector(nCenterX + 1, nCenterY - CROSSHAIR_DIST_VERTICAL, 0),
				Vector(nCenterX + 1, nCenterY + CROSSHAIR_DIST_VERTICAL, 0) );

	SetDrawColor(255,255,255);
	
	DrawLine(	Vector(nCenterX - CROSSHAIR_DIST_HORIZONTAL, nCenterY, 0),
				Vector(nCenterX + CROSSHAIR_DIST_HORIZONTAL + 1, nCenterY, 0) );

	DrawLine(	Vector(nCenterX, nCenterY - CROSSHAIR_DIST_VERTICAL, 0),
				Vector(nCenterX, nCenterY + CROSSHAIR_DIST_VERTICAL, 0) );

	PopRenderMode();
}

//-----------------------------------------------------------------------------
// Purpose: Renders 2D elements that overlay the 3D objects.
//-----------------------------------------------------------------------------
void CRender3D::RenderOverlayElements(void)
{
	bool bPopMode = BeginClientSpace();

	if (m_RenderState.bCenterCrosshair)
		RenderCrossHair();

	if ( bPopMode )
		EndClientSpace();

}


//-----------------------------------------------------------------------------
// Purpose: Gives all the tools a chance to render themselves.
//-----------------------------------------------------------------------------
void CRender3D::RenderTool(void)
{
	CMapDoc *pDoc = m_pView->GetMapDoc();

	CBaseTool *pTool = pDoc->GetTools()->GetActiveTool();

	if ( pTool )
	{
		pTool->RenderTool3D(this);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRender3D::RenderTree( CMapWorld *pWorld )
{
	if (pWorld == NULL)
	{
		return;
	}

	//
	// Recursively traverse the culling tree, rendering visible nodes.
	//
	CCullTreeNode *pTree = pWorld->CullTree_GetCullTree();
	if (pTree != NULL)
	{
		Vector vecMins;
		Vector vecMaxs;
		pTree->GetBounds(vecMins, vecMaxs);
		Visibility_t eVis = IsBoxVisible(vecMins, vecMaxs);

		if (eVis != VIS_NONE)
		{
			RenderNode(pTree, eVis == VIS_TOTAL);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns whether we are in lighting preview mode or not.
//-----------------------------------------------------------------------------
bool CRender3D::IsInLightingPreview()
{
	return false; //m_bLightingPreview;
}


//-----------------------------------------------------------------------------
// Purpose: Enables/disables lighting preview mode.
//-----------------------------------------------------------------------------
void CRender3D::SetInLightingPreview( bool bLightingPreview )
{
	m_bLightingPreview = false; //bLightingPreview;
}


void CRender3D::ResetFocus()
{
	// A bizarre workaround; the drop-down menu somehow
	// sets some wierd state that causes the whole screen to not be updated
	InvalidateRect( m_WinData.hWnd, 0, false );
}


//-----------------------------------------------------------------------------
// indicates we need to render an overlay pass...
//-----------------------------------------------------------------------------
bool CRender3D::NeedsOverlay() const
{
	return (m_eCurrentRenderMode == RENDER_MODE_LIGHTMAP_GRID) ||
		(m_eCurrentRenderMode == RENDER_MODE_TEXTURED_SHADED) ||
		(m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW2) ||
		(m_eCurrentRenderMode == RENDER_MODE_LIGHT_PREVIEW_RAYTRACED) ||
		(m_eCurrentRenderMode == RENDER_MODE_TEXTURED);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRender3D::ShutDown(void)
{
	MaterialSystemInterface()->RemoveView( m_WinData.hWnd );

	if (m_WinData.hDC)
	{
		m_WinData.hDC = NULL;
	}
	
	if (m_WinData.bFullScreen)
	{
		ChangeDisplaySettings(NULL, 0);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Uncaches all cached textures
//-----------------------------------------------------------------------------
void CRender3D::UncacheAllTextures()
{
}


//-----------------------------------------------------------------------------
// Purpose: Enables and disables various rendering parameters.
// Input  : eRenderState - Parameter to enable or disable. See RenderState_t.
//			bEnable - true to enable, false to disable the specified render state.
//-----------------------------------------------------------------------------
void CRender3D::RenderEnable(RenderState_t eRenderState, bool bEnable)
{
	switch (eRenderState)
	{
		case RENDER_POLYGON_OFFSET_FILL:
		{
			m_nDecalMode = bEnable?1:0;
			SetRenderMode( RENDER_MODE_CURRENT, true );
		}
		break;

		case RENDER_POLYGON_OFFSET_LINE:
		{
			assert(0);
			/* FIXME:
			   Think we'll need to have two versions of the wireframe material
			   one which ztests with offset + culling, the other which doesn't
			   ztest, doesn't offect, and doesn't cull??!?

			   m_pWireframeIgnoreZ->SetIntValue( bEnable );
			   m_pWireframe->GetMaterial()->InitializeStateSnapshots();
			   /*
			   if (bEnable)
			   {
			   glEnable(GL_POLYGON_OFFSET_LINE);
			   glPolygonOffset(-1, -1);
			   }
			   else
			   {
			   glDisable(GL_POLYGON_OFFSET_LINE);
			   }
			*/
			break;
		}

		case RENDER_CENTER_CROSSHAIR:
		{
			m_RenderState.bCenterCrosshair = bEnable;
			break;
		}

		case RENDER_GRID:
		{
			m_RenderState.bDrawGrid = bEnable;
			break;
		}

		case RENDER_FILTER_TEXTURES:
		{
			m_RenderState.bFilterTextures = bEnable;
			break;
		}

		case RENDER_REVERSE_SELECTION:
		{
			m_RenderState.bReverseSelection = bEnable;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Groovy little debug hook; can be whatever I want or need.
// Input  : pData - 
//-----------------------------------------------------------------------------
void CRender3D::DebugHook1(void *pData)
{
	g_bShowStatistics = !g_bShowStatistics;

#ifdef _DEBUG
	m_bRecomputeFrustumRenderGeometry = true;
	m_bRenderFrustum = true;
#endif

	//if (!m_bDroppedCamera)
	//{
	//	*m_pDropCamera = *m_pCamera;
	//	m_bDroppedCamera = true;
	//}
	//else
	//{
	//	m_bDroppedCamera = false;
	//}
}


//-----------------------------------------------------------------------------
// Purpose: Another groovy little debug hook; can be whatever I want or need.
// Input  : pData - 
//-----------------------------------------------------------------------------
void CRender3D::DebugHook2(void *pData)
{
	g_bRenderCullBoxes = !g_bRenderCullBoxes;
}

