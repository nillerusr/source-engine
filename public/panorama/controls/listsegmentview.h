//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef LIST_SEGMENT_VIEW_H
#define LIST_SEGMENT_VIEW_H

#ifdef _WIN32
#pragma once
#endif

#include "panel2d.h"

DECLARE_PANEL_EVENT0( ListSegmentViewRetreat );
DECLARE_PANEL_EVENT0( ListSegmentViewAdvance );

DECLARE_PANORAMA_EVENT0( ListSegmentViewChanged );


namespace panorama
{
	/*
	
	A class that handles displaying X sequential elements from its children.  Similar to a carousel, but more
	minimal -- you handle advancing/retreating yourself.  Supports up to 10 visible elements.

	Things you need to do:
	- Specify "items-displayed:" in the xml, which is how many elements are shown at once.
	- Specify "step-size:" in the xml, which is how many steps it'll move when advancing or retreating.  Cannot be bigger than items-displayed.
	- Define these classes, which will be added to the child elements (X is always 1 through 10):
	   .ListSegmentDisplayedX, one for each of the elements you intend to display.
	   .ListSegmentHiddenBeforeX, one for each hidden element that's off the "top/left" of the display.  These guys are prior to the displayed window.
	   .ListSegmentHiddenAfterX, one for each hidden element that's off the "bottom/right" of the display.  These guys are after the displayed window.
	   .ListSegmentSnap, which eliminates any transitions you're using in your element class, so that elements just go directly to their displayed/hidden states immediately.

	Then you just need to add children (in order) to it, and call AdvancePosition and RetreatPosition to move through them.

	*/

//-----------------------------------------------------------------------------
// Purpose: List Segment View
//-----------------------------------------------------------------------------
class CListSegmentView : public CPanel2D
{
	DECLARE_PANEL2D( CListSegmentView, CPanel2D );

public:
	CListSegmentView( CPanel2D *parent, const char * pchPanelID );
	virtual ~CListSegmentView();

	virtual bool BSetProperty( CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void GetDebugPropertyInfo( CUtlVector< DebugPropertyOutput_t *> *pvecProperties ) OVERRIDE;
	
	virtual void OnStylesChanged() OVERRIDE			{ UpdateChildrenStyles( false ); BaseClass::OnStylesChanged(); }
	virtual void OnAfterChildrenChanged() OVERRIDE	{ UpdateChildrenStyles( false ); BaseClass::OnAfterChildrenChanged(); }

	virtual bool BRequiresContentClipLayer() OVERRIDE { return true; }

	bool RetreatPosition( const CPanelPtr< IUIPanel > &panelPtr );
	bool AdvancePosition( const CPanelPtr< IUIPanel > &panelPtr );

	int GetCurrentPage( void );
	int GetNumPages( void );

	bool IsAtStart( void );
	bool IsAtEnd( void );

	void ResetPosition( void );

private:

	void UpdateChildrenStyles( bool bSnap );

	int m_nItemsDisplayed;
	int m_nStepSize;

	int m_nCurrentChildIndex;
};


} // namespace panorama

#endif // LIST_SEGMENT_VIEW_H
