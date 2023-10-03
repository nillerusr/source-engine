//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Dialog for selecting stim music
//
// $NoKeywords: $
//===========================================================================//

#ifndef STIMMUSICSELECTDIALOG_H
#define STIMMUSICSELECTDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/FileOpenDialog.h"

class StimMusicSelectDialog : public vgui::FileOpenDialog
{
	DECLARE_CLASS_SIMPLE( StimMusicSelectDialog, vgui::FileOpenDialog );

public:
	// The context keyvalues are added to all messages sent by this dialog if they are specified
	StimMusicSelectDialog( Panel *parent, const char *title, vgui::FileOpenDialogType_t type, KeyValues *pContextKeyValues = 0 );
	
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );
};

#endif // STIMMUSICSELECTDIALOG_H
