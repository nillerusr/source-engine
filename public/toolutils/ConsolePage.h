//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CONSOLEPAGE_H
#define CONSOLEPAGE_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/consoledialog.h"


//-----------------------------------------------------------------------------
// Purpose: Game/dev console dialog
//-----------------------------------------------------------------------------
class CConsolePage : public vgui::CConsolePanel
{
	DECLARE_CLASS_SIMPLE( CConsolePage, vgui::CConsolePanel );

public:
	CConsolePage( Panel *parent, bool bStatusVersion );

private:
	MESSAGE_FUNC_CHARPTR( OnCommandSubmitted, "CommandSubmitted", command );

	// vgui overrides
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
};

#endif // CONSOLEPAGE_H
