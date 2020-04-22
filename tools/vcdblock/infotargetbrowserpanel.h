//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef INFOTARGETBROWSERPANEL_H
#define INFOTARGETBROWSERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/editablepanel.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CVcdBlockDoc;
class CDmeVMFEntity;
namespace vgui
{
	class ComboBox;
	class Button;
	class TextEntry;
	class ListPanel;
	class CheckButton;
	class RadioButton;
}


//-----------------------------------------------------------------------------
// Panel that shows all entities in the level
//-----------------------------------------------------------------------------
class CInfoTargetBrowserPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CInfoTargetBrowserPanel, vgui::EditablePanel );

public:
	CInfoTargetBrowserPanel( CVcdBlockDoc *pDoc, vgui::Panel* pParent, const char *pName );   // standard constructor
	virtual ~CInfoTargetBrowserPanel();

	// Inherited from Panel
	virtual void OnCommand( const char *pCommand );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

	// Methods related to updating the listpanel
	void UpdateEntityList();
	void Refresh();

	// Select a particular node
	void SelectNode( CDmeVMFEntity *pNode );

private:
	// Messages handled
	MESSAGE_FUNC( OnDeleteEntities, "DeleteEntities" );
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

	// Shows the most recent selected object in properties window
	void OnProperties();

	CVcdBlockDoc *m_pDoc;
	vgui::ListPanel		*m_pEntities;
};


#endif // INFOTARGETBROWSERPANEL_H
