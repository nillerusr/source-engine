//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "hlfaceposer.h"
#include "mxExpressionTab.h"
#include "mdlviewer.h"
#include "expressions.h"

mxExpressionTab *g_pExpressionClass = 0;

//-----------------------------------------------------------------------------
// Purpose: Right click context menu
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void mxExpressionTab::ShowRightClickMenu( int mx, int my )
{
	if ( !g_MDLViewer )
		return;

	mxPopupMenu *pop = new mxPopupMenu();
	Assert( pop );

	pop->add( "New...", IDC_EXPRESSIONS_NEW );
	pop->addSeparator ();
	pop->add( "Load...", IDC_EXPRESSIONS_LOAD );
	pop->add( "Save", IDC_EXPRESSIONS_SAVE );
	pop->addSeparator ();
	pop->add( "Export to VFE", IDC_EXPRESSIONS_EXPORT );
	pop->addSeparator ();
	if ( m_nSelected != -1 )
	{
		pop->add( "Close class", IDC_EXPRESSIONS_CLOSE );
	}
	pop->add( "Close all classes", IDC_EXPRESSIONS_CLOSEALL );
	pop->addSeparator();
	pop->add( "Recreate all bitmaps", IDC_EXPRESSIONS_REDOBITMAPS );

	// Convert click position
	POINT pt;
	pt.x = mx;
	pt.y = my;
	ClientToScreen( (HWND)getHandle(), &pt );
	ScreenToClient( (HWND)g_MDLViewer->getHandle(), &pt );

	// Convert coordinate space
	pop->popup( g_MDLViewer, pt.x, pt.y );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int	
//-----------------------------------------------------------------------------
int	mxExpressionTab::getSelectedIndex () const
{
	// Convert based on override index
	return m_nSelected;
}
