//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DMEDAGEDITPANEL_H
#define DMEDAGEDITPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "vgui_controls/EditablePanel.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeDagRenderPanel;
class CDmeAnimationList;
class CDmeChannelsClip;
class CDmeSourceSkin;
class CDmeSourceAnimation;
class CDmeDCCMakefile;
class CDmeCombinationOperatorPanel;
class CDmeCombinationOperator;
class CDmeAnimationListPanel;
class CBaseAnimationSetEditor;
class CDmeAnimationSet;
class CDmeDag;

namespace vgui
{
	class Splitter;
	class PropertySheet;
	class PropertyPage;
	class ScrollableEditablePanel;
}



//-----------------------------------------------------------------------------
// Dag editor panel
//-----------------------------------------------------------------------------
class CDmeDagEditPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDmeDagEditPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CDmeDagEditPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmeDagEditPanel();

	// Overriden methods of vgui::Panel
	virtual void Paint();

	// Sets the current scene + animation list
	void SetDmeElement( CDmeDag *pScene );
	void SetAnimationList( CDmeAnimationList *pAnimationList );
	void SetVertexAnimationList( CDmeAnimationList *pAnimationList );
	void SetCombinationOperator( CDmeCombinationOperator *pComboOp );
	void RefreshCombinationOperator();

	CDmeDag *GetDmeElement();

	// Other methods which hook into DmePanel
	void SetDmeElement( CDmeSourceSkin *pSkin );
	void SetDmeElement( CDmeSourceAnimation *pAnimation );
	void SetDmeElement( CDmeDCCMakefile *pDCCMakefile );

private:
	// Called when the selection changes moves
	MESSAGE_FUNC( OnPageChanged, "PageChanged" );
	MESSAGE_FUNC_PARAMS( OnAnimationSelected, "AnimationSelected", kv );
	MESSAGE_FUNC_PARAMS( OnAnimationDeselected, "AnimationDeselected", kv );

	// Sets up the various panels in the dag editor
	void SetMakefileRootElement( CDmElement *pRoot );

	vgui::PropertySheet *m_pEditorSheet;
	vgui::PropertyPage *m_pAnimationPage;
	vgui::PropertyPage *m_pVertexAnimationPage;
	vgui::PropertyPage *m_pCombinationPage;
	vgui::Splitter *m_pPropertiesSplitter;
	CDmeDagRenderPanel *m_pDagRenderPanel;
	CDmeAnimationListPanel *m_pAnimationListPanel;
	CDmeAnimationListPanel *m_pVertexAnimationListPanel;
	CDmeCombinationOperatorPanel *m_pCombinationPanel;
};


#endif // DMEDAGEDITPANEL_H