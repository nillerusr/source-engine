//========= Copyright Valve Corporation, All rights reserved. ============//
// ManifestDialog.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "ManifestDialog.h"
#include "MapDoc.h"
#include "Manifest.h"
#include "MapInstance.h"
#include "ControlBarIDs.h"
#include "p4lib/ip4.h"

// CManifestMove dialog

IMPLEMENT_DYNAMIC(CManifestMove, CDialog)

//-----------------------------------------------------------------------------
// Purpose: contructor
// Input  : pParent - the parent window of this dialog
//-----------------------------------------------------------------------------
CManifestMove::CManifestMove( bool bIsMove, CWnd* pParent /*=NULL*/ )
	: CDialog(CManifestMove::IDD, pParent)
{
	m_bIsMove = bIsMove;
}


//-----------------------------------------------------------------------------
// Purpose: the default destructor
//-----------------------------------------------------------------------------
CManifestMove::~CManifestMove()
{
}


//-----------------------------------------------------------------------------
// Purpose: MFC data exchange function
// Input  : pDX -
//-----------------------------------------------------------------------------
void CManifestMove::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MANIFEST_FILENAME, m_FileNameControl);
	DDX_Control(pDX, IDC_MANIFEST_CENTER_AROUND_BRUSH, m_CenterContentsControl);
	DDX_Control(pDX, IDC_MANIFEST_NAME2, m_FriendlyNameControl);
	DDX_Control(pDX, IDC_FULL_PATH, m_FullPathNameControl);
}


