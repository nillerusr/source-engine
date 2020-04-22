//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "AnchorMgr.h"


static int ProcessAnchorHorz( int originalCoord, int originalParentSize[2], EAnchorHorz eAnchor, int parentWidth, int parentHeight )
{
	if ( eAnchor == k_eAnchorLeft )
		return originalCoord;
	else
		return parentWidth - (originalParentSize[0] - originalCoord);
}

static int ProcessAnchorVert( int originalCoord, int originalParentSize[2], EAnchorVert eAnchor, int parentWidth, int parentHeight )
{
	if ( eAnchor == k_eAnchorTop )
		return originalCoord;
	else
		return parentHeight - (originalParentSize[1] - originalCoord);
}


CAnchorDef::CAnchorDef( int dlgItemID, ESimpleAnchor eSimpleAnchor )
{
	Init( NULL, dlgItemID, eSimpleAnchor );
}

CAnchorDef::CAnchorDef( int dlgItemID, EAnchorHorz eLeftSide, EAnchorVert eTopSide, EAnchorHorz eRightSide, EAnchorVert eBottomSide )
{
	Init( NULL, dlgItemID, eLeftSide, eTopSide, eRightSide, eBottomSide );
}

CAnchorDef::CAnchorDef( HWND hWnd, ESimpleAnchor eSimpleAnchor )
{
	Init( hWnd, -1, eSimpleAnchor );
}

CAnchorDef::CAnchorDef( HWND hWnd, EAnchorHorz eLeftSide, EAnchorVert eTopSide, EAnchorHorz eRightSide, EAnchorVert eBottomSide )
{
	Init( hWnd, -1, eLeftSide, eTopSide, eRightSide, eBottomSide );
}

void CAnchorDef::Init( HWND hWnd, int dlgItemID, ESimpleAnchor eSimpleAnchor )
{
	if ( eSimpleAnchor == k_eSimpleAnchorBottomRight )
	{
		Init( hWnd, dlgItemID, k_eAnchorRight, k_eAnchorBottom, k_eAnchorRight, k_eAnchorBottom );
	}
	else if ( eSimpleAnchor == k_eSimpleAnchorAllSides )
	{
		Init( hWnd, dlgItemID, k_eAnchorLeft, k_eAnchorTop, k_eAnchorRight, k_eAnchorBottom );
	}
	else if ( eSimpleAnchor == k_eSimpleAnchorStretchRight )
	{
		Init( hWnd, dlgItemID, k_eAnchorLeft, k_eAnchorTop, k_eAnchorRight, k_eAnchorTop );
	}
	else if ( eSimpleAnchor == k_eSimpleAnchorRightSide )
	{
		Init( hWnd, dlgItemID, k_eAnchorRight, k_eAnchorTop, k_eAnchorRight, k_eAnchorTop );
	}
	else if ( eSimpleAnchor == k_eSimpleAnchorBottomSide )
	{
		Init( hWnd, dlgItemID, k_eAnchorLeft, k_eAnchorBottom, k_eAnchorLeft, k_eAnchorBottom );
	}
}

void CAnchorDef::Init( HWND hWnd, int dlgItemID, EAnchorHorz eLeftSide, EAnchorVert eTopSide, EAnchorHorz eRightSide, EAnchorVert eBottomSide )
{
	m_hInputWnd = hWnd;
	m_DlgItemID = dlgItemID;
	m_AnchorLeft = eLeftSide;
	m_AnchorTop = eTopSide;
	m_AnchorRight = eRightSide;
	m_AnchorBottom = eBottomSide;
}


CAnchorMgr::CAnchorMgr()
{
}

void CAnchorMgr::Init( HWND hParentWnd, CAnchorDef *pAnchors, int nAnchors )
{
	m_Anchors.CopyArray( pAnchors, nAnchors );
	
	m_hParentWnd = hParentWnd;
	
	// Figure out the main window's size.
	RECT rcParent, rcItem;
	GetWindowRect( m_hParentWnd, &rcParent );
	m_OriginalParentSize[0] = rcParent.right - rcParent.left;
	m_OriginalParentSize[1] = rcParent.bottom - rcParent.top;

	// Get all the subitem positions.	
	for ( int i=0; i < m_Anchors.Count(); i++ )
	{
		CAnchorDef *pAnchor = &m_Anchors[i];

		if ( pAnchor->m_hInputWnd )
			pAnchor->m_hWnd = pAnchor->m_hInputWnd;
		else	
			pAnchor->m_hWnd = GetDlgItem( m_hParentWnd, pAnchor->m_DlgItemID );
			
		if ( !pAnchor->m_hWnd )
			continue;

		GetWindowRect( pAnchor->m_hWnd, &rcItem );
		POINT ptTopLeft;
		ptTopLeft.x = rcItem.left;
		ptTopLeft.y = rcItem.top;
		ScreenToClient( m_hParentWnd, &ptTopLeft );
		
		pAnchor->m_OriginalPos[0] = ptTopLeft.x;
		pAnchor->m_OriginalPos[1] = ptTopLeft.y;
		pAnchor->m_OriginalPos[2] = ptTopLeft.x + (rcItem.right - rcItem.left);
		pAnchor->m_OriginalPos[3] = ptTopLeft.y + (rcItem.bottom - rcItem.top);
	}
}

void CAnchorMgr::OnSize()
{
	// Get the new size.
	int width, height;
	RECT rcParent;
	GetWindowRect( m_hParentWnd, &rcParent );
	width = rcParent.right - rcParent.left;
	height = rcParent.bottom - rcParent.top;
	
	// Move each item.
	for ( int i=0; i < m_Anchors.Count(); i++ )
	{
		CAnchorDef *pAnchor = &m_Anchors[i];
		if ( !pAnchor->m_hWnd )
			continue;
	
		RECT rcNew;
		rcNew.left   = ProcessAnchorHorz( pAnchor->m_OriginalPos[0], m_OriginalParentSize, pAnchor->m_AnchorLeft, width, height );
		rcNew.right  = ProcessAnchorHorz( pAnchor->m_OriginalPos[2], m_OriginalParentSize, pAnchor->m_AnchorRight, width, height );
		rcNew.top    = ProcessAnchorVert( pAnchor->m_OriginalPos[1], m_OriginalParentSize, pAnchor->m_AnchorTop, width, height );
		rcNew.bottom = ProcessAnchorVert( pAnchor->m_OriginalPos[3], m_OriginalParentSize, pAnchor->m_AnchorBottom, width, height );
	
		SetWindowPos( pAnchor->m_hWnd, NULL, rcNew.left, rcNew.top, rcNew.right-rcNew.left, rcNew.bottom-rcNew.top, SWP_NOZORDER );
		InvalidateRect( pAnchor->m_hWnd, NULL, false );
	}
}

