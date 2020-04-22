//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef ANCHORMGR_H
#define ANCHORMGR_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"


enum ESimpleAnchor
{
	k_eSimpleAnchorAllSides,	
	k_eSimpleAnchorBottomRight,		// The whole control follows the bottom right.
	k_eSimpleAnchorStretchRight,	// Only grow in width.
	k_eSimpleAnchorRightSide,
	k_eSimpleAnchorBottomSide
};
enum EAnchorHorz
{
	k_eAnchorLeft,		// Anchor this side of the control to the left of its parent window.
	k_eAnchorRight		// Anchor this side of the control to the right of its parent window.
};
enum EAnchorVert
{
	k_eAnchorTop,		// Anchor this side of the control to the top of its parent window.
	k_eAnchorBottom		// Anchor this side of the control to the bottom of its parent window.
};

class CAnchorDef
{
public:
	friend class CAnchorMgr;
	
	// You can use the first constructor for simple cases.
	CAnchorDef() {}
	CAnchorDef( int dlgItemID, ESimpleAnchor eSimpleAnchor );
	CAnchorDef( HWND hWnd, ESimpleAnchor eSimpleAnchor );

	CAnchorDef( int dlgItemID, EAnchorHorz eLeftSide=k_eAnchorLeft, EAnchorVert eTopSide=k_eAnchorTop, EAnchorHorz eRightSide=k_eAnchorRight, EAnchorVert eBottomSide=k_eAnchorBottom );
	CAnchorDef( HWND hWnd, EAnchorHorz eLeftSide=k_eAnchorLeft, EAnchorVert eTopSide=k_eAnchorTop, EAnchorHorz eRightSide=k_eAnchorRight, EAnchorVert eBottomSide=k_eAnchorBottom );

	// Only one of hWnd or dlgItemID should be set.
	void Init( HWND hWnd, int dlgItemID, ESimpleAnchor eSimpleAnchor );
	void Init( HWND hWnd, int dlgItemID, EAnchorHorz eLeftSide, EAnchorVert eTopSide, EAnchorHorz eRightSide, EAnchorVert eBottomSide );

public:
	int m_DlgItemID;
	HWND m_hInputWnd;	// Either this or m_DlgItemID is set.
	EAnchorHorz m_AnchorLeft, m_AnchorRight;
	EAnchorVert m_AnchorTop, m_AnchorBottom;

private:
	int m_OriginalPos[4];	// left, top, right, bottom
	HWND m_hWnd;
};


//-----------------------------------------------------------------------------
// Purpose: This class helps a resizable window resize and move its controls
// whenever the window is resized. First, you call Init() to tell it how
// you want all the child windows anchored, then just call OnSize() whenever
// the parent window is sized.
//-----------------------------------------------------------------------------
class CAnchorMgr
{
public:
	CAnchorMgr();
	void Init( HWND hParentWnd, CAnchorMgr::CAnchorDef *pAnchors, int nAnchors );

	// Call this when the parent window's size changes and it'll resize all the subcontrols.
	void OnSize();
	
private:
	CUtlVector<CAnchorMgr::CAnchorDef> m_Anchors;	
	HWND m_hParentWnd;
	int m_OriginalParentSize[2];	// wide, tall
};


#endif // ANCHORMGR_H
