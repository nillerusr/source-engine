#include "cbase.h"
#include "asw_vgui_info_message.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/Button.h"
#include "asw_info_message_shared.h"
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ScrollBar.h>
#include "WrappedLabel.h"
#include <vgui/IInput.h>
#include "ImageButton.h"
#include "controller_focus.h"
#include <vgui_controls/ImagePanel.h>
#include "iclientmode.h"
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "c_asw_player.h"
#include "iinput.h"
#include "input.h"
#include "asw_input.h"
#include "clientmode_asw.h"
#include "c_user_message_register.h"
#include "asw_vgui_debug_panel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

vgui::DHANDLE< CASW_VGUI_Debug_Panel >	g_hCurrentDebugPanel;

CASW_VGUI_Debug_Panel::CASW_VGUI_Debug_Panel( vgui::Panel *pParent, const char *pElementName ) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel()
{	
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(0, "resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme( scheme );

	if ( g_hCurrentDebugPanel.Get() )
	{
		g_hCurrentDebugPanel->MarkForDeletion();
		g_hCurrentDebugPanel->SetVisible( false );
	}
	g_hCurrentDebugPanel = this;

	m_pDebugBar = new vgui::Panel( this, "DebugBar" );
	
	// load in debug panel keyvalues
	m_pDebugKV = new KeyValues( "DebugPanel" );
	if ( m_pDebugKV->LoadFromFile( g_pFullFileSystem, "resource/debug_panel.txt", "GAME" ) )
	{
		for ( KeyValues *pMenuKey = m_pDebugKV->GetFirstSubKey(); pMenuKey; pMenuKey = pMenuKey->GetNextKey() )
		{
			if ( !Q_stricmp( pMenuKey->GetName(), "Menu" ) )
			{
				ImageButton *pButton = new ImageButton( m_pDebugBar, pMenuKey->GetString( "ButtonText" ), pMenuKey->GetString( "ButtonText" ) );
				pButton->SetZPos( 2 );
				m_MenuButtons.AddToTail( pButton );
			}
		}
	}
	m_pCurrentMenu = NULL;
	m_pCurrentMenuKeys = NULL;
}

CASW_VGUI_Debug_Panel::~CASW_VGUI_Debug_Panel()
{
	if ( g_hCurrentDebugPanel.Get() == this )
	{
		g_hCurrentDebugPanel = NULL;
	}
	if ( m_pDebugKV )
	{
		m_pDebugKV->deleteThis();
	}
}

void CASW_VGUI_Debug_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	// this panel fills the screen, so we can capture mouse clicks anywhere in the view
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );

	// put the debug bar at the top
	int iDebugBarHeight = YRES( 11 );
	m_pDebugBar->SetBounds( 0, 0, ScreenWidth(), iDebugBarHeight );

	// layout each of the menu buttons
	int border = YRES( 4 );
	int cursor_x = ScreenWidth() - YRES( 3 );
	for ( int i = 0; i < m_MenuButtons.Count(); i++ )
	{
		m_MenuButtons[i]->m_pLabel->SetTextInset( YRES( 1 ), 0 );
		m_MenuButtons[i]->InvalidateLayout( true );
		int tx, ty;
		m_MenuButtons[i]->m_pLabel->GetContentSize( tx, ty );
		cursor_x -= tx;
		m_MenuButtons[i]->SetBounds( cursor_x, 0, tx, m_pDebugBar->GetTall() );
		cursor_x -= border;
	}

	if ( m_pCurrentMenu )
	{
		int cursor_y = 0;
		for ( int i = 0; i < m_MenuEntries.Count(); i++ )
		{
			m_MenuEntries[i]->SetPos( 0, cursor_y );
			m_MenuEntries[i]->InvalidateLayout( true );
			cursor_y += m_MenuEntries[i]->GetTall();
		}

		int iEntryWidth = YRES( 90 );

		m_pCurrentMenu->SetBounds( ScreenWidth() - ( iEntryWidth + border ), iDebugBarHeight, iEntryWidth + border, cursor_y );		
	}
}

void CASW_VGUI_Debug_Panel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pDebugBar->SetPaintBackgroundEnabled( true );
	m_pDebugBar->SetBgColor( pScheme->GetColor( "DarkBlueTrans", Color( 65, 74, 96, 64 ) ) );
	m_pDebugBar->SetBorder( pScheme->GetBorder( "ButtonBorder" ) );

	if ( m_pCurrentMenu )
	{
		m_pCurrentMenu->SetPaintBackgroundEnabled( true );
		m_pCurrentMenu->SetBgColor( pScheme->GetColor( "DarkBlueTrans", Color( 65, 74, 96, 64 ) ) );
		//m_pCurrentMenu->SetBorder( pScheme->GetBorder( "ButtonBorder" ) );
	}

	for (int i = 0; i < m_MenuButtons.Count(); i++ )
	{
		m_MenuButtons[i]->SetBorders( "none", "none" );
		m_MenuButtons[i]->SetFont( pScheme->GetFont( "MissionChooserFont", IsProportional() ) );
	}
}

