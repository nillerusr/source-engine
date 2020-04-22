//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef EDGE_SCROLLER_H
#define EDGE_SCROLLER_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"

namespace panorama
{

class CEdgeScrollBar;
class CUIEdgeScrollBar;
class CButton;

//-----------------------------------------------------------------------------
// Purpose: Control that shows buttons on the edges to scroll rather than a normal scroll bar
//-----------------------------------------------------------------------------
class CEdgeScroller : public CPanel2D
{
	DECLARE_PANEL2D( CEdgeScroller, CPanel2D );

public:
	CEdgeScroller( CPanel2D *pParent, const char *pchID );
	virtual ~CEdgeScroller();

	// Callback to client panel to create a scrollbar
	virtual IUIScrollBar *CreateNewVerticalScrollBar( float flInitialScrollPos ) OVERRIDE;
	virtual IUIScrollBar *CreateNewHorizontalScrollBar( float flInitialScrollPos ) OVERRIDE;
};


//-----------------------------------------------------------------------------
// Purpose: Scrollbar that just shows buttons on the edges of the panel rather than an actual Scrollbar
//-----------------------------------------------------------------------------
class CEdgeScrollBar : public CBaseScrollBar
{
	DECLARE_PANEL2D( CEdgeScrollBar, CBaseScrollBar );

public:
	CEdgeScrollBar( CPanel2D *pParent, const char *pchID, bool bHorizontal );
	virtual ~CEdgeScrollBar();

protected:
	virtual void UpdateLayout( bool bImmediateMove ) OVERRIDE;

private: 
	bool m_bHorizontal;

	CButton *m_pMinButton;
	CButton *m_pMaxButton;
};


} // namespace panorama

#endif // EDGE_SCROLLER_H
