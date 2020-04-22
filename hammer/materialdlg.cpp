//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <stdafx.h>
#include "materialdlg.h"
#include "FaceEditSheet.h"
#include "mapdoc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//=============================================================================
//
// Face Smoothing Dialog Functions
//
BEGIN_MESSAGE_MAP( CFaceSmoothingDlg, CDialog )
	//{{AFX_MSG_MAP(CFaceSmoothingDlg)
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_1, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_2, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_3, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_4, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_5, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_6, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_7, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_8, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_9, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_10, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_11, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_12, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_13, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_14, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_15, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_16, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_17, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_18, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_19, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_20, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_21, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_22, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_23, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_24, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_25, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_26, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_27, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_28, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_29, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_30, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_31, OnButtonGroup )
	ON_COMMAND_EX( ID_SMOOTHING_GROUP_32, OnButtonGroup )

	ON_WM_CLOSE()
	ON_WM_DESTROY()	
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFaceSmoothingDlg::CFaceSmoothingDlg( CWnd *pParent ) : CDialog( CFaceSmoothingDlg::IDD, pParent )
{
	InitButtonIDs();
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
CFaceSmoothingDlg::~CFaceSmoothingDlg()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CFaceSmoothingDlg::OnInitDialog( void )
{
	static bool bInit = false;

	CDialog::OnInitDialog();

	if ( bInit )
	{
		SetWindowPos( &wndTop, m_DialogPosRect.left, m_DialogPosRect.top,
			          m_DialogPosRect.Width(), m_DialogPosRect.Height(), SWP_NOZORDER );
	}

	// Update the controls.
	UpdateControls();

	// Initialized.
	bInit = true;

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFaceSmoothingDlg::OnClose( void )
{
	// Get the parent and set the material tool as the current tool.
	CFaceEditSheet *pSheet = static_cast<CFaceEditSheet*>( GetParent() );
	if ( pSheet )
	{
		pSheet->m_MaterialPage.SetMaterialPageTool( CFaceEditMaterialPage::MATERIALPAGETOOL_MATERIAL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFaceSmoothingDlg::OnDestroy( void )
{
	// Save the current window position.
	GetWindowRect( &m_DialogPosRect );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFaceSmoothingDlg::InitButtonIDs( void )
{
	m_ButtonIDs[0]  = 0;						// Default - no button!
	m_ButtonIDs[1]  = ID_SMOOTHING_GROUP_1;
	m_ButtonIDs[2]  = ID_SMOOTHING_GROUP_2;
	m_ButtonIDs[3]  = ID_SMOOTHING_GROUP_3;
	m_ButtonIDs[4]  = ID_SMOOTHING_GROUP_4;
	m_ButtonIDs[5]  = ID_SMOOTHING_GROUP_5;
	m_ButtonIDs[6]  = ID_SMOOTHING_GROUP_6;
	m_ButtonIDs[7]  = ID_SMOOTHING_GROUP_7;
	m_ButtonIDs[8]  = ID_SMOOTHING_GROUP_8;
	m_ButtonIDs[9]  = ID_SMOOTHING_GROUP_9;
	m_ButtonIDs[10] = ID_SMOOTHING_GROUP_10;
	m_ButtonIDs[11] = ID_SMOOTHING_GROUP_11;
	m_ButtonIDs[12] = ID_SMOOTHING_GROUP_12;
	m_ButtonIDs[13] = ID_SMOOTHING_GROUP_13;
	m_ButtonIDs[14] = ID_SMOOTHING_GROUP_14;
	m_ButtonIDs[15] = ID_SMOOTHING_GROUP_15;
	m_ButtonIDs[16] = ID_SMOOTHING_GROUP_16;
	m_ButtonIDs[17] = ID_SMOOTHING_GROUP_17;
	m_ButtonIDs[18] = ID_SMOOTHING_GROUP_18;
	m_ButtonIDs[19] = ID_SMOOTHING_GROUP_19;
	m_ButtonIDs[20] = ID_SMOOTHING_GROUP_20;
	m_ButtonIDs[21] = ID_SMOOTHING_GROUP_21;
	m_ButtonIDs[22] = ID_SMOOTHING_GROUP_22;
	m_ButtonIDs[23] = ID_SMOOTHING_GROUP_23;
	m_ButtonIDs[24] = ID_SMOOTHING_GROUP_24;
	m_ButtonIDs[25] = ID_SMOOTHING_GROUP_25;
	m_ButtonIDs[26] = ID_SMOOTHING_GROUP_26;
	m_ButtonIDs[27] = ID_SMOOTHING_GROUP_27;
	m_ButtonIDs[28] = ID_SMOOTHING_GROUP_28;
	m_ButtonIDs[29] = ID_SMOOTHING_GROUP_29;
	m_ButtonIDs[30] = ID_SMOOTHING_GROUP_30;
	m_ButtonIDs[31] = ID_SMOOTHING_GROUP_31;
	m_ButtonIDs[32] = ID_SMOOTHING_GROUP_32;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CFaceSmoothingDlg::OnButtonGroup( UINT uCmd )
{
	int iGroup = GetSmoothingGroup( uCmd );
	if ( iGroup == -1 )
		return FALSE;

	// Add or remove.
	CButton *pGroupButton = ( CButton* )GetDlgItem( uCmd );
	if ( !pGroupButton )
		return FALSE;

	bool bAdd = ( pGroupButton->GetCheck() != 0 );

	// Get the selected faces and add them to the appropriate group.
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if ( pSheet )
	{
		int nFaceCount = pSheet->GetFaceListCount();
		for ( int iFace = 0; iFace < nFaceCount; ++iFace )
		{
			CMapFace *pFace = pSheet->GetFaceListDataFace( iFace );
			if ( pFace )
			{
				if ( bAdd )
				{
					pFace->AddSmoothingGroup( iGroup );
				}
				else
				{
					pFace->RemoveSmoothingGroup( iGroup );
				}
			}
		}
	}

	UpdateControls();

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
UINT CFaceSmoothingDlg::GetSmoothingGroup( UINT uCmd )
{
	for ( int iGroup = 1; iGroup <= SMOOTHING_GROUP_MAX_COUNT; ++iGroup )
	{
		if ( m_ButtonIDs[iGroup] == uCmd )
			return iGroup;
	}

	return 0xFFFFFFFF;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CFaceSmoothingDlg::GetActiveSmoothingGroup( void )
{
	for ( int iButton = 1; iButton <= SMOOTHING_GROUP_MAX_COUNT; ++iButton )
	{
		CButton *pGroupButton = ( CButton* )GetDlgItem( m_ButtonIDs[iButton] );
		if ( !pGroupButton )
			continue;

		if ( pGroupButton->GetCheck() != 0 )
			return iButton;
	}

	return 0xFFFFFFFF;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFaceSmoothingDlg::UpdateControls( void )
{
	// Get the selected faces and add them to the appropriate group.
	CFaceEditSheet *pSheet = ( CFaceEditSheet* )GetParent();
	if ( !pSheet )
		return;

	// Check for a face count.
	int nFaceCount = pSheet->GetFaceListCount();
	if ( nFaceCount == 0 )
		return;

	// Keep a list of used groups.
	int nGroupCounts[SMOOTHING_GROUP_MAX_COUNT+1];
	for ( int iGroup = 0; iGroup <= SMOOTHING_GROUP_MAX_COUNT; ++iGroup )
	{
		nGroupCounts[iGroup] = 0;
	}

	for ( int iFace = 0; iFace < nFaceCount; ++iFace )
	{
		CMapFace *pFace = pSheet->GetFaceListDataFace( iFace );
		if ( pFace )
		{
			for ( int iGroup = 1; iGroup <= SMOOTHING_GROUP_MAX_COUNT; ++iGroup )
			{
				if ( pFace->InSmoothingGroup( iGroup ) )
				{
					nGroupCounts[iGroup]++;
				}
			}
		}
	}

	CheckGroupButtons( nGroupCounts, nFaceCount );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFaceSmoothingDlg::CheckGroupButtons( int *pGroupCounts, int nFaceCount )
{
	m_bColorOverride = true;

	for ( int iButton = 1; iButton <= SMOOTHING_GROUP_MAX_COUNT; ++iButton )
	{
		CButton *pGroupButton = ( CButton* )GetDlgItem( m_ButtonIDs[iButton] );
		if ( !pGroupButton )
			continue;
		
		pGroupButton->SetCheck( FALSE );

		if ( pGroupCounts[iButton] == 0 )
			continue;

		if ( pGroupCounts[iButton] == nFaceCount )
		{
			pGroupButton->SetCheck( TRUE );
		}
		else
		{
			// Todo: Come up with a better effect here!
			CDC *pDC = pGroupButton->GetDC();
			m_Brush.CreateSolidBrush( pDC->GetTextColor() );
			CRect buttonRect;
			pGroupButton->GetClientRect( buttonRect );
			pDC->FillRect( buttonRect, &m_Brush );
			m_Brush.DeleteObject();
			pGroupButton->ReleaseDC( pDC );
		}
	}

	m_bColorOverride = false;
}

//=============================================================================

BEGIN_MESSAGE_MAP( CFaceSmoothingVisualDlg, CDialog )
	//{{AFX_MSG_MAP(CFaceSmoothingVisualDlg)
	ON_COMMAND_EX( ID_SG_VISUAL_1, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_2, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_3, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_4, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_5, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_6, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_7, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_8, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_9, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_10, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_11, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_12, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_13, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_14, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_15, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_16, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_17, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_18, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_19, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_20, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_21, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_22, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_23, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_24, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_25, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_26, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_27, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_28, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_29, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_30, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_31, OnButtonGroup )
	ON_COMMAND_EX( ID_SG_VISUAL_32, OnButtonGroup )
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFaceSmoothingVisualDlg::CFaceSmoothingVisualDlg( CWnd *pParent ) : CDialog( CFaceSmoothingDlg::IDD, pParent )
{
	InitButtonIDs();
}

//-----------------------------------------------------------------------------
// Purpose: Deconstructor
//-----------------------------------------------------------------------------
CFaceSmoothingVisualDlg::~CFaceSmoothingVisualDlg()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CFaceSmoothingVisualDlg::OnInitDialog( void )
{
	CDialog::OnInitDialog();

//	SetWindowPos( &wndTop, m_DialogPosRect.left, m_DialogPosRect.top,
//		m_DialogPosRect.Width(), m_DialogPosRect.Height(), SWP_NOZORDER );

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CFaceSmoothingVisualDlg::OnButtonGroup( UINT uCmd )
{
	int iGroup = GetSmoothingGroup( uCmd );
	if ( iGroup == -1 )
		return FALSE;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( pDoc )
	{
		pDoc->SetSmoothingGroupVisual( iGroup );
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFaceSmoothingVisualDlg::InitButtonIDs( void )
{
	m_ButtonIDs[0]  = 0;						// Default - no button!
	m_ButtonIDs[1]  = ID_SG_VISUAL_1;
	m_ButtonIDs[2]  = ID_SG_VISUAL_2;
	m_ButtonIDs[3]  = ID_SG_VISUAL_3;
	m_ButtonIDs[4]  = ID_SG_VISUAL_4;
	m_ButtonIDs[5]  = ID_SG_VISUAL_5;
	m_ButtonIDs[6]  = ID_SG_VISUAL_6;
	m_ButtonIDs[7]  = ID_SG_VISUAL_7;
	m_ButtonIDs[8]  = ID_SG_VISUAL_8;
	m_ButtonIDs[9]  = ID_SG_VISUAL_9;
	m_ButtonIDs[10] = ID_SG_VISUAL_10;
	m_ButtonIDs[11] = ID_SG_VISUAL_11;
	m_ButtonIDs[12] = ID_SG_VISUAL_12;
	m_ButtonIDs[13] = ID_SG_VISUAL_13;
	m_ButtonIDs[14] = ID_SG_VISUAL_14;
	m_ButtonIDs[15] = ID_SG_VISUAL_15;
	m_ButtonIDs[16] = ID_SG_VISUAL_16;
	m_ButtonIDs[17] = ID_SG_VISUAL_17;
	m_ButtonIDs[18] = ID_SG_VISUAL_18;
	m_ButtonIDs[19] = ID_SG_VISUAL_19;
	m_ButtonIDs[20] = ID_SG_VISUAL_20;
	m_ButtonIDs[21] = ID_SG_VISUAL_21;
	m_ButtonIDs[22] = ID_SG_VISUAL_22;
	m_ButtonIDs[23] = ID_SG_VISUAL_23;
	m_ButtonIDs[24] = ID_SG_VISUAL_24;
	m_ButtonIDs[25] = ID_SG_VISUAL_25;
	m_ButtonIDs[26] = ID_SG_VISUAL_26;
	m_ButtonIDs[27] = ID_SG_VISUAL_27;
	m_ButtonIDs[28] = ID_SG_VISUAL_28;
	m_ButtonIDs[29] = ID_SG_VISUAL_29;
	m_ButtonIDs[30] = ID_SG_VISUAL_30;
	m_ButtonIDs[31] = ID_SG_VISUAL_31;
	m_ButtonIDs[32] = ID_SG_VISUAL_32;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
UINT CFaceSmoothingVisualDlg::GetSmoothingGroup( UINT uCmd )
{
	for ( int iGroup = 1; iGroup <= SMOOTHING_GROUP_MAX_COUNT; ++iGroup )
	{
		if ( m_ButtonIDs[iGroup] == uCmd )
			return iGroup;
	}

	return 0xFFFFFFFF;
}
