//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a special dockable dialog bar that activates itself when
//			the mouse cursor moves over it. This enables stacking of the
//			bars with only a small portion of each visible.
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "HammerBar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


BEGIN_MESSAGE_MAP(CHammerBar, CDialogBar)
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Automagically bring this bar to the top when the mouse cursor passes
//			over it.
// Input  : pWnd - 
//			nHitTest - 
//			message - 
// Output : Always returns FALSE.
//-----------------------------------------------------------------------------
BOOL CHammerBar::OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message)
{
	if (APP()->IsActiveApp())
	{
		// The control bar window is actually our grandparent.
		CWnd *pwnd = GetParent();
		if (pwnd != NULL)
		{
			pwnd = pwnd->GetParent();
			if (pwnd != NULL)
			{
				pwnd->BringWindowToTop();
			}
		}
	}

	//this wasn't being called and fixes some minor cursor problems.
	return CWnd::OnSetCursor( pWnd, nHitTest, message );
}

//-----------------------------------------------------------------------------
// Purpose: called automatically on a resize.
//			calls the function to move the bar's controls around on a resize.
//			also calls the CWnd OnSize so it can do what it needs to.  
// Input  : passed in by windows
//			UINT nType
//			int cx - change in width
//			int cy - change in height
// Output : void
//-----------------------------------------------------------------------------
void CHammerBar::OnSize( UINT nType, int cx, int cy )
{
	CWnd::OnSize( nType, cx, cy );
	AdjustControls();
	
}

//-----------------------------------------------------------------------------
// Purpose: called by windows when the edges of the dialog are clicked
//			returns the size of the dialog
// Input  : passed automatically by windows
//			int nLength - amount the dialog edge has moved
//			DWORD dwMode - type of dialog/movement we are dealing with			
// Output : CSize - size of the dialog
//-----------------------------------------------------------------------------
CSize CHammerBar::CalcDynamicLayout(int nLength, DWORD dwMode)
{		
	CSize newSize;

    // If the bar is docked, use the default size
    if ( (dwMode & LM_VERTDOCK) || (dwMode & LM_HORZDOCK) )
    {	
		m_sizeDocked.cy = m_sizeFloating.cy;		
 		return m_sizeDocked;		
    }

	//if the bar is floating, use the current floating size
    if ( dwMode & LM_MRUWIDTH )
	{
		return m_sizeFloating;
	}
	
    // In all other cases, we are changing the length with a drag
    if ( dwMode & LM_LENGTHY )
	{
		//the bar is being resized in the y direction
		newSize = CSize( m_sizeFloating.cx, m_sizeFloating.cy = nLength );
	}
    else
	{
		//the bar is being resized in the x direction
		newSize = CSize( m_sizeFloating.cx = nLength, m_sizeFloating.cy);
	}    

	CString textTitle;
	GetWindowText( textTitle );	
	
	//it should not be possible that a bar with no name could be dynamic; this check is just to be safe
	if ( !textTitle.IsEmpty() )
	{
		//writing the new size of the bar to the registry
		textTitle = "FloatingBarSize\\" + textTitle;
		APP()->WriteProfileInt( textTitle, "floatX", newSize.cx );
		APP()->WriteProfileInt( textTitle, "floatY", newSize.cy );    
	}

	return newSize;
}