void CASW_VGUI_Debug_Panel::OnThink()
{
	BaseClass::OnThink();
}

bool CASW_VGUI_Debug_Panel::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	for (int i = 0; i < m_MenuButtons.Count(); i++ )
	{
		if ( !m_MenuButtons[i]->IsCursorOver() )
			continue;

		char buffer[128];
		m_MenuButtons[i]->m_pLabel->GetText( buffer, sizeof( buffer ) );

		for ( KeyValues *pMenuKey = m_pDebugKV->GetFirstSubKey(); pMenuKey; pMenuKey = pMenuKey->GetNextKey() )
		{
			if ( !Q_stricmp( pMenuKey->GetName(), "Menu" ) && !Q_stricmp( pMenuKey->GetString( "ButtonText" ), buffer ) )
			{
				if ( bDown )
					return true;

				if ( m_pCurrentMenuKeys )		// if already have a menu open, then close it
				{
					for ( int k = 0; k < m_MenuEntries.Count(); k++ )
					{
						m_MenuEntries[k]->MarkForDeletion();
					}
					m_MenuEntries.Purge();
					m_pCurrentMenu->MarkForDeletion();
					m_pCurrentMenu->SetVisible( false );
					m_pCurrentMenu = NULL;
					if ( m_pCurrentMenuKeys == pMenuKey )
					{
						m_pCurrentMenuKeys = NULL;
						return true;
					}
				}

				m_pCurrentMenu = new vgui::Panel( this, buffer );
				m_pCurrentMenuKeys = pMenuKey;

				// add each entry
				for ( KeyValues *pEntryKey = pMenuKey->GetFirstSubKey(); pEntryKey; pEntryKey = pEntryKey->GetNextKey() )
				{
					if ( !Q_stricmp( pEntryKey->GetName(), "ENTRY" ) )
					{
						CASW_Debug_Panel_Entry *pEntry = new CASW_Debug_Panel_Entry( m_pCurrentMenu, "DebugEntry" );
						pEntry->LoadFromKeyValues( pEntryKey );
						m_MenuEntries.AddToTail( pEntry );
					}
				}
				InvalidateLayout( true, true );
				return true;
			}
		}
	}

	// TODO: check for clicking entries
	for ( int i = 0;i < m_MenuEntries.Count(); i++ )
	{
		if ( m_MenuEntries[i]->IsCursorOver() )
		{
			if ( m_MenuEntries[i]->MouseClick( x, y, bRightClick, bDown ) )
				return true;
		}
	}

	return false;	// always swallow clicks in our window
}

// =====================================================

CASW_Debug_Panel_Entry::CASW_Debug_Panel_Entry( vgui::Panel *pParent, const char *pElementName ) :	vgui::Panel( pParent, pElementName )
{
	m_pLabel = new vgui::Label( this, "Label", "" );
	m_pEntryKeys = NULL;
}
CASW_Debug_Panel_Entry::~CASW_Debug_Panel_Entry()
{
	
}

void CASW_Debug_Panel_Entry::PerformLayout()
{
	BaseClass::PerformLayout();

	int iEntryWidth = YRES( 90 );
	int iEntryHeight = YRES( 12 );
	int iBorder = YRES( 2 );

	// TODO: Taller if model panel, image, etc.

	SetSize( iEntryWidth, iEntryHeight );
	m_pLabel->SetBounds( iBorder, 0, iEntryWidth - iBorder * 2, iEntryHeight );
}

void CASW_Debug_Panel_Entry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pLabel->SetFont( pScheme->GetFont( "MissionChooserFont", IsProportional() ) );
	SetPaintBackgroundEnabled( false );
	SetBgColor( Color( 192,192,192,192 ) );
}
bool CASW_Debug_Panel_Entry::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	if ( bDown )
		return true;

	Msg( "Clicked %s\n", m_pEntryKeys->GetString( "Text" ) );
	if ( m_pEntryKeys->GetInt( "WaitForClick", 0 ) == 0 )
	{
		// TODO: Show which command was entered
		engine->ClientCmd( m_pEntryKeys->GetString( "Command" ) );
	}
	return true;
}

void CASW_Debug_Panel_Entry::LoadFromKeyValues( KeyValues *pEntryKey )
{
	m_pEntryKeys = pEntryKey;

	m_pLabel->SetText( m_pEntryKeys->GetString( "Text" ) );
	// TODO: Model panel, image, etc.
	InvalidateLayout();
	GetParent()->InvalidateLayout();
}

void CASW_Debug_Panel_Entry::OnThink()
{
	BaseClass::OnThink();

	if ( IsCursorOver() )
	{
		m_pLabel->SetFgColor( Color( 0, 0, 0, 255 ) );
		SetPaintBackgroundEnabled( true );
	}
	else
	{
		m_pLabel->SetFgColor( Color( 255, 255, 255, 255 ) );
		SetPaintBackgroundEnabled( false );
	}
}