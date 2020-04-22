//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMEPICKER_H
#define DMEPICKER_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/Frame.h"
#include "datamodel/dmehandle.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Panel;
	class TextEntry;
}


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct DmePickerInfo_t
{
	DmElementHandle_t m_hElement;
	const char *m_pChoiceString;
};


//-----------------------------------------------------------------------------
// Purpose: Main app window
//-----------------------------------------------------------------------------
class CDmePicker : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDmePicker, vgui::EditablePanel );

public:
	CDmePicker( vgui::Panel *pParent );
	~CDmePicker();

	// overridden frame functions
	virtual void Activate( const CUtlVector< DmePickerInfo_t >& vec );

	// Forward arrow keys to the list
	virtual void OnKeyCodePressed( vgui::KeyCode code );

	// Returns the selceted DmElement
	CDmElement *GetSelectedDme( );

private:
	void RefreshDmeList();

	MESSAGE_FUNC( OnTextChanged, "TextChanged" );

	vgui::TextEntry *m_pFilterList;
	vgui::ListPanel *m_pDmeBrowser;
	CUtlString m_Filter;

	friend class CDmePickerFrame;
};


//-----------------------------------------------------------------------------
// Purpose: Main app window
//-----------------------------------------------------------------------------
class CDmePickerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDmePickerFrame, vgui::Frame );

public:
	CDmePickerFrame( vgui::Panel *pParent, const char *pTitle );
	~CDmePickerFrame();

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

	// Purpose: Activate the dialog
	// the message "DmeSelected" will be sent if one was picked
	// Pass in a message to add as a subkey to the DmeSelected message
	void DoModal( const CUtlVector< DmePickerInfo_t >& vec, KeyValues *pContextKeyValues = NULL );

private:
	void CleanUpMessage();

	CDmePicker *m_pPicker;
	vgui::Button *m_pOpenButton;
	vgui::Button *m_pCancelButton;
	KeyValues *m_pContextKeyValues;
};


#endif // DMEPICKER_H
