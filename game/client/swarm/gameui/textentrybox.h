//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#ifndef TEXTENTRYBOX_H
#define TEXTENTRYBOX_H
#ifdef _WIN32
#pragma once
#endif

#include "KeyValues.h"
#include <vgui_controls/QueryBox.h>

class vgui::Frame;
class vgui::TextEntry;
class vgui::Panel;
class CCvarTextEntry;

//--------------------------------------------------------------------------------------------------------------
/**
 *  Popup dialog with a text entry, extending the QueryBox, which extends the MessageBox
 */
class CTextEntryBox : public vgui::QueryBox
{
public:
	CTextEntryBox(const char *title, const char *labelText, const char *entryText, bool isCvar, vgui::Panel *parent = NULL);

	virtual ~CTextEntryBox();
 
	virtual void PerformLayout();						///< Layout override to position the label and text entry
	virtual void ShowWindow(vgui::Frame *pFrameOver);	///< Show window override to give focus to text entry

private:
	typedef vgui::QueryBox BaseClass;

protected:
	CCvarTextEntry	*m_pCvarEntry;
	vgui::TextEntry	*m_pEntry;

	virtual void OnKeyCodeTyped(vgui::KeyCode code);
	void OnCommand( const char *command);			///< Handle button presses
};

#endif // CVARTEXTENTRYBOX_H
