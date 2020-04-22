//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Standard edit menu for tools
//
//=============================================================================

#include "toolutils/tooleditmenubutton.h"
#include "toolutils/toolmenubutton.h"
#include "tier1/KeyValues.h"
#include "toolutils/enginetools_int.h"
#include "datamodel/idatamodel.h"
#include "vgui_controls/menu.h"
#include "vgui/ilocalize.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
// The Edit menu
//
//-----------------------------------------------------------------------------
class CToolEditMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CToolEditMenuButton, CToolMenuButton );
public:
	CToolEditMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu( vgui::Menu *menu );
};


//-----------------------------------------------------------------------------
// Global function to create the file menu
//-----------------------------------------------------------------------------
CToolMenuButton* CreateToolEditMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionTarget )
{
	return new CToolEditMenuButton( parent, panelName, text, pActionTarget );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolEditMenuButton::CToolEditMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	AddMenuItem( "undo", "#ToolEditUndo", new KeyValues ( "Command", "command", "OnUndo" ), pActionSignalTarget, NULL, "undo" );
	AddMenuItem( "redo", "#ToolEditRedo", new KeyValues ( "Command", "command", "OnRedo" ), pActionSignalTarget, NULL, "redo" );
	AddSeparator();
	AddMenuItem( "describe", "#ToolEditDescribeUndo", new KeyValues ( "Command", "command", "OnDescribeUndo" ), pActionSignalTarget);
	AddMenuItem( "wipeundo", "#ToolEditWipeUndo", new KeyValues ( "Command", "command", "OnWipeUndo" ), pActionSignalTarget);
	AddSeparator();
	AddMenuItem( "editkeybindings", "#BxEditKeyBindings", new KeyValues( "OnEditKeyBindings" ), pActionSignalTarget, NULL, "editkeybindings" );

	SetMenu(m_pMenu);
}

void CToolEditMenuButton::OnShowMenu( vgui::Menu *menu )
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	char sz[ 512 ];

	int id;
	id = m_Items.Find( "undo" );
	if ( g_pDataModel->CanUndo() )
	{
		m_pMenu->SetItemEnabled( id, true );
		
		wchar_t *fmt = g_pVGuiLocalize->Find( "ToolEditUndoStr" );
		if ( fmt )
		{
			wchar_t desc[ 256 ];
			g_pVGuiLocalize->ConvertANSIToUnicode( g_pDataModel->GetUndoDesc(), desc, sizeof( desc ) );

			wchar_t buf[ 512 ];
			g_pVGuiLocalize->ConstructString( buf, sizeof( buf ), fmt, 1, desc );

			m_pMenu->UpdateMenuItem( id, buf, new KeyValues( "Command", "command", "OnUndo" ) );
		}
		else
		{
			m_pMenu->UpdateMenuItem( id, "#ToolEditUndo", new KeyValues( "Command", "command", "OnUndo" ) );
		}
	}
	else
	{
		m_pMenu->SetItemEnabled( id, false );
		m_pMenu->UpdateMenuItem( id, "#ToolEditUndo", new KeyValues( "Command", "command", "OnUndo" ) );
	}

	id = m_Items.Find( "redo" );
	if ( g_pDataModel->CanRedo() )
	{
		m_pMenu->SetItemEnabled( id, true );

		wchar_t *fmt = g_pVGuiLocalize->Find( "ToolEditRedoStr" );
		if ( fmt )
		{
			wchar_t desc[ 256 ];
			g_pVGuiLocalize->ConvertANSIToUnicode( g_pDataModel->GetRedoDesc(), desc, sizeof( desc ) );

			wchar_t buf[ 512 ];
			g_pVGuiLocalize->ConstructString( buf, sizeof( buf ), fmt, 1, desc );

			m_pMenu->UpdateMenuItem( id, buf, new KeyValues( "Command", "command", "OnRedo" ) );
		}
		else
		{
			m_pMenu->UpdateMenuItem( id, sz, new KeyValues( "Command", "command", "OnRedo" ) );
		}
	}
	else
	{
		m_pMenu->SetItemEnabled( id, false );
		m_pMenu->UpdateMenuItem( id, "#ToolEditRedo", new KeyValues( "Command", "command", "OnRedo" ) );
	}

	id = m_Items.Find( "describe" );
	if ( g_pDataModel->CanUndo() )
	{
		m_pMenu->SetItemEnabled( id, true );
	}
	else
	{
		m_pMenu->SetItemEnabled( id, false );
	}
}
