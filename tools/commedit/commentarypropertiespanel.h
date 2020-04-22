//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef COMMENTARYPROPERTIESPANEL_H
#define COMMENTARYPROPERTIESPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/editablepanel.h"
#include "tier1/utlstring.h"
#include "datamodel/dmehandle.h"


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
class CCommentaryPropertiesPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CCommentaryPropertiesPanel, vgui::EditablePanel );

public:
	CCommentaryPropertiesPanel( CCommEditDoc *pDoc, vgui::Panel* pParent );   // standard constructor

	// Inherited from Panel
	virtual void OnCommand( const char *pCommand );

	// Sets the object to look at
	void SetObject( CDmeCommentaryNodeEntity *pEntity );

private:
	// Populates the commentary node fields
	void PopulateCommentaryNodeFields();

	// Populates the info_target fields
	void PopulateInfoTargetFields();

	// Text to attribute...
	void TextEntryToAttribute( vgui::TextEntry *pEntry, const char *pAttributeName );
	void TextEntriesToVector( vgui::TextEntry *pEntry[3], const char *pAttributeName );

	// Updates entity state when text fields change
	void UpdateCommentaryNode();
	void UpdateInfoTarget();

	// Called when the audio picker button is selected
	void PickSound();

	// Called when sound recording is requested
	void RecordSound( );

	// Called when the audio picker button is selected
	void PickInfoTarget( vgui::TextEntry *pControl );

	// Messages handled
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", kv );
	MESSAGE_FUNC_PARAMS( OnSoundSelected, "SoundSelected", kv );
	MESSAGE_FUNC_PARAMS( OnPicked, "Picked", kv );
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", kv );
	MESSAGE_FUNC_PARAMS( OnSoundRecorded, "SoundRecorded", kv );

	CCommEditDoc *m_pDoc;

	vgui::EditablePanel *m_pCommentaryNodeScroll;
	vgui::EditablePanel *m_pInfoTargetScroll;
	vgui::EditablePanel *m_pCommentaryNode;
	vgui::EditablePanel *m_pInfoTarget;

	vgui::TextEntry *m_pNodeName;
	vgui::Button *m_pSoundFilePicker;
	vgui::TextEntry *m_pSoundFileName;
	vgui::Button *m_pRecordSound;
	vgui::TextEntry *m_pSpeakerName;
	vgui::TextEntry *m_pSynopsis;
	vgui::TextEntry *m_pViewPosition;
	vgui::Button *m_pViewPositionPicker;
	vgui::TextEntry *m_pViewTarget;
	vgui::Button *m_pViewTargetPicker;
	vgui::TextEntry *m_pStartCommands;
	vgui::TextEntry *m_pEndCommands;
	vgui::CheckButton *m_pPreventMovement;
	vgui::TextEntry *m_pPosition[3];
	vgui::TextEntry *m_pOrientation[3];

	vgui::TextEntry *m_pTargetName;
	vgui::TextEntry *m_pTargetPosition[3];
	vgui::TextEntry *m_pTargetOrientation[3];

	CDmeHandle< CDmeCommentaryNodeEntity > m_hEntity;
};


#endif // COMMENTARYPROPERTIESPANEL_H
