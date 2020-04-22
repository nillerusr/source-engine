//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  baseclientstate.cpp: implementation of the CBaseClientState class.
//
//===========================================================================//

#include "client_pch.h"
#include "limits.h"
#include "cl_pluginhelpers.h"

#include <inetchannel.h>

#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include <vgui/IPanel.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/AnimationController.h>
#include "vgui_baseui_interface.h"
#include "vgui_askconnectpanel.h"
#include "cmd.h"
#include "tier1/convar.h"
#include "baseclientstate.h"

extern CClientState	cl;

//-----------------------------------------------------------------------------
// Purpose: Displays the options menu
//-----------------------------------------------------------------------------
class CPluginMenu : public vgui::EditablePanel
{
private:
	DECLARE_CLASS_SIMPLE( CPluginMenu, vgui::EditablePanel );

public:
	CPluginMenu( vgui::Panel *parent );
	virtual ~CPluginMenu();

	void Show( KeyValues *kv );
	void OnCommand(const char *command);
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPluginMenu::CPluginMenu( vgui::Panel *parent ) : EditablePanel(parent, "PluginMenu" )
{
	LoadControlSettings("Resource/UI/PluginMenu.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPluginMenu::~CPluginMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Show the options menu after using key values to configure it
//-----------------------------------------------------------------------------
void CPluginMenu::Show( KeyValues *kv )
{
	vgui::Label *control = dynamic_cast<vgui::Label *>(FindChildByName("Text"));
	if (control)
	{
		control->SetText( kv->GetWString( "msg" ) );
	}


	int i = 0;
	// hide all the buttons
	for ( i = 0; i < GetChildCount(); i++ )
	{
		vgui::Button *button = dynamic_cast<vgui::Button *>(GetChild(i));
		if ( button )
		{
			button->SetVisible( false );
		}
	}

	i = 1;
	// now work out what buttons to display
	for ( KeyValues *pCur=kv->GetFirstTrueSubKey(); pCur; pCur=pCur->GetNextTrueSubKey(), i++ )
	{
		char controlName[64];
		Q_snprintf( controlName, sizeof(controlName), "option%i", i );
		vgui::Button *button = dynamic_cast<vgui::Button *>(FindChildByName(controlName,true));
		Assert( button );
		if ( button )
		{
			button->SetText( pCur->GetWString( "msg" ));
			button->SetCommand( pCur->GetString( "command" ));
			button->SetVisible( true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: when a button is pressed send that command back to the engine
//-----------------------------------------------------------------------------
void CPluginMenu::OnCommand( const char *command )
{
	Cbuf_AddText( command );
	Cbuf_AddText( "\n" );

	CallParentFunction( new KeyValues( "Command", "command", "close" ) );
}

//-----------------------------------------------------------------------------
// Purpose: Displays the gameui portion of plugin menus
//-----------------------------------------------------------------------------
class CPluginGameUIDialog : public vgui::Frame
{
private:
	DECLARE_CLASS_SIMPLE( CPluginGameUIDialog, vgui::Frame );

public:
	CPluginGameUIDialog();
	virtual ~CPluginGameUIDialog();

	virtual void Show( DIALOG_TYPE type, KeyValues *kv );

protected:
	void OnCommand( const char *cmd );

private:
	CPluginMenu *m_Menu;
	vgui::RichText *m_RichText;
	vgui::Label *m_Message;
	vgui::TextEntry *m_Entry;
	vgui::Label *m_EntryLabel;
	vgui::Button *m_CloseButton;
	char	m_szEntryCommand[ 255 ];
};

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CPluginGameUIDialog::CPluginGameUIDialog() : vgui::Frame( NULL, "Plugins" )
{
	// initialize dialog
	SetTitle( "Plugins", true );
	SetAlpha( 255 );

	SetScheme( "Tracker" );

	m_szEntryCommand[ 0 ] = 0;

	m_Menu = new CPluginMenu( this );
	m_RichText = new vgui::RichText( this, "Rich" );
	m_Message = new vgui::Label( this, "Label", "" );
	m_Entry = new vgui::TextEntry( this, "Entry" ); 
	m_EntryLabel = new vgui::Label( this, "EntryLabel", "" );
	m_CloseButton = new vgui::Button( this, "Close", "");

	LoadControlSettings("Resource/UI/Plugin.res");
	InvalidateLayout();	
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CPluginGameUIDialog::~CPluginGameUIDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: called when the close button is pressed
//-----------------------------------------------------------------------------
void CPluginGameUIDialog::OnCommand( const char *cmd )
{
	if ( !Q_stricmp( cmd, "close" ) )
	{
		if ( Q_strlen(m_szEntryCommand) > 0 )
		{
			// Check that we can add the two execution markers
			if ( !Cbuf_HasRoomForExecutionMarkers( 2 ) )
			{
				AssertMsg( false, "CPluginGameUIDialog::OnCommand called but there is no room for the execution markers. Ignoring command." );
				return;
			}

			char userCMD[ 512 ];
			char entryText[ 255 ];
			m_Entry->GetText( entryText, sizeof(entryText) );
			Q_snprintf( userCMD, sizeof(userCMD), "%s %s\n", m_szEntryCommand, entryText );
			
			// Only let them run commands marked with FCVAR_CLIENTCMD_CAN_EXECUTE.
			Cbuf_AddTextWithMarkers( eCmdExecutionMarker_Enable_FCVAR_CLIENTCMD_CAN_EXECUTE, userCMD, eCmdExecutionMarker_Disable_FCVAR_CLIENTCMD_CAN_EXECUTE );
		}
		Close();
		g_PluginManager->OnPanelClosed();
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows a precanned style of message in the GameUI
//-----------------------------------------------------------------------------
void CPluginGameUIDialog::Show( DIALOG_TYPE type, KeyValues *kv )
{
	m_Menu->SetVisible(false);
	m_RichText->SetVisible(false);
	m_Message->SetVisible(false);
	m_Entry->SetVisible(false);
	m_EntryLabel->SetVisible(false);
	m_szEntryCommand[ 0 ] = 0;
		
	SetTitle( kv->GetWString( "title" ), true );

	switch ( type )
	{
	case DIALOG_MENU: // a options menu
		m_Menu->Show( kv );
		m_Menu->SetVisible( true );
		break;
	case DIALOG_TEXT: // a richtext dialog
		m_RichText->SetText( kv->GetWString( "msg" ) );
		m_RichText->SetVisible( true );
		break;
	case DIALOG_MSG: // just a msg to the screen, don't display in the gameui
		SetVisible( false );
		return;
		break;
	case DIALOG_ENTRY:
		m_Entry->SetVisible( true );
		m_EntryLabel->SetVisible( true );
		m_EntryLabel->SetText( kv->GetWString( "msg" ) );
		Q_strncpy( m_szEntryCommand, kv->GetString( "command" ), sizeof(m_szEntryCommand) );
		m_CloseButton->SetText( "#GameUI_OK" );
		break;
	default:
		Msg( "Invalid menu type (%i)\n", type );
		break;
	}
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: the individual message snippets
//-----------------------------------------------------------------------------
class CMessage : public vgui::Label
{
private:
	DECLARE_CLASS_SIMPLE( CMessage, vgui::Label );
public:
	CMessage(vgui::Panel *parent, const char *panelName, const char *text);
	~CMessage();

	bool HasExtraPanel() { return m_bHasExtraPanel; }

protected:
	void ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	bool m_bHasExtraPanel;
};

CMessage::CMessage( vgui::Panel *parent, const char *panelName, const char *text ) : vgui::Label( parent, panelName, text )
{
	m_bHasExtraPanel = false;
}

CMessage::~CMessage()
{
}

void CMessage::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	vgui::HFont font = pScheme->GetFont( "PluginText", false );
	if ( font == vgui::INVALID_FONT )
	{
		font = pScheme->GetFont( "HudHintText", false );
	}
	SetFont(font);
	BaseClass::ApplySchemeSettings( pScheme );
}


//-----------------------------------------------------------------------------
// Purpose: the hud plugin message panel
//-----------------------------------------------------------------------------
class CPluginHudMessage : public vgui::Frame
{
private:
	DECLARE_CLASS_SIMPLE( CPluginHudMessage, vgui::Frame );

public:
	CPluginHudMessage( vgui::VPANEL parent );
	~CPluginHudMessage();

	void ShowMessage( const wchar_t *message, int time, Color clr, bool bHasExtraPanel );
	void StartHiding();
	void Hide();

protected:
	void ApplySchemeSettings( vgui::IScheme *pScheme );
	void OnTick();
	void OnSizeChanged( int newWide, int newTall );

private:
	enum { MESSAGE_X_INSET = 40, MAX_TEXT_LEN_PIXELS = 400  };

	CMessage *m_Message;
	vgui::ImagePanel *m_pExtraPanelIcon;
	bool m_bHidingControl;
	int m_iTargetH, m_iTargetW;
	Color m_fgColor;

	vgui::AnimationController *m_pAnimationController;
};

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CPluginHudMessage::CPluginHudMessage( vgui::VPANEL parent ) : vgui::Frame( NULL, "PluginHudMessage" ) 
{
	SetParent( parent );
	SetVisible( false );
	SetAlpha( 255 );
	SetMinimumSize( 10 , 10 );

	SetScheme( "ClientScheme" );
	SetMoveable(false);
	SetSizeable(false);
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetTitleBarVisible( false );

	m_pExtraPanelIcon = new vgui::ImagePanel( this, "ExtraPanelIcon" );
	m_pExtraPanelIcon->SetVisible( false );

	m_Message = new CMessage( this, "Msg", "");
	m_Message->SetVisible( false );

	m_pAnimationController = new vgui::AnimationController( NULL );
	m_pAnimationController->SetParent( parent );
	m_pAnimationController->SetScriptFile( parent, "scripts/plugin_animations.txt" );
	m_pAnimationController->SetProportional( false );

	vgui::ivgui()->AddTickSignal(GetVPanel());

	LoadControlSettings("Resource/UI/PluginHud.res");
	InvalidateLayout();	
	GetSize( m_iTargetW, m_iTargetH );
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CPluginHudMessage::~CPluginHudMessage()
{
}

//-----------------------------------------------------------------------------
// Purpose: set the see through color and rounded corners
//-----------------------------------------------------------------------------
void CPluginHudMessage::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	m_pExtraPanelIcon->SetImage( vgui::scheme()->GetImage( "plugin/message_waiting", true ) );
	BaseClass::ApplySchemeSettings( pScheme );
	SetBgColor( pScheme->GetColor( "Plugins.BgColor", pScheme->GetColor( "TransparentBlack", Color( 0, 0, 0, 192 ))) );
	SetPaintBackgroundType( 2 );

	m_Message->SetFgColor( m_fgColor );
	m_pExtraPanelIcon->SetVisible( !m_bHidingControl );
}

//-----------------------------------------------------------------------------
// Purpose: run the anim controller and hide the message label if the anim var says to
//-----------------------------------------------------------------------------
void CPluginHudMessage::OnTick()
{
	m_pAnimationController->UpdateAnimations( Sys_FloatTime() );
	BaseClass::OnTick();
}

//-----------------------------------------------------------------------------
// Purpose: get the label size to track
//-----------------------------------------------------------------------------
void CPluginHudMessage::OnSizeChanged( int newWide, int newTall )
{
	BaseClass::OnSizeChanged( newWide, newTall );
	int w, h;
	GetSize( w, h );
	m_Message->SetBounds( MESSAGE_X_INSET, 5, w - MESSAGE_X_INSET - 10, h - 10 );

}

//-----------------------------------------------------------------------------
// Purpose: start the shrinking anim for the control if it should be showed, the hide anim otherwsise
//-----------------------------------------------------------------------------
void CPluginHudMessage::StartHiding()
{
	if ( m_pExtraPanelIcon->IsVisible() )
	{
		m_pAnimationController->StartAnimationSequence( "PluginMessageSmall" ); 
	}
	else
	{
		Hide();
	}
}

//-----------------------------------------------------------------------------
// Purpose: starts the hide anim for the control
//-----------------------------------------------------------------------------
void CPluginHudMessage::Hide()
{
	m_pAnimationController->StartAnimationSequence( "PluginMessageHide" ); 
	m_pExtraPanelIcon->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: shows a text message on the hud
//-----------------------------------------------------------------------------
void CPluginHudMessage::ShowMessage( const wchar_t *text, int time, Color clr, bool bHasExtraPanel )
{
	m_Message->SetVisible( true );
	m_Message->SetBounds( MESSAGE_X_INSET, 5, m_iTargetW - MESSAGE_X_INSET - 10, m_iTargetH - 10 );
	m_Message->SetText( text );
	m_Message->SetFgColor( clr );
	m_fgColor = clr;
	m_bHidingControl = !bHasExtraPanel;

	if ( bHasExtraPanel )
	{
		m_pExtraPanelIcon->SetVisible( true );
	}

	m_pAnimationController->StartAnimationSequence( "PluginMessageShow" ); 

	SetVisible( true );
	InvalidateLayout();
	int textW, textH;
	m_Message->GetContentSize( textW, textH );
	
	textW = min( textW + MESSAGE_X_INSET + 10, (int)MAX_TEXT_LEN_PIXELS ); 
	SetSize( textW, m_iTargetH ); // the "small" animation event changes our size
}









//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CPluginUIManager::CPluginUIManager() : vgui::Panel( NULL, "PluginManager" )
{
	m_iCurPriority = INT_MAX;
	m_iMessageDisplayUntil = 0;
	m_iHudDisplayUntil = 0;
	m_bShutdown = false;

	m_pGameUIDialog = new CPluginGameUIDialog();
	Assert( m_pGameUIDialog );
	m_pGameUIDialog->SetParent( EngineVGui()->GetPanel( PANEL_GAMEUIDLL ) );

	m_pHudMessage = new CPluginHudMessage(EngineVGui()->GetPanel( PANEL_CLIENTDLL ));
	Assert( m_pHudMessage );

	vgui::ivgui()->AddTickSignal(GetVPanel());
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CPluginUIManager::~CPluginUIManager()
{
}

//-----------------------------------------------------------------------------
// Purpose: hides the two plugin dialogs at the appropriate times
//-----------------------------------------------------------------------------
void CPluginUIManager::OnTick()
{
	if ( m_bShutdown )
	{
		return;
	}

	if ( m_iMessageDisplayUntil != 0 && !EngineVGui()->IsGameUIVisible() && m_iMessageDisplayUntil < Sys_FloatTime() ) // check the GameUI large message
	{
		m_pGameUIDialog->SetVisible( false );
		m_pHudMessage->Hide();
		m_iMessageDisplayUntil = 0;
		m_iCurPriority = INT_MAX;
	}

	if ( m_iHudDisplayUntil != 0 && m_iHudDisplayUntil < Sys_FloatTime() ) // check the hud panel
	{
		m_pHudMessage->StartHiding();
		m_iHudDisplayUntil = 0;
	}
	
	BaseClass::OnTick();
}

//-----------------------------------------------------------------------------
// Purpose: shuts down the plugin UI
//-----------------------------------------------------------------------------
void CPluginUIManager::Shutdown()
{
	vgui::ivgui()->RemoveTickSignal(GetVPanel());
	m_pHudMessage->Hide();
	m_pGameUIDialog->SetVisible( false );
	MarkForDeletion();
	m_bShutdown = true;
}

//-----------------------------------------------------------------------------
// Purpose: shows a particular ui type and queues their lifetime 
//-----------------------------------------------------------------------------
void CPluginUIManager::Show( DIALOG_TYPE type, KeyValues *kv )
{
	// Check for the special DIALOG_ASKCONNECT command.
	if ( type == DIALOG_ASKCONNECT )
	{
		// Don't allow this prompt on QuickPlay servers
		if ( cl.IsClientConnectionViaMatchMaking() )
			return;
		
		// Do the askconnect dialog.
		float flDuration = kv->GetFloat( "time", 4.0f );
		const char *pIP = kv->GetString( "title", NULL );
		if ( !pIP )
		{
			DevMsg( "Ignoring DIALOG_ASKCONNECT message. No IP specified." );
			return;
		}
		
		ShowAskConnectPanel( pIP, flDuration );
		return;
	}
	
	int level = kv->GetInt( "level", INT_MAX );
	if ( level < m_iCurPriority )
	{
		m_iCurPriority = level;
	}
	else
	{
		DevMsg( "Ignoring message %s, %i < %i\n", kv->GetName(), level, m_iCurPriority );
		return;
	}

	if ( type != DIALOG_MSG )
	{
		m_iMessageDisplayUntil = Sys_FloatTime() + min(max( kv->GetInt( "time", 10 ),10),200);
	}
	else
	{
		m_iMessageDisplayUntil = Sys_FloatTime() + 10;
	}

	m_iHudDisplayUntil = Sys_FloatTime() + 10; // hud messages only get 10 seconds

	m_pGameUIDialog->Show( type, kv );
	Color clr( 255, 255, 255, 255 );
	if ( !kv->IsEmpty( "color" ) )
	{
		clr = kv->GetColor( "color" );
	}

	m_pHudMessage->ShowMessage( kv->GetWString( "title" ), 10, clr, type != DIALOG_MSG );

}

//-----------------------------------------------------------------------------
// Purpose: called when the gameui panel is closed
//-----------------------------------------------------------------------------
void CPluginUIManager::OnPanelClosed()
{
	m_iCurPriority = INT_MAX;	
	m_iHudDisplayUntil = 0;
	m_iMessageDisplayUntil = 0;
	m_pGameUIDialog->SetVisible( false );
	m_pHudMessage->Hide();
}

void CPluginUIManager::GetHudMessagePosition( int &x, int &y, int &wide, int &tall )
{
	if ( m_pHudMessage )
	{
		m_pHudMessage->GetBounds( x, y, wide, tall );
	}
	else
	{
		x = y = wide = tall = 0;
	}
}

CPluginUIManager *g_PluginManager = NULL;


//=============================================================================
//
// external interfaces
//
//=============================================================================
ConVar cl_showpluginmessages ( "cl_showpluginmessages", "1", FCVAR_ARCHIVE, "Allow plugins to display messages to you" );

void PluginHelpers_Menu( SVC_Menu *msg )
{
	if ( !msg->m_MenuKeyValues )
	{
		return;
	}

	if ( !cl_showpluginmessages.GetBool() )
	{
		return;
	}

	if ( !g_PluginManager )
	{
		g_PluginManager = new CPluginUIManager();
	}

	g_PluginManager->Show( msg->m_Type, msg->m_MenuKeyValues );
}
