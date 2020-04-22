//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "client_pch.h"
#include "cl_demoeditorpanel.h"
#include "cl_demoactionmanager.h"
#include "cl_demoaction.h"
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Label.h>

#include <vgui_controls/Controls.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui/IVGui.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/MenuButton.h>
#include <vgui_controls/Menu.h>
#include "cl_demoactioneditors.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// So new actions can have sequential/unique names
static int g_nNewActionNumber = 1;

//-----------------------------------------------------------------------------
// Purpose: A menu button that knows how to parse cvar/command menu data from gamedir\scripts\debugmenu.txt
//-----------------------------------------------------------------------------
class CNewActionButton : public vgui::MenuButton
{
	typedef vgui::MenuButton BaseClass;

public:
	// Construction
	CNewActionButton( vgui::Panel *parent, const char *panelName, const char *text );

private:
	// Menu associated with this button
	Menu	*m_pMenu;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CNewActionButton::CNewActionButton(Panel *parent, const char *panelName, const char *text)
	: BaseClass( parent, panelName, text )
{
	// Assume no menu
	m_pMenu = new Menu( this, "DemoEditNewAction" );

	int count = NUM_DEMO_ACTIONS;
	int i;
	for ( i = 1 ; i < count; i++ )
	{
		char const *actionType = CBaseDemoAction::NameForType( (DEMOACTION)i );

		m_pMenu->AddMenuItem( actionType, actionType, parent );
		m_pMenu->SetItemEnabled( actionType, CBaseDemoAction::HasEditorFactory( (DEMOACTION)i ) );
	}
	
	m_pMenu->MakePopup();
	MenuButton::SetMenu(m_pMenu);
	SetOpenDirection(Menu::UP);
}

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CDemoEditorPanel::CDemoEditorPanel( vgui::Panel *parent ) : Frame( parent, "DemoEditorPanel")
{
	int w = 440;
	int h = 300;

	SetSize( w, h );

	SetTitle("Demo Editor", true);

	m_pSave = new vgui::Button( this, "DemoEditSave", "Save" );
	m_pRevert = new vgui::Button( this, "DemoEditRevert", "Revert" );;
	m_pOK = new vgui::Button( this, "DemoEditOk", "OK" );
	m_pCancel = new vgui::Button( this, "DemoEditCancel", "Cancel" );

	m_pNew = new CNewActionButton( this, "DemoEditNew", "New->" );
	m_pEdit = new vgui::Button( this, "DemoEditEdit", "Edit..." );
	m_pDelete = new vgui::Button( this, "DemoEditDelete", "Delete" );

	m_pCurrentDemo = new vgui::Label( this, "DemoName", "" );

	m_pActions = new vgui::ListPanel( this, "DemoActionList" );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	LoadControlSettings("Resource\\DemoEditorPanel.res");

	int xpos, ypos;
	parent->GetPos( xpos, ypos );
	ypos += parent->GetTall();

	SetPos( xpos, ypos );

	m_pActions->AddColumnHeader(0, "actionname", "Action", m_pActions->GetWide() / 3);
	m_pActions->AddColumnHeader(1, "actiontype", "Type", m_pActions->GetWide() / 3);
	m_pActions->AddColumnHeader(2, "actionstart", "Start", m_pActions->GetWide() / 3);
	OnRefresh();

	SetVisible( true );
	SetSizeable( false );
	SetMoveable( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDemoEditorPanel::~CDemoEditorPanel()
{
}

void CDemoEditorPanel::OnTick()
{
	BaseClass::OnTick();

	m_pCurrentDemo->SetText( demoaction->GetCurrentDemoFile() );
	bool hasdemo = demoaction->GetCurrentDemoFile()[0] ? true : false;

	if ( !hasdemo )
	{
		m_pNew->SetEnabled( false );
		m_pEdit->SetEnabled( false );
		m_pDelete->SetEnabled( false );
		m_pSave->SetEnabled( false );
		m_pRevert->SetEnabled( false );

	}
	else
	{
		m_pNew->SetEnabled( true );
		
		int count = demoaction->GetActionCount();

		m_pEdit->SetEnabled( count > 0 );
		m_pDelete->SetEnabled( count > 0 );

		if ( m_pActions && m_pActions->GetSelectedItemsCount() != 1 )
		{
			m_pEdit->SetEnabled( false );
			m_pDelete->SetEnabled( false );
		}

		m_pSave->SetEnabled( demoaction->IsDirty() );
		m_pRevert->SetEnabled( demoaction->IsDirty() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoEditorPanel::IsNewActionCommand( char const *command )
{
	DEMOACTION type = CBaseDemoAction::TypeForName( command );
	if ( type != DEMO_ACTION_UNKNOWN )
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actiontype - 
//-----------------------------------------------------------------------------
void CDemoEditorPanel::CreateNewAction( char const *actiontype )
{
	if ( m_hCurrentEditor != 0 )
		return;

	DEMOACTION type = CBaseDemoAction::TypeForName( actiontype );
	if ( type == DEMO_ACTION_UNKNOWN )
		return;
	
	CBaseDemoAction *action = CBaseDemoAction::CreateDemoAction( type );
	if ( action )
	{
		action->SetActionName( va( "Unnamed%i", g_nNewActionNumber++ ) );
		demoaction->SetDirty( true );

		m_hCurrentEditor = CBaseDemoAction::CreateActionEditor( action->GetType(), this, action, true );
		if ( m_hCurrentEditor != 0 )
		{
			m_hCurrentEditor->SetVisible( true );
			m_hCurrentEditor->SetSize( 400, 300 );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CDemoEditorPanel::OnCommand(const char *command)
{
	if ( !Q_strcasecmp( command, "edit" ) )
	{
		OnEdit();
	}
	else if ( !Q_strcasecmp( command, "delete" ) )
	{
		OnDelete();
	}
	else if ( !Q_strcasecmp( command, "save" ) )
	{
		OnSave();
	}
	else if ( !Q_strcasecmp( command, "Close" ) )
	{
		OnSave();
		MarkForDeletion();
		OnClose();
	}
	else if ( !Q_strcasecmp( command, "cancel" ) )
	{
		OnRevert();
		MarkForDeletion();
		OnClose();
	}
	else if ( !Q_strcasecmp( command, "revert" ) )
	{
		OnRevert();
	}
	else if ( IsNewActionCommand( command ) )
	{
		CreateNewAction( command );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CDemoEditorPanel::OnVDMChanged( void )
{
	OnRefresh();
}

void CDemoEditorPanel::PurgeActionList()
{
	if ( !m_pActions )
	{
		Assert( 0 );
		return;
	}

	m_pActions->DeleteAllItems();
}

void CDemoEditorPanel::PopulateActionList()
{
	PurgeActionList();

	int count = demoaction->GetActionCount();
	int i;
	for ( i = 0; i < count; i++ )
	{
		CBaseDemoAction *action = demoaction->GetAction( i );
		Assert( action );

		KeyValues *item = new KeyValues( "data", "actionname", action->GetActionName() );
		item->SetString( "actiontype", CBaseDemoAction::NameForType( action->GetType() ) );
		switch ( action->GetTimingType() )
		{
		default:
		case ACTION_USES_NEITHER:
			break;
		case ACTION_USES_TICK:
			{
				item->SetString( "actionstart", va( "Tick %i", action->GetStartTick() ) );
			}
			break;
		case ACTION_USES_TIME:
			{
				item->SetString( "actionstart", va( "Time %.3f", action->GetStartTime() ) );
			}
			break;
		}

		m_pActions->AddItem( item , 0, false, false);
	}
}

void CDemoEditorPanel::OnEdit()
{
	if ( m_hCurrentEditor != 0 )
		return;

	int numselected = m_pActions->GetSelectedItemsCount();
	if ( numselected != 1 )
		return;

	int row = m_pActions->GetSelectedItem( 0 );
	if ( row == -1 )
		return;

	CBaseDemoAction *action = demoaction->GetAction( row );

	m_hCurrentEditor = CBaseDemoAction::CreateActionEditor( action->GetType(), this, action, false );
	if ( m_hCurrentEditor != 0 )
	{
		m_hCurrentEditor->SetVisible( true );
		m_hCurrentEditor->SetSize( 400, 300 );
	}
	
	// edit it

//	demoaction->SetDirty( true );

//	PopulateActionList();
}

void CDemoEditorPanel::OnDelete()
{
	int numselected = m_pActions->GetSelectedItemsCount();
	if ( numselected < 1 )
		return;

	int i;
	for ( i = 0; i < numselected; i++ )
	{
		int row = m_pActions->GetSelectedItem(0);
		if ( row == -1 )
			continue;

		CBaseDemoAction *action = demoaction->GetAction( row );
		if ( action )
		{
			// This sets dirty bit
			demoaction->RemoveAction( action );
		}
	}

	OnRefresh();
}

void CDemoEditorPanel::OnSave()
{
	demoaction->SaveToFile();
}

void CDemoEditorPanel::OnRevert()
{
	demoaction->ReloadFromDisk();
	OnRefresh();
}

CBaseDemoAction *CDemoEditorPanel::FindActionByName( char const *name )
{
	int count = demoaction->GetActionCount();
	int i;
	for ( i = 0; i < count; i++ )
	{
		CBaseDemoAction *action = demoaction->GetAction( i );
		Assert( action );
		if ( !Q_strcasecmp( name, action->GetActionName() ) )
			return action;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoEditorPanel::OnRefresh()
{
	PopulateActionList();
}

