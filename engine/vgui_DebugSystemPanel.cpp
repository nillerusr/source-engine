//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "client_pch.h"
#include "ivideomode.h"
#include "vgui_DebugSystemPanel.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/MenuButton.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuItem.h>
#include <vgui/Cursor.h>
#include <vgui_controls/TreeView.h>
#include <vgui_controls/ImageList.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/PropertyDialog.h>
#include <vgui_controls/PropertySheet.h>
#include "tier1/CommandBuffer.h"
#include "tier1/tier1.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: A menu button that knows how to parse cvar/command menu data from gamedir\scripts\debugmenu.txt
//-----------------------------------------------------------------------------
class CDebugMenuButton : public MenuButton
{
	typedef MenuButton BaseClass;

public:
	// Construction
	CDebugMenuButton( Panel *parent, const char *panelName, const char *text );

private:
	// Menu associated with this button
	Menu	*m_pMenu;
};

class CDebugCommandButton : public vgui::Button
{
typedef vgui::Button BaseClass;
public:
	CDebugCommandButton( vgui::Panel *parent, const char *panelName, const char *labelText, const char *command )
		: BaseClass( parent, panelName, labelText )
	{
		AddActionSignalTarget( this );
		SetCommand( command );
	}

	virtual void OnCommand( const char *command )
	{
		Cbuf_AddText( va( "%s\n", (char *)command ) );
	}

	virtual void OnTick( void )
	{
	}
};

class CDebugCommandCheckbox : public vgui::CheckButton
{
typedef vgui::CheckButton BaseClass;
public:
	CDebugCommandCheckbox( vgui::Panel *parent, const char *panelName, const char *labelText, const char *command )
		: BaseClass( parent, panelName, labelText )
	{
		m_pVar = ( ConVar * )g_pCVar->FindVar( command );
		SetCommand( command );
		AddActionSignalTarget( this );
	}

	virtual void OnCommand( const char *command )
	{
		if ( m_pVar )
		{
			Cbuf_AddText( va( "%s %d\n", m_pVar->GetName(), !m_pVar->GetInt() ) );
		}
	}

private:
	ConVar		*m_pVar;
};

class CDebugIncrementCVarButton : public vgui::Button
{
typedef vgui::Button BaseClass;
public:
	CDebugIncrementCVarButton( vgui::Panel *pParent, const char *pPanelName, const char *pLabelText, const char *pCommand )
		: BaseClass( pParent, pPanelName, pLabelText )
	{
		CCommand args;
		args.Tokenize( pCommand );

		m_pVar = NULL;
		if ( args.ArgC() >= 4 )
		{
			m_pVar = ( ConVar * )g_pCVar->FindVar( args[0] );

			m_flMinvalue = (float)atof( args[1] );
			m_flMaxvalue = (float)atof( args[2] );
			m_flIncrement = (float)atof( args[3] );
		}

		SetCommand( "increment" );
		AddActionSignalTarget( this );

		m_flPreviousValue = -9999.0f;

		OnTick();
	}

	virtual void OnCommand( const char *command )
	{
		//
		if ( !m_pVar )
			return;

		float curValue = m_pVar->GetFloat();
		curValue += m_flIncrement;
		if ( curValue > m_flMaxvalue )
		{
			curValue = m_flMinvalue;
		}
		else if ( curValue < m_flMinvalue )
		{
			curValue = m_flMaxvalue;
		}
		m_pVar->SetValue( curValue );
	}

	virtual void OnTick( void )
	{
		if ( !m_pVar )
			return;

		if ( m_pVar->GetFloat() == m_flPreviousValue )
			return;

		char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "%s %.2f", m_pVar->GetName(), m_pVar->GetFloat() );
		SetText( sz );
		SizeToContents();
		m_flPreviousValue = m_pVar->GetFloat();
	}

private:

	ConVar		*m_pVar;
	float		m_flMinvalue;
	float		m_flMaxvalue;
	float		m_flIncrement;

	float		m_flPreviousValue;

};

class CDebugOptionsPage : public vgui::PropertyPage
{
	typedef vgui::PropertyPage BaseClass;
public:
	CDebugOptionsPage ( vgui::Panel *parent, const char *panelName )
		: BaseClass( parent, panelName )
	{
		vgui::ivgui()->AddTickSignal( GetVPanel(), 250 );
	}

	virtual void OnTick( void )
	{
		BaseClass::OnTick();

		if ( !IsVisible() )
			return;

		int c = m_LayoutItems.Count();
		for ( int i = 0; i < c; i++ )
		{
			vgui::Panel *p = m_LayoutItems[ i ];
			p->OnTick();
		}
	}

	virtual void PerformLayout( void )
	{
		BaseClass::PerformLayout();

		int c = m_LayoutItems.Count();
		int x = 5;
		int y = 5;

		int w = 150;
		int h = 18;
		int gap = 2;

		int tall = GetTall();

		// LoadControlSettings( va( "resource\\%s.res", kv->GetName() ) );
		for ( int i = 0; i < c; i++ )
		{
			vgui::Panel *p = m_LayoutItems[ i ];
			p->SetBounds( x, y, w, h );

			y += ( h + gap );
			if ( y >= tall - h )
			{
				x += ( w + gap );
				y = 5;
			}
		}
	}

