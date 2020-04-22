//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( VGUI_BASEPANEL_H )
#define VGUI_BASEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>

//-----------------------------------------------------------------------------
// Purpose: Base Panel for engine vgui panels ( can handle some drawing stuff )
//-----------------------------------------------------------------------------
abstract_class CBasePanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;

public:
					CBasePanel( vgui::Panel *parent, char const *panelName );
	virtual			~CBasePanel( void );

	// should this panel be drawn this frame?
	virtual bool	ShouldDraw( void ) = 0;
	virtual void	OnTick( void );

protected:
};


// Global version of the DrawColoredText function.
int DrawColoredText( vgui::HFont font, int x, int y, int r, int g, int b, int a, const wchar_t *text );
int DrawColoredText( vgui::HFont font, int x, int y, Color clr, const wchar_t *text );
int DrawCenteredColoredText( vgui::HFont font, int left, int top, int right, int bottom, Color clr, const wchar_t *text );
int DrawTextLen( vgui::HFont font, const wchar_t *text );


#endif // VGUI_BASEPANEL_H
