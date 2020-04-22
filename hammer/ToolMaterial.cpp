//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles the selection of faces and material application.
//
//			TODO: consider making face selection a mode of the selection tool
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "FaceEditSheet.h"
#include "History.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "MapSolid.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "StatusBarIDs.h"
#include "ToolManager.h"
#include "ToolMaterial.h"
#include "Options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: Called when this tool is deactivated in favor of another tool.
// Input  : eNewTool - The tool that is being activated.
//-----------------------------------------------------------------------------
void CToolMaterial::OnDeactivate()
{
	if ( m_pDocument->GetTools()->GetActiveToolID() != TOOL_FACEEDIT_DISP )
	{
		// Clear the selected faces when we are deactivated.
		m_pDocument->SelectFace(NULL, 0, scClear );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			nFlags - 
//			point - 
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolMaterial::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	if (nFlags & MK_CONTROL)
	{
		//
		// CONTROL is down, perform selection only.
		//
		pView->SelectAt( vPoint, false, true);
	}
	else
	{
		pView->SelectAt( vPoint, true, true);
	}

	return (true);
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 3D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolMaterial::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	CMapDoc *pDoc = pView->GetMapDoc();
	if (pDoc == NULL)
	{
		return false;
	}

	bool bShift = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);

	ULONG ulFace;
	CMapClass *pObject = pView->NearestObjectAt( vPoint, ulFace);

	if ((pObject != NULL) && (pObject->IsMapClass(MAPCLASS_TYPE(CMapSolid))))
	{
		CMapSolid *pSolid = (CMapSolid *)pObject;

		int cmd = scToggle | scClear;

		// No clear if CTRL pressed.
		if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
		{
			cmd &= ~scClear;	
		}

		// If they are holding down SHIFT, select the entire solid.
		if (bShift)
		{
			pDoc->SelectFace(pSolid, -1, cmd);
		}
		// Otherwise, select a single face.
		else
		{
			pDoc->SelectFace(pSolid, ulFace, cmd);
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles right mouse button down events in the 3D view.
// Input  : Per CWnd::OnRButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolMaterial::OnRMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	BOOL bShift = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
	BOOL bEdgeAlign = ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
	
	ULONG ulFace;
	CMapClass *pObject = pView->NearestObjectAt( vPoint, ulFace);

	if (pObject != NULL)
	{
		if (pObject->IsMapClass(MAPCLASS_TYPE(CMapSolid)))
		{
			CMapSolid *pSolid = (CMapSolid *)pObject;
			GetHistory()->MarkUndoPosition(NULL, "Apply texture");
			GetHistory()->Keep(pSolid);
			
			// Setup the flags.
			int cmdFlags = 0;
			if (bEdgeAlign)
			{
				cmdFlags |= CFaceEditSheet::cfEdgeAlign;
			}

			// If we're in a lightmap grid preview window, only apply the lightmap scale.
			int eMode;
			if (pView->GetDrawType() == VIEW3D_LIGHTMAP_GRID)
			{
				eMode = CFaceEditSheet::ModeApplyLightmapScale;
			}
			else
			{
				eMode = CFaceEditSheet::ModeApplyAll;
			}
			
			// If they are holding down the shift key, apply to the entire solid.
			if (bShift)
			{
				int nFaces = pSolid->GetFaceCount();
				for(int i = 0; i < nFaces; i++)
				{
					GetMainWnd()->m_pFaceEditSheet->ClickFace(pSolid, i, cmdFlags, eMode);
				}
			}
			// If not, apply to a single face.
			else
			{
				GetMainWnd()->m_pFaceEditSheet->ClickFace(pSolid, ulFace, cmdFlags, eMode);
			}
		}				
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the mouse move message in the 3D view.
// Input  : Per CWnd::OnMouseMove.
//-----------------------------------------------------------------------------
bool CToolMaterial::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	//
	// Manage the cursor.
	//
	static HCURSOR hcurFacePaint = 0;
	if (!hcurFacePaint)
	{
		hcurFacePaint = LoadCursor(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_FACEPAINT));
	}

	SetCursor(hcurFacePaint);

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CToolMaterial::UpdateStatusBar()
{
	CString str;
	str.Format("%d faces selected", GetMainWnd()->m_pFaceEditSheet->GetFaceListCount() );
	SetStatusText(SBI_SELECTION, str);
	SetStatusText(SBI_SIZE, "");
}

//-----------------------------------------------------------------------------
// Purpose: Handles the key down event in the 3D view.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool CToolMaterial::OnKeyDown3D( CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags )
{
	// TO DO:  undo?
	//		   justify (SHIFT?)
	
	CMapDoc *pDoc = pView->GetMapDoc();
	if ( pDoc == NULL )
	{
		return false;
	}

	if ( nChar == VK_UP || nChar == VK_DOWN || nChar == VK_LEFT || nChar == VK_RIGHT )
	{
		// Bail out if the user doesn't have Nudging enabled.
		if ( !Options.view2d.bNudge )
		{
			return false;
		}

		CFaceEditSheet *pSheet = GetMainWnd()->m_pFaceEditSheet;
		if( pSheet )
		{
			// Check for a face list.
			int nFaceCount = pSheet->GetFaceListCount();
			if( nFaceCount == 0 )
			{
				return false;
			}

			int nGridSize = m_pDocument->GetGridSpacing();
			bool bCtrlDown = ( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 );

			if ( bCtrlDown )
			{
				nGridSize = 1;
			}

			for( int ndxface = 0; ndxface < nFaceCount; ndxface++ )
			{
				CMapFace *pFace;
				pFace = pSheet->GetFaceListDataFace( ndxface );

				TEXTURE &t = pFace->texture;

				if ( nChar == VK_UP )
				{
					t.VAxis[ 3 ] = ( (int)( t.VAxis[ 3 ] + nGridSize ) % 1024 );  
				}
				else if ( nChar == VK_DOWN )
				{
					t.VAxis[ 3 ] = ( (int)( t.VAxis[ 3 ] - nGridSize ) % 1024 );

				}
				else if ( nChar == VK_LEFT )
				{
					t.UAxis[ 3 ] = ( (int)( t.UAxis[ 3 ] + nGridSize ) % 1024 );
				}
				else
				{
					t.UAxis[ 3 ] = ( (int)( t.UAxis[ 3 ] - nGridSize ) % 1024 );
				}
				
				pFace->CalcTextureCoords();
				pSheet->m_MaterialPage.UpdateDialogData( pFace );
			}
			pDoc->SetModifiedFlag();
			return true;
		}
	}
	return false;
}

