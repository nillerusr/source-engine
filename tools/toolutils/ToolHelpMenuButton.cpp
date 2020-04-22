//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "toolutils/toolswitchmenubutton.h"
#include "vgui_controls/panel.h"
#include "toolutils/toolmenubutton.h"
#include "toolutils/enginetools_int.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/menu.h"
#include "vgui/ILocalize.h"
#include "toolframework/ienginetool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Menu to switch between tools
//-----------------------------------------------------------------------------
class CToolHelpMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CToolHelpMenuButton, CToolMenuButton );

public:
	CToolHelpMenuButton( char const *toolName, char const *helpBinding, vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget );
};


//-----------------------------------------------------------------------------
// Global function to create the help menu
//-----------------------------------------------------------------------------
CToolMenuButton* CreateToolHelpMenuButton( char const *toolName, char const *helpBinding, vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget )
{
	return new CToolHelpMenuButton( toolName, helpBinding, parent, panelName, text, pActionTarget );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolHelpMenuButton::CToolHelpMenuButton( char const *toolName, char const *helpBinding, vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget ) :
	BaseClass( parent, panelName, text, pActionTarget )
{
	wchar_t *fmt = g_pVGuiLocalize->Find( "ToolHelpShowHelp" );
	if ( fmt )
	{
		wchar_t desc[ 256 ];
		g_pVGuiLocalize->ConvertANSIToUnicode( toolName, desc, sizeof( desc ) );

		wchar_t buf[ 512 ];
		g_pVGuiLocalize->ConstructString( buf, sizeof( buf ), fmt, 1, desc );

		AddMenuItem( "help", buf, new KeyValues( "OnHelp" ), pActionTarget, NULL, helpBinding );
	}

	SetMenu(m_pMenu);
}
