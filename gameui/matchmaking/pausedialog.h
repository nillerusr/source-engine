//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Multiplayer pause menu
//
//=============================================================================//

#ifndef PAUSEDIALOG_H
#define PAUSEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"

//-----------------------------------------------------------------------------
// Purpose: Multiplayer pause menu
//-----------------------------------------------------------------------------
class CPauseDialog : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE( CPauseDialog, CBaseDialog ); 

public:
	CPauseDialog( vgui::Panel *parent );

	virtual void Activate( void );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
};


#endif // PAUSEDIALOG_H