BEGIN_MESSAGE_MAP(CManifestMove, CDialog)
	ON_EN_CHANGE(IDC_MANIFEST_FILENAME, &CManifestMove::OnEnChangeManifestFilename)
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: the default dialog initialization routine
// Output : returns true upon success
//-----------------------------------------------------------------------------
BOOL CManifestMove::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_FileNameControl.SetWindowText( "" );

	if ( m_bIsMove == false )
	{
		m_CenterContentsControl.ShowWindow( SW_HIDE );
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: This function responds to the user hitting the OK button.
//-----------------------------------------------------------------------------
void CManifestMove::OnOK()
{
	char		FullFileName[ MAX_PATH ];

	CDialog::OnOK();

	m_FriendlyNameControl.GetWindowText( m_FriendlyName );
	m_FileNameControl.GetWindowText( m_FileName );
	strcpy( FullFileName, m_FileName );
	V_SetExtension( FullFileName, ".vmf", sizeof( FullFileName ) );
	m_FileName = FullFileName;
	m_CenterContents = ( m_CenterContentsControl.GetCheck() == BST_CHECKED );
}


//-----------------------------------------------------------------------------
// Purpose: this function is called when the file name edit box is updated.  it sets the static text
//			field for the full file name.
//-----------------------------------------------------------------------------
void CManifestMove::OnEnChangeManifestFilename()
{
	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();

	if ( activeDoc )
	{
		if ( activeDoc->GetManifest() )
		{
			CManifest	*pManifest = activeDoc->GetManifest();
			char		FullFileName[ MAX_PATH ];

			m_FileNameControl.GetWindowText( m_FileName );
			strcpy( FullFileName, m_FileName );
			GetDlgItem( IDOK )->EnableWindow( FullFileName[ 0 ] != 0 );
			V_SetExtension( FullFileName, ".vmf", sizeof( FullFileName ) );
			m_FileName = FullFileName;
			pManifest->GetFullMapPath( m_FileName, FullFileName );

			m_FullPathNameControl.SetWindowText( FullFileName );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this is the default constructor for the manifest list box
//-----------------------------------------------------------------------------
CManifestListBox::CManifestListBox( void ) : 
	CListBox()
{
	m_Icons.Create( IDB_MANIFEST_ICONS, 16, 1, RGB( 0, 255, 255 ) );

	m_ManifestFilterMenu.LoadMenu( IDR_MANIFEST_FILTER );
	m_ManifestFilterSecondaryMenu.Attach( ::GetSubMenu( m_ManifestFilterMenu.m_hMenu, 0 ) );
	m_ManifestFilterPrimaryMenu.Attach( ::GetSubMenu( m_ManifestFilterMenu.m_hMenu, 1 ) );
	m_ManifestFilterBlankMenu.Attach( ::GetSubMenu( m_ManifestFilterMenu.m_hMenu, 2 ) );

	m_pTrackerManifestMap = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: this function handles the owner draw of the list box for each manifest sub map
// Input  : lpDrawItemStruct - the list box item being rendered
//-----------------------------------------------------------------------------
void CManifestListBox::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	CDC		dc;
	RECT	&r = lpDrawItemStruct->rcItem;
	RECT	ItemRect;

	ItemRect = r;
	ItemRect.left += 36;

	dc.Attach( lpDrawItemStruct->hDC );
	dc.SetROP2( R2_COPYPEN );

	CPen m_hPen, *pOldPen;
	m_hPen.CreatePen( PS_SOLID, 1, ::GetSysColor( COLOR_3DSHADOW ) );
	pOldPen = dc.SelectObject( &m_hPen );

	int iBackIndex = COLOR_WINDOW;
	int iForeIndex = COLOR_WINDOWTEXT;

	CManifestMap	*pManifestMap = NULL;
	CMapDoc			*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( activeDoc && activeDoc->GetManifest() )
	{
		CManifest *pManifest = activeDoc->GetManifest();
		pManifestMap = pManifest->GetMap( ( int )lpDrawItemStruct->itemData );
	}

	dc.FillSolidRect( &lpDrawItemStruct->rcItem, ::GetSysColor( COLOR_3DFACE ) );
	if ( ( lpDrawItemStruct->itemState & ODS_SELECTED ) )
	{
		dc.FillSolidRect( &ItemRect, ::GetSysColor( COLOR_HIGHLIGHT ) );
		iForeIndex = COLOR_HIGHLIGHTTEXT;
	}
	else if ( m_pTrackerManifestMap == pManifestMap )
	{
		dc.FillSolidRect( &ItemRect, ::GetSysColor( COLOR_INACTIVECAPTION ) );
	}
		
	dc.MoveTo( r.left, r.top );
	dc.LineTo( r.right, r.top );
	dc.MoveTo( r.left, r.bottom );
	dc.LineTo( r.right, r.bottom );
	dc.MoveTo( r.left + 35, r.top );
	dc.LineTo( r.left + 35, r.bottom );

	if ( pManifestMap )
	{
		// draw the visible icon
		RECT	VisibleRect;
		POINT	p;

		VisibleRect.left = r.left + 1;
		VisibleRect.top = r.top + 2;
		VisibleRect.right = VisibleRect.left + 16;
		VisibleRect.bottom = VisibleRect.top + 16;
		p.x = VisibleRect.left;
		p.y = VisibleRect.top;

		dc.FillSolidRect( &VisibleRect, ::GetSysColor( COLOR_3DFACE ) );
		dc.Draw3dRect( &VisibleRect, ::GetSysColor( COLOR_3DSHADOW ), ::GetSysColor( COLOR_3DHILIGHT ) );

		if ( pManifestMap->m_bVisible )
		{
			m_Icons.Draw( &dc, 0, p, ILD_NORMAL );
		}

		VisibleRect.left = r.left + 1 + 17;
		VisibleRect.right = VisibleRect.left + 16;
		p.x = VisibleRect.left;
		p.y = VisibleRect.top;

		dc.FillSolidRect( &VisibleRect, ::GetSysColor( COLOR_3DFACE ) );
		dc.Draw3dRect( &VisibleRect, ::GetSysColor( COLOR_3DSHADOW ), ::GetSysColor( COLOR_3DHILIGHT ) );

		if ( pManifestMap->m_bProtected )
		{
			m_Icons.Draw( &dc, 1, p, ILD_NORMAL );
		}
		else if ( pManifestMap->m_bReadOnly )
		{
			m_Icons.Draw( &dc, 2, p, ILD_NORMAL );
		}
		else if ( pManifestMap->m_bCheckedOut )
		{
			m_Icons.Draw( &dc, 3, p, ILD_NORMAL );
		}


		VisibleRect.left = r.left + 1 + 17;
		VisibleRect.top = r.top + 2 + 17;
		VisibleRect.right = VisibleRect.left + 16;
		VisibleRect.bottom = VisibleRect.top + 16;
		p.x = VisibleRect.left;
		p.y = VisibleRect.top;

		dc.FillSolidRect( &VisibleRect, ::GetSysColor( COLOR_3DFACE ) );
		dc.Draw3dRect( &VisibleRect, ::GetSysColor( COLOR_3DSHADOW ), ::GetSysColor( COLOR_3DHILIGHT ) );

		if ( pManifestMap->m_Map->IsModified() )
		{
			m_Icons.Draw( &dc, 4, p, ILD_NORMAL );
		}


		dc.SetTextColor( GetSysColor( iForeIndex ) );
		dc.SetBkColor( GetSysColor( iBackIndex ) );
	
		dc.SetBkMode( TRANSPARENT );
		VisibleRect.left = r.left + 36;
		VisibleRect.top = r.top + 1;
		VisibleRect.right = r.right - 1;
		VisibleRect.bottom = r.bottom - 1;

		dc.DrawText( pManifestMap->m_FriendlyName, -1, &VisibleRect, DT_LEFT | DT_VCENTER );
	}

	if ( ( lpDrawItemStruct->itemState & ODS_FOCUS ) )
	{
		dc.DrawFocusRect( &ItemRect );
	}

	dc.SelectObject( pOldPen );
	dc.Detach();
}


//-----------------------------------------------------------------------------
// Purpose: this function returns the height for the owner draw item
// Output : sets the item height
//-----------------------------------------------------------------------------
void CManifestListBox::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
	lpMeasureItemStruct->itemHeight = 36;
}


//-----------------------------------------------------------------------------
// Purpose: this function compares two items - it does nothing
// Output : returns 0 indicating no sorting has happened
//-----------------------------------------------------------------------------
int CManifestListBox::CompareItem(LPCOMPAREITEMSTRUCT lpCompareItemStruct)
{
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: this function responds to the left mouse button being pressed.  the left side of the manifest item
//			is the visibility toggle.  the right side handles the selection to make it the primary map.
// Input  : nFlags - ignored
//			point - the location of the click
//-----------------------------------------------------------------------------
void CManifestListBox::OnLButtonDown(UINT nFlags, CPoint point)
{
	BOOL	bOutside;

	m_pTrackerManifestMap = NULL;

	int index = ItemFromPoint( point, bOutside );
	if ( bOutside == false )
	{
		int height = GetItemHeight( index );

		if ( point.x < 36 )
		{
			CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
			if ( activeDoc && activeDoc->GetManifest() )
			{
				CManifest		*pManifest = activeDoc->GetManifest();
				CManifestMap	*pManifestMap = pManifest->GetMap( ( int )GetItemData( index ) );
				if ( pManifestMap )
				{
					if ( point.x < 18 && ( point.y % height ) < 18 )
					{
						pManifest->SetVisibility( pManifestMap, !pManifestMap->m_bVisible );
					}
				}
			}
		}
		else
		{
			CListBox::OnLButtonDown( nFlags, point );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: passes along the double click message if it is on the right side of a list item
// Input  : nFlags - not used
//			point - the mouse coordinates
//-----------------------------------------------------------------------------
void CManifestListBox::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	BOOL	bOutside;

	m_pTrackerManifestMap = NULL;

//	int index = 
		ItemFromPoint( point, bOutside );
	if ( bOutside == false )
	{
		if ( point.x >= 36 )
		{
			CListBox::OnLButtonDblClk( nFlags, point );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this handles bringing up the specific context menu based upon if you
//			are over the primary map, a different sub map, or an empty spot.
// Input  : nFlags - not used
//			point - the mouse coordinates
//-----------------------------------------------------------------------------
void CManifestListBox::OnRButtonUp(UINT nFlags, CPoint point)
{
	BOOL		bOutside;
	CMenu		*pWhichMenu;
	const UINT	nEnable = MF_BYCOMMAND | MF_ENABLED;
	const UINT	nDisable = MF_BYCOMMAND | MF_DISABLED | MF_GRAYED;


	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifest		*pManifest = activeDoc->GetManifest();
	m_pTrackerManifestMap = NULL;

	CPoint ptScreen( point.x, point.y );
	ClientToScreen(& ptScreen );

	int index = ItemFromPoint( point, bOutside );

	if ( bOutside )
	{
		pWhichMenu = &m_ManifestFilterBlankMenu;
	}
	else
	{
		m_pTrackerManifestMap = pManifest->GetMap( ( int )GetItemData( index ) );

		if ( m_pTrackerManifestMap->m_bPrimaryMap )
		{
			pWhichMenu = &m_ManifestFilterPrimaryMenu;
		}
		else
		{
			pWhichMenu = &m_ManifestFilterSecondaryMenu;
		}
	}

	if ( activeDoc->GetSelection()->IsEmpty() == false )
	{
		pWhichMenu->EnableMenuItem( ID_MOVESELECTIONTO_SUBMAP, nEnable );
		pWhichMenu->EnableMenuItem( ID_MOVESELECTIONTO_NEWSUBMAP, nEnable );
	}
	else
	{
		pWhichMenu->EnableMenuItem( ID_MOVESELECTIONTO_SUBMAP, nDisable );
		pWhichMenu->EnableMenuItem( ID_MOVESELECTIONTO_NEWSUBMAP, nDisable );
	}

	if ( pManifest->GetNumMaps() > 1 )
	{
		pWhichMenu->EnableMenuItem( ID_MANIFEST_REMOVE, nEnable );	
	}
	else
	{
		pWhichMenu->EnableMenuItem( ID_MANIFEST_REMOVE, nDisable );
	}

	pWhichMenu->EnableMenuItem( ID_VERSIONCONTROL_CHECKOUT, nDisable );
	pWhichMenu->EnableMenuItem( ID_VERSIONCONTROL_CHECKIN, nDisable );
	pWhichMenu->EnableMenuItem( ID_VERSIONCONTROL_ADD, nDisable );

	if ( p4 && m_pTrackerManifestMap )
	{
		if ( m_pTrackerManifestMap->m_bIsVersionControlled )
		{
			if ( m_pTrackerManifestMap->m_bCheckedOut )
			{
				pWhichMenu->EnableMenuItem( ID_VERSIONCONTROL_CHECKIN, nEnable );
			}
			else
			{
				pWhichMenu->EnableMenuItem( ID_VERSIONCONTROL_CHECKOUT, nEnable );
			}
		}
		else
		{
			pWhichMenu->EnableMenuItem( ID_VERSIONCONTROL_ADD, nEnable );
		}
	}

	Invalidate();
	pWhichMenu->TrackPopupMenu( TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y, this );
	
	CListBox::OnRButtonUp(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: this function handles the menu command for moving the selection to an existing sub map
//-----------------------------------------------------------------------------
void CManifestListBox::OnMoveSelectionToSubMap()
{
	if ( !m_pTrackerManifestMap )
	{
		return;
	}

	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifest		*pManifest = activeDoc->GetManifest();

	pManifest->MoveSelectionToSubmap( m_pTrackerManifestMap, false );

	m_pTrackerManifestMap = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: this function handles the menu command for moving the selection to a new sub map
//-----------------------------------------------------------------------------
void CManifestListBox::OnMoveSelectionToNewSubMap()
{
	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifestMove	ManifestMove( true );

	if ( ManifestMove.DoModal() == IDOK )
	{
		CString	FriendlyName, FileName;

		ManifestMove.GetFriendlyName( FriendlyName );
		ManifestMove.GetFileName( FileName );

		CManifest		*pManifest = activeDoc->GetManifest();
		CManifestMap	*pNewManifestMap = pManifest->MoveSelectionToNewSubmap( FriendlyName, FileName, ManifestMove.GetCenterContents() );
		if ( pNewManifestMap )
		{
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CManifestListBox::OnVersionControlCheckOut()
{
	CMapDoc		*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}
	CManifest	*pManifest = activeDoc->GetManifest();

	if ( !p4->OpenFileForEdit( m_pTrackerManifestMap->m_AbsoluteMapFileName ) )
	{
		char temp[ 2048 ];
		
		sprintf( temp, "Could not check out map: %s", p4->GetLastError() );
		AfxMessageBox( temp, MB_ICONHAND | MB_OK );
	}
	else
	{
		pManifest->CheckFileStatus();
		Invalidate();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CManifestListBox::OnVersionControlCheckIn()
{
	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifest		*pManifest = activeDoc->GetManifest();

	pManifest->m_bDefaultCheckin = false;
	for( int i = 0; i < pManifest->GetNumMaps(); i++ )
	{
		CManifestMap	*pManifestMap = pManifest->GetMap( i );
		pManifestMap->m_bDefaultCheckin = false;
	}
	if ( m_pTrackerManifestMap )
	{
		m_pTrackerManifestMap->m_bDefaultCheckin = true;
	}

	CManifestCheckin	ManifestCheckin;
	if ( ManifestCheckin.DoModal() == IDOK )
	{
		pManifest->CheckFileStatus();
		Invalidate();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CManifestListBox::OnVersionControlAdd()
{
	CMapDoc		*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}
	CManifest	*pManifest = activeDoc->GetManifest();

	if ( !p4->OpenFileForAdd( m_pTrackerManifestMap->m_AbsoluteMapFileName ) )
	{
		char temp[ 2048 ];

		sprintf( temp, "Could not add map: %s", p4->GetLastError() );
		AfxMessageBox( temp, MB_ICONHAND | MB_OK );
	}
	else
	{
		pManifest->CheckFileStatus();
		Invalidate();
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function handles the menu command to insert a new empty sub map
//-----------------------------------------------------------------------------
void CManifestListBox::OnInsertEmptySubMap()
{
	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifestMove	ManifestMove( false );

	if ( ManifestMove.DoModal() == IDOK )
	{
		CString	FriendlyName, FileName;

		ManifestMove.GetFriendlyName( FriendlyName );
		ManifestMove.GetFileName( FileName );

		CManifest		*pManifest = activeDoc->GetManifest();
		CManifestMap	*pNewManifestMap = pManifest->AddNewSubmap( FriendlyName, FileName );
		if ( pNewManifestMap )
		{
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function handles the menu command to insert an existing sub map
//-----------------------------------------------------------------------------
void CManifestListBox::OnInsertExistingSubMap()
{
	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifest		*pManifest = activeDoc->GetManifest();

	pManifest->AddExistingMap();
}


//-----------------------------------------------------------------------------
// Purpose: this function will handle bringing up the properties dialog of a sub map
//-----------------------------------------------------------------------------
void CManifestListBox::OnManifestProperties()
{
	if ( !m_pTrackerManifestMap )
	{
		return;
	}

	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifest		*pManifest = activeDoc->GetManifest();

	CManifestMapDlg	ManifestMapDlg( m_pTrackerManifestMap, this );

	if ( ManifestMapDlg.DoModal() == IDOK )
	{
		pManifest->SetManifestPrefsModifiedFlag( true );
		Invalidate();
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will remove an existing sub map from the manifest, but will
//			not delete the file.
//-----------------------------------------------------------------------------
void CManifestListBox::OnManifestRemove()
{
	if ( !m_pTrackerManifestMap )
	{
		return;
	}

	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		return;
	}

	CManifest		*pManifest = activeDoc->GetManifest();

	if ( AfxMessageBox( "Are you sure you want to remove this sub map from the manifest?", MB_YESNO | MB_ICONQUESTION ) == IDNO )
	{
		return;
	}

	pManifest->RemoveSubMap( m_pTrackerManifestMap );
	pManifest->SetPrimaryMap( pManifest->GetMap( 0 ) );
	m_pTrackerManifestMap = NULL;

	AfxMessageBox( "The sub map has been removed from the manifest, but the file has not been deleted.", MB_OK | MB_ICONASTERISK );
}


BEGIN_MESSAGE_MAP(CManifestListBox, CListBox)
	//{{AFX_MSG_MAP(CManifestListBox)
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONUP()
	//}}AFX_MSG_MAP
	ON_WM_LBUTTONDBLCLK()
	ON_COMMAND(ID_MOVESELECTIONTO_SUBMAP, OnMoveSelectionToSubMap)
	ON_COMMAND(ID_MOVESELECTIONTO_NEWSUBMAP, OnMoveSelectionToNewSubMap)
	ON_COMMAND(ID_VERSIONCONTROL_CHECKOUT, OnVersionControlCheckOut)
	ON_COMMAND(ID_VERSIONCONTROL_CHECKIN, OnVersionControlCheckIn)
	ON_COMMAND(ID_VERSIONCONTROL_ADD, OnVersionControlAdd)
	ON_COMMAND(ID_INSERT_EMPTYSUBMAP, OnInsertEmptySubMap)
	ON_COMMAND(ID_INSERT_EXISTINGSUBMAP, OnInsertExistingSubMap)
	ON_COMMAND(ID_MANIFEST_PROPERTIES, OnManifestProperties)
	ON_COMMAND(ID_MANIFEST_REMOVE, OnManifestRemove)
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: this function will create the hammber bar window
// Input  : pParentWnd - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CManifestFilter::Create(CWnd *pParentWnd)
{
	if (!CHammerBar::Create(pParentWnd, IDD_MANIFEST_CONTROL, CBRS_RIGHT | CBRS_SIZE_DYNAMIC, IDCB_MANIFEST_CONTROL, "Manifest Control"))
	{
		return FALSE;
	}

	m_ManifestList.SubclassDlgItem( IDC_MANIFEST_LIST, this );
	m_ManifestList.SetItemHeight( 0, 36 );

	AddControl( IDC_MANIFEST_LIST, GROUP_BOX );

	UpdateManifestList();

	m_pBkBrush = new CBrush( ::GetSysColor( COLOR_3DFACE ) );

	bInitialized = TRUE;

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: default destructor
//-----------------------------------------------------------------------------
CManifestFilter::~CManifestFilter()
{
}


//-----------------------------------------------------------------------------
// Purpose: this function will update the manifest list ( for when sub maps are added / removed 
//			or the primary map is changed.
//-----------------------------------------------------------------------------
void CManifestFilter::UpdateManifestList( void )
{
	m_ManifestList.ResetContent();

	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( activeDoc && activeDoc->GetManifest() )
	{
		CManifest *pManifest = activeDoc->GetManifest();

		for( int i = 0; i < pManifest->GetNumMaps(); i++ )
		{
			CManifestMap	*pManifestMap = pManifest->GetMap( i );
			
			int index = m_ManifestList.AddString( "Manifest" );
			m_ManifestList.SetItemData( index, i );

			if ( pManifestMap->m_bPrimaryMap )
			{
				m_ManifestList.SetCurSel( index );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: data exchange function for assigning variables to controls
//-----------------------------------------------------------------------------
void CManifestFilter::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CManifestFilter, CHammerBar)
	ON_LBN_SELCHANGE(IDC_MANIFEST_LIST, &CManifestFilter::OnLbnSelchangeManifestList)
	ON_LBN_DBLCLK(IDC_MANIFEST_LIST, &CManifestFilter::OnLbnDblClkManifestList)
	ON_WM_CTLCOLOR()
	ON_WM_DESTROY()
	ON_WM_SIZE()
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: this function will handle the selection of a sub map to set it to the primary map
//-----------------------------------------------------------------------------
void CManifestFilter::OnLbnSelchangeManifestList()
{
	int nIndex = m_ManifestList.GetCurSel();
	int nCount = m_ManifestList.GetCount();

	if ( ( nIndex != LB_ERR ) && ( nCount > 1 ) )
	{
		CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
		if ( activeDoc && activeDoc->GetManifest() )
		{
			CManifest		*pManifest = activeDoc->GetManifest();
			CManifestMap	*pManifestMap = pManifest->GetMap( ( int )m_ManifestList.GetItemData( nIndex ) );

			pManifest->SetPrimaryMap( pManifestMap );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will bring up the properties dialog of a sub map
//-----------------------------------------------------------------------------
void CManifestFilter::OnLbnDblClkManifestList()
{
	int nIndex = m_ManifestList.GetCurSel();
//	int nCount = m_ManifestList.GetCount();

	if ( ( nIndex != LB_ERR ) )
	{
		CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
		if ( activeDoc && activeDoc->GetManifest() )
		{
			CManifest		*pManifest = activeDoc->GetManifest();
			CManifestMap	*pManifestMap = pManifest->GetMap( ( int )m_ManifestList.GetItemData( nIndex ) );

			CManifestMapDlg	ManifestMapDlg( pManifestMap, this );

			if ( ManifestMapDlg.DoModal() == IDOK )
			{
				pManifest->SetManifestPrefsModifiedFlag( true );
				m_ManifestList.Invalidate();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function sets the text and background color of the custom list box
// Input  : pDC - the display context
//			pWnd - the owning window
//			nCtlColor - the color type to be set
// Output : returns the brush for the control color
//-----------------------------------------------------------------------------
HBRUSH CManifestFilter::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	switch (nCtlColor) 
	{
		  case CTLCOLOR_LISTBOX:
			  pDC->SetTextColor( ::GetSysColor( COLOR_WINDOWTEXT ) );
			  pDC->SetBkColor( ::GetSysColor( COLOR_3DFACE ) );
			  return ( HBRUSH )( m_pBkBrush->GetSafeHandle() );

		  default:
			  return __super::OnCtlColor(pDC, pWnd, nCtlColor);
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function handles the destruction of the dialog to make sure the brush is destroyed
//-----------------------------------------------------------------------------
void CManifestFilter::OnDestroy()
{
	__super::OnDestroy();

	delete m_pBkBrush;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nType - 
//			cx - 
//			cy - 
//-----------------------------------------------------------------------------
void CManifestFilter::OnSize(UINT nType, int cx, int cy)
{
	// TODO: make larger / resizable when floating
	//if (IsFloating())
	//{
	//	CWnd *pwnd = GetDlgItem(IDC_GROUPS);
	//	if (pwnd && IsWindow(pwnd->GetSafeHwnd()))
	//	{
	//		pwnd->MoveWindow(2, 10, cx - 2, cy - 2, TRUE);
	//	}
	//}

	CHammerBar::OnSize(nType, cx, cy);
}


IMPLEMENT_DYNAMIC(CManifestMapDlg, CDialog)


//-----------------------------------------------------------------------------
// Purpose: default constructor
// Input  : pManifestMap - the map for this dialog
//			pParent - the parent window
//-----------------------------------------------------------------------------
CManifestMapDlg::CManifestMapDlg( CManifestMap *pManifestMap, CWnd* pParent /*=NULL*/ )
	: CDialog(CManifestMapDlg::IDD, pParent)
{
	m_pManifestMap = pManifestMap;
}


//-----------------------------------------------------------------------------
// Purpose: default destructor
//-----------------------------------------------------------------------------
CManifestMapDlg::~CManifestMapDlg()
{
}


//-----------------------------------------------------------------------------
// Purpose: data exchange function for assigning variables to controls
//-----------------------------------------------------------------------------
void CManifestMapDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MANIFEST_FRIENDLY_NAME, m_FriendlyNameControl);
	DDX_Control(pDX, IDC_MANIFEST_FULL_FILENAME, m_FullFileNameCtrl);
}


BEGIN_MESSAGE_MAP(CManifestMapDlg, CDialog)
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: routine to handle the initialization of the dialog
//-----------------------------------------------------------------------------
BOOL CManifestMapDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_FriendlyNameControl.SetWindowText( m_pManifestMap->m_FriendlyName );
	m_FullFileNameCtrl.SetWindowText( m_pManifestMap->m_AbsoluteMapFileName );

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: this function handles the user hitting ok
//-----------------------------------------------------------------------------
void CManifestMapDlg::OnOK()
{
	m_FriendlyNameControl.GetWindowText( m_pManifestMap->m_FriendlyName );

	CDialog::OnOK();
}



// CManifestCheckin dialog

IMPLEMENT_DYNAMIC(CManifestCheckin, CDialog)

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CManifestCheckin::CManifestCheckin(CWnd* pParent /*=NULL*/)
	: CDialog(CManifestCheckin::IDD, pParent)
{

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
CManifestCheckin::~CManifestCheckin()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CManifestCheckin::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CHECKIN_LIST, m_CheckinListCtrl);
	DDX_Control(pDX, IDC_CHECKIN_DESCRIPTION, m_DescriptionCtrl);
}


BEGIN_MESSAGE_MAP(CManifestCheckin, CDialog)
	ON_BN_CLICKED(IDOK, &CManifestCheckin::OnBnClickedOk)
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
BOOL CManifestCheckin::OnInitDialog()
{
	P4File_t	FileInfo;

	CDialog::OnInitDialog();

	m_CheckinListCtrl.SetExtendedStyle( m_CheckinListCtrl.GetExtendedStyle() | LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT );

	m_CheckinListCtrl.InsertColumn( 0, "", LVCFMT_LEFT, 30, -1 );
	m_CheckinListCtrl.InsertColumn( 1, "Status", LVCFMT_LEFT, 50, -1 );
	m_CheckinListCtrl.InsertColumn( 2, "Name", LVCFMT_LEFT, 100, -1 );
	m_CheckinListCtrl.InsertColumn( 3, "Folder", LVCFMT_LEFT, 350, -1 );

	if ( p4 == NULL ) 
	{
		return TRUE;
	}

	int nCount = 0;

	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( activeDoc && activeDoc->GetManifest() )
	{
		CManifest *pManifest = activeDoc->GetManifest();

		if ( pManifest->m_bCheckedOut )
		{
			if ( p4->GetFileInfo( pManifest->GetPathName(), &FileInfo ) == true )
			{
				int nIndex = m_CheckinListCtrl.InsertItem( nCount, "" );
				nCount++;
				m_CheckinListCtrl.SetItemData( nIndex, ( DWORD_PTR )NULL );
				switch( FileInfo.m_eOpenState )
				{	
					case P4FILE_OPENED_FOR_ADD:
						m_CheckinListCtrl.SetItemText( nIndex, 1, "Add" );
						break;

					case P4FILE_OPENED_FOR_EDIT:
						m_CheckinListCtrl.SetItemText( nIndex, 1, "Edit" );
						break;
				}
				m_CheckinListCtrl.SetItemText( nIndex, 2, p4->String( FileInfo.m_sName ) );
				m_CheckinListCtrl.SetItemText( nIndex, 3, p4->String( FileInfo.m_sPath ) );

				if ( pManifest->m_bDefaultCheckin )
				{
					ListView_SetItemState( m_CheckinListCtrl.m_hWnd, nIndex, INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ), LVIS_STATEIMAGEMASK );
				}
			}
		}

		for( int i = 0; i < pManifest->GetNumMaps(); i++ )
		{
			CManifestMap	*pManifestMap = pManifest->GetMap( i );

			if ( pManifestMap->m_bCheckedOut )
			{
				if ( p4->GetFileInfo( pManifestMap->m_AbsoluteMapFileName, &FileInfo ) == true )
				{
					int nIndex = m_CheckinListCtrl.InsertItem( nCount, "" );
					nCount++;
					m_CheckinListCtrl.SetItemData( nIndex, ( DWORD_PTR )pManifestMap );
					switch( FileInfo.m_eOpenState )
					{	
						case P4FILE_OPENED_FOR_ADD:
							m_CheckinListCtrl.SetItemText( nIndex, 1, "Add" );
							break;

						case P4FILE_OPENED_FOR_EDIT:
							m_CheckinListCtrl.SetItemText( nIndex, 1, "Edit" );
							break;
					}
					m_CheckinListCtrl.SetItemText( nIndex, 2, p4->String( FileInfo.m_sName ) );
					m_CheckinListCtrl.SetItemText( nIndex, 3, p4->String( FileInfo.m_sPath ) );

					if ( pManifestMap->m_bDefaultCheckin )
					{
						ListView_SetItemState( m_CheckinListCtrl.m_hWnd, nIndex, INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ), LVIS_STATEIMAGEMASK );
					}
				}
			}
		}
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CManifestCheckin::OnBnClickedOk()
{
	int nFileCount = 0;

	CMapDoc	*activeDoc = CMapDoc::GetActiveMapDoc();
	if ( !activeDoc || !activeDoc->GetManifest() )
	{
		OnOK();
	}

	CManifest *pManifest = activeDoc->GetManifest();

	for( int i = 0; i < m_CheckinListCtrl.GetItemCount(); i++ )
	{
		if ( m_CheckinListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
		{
			nFileCount++;
		}
	}

	if ( nFileCount > 0 )
	{
		CString	Description;
		m_DescriptionCtrl.GetWindowText( Description );
		if ( Description.GetLength() < 2 )
		{
			AfxMessageBox( "Please put in something descriptive for the description.  I took the time to type this dialog, the least you could do is type something!", MB_ICONHAND | MB_OK );
			return;
		}

		const char **ppFileNames = ( const char** )stackalloc( nFileCount * sizeof( char * ) );

		nFileCount = 0;
		for( int i = 0; i < m_CheckinListCtrl.GetItemCount(); i++ )
		{
			if ( m_CheckinListCtrl.GetItemState( i, LVIS_STATEIMAGEMASK ) == INDEXTOSTATEIMAGEMASK( LVIS_SELECTED ) )
			{
				CManifestMap	*pManifestMap = ( CManifestMap * )m_CheckinListCtrl.GetItemData( i );

				if ( pManifestMap == NULL )
				{
					ppFileNames[ nFileCount ] = pManifest->GetPathName();
					pManifest->SaveVMFManifest( pManifest->GetPathName() );
				}
				else
				{
					ppFileNames[ nFileCount ] = pManifestMap->m_AbsoluteMapFileName;
					pManifestMap->m_Map->SaveVMF( pManifestMap->m_AbsoluteMapFileName, 0 );
				}
				nFileCount++;
			}
		}

		if ( p4->SubmitFiles( nFileCount, ppFileNames, Description ) == false )
		{
			char temp[ 2048 ];

			sprintf( temp, "Could not check in map(s): %s", p4->GetLastError() );
			AfxMessageBox( temp, MB_ICONHAND | MB_OK );

			return;
		}
	}

	OnOK();
}
