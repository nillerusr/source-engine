//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CUSTOMTABEXPLANATIONDIALOG_H
#define CUSTOMTABEXPLANATIONDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "utlvector.h"
#include <vgui/KeyCode.h>
#include "vgui_controls/URLLabel.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog that explains the custom tab
//-----------------------------------------------------------------------------
class CCustomTabExplanationDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CCustomTabExplanationDialog, vgui::Frame );

public:
	CCustomTabExplanationDialog(vgui::Panel *parent);
	~CCustomTabExplanationDialog();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnCommand( const char *command );
	virtual void OnClose( void );
};

#endif // CUSTOMTABEXPLANATIONDIALOG_H
