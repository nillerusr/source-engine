//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CLIPVIEWPANEL_H
#define CLIPVIEWPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeDagEditPanel;
class CDmeDag;
class CDmeAnimationList;
class CDmeCombinationOperator;


//-----------------------------------------------------------------------------
// Material Viewer Panel
//-----------------------------------------------------------------------------
class CClipViewPanel : public vgui::Frame
{
	typedef vgui::Frame BaseClass;

public:
	// constructor, destructor
	CClipViewPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CClipViewPanel();

	// Overriden methods of vgui::Panel
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	// Sets the current scene + camera
	void SetScene( CDmeDag *pScene );
	void SetAnimationList( CDmeAnimationList *pAnimationList );
	void SetVertexAnimationList( CDmeAnimationList *pAnimationList );
	void SetCombinationOperator( CDmeCombinationOperator *pComboOp );
	void RefreshCombinationOperator();

	CDmeDag *GetScene( );

private:
	CDmeDagEditPanel *m_pClipViewPreview;
};



#endif // CLIPVIEWPANEL_H