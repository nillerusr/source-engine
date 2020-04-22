//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMESOURCEDCCFILEPANEL_H
#define DMESOURCEDCCFILEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class TextEntry;
}

class CDmeSourceDCCFile;


//-----------------------------------------------------------------------------
// Purpose: Asset builder
//-----------------------------------------------------------------------------
class CDmeSourceDCCFilePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDmeSourceDCCFilePanel, EditablePanel );

public:
	CDmeSourceDCCFilePanel( vgui::Panel *pParent, const char *pPanelName );
	virtual ~CDmeSourceDCCFilePanel();

	// Inherited from Panel
	virtual void OnCommand( const char *pCommand );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

	void SetDmeElement( CDmeSourceDCCFile *pSourceDCCFile );

	/*
	messages sent:
		"DmeElementChanged"	The element has been changed
	*/

private:
	MESSAGE_FUNC_PARAMS( OnTextNewLine, "TextNewLine", kv );
	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", kv );
	MESSAGE_FUNC_PARAMS( OnItemSelected, "ItemSelected", kv );	
	MESSAGE_FUNC_PARAMS( OnItemDeselected, "ItemDeselected", kv );	

	// Shows the DCC object browser (once we have one)
	void ShowDCCObjectBrowser( const char *pTitle, const char *pPrompt, KeyValues *pDialogKeys );

	// Called when we're browsing for a DCC object and one was selected
	void OnDCCObjectAdded( const char *pDCCObjectName, KeyValues *pContextKeys );

	// Refresh the source list
	void RefreshDCCObjectList( );

	// Called when the source file name changes
	bool CheckForDuplicateNames( const char *pDCCObjectName, int nDCCObjectSkipIndex = -1 );

	void OnBrowseDCCObject();
	void OnAddDCCObject();
	void OnRemoveDCCObject();
	void OnDCCObjectNameChanged();

	// Selects a particular DCC object
	void SelectDCCObject( int nDCCObjectIndex );

	// Called when a list panel's selection changes
	void OnItemSelectionChanged( );

	// Marks the file as dirty
	void SetDirty( );

	vgui::ListPanel *m_pRootDCCObjects;
	vgui::Button *m_pDCCObjectBrowser;
	vgui::Button *m_pAddDCCObject;
	vgui::Button *m_pRemoveDCCObject;
	vgui::Button *m_pApplyChanges;
	vgui::TextEntry *m_pDCCObjectName;

	CDmeHandle< CDmeSourceDCCFile > m_hSourceDCCFile;
};


#endif // DMESOURCEDCCFILEPANEL_H