	void Init( KeyValues *kv )
	{
		// LoadControlSettings( va( "resource\\%s.res", kv->GetName() ) );
		for (KeyValues *control = kv->GetFirstSubKey(); control != NULL; control = control->GetNextKey())
		{
			const char *t;
			
			t = control->GetString( "command", "" );
			if ( t && t[0] )
			{
				CDebugCommandButton *btn = new CDebugCommandButton( this, "CommandButton", control->GetName(), t );
				m_LayoutItems.AddToTail( btn );
				continue;
			}
			t = control->GetString( "togglecvar", "" );
			if ( t && t[0] )
			{
				CDebugCommandCheckbox *checkbox = new CDebugCommandCheckbox( this, "CommandCheck", control->GetName(), t );
				m_LayoutItems.AddToTail( checkbox );
				continue;
			}
			t = control->GetString( "incrementcvar", "" );
			if ( t && t[0] )
			{
				CDebugIncrementCVarButton *increment = new CDebugIncrementCVarButton( this, "IncrementCVar", control->GetName(), t );
				m_LayoutItems.AddToTail( increment );
				continue;
			}
			
		}
	}

private:

	CUtlVector< vgui::Panel * >		m_LayoutItems;
};

class CDebugOptionsPanel : public vgui::PropertyDialog
{
	typedef vgui::PropertyDialog BaseClass;
public:

	CDebugOptionsPanel( vgui::Panel *parent, const char *panelName )
		: BaseClass( parent, panelName )
	{
		SetTitle( "Debug Options", true );

		KeyValues *kv = new KeyValues( "DebugOptions" );
		if ( kv )
		{
			if ( kv->LoadFromFile(g_pFullFileSystem, "scripts/DebugOptions.txt") )
			{
				for (KeyValues *dat = kv->GetFirstSubKey(); dat != NULL; dat = dat->GetNextKey())
				{
					if ( !Q_strcasecmp( dat->GetName(), "width" ) )
					{
						SetWide( dat->GetInt() );
						continue;
					}
					else if ( !Q_strcasecmp( dat->GetName(), "height" ) )
					{
						SetTall( dat->GetInt() );
						continue;
					}

					CDebugOptionsPage *page = new CDebugOptionsPage( this, dat->GetName() );
					page->Init( dat );
	
					AddPage( page, dat->GetName() );
				}
			}
			kv->deleteThis();
		}
	
		GetPropertySheet()->SetTabWidth(72);
		SetPos( videomode->GetModeStereoWidth() - GetWide() - 10 , 10 );
		SetVisible( true );

		if ( g_pFullFileSystem->FileExists( "resource/DebugOptionsPanel.res" ) )
		{
			LoadControlSettings( "resource/DebugOptionsPanel.res" );
		}
	}

	void	Init( KeyValues *kv );
};


void CDebugOptionsPanel::Init( KeyValues *kv )
{
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDebugMenuButton::CDebugMenuButton(Panel *parent, const char *panelName, const char *text)
	: BaseClass( parent, panelName, text )
{
	MakePopup();

	// Assume no menu
	m_pMenu = new Menu( this, "DebugMenu" );
	m_pMenu->AddMenuItem( "Debug Panel", "toggledebugpanel", parent );
	m_pMenu->AddMenuItem( "Quit", "Quit", parent );

	MenuButton::SetMenu(m_pMenu);
	SetOpenDirection(Menu::DOWN);
}

//-----------------------------------------------------------------------------
// Purpose: Container for menu button
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CDebugSystemPanel::CDebugSystemPanel( Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{

	SetBounds( 0, 0, videomode->GetModeStereoWidth(), videomode->GetModeStereoHeight() );

	// Show arrow cursor while in this mode
	SetCursor( vgui::dc_arrow );
	SetVisible( false );
	SetPaintEnabled( false );
	SetPaintBackgroundEnabled( false );

	m_pDebugMenu = new CDebugMenuButton( this, "Debug Menu", "Debug Menu" );
	
	int h = 24;
	// Locate it at top left
	m_pDebugMenu->SetPos( 0, 0 );
	m_pDebugMenu->SetSize( 110, h );

	m_hDebugOptions = new CDebugOptionsPanel( this, "DebugOptions" );
}

//-----------------------------------------------------------------------------
// Purpose: Hook so we can force cursor visible
// Input  : state - 
//-----------------------------------------------------------------------------
void CDebugSystemPanel::SetVisible( bool state )
{
	BaseClass::SetVisible( state );
	if ( state )
	{
		surface()->SetCursor( GetCursor() );
	}
}

void CDebugSystemPanel::OnCommand( const char *command )
{
	if ( !Q_strcasecmp( command, "toggledebugpanel" ) )
	{
		if ( m_hDebugOptions )
		{
			m_hDebugOptions->SetVisible( !m_hDebugOptions->IsVisible() );
		}
		return;
	}
	else if ( !Q_strcasecmp( command, "quit" ) )
	{
		Cbuf_AddText( "quit\n" );
	}

	BaseClass::OnCommand( command );
}