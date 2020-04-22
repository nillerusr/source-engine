//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef COMMENTARYNODEBROWSERPANEL_H
#define COMMENTARYNODEBROWSERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/editablepanel.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CCommEditDoc;
class CDmeCommentaryNodeEntity;
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
class CCommentaryNodeBrowserPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CCommentaryNodeBrowserPanel, vgui::EditablePanel );

public:
	CCommentaryNodeBrowserPanel( CCommEditDoc *pDoc, vgui::Panel* pParent, const char *pName );   // standard constructor
	virtual ~CCommentaryNodeBrowserPanel();

// Inherited from Panel
	virtual void OnCommand( const char *pCommand );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

	// Methods related to updating the listpanel
	void UpdateEntityList();

	// Select a particular node
	void SelectNode( CDmeCommentaryNodeEntity *pNode );

private:
	// Messages handled
	MESSAGE_FUNC( OnDeleteEntities, "DeleteEntities" );
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

	// Shows the most recent selected object in properties window
	void OnProperties();

	CCommEditDoc *m_pDoc;
	vgui::ListPanel		*m_pEntities;
};


#endif // COMMENTARYNODEBROWSERPANEL_H
