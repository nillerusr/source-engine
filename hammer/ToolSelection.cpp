//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "stdafx.h"
#include "Gizmo.h"
#include "GlobalFunctions.h"		// FIXME: For NotifyDuplicates
#include "History.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "MapDefs.h"
#include "MapEntity.h"
#include "MapPointHandle.h"
#include "MapSolid.h"
#include "MapView2D.h"
#include "MapViewLogical.h"
#include "MapView3D.h"
#include "ObjectProperties.h"
#include "Options.h"
#include "Render2D.h"
#include "ToolSelection.h"
#include "StatusBarIDs.h"
#include "ToolManager.h"
#include "hammer.h"
#include "vgui/Cursor.h"
#include "mapdecal.h"
#include "RenderUtils.h"
#include "tier0/icommandline.h"
#include "Manifest.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#pragma warning(disable:4244)


// For debugging mouse messages
//static int _nMouseMove = 0;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Selection3D::Selection3D(void)
{
	// The block tool uses our bounds as the default size when starting a new
	// box. Set to reasonable defaults to begin with.

	m_bIsLogicalTranslating = false;
	m_bInLogicalBoxSelection = false;
	m_bBoxSelection = false;
		
	m_bEyedropper = false;
	m_b3DEditMode = false;
	m_bSelected = false;
	m_bLButtonDown = false;
	m_bLeftDragged = false;
	m_bDrawAsSolidBox = false;

	SetDrawFlags(Box3D::expandbox | Box3D::boundstext);
	SetDrawColors(Options.colors.clrToolHandle, Options.colors.clrToolSelection);
	m_clrLogicalBox = Options.colors.clrToolSelection;
	m_vLDownLogicalClient.Init();

	m_pSelection = NULL;
}


