//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the 3D view message handling. This class is responsible
//			for 3D camera control, activating tools in the 3D view, calling
//			into the renderer when necessary, and synchronizing the 2D camera
//			information with the 3D camera.
//
//=============================================================================//

#include "stdafx.h"
#include <oleauto.h>
#include <oaidl.h>
#if _MSC_VER < 1300
#include <afxpriv.h>
#endif
#include <mmsystem.h>
#include "Camera.h"
#include "GlobalFunctions.h"
#include "Gizmo.h"
#include "History.h"
#include "Keyboard.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "MapDecal.h"
#include "MapEntity.h"
#include "MapSolid.h"
#include "MapStudioModel.h"
#include "MapWorld.h"
#include "MapView3D.h"
#include "MapView2D.h"
#include "ObjectBar.h"
#include "Options.h"
#include "StatusBarIDs.h"
#include "TitleWnd.h"
#include "ToolManager.h"
#include "hammer.h"
#include "mathlib/vector.h"
#include "MapOverlay.h"
#include "engine_launcher_api.h"
#include "vgui/Cursor.h"
#include "ToolCamera.h"
#include "HammerVGui.h"
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#pragma warning(disable:4244 4305)


typedef struct
{
	CMapObjectList *pList;
	POINT pt;
	CMapWorld *pWorld;
} SELECT3DINFO;


int g_nClipPoints = 0;
Vector g_ClipPoints[4];


//
// Defines the logical keys.
//
#define LOGICAL_KEY_FORWARD			0
#define LOGICAL_KEY_BACK			1
#define LOGICAL_KEY_LEFT			2
#define LOGICAL_KEY_RIGHT			3
#define LOGICAL_KEY_UP				4
#define LOGICAL_KEY_DOWN			5
#define LOGICAL_KEY_PITCH_UP		6
#define LOGICAL_KEY_PITCH_DOWN		7
#define LOGICAL_KEY_YAW_LEFT		8
#define LOGICAL_KEY_YAW_RIGHT		9

//
// Rotation speeds, in degrees per second.
//
#define YAW_SPEED					180
#define PITCH_SPEED					180
#define ROLL_SPEED					180


IMPLEMENT_DYNCREATE(CMapView3D, CView)


BEGIN_MESSAGE_MAP(CMapView3D, CView)
	//{{AFX_MSG_MAP(CMapView3D)
	ON_WM_KILLFOCUS()
	ON_WM_TIMER()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	ON_WM_RBUTTONUP()
	ON_WM_CHAR()
	ON_WM_SETFOCUS()
	ON_WM_NCPAINT()
	ON_COMMAND(ID_FILE_PRINT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CView::OnFilePrintPreview)
	ON_COMMAND(ID_VIEW_3DWIREFRAME, OnView3dWireframe)
	ON_COMMAND(ID_VIEW_3DPOLYGON, OnView3dPolygon)
	ON_COMMAND(ID_VIEW_3DTEXTURED, OnView3dTextured)
	ON_COMMAND(ID_VIEW_3DLIGHTMAP_GRID, OnView3dLightmapGrid)
	ON_COMMAND(ID_VIEW_LIGHTINGPREVIEW, OnView3dLightingPreview)
	ON_COMMAND(ID_VIEW_LIGHTINGPREVIEW_RAYTRACED, OnView3dLightingPreviewRayTraced)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members to default values.
