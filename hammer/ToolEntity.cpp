//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the entity/prefab placement tool.
//
//=============================================================================//

#include "stdafx.h"
#include "History.h"
#include "MainFrm.h"
#include "MapDefs.h"
#include "MapSolid.h"
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "Material.h"
#include "materialsystem/imesh.h"
#include "Render2D.h"
#include "Render3D.h"
#include "StatusBarIDs.h"
#include "TextureSystem.h"
#include "ToolEntity.h"
#include "ToolManager.h"
#include "hammer.h"
#include "vgui/Cursor.h"
#include "Selection.h"
#include "vstdlib/random.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//#pragma warning(disable:4244)


static HCURSOR s_hcurEntity = NULL;


class CToolEntityMessageWnd : public CWnd
{
	public:

		bool Create(void);
		void PreMenu2D(CToolEntity *pTool, CMapView2D *pView);

	protected:

		//{{AFX_MSG_MAP(CToolEntityMessageWnd)
		afx_msg void OnCreateObject();
		//}}AFX_MSG
	
		DECLARE_MESSAGE_MAP()

	private:

		CToolEntity *m_pToolEntity;
		CMapView2D *m_pView2D;
};


static CToolEntityMessageWnd s_wndToolMessage;
static const char *g_pszClassName = "ValveEditor_EntityToolWnd";


BEGIN_MESSAGE_MAP(CToolEntityMessageWnd, CWnd)
	//{{AFX_MSG_MAP(CToolMessageWnd)
	ON_COMMAND(ID_CREATEOBJECT, OnCreateObject)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Creates the hidden window that receives context menu commands for the
