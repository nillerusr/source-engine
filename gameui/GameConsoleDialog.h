//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef GAMECONSOLEDIALOG_H
#define GAMECONSOLEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/consoledialog.h"
#include <Color.h>
#include "utlvector.h"
#include "EngineInterface.h"
#include "vgui_controls/Frame.h"


//-----------------------------------------------------------------------------
// Purpose: Game/dev console dialog
//-----------------------------------------------------------------------------
class CGameConsoleDialog : public vgui::CConsoleDialog
{
	DECLARE_CLASS_SIMPLE( CGameConsoleDialog, vgui::CConsoleDialog );

public:
	CGameConsoleDialog();

private:
	MESSAGE_FUNC( OnClosedByHittingTilde, "ClosedByHittingTilde" );
	MESSAGE_FUNC_CHARPTR( OnCommandSubmitted, "CommandSubmitted", command );

	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnCommand( const char *command );
};


#endif // GAMECONSOLEDIALOG_H