//-----------------------------------------------------------------------------
CMapView3D::CMapView3D(void)
{
	m_eDrawType = VIEW3D_WIREFRAME;
	m_pRender = NULL;
	m_pCamera = NULL;

	m_dwTimeLastInputSample = 0;

	m_fForwardSpeed = 0;
	m_fStrafeSpeed = 0;
	m_fVerticalSpeed = 0;

	m_pwndTitle = NULL;
	m_bLightingPreview = false;

	m_bMouseLook = false;
	m_bStrafing = false;
	m_bRotating = false;

	m_ptLastMouseMovement.x = 0;
	m_ptLastMouseMovement.y = 0;

	m_nLastRaytracedBitmapRenderTimeStamp = -1;

	m_bCameraPosChanged = false;
	m_bClippingChanged = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Releases dynamically allocated resources.
//-----------------------------------------------------------------------------
CMapView3D::~CMapView3D(void)
{
	if (m_pCamera != NULL)
	{
		delete m_pCamera;
	}

	if (m_pRender != NULL)
	{
		m_pRender->ShutDown();
		delete m_pRender;
	}

	if (m_pwndTitle != NULL)
	{
		delete m_pwndTitle;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : cs - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapView3D::PreCreateWindow(CREATESTRUCT& cs)
{
	static CString className;
	
	if(className.IsEmpty())
	{
		//
		// We need the CS_OWNDC bit so that we don't need to call GetDC every time we render. That fixes the flicker under Win98.
		//
		className = AfxRegisterWndClass(CS_BYTEALIGNCLIENT | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_OWNDC, NULL, HBRUSH(GetStockObject(BLACK_BRUSH)));
	}

	cs.lpszClass = className;

	return CView::PreCreateWindow(cs);
}


//-----------------------------------------------------------------------------
// Purpose: Disables mouselook when the view loses focus. This ensures that the
//			cursor is shown and not locked in the center of the 3D view.
// Input  : pNewWnd - The window getting focus.
//-----------------------------------------------------------------------------
void CMapView3D::OnKillFocus(CWnd *pNewWnd)
{
	EnableMouseLook(false);
	EnableRotating(false);
	EnableStrafing(false);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nDrawType - 
//-----------------------------------------------------------------------------
void CMapView3D::SetDrawType(DrawType_t eDrawType)
{
	EditorRenderMode_t eRenderMode;

	// Turn off the dialog.
	if ( m_eDrawType == VIEW3D_SMOOTHING_GROUP )
	{
		CMainFrame *pMainFrame = GetMainWnd();
		if ( pMainFrame )
		{		
			CFaceSmoothingVisualDlg *pSmoothDlg = pMainFrame->GetSmoothingGroupDialog();
			pSmoothDlg->ShowWindow( SW_HIDE );
		}
	}

	if (m_pwndTitle != NULL)
	{
		m_pwndTitle->SetTitle("camera");
	}

	m_bLightingPreview = false;
	switch (eDrawType)
	{
		case VIEW3D_WIREFRAME:
		{
			eRenderMode = RENDER_MODE_WIREFRAME;
			break;
		}

		case VIEW3D_POLYGON:
		{
			eRenderMode = RENDER_MODE_FLAT;
			break;
		}

		case VIEW3D_TEXTURED:
		{
			eRenderMode = RENDER_MODE_TEXTURED;
			break;
		}

		case VIEW3D_TEXTURED_SHADED:
		{
			eRenderMode = RENDER_MODE_TEXTURED_SHADED;
			break;
		}
		case VIEW3D_LIGHTMAP_GRID:
		{
			eRenderMode = RENDER_MODE_LIGHTMAP_GRID;
			break;
		}

		case VIEW3D_LIGHTING_PREVIEW2:
		{
			eRenderMode = RENDER_MODE_LIGHT_PREVIEW2;
			break;
		}
		
		case VIEW3D_LIGHTING_PREVIEW_RAYTRACED:
		{
			eRenderMode = RENDER_MODE_LIGHT_PREVIEW_RAYTRACED;
			break;
		}

		case VIEW3D_SMOOTHING_GROUP:
		{
			CMainFrame *pMainFrame = GetMainWnd();
			if ( pMainFrame )
			{		
				CFaceSmoothingVisualDlg *pSmoothDlg = pMainFrame->GetSmoothingGroupDialog();
				pSmoothDlg->ShowWindow( SW_SHOW );
			}

			// Always set the initial group to visualize (zero).
			CMapDoc *pDoc = GetMapDoc();
			pDoc->SetSmoothingGroupVisual( 0 );

			eRenderMode = RENDER_MODE_SMOOTHING_GROUP;
			break;
		}

		//case VIEW3D_ENGINE:
		//{
		//	eRenderMode = RENDER_MODE_TEXTURED;
		//	if ( IsRunningInEngine() )
		//	{
		//		CMapDoc *pMapDoc = CMapDoc::GetActiveMapDoc();
		//		if ( pMapDoc )
		//		{
		//			const char *pFullPathName = pMapDoc->GetPathName();
		//			if ( pFullPathName && pFullPathName[0] )
		//			{
		//				char buf[MAX_PATH];
		//				Q_FileBase( pFullPathName, buf,	MAX_PATH );
		//	
		//				// Don't do it if we're untitled
		//				//if ( !Q_stristr( buf, "untitled" ) )
		//				{
		//					g_pEngineAPI->SetEngineWindow( m_hWnd );
		//					//g_pEngineAPI->SetMap( buf );
		//					g_pEngineAPI->ActivateSimulation( true );
		//				}
		//			}
		//		}
		//	}
		//	if (m_pwndTitle != NULL)
		//	{
		//		m_pwndTitle->SetTitle("engine");
		//	}
		//	break;
		//}

		default:
		{
			Assert(FALSE);
			eDrawType = VIEW3D_WIREFRAME;
			eRenderMode = RENDER_MODE_WIREFRAME;
			break;
		}
	}

	m_eDrawType = eDrawType;

	//
	// Set renderer to use the new rendering mode.
	//
	if (m_pRender != NULL)
	{
		m_pRender->SetDefaultRenderMode(eRenderMode);
		m_pRender->SetInLightingPreview( m_bLightingPreview );

		// Somehow, this drop down box screws up MFC's notion
		// of what we're supposed to be updating. This is a workaround.
		m_pRender->ResetFocus();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the position and direction of the camera for this view.
//-----------------------------------------------------------------------------
void CMapView3D::SetCamera(const Vector &vecPos, const Vector &vecLookAt)
{
	m_pCamera->SetViewPoint(vecPos);
	m_pCamera->SetViewTarget(vecLookAt);
}

//-----------------------------------------------------------------------------
// Purpose: Prepares to print.
// Input  : Per CView::OnPreparePrinting.
// Output : Returns nonzero to begin printing, zero to cancel printing.
//-----------------------------------------------------------------------------
BOOL CMapView3D::OnPreparePrinting(CPrintInfo* pInfo)
{
	return(DoPreparePrinting(pInfo));
}


//-----------------------------------------------------------------------------
// Purpose: Debugging functions.
//-----------------------------------------------------------------------------
#ifdef _DEBUG
void CMapView3D::AssertValid() const
{
	CView::AssertValid();
}

void CMapView3D::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif //_DEBUG
 

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIDEvent - 
//-----------------------------------------------------------------------------
void CMapView3D::OnTimer(UINT nIDEvent) 
{
	static bool s_bPicking = false; // picking mutex

	switch (nIDEvent)
	{
		case MVTIMER_PICKNEXT:
		{
			if ( !s_bPicking ) 
			{
				s_bPicking = true;
				// set current document hit
				GetMapDoc()->GetSelection()->SetCurrentHit(hitNext);
				s_bPicking = false;
			}
			break;
		}
	}

	CView::OnTimer(nIDEvent);
}


//-----------------------------------------------------------------------------
// Purpose: Called just before we are destroyed.
//-----------------------------------------------------------------------------
BOOL CMapView3D::DestroyWindow() 
{
	KillTimer(MVTIMER_PICKNEXT);
	return CView::DestroyWindow();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView3D::UpdateStatusBar(void)
{
	if (!IsWindow(m_hWnd))
	{
		return;
	}

	SetStatusText(SBI_GRIDZOOM, "");
	SetStatusText(SBI_COORDS, "");
}


//-----------------------------------------------------------------------------
// Purpose: Sets up key bindings for the 3D view.
//-----------------------------------------------------------------------------
void CMapView3D::InitializeKeyMap(void)
{
	m_Keyboard.RemoveAllKeyMaps();

	if (!Options.view2d.bNudge)
	{
		m_Keyboard.AddKeyMap(VK_LEFT, 0, LOGICAL_KEY_YAW_LEFT);
		m_Keyboard.AddKeyMap(VK_RIGHT, 0, LOGICAL_KEY_YAW_RIGHT);
		m_Keyboard.AddKeyMap(VK_DOWN, 0, LOGICAL_KEY_PITCH_DOWN);
		m_Keyboard.AddKeyMap(VK_UP, 0, LOGICAL_KEY_PITCH_UP);

		m_Keyboard.AddKeyMap(VK_LEFT, KEY_MOD_SHIFT, LOGICAL_KEY_LEFT);
		m_Keyboard.AddKeyMap(VK_RIGHT, KEY_MOD_SHIFT, LOGICAL_KEY_RIGHT);
		m_Keyboard.AddKeyMap(VK_DOWN, KEY_MOD_SHIFT, LOGICAL_KEY_DOWN);
		m_Keyboard.AddKeyMap(VK_UP, KEY_MOD_SHIFT, LOGICAL_KEY_UP);
	}

	if (Options.view3d.bUseMouseLook)
	{
		m_Keyboard.AddKeyMap('W', 0, LOGICAL_KEY_FORWARD);
		m_Keyboard.AddKeyMap('A', 0, LOGICAL_KEY_LEFT);
		m_Keyboard.AddKeyMap('D', 0, LOGICAL_KEY_RIGHT);
		m_Keyboard.AddKeyMap('S', 0, LOGICAL_KEY_BACK);
	}
	else
	{
		m_Keyboard.AddKeyMap('D', 0, LOGICAL_KEY_FORWARD);
		m_Keyboard.AddKeyMap('C', 0, LOGICAL_KEY_BACK);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pWnd - 
//			point - 
//-----------------------------------------------------------------------------
void CMapView3D::OnContextMenu(CWnd *pWnd, CPoint point)
{
    // Pass the message to the active tool.
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool)
	{
		if ( pTool->OnContextMenu3D(this, 0, Vector2D(point.x, point.y) ) )
		{
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles the key down event.
// Input  : Per CWnd::OnKeyDown.
//-----------------------------------------------------------------------------
void CMapView3D::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	CMapDoc *pDoc = GetMapDoc();
	if (pDoc == NULL)
	{
		return;
	}

	//
	// 'z' toggles mouselook.
	//
	if (((char)tolower(nChar) == 'z') && !(nFlags & 0x4000) && (Options.view3d.bUseMouseLook))
	{
		if (pDoc != NULL)
		{
			EnableMouseLook(!m_bMouseLook);

			//
			// If we just stopped mouse looking, update the camera variables.
			//
			if (!m_bMouseLook)
			{
				UpdateCameraVariables();
			}
		}
		
		return;
	}

    // Got to check for m_pToolManager here because otherwise it can crash on startup if they have keys pressed.
    if ( m_pToolManager )
    {
		//
		// Pass the message to the active tool.
		//
		CBaseTool *pTool = m_pToolManager->GetActiveTool();
		if (pTool)
		{
			if (pTool->OnKeyDown3D(this, nChar, nRepCnt, nFlags))
			{
				return;
			}
		}
	}

	m_Keyboard.OnKeyDown(nChar, nRepCnt, nFlags);

	switch (nChar)
	{
		case VK_DELETE:
		{
			pDoc->OnCmdMsg(ID_EDIT_DELETE, CN_COMMAND, NULL, NULL);
			break;
		}

		case VK_NEXT:
		{
			pDoc->OnCmdMsg(ID_EDIT_SELNEXT, CN_COMMAND, NULL, NULL);
			break;
		}

		case VK_PRIOR:
		{
			pDoc->OnCmdMsg(ID_EDIT_SELPREV, CN_COMMAND, NULL, NULL);
			break;
		}

		//
		// Move the back clipping plane closer in.
		//
		case '1':
		{
			float fBack = m_pCamera->GetFarClip();
			if (fBack >= 2000)
			{
				m_pCamera->SetFarClip(fBack - 1000);
				Options.view3d.iBackPlane = fBack;
			}
			else if (fBack > 500)
			{
				m_pCamera->SetFarClip(fBack - 250);
				Options.view3d.iBackPlane = fBack;
			}
			m_bUpdateView = true;
			m_bClippingChanged = true;
			break;
		}

		//
		// Move the back clipping plane farther away.
		//
		case '2':
		{
			float fBack = m_pCamera->GetFarClip();
			if ((fBack <= 9000) && (fBack > 1000))
			{
				m_pCamera->SetFarClip(fBack + 1000);
				Options.view3d.iBackPlane = fBack;
			}
			else if (fBack < 10000)
			{
				m_pCamera->SetFarClip(fBack + 250);
				Options.view3d.iBackPlane = fBack;
			}
			m_bUpdateView = true;
			m_bClippingChanged = true;
			break;
		}

		case 'O':
		case 'o':
		{
			m_pRender->DebugHook1();
			break;
		}

		case 'I':
		case 'i':
		{
			m_pRender->DebugHook2();
			break;
		}

		case 'P':
		case 'p':
		{
			pDoc->OnToggle3DGrid();
			break;
		}

		default:
		{
			break;
		}
	}

	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}


//-----------------------------------------------------------------------------
// Purpose: Handles key release events.
// Input  : Per CWnd::OnKeyup
//-----------------------------------------------------------------------------
void CMapView3D::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
    // Got to check for m_pToolManager here because otherwise it can crash on startup if they have keys pressed.
    if ( m_pToolManager )
    {
   		// Pass the message to the active tool.
		CBaseTool *pTool = m_pToolManager->GetActiveTool();
		if (pTool)
		{
			if (pTool->OnKeyUp3D(this, nChar, nRepCnt, nFlags))
			{
				return;
			}
		}

		m_Keyboard.OnKeyUp(nChar, nRepCnt, nFlags);

		UpdateCameraVariables();
	}
	
	CView::OnKeyUp(nChar, nRepCnt, nFlags);
}


//-----------------------------------------------------------------------------
// Purpose: Called when the view is resized.
// Input  : nType - 
//			cx - 
//			cy - 
//-----------------------------------------------------------------------------
void CMapView3D::OnSize(UINT nType, int cx, int cy) 
{
	if ( m_pCamera )
	{
		m_pCamera->SetViewPort( cx, cy );
	}

	CView::OnSize(nType, cx, cy);
}


//-----------------------------------------------------------------------------
// Purpose: Finds the axis that is most closely aligned with the given vector.
// Input  : Vector - Vector to find closest axis to.
// Output : Returns an axis index as follows:
//				0 - Positive X axis.
//				1 - Positive Y axis.
//				2 - Positive Z axis.
//				3 - Negative X axis.
//				4 - Negative Y axis.
//				5 - Negative Z axis.
//-----------------------------------------------------------------------------
const Vector&ClosestAxis(const Vector& v)
{
    static Vector vBestAxis;
	float fBestDot = -1;
	Vector vNormal = v;

	VectorNormalize( vNormal );
	vBestAxis.Init();
	
	for (int i = 0; i < 6; i++)
	{
		Vector vTestAxis(0,0,0);

		vTestAxis[i%3] = (i>=3)?-1:1;

		float fTestDot = DotProduct(v, vTestAxis);
		if (fTestDot > fBestDot)
		{
			fBestDot = fTestDot;
			vBestAxis = vTestAxis;
		}
	}

	return vBestAxis;
}

void CMapView3D::GetBestTransformPlane( Vector &horzAxis, Vector &vertAxis, Vector &thirdAxis)
{
	Vector vAxis;

 	m_pCamera->GetViewRight( vAxis );
	horzAxis = ClosestAxis( vAxis );
	
	m_pCamera->GetViewUp( vAxis );
	vertAxis = ClosestAxis( vAxis );

	m_pCamera->GetViewForward( vAxis );
	thirdAxis = ClosestAxis( vAxis );
}


//-----------------------------------------------------------------------------
// Purpose: Synchronizes the 2D camera information with the 3D view.
//-----------------------------------------------------------------------------
void CMapView3D::UpdateCameraVariables(void)
{
	Camera3D *pCamTool = dynamic_cast<Camera3D*>(m_pToolManager->GetToolForID( TOOL_CAMERA ));

	if (!m_pCamera || !pCamTool )
		return;
	
	Vector viewPoint,viewForward;
	
	m_pCamera->GetViewPoint(viewPoint);
	m_pCamera->GetViewForward(viewForward);

	// tell camera tool to update active camera
	pCamTool->UpdateActiveCamera( viewPoint, viewForward );
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button double click event.
//-----------------------------------------------------------------------------
void CMapView3D::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	//
	// Don't forward message if we are controlling the camera.
	//
	if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0)
	{
		return;
	}

	//
	// Pass the message to the active tool.
	//
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool != NULL)
	{
		Vector2D vPoint( point.x,point.y);
		if (pTool->OnLMouseDblClk3D( this, nFlags, vPoint ))
		{
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles the left mouse button down event.
//-----------------------------------------------------------------------------
void CMapView3D::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0)
	{
		EnableRotating(true);
		return;
	}

    //
	// Pass the message to the active tool.
    //
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool != NULL)
	{
		Vector2D vPoint( point.x,point.y);
		if (pTool->OnLMouseDown3D(this, nFlags, vPoint))
		{
			return;
		}
	}

	CView::OnLButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Called by the selection tool to begin timed selection by depth.
//-----------------------------------------------------------------------------
void CMapView3D::BeginPick(void)
{
	SetTimer(MVTIMER_PICKNEXT, 500, NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Called by the selection tool to end timed selection by depth.
//-----------------------------------------------------------------------------
void CMapView3D::EndPick(void)
{
	//
	// Kill pick timer.
	//
	KillTimer(MVTIMER_PICKNEXT);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CMapView3D::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (m_bRotating)
	{
		EnableRotating(false);
		UpdateCameraVariables();
		return;
	}

	//
	// Pass the message to the active tool.
	//
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool != NULL)
	{
		Vector2D vPoint( point.x,point.y);
		if (pTool->OnLMouseUp3D(this, nFlags, vPoint))
		{
			return;
		}
	}

	CView::OnLButtonUp(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Creates the renderer and the camera and initializes them.
//-----------------------------------------------------------------------------
void CMapView3D::OnInitialUpdate(void)
{
	InitializeKeyMap();

	//
	// Create a title window.
	//
	m_pwndTitle = CTitleWnd::CreateTitleWnd(this, ID_2DTITLEWND);
	Assert(m_pwndTitle != NULL);
	if (m_pwndTitle != NULL)
	{
		m_pwndTitle->SetTitle("camera");
	}

	//
	// CMainFrame::LoadWindowStates calls InitialUpdateFrame which causes us to get two
	// OnInitialUpdate messages! Check for a NULL renderer to avoid processing twice.
	//
	if (m_pRender != NULL)
	{
		return;
	}

	//
	// Create and initialize the renderer.
	//
	m_pRender = new CRender3D();
	
	CMapDoc *pDoc = GetMapDoc();
	if (pDoc == NULL)
	{
		Assert(pDoc != NULL);
		return;
	}

	m_pRender->SetView( this );

	m_pToolManager = pDoc->GetTools();

	SetDrawType(m_eDrawType);

	
	//
	// Create and initialize the camera.
	//
	m_pCamera = new CCamera();
	Assert(m_pCamera != NULL);
	if (m_pCamera == NULL)
	{
		return;
	}

	CRect rect;
	GetClientRect( rect );
	m_pCamera->SetViewPort( rect.Width(), rect.Height() );

	m_fForwardSpeedMax = Options.view3d.nForwardSpeedMax;
	m_fStrafeSpeedMax = Options.view3d.nForwardSpeedMax * 0.75f;
	m_fVerticalSpeedMax = Options.view3d.nForwardSpeedMax * 0.5f;

	//
	// Calculate the acceleration based on max speed and the time to max speed.
	//
	if (Options.view3d.nTimeToMaxSpeed != 0)
	{
		m_fForwardAcceleration = m_fForwardSpeedMax / (Options.view3d.nTimeToMaxSpeed / 1000.0f);
		m_fStrafeAcceleration = m_fStrafeSpeedMax / (Options.view3d.nTimeToMaxSpeed / 1000.0f);
		m_fVerticalAcceleration = m_fVerticalSpeedMax / (Options.view3d.nTimeToMaxSpeed / 1000.0f);
	}
	else
	{
		m_fForwardAcceleration = 0;
		m_fStrafeAcceleration = 0;
		m_fVerticalAcceleration = 0;
	}

	//
	// Set up the frustum. We set the vertical FOV to zero because the renderer
	// only uses the horizontal FOV.
	//
	if ( Options.general.bRadiusCulling )
	{
		// Hack!  Don't use frustum culling when doing radial distance culling (slam the distance to 10K)
		m_pCamera->SetPerspective( Options.view3d.fFOV, CAMERA_FRONT_PLANE_DISTANCE, 10000);
	}
	else
	{
		m_pCamera->SetPerspective( Options.view3d.fFOV, CAMERA_FRONT_PLANE_DISTANCE, Options.view3d.iBackPlane);
	}

	//
	// Set the distance at which studio models become bounding boxes.
	//
	CMapStudioModel::SetRenderDistance(Options.view3d.nModelDistance);
	CMapStudioModel::EnableAnimation(Options.view3d.bAnimateModels);

	//
	// Enable or disable reverse selection.
	//
	m_pRender->RenderEnable(RENDER_REVERSE_SELECTION, (Options.view3d.bReverseSelection == TRUE));

	//
	// Enable or disable the 3D grid.
	//
	m_pRender->RenderEnable(RENDER_GRID, pDoc->Is3DGridEnabled());

	//
	// Enable or disable texture filtering.
	//
	m_pRender->RenderEnable(RENDER_FILTER_TEXTURES, (Options.view3d.bFilterTextures == TRUE));

	// Get the initial viewpoint and view direction from the default camera in the document.

	Camera3D *pCamTool = dynamic_cast<Camera3D*>(m_pToolManager->GetToolForID( TOOL_CAMERA ));
	if ( pCamTool )
	{
		Vector vecPos,vecLookAt;
		pCamTool->GetCameraPos( vecPos,vecLookAt );
		SetCamera(vecPos, vecLookAt);
	}

	CView::OnInitialUpdate();
}


//-----------------------------------------------------------------------------
// Purpose: Turns on wireframe mode from the floating "Camera" menu.
//-----------------------------------------------------------------------------
void CMapView3D::OnView3dWireframe(void)
{
	SetDrawType(VIEW3D_WIREFRAME);
}


//-----------------------------------------------------------------------------
// Purpose: Turns on flat shaded mode from the floating "Camera" menu.
//-----------------------------------------------------------------------------
void CMapView3D::OnView3dPolygon(void)
{
	SetDrawType(VIEW3D_POLYGON);
}


//-----------------------------------------------------------------------------
// Purpose: Turns on textured mode from the floating "Camera" menu.
//-----------------------------------------------------------------------------
void CMapView3D::OnView3dTextured(void)
{
	SetDrawType(VIEW3D_TEXTURED);
}


//-----------------------------------------------------------------------------
// Purpose: Turns on lightmap grid mode from the floating "Camera" menu.
//-----------------------------------------------------------------------------
void CMapView3D::OnView3dLightmapGrid(void)
{
	SetDrawType(VIEW3D_LIGHTMAP_GRID);
}


//-----------------------------------------------------------------------------
// Purpose: Turns on lighting preview mode from the floating "Camera" menu.
//-----------------------------------------------------------------------------
void CMapView3D::OnView3dLightingPreview(void)
{
	SetDrawType(VIEW3D_LIGHTING_PREVIEW2);
}

void CMapView3D::OnView3dLightingPreviewRayTraced(void)
{
	SetDrawType(VIEW3D_LIGHTING_PREVIEW_RAYTRACED);
}

//-----------------------------------------------------------------------------
// Purpose: Turns on engine mode from the floating "Camera" menu.
//-----------------------------------------------------------------------------
//void CMapView3D::OnView3dEngine(void)
//{
//	SetDrawType(VIEW3D_ENGINE);
//}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bActivate - 
//			pActivateView - 
//			pDeactiveView - 
//-----------------------------------------------------------------------------
void CMapView3D::ActivateView(bool bActivate)
{
	CMapView::ActivateView(bActivate);

	if (bActivate)
	{
		CMapDoc *pDoc = GetMapDoc();
		CMapDoc::SetActiveMapDoc(pDoc);
		
		UpdateStatusBar();

		// tell doc to update title
		pDoc->UpdateTitle(this);

		m_Keyboard.ClearKeyStates();

		//
		// Reset the last input sample time.
		//
		m_dwTimeLastInputSample = 0;
	}
}

void CMapView3D::OnDraw(CDC *pDC)
{
	CWnd *focusWnd = GetForegroundWindow();

	if ( focusWnd && focusWnd->ContinueModal() )
	{
		// render the view now since were not running the main loop
		RenderView();
	}
	else
	{
		// just flag view to be update with next main loop
		m_bUpdateView = true;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapView3D::RenderView()
{
	Render();
	m_bUpdateView = false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapView3D::ShouldRender()
{
	if ( m_eDrawType == VIEW3D_LIGHTING_PREVIEW_RAYTRACED )
	{
		// check if we have new results from lpreview thread
// 		if ( m_nLastRaytracedBitmapRenderTimeStamp != 
// 			 GetUpdateCounter( EVTYPE_BITMAP_RECEIVED_FROM_LPREVIEW ) )
// 			return true;
	}
	else
	{
		// don't animate ray traced displays
		if ( Options.view3d.bAnimateModels )
		{
			DWORD dwTimeElapsed = timeGetTime() - m_dwTimeLastRender;
			
			if ( (dwTimeElapsed/1000.0f) > 1.0f/20.0f)
			{
				m_bUpdateView = true;
			}
		}
	}
	return CMapView::ShouldRender();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDC - 
//-----------------------------------------------------------------------------
void CMapView3D::OnSetFocus(CWnd *pOldWnd)
{
	// Make sure the whole window region is marked as invalid 
	m_bUpdateView = true;
}


//-----------------------------------------------------------------------------
// Purpose: Called to paint the non client area of the window.
//-----------------------------------------------------------------------------
void CMapView3D::OnNcPaint(void)
{
	// Make sure the whole window region is marked as invalid 
	m_bUpdateView = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pSender - 
//			lHint - 
//			pHint - 
//-----------------------------------------------------------------------------
void CMapView3D::UpdateView(int nFlags)
{
	if ( !m_pRender )
		return;

	if (nFlags & ( MAPVIEW_UPDATE_ONLY_2D | MAPVIEW_UPDATE_ONLY_LOGICAL ) )
		return;
	
	//
	// One of the options in the 3D options page is changing.
	//
	if (nFlags & MAPVIEW_OPTIONS_CHANGED)
	{
		InitializeKeyMap();

		CMapStudioModel::SetRenderDistance(Options.view3d.nModelDistance);
		CMapStudioModel::EnableAnimation(Options.view3d.bAnimateModels);

		m_pRender->RenderEnable(RENDER_REVERSE_SELECTION, (Options.view3d.bReverseSelection == TRUE));

		m_fForwardSpeedMax = Options.view3d.nForwardSpeedMax;
		m_fStrafeSpeedMax = Options.view3d.nForwardSpeedMax * 0.75f;
		m_fVerticalSpeedMax = Options.view3d.nForwardSpeedMax * 0.5f;

		//
		// Calculate the acceleration based on max speed and the time to max speed.
		//
		if (Options.view3d.nTimeToMaxSpeed != 0)
		{
			m_fForwardAcceleration = m_fForwardSpeedMax / (Options.view3d.nTimeToMaxSpeed / 1000.0f);
			m_fStrafeAcceleration = m_fStrafeSpeedMax / (Options.view3d.nTimeToMaxSpeed / 1000.0f);
			m_fVerticalAcceleration = m_fVerticalSpeedMax / (Options.view3d.nTimeToMaxSpeed / 1000.0f);
		}
		else
		{
			m_fForwardAcceleration = 0;
			m_fStrafeAcceleration = 0;
			m_fVerticalAcceleration = 0;
		}

		m_pCamera->SetPerspective( Options.view3d.fFOV, CAMERA_FRONT_PLANE_DISTANCE, Options.view3d.iBackPlane);
		
		CMapDoc *pDoc = GetMapDoc();
		if ((pDoc != NULL) && (m_pRender != NULL))
		{
			m_pRender->RenderEnable(RENDER_GRID, pDoc->Is3DGridEnabled());
			m_pRender->RenderEnable(RENDER_FILTER_TEXTURES, (Options.view3d.bFilterTextures == TRUE));
		}
	}

	if (nFlags & MAPVIEW_UPDATE_OBJECTS)
	{
		// dvs: could use this hint to update the octree
	}

	CMapView::UpdateView( nFlags );
}


//-----------------------------------------------------------------------------
// Purpose: Determines the object at the point (point.x, point.y) in the 3D view.
// Input  : point - Point to use for hit test.
//			ulFace - Index of face in object that was hit.
// Output : Returns a pointer to the CMapClass object at the coordinates, NULL if none.
//-----------------------------------------------------------------------------
CMapClass *CMapView3D::NearestObjectAt( const Vector2D &vPoint, ULONG &ulFace, unsigned int nFlags, VMatrix *pLocalMatrix )
{
	ulFace = 0;
	if (m_pRender == NULL)
	{
		return(NULL);
	}

	HitInfo_t Hits;

	if (m_pRender->ObjectsAt( vPoint.x, vPoint.y, 1, 1, &Hits, 1, nFlags ) != 0)
	{
		//
		// If they clicked on a solid, the index of the face they clicked on is stored
		// in array index [1].
		//
		CMapAtom *pObject = (CMapAtom *)Hits.pObject;
		CMapSolid *pSolid = dynamic_cast<CMapSolid *>(pObject);
		if ( pLocalMatrix != NULL )
		{
			*pLocalMatrix = Hits.m_LocalMatrix;
		}
		if (pSolid != NULL)
		{
			ulFace = Hits.uData;
			return(pSolid);
		}

		return((CMapClass *)pObject);
	}

	return(NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Casts a ray from the viewpoint through the given plane and determines
//			the point of intersection of the ray on the plane.
// Input  : point - Point in client screen coordinates.
//			plane - Plane being 'clicked' on.
//			pos - Returns the point on the plane that projects to the given point.
//-----------------------------------------------------------------------------
void CMapView3D::GetHitPos(const Vector2D &point, PLANE &plane, Vector &pos)
{
	//
	// Find the point they clicked on in world coordinates. It lies on the near
	// clipping plane.
	//
	Vector ClickPoint;

	ClientToWorld( ClickPoint, point );
	
	//
	// Build a ray from the viewpoint through the point on the near clipping plane.
	//
	Vector ViewPoint;
	Vector Ray;
	m_pCamera->GetViewPoint(ViewPoint);
	VectorSubtract(ClickPoint, ViewPoint, Ray);

	//
	// Find the point of intersection of the ray with the given plane.
	//
	float t = DotProduct(plane.normal, ViewPoint) - plane.dist;
	t = t / -DotProduct(plane.normal, Ray);
	
	pos = ViewPoint + t * Ray;
}

bool CMapView3D::HitTest( const Vector2D &vPoint, const Vector& mins, const Vector& maxs)
{
	Vector vStart, vEnd;
	int nFace;
	BuildRay( vPoint, vStart, vEnd );
	return IntersectionLineAABBox( mins, maxs, vStart, vEnd, nFace ) >= 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Finds all objects under the given rectangular region in the view.
// Input  : x - Client x coordinate.
//			y - Client y coordinate.
//			fWidth - Width of region in client pixels.
//			fHeight - Height of region in client pixels.
//			pObjects - Receives objects in the given region.
//			nMaxObjects - Size of the array pointed to by pObjects.
// Output : Returns the number of objects in the given region.
//-----------------------------------------------------------------------------
int CMapView3D::ObjectsAt( const Vector2D &vPoint, HitInfo_t *pObjects, int nMaxObjects, unsigned int nFlags )
{
	if (m_pRender != NULL)
	{
		return m_pRender->ObjectsAt( vPoint.x, vPoint.y, 1, 1, pObjects, nMaxObjects, nFlags );
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Makes sure that this view has focus if the mouse moves over it.
//-----------------------------------------------------------------------------
void CMapView3D::OnMouseMove(UINT nFlags, CPoint point)
{
	//
	// Make sure we are the active view.
	//
	if (!IsActive())
	{
		CMapDoc *pDoc = GetMapDoc();
		pDoc->SetActiveView(this);
	}

	//
	// If we are the active application, make sure this view has the input focus.
	//	
	if (APP()->IsActiveApp())
	{
		if (GetFocus() != this)
		{
			SetFocus();
		}
	}


	CView::OnMouseMove(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView3D::ProcessInput(void)
{
	if (m_dwTimeLastInputSample == 0)
	{
		m_dwTimeLastInputSample = timeGetTime();
	}

	DWORD dwTimeNow = timeGetTime();

	float fElapsedTime = (float)(dwTimeNow - m_dwTimeLastInputSample) / 1000.0f;

	m_dwTimeLastInputSample = dwTimeNow;

	// Clamp (can get really big when we cache textures in )
	if (fElapsedTime > 0.3f)
	{
		fElapsedTime = 0.3f;
	}
	else if ( fElapsedTime <=0 )
	{
		return; // dont process input
	}

	ProcessKeys( fElapsedTime );

	ProcessMouse();

	if ( Options.general.bRadiusCulling )
	{
		ProcessCulling();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Applies an acceleration to a velocity, allowing instantaneous direction
//			change and zeroing the velocity in the absence of acceleration.
// Input  : fVelocity - Current velocity.
//			fAccel - Amount of acceleration to apply.
//			fTimeScale - The time for which the acceleration should be applied.
//			fMaxVelocity - The maximum velocity to allow.
// Output : Returns the new velocity.
//-----------------------------------------------------------------------------
static float Accelerate(float fVelocity, float fAccel, float fAccelScale, float fTimeScale, float fVelocityMax)
{
	//
	// If we have a finite acceleration in this direction, apply it to the velocity.
	//
	if ((fAccel != 0) && (fAccelScale != 0))
	{
		//
		// Check for direction reversal - zero velocity when reversing.
		//
		if (fAccelScale > 0)
		{
			if (fVelocity < 0)
			{
				fVelocity = 0;
			}
		}
		else if (fAccelScale < 0)
		{
			if (fVelocity > 0)
			{
				fVelocity = 0;
			}
		}

		//
		// Apply the acceleration.
		//
		fVelocity += fAccel * fAccelScale * fTimeScale;
		if (fVelocity > fVelocityMax)
		{
			fVelocity = fVelocityMax;
		}
		else if (fVelocity < -fVelocityMax)
		{
			fVelocity = -fVelocityMax;
		}

	}
	//
	// If we have infinite acceleration, go straight to maximum velocity.
	//
	else if (fAccelScale != 0)
	{
		fVelocity = fVelocityMax * fAccelScale;
	}
	//
	// Else no velocity in this direction at all.
	//
	else
	{
		fVelocity = 0;
	}

	return(fVelocity);
}


//-----------------------------------------------------------------------------
// Purpose: Moves the camera based on the keyboard state.
//-----------------------------------------------------------------------------
void CMapView3D::ProcessMovementKeys(float fElapsedTime)
{
	//
	// Read the state of the camera movement keys.
	//
	float fBack = m_Keyboard.GetKeyScale(LOGICAL_KEY_BACK);
	float fMoveForward = m_Keyboard.GetKeyScale(LOGICAL_KEY_FORWARD) - fBack;

	float fLeft = m_Keyboard.GetKeyScale(LOGICAL_KEY_LEFT);
	float fMoveRight = m_Keyboard.GetKeyScale(LOGICAL_KEY_RIGHT) - fLeft;

	float fDown = m_Keyboard.GetKeyScale(LOGICAL_KEY_DOWN);
	float fMoveUp = m_Keyboard.GetKeyScale(LOGICAL_KEY_UP) - fDown;

	float fPitchUp = m_Keyboard.GetKeyScale(LOGICAL_KEY_PITCH_UP);
	float fPitchDown = m_Keyboard.GetKeyScale(LOGICAL_KEY_PITCH_DOWN);

	float fYawLeft = m_Keyboard.GetKeyScale(LOGICAL_KEY_YAW_LEFT);
	float fYawRight = m_Keyboard.GetKeyScale(LOGICAL_KEY_YAW_RIGHT);

	//
	// Apply pitch and yaw if they are nonzero.
	//
	if ((fPitchDown - fPitchUp) != 0)
	{
		m_pCamera->Pitch((fPitchDown - fPitchUp) * fElapsedTime * PITCH_SPEED);
		m_bUpdateView = true;
	}

	if ((fYawRight - fYawLeft) != 0)
	{
		m_pCamera->Yaw((fYawRight - fYawLeft) * fElapsedTime * YAW_SPEED);
		m_bUpdateView = true;
	}

	//
	// Apply the accelerations to the forward, strafe, and vertical speeds. They are actually
	// velocities because they are signed values.
	//
	m_fForwardSpeed = Accelerate(m_fForwardSpeed, m_fForwardAcceleration, fMoveForward, fElapsedTime, m_fForwardSpeedMax);
	m_fStrafeSpeed = Accelerate(m_fStrafeSpeed, m_fStrafeAcceleration, fMoveRight, fElapsedTime, m_fStrafeSpeedMax);
	m_fVerticalSpeed = Accelerate(m_fVerticalSpeed, m_fVerticalAcceleration, fMoveUp, fElapsedTime, m_fVerticalSpeedMax);

	//
	// Move the camera if any of the speeds are nonzero.
	//
	if (m_fForwardSpeed != 0)
	{
		m_pCamera->MoveForward(m_fForwardSpeed * fElapsedTime);
		m_bUpdateView = true;
		m_bCameraPosChanged = true;
	}

	if (m_fStrafeSpeed != 0)
	{
		m_pCamera->MoveRight(m_fStrafeSpeed * fElapsedTime);
		m_bUpdateView = true;
		m_bCameraPosChanged = true;
	}

	if (m_fVerticalSpeed != 0)
	{
		m_pCamera->MoveUp(m_fVerticalSpeed * fElapsedTime);
		m_bUpdateView = true;
		m_bCameraPosChanged = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView3D::ProcessKeys(float fElapsedTime)
{
	ProcessMovementKeys(fElapsedTime);

	m_Keyboard.ClearImpulseFlags();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView3D::ProcessCulling( void )
{
	if ( m_bCameraPosChanged || m_bClippingChanged )
	{
		CMapDoc *pDoc = GetMapDoc();
		pDoc->UpdateVisibilityAll();

		m_bClippingChanged = false;
		m_bCameraPosChanged = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMapView3D::ControlCamera(const CPoint &point)
{
	if (!m_bStrafing && !m_bRotating && !m_bMouseLook)
	{
		return false;
	}

	bool bShift = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);

	CRect rect;
	GetClientRect(&rect);
	// get mouse distance to client window center
	CPoint WindowCenter = rect.CenterPoint();
	CSize MouseLookDelta = point - WindowCenter;

	// camera look is on, but no mouse changes
	if ( MouseLookDelta.cx == 0 && MouseLookDelta.cy == 0 )
		true;
	
	//
	// If strafing, left-right movement moves the camera from side to side.
	// Up-down movement either moves the camera forward and back if the SHIFT
	// key is held down, or up and down if the SHIFT key is not held down.
	// If rotating and strafing simultaneously, the behavior is as if SHIFT is
	// held down.
	//
	if (m_bStrafing)
	{
		if (bShift || m_bRotating)
		{
			MoveForward(-MouseLookDelta.cy * 2);
		}
		else
		{
			MoveUp(-MouseLookDelta.cy * 2);
		}

		MoveRight(MouseLookDelta.cx * 2);

		m_bCameraPosChanged = true;
	}
	//
	// If mouse looking, left-right movement controls yaw, and up-down
	// movement controls pitch.
	//
	else
	{
		//
		// Up-down mouse movement changes the camera pitch.
		//
		if (MouseLookDelta.cy)
		{
			float fTheta = MouseLookDelta.cy * 0.4;
			if (Options.view3d.bReverseY)
			{
				fTheta = -fTheta;
			}

			Pitch(fTheta);
		}
	
		//
		// Left-right mouse movement changes the camera yaw.
		//
		if (MouseLookDelta.cx)
		{
			float fTheta = MouseLookDelta.cx * 0.4;
			Yaw(fTheta);
		}
	}

	// move mouse back to center
	CWnd::ClientToScreen(&WindowCenter);
	SetCursorPos(WindowCenter.x, WindowCenter.y);
	m_bUpdateView = true;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Called by RunFrame to tell this view to process mouse input. This
//			function samples the cursor position and takes the appropriate
//			action based on the current mode (camera, morphing).
//-----------------------------------------------------------------------------
void CMapView3D::ProcessMouse(void)
{
	//
	// Get the cursor position in client coordinates.
	//

	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);

	if ( point == m_ptLastMouseMovement )
		return;

	m_ptLastMouseMovement = point;

	if ( ControlCamera( point ) )
	{
		return;
	}

	// If not in mouselook mode, only process mouse messages if there
	// is an active tool.
	//
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool != NULL)
	{
		//
		// Pass the message to the tool.
		//

		int nFlags = 0;

		if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			nFlags |= MK_CONTROL;
		}

		if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
		{
			nFlags |= MK_SHIFT;
		}

		Vector2D vPoint( point.x,point.y);
		pTool->OnMouseMove3D(this, nFlags, vPoint);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse wheel events. The mouse wheel is used in camera mode
//			to dolly the camera forward and back.
// Input  : Per CWnd::OnMouseWheel.
//-----------------------------------------------------------------------------
BOOL CMapView3D::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
    //
	// Pass the message to the active tool.
    //
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool != NULL)
	{
		Vector2D vPoint( point.x,point.y);
		if (pTool->OnMouseWheel3D(this, nFlags, zDelta, vPoint))
		{
			return(TRUE);
		}
	}

	m_pCamera->MoveForward(zDelta / 2);
	
	//
	// Render now to avoid an ugly lag between the 2D views and the 3D view
	// when "center 2D views on camera" is enabled.
	//
	m_bUpdateView = true;
	m_bCameraPosChanged = true;

	UpdateCameraVariables();

	return CView::OnMouseWheel(nFlags, zDelta, point);
}


//-----------------------------------------------------------------------------
// Purpose: Handles right mouse button down events.
// Input  : Per CWnd::OnRButtonDown.
//-----------------------------------------------------------------------------
void CMapView3D::OnRButtonDown(UINT nFlags, CPoint point) 
{
	if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0)
	{
		EnableStrafing(true);
		return;
	}

	//
	// Pass the message to the active tool.
	//
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool != NULL)
	{
		Vector2D vPoint( point.x,point.y);
		if (pTool->OnRMouseDown3D( this, nFlags, vPoint ))
		{
			return;
		}
	}

	CView::OnRButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Handles right mouse button up events.
// Input  : Per CWnd::OnRButtonUp.
//-----------------------------------------------------------------------------
void CMapView3D::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (m_bStrafing)
	{
		//
		// Turn off strafing and update the 2D views.
		//
		EnableStrafing(false);
		UpdateCameraVariables();
		return;
	}

	//
	// Pass the message to the active tool.
	//
	CBaseTool *pTool = m_pToolManager->GetActiveTool();
	if (pTool != NULL)
	{
		Vector2D vPoint( point.x,point.y);
		if (pTool->OnRMouseUp3D( this, nFlags, vPoint ))
		{
			return;
		}
	}

	CView::OnRButtonUp(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Handles character events.
// Input  : Per CWnd::OnChar.
//-----------------------------------------------------------------------------
void CMapView3D::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
    // Got to check for m_pToolManager here because otherwise it can crash on startup if they have keys pressed.
    if ( m_pToolManager )
    {
		//
		// Pass the message to the active tool.
		//
		CBaseTool *pTool = m_pToolManager->GetActiveTool();
		if (pTool != NULL)
		{
			if (pTool->OnChar3D(this, nChar, nRepCnt, nFlags))
			{
				return;
			}
		}
	}

	CView::OnChar(nChar, nRepCnt, nFlags);
}


//-----------------------------------------------------------------------------
// Purpose: Called when mouselook is enabled. The cursor is moved to the center
//			of the screen and hidden.
// Input  : bEnable - true to lock and hide the cursor, false to unlock and show it.
//-----------------------------------------------------------------------------
void CMapView3D::EnableCrosshair(bool bEnable)
{
	CRect Rect;
	CPoint Point;

	GetClientRect(&Rect);
	CWnd::ClientToScreen(&Rect);
	Point = Rect.CenterPoint();
	SetCursorPos(Point.x, Point.y);

	if (bEnable)
	{
		ClipCursor(&Rect);
	}
	else
	{
		ClipCursor(NULL);
	}

	ShowCursor(bEnable ? FALSE : TRUE);
	m_pRender->RenderEnable(RENDER_CENTER_CROSSHAIR, bEnable);
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables mouselook. When mouselooking, the cursor is hidden
//			and a crosshair is rendered in the center of the view.
// Input  : bEnable - TRUE to enable, FALSE to disable mouselook.
//-----------------------------------------------------------------------------
//void CMapView3D::EnableMouseLook(bool bEnable)
//{
//	if (m_bMouseLook != bEnable)
//	{
//		CMapDoc *pDoc = GetDocument();
//		if (pDoc != NULL)
//		{
//			EnableCrosshair(bEnable);
//			m_bMouseLook = bEnable;
//		}
//	}
//}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables mouselook. When mouselooking, the cursor is hidden
//			and a crosshair is rendered in the center of the view.
//-----------------------------------------------------------------------------
void CMapView3D::EnableMouseLook(bool bEnable)
{
	if (m_bMouseLook != bEnable)
	{
		if (!(m_bStrafing || m_bRotating))
		{
			EnableCrosshair(bEnable);
		}

		m_bMouseLook = bEnable;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables camera rotating. When rotating, the cursor is hidden
//			and a crosshair is rendered in the center of the view.
//-----------------------------------------------------------------------------
void CMapView3D::EnableRotating(bool bEnable)
{
	if (m_bRotating != bEnable)
	{
		if (!(m_bStrafing || m_bMouseLook))
		{
			EnableCrosshair(bEnable);
		}

		m_bRotating = bEnable;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables camera strafing. When strafing, the cursor is hidden
//			and a crosshair is rendered in the center of the view.
//-----------------------------------------------------------------------------
void CMapView3D::EnableStrafing(bool bEnable)
{
	if (m_bStrafing != bEnable)
	{
		if (!(m_bMouseLook || m_bRotating))
		{
			EnableCrosshair(bEnable);
		}

		m_bStrafing = bEnable;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Actually renders the 3D view. Called from the frame loop and from
//			some mouse messages when timely updating is important.
//-----------------------------------------------------------------------------
void CMapView3D::Render(void)
{
	if ( m_pRender != NULL )
	{
		m_pRender->Render();
	}

	if (m_pwndTitle != NULL)
	{
		m_pwndTitle->BringWindowToTop();
		m_pwndTitle->Invalidate();
		m_pwndTitle->UpdateWindow();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObject - 
//-----------------------------------------------------------------------------
void CMapView3D::RenderPreloadObject(CMapAtom *pObject)
{
	if ((pObject != NULL) && (m_pRender != NULL))
	{
		pObject->RenderPreload(m_pRender, false);
	}
}


//-----------------------------------------------------------------------------
// Release all video memory.
//-----------------------------------------------------------------------------
void CMapView3D::ReleaseVideoMemory(void)
{
	m_pRender->UncacheAllTextures();
}


//-----------------------------------------------------------------------------
// Purpose: Moves the camera forward by flDistance units. Negative units move back.
//-----------------------------------------------------------------------------
void CMapView3D::MoveForward(float flDistance)
{
	if (m_pCamera != NULL)
	{
		m_pCamera->MoveForward(flDistance);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Moves the camera up by flDistance units. Negative units move down.
//-----------------------------------------------------------------------------
void CMapView3D::MoveUp(float flDistance)
{
	if (m_pCamera != NULL)
	{
		m_pCamera->MoveUp(flDistance);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Moves the camera right by flDistance units. Negative units move left.
//-----------------------------------------------------------------------------
void CMapView3D::MoveRight(float flDistance)
{
	if (m_pCamera != NULL)
	{
		m_pCamera->MoveRight(flDistance);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Pitches the camera forward by flDegrees degrees. Negative units pitch back.
//-----------------------------------------------------------------------------
void CMapView3D::Pitch(float flDegrees)
{
	if (m_pCamera != NULL)
	{
		m_pCamera->Pitch(flDegrees);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Yaws the camera left by flDegrees degrees. Negative units yaw right.
//-----------------------------------------------------------------------------
void CMapView3D::Yaw(float flDegrees)
{
	if (m_pCamera != NULL)
	{
		m_pCamera->Yaw(flDegrees);
	}
}



void CMapView3D::WorldToClient(Vector2D &vClient, const Vector &vWorld)
{
	m_pCamera->WorldToView( vWorld, vClient );
}

void CMapView3D::ClientToWorld(Vector &vWorld, const Vector2D &vClient)
{
	m_pCamera->ViewToWorld( vClient, vWorld );
}

void CMapView3D::SetCursor( vgui::HCursor hCursor )
{
	// translate VGUI -> GDI cursors
    switch( hCursor )
	{
		case vgui::dc_arrow :		::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW)); break;
		case vgui::dc_sizenwse :	::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENWSE)); break;
		case vgui::dc_sizenesw :	::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENESW)); break;
		case vgui::dc_sizewe :		::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE)); break;
		case vgui::dc_sizens :		::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS)); break;
		case vgui::dc_sizeall :		::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEALL)); break;
		case vgui::dc_crosshair :	::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_CROSS)); break;
		default :					::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW)); break;
	}
}


LRESULT CMapView3D::WindowProc( UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
	case WM_KILLFOCUS:
		m_Keyboard.ClearKeyStates();
//		Msg( mwStatus, "debug: lost focus, clearing key states\n");
		break;
	}

	return CView::WindowProc( message, wParam, lParam ) ;
}