//-----------------------------------------------------------------------------
// Purpose: creates the CHammerBar.  
//			makes sure a bar created with this old function isn't dyanmic	
//			initializes size variables.
// Input  : CWnd *pParentWnd
//			UINT nIDTemplate - the ID of the dialog to be created
//			UINT nStyle - the type of dialog we are making
//			UINT nID - 
// Output : BOOL - TRUE on success
//-----------------------------------------------------------------------------
BOOL CHammerBar::Create( CWnd* pParentWnd, UINT nIDTemplate, UINT nStyle, UINT nID )
{
	//cannot have a dynamic bar with this Create function; must use the one that takes the window name
	UINT nStyleFixed = nStyle & ~CBRS_SIZE_DYNAMIC;

	if ( !CDialogBar::Create(pParentWnd,nIDTemplate,nStyleFixed,nID) )
        return FALSE;
	
	m_sizeFloating = m_sizeDocked = m_sizeDefault;
    return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: creates the CHammerBar.  
//			initializes size variables.
//			reads saved dimension information from registry
// Input  : CWnd *pParentWnd
//			UINT nIDTemplate - the ID of the dialog to be created
//			UINT nStyle - the type of dialog we are making
//			UINT nID - 
// Output : BOOL - TRUE on success
//-----------------------------------------------------------------------------
BOOL CHammerBar::Create( CWnd* pParentWnd, UINT nIDTemplate, UINT nStyle, UINT nID, char *pszName )
{
	UINT nStyleFixed = nStyle;
	if ( *pszName == 0 )
	{
		//did not give a title and cannot have a dynamic bar.  Routing back through old create
		return Create( pParentWnd, nIDTemplate, nStyle, nID );   
	}
	else
	{
		if ( !CDialogBar::Create(pParentWnd,nIDTemplate,nStyleFixed,nID) )
			return FALSE;

		SetWindowText( pszName );

		CString textTitle;
		textTitle = "FloatingBarSize\\" + CString(pszName);

		//read size from registry
		m_sizeFloating.cx = APP()->GetProfileInt( textTitle, "floatX", m_sizeDefault.cx );
		m_sizeFloating.cy = APP()->GetProfileInt( textTitle, "floatY", m_sizeDefault.cy );
		m_sizeDocked = m_sizeDefault;
	}
    return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: moves controls based on their placement information
//
// Input  : void			
//			
// Output : void
//-----------------------------------------------------------------------------
void CHammerBar::AdjustControls( void )
{
	CRect HammerBarPos;
	GetWindowRect( &HammerBarPos );
	int nHammerBarHeight = HammerBarPos.Height();
	int nHammerBarWidth  = HammerBarPos.Width();
	
	for( int iControl = 0; iControl < m_ControlList.Size(); iControl++ )
	{
		ControlInfo_t currentControl = m_ControlList[ iControl ];
		int nDialogID = currentControl.m_nIDDialogItem;		
		int nControlWidthDifference  = currentControl.m_nWidthBuffer;
		int nControlHeightDifference = currentControl.m_nHeightBuffer;
		DWORD dwPlacement = currentControl.m_dwPlacementFlag;

		CWnd* pControl = GetDlgItem( nDialogID );
		if ( pControl != NULL )
		{
			if ( dwPlacement & GROUP_BOX  )
			{
				pControl->SetWindowPos( NULL, 0, 0, nHammerBarWidth - nControlWidthDifference , 
					nHammerBarHeight - nControlHeightDifference, SWP_NOMOVE|SWP_NOZORDER );			
			}
			if ( dwPlacement & BOTTOM_JUSTIFY )
			{
				CRect controlPos;
				pControl->GetWindowRect( &controlPos );
				pControl->SetWindowPos( NULL, controlPos.left - HammerBarPos.left,
					HammerBarPos.Height() - currentControl.m_nPosY, 0, 0, SWP_NOSIZE|SWP_NOZORDER );
			}
			if ( dwPlacement & RIGHT_JUSTIFY )
			{
				CRect controlPos;
				pControl->GetWindowRect( &controlPos );
				pControl->SetWindowPos( NULL, HammerBarPos.Width() - currentControl.m_nPosX,
					controlPos.top - HammerBarPos.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER );
			}
		}

	}
}

//-----------------------------------------------------------------------------
// Purpose: adds controls to the CHammerBar
//			determines position and stretch settings based on default design /			
// Input  : int nIDTemplate - ID from .rc file of control to be added
//			DWORD dwPlacementFlag - placement information
//				GROUP_BOX - will be stretched to fit new size
//				BOTTOM_JUSTIFY - will move with the bottom of the dialog	
//				RIGHT_JUSTIFY - will move with the right side of the dialog//			
// Output : void
//-----------------------------------------------------------------------------
void CHammerBar::AddControl( int nIDTemplate, DWORD dwPlacementFlag )
{
	ControlInfo_t newControl;
	newControl.m_nIDDialogItem = nIDTemplate;
	newControl.m_dwPlacementFlag = dwPlacementFlag;

	CWnd *pControl = GetDlgItem( nIDTemplate );
	if ( pControl != NULL )
	{
		CRect controlPos, hammerBarPos;
		pControl->GetWindowRect( &controlPos );
		GetWindowRect( &hammerBarPos );

		newControl.m_nHeightBuffer	= m_sizeDefault.cy - controlPos.Height(); 
		newControl.m_nWidthBuffer	= m_sizeDefault.cx - controlPos.Width();
		newControl.m_nPosX			= m_sizeDefault.cx + hammerBarPos.left - controlPos.left;
		newControl.m_nPosY			= m_sizeDefault.cy + hammerBarPos.top - controlPos.top;
		
		m_ControlList.AddToTail( newControl );
	}
}

CHammerBar::~CHammerBar()
{
	m_ControlList.RemoveAll();
}