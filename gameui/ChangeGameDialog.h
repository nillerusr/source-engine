//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CHANGEGAMEDIALOG_H
#define CHANGEGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

//-----------------------------------------------------------------------------
// Purpose: Dialogs for use to change current loaded mod
//-----------------------------------------------------------------------------
class CChangeGameDialog : public vgui::Frame
{
public:
	CChangeGameDialog(vgui::Panel *parent);
	~CChangeGameDialog();

	virtual void OnCommand( const char *command );

private:
	void LoadModList();

	vgui::ListPanel *m_pModList;

	typedef vgui::Frame BaseClass;
};


#endif // CHANGEGAMEDIALOG_H
