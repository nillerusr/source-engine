//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef GAMEEVENTEDITPANEL_H
#define GAMEEVENTEDITPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/editablepanel.h"
#include "tier1/utlstring.h"
#include "datamodel/dmehandle.h"
#include "igameevents.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CGameEventEditDoc;

namespace vgui
{
	class ComboBox;
	class Button;
	class TextEntry;
	class ListPanel;
	class CheckButton;
	class RadioButton;
}

#define MAX_GAME_EVENT_PARAMS 20

extern IGameEventManager2 *gameeventmanager;

//-----------------------------------------------------------------------------
// Panel that shows all entities in the level
//-----------------------------------------------------------------------------
class CGameEventEditPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CGameEventEditPanel, vgui::EditablePanel );

public:
	CGameEventEditPanel( CGameEventEditDoc *pDoc, vgui::Panel* pParent );   // standard constructor
	~CGameEventEditPanel();

	// Inherited from Panel
	virtual void OnCommand( const char *pCommand );

private:
	// Text to attribute...
	//void TextEntryToAttribute( vgui::TextEntry *pEntry, const char *pAttributeName );
	//void TextEntriesToVector( vgui::TextEntry *pEntry[3], const char *pAttributeName );

	// Messages handled
	/*
	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", kv );
	MESSAGE_FUNC_PARAMS( OnSoundSelected, "SoundSelected", kv );
	MESSAGE_FUNC_PARAMS( OnPicked, "Picked", kv );
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", kv );
	MESSAGE_FUNC_PARAMS( OnSoundRecorded, "SoundRecorded", kv );*/

	//MESSAGE_FUNC( OnEventSend, "SendEvent" );

	MESSAGE_FUNC_PARAMS( OnTextChanged, "TextChanged", kv );

	void LoadEventsFromFile( const char *filename );

	CGameEventEditDoc *m_pDoc;

	// drop down of available events

	vgui::ComboBox *m_pEventCombo;

	vgui::Label *m_pParamLabels[MAX_GAME_EVENT_PARAMS];
	vgui::TextEntry *m_pParams[MAX_GAME_EVENT_PARAMS];

	vgui::Button *m_pSendEventButton;

	CUtlSymbolTable	m_EventFiles;	// list of all loaded event files
	CUtlVector<CUtlSymbol>	m_EventFileNames; 

	KeyValues *m_pEvents;

	vgui::TextEntry *m_pFilterBox;
};


#endif // GAMEEVENTEDITPANEL_H
