//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Rendering and mouse handling in the 2D view.
//
//===========================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "MapDoc.h"
#include "Render2D.h"
#include "ToolManager.h"
#include "History.h"
#include "TitleWnd.h"
#include "mainfrm.h"
#include "MapSolid.h"
#include "ToolMorph.h"		// FIXME: remove
#include "MapWorld.h"
#include "camera.h"
#include "Manifest.h"
#include "MapInstance.h"
#include "Options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


extern bool g_bUpdateBones2D;

static DrawType_t __eNextViewType = VIEW2D_XY;


IMPLEMENT_DYNCREATE(CMapView2D, CMapView2DBase)


BEGIN_MESSAGE_MAP(CMapView2D, CMapView2DBase)
	//{{AFX_MSG_MAP(CMapView2D)
	ON_WM_KEYDOWN()
	ON_COMMAND(ID_VIEW_2DXY, OnView2dxy)
	ON_COMMAND(ID_VIEW_2DYZ, OnView2dyz)
	ON_COMMAND(ID_VIEW_2DXZ, OnView2dxz)
	ON_COMMAND_EX(ID_TOOLS_ALIGNTOP, OnToolsAlign)
	ON_COMMAND_EX(ID_TOOLS_ALIGNBOTTOM, OnToolsAlign)
	ON_COMMAND_EX(ID_TOOLS_ALIGNLEFT, OnToolsAlign)
	ON_COMMAND_EX(ID_TOOLS_ALIGNRIGHT, OnToolsAlign)
	ON_COMMAND_EX(ID_FLIP_HORIZONTAL, OnFlip)
	ON_COMMAND_EX(ID_FLIP_VERTICAL, OnFlip)
	ON_UPDATE_COMMAND_UI(ID_FLIP_HORIZONTAL, OnUpdateEditSelection)
	ON_UPDATE_COMMAND_UI(ID_FLIP_VERTICAL, OnUpdateEditSelection)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_ALIGNTOP, OnUpdateEditSelection)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_ALIGNBOTTOM, OnUpdateEditSelection)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_ALIGNLEFT, OnUpdateEditSelection)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_ALIGNRIGHT, OnUpdateEditSelection)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Allows for iteration of draw types in order.
// Input  : eDrawType - Current draw type.
// Output : Returns the next draw type in the list: XY, YZ, XZ. List wraps.
//-----------------------------------------------------------------------------
static DrawType_t NextDrawType(DrawType_t eDrawType)
{
	if (eDrawType == VIEW2D_XY)
	{
		return(VIEW2D_YZ);
	}

	if (eDrawType == VIEW2D_YZ)
	{
		return(VIEW2D_XZ);
	}

	return(VIEW2D_XY);
}


