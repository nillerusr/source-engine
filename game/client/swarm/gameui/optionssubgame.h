//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_GAME_H
#define OPTIONS_SUB_GAME_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"

//-----------------------------------------------------------------------------
// Purpose: Game Settings, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubGame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( COptionsSubGame, vgui::Frame );

public:
	COptionsSubGame( vgui::Panel *parent, const char *name );
	~COptionsSubGame( void );

	virtual void OnCommand( const char *command );
	virtual void OnClose( void );

private:
};



#endif // OPTIONS_SUB_GAME_H