//			entity tool.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntityMessageWnd::Create(void)
{
	WNDCLASS wndcls;
	memset(&wndcls, 0, sizeof(WNDCLASS));
    wndcls.lpfnWndProc   = AfxWndProc;
    wndcls.hInstance     = AfxGetInstanceHandle();
    wndcls.lpszClassName = g_pszClassName;

	if (!AfxRegisterClass(&wndcls))
	{
		return(false);
	}

	return(CWnd::CreateEx(0, g_pszClassName, g_pszClassName, 0, CRect(0, 0, 10, 10), NULL, 0) == TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: Attaches the entity tool to this window before activating the context
//			menu.
//-----------------------------------------------------------------------------
void CToolEntityMessageWnd::PreMenu2D(CToolEntity *pToolEntity, CMapView2D *pView)
{
	Assert(pToolEntity != NULL);
	m_pToolEntity = pToolEntity;
	m_pView2D = pView;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolEntityMessageWnd::OnCreateObject()
{
	m_pToolEntity->CreateMapObject(m_pView2D);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CToolEntity::CToolEntity(void)
{
	SetEmpty();

	m_vecPos.Init();
	
	if (s_hcurEntity == NULL)
	{
		s_hcurEntity = LoadCursor(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_ENTITY));
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CToolEntity::~CToolEntity(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			BOOL - 
// Output : 
//-----------------------------------------------------------------------------
int CToolEntity::HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles)
{
	return HitRect( pView, ptClient, m_vecPos, 8 )?TRUE:FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bSave - 
//-----------------------------------------------------------------------------
void CToolEntity::FinishTranslation(bool bSave)
{
	if (bSave)
	{
		TranslatePoint( m_vecPos );
		m_bEmpty = false;
	}

	Tool3D::FinishTranslation(bSave);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			uFlags - 
//			size - 
// Output : Returns true if the translation delta was nonzero.
//-----------------------------------------------------------------------------
bool CToolEntity::UpdateTranslation( const Vector &vUpdate, UINT uFlags)
{
	Vector vOldDelta = m_vTranslation;

	if ( !Tool3D::UpdateTranslation( vUpdate, uFlags ) )
		return false;

	// apply snap to grid constrain
	if ( uFlags )
	{
		ProjectOnTranslationPlane( m_vecPos + m_vTranslation, m_vTranslation, uFlags );
		m_vTranslation -= m_vecPos;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CToolEntity::RenderTool2D(CRender2D *pRender)
{
	Vector v = m_vecPos;
	
	if ( IsTranslating() )
	{
		TranslatePoint( v );
	}
	else if ( IsEmpty() )
	{
		return;
	}
	
	pRender->SetDrawColor( 35, 255, 75 );

	//
	// Draw center rect.
	//
	pRender->DrawRectangle( v, v, false, 6.0f );

	//
	// Draw crosshair
	//
 	pRender->DrawLine( Vector( g_MIN_MAP_COORD, v.y, v.z), Vector( g_MAX_MAP_COORD, v.y , v.z) );
	pRender->DrawLine( Vector( v.x, g_MIN_MAP_COORD, v.z), Vector( v.x, g_MAX_MAP_COORD, v.z) );
	pRender->DrawLine( Vector( v.x, v.y, g_MIN_MAP_COORD), Vector( v.x, v.y, g_MAX_MAP_COORD) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - 
// Output : 
//-----------------------------------------------------------------------------
bool CToolEntity::OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	if (!IsEmpty())
	{
		CMapDoc *pDoc = pView->GetMapDoc();
		if (pDoc == NULL)
		{
			return true;
		}

		if (!pView->PointInClientRect(vPoint))
		{
			return true;
		}

		if ( HitTest( pView, vPoint, false) )
		{
			static CMenu menu, menuCreate;
			static bool bInit = false;

			if (!bInit)
			{
				bInit = true;
				menu.LoadMenu(IDR_POPUPS);
				menuCreate.Attach(::GetSubMenu(menu.m_hMenu, 1));

				// Create the window that handles menu messages.
				s_wndToolMessage.Create();
			}

			CPoint ptScreen( vPoint.x,vPoint.y);
			pView->ClientToScreen(&ptScreen);

			s_wndToolMessage.PreMenu2D(this, pView);
			menuCreate.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y, &s_wndToolMessage);
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//			nChar - 
//			nRepCnt - 
//			nFlags - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntity::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	switch (nChar)
	{
		case VK_RETURN:
		{
			if (!IsEmpty())
			{
				CreateMapObject(pView);
			}
			return true;
		}

		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			nFlags - 
//			point - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntity::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	unsigned int uConstraints = GetConstraints( nFlags );

	Tool3D::OnLMouseDown2D(pView, nFlags, vPoint);

	if ( HitTest( pView, vPoint, false) )
	{
		// translate existing object
		StartTranslation( pView, vPoint );
	}
	else
	{
		Vector vecWorld;
		pView->ClientToWorld(vecWorld, vPoint );

		//
		// Snap starting position to grid.
		//
		if ( uConstraints & constrainSnap )
			m_pDocument->Snap(vecWorld, uConstraints);
		
		// create new one, keep old third axis
		m_vecPos[pView->axHorz] = vecWorld[pView->axHorz];
		m_vecPos[pView->axVert] = vecWorld[pView->axVert];
		m_bEmpty = false;
		StartTranslation( pView, vPoint );
	}

	return true;
}

// set temp transformation plane 

void CToolEntity::StartTranslation( CMapView *pView, const Vector2D &vPoint )
{
	Vector vOrigin, v1,v2,v3;

	pView->GetBestTransformPlane( v1,v2,v3 );

	SetTransformationPlane(m_vecPos, v1, v2, v3 );
	// align translation plane to world origin
	ProjectOnTranslationPlane( vec3_origin, vOrigin, 0 );
	// set transformation plane
	SetTransformationPlane(vOrigin, v1, v2, v3 );

	Tool3D::StartTranslation( pView, vPoint, false );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Pre CWnd::OnLButtonUp.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntity::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseUp2D(pView, nFlags, vPoint);

	if (IsTranslating())
	{
		FinishTranslation( true );
	}

	m_pDocument->UpdateStatusbar();

	return true;
}


//-----------------------------------------------------------------------------
// Returns true if the message was handled, false otherwise.
//-----------------------------------------------------------------------------
bool CToolEntity::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnMouseMove2D(pView, nFlags, vPoint);
	
	vgui::HCursor hCursor = vgui::dc_arrow;

	unsigned int uConstraints = GetConstraints( nFlags );

	// Convert to world coords.
	
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);

	// Update status bar position display.

	char szBuf[128];

	if ( uConstraints & constrainSnap )
		m_pDocument->Snap(vecWorld,uConstraints);

	sprintf(szBuf, " @%.0f, %.0f ",  vecWorld[pView->axHorz], vecWorld[pView->axVert] );
	SetStatusText(SBI_COORDS, szBuf);

	//
	// If we are currently dragging the marker, update that operation based on
	// the current cursor position and keyboard state.
	//
	if (IsTranslating())
	{
		Tool3D::UpdateTranslation( pView, vPoint, uConstraints );

		// Don't change the cursor while dragging - it should remain a cross.
		hCursor = vgui::dc_none;
	}
	else if (!IsEmpty())
	{
		// Don't change the cursor while dragging - it should remain a cross.
		hCursor = vgui::dc_crosshair;
	}

	if ( hCursor != vgui::dc_none  )
		pView->SetCursor( hCursor );

	return true;
}


//-----------------------------------------------------------------------------
// Returns true if the message was handled, false otherwise.
//-----------------------------------------------------------------------------
bool CToolEntity::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	return true;
}


//-----------------------------------------------------------------------------
// Returns true if the message was handled, false otherwise.
//-----------------------------------------------------------------------------
bool CToolEntity::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CMapDoc *pDoc = pView->GetMapDoc();
	if (pDoc == NULL)
	{
		return false;
	}

	switch (nChar)
	{
		case VK_RETURN:
		{
			//
			// Create the entity or prefab.
			//
			if (!IsEmpty())
			{
				//CreateMapObject(pView); // TODO: support in 3D
			}
			return true;
		}

		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void CToolEntity::OnEscape(void)
{
	//
	// Cancel the object creation tool.
	//
	if (!IsEmpty())
	{
		SetEmpty();
	}
	else
	{
		ToolManager()->SetTool(TOOL_POINTER);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//			nFlags - 
//			point - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntity::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	ULONG ulFace;
	VMatrix LocalMatrix, LocalMatrixNeg;
	CMapClass *pObject = pView->NearestObjectAt( vPoint, ulFace, FLAG_OBJECTS_AT_RESOLVE_INSTANCES, &LocalMatrix );
	Tool3D::OnLMouseDown3D(pView, nFlags, vPoint);

	if (pObject != NULL)
	{
		CMapSolid *pSolid = dynamic_cast <CMapSolid *> (pObject);
		if (pSolid == NULL)
		{
			// Clicked on a point entity - do nothing.
			return true;
		}

		LocalMatrix.InverseTR( LocalMatrixNeg );

		// Build a ray to trace against the face that they clicked on to
		// find the point of intersection.

		Vector Start,End;
		pView->GetCamera()->BuildRay( vPoint, Start, End);

		Vector HitPos, HitNormal;
		CMapFace *pFace = pSolid->GetFace(ulFace);
		Vector vFinalStart, vFinalEnd;
		LocalMatrixNeg.V3Mul( Start, vFinalStart );
		LocalMatrixNeg.V3Mul( End, vFinalEnd );
		if (pFace->TraceLine( HitPos, HitNormal, vFinalStart, vFinalEnd))
		{
			Vector vFinalHitPos, vFinalHitNormal;
			LocalMatrix.V3Mul( HitPos, vFinalHitPos );
			vFinalHitNormal = LocalMatrix.ApplyRotation( HitNormal );
			CMapClass *pNewObject = NULL;

			if (GetMainWnd()->m_ObjectBar.IsEntityToolCreatingPrefab())
			{
				//
				// Prefab creation.
				//
				unsigned int uConstraints = GetConstraints( nFlags );
				m_pDocument->Snap(vFinalHitPos,uConstraints);

				GetHistory()->MarkUndoPosition(m_pDocument->GetSelection()->GetList(), "New Prefab");

				// Get prefab object
				CMapClass *pPrefabObject = GetMainWnd()->m_ObjectBar.BuildPrefabObjectAtPoint(vFinalHitPos);

				//
				// Add prefab to the world.
				//
				CMapWorld *pWorld = m_pDocument->GetMapWorld();
				m_pDocument->ExpandObjectKeywords(pPrefabObject, pWorld);

				pNewObject = pPrefabObject;
			}
			else if (GetMainWnd()->m_ObjectBar.IsEntityToolCreatingEntity())
			{
				//
				// Entity creation.
				//
				GetHistory()->MarkUndoPosition(m_pDocument->GetSelection()->GetList(), "New Entity");

				CMapEntity *pEntity = new CMapEntity;
				pEntity->SetPlaceholder(TRUE);
				pEntity->SetOrigin(vFinalHitPos);
				pEntity->SetClass(CObjectBar::GetDefaultEntityClass());

				VPlane	BeforeTransform( pFace->plane.normal, pFace->plane.dist ), AfterTransform;
				LocalMatrix.TransformPlane( BeforeTransform, AfterTransform );

				PLANE	NewPlane;

				NewPlane.dist = AfterTransform.m_Dist;
				NewPlane.normal = AfterTransform.m_Normal;
				// Align the entity on the plane properly
				pEntity->AlignOnPlane(vFinalHitPos, &NewPlane, (vFinalHitNormal.z > 0.0f) ? CMapEntity::ALIGN_BOTTOM : CMapEntity::ALIGN_TOP);

				pNewObject = pEntity;
			}

			if ( pNewObject )
			{
				if ( GetMainWnd()->m_ObjectBar.UseRandomYawOnEntityPlacement() )
				{
					// They checked "random yaw" on the object bar, so come up with a random yaw.
					VMatrix vmRotate, vmT1, vmT2;
					Vector vOrigin;
					QAngle angRandom( 0, RandomInt( -180, 180 ), 0 );

					pNewObject->GetOrigin( vOrigin );

					// Setup a matrix that translates them to the origin, rotates it, then translates back.
					MatrixFromAngles( angRandom, vmRotate );
					MatrixBuildTranslation( vmT1, -vOrigin );
					MatrixBuildTranslation( vmT2, vOrigin );

					// Transform the object.
					pNewObject->Transform( vmT2 * vmRotate * vmT1 );
				}

				m_pDocument->AddObjectToWorld( pNewObject );
				GetHistory()->KeepNew( pNewObject );

				// Select the new object.
				m_pDocument->SelectObject( pNewObject, scClear|scSelect|scSaveChanges );

				m_pDocument->SetModifiedFlag();
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Renders a selection gizmo at our bounds center.
// Input  : pRender - Rendering interface.
//-----------------------------------------------------------------------------
void CToolEntity::RenderTool3D(CRender3D *pRender)
{
	Vector pos = m_vecPos;

	if ( IsTranslating() )
	{
		TranslatePoint( pos );
	}
	else if ( IsEmpty() )
	{
		return;
	}
	
	//
	// Setup the renderer.
	//
	pRender->PushRenderMode( RENDER_MODE_WIREFRAME);

	CMeshBuilder meshBuilder;
	CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	meshBuilder.Begin(pMesh, MATERIAL_LINES, 3);

	meshBuilder.Position3f(g_MIN_MAP_COORD, pos.y, pos.z);
	meshBuilder.Color3ub(255, 0, 0);
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f(g_MAX_MAP_COORD, pos.y, pos.z);
	meshBuilder.Color3ub(255, 0, 0);
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f(pos.x, g_MIN_MAP_COORD, pos.z);
	meshBuilder.Color3ub(0, 255, 0);
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f(pos.x, g_MAX_MAP_COORD, pos.z);
	meshBuilder.Color3ub(0, 255, 0);
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f(pos.x, pos.y, g_MIN_MAP_COORD);
	meshBuilder.Color3ub(0, 0, 255);
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3f(pos.x, pos.y, g_MAX_MAP_COORD);
	meshBuilder.Color3ub(0, 0, 255);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRender->PopRenderMode();
}


void CToolEntity::CreateMapObject(CMapView2D *pView)
{
	CMapWorld *pWorld = m_pDocument->GetMapWorld();
	CMapClass *pobj = NULL;

	//
	// Handle prefab creation.
	//
	if (GetMainWnd()->m_ObjectBar.IsEntityToolCreatingPrefab())
	{			
		GetHistory()->MarkUndoPosition(m_pDocument->GetSelection()->GetList(), "New Prefab");

		CMapClass *pPrefabObject = GetMainWnd()->m_ObjectBar.BuildPrefabObjectAtPoint(m_vecPos);

		if (pPrefabObject == NULL)
		{
			pView->MessageBox("Unable to load prefab", "Error", MB_OK);
			SetEmpty();
			return;
		}

		m_pDocument->ExpandObjectKeywords(pPrefabObject, pWorld);
		m_pDocument->AddObjectToWorld(pPrefabObject);

		GetHistory()->KeepNew(pPrefabObject);

		pobj = pPrefabObject;
	}
	//
	// Handle entity creation.
	//
	else if (GetMainWnd()->m_ObjectBar.IsEntityToolCreatingEntity())
	{
		GetHistory()->MarkUndoPosition(m_pDocument->GetSelection()->GetList(), "New Entity");
		
		CMapEntity *pEntity = new CMapEntity;
		
		pEntity->SetPlaceholder(TRUE);
		pEntity->SetOrigin(m_vecPos);
		pEntity->SetClass(CObjectBar::GetDefaultEntityClass());

		m_pDocument->AddObjectToWorld(pEntity);
		
		pobj = pEntity;
		
		GetHistory()->KeepNew(pEntity);
	}

	//
	// Select the new object.
	//
	m_pDocument->SelectObject(pobj, scClear |scSelect|scSaveChanges);

	SetEmpty();

	m_pDocument->SetModifiedFlag();
}


