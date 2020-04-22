//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A page in a tabbed dialog that allows the editing of face properties.
//
//=============================================================================//

#include <stdafx.h>
#include "hammer.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "FaceEditSheet.h"
#include "MapSolid.h"
#include "MapFace.h"
#include "MapDisp.h"
#include "ToolManager.h"
#include "mapdoc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_DYNAMIC( CFaceEditSheet, CPropertySheet )

BEGIN_MESSAGE_MAP( CFaceEditSheet, CPropertySheet )
	//{{AFX_MSG_MAP( CFaceEdtiSheet )
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CFaceEditSheet::CFaceEditSheet( LPCTSTR pszCaption, CWnd *pParentWnd, UINT iSelectPage ) : 
                CPropertySheet( pszCaption, pParentWnd, iSelectPage )
{
	m_ClickMode = -1;
	m_bEnableUpdate = true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CFaceEditSheet::~CFaceEditSheet()
{
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditSheet::SetupPages( void )
{
	//
	// add pages to sheet
	//
	AddPage( &m_MaterialPage );
	AddPage( &m_DispPage );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditSheet::Setup( void )
{
	SetupPages();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CFaceEditSheet::Create( CWnd *pParentWnd )
{
	if( !CPropertySheet::Create( pParentWnd ) )
		return FALSE;

	//
	// touch all pages so they create the hWnd for each
	//
	SetActivePage( &m_DispPage );
	SetActivePage( &m_MaterialPage );

	//
	// initialize the pages
	//
	m_MaterialPage.Init();
//	m_DispPage.Init();

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Disables window updates when adding a large number of faces to the
//			dialog at once. When updates are re-enabled, the window is updated.
// Input  : bEnable - true to enable updates, false to disable them.
//-----------------------------------------------------------------------------
void CFaceEditSheet::EnableUpdate( bool bEnable )
{
	bool bOld = m_bEnableUpdate;
	m_bEnableUpdate = bEnable;

	if( ( bEnable ) && ( !bOld ) )
	{
		m_MaterialPage.UpdateDialogData();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when a new material is detected.
//-----------------------------------------------------------------------------
void CFaceEditSheet::NotifyNewMaterial( IEditorTexture *pTex )
{
	m_MaterialPage.NotifyNewMaterial( pTex );
}


//-----------------------------------------------------------------------------
// Purpose: Clear the face list.
//-----------------------------------------------------------------------------
void CFaceEditSheet::ClearFaceList( void )
{
	//
	// reset selection state of all faces currently in the list
	//
	for( int i = 0; i < m_Faces.Count(); i++ )
	{
		m_Faces[i].pMapFace->SetSelectionState( SELECT_NONE );
		EditDispHandle_t handle = m_Faces[i].pMapFace->GetDisp();
		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
		if( pDisp )
		{
			pDisp->ResetTexelHitIndex();
		}
	}

	//
	// reset the list
	//
	m_Faces.Purge();

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( pDoc )
		pDoc->UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
}


void CFaceEditSheet::ClearFaceListByMapDoc( CMapDoc *pDoc )
{
	// Remove any faces from our list that came from this CMapDoc.
	for( int i = 0; i < m_Faces.Count(); i++ )
	{
		if ( m_Faces[i].pMapDoc == pDoc )
		{
			m_Faces.Remove( i );
			--i;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Search for the given face in the face selection list.  If found, 
//          return the index of the face in the list.  Otherwise, return -1.  
//-----------------------------------------------------------------------------
int CFaceEditSheet::FindFaceInList( CMapFace *pFace )
{
	for( int i = 0; i < m_Faces.Count(); i++ )
	{
		if( m_Faces[i].pMapFace == pFace )
			return i;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// It is really lame that I have to have material/displacement specific code
// here!  It is unfortunately necessary - see CMapDoc (SelectFace).  ClickFace
// not only specifies face selection (which it should do only!!!), but also
// specifies the "mode" (material specific) of the click -- LAME!!!!  This will
// be a problem spot if/when we make face selection general.  cab
//-----------------------------------------------------------------------------
void CFaceEditSheet::ClickFace( CMapSolid *pSolid, int faceIndex, int cmd, int clickMode )
{
	//
	// set the click mode (either to new mode or previously used)
	//
	if( clickMode == -1 )
	{
		clickMode = m_ClickMode;
	}

	//
	// clear the face list?
	//
	if( cmd & cfClear )
	{
		ClearFaceList();
	}
	cmd &= ~cfClear;

	//
	// check for valid solid
	//
	if( !pSolid )
		return;

	//
	// check for face in list, -1 from FindFaceInList indicates face not found
	//
	CMapFace *pFace = pSolid->GetFace( faceIndex );
	int selectIndex = FindFaceInList( pFace );
	bool bFoundInList = ( selectIndex != -1 );

	//
	// handle the face list selection
	//
	if( ( clickMode == ModeSelect ) || ( clickMode == ModeLiftSelect ) )
	{
		// toggle selection if necessary
		if( cmd == cfToggle )
		{
			cmd = bFoundInList ? cfUnselect : cfSelect;			
		}

		CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
		if( ( cmd == cfSelect ) && !bFoundInList )
		{
			// add face to list
			StoredFace_t sf;
			sf.pMapDoc = pDoc;
			sf.pMapFace = pFace;
			sf.pMapSolid = pSolid;
			m_Faces.AddToTail( sf );
			pFace->SetSelectionState( SELECT_NORMAL );
		}
		else if( ( cmd == cfUnselect ) && bFoundInList )
		{
			// remove from list
			m_Faces.Remove( selectIndex );
			pFace->SetSelectionState( SELECT_NONE );
			EditDispHandle_t handle = pFace->GetDisp();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
			if( pDisp )
			{
				pDisp->ResetTexelHitIndex();
			}
		}

		if ( pDoc )
			pDoc->UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
	}

	//
	// pass to children (pages)
	//
	m_MaterialPage.ClickFace( pSolid, faceIndex, cmd, clickMode );
	m_DispPage.ClickFace( pSolid, faceIndex, cmd, clickMode );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditSheet::SetVisibility( bool bVisible )
{
	if( bVisible )
	{
		ShowWindow( SW_SHOW );
		SetActivePage( &m_DispPage );			// gross hack to active the default button!!!!
		SetActivePage( &m_MaterialPage );
		m_MaterialPage.UpdateDialogData();
		m_DispPage.UpdateDialogData();
	}
	else
	{
		ShowWindow( SW_HIDE );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CFaceEditSheet::PreTranslateMessage( MSG *pMsg )
{
	HACCEL hAccel = GetMainWnd()->GetAccelTable();
	if( !(hAccel && ::TranslateAccelerator( GetMainWnd()->m_hWnd, hAccel, pMsg ) ) )
	{
		if (IsDialogMessage(pMsg))
		{
			return(TRUE);
		}

		return CWnd::PreTranslateMessage( pMsg );
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditSheet::OnClose( void )
{
	// make sure all dialogs are closed upon exit!!
	m_DispPage.CloseAllDialogs();
	m_DispPage.ResetForceShows();

	// toggle the face edit sheet and texture bar!
	GetMainWnd()->ShowFaceEditSheetOrTextureBar( false );

	// Force the material page to the original tool.  This will clear out all
	// dialogs
	m_MaterialPage.SetMaterialPageTool( CFaceEditMaterialPage::MATERIALPAGETOOL_MATERIAL );

	// set the tool pointer as default tool
	ToolManager()->SetTool(TOOL_POINTER);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditSheet::CloseAllPageDialogs( void )
{
	// Make sure all dialogs are closed upon exit!
	m_DispPage.CloseAllDialogs();
	m_DispPage.ResetForceShows();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditSheet::UpdateControls( void )
{
	// Currently this is only needed for the material page.
	m_MaterialPage.UpdateDialogData();
}