void Selection3D::Init( CMapDoc *pDocument )
{
	Box3D::Init( pDocument );
	m_pSelection = pDocument->GetSelection();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Selection3D::~Selection3D(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: Called when the tool is activated.
// Input  : eOldTool - The ID of the previously active tool.
//-----------------------------------------------------------------------------
void Selection3D::OnActivate()
{
	EnableHandles(true);
}


//-----------------------------------------------------------------------------
// Purpose: Called when the tool is deactivated.
// Input  : eNewTool - The ID of the tool that is being activated.
//-----------------------------------------------------------------------------
void Selection3D::OnDeactivate()
{
	EnableHandles(false);
}


//-----------------------------------------------------------------------------
// Purpose: Enables or disables the selection handles based on the current
//			state of the tool.
//-----------------------------------------------------------------------------
void Selection3D::UpdateHandleState(void)
{
	if ( !IsActiveTool() || m_pSelection->IsEditable() == false )
	{
		EnableHandles(false);
	}
	else
	{
		EnableHandles(true);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - The view that invoked the eyedropper.
//			VarList - 
// Output : 
//-----------------------------------------------------------------------------
GDinputvariable *Selection3D::ChooseEyedropperVar(CMapView *pView, CUtlVector<GDinputvariable *> &VarList)
{
	//
	// Build a popup menu containing all the variable names.
	//
	CMenu menu;
	menu.CreatePopupMenu();
	int nVarCount = VarList.Count();
	for (int nVar = 0; nVar < nVarCount; nVar++)
	{
		GDinputvariable *pVar = VarList.Element(nVar);
		menu.AppendMenu(MF_STRING, nVar + 1, pVar->GetLongName());
	}

	//
	// Invoke the popup menu.
	//
	CPoint point;
	GetCursorPos(&point);
	int nID = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, point.x, point.y, NULL, NULL);
	if (nID == 0)
	{
		return NULL;
	}

	return VarList.Element(nID - 1);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			bValidOnly - 
// Output : Returns the handle under the given point, -1 if there is none.
//-----------------------------------------------------------------------------
int Selection3D::HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles)
{
	if (!IsEmpty())
	{
		return Box3D::HitTest(pView, ptClient, bTestHandles);
	}

	return FALSE;
}

bool Selection3D::HitTestLogical( CMapView *pView, const Vector2D &ptClient )
{	
	Vector2D vecLogicalMins, vecLogicalMaxs;
	if ( !m_pSelection->GetLogicalBounds(vecLogicalMins, vecLogicalMaxs) )
		return false;

	// Build a rect from our bounds to hit test against.
	Vector2D vecMinClient, vecMaxClient;
	Vector vecMins( vecLogicalMins.x, vecLogicalMins.y, 0.0f );
	Vector vecMaxs( vecLogicalMaxs.x, vecLogicalMaxs.y, 0.0f );

	pView->WorldToClient( vecMinClient, vecMins );
	pView->WorldToClient( vecMaxClient, vecMaxs );

	CRect rect(vecMinClient.x, vecMinClient.y, vecMaxClient.x, vecMaxClient.y);
	rect.NormalizeRect();

	// See if the point lies within the main rect.
	return rect.PtInRect( CPoint( ptClient.x, ptClient.y ) );
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Selection3D::SetEmpty(void)
{
	m_vTranslation.Init();
	m_bIsTranslating = false;
	m_pSelection->SelectObject(NULL,scClear);
	UpdateSelectionBounds();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool Selection3D::IsEmpty(void)
{
	return (m_bBoxSelection || m_pSelection->GetCount()) ? false : true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void Selection3D::UpdateSelectionBounds( void )
{
	if ( !m_pSelection->GetBounds( bmins, bmaxs ) )
	{
		ResetBounds();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt3 - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
bool Selection3D::StartBoxSelection( CMapView *pView, const Vector2D &vPoint, const Vector &vStart)
{
	m_bBoxSelection = true;
	
	Box3D::StartNew( pView, vPoint, vStart, Vector(0,0,0) );
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Selection3D::EndBoxSelection()
{
	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL | MAPVIEW_UPDATE_SELECTION );
	m_bBoxSelection = false;
}


//-----------------------------------------------------------------------------
// Start, end logical selection
//-----------------------------------------------------------------------------
void Selection3D::StartLogicalBoxSelection( CMapViewLogical *pView, const Vector &vStart )
{
	m_bInLogicalBoxSelection = true;
	m_clrLogicalBox = RGB( 50, 255, 255 );
	m_vecLogicalSelBoxMins = m_vecLogicalSelBoxMaxs = vStart.AsVector2D();
}

void Selection3D::EndLogicalBoxSelection( )
{
	m_clrLogicalBox = Options.colors.clrToolSelection;
	m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL | MAPVIEW_UPDATE_SELECTION );
	m_bInLogicalBoxSelection = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Selection3D::TransformSelection(void)
{
	// Transform the selected objects.
	const CMapObjectList *pSelList = m_pSelection->GetList();
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pobj = pSelList->Element(i);
		pobj->Transform( GetTransformMatrix() );
	}

	m_pDocument->SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Selection3D::TransformLogicalSelection( const Vector2D &vecTranslation )
{
	// Transform the selected objects.
	const CMapObjectList *pSelList = m_pSelection->GetList();
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObj = pSelList->Element(i);
		Vector2D vecNewPosition;
		Vector2DAdd( pObj->GetLogicalPosition(), vecTranslation, vecNewPosition );
		pObj->SetLogicalPosition( vecNewPosition );
	}

	// The transformation may have changed some entity properties (such as the "angles" key),
	// so we must refresh the Object Properties dialog.
	GetMainWnd()->pObjectProperties->MarkDataDirty();
}


//-----------------------------------------------------------------------------
// Purpose: Draws objects when they are selected. Odd, how this code is stuck
//			in this obscure place, away from all the other 2D rendering code.
// Input  : pobj - Object to draw.
//			pSel - 
// Output : Returns TRUE to keep enumerating.
//-----------------------------------------------------------------------------
static BOOL DrawObject(CMapClass *pobj, CRender *pRender)
{
	if ( !pobj->IsVisible() )
		return true;

	// switch selection mode so transformed object is drawn normal
	pobj->SetSelectionState( SELECT_NONE );

	CRender2D *pRender2D = dynamic_cast<CRender2D*>(pRender);

	if ( pRender2D )
		pobj->Render2D(pRender2D);

	CRender3D *pRender3D = dynamic_cast<CRender3D*>(pRender);

	if ( pRender3D )
		pobj->Render3D(pRender3D);

	pobj->SetSelectionState( SELECT_MODIFY );

	return TRUE;
}


static BOOL DrawObjectLogical( CMapClass *pObj, CRender2D *pRender2D )
{
	if ( !pObj->IsVisibleLogical() )
		return true;

	// switch selection mode so transformed object is drawn normal
	pObj->SetSelectionState( SELECT_NONE );

	if ( pRender2D )
	{
		pObj->RenderLogical( pRender2D );
	}

	pObj->SetSelectionState( SELECT_MODIFY );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRender - 
//-----------------------------------------------------------------------------
void Selection3D::RenderTool2D(CRender2D *pRender)
{
	if ( !m_pSelection->IsEmpty() && IsTranslating() && !IsBoxSelecting() )
	{
		//
		// Even if this is not the active tool, selected objects should be rendered
		// with the selection color.
		//
		COLORREF clr = Options.colors.clrSelection;

		pRender->SetDrawColor( GetRValue(clr), GetGValue(clr), GetBValue(clr) );

		VMatrix matrix = GetTransformMatrix();

		pRender->BeginLocalTransfrom( matrix );

		const CMapObjectList *pSelList = m_pSelection->GetList();
		for (int i = 0; i < pSelList->Count(); i++)
		{
			CMapClass *pobj = pSelList->Element(i);
			
			DrawObject(pobj, pRender);
			pobj->EnumChildren((ENUMMAPCHILDRENPROC)DrawObject, (DWORD)pRender);
		}

		pRender->EndLocalTransfrom();
	}
	else if ( !IsBoxSelecting() )
	{
		UpdateSelectionBounds();
	}

	Box3D::RenderTool2D(pRender);
}


//-----------------------------------------------------------------------------
// Render tool in visio view
//-----------------------------------------------------------------------------
void Selection3D::RenderToolLogical( CRender2D *pRender )
{
	if ( !m_pSelection->IsEmpty() && m_bIsLogicalTranslating && !IsLogicalBoxSelecting() )
	{
		// Even if this is not the active tool, selected objects should be rendered
		// with the selection color.
		COLORREF clr = Options.colors.clrSelection;

		pRender->SetDrawColor( GetRValue(clr), GetGValue(clr), GetBValue(clr) );

		VMatrix matrix = GetTransformMatrix();
		MatrixBuildTranslation( matrix, m_vLogicalTranslation.x, m_vLogicalTranslation.y, 0.0f );
		pRender->BeginLocalTransfrom( matrix );

		const CMapObjectList *pSelList = m_pSelection->GetList();
		for (int i = 0; i < pSelList->Count(); i++)
		{
			CMapClass *pobj = pSelList->Element(i);
			
			DrawObjectLogical(pobj, pRender);
			pobj->EnumChildren((ENUMMAPCHILDRENPROC)DrawObjectLogical, (DWORD)pRender);
		}

		pRender->EndLocalTransfrom();
	}
	
	Vector2D vecLogicalMins, vecLogicalMaxs;
	if ( IsLogicalBoxSelecting() )
	{
		vecLogicalMins = m_vecLogicalSelBoxMins;
		vecLogicalMaxs = m_vecLogicalSelBoxMaxs;
	}
	else if ( !m_pSelection->GetLogicalBounds( vecLogicalMins, vecLogicalMaxs ) )
		return;

	

	Vector mins( vecLogicalMins.x, vecLogicalMins.y, 0.0f );
	Vector maxs( vecLogicalMaxs.x, vecLogicalMaxs.y, 0.0f );

	Assert( pRender );
	pRender->PushRenderMode( RENDER_MODE_DOTTED );
	pRender->SetDrawColor( GetRValue(Options.colors.clrToolDrag), GetGValue(Options.colors.clrToolDrag), GetBValue(Options.colors.clrToolDrag) );
	pRender->DrawRectangle( mins, maxs, false, 2 );
	pRender->PopRenderMode();
}

	
//-----------------------------------------------------------------------------
// Purpose: Renders a selection gizmo at our bounds center.
// Input  : pRender - 
//-----------------------------------------------------------------------------
void Selection3D::RenderTool3D(CRender3D *pRender)
{
	const CMapObjectList *pSelList = m_pSelection->GetList();

	if ( m_bDrawAsSolidBox )
	{
		// while picking draw Selection tool as solid box
		// so we cant pick stuff behind it
		if ( pSelList->Count() )
		{
			pRender->PushRenderMode( RENDER_MODE_FLAT );
			pRender->BeginRenderHitTarget( pSelList->Element(0) );
			pRender->RenderBox( bmins, bmaxs, 255,255,255, SELECT_NONE );
			pRender->EndRenderHitTarget();
			pRender->PopRenderMode();
		}
		return;
	}

	else if ( !m_pSelection->IsEmpty() && IsTranslating() && !IsBoxSelecting() )
	{
		//
		// Even if this is not the active tool, selected objects should be rendered
		// with the selection color.
		//
		COLORREF clr = Options.colors.clrSelection;

		pRender->SetDrawColor( GetRValue(clr), GetGValue(clr), GetBValue(clr) );

		VMatrix matrix = GetTransformMatrix();
		pRender->BeginLocalTransfrom( matrix );

		
		for (int i = 0; i < pSelList->Count(); i++)
		{
			CMapClass *pobj = pSelList->Element(i);

			DrawObject(pobj, pRender);
			pobj->EnumChildren((ENUMMAPCHILDRENPROC)DrawObject, (DWORD)pRender);
		}

		pRender->EndLocalTransfrom();

		if ( m_pDocument->m_bShowGrid && m_b3DEditMode )
			RenderTranslationPlane( pRender );
	}
	else if ( !IsBoxSelecting() )
	{
		UpdateSelectionBounds();
	}

	if ( m_b3DEditMode )
	{
		Box3D::RenderTool3D(pRender);
	}
}

CBaseTool *Selection3D::GetToolObject( CMapView2D *pView, const Vector2D &vPoint, bool bAttach )
{

	const CMapObjectList *pSelList = m_pSelection->GetList();
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);

		//
		// Hit test against the object. nHitData will return with object-specific
		// information about what was clicked on.
		//
		HitInfo_t HitData;
		if ( pObject->HitTest2D(pView, vPoint, HitData) )
		{
			//
			// They clicked on some part of the object. See if there is a
			// tool associated with what we clicked on.
			//
			CBaseTool *pToolHit = HitData.pObject->GetToolObject(HitData.uData, bAttach );
			if ( pToolHit != NULL )
			{
				return pToolHit;
			}
		}
	}

	return NULL;
}

CBaseTool *Selection3D::GetToolObjectLogical( CMapViewLogical *pView, const Vector2D &vPoint, bool bAttach )
{
	const CMapObjectList *pSelList = m_pSelection->GetList();
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);

		//
		// Hit test against the object. nHitData will return with object-specific
		// information about what was clicked on.
		//
		HitInfo_t HitData;
		if ( pObject->HitTestLogical(pView, vPoint, HitData) )
		{
			//
			// They clicked on some part of the object. See if there is a
			// tool associated with what we clicked on.
			//
			CBaseTool *pToolHit = HitData.pObject->GetToolObject(HitData.uData, bAttach );
			if ( pToolHit != NULL )
			{
				return pToolHit;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - 
//-----------------------------------------------------------------------------
bool Selection3D::OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	// First give any selected tool helpers a chance to handle the message.
	// Don't hit test against tool helpers when shift is held down
	// (beginning a Clone operation).
		
	CBaseTool *pToolHit = GetToolObject( pView, vPoint, true );

	if ( pToolHit )
	{
		return pToolHit->OnContextMenu2D(pView, nFlags, vPoint);
	}

	static CMenu menu, menuSelection;
	static bool bInit = false;

	if (!bInit)
	{
		bInit = true;
		menu.LoadMenu(IDR_POPUPS);
		menuSelection.Attach(::GetSubMenu(menu.m_hMenu, 0));
	}

	if ( !pView->PointInClientRect( vPoint ) )
		return false;

	if (!IsEmpty() && !IsBoxSelecting())
	{
		if ( HitTest(pView, vPoint, false) )
		{
			CPoint ptScreen( vPoint.x,vPoint.y);
			pView->ClientToScreen(&ptScreen);
			menuSelection.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y, pView);
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - 
//-----------------------------------------------------------------------------
bool Selection3D::OnContextMenuLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint) 
{
	// First give any selected tool helpers a chance to handle the message.
	// Don't hit test against tool helpers when shift is held down
	// (beginning a Clone operation).
	CBaseTool *pToolHit = GetToolObjectLogical( pView, vPoint, true );
	if ( pToolHit )
		return pToolHit->OnContextMenuLogical(pView, nFlags, vPoint);

	static CMenu menu, menuSelection;
	static bool bInit = false;

	if (!bInit)
	{
		bInit = true;
		menu.LoadMenu(IDR_POPUPS);
		menuSelection.Attach(::GetSubMenu(menu.m_hMenu, 8));
	}

	if ( !pView->PointInClientRect( vPoint ) )
		return false;

	if (!IsEmpty() && !IsLogicalBoxSelecting())
	{
		if ( HitTestLogical( pView, vPoint ) )
		{
			CPoint ptScreen( vPoint.x, vPoint.y );
			pView->ClientToScreen(&ptScreen);
			menuSelection.TrackPopupMenu(TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y, pView);
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Selection3D::SelectInBox(CMapDoc *pDoc, bool bInsideOnly)
{
	BoundBox box(*this);
	EndBoxSelection();

	//
	// Make selection box "infinite" in 0-depth axes, of which there
	// should not be more than 1.
	//
	int countzero = 0;
	for(int i = 0; i < 3; i++)
	{
		if (box.bmaxs[i] == box.bmins[i])
		{
			box.bmins[i] = -COORD_NOTINIT;
			box.bmaxs[i] = COORD_NOTINIT;
			++countzero;
		}
	}

	if (countzero <= 1)
	{
		pDoc->SelectRegion(&box, bInsideOnly);
	}

	UpdateSelectionBounds();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Selection3D::SelectInLogicalBox(CMapDoc *pDoc, bool bInsideOnly)
{
	Vector2D mins = m_vecLogicalSelBoxMins;
	Vector2D maxs = m_vecLogicalSelBoxMaxs;

	// Make selection box "infinite" in 0-depth axes, of which there
	// should not be more than 1.
	int countzero = 0;
	for (int i = 0; i < 2; i++)
	{
		if (maxs[i] == mins[i])
		{
			mins[i] = -COORD_NOTINIT;
			maxs[i] = COORD_NOTINIT;
			++countzero;
		}
	}

	if (countzero <= 1)
	{
		pDoc->SelectLogicalRegion( mins, maxs, bInsideOnly );
	}

	UpdateSelectionBounds();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Selection3D::NudgeObjects(CMapView *pView, int nChar, bool bSnap, bool bClone)
{
	Vector vecDelta, vVert, vHorz, vThrd;

	pView->GetBestTransformPlane( vHorz, vVert, vThrd );

    m_pDocument->GetNudgeVector( vHorz, vVert,  nChar, bSnap, vecDelta);
	m_pDocument->NudgeObjects(vecDelta, bClone);

	CMapView2DBase *pView2D = dynamic_cast<CMapView2DBase*>(pView);

	if ( !pView2D )
		return;
	
	// Try to keep the selection fully in the view if it started that way.
	bool bFullyVisible = pView2D->IsBoxFullyVisible(bmins, bmaxs);

	// Make sure it can still fit entirely in the view after nudging and don't scroll the
	// view if it can't. This second check is probably unnecessary, but it can't hurt,
	// and there might be cases where the selection changes size after a nudge operation.
	if (bFullyVisible && pView2D->CanBoxFitInView(bmins, bmaxs))
	{
		pView2D->LockWindowUpdate();
		pView2D->EnsureVisible(bmins, 25);
		pView2D->EnsureVisible(bmaxs, 25);
		pView2D->UnlockWindowUpdate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles key down events in the 2D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Selection3D::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	bool bShift = ((GetKeyState(VK_SHIFT) & 0x8000) != 0);
	bool bCtrl = ((GetKeyState(VK_CONTROL) & 0x8000) != 0);

	if (Options.view2d.bNudge && (nChar == VK_UP || nChar == VK_DOWN || nChar == VK_LEFT || nChar == VK_RIGHT))
	{
		if (!IsEmpty())
		{
			bool bSnap = m_pDocument->IsSnapEnabled() && !bCtrl;
			NudgeObjects(pView, nChar, bSnap, bShift);
			return true;
		}
	}

	switch (nChar)
	{
		// TODO: do we want this here or in the view?
		case VK_DELETE:
		{
			m_pDocument->OnCmdMsg(ID_EDIT_DELETE, CN_COMMAND, NULL, NULL);
			break;
		}

		case VK_NEXT:
		{
			m_pDocument->OnCmdMsg(ID_EDIT_SELNEXT, CN_COMMAND, NULL, NULL);
			break;
		}

		case VK_PRIOR:
		{
			m_pDocument->OnCmdMsg(ID_EDIT_SELPREV, CN_COMMAND, NULL, NULL);
			break;
		}

		case VK_ESCAPE:
		{
			OnEscape(m_pDocument);
			break;
		}

		case VK_RETURN:
		{
			if (IsBoxSelecting())
			{
				SelectInBox(m_pDocument, bShift);
				UpdateHandleState();
			}
			break;
		}

		default:
		{
			return false;
		}
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles key down events in the logical view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Selection3D::OnKeyDownLogical(CMapViewLogical *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	bool bShift = ((GetKeyState(VK_SHIFT) & 0x8000) != 0);
	bool bAlt = GetKeyState(VK_MENU) < 0;

	/* FIXME
	if ( Options.view2d.bNudge && ( nChar == VK_UP || nChar == VK_DOWN || nChar == VK_LEFT || nChar == VK_RIGHT ) )
	{
		if (!IsEmpty())
		{
			NudgeObjects2D(pView, nChar, !bCtrl, bShift);
			return true;
		}
	}
	*/
	switch (nChar)
	{
	// TODO: do we want this here or in the view?
	case VK_DELETE:
		m_pDocument->OnCmdMsg(ID_EDIT_DELETE, CN_COMMAND, NULL, NULL);
		break;

	case VK_NEXT:
		m_pDocument->OnCmdMsg( bAlt ? ID_EDIT_SELNEXT_CASCADING : ID_EDIT_SELNEXT, CN_COMMAND, NULL, NULL);
		break;

	case VK_PRIOR:
		m_pDocument->OnCmdMsg( bAlt ? ID_EDIT_SELPREV_CASCADING : ID_EDIT_SELPREV, CN_COMMAND, NULL, NULL);
		break;

	case VK_ESCAPE:
		OnEscape( m_pDocument );
		break;

	case VK_RETURN:
		if ( m_bInLogicalBoxSelection )
		{
			EndLogicalBoxSelection( );
			SelectInLogicalBox( m_pDocument, bShift );
		}
		break;

	default:
		return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	// First give any selected tool helpers a chance to handle the message.
	// Don't hit test against tool helpers when shift is held down
	// (beginning a Clone operation).
	
	if (!(nFlags & MK_SHIFT))
	{
		CBaseTool *pToolHit = GetToolObject( pView, vPoint, true );

		if (pToolHit)
		{
			// There is a tool. Attach the object to the tool and forward
			// the message to the tool.
			return pToolHit->OnLMouseDown2D(pView, nFlags, vPoint);
		}
	}

	Tool3D::OnLMouseDown2D(pView, nFlags, vPoint);

	m_bSelected = false;

	if ( IsBoxSelecting() )
	{
		// if we click outside of the current selection box, remove old box
		if ( !HitTest(pView, vPoint, true) )
		{
			EndBoxSelection();
		}
	}

	if (nFlags & MK_CONTROL)
	{
		// add object under cursor to selection
		m_bSelected = pView->SelectAt(vPoint, false, false);
		UpdateHandleState();
	}
	else if ( IsEmpty() || !HitTest(pView,vPoint, true) )
	{
		// start new selection
		m_TranslateMode = modeScale;
		m_bSelected = pView->SelectAt(vPoint, true, false);
		UpdateHandleState();
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the constraints flags for the translation.
// Input  : bDisableSnap - 
//			nKeyFlags - 
//-----------------------------------------------------------------------------
unsigned int Selection3D::GetConstraints(unsigned int nKeyFlags)
{
	unsigned int uConstraints = Tool3D::GetConstraints( nKeyFlags );

	if ( m_TranslateMode==modeRotate )
	{
		// backwards capability, SHIFT turns snapping off during rotation
		if ( (nKeyFlags & MK_SHIFT) || !Options.view2d.bRotateConstrain )
		{
			uConstraints = 0;
		}
	}

	if ( uConstraints & constrainSnap )
	{
		if ( m_pSelection->GetCount() == 1)
		{
			CMapClass *pObject = m_pSelection->GetList()->Element(0);

			if (pObject->ShouldSnapToHalfGrid())
			{
				uConstraints |= constrainHalfSnap;
			}
		}
	}

	return uConstraints;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	Tool3D::OnMouseMove2D(pView, nFlags, vPoint);

	bool	IsEditable = m_pSelection->IsEditable();

	vgui::HCursor hCursor = vgui::dc_arrow;

	bool bCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000);
	unsigned int uConstraints = GetConstraints( nFlags);
			
	// Convert to world coords.
	
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);

	//
	// Update status bar position display.
	//
	char szBuf[128];
	sprintf(szBuf, " @%.0f, %.0f ", vecWorld[pView->axHorz], vecWorld[pView->axVert]);
	SetStatusText(SBI_COORDS, szBuf);

	//
	// If we are currently dragging the selection (moving, scaling, rotating, or shearing)
	// update that operation based on the current cursor position and keyboard state.
	//
	if ( IsTranslating() )
	{
		Tool3D::UpdateTranslation( pView, vPoint, uConstraints );
		
		hCursor = vgui::dc_none;
	}
	//
	// Else if we have just started dragging the selection, begin a new translation
	//
	else if ( m_bMouseDragged[MOUSE_LEFT] )
	{
		pView->SetCapture();

		if ( IsEditable && !bCtrl && HitTest( pView, m_vMouseStart[MOUSE_LEFT], true) )
		{
			// we selected a handle - start translation the selection
			StartTranslation( pView, m_vMouseStart[MOUSE_LEFT], m_LastHitTestHandle ); 

			hCursor = UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );
		}
		else if ( !m_bSelected )
		{
			// start new box selection if we didnt select an addition object
			Vector ptOrg;
			pView->ClientToWorld(ptOrg, m_vMouseStart[MOUSE_LEFT] );

			// set best third axis value
			ptOrg[pView->axThird] = COORD_NOTINIT;
			m_pDocument->GetBestVisiblePoint(ptOrg);

			if ( uConstraints & constrainSnap )
				m_pDocument->Snap(ptOrg,uConstraints);

			StartBoxSelection( pView, m_vMouseStart[MOUSE_LEFT], ptOrg );
			EnableHandles(true);
		}
	}
	else if (!IsEmpty())
	{
		//DBG("(%d) OnMouseMove2D: Selection NOT empty, update cursor\n", _nMouseMove);

		//
		// Just in case the selection set is not empty and "selection" hasn't received a left mouse click.
		// (NOTE: this is gross, but unfortunately necessary (cab))
		//
		UpdateHandleState();

		//
		// If the cursor is on a handle, the cursor will be set by the HitTest code.
		//
		bool bFoundTool = false;

		if ( GetToolObject( pView, vPoint, false ) )
		{
			// If they moused over an interactive handle, it should have set the cursor.
			hCursor =  vgui::dc_crosshair;
			bFoundTool = true;
		}

		// If we haven't moused over any interactive handles contained in the object, see if the
		// mouse is over one of the selection handles.
		if ( IsEditable && !bFoundTool && HitTest(pView, vPoint, true) )
		{
			hCursor = UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );
		}
	}

	if ( hCursor != vgui::dc_none )
	{
		pView->SetCursor( hCursor );
	}

	return true;
}

void Selection3D::FinishTranslation(bool bSave, bool bClone )
{
	const CMapObjectList *pSelList = m_pSelection->GetList();

	// keep copy of current objects?
	if ( bClone && (GetTranslateMode() == modeMove))
	{
		GetHistory()->MarkUndoPosition(pSelList, "Clone Objects");
		m_pDocument->CloneObjects(*pSelList);
		GetHistory()->KeepNew(pSelList);
	}
	else
	{
		GetHistory()->MarkUndoPosition(pSelList, "Translation");
		GetHistory()->Keep(pSelList);
	}

	if ( bSave )
	{
		// transform selected objects
		TransformSelection();
	}

    // finish the tool translation
	Box3D::FinishTranslation( bSave );

	if ( bSave )
	{
		// update selection bounds
		UpdateSelectionBounds();
		NotifyDuplicates(pSelList);
	}

	m_pSelection->SetSelectionState( SELECT_NORMAL );
}

void Selection3D::StartTranslation(CMapView *pView, const Vector2D &vPoint, const Vector &vHandleOrigin )
{
	Vector refPoint;
    Vector *pRefPoint = NULL; 

	// use single object origin as translation origin
	if (m_pSelection->GetCount() == 1)
	{
		if ( vHandleOrigin.IsZero() || m_TranslateMode == modeRotate )
		{
			CMapEntity *pObject = (CMapEntity *)m_pSelection->GetList()->Element(0);

			if ( pObject->IsMapClass(MAPCLASS_TYPE(CMapEntity)) && pObject->IsPlaceholder() )
			{
				// set entity origin as translation center
				pObject->GetOrigin( refPoint );
				pRefPoint = &refPoint;
			}
		}
	}

	// we selected a handle - start translation the selection

	// If translating, redo our bounds temporarily to use the entity origins rather than their bounds
	// so things will stay on the grid correctly.
	Vector vCustomHandleBox[2];
	Vector *pCustomHandleBox = NULL;
	if ( vHandleOrigin.IsZero() )
	{
		pCustomHandleBox = vCustomHandleBox;
		m_pSelection->GetBoundsForTranslation( vCustomHandleBox[0], vCustomHandleBox[1] );
	}
	Box3D::StartTranslation( pView, vPoint, vHandleOrigin, pRefPoint, pCustomHandleBox ); 
	if ( !m_pSelection->IsEmpty() )
		UpdateSelectionBounds();

	m_pSelection->SetSelectionState( SELECT_MODIFY );
}

//-----------------------------------------------------------------------------
// Purpose: Handles left button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	bool bShift = nFlags & MK_SHIFT;

	Tool3D::OnLMouseUp2D(pView, nFlags, vPoint);

	bool	IsEditable = m_pSelection->IsEditable();

	if ( IsTranslating() )
	{
		// selecting stuff in box
		if ( IsBoxSelecting() )
		{
			Box3D::FinishTranslation(true);

			if (Options.view2d.bAutoSelect)
			{
				SelectInBox(m_pDocument, bShift);
				UpdateHandleState();
			}
		}
		else
		{
			FinishTranslation( true, bShift );
		}
			
	}
	else if ( !m_bSelected && !m_pSelection->IsEmpty() )
	{
		if ( IsEditable && HitTest(pView, vPoint, false) )
		{
			ToggleTranslateMode();

			UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );

			m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
		}
	}

	// we might have removed some stuff that was relevant:
	m_pDocument->UpdateStatusbar();
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnLMouseDownLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint)
{				 
	// First give any selected tool helpers a chance to handle the message.
	// Don't hit test against tool helpers when shift is held down
	// (beginning a Clone operation).
	
	if (!(nFlags & MK_SHIFT))
	{
		CBaseTool *pToolHit = GetToolObjectLogical( pView, vPoint, true );

		if (pToolHit)
		{
			// There is a tool. Attach the object to the tool and forward
			// the message to the tool.
			return pToolHit->OnLMouseDownLogical(pView, nFlags, vPoint);
		}
	}

	m_bLButtonDown = true;
	m_vLDownLogicalClient = vPoint;

	pView->SetCapture();

	m_bLeftDragged = false;
	m_bSelected = false;

 	if ( m_bInLogicalBoxSelection )
	{
		EndLogicalBoxSelection( );
	}

	// If they weren't alt- or ctrl-clicking and we have a selection, if they clicked
	// in the selection rectangle, maintain what we got.
	bool bCtrlClick = (nFlags & MK_CONTROL) != 0;
	bool bAltClick = GetKeyState(VK_MENU) < 0;
	if ( !bAltClick && !bCtrlClick && !IsEmpty() )
	{
		if ( HitTestLogical( pView, vPoint ) )
			return true;
	}
	 
	if ( bAltClick )
	{
		m_bSelected = ( pView->SelectAtCascading( vPoint, !bCtrlClick ) == true );
		return true;
	}

	m_bSelected = ( pView->SelectAt( vPoint, !bCtrlClick, false ) == true );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D visio view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnMouseMoveLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint) 
{			
	if ( m_bLButtonDown )
	{
		if ( !m_bLeftDragged )
		{
			// check if mouse was dragged if button is pressed down
			Vector2D sizeDragged = vPoint - m_vLDownLogicalClient;
			
			if ((abs(sizeDragged.x) > DRAG_THRESHHOLD) || (abs(sizeDragged.y) > DRAG_THRESHHOLD))
			{
				// If here, means we've dragged the mouse
				m_bLeftDragged = true;
			}
		}

		// Make sure the point is visible.
		pView->ToolScrollToPoint( vPoint );
	}

	// Convert to world coords.
	Vector2D vecWorld;
	pView->ClientToWorld( vecWorld, vPoint );

	// Update status bar position display.
	char szBuf[128];
	sprintf(szBuf, " @%.0f, %.0f ", vecWorld.x, vecWorld.y);
	SetStatusText( SBI_COORDS, szBuf );
		   
	// If we are currently dragging the selection (moving)
	// update that operation based on the current cursor position and keyboard state.
	if ( m_bIsLogicalTranslating )
	{			  
		Vector2D vecTranslation;
		Vector2DSubtract( vecWorld, m_vLastLogicalDragPoint, vecTranslation );
		m_vLastLogicalDragPoint = vecWorld;
		m_vLogicalTranslation += vecTranslation;
		pView->UpdateView( MAPVIEW_UPDATE_TOOL );
		return true;
	}

	if ( m_bInLogicalBoxSelection && (nFlags & MK_LBUTTON) )
	{
		Vector vecStartWorld;
		pView->ClientToWorld( vecStartWorld, m_vLDownLogicalClient );
		if ( vecWorld.x < vecStartWorld.x )
		{
			m_vecLogicalSelBoxMins.x = vecWorld.x;
			m_vecLogicalSelBoxMaxs.x = vecStartWorld.x;
		}
		else
		{
			m_vecLogicalSelBoxMins.x = vecStartWorld.x;
			m_vecLogicalSelBoxMaxs.x = vecWorld.x;
		}
		if ( vecWorld.y < vecStartWorld.y )
		{
			m_vecLogicalSelBoxMins.y = vecWorld.y;
			m_vecLogicalSelBoxMaxs.y = vecStartWorld.y;
		}
		else
		{
			m_vecLogicalSelBoxMins.y = vecStartWorld.y;
			m_vecLogicalSelBoxMaxs.y = vecWorld.y;
		}
		pView->UpdateView( MAPVIEW_UPDATE_TOOL );
		return true;
	}

	// If we have just started dragging the selection, begin a new translation
	if ( m_bLButtonDown && (nFlags & MK_LBUTTON) && m_bLeftDragged )
	{
		pView->SetCapture();

		// Check to see if the point at which we started clicking lies within the selection region
		if ( HitTestLogical( pView, m_vLDownLogicalClient ) )
		{
			pView->ClientToWorld( m_vLastLogicalDragPoint, m_vLDownLogicalClient );
			m_vLogicalTranslation.Init();
			m_bIsLogicalTranslating = true;
			pView->SetCursor( vgui::dc_sizeall );
			m_pSelection->SetSelectionState( SELECT_MODIFY );
		}
		else if ( !m_bSelected )
		{
			// We're doing a drag with the mouse down, and nothing is selected. 
			// Start a logical box selection
			Vector ptOrg;
			pView->ClientToWorld( ptOrg, m_vLDownLogicalClient );
			StartLogicalBoxSelection( pView, ptOrg );
		}

		return true;
	}
	
	// If we are simply hovering over an object but the mouse isn't down, update the cursor.
	vgui::HCursor hCursor = vgui::dc_arrow;
	if ( !IsEmpty() )
	{
		// If the cursor is on a handle, the cursor will be set by the HitTest code.
		bool bFoundTool = false;
		if ( GetToolObjectLogical( pView, vPoint, false ) )
		{
			// If they moused over an interactive handle, it should have set the cursor.
			hCursor =  vgui::dc_crosshair;
			bFoundTool = true;
		}
		  
		// If we haven't moused over any interactive handles contained in the object, see if the
		// mouse is over one of the selection handles.
		if ( !bFoundTool && HitTestLogical(pView, vPoint) )
		{
			hCursor = vgui::dc_sizeall;
		}
	}

	if ( hCursor != vgui::dc_none )
	{
		pView->SetCursor( hCursor );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnLMouseUpLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint) 
{
	bool bShift = ((GetKeyState(VK_SHIFT) & 0x8000) != 0);

  	ReleaseCapture();
	m_bLButtonDown = false;

	const CMapObjectList *pSelList = m_pSelection->GetList();

	// selecting stuff in box
	if ( m_bInLogicalBoxSelection )
	{
		if ( Options.view2d.bAutoSelect )
		{
			EndLogicalBoxSelection( );
			SelectInLogicalBox( m_pDocument, bShift );
		}

		m_pSelection->SetSelectionState( SELECT_NORMAL );
		goto updateStatusBar;
	}
		  
	if ( m_bIsLogicalTranslating )
	{
		// keep copy of current objects?
		if ( nFlags & MK_SHIFT )
		{
			GetHistory()->MarkUndoPosition(pSelList, "Clone Objects");
			m_pDocument->CloneObjects(*pSelList);
			GetHistory()->KeepNew(pSelList);
		}
		else
		{
			GetHistory()->MarkUndoPosition(pSelList, "Logical Translation");
			GetHistory()->Keep( pSelList );
		}

		TransformLogicalSelection( m_vLogicalTranslation );

		// finish the tool translation
		m_bIsLogicalTranslating = false;

		// update selection bounds
		UpdateSelectionBounds();

		m_pDocument->SetModifiedFlag();

		NotifyDuplicates( pSelList );

		m_pSelection->SetSelectionState( SELECT_NORMAL );
		goto updateStatusBar;
	}
	
updateStatusBar:
	// we might have removed some stuff that was relevant:
	m_pDocument->UpdateStatusbar();
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles key down events in the 3D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	bool bShift = ((GetKeyState(VK_SHIFT) & 0x8000) != 0);
	bool bCtrl = ((GetKeyState(VK_CONTROL) & 0x8000) != 0);
	
	switch (nChar)
	{
		/*
		dvs: The eyedropper is a somewhat failed experiment, an attempt to create a way to
			 quickly hook entities together. I think a dedicated connection tool with a more
			 rubber-band style UI might be more successful. Either that or relegate that work
			 to a Logical-style view.
		case 'e':
		case 'E':
		{
			m_bEyedropper = !m_bEyedropper;
			if (m_bEyedropper)
			{
				SetEyedropperCursor();
			}
			else
			{
				SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
			}
			return true;
		}
		*/
#ifndef SDK_BUILD
		case 'x':
		case 'X':
		{
			m_b3DEditMode = !m_b3DEditMode;
			pView->UpdateView( MAPVIEW_UPDATE_TOOL );
			return true;
		}
#endif

		case VK_DELETE:
		{
			m_pDocument->OnCmdMsg(ID_EDIT_DELETE, CN_COMMAND, NULL, NULL);
			return true;
		}

		case VK_ESCAPE:
		{
			OnEscape(m_pDocument);
			return true;
		}
	}

	if (Options.view2d.bNudge && (nChar == VK_UP || nChar == VK_DOWN || nChar == VK_LEFT || nChar == VK_RIGHT))
	{
		if (!IsEmpty())
		{
			bool bSnap = m_pDocument->IsSnapEnabled() && !bCtrl;
			NudgeObjects(pView, nChar, bSnap, bShift);
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles double click events in the 3D view.
// Input  : Per CWnd::OnLButtonDblClk.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnLMouseDblClk3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	if ( !m_pSelection->IsEmpty() )
	{
		if ( m_pSelection->GetCount() == 1 )
		{
			CMapClass			*pObject = m_pSelection->GetList()->Element( 0 );
			CManifestInstance	*pManifestInstance = dynamic_cast< CManifestInstance * >( pObject );
			if ( pManifestInstance )
			{
				CManifest *pManifest = CMapDoc::GetManifest();

				if ( pManifest )
				{
					pManifest->SetPrimaryMap( pManifestInstance->GetManifestMap() );
					return true;
				}
			}
		}

		GetMainWnd()->pObjectProperties->ShowWindow(SW_SHOW);
	}

	return true;
}

bool Selection3D::OnLMouseDblClkLogical(CMapViewLogical *pView, UINT nFlags, const Vector2D &vPoint)
{
	if ( !m_pSelection->IsEmpty() )
	{
		GetMainWnd()->pObjectProperties->ShowWindow(SW_SHOW);
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - 
//-----------------------------------------------------------------------------
void Selection3D::EyedropperPick2D(CMapView2D *pView, const Vector2D &vPoint)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//			point - 
//-----------------------------------------------------------------------------
void Selection3D::EyedropperPick3D(CMapView3D *pView, const Vector2D &vPoint)
{
	//
	// We only want to do this if we have at least one entity selected.
	//
	if ( !m_pSelection->IsAnEntitySelected() )
	{
		MessageBox( NULL, "No entities are selected, so the eyedropper has nothing to assign to.", "No selected entities", MB_OK);
		return;
	}

	//
	// If they clicked on an entity, get the name of the entity they clicked on.
	//
	ULONG ulFace;
	CMapClass *pClickObject = pView->NearestObjectAt( vPoint, ulFace);
	if (pClickObject != NULL)
	{
		EyedropperPick(pView, pClickObject);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
//-----------------------------------------------------------------------------
void Selection3D::EyedropperPick(CMapView *pView, CMapClass *pObject)
{
	//
	// The eyedropper currently only supports clicking on entities.
	// TODO: consider using this to fill out face lists if they click on a solid
	//
	CMapEntity *pEntity = FindEntityInTree(pObject);
	if (pEntity == NULL)
	{
		// They clicked on something that is not an entity.
		return;
	}

	//
	// Get the name of the clicked on entity.
	//
	const char *pszClickName = NULL;
	pszClickName = pEntity->GetKeyValue("targetname");
	if (pszClickName == NULL)
	{
		//
		// They clicked on an entity with no name.
		//
		MessageBox( NULL, "The chosen entity has no name.", "No name to pick", MB_OK );
		return;
	}

	//
	// Build a list of all the keyvalues in the selected entities that support the eyedropper.
	//
	CUtlVector<GDinputvariable *> VarList;

	int nEntityCount = 0;

	const CMapObjectList *pSelList = m_pSelection->GetList();

	for (int i = 0; i < pSelList->Count(); i++)
	{
		pObject = pSelList->Element(i);
		pEntity = dynamic_cast <CMapEntity *> (pObject);
		if (pEntity != NULL)
		{
			nEntityCount++;
			GDclass *pClass = pEntity->GetClass();

			int nVarCount = pClass->GetVariableCount();
			for (int nVar = 0; nVar < nVarCount; nVar++)
			{
				GDinputvariable *pVar = pClass->GetVariableAt(nVar);
				if (pVar && ((pVar->GetType() == ivTargetDest) || (pVar->GetType() == ivTargetNameOrClass)))
				{
					VarList.AddToTail(pVar);
				}
			}
		}
	}

	//
	// Prompt for what keyvalue in the selected entities we are filling out.
	//
	int nCount = VarList.Count();
	if (nCount <= 0)
	{
		//
		// No selected entities have keys of the appropriate type, so there's nothing we can do.
		//
		MessageBox( NULL, "No selected entities have keyvalues that accept an entity name, so the eyedropper has nothing to assign to.", "No eligible keyvalues", MB_OK );
		return;
	}

	//
	// Determine the name of the key that we are filling out.
	//
	GDinputvariable *pVar = ChooseEyedropperVar(pView, VarList);
	if (!pVar)
	{
		return;
	}
	const char *pszVarName = pVar->GetName();
	if (!pszVarName)
	{
		return;
	}

	GetHistory()->MarkUndoPosition( pSelList, "Set Keyvalue");

	//
	// Apply the key to all selected entities with the chosen keyvalue.
	//
	for (int i = 0; i < pSelList->Count(); i++)
	{
		pObject = pSelList->Element(i);
	
		pEntity = dynamic_cast <CMapEntity *> (pObject);
		if (pEntity != NULL)
		{
			GDclass *pClass = pEntity->GetClass();
			pVar = pClass->VarForName(pszVarName);
			if (pVar && ((pVar->GetType() == ivTargetDest) || (pVar->GetType() == ivTargetNameOrClass)))
			{
				GetHistory()->Keep(pEntity);
				pEntity->SetKeyValue(pszVarName, pszClickName);
			}
		}
	}

	CMapDoc *pDoc = pView->GetMapDoc();
	if (pDoc != NULL)
	{
		pDoc->SetModifiedFlag();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns the nearest CMapEntity object up the hierarchy from the
//			given object.
// Input  : pObject - Object to start from.
//-----------------------------------------------------------------------------
CMapEntity *Selection3D::FindEntityInTree(CMapClass *pObject)
{
	do
	{
		CMapEntity *pEntity = dynamic_cast <CMapEntity *> (pObject);
		if (pEntity != NULL)
		{
			return pEntity;
		}

		pObject = pObject->GetParent();

	} while (pObject != NULL);

	// No entity in this branch of the object tree.
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left button down events in the 3D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	Tool3D::OnLMouseDown3D(pView, nFlags, vPoint);

	m_bSelected = false;

	//
	// If they are holding down the eyedropper hotkey, do an eyedropper pick. The eyedropper fills out
	// keyvalues in selected entities based on the object they clicked on.
	//
	if (m_bEyedropper)
	{
		EyedropperPick3D(pView, vPoint);
		m_bEyedropper = false;
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
		return true;
	}

	if (nFlags & MK_CONTROL)
	{
		m_bSelected = pView->SelectAt(vPoint, false, false);;
		UpdateHandleState();
	}
	else if ( m_b3DEditMode && HitTest(pView,vPoint, true) )
	{
		// if clicked on handles, never change selection
		if ( !IsBoxSelecting() && m_LastHitTestHandle == vec3_origin )
		{
			//  clicked somewhere on our selection tool but maybe something else is inbetween
			
			HitInfo_t HitData;

			m_bDrawAsSolidBox = true;

			pView->ObjectsAt( vPoint, &HitData, 1 );

			if ( HitData.pObject && !HitData.pObject->IsSelected() )
			{
				m_bSelected = pView->SelectAt(vPoint, true, false);
				UpdateHandleState();
			}

			m_bDrawAsSolidBox = false;

			pView->SetCursor( UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode ) );
		}
	}
	else
	{
		m_TranslateMode = modeScale;
		m_bSelected = pView->SelectAt(vPoint, true, false);
		UpdateHandleState();
	}

	if ( m_bSelected && !m_b3DEditMode )
	{
		pView->BeginPick();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left button up events in the 3D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	bool bShift = nFlags & MK_SHIFT;

	Tool3D::OnLMouseUp3D(pView, nFlags, vPoint) ;

	bool	IsEditable = m_pSelection->IsEditable();

	if ( IsTranslating() )
	{
		// selecting stuff in box
		if ( IsBoxSelecting() )
		{
			Box3D::FinishTranslation(true);

			if (Options.view2d.bAutoSelect)
			{
				SelectInBox(m_pDocument, bShift);
				UpdateHandleState();
			}
		}
		else
		{
			FinishTranslation( true, bShift );
		}
	}
	else if ( m_b3DEditMode && !m_bSelected && !m_pSelection->IsEmpty() )
	{
		if ( IsEditable && HitTest(pView, vPoint, false) )
		{
			ToggleTranslateMode();

			UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );

			m_pDocument->UpdateAllViews( MAPVIEW_UPDATE_TOOL );
		}
	}

	pView->EndPick();

	// we might have removed some stuff that was relevant:
	m_pDocument->UpdateStatusbar();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 3D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Selection3D::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnMouseMove3D(pView, nFlags, vPoint);

	bool	IsEditable = m_pSelection->IsEditable();

	vgui::HCursor hCursor = vgui::dc_arrow;

	if ( m_bEyedropper )
	{
		SetEyedropperCursor();
	}
	//
	// If we are currently dragging the selection (moving, scaling, rotating, or shearing)
	// update that operation based on the current cursor position and keyboard state.
	//
	
	else if ( IsTranslating() )
	{
		unsigned int uConstraints = GetConstraints(nFlags);

		//
		// If they are dragging with a valid handle, update the views.
		//

		Tool3D::UpdateTranslation( pView, vPoint, uConstraints );

		hCursor = vgui::dc_none;
	}
	//
	// Else if we have just started dragging the selection, begin a new translation
	//
	else if ( m_b3DEditMode && m_bMouseDragged[MOUSE_LEFT] )
	{
		if ( IsEditable && HitTest( pView, m_vMouseStart[MOUSE_LEFT], true) )
		{
			// we selected a handle - start translation the selection
			StartTranslation( pView, vPoint, m_LastHitTestHandle ); 
			
			hCursor = UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );
		}
	}
	else if ( IsEditable && m_b3DEditMode && !IsEmpty() )
	{
		UpdateHandleState();

		if ( HitTest(pView, vPoint, true) )
		{
			hCursor = UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );
		}
	}

	if ( hCursor != vgui::dc_none )
	{
		pView->SetCursor( hCursor );
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the cursor to the eyedropper cursor.
//-----------------------------------------------------------------------------
void Selection3D::SetEyedropperCursor(void)
{
	static HCURSOR hcurEyedropper = NULL;
	
	if (!hcurEyedropper)
	{
		hcurEyedropper = LoadCursor(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_EYEDROPPER));
	}
	
	SetCursor(hcurEyedropper);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void Selection3D::OnEscape(CMapDoc *pDoc)
{
	//
	// If we're in eyedropper mode, go back to selection mode.
	//
	if (m_bEyedropper)
	{
		m_bEyedropper = false;
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
	}
	//
	// If we're box selecting, clear the box.
	//
	else if (IsBoxSelecting())
	{
		EndBoxSelection();
		UpdateSelectionBounds();
	}
	//
	// If we're logical box selecting, clear the box.
	//
	else if ( m_bInLogicalBoxSelection )
	{
		EndLogicalBoxSelection();
	}
	//
	// If we're moving a brush, put it back.
	//
	else if (IsTranslating())
	{
		FinishTranslation(false,false);
	}
	//
	// If we have a selection, deselect it.
	//
	else if (!IsEmpty())
	{
		SetEmpty();
	}
}