//-----------------------------------------------------------------------------
// Purpose: Allows for iteration of draw types in reverse order.
// Input  : eDrawType - Current draw type.
// Output : Returns the previous draw type in the list: XY, YZ, XZ. List wraps.
//-----------------------------------------------------------------------------
static DrawType_t PrevDrawType(DrawType_t eDrawType)
{
	if (eDrawType == VIEW2D_XY)
	{
		return(VIEW2D_XZ);
	}

	if (eDrawType == VIEW2D_YZ)
	{
		return(VIEW2D_XY);
	}

	return(VIEW2D_YZ);
}


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//	---------------------------------------------------------------------------
CMapView2D::CMapView2D(void)
{
	//
	// Create next 2d view type.
	//
	__eNextViewType = NextDrawType(__eNextViewType);
	SetDrawType(__eNextViewType);

	m_bUpdateRenderObjects = true;
	m_bLastActiveView = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees dynamically allocated resources.
//-----------------------------------------------------------------------------
CMapView2D::~CMapView2D(void)
{
	if ( GetMapDoc() )
	{
		GetMapDoc()->RemoveMRU(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: First-time initialization of this view.
//-----------------------------------------------------------------------------
void CMapView2D::OnInitialUpdate(void)
{
	// NOTE: This must occur becore OnInitialUpdate
	// Creates the title window
	CreateTitleWindow();

	// NOTE: This must occur becore OnInitialUpdate
	// Other initialization.

	

	SetDrawType( GetDrawType() );

	CMapView2DBase::OnInitialUpdate();

	// Add to doc's MRU list
	CMapDoc *pDoc = GetMapDoc();
	pDoc->SetMRU(this);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRender  - 
//-----------------------------------------------------------------------------
void CMapView2D::DrawPointFile( CRender2D *pRender )
{
	pRender->SetDrawColor( 255,0,0 );

	int nPFPoints = GetMapDoc()->m_PFPoints.Count();
	Vector* pPFPoints = GetMapDoc()->m_PFPoints.Base();

	pRender->MoveTo( pPFPoints[0] );

	for(int i = 1; i < nPFPoints; i++)
	{
		pRender->DrawLineTo( pPFPoints[i] );
	}
}


//-----------------------------------------------------------------------------
// Called when the base class wants the render lists to be recomputed
//-----------------------------------------------------------------------------
void CMapView2D::OnRenderListDirty()
{
	m_bUpdateRenderObjects = true;
}

	
//-----------------------------------------------------------------------------
// Purpose: Sorts the object to be rendered into one of two groups: normal objects
//			and selected objects, so that selected objects can be rendered last.
// Input  : pObject - 
//			pRenderList - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
void CMapView2D::AddToRenderLists(CMapClass *pObject)
{
	if ( !pObject->IsVisible() )
		return;
	
	// Don't render groups, render their children instead.
	if ( !pObject->IsGroup() )
	{
		if ( !pObject->IsVisible2D() )
			return;

		Vector vecMins, vecMaxs;
		pObject->GetCullBox( vecMins, vecMaxs );
		
		if ( IsValidBox( vecMins, vecMaxs ) )
		{
			// Make sure the object is in the update region.
			if ( !IsInClientView(vecMins, vecMaxs) )
				return; 
		}
		       	
		m_RenderList.AddToTail(pObject);
	}

	// Recurse into children and add them.
	const CMapObjectList *pChildren = pObject->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		AddToRenderLists(pChildren->Element(pos));
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : rectUpdate - 
//-----------------------------------------------------------------------------
void CMapView2D::Render()
{

	CMapDoc		*pDoc = GetMapDoc();
	CMapWorld	*pWorld = pDoc->GetMapWorld();
	CManifest	*pManifest = pDoc->GetManifest();

	if ( pManifest )
	{
		pWorld = pManifest->GetManifestWorld();
	}

	GetRender()->StartRenderFrame();
	
	if ( Options.general.bRadiusCulling )
	{
		DrawCullingCircleHelper2D( GetRender() );
	}

	// Draw grid if enabled.
	if (pDoc->m_bShowGrid)
	{
		DrawGrid( GetRender(), axHorz, axVert, 0 );
	}

	//
	// Draw the world if we have one.
	//
	if (pWorld == NULL)
		return;
	
	// Traverse the entire world, sorting visible elements into two arrays:
	// Normal objects and selected objects, so that we can render the selected
	// objects last.
	//

	if ( m_bUpdateRenderObjects )
	{
		m_RenderList.RemoveAll();
		
		// fill render lists with visible objects
		AddToRenderLists( pWorld );

		g_bUpdateBones2D = true;
	}

	//
	// Render normal (nonselected) objects first
	//

	GetRender()->PrepareInstanceStencil();

	CUtlVector<CMapClass *> selectedObjects;
	CUtlVector<CMapClass *> helperObjects;

	for (int i = 0; i < m_RenderList.Count(); i++)
	{
		CMapClass *pObject = m_RenderList[i];

		if ( pObject->IsSelected() )
		{
			// render later
			if ( pObject->GetToolObject(0,false) )
			{
				helperObjects.AddToTail( pObject );
			}
			else
			{
				selectedObjects.AddToTail( pObject );
			}
		}
		else
		{
			// render now
			pObject->Render2D( GetRender() );
		}
	}

	//
	// Render selected objects in second batch, so they overdraw normal object
	//
	for (int i = 0; i < selectedObjects.Count(); i++)
	{
		selectedObjects[i]->Render2D( GetRender() );
	}

	GetRender()->DrawInstanceStencil();

	//
	// Draw pointfile if enabled.
	//
	if (pDoc->m_PFPoints.Count())
	{
		DrawPointFile( GetRender() );
	}

	pDoc->RenderDocument( GetRender() );

	m_bUpdateRenderObjects = false;
	g_bUpdateBones2D = false;

	// render all tools
	CBaseTool *pCurTool = m_pToolManager->GetActiveTool();

	// render active tool
	if ( pCurTool )
	{
		pCurTool->RenderTool2D( GetRender() );
	}

	// render map helpers at last
	for (int i = 0; i < helperObjects.Count(); i++)
	{
		helperObjects[i]->Render2D( GetRender() );
	}

	GetRender()->EndRenderFrame();
}


//-----------------------------------------------------------------------------
// Purpose: this function will render an instance map at the specific offset and rotation
// Input  : pInstanceClass - the map class of the func_instance
//			pMapClass - the map class of the world spawn of the instance
//			InstanceOrigin - the translation offset
//			InstanceAngles - the axis rotation
// Output : none
//-----------------------------------------------------------------------------
void CMapView2D::RenderInstance( CMapInstance *pInstanceClass, CMapClass *pMapClass, Vector &InstanceOrigin, QAngle &InstanceAngles )
{
	if ( !pInstanceClass->IsInstanceVisible() )
	{
		return;
	}

	GetRender()->PushInstanceData( pInstanceClass, InstanceOrigin, InstanceAngles );

	RenderInstanceMapClass_r( pMapClass );

	GetRender()->PopInstanceData();
}


//-----------------------------------------------------------------------------
// Purpose: this function will recursively render an instance and all of its children
// Input  : pObject - the object to be rendered
// Output : none
//-----------------------------------------------------------------------------
void CMapView2D::RenderInstanceMapClass_r( CMapClass *pObject )
{
	if ( !pObject->IsVisible() )
	{
		return;
	}

	// Don't render groups, render their children instead.
	if ( !pObject->IsGroup() )
	{
		if ( !pObject->IsVisible2D() )
		{
			return;
		}

		Vector vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs;
		pObject->GetCullBox( vecMins, vecMaxs );
		GetRender()->TransformInstanceAABB( vecMins, vecMaxs, vecExpandedMins, vecExpandedMaxs );

		if ( IsValidBox( vecExpandedMins, vecExpandedMaxs ) )
		{
			// Make sure the object is in the update region.
			if ( !IsInClientView( vecExpandedMins, vecExpandedMaxs ) )
			{
				return; 
			}
		}

		pObject->Render2D( GetRender() );
	}

	// Recurse into children and add them.
	const CMapObjectList *pChildren = pObject->GetChildren();
	FOR_EACH_OBJ( *pChildren, pos )
	{
		RenderInstanceMapClass_r(pChildren->Element(pos));
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : m_DrawType - 
//			bForceUpdate - 
//-----------------------------------------------------------------------------
void CMapView2D::SetDrawType(DrawType_t drawType)
{
	Vector vOldView; 
		
	// reset old third axis to selection center level
	m_pCamera->GetViewPoint( vOldView );

	CMapDoc *pDoc = GetMapDoc();

	if ( pDoc && !pDoc->GetSelection()->IsEmpty() )
	{
		Vector vCenter;
		GetMapDoc()->GetSelection()->GetBoundsCenter( vCenter );
		vOldView[axThird] = vCenter[axThird];
	}
	else
	{
		vOldView[axThird] = 0;
	}

	switch (drawType)
	{
	case VIEW2D_XY:
		SetAxes(AXIS_X, FALSE, AXIS_Y, TRUE);
		if ( HasTitleWnd() )
		{
			GetTitleWnd()->SetTitle("top (x/y)");
		}
		break;
	
	case VIEW2D_YZ:
		SetAxes(AXIS_Y, FALSE, AXIS_Z, TRUE);
		if ( HasTitleWnd() )
		{
			GetTitleWnd()->SetTitle("front (y/z)");
		}
		break;

	case VIEW2D_XZ:
		SetAxes(AXIS_X, FALSE, AXIS_Z, TRUE);
		if ( HasTitleWnd() )
		{
			GetTitleWnd()->SetTitle("side (x/z)");
		}
		break;
	}

	

	m_eDrawType = drawType;

	m_pCamera->SetViewPoint( vOldView );
	
	UpdateClientView();

	if (m_bLastActiveView && GetMapDoc())
	{
		GetMapDoc()->UpdateTitle(this);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView2D::OnView2dxy(void)
{
	SetDrawType(VIEW2D_XY);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView2D::OnView2dyz(void)
{
	SetDrawType(VIEW2D_YZ);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapView2D::OnView2dxz(void)
{
	SetDrawType(VIEW2D_XZ);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bActivate - 
//			pActivateView - 
//			pDeactiveView - 
//-----------------------------------------------------------------------------
void CMapView2D::ActivateView(bool bActivate) 
{
	CMapView2DBase::ActivateView( bActivate );

	if ( bActivate )
	{
		CMapDoc *pDoc = GetMapDoc();
		pDoc->SetMRU(this);

		// tell doc to update title
		m_bLastActiveView = true;
	}
	else
	{
		m_bLastActiveView = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nID - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapView2D::OnToolsAlign(UINT nID) 
{
	CMapDoc *pDoc = GetMapDoc();
	CSelection *pSelection = pDoc->GetSelection();
	const CMapObjectList *pSelList = pSelection->GetList();

	GetHistory()->MarkUndoPosition(pSelList, "Align");
	GetHistory()->Keep(pSelList);

	// convert nID into the appropriate ID_TOOLS_ALIGNxxx define
	// taking into consideration the orientation of the axes
	if(nID == ID_TOOLS_ALIGNTOP && bInvertVert)
		nID = ID_TOOLS_ALIGNBOTTOM;
	else if(nID == ID_TOOLS_ALIGNBOTTOM && bInvertVert)
		nID = ID_TOOLS_ALIGNTOP;
	else if(nID == ID_TOOLS_ALIGNLEFT && bInvertHorz)
		nID = ID_TOOLS_ALIGNRIGHT;
	else if(nID == ID_TOOLS_ALIGNRIGHT && bInvertHorz)
		nID = ID_TOOLS_ALIGNLEFT;

	// use boundbox of selection - move all objects to match extreme 
	// side of all the objects
	BoundBox box;
	pSelection->GetBounds(box.bmins, box.bmaxs);

	Vector ptMove( 0, 0, 0 );

	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);

		Vector vecMins;
		Vector vecMaxs;
		pObject->GetRender2DBox(vecMins, vecMaxs);

		// align top
		if (nID == ID_TOOLS_ALIGNTOP)
		{
			ptMove[axVert] = box.bmins[axVert] - vecMins[axVert];
		}
		else if (nID == ID_TOOLS_ALIGNBOTTOM)
		{
			ptMove[axVert] = box.bmaxs[axVert] - vecMaxs[axVert];
		}
		else if (nID == ID_TOOLS_ALIGNLEFT)
		{
			ptMove[axHorz] = box.bmins[axHorz] - vecMins[axHorz];
		}
		else if (nID == ID_TOOLS_ALIGNRIGHT)
		{
			ptMove[axHorz] = box.bmaxs[axHorz] - vecMaxs[axHorz];
		}
		pObject->TransMove(ptMove);
	}

	pDoc->SetModifiedFlag();

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Flips the selection horizontally or vertically (with respect to the
//			view orientation.
// Input  : nID - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CMapView2D::OnFlip(UINT nID) 
{
	CMapDoc *pDoc = GetMapDoc();
	CSelection *pSelection = pDoc->GetSelection();
	const CMapObjectList *pSelList = pSelection->GetList();

	if ( pSelection->IsEmpty() )
	{
		return TRUE;	// no selection
	}

	// flip objects from center of selection
	Vector ptRef, vScale(1,1,1);
	pSelection->GetBoundsCenter(ptRef);

	// never about this axis:
	if (nID == ID_FLIP_HORIZONTAL)
	{
		vScale[axHorz] = -1;
	}
	else if (nID == ID_FLIP_VERTICAL)
	{
		vScale[axVert] = -1;
	}

	GetHistory()->MarkUndoPosition( pSelList, "Flip Objects");
	GetHistory()->Keep(pSelList);
		
	// do flip
	for (int i = 0; i < pSelList->Count(); i++)
	{
		CMapClass *pObject = pSelList->Element(i);
		pObject->TransScale(ptRef,vScale);
	}

	pDoc->SetModifiedFlag();

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Manages the state of the Copy menu item.
//-----------------------------------------------------------------------------
void CMapView2D::OnUpdateEditSelection(CCmdUI *pCmdUI) 
{
	pCmdUI->Enable((!GetMapDoc()->GetSelection()->IsEmpty()) &&
					(m_pToolManager->GetActiveToolID() != TOOL_FACEEDIT_MATERIAL) &&
					!GetMainWnd()->IsShellSessionActive());
}

void CMapView2D::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (nChar == VK_TAB)
	{
		// swicth to next draw type
		SetDrawType( NextDrawType( m_eDrawType ) );
		return;
	}

	CMapView2DBase::OnKeyDown( nChar, nRepCnt, nFlags );
}

void CMapView2D::DrawCullingCircleHelper2D( CRender2D *pRender )
{
	CMapDoc *pDoc = GetMapDoc();

	POSITION viewpos = pDoc->GetFirstViewPosition();

	while ( viewpos )
	{
		CMapView3D *pView = dynamic_cast<CMapView3D*>( pDoc->GetNextView( viewpos ) );
		if ( pView )
		{
			CCamera *pCamera = pView->GetCamera();

			Vector cameraPos;
			pCamera->GetViewPoint( cameraPos );
			int iClipDist = (int)pCamera->GetFarClip();

			pRender->SetDrawColor( 255, 0, 0 );
			pRender->DrawCircle( cameraPos, iClipDist );
		}
	}
}


