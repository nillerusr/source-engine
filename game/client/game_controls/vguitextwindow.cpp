//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "vguitextwindow.h"
#include <networkstringtabledefs.h>
#include <cdll_client_int.h>

#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <FileSystem.h>
#include <KeyValues.h>
#include <convar.h>
#include <vgui_controls/ImageList.h>

#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>

#include <game/client/iviewport.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
extern INetworkStringTable *g_pStringTableInfoPanel;

#define TEMP_HTML_FILE	"textwindow_temp.html"

#define MINI_MOTD_FADE_TIME 2.5f
#define MINI_MOTD_HOLD_TIME 5.0f;


CON_COMMAND( showinfo, "Shows a info panel: <type> <title> <message> [<command>]" )
{
	if ( !GetViewPortInterface() )
		return;
	
	if ( args.ArgC() < 4 )
		return;
		
	IViewPortPanel * panel = GetViewPortInterface()->FindPanelByName( PANEL_INFO );

	 if ( panel )
	 {
		 KeyValues *kv = new KeyValues("data");
		 kv->SetInt( "type", Q_atoi(args[ 1 ]) );
		 kv->SetString( "title", args[ 2 ] );
		 kv->SetString( "message", args[ 3 ] );

		 if ( args.ArgC() == 5 )
			 kv->SetString( "command", args[ 4 ] );

		 panel->SetData( kv );

		 GetViewPortInterface()->ShowPanel( panel, true );

		 kv->deleteThis();
	 }
	 else
	 {
		 Msg("Couldn't find info panel.\n" );
	 }
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextWindow::CTextWindow(IViewPort *pViewPort) : BaseClass(NULL, PANEL_INFO	)
{
	// initialize dialog
	m_pViewPort = pViewPort;

//	SetTitle("", true);

	m_szTitle[0] = '\0';
	m_szMessage[0] = '\0';
	m_szExitCommand[0] = '\0';
	
	// load the new scheme early!!
	SetScheme("ClientScheme");
	SetMoveable(false);
	SetSizeable(false);
	SetProportional(true);
	SetTitleBarVisible( false );

	m_pTextMessage = new TextEntry(this, "TextMessage");
#if defined( ENABLE_HTMLWINDOW )
	m_pHTMLMessage = new HTML(this,"HTMLMessage");;
#endif
	m_pTitleLabel  = new Label( this, "MessageTitle", "Message Title" );
	m_pOK		   = new Button(this, "ok", "#PropertyDialog_OK");

	m_pOK->SetCommand("okay");
	m_pTextMessage->SetMultiline( true );
	m_nContentType = TYPE_TEXT;
	m_iFadeStatus = FADE_STATUS_IN;

	m_bMiniMode = false;

	ListenForGameEvent( "game_newmap" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextWindow::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	if ( !m_bMiniMode )
	{
		LoadControlSettings("Resource/UI/TextWindow.res");
	}
	else
	{
		LoadControlSettings("Resource/UI/MiniMOTD.res");
	}

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextWindow::~CTextWindow()
{
	// remove temp file again
	g_pFullFileSystem->RemoveFile( TEMP_HTML_FILE, "DEFAULT_WRITE_PATH" );
}

void CTextWindow::Reset( void )
{
	Q_strcpy( m_szTitle, "This could be your Title." );
	Q_strcpy( m_szMessage, "Just for 10 Euros a week!" );
	m_szExitCommand[0] = 0;

	if ( m_bMiniMode )
	{
		m_iFadeStatus = FADE_STATUS_OFF;
		SetAlpha( 0 );
	}
	else
	{
		SetAlpha( 255 );
	}

	UpdateContents();
}

void CTextWindow::ShowText( const char *text)
{
	m_pTextMessage->SetVisible( true );
	m_pTextMessage->SetText( text );
	m_pTextMessage->GotoTextStart();
}

void CTextWindow::ShowURL( const char *URL)
{
#if defined( ENABLE_HTMLWINDOW )
	m_pHTMLMessage->SetVisible( true );
	m_pHTMLMessage->OpenURL( URL );
#endif
}

void CTextWindow::ShowIndex( const char *entry)
{
	const char *data = NULL;
	int length = 0;

	if ( NULL == g_pStringTableInfoPanel )
		return;

	int index = g_pStringTableInfoPanel->FindStringIndex( m_szMessage );
		
	if ( index != ::INVALID_STRING_INDEX )
		data = (const char *)g_pStringTableInfoPanel->GetStringUserData( index, &length );

	if ( !data || !data[0] )
		return; // nothing to show

	// is this a web URL ?
	if ( !Q_strncmp( data, "http://", 7 ) )
	{
		ShowURL( data );
		return;
	}

	// try to figure out if this is HTML or not
	if ( data[0] != '<' )
	{
		ShowText( data );
		return;
	}

	// data is a HTML, we have to write to a file and then load the file
	FileHandle_t hFile = g_pFullFileSystem->Open( TEMP_HTML_FILE, "wb", "DEFAULT_WRITE_PATH" );

	if ( hFile == FILESYSTEM_INVALID_HANDLE )
		return;

	g_pFullFileSystem->Write( data, length, hFile );
	g_pFullFileSystem->Close( hFile );

	if ( g_pFullFileSystem->Size( TEMP_HTML_FILE ) != (unsigned int)length )
		return; // something went wrong while writing

	ShowFile( TEMP_HTML_FILE );
}

void CTextWindow::ShowFile( const char *filename )
{
	if  ( Q_stristr( filename, ".htm" ) || Q_stristr( filename, ".html" ) )
	{
		// it's a local HTML file
		char localURL[ _MAX_PATH + 7 ];
		Q_strncpy( localURL, "file://", sizeof( localURL ) );
		
		char pPathData[ _MAX_PATH ];
		g_pFullFileSystem->GetLocalPath( filename, pPathData, sizeof(pPathData) );
		Q_strncat( localURL, pPathData, sizeof( localURL ), COPY_ALL_CHARACTERS );

		// force steam to dump a local copy
		filesystem->GetLocalCopy( pPathData );

		ShowURL( localURL );
	}
	else
	{
		// read from local text from file
		FileHandle_t f = g_pFullFileSystem->Open( m_szMessage, "rb", "GAME" );

		if ( !f )
			return;

		char buffer[2048];
			
		int size = MIN( g_pFullFileSystem->Size( f ), sizeof(buffer)-1 ); // just allow 2KB

		g_pFullFileSystem->Read( buffer, size, f );
		g_pFullFileSystem->Close( f );

		buffer[size]=0; //terminate string

		ShowText( buffer );
	}
}

void CTextWindow::Update( void )
{
	if ( IsVisible() == false )
		return;

	if ( m_bMiniMode )
	{
		if ( m_iFadeStatus == FADE_STATUS_IN )
		{
			float flDeltaTime = ( m_flNextFadeTime - gpGlobals->curtime );

			SetAlpha ( MAX( 0, RemapValClamped( flDeltaTime, MINI_MOTD_FADE_TIME, 0, -128, 255 ) ) );

			if ( flDeltaTime <= 0.0f )
			{
				m_iFadeStatus = FADE_STATUS_HOLD;
				m_flNextFadeTime = gpGlobals->curtime + MINI_MOTD_HOLD_TIME
			}
		}
		else if ( m_iFadeStatus == FADE_STATUS_HOLD )
		{
			float flDeltaTime = ( m_flNextFadeTime - gpGlobals->curtime );

			if ( flDeltaTime <= 0.0f )
			{
				m_iFadeStatus = FADE_STATUS_OUT;
				m_flNextFadeTime = gpGlobals->curtime + MINI_MOTD_FADE_TIME;
			}
		}
		else if ( m_iFadeStatus == FADE_STATUS_OUT )
		{
			float flDeltaTime = ( m_flNextFadeTime - gpGlobals->curtime );

			SetAlpha ( RemapValClamped( flDeltaTime, 0.0f, MINI_MOTD_FADE_TIME, 0, 255 ) );

			if ( flDeltaTime <= 0.0f )
			{
				m_iFadeStatus = FADE_STATUS_OFF;
				SetVisible( false );
			}
		}
	}
}

void CTextWindow::OnCommand( const char *command)
{
    if (!Q_strcmp(command, "okay"))
    {
		if ( m_szExitCommand[0] )
		{
			engine->ClientCmd( m_szExitCommand );
		}
		
		m_pViewPort->ShowPanel( this, false );
	}

	BaseClass::OnCommand(command);
}

void CTextWindow::SetData(KeyValues *data)
{
	if ( IsVisible() == true )
		return;

	SetData( data->GetInt( "type" ), data->GetString( "title"), data->GetString( "msg" ), data->GetString( "cmd" ) );
}

void CTextWindow::SetData( int type, const char *title, const char *message, const char *command )
{
	Q_strncpy(  m_szTitle, title, sizeof( m_szTitle ) );
	Q_strncpy(  m_szMessage, message, sizeof( m_szMessage ) );
	
	if ( command )
	{
		Q_strncpy( m_szExitCommand, command, sizeof(m_szExitCommand) );
	}
	else
	{
		m_szExitCommand[0]=0;
	}

	m_nContentType = type;

	UpdateContents();
}

void CTextWindow::UpdateContents( void )
{
	SetTitle( m_szTitle, false );

	if ( m_pTitleLabel )
	{
		m_pTitleLabel->SetText( m_szTitle );
	}

#if defined( ENABLE_HTMLWINDOW )
	if ( m_pHTMLMessage )
	{
		m_pHTMLMessage->SetVisible( false );
	}
#endif

	if ( m_pTextMessage )
	{
		m_pTextMessage->SetVisible( false );
	}

	if ( m_bMiniMode )
		return;

	if ( m_nContentType == TYPE_INDEX )
	{
		ShowIndex( m_szMessage );
	}
	else if ( m_nContentType == TYPE_URL )
	{
		ShowURL( m_szMessage );
	}
	else if ( m_nContentType == TYPE_FILE )
	{
		ShowFile( m_szMessage );
	}
	else if ( m_nContentType == TYPE_TEXT )
	{
		ShowText( m_szMessage );
	}
	else
	{
		DevMsg("CTextWindow::Update: unknown content type %i\n", m_nContentType );
	}
}

void CTextWindow::ShowPanel( bool bShow )
{
	if ( IsX360() )
	{
		// Say no to MOTD from Xbox 360 clients!
		bShow = false;
	}

	if ( BaseClass::IsVisible() == bShow )
		return;

	m_pViewPort->ShowBackGround( bShow );

	m_bMiniMode = false;
	if ( m_szMessage && m_szMessage[0] )
	{
		if ( !Q_strncmp( m_szMessage, "hostfile", 8 ) )
		{
			m_bMiniMode = true;
		}
	}

	if ( bShow )
	{
		InvalidateLayout( true, true );
	}

	UpdateContents();

	if ( bShow )
	{
		Activate();

		if ( !m_bMiniMode )
		{
			SetVisible( true );
			SetMouseInputEnabled( true );
			SetKeyBoardInputEnabled( true );
			SetAlpha( 255 );
		}
		else
		{
			m_iFadeStatus = FADE_STATUS_IN;
			m_flNextFadeTime = gpGlobals->curtime + MINI_MOTD_FADE_TIME;
			SetAlpha( 0 );

			SetMouseInputEnabled( false );
			SetKeyBoardInputEnabled( false );
		}
	}
	else
	{
		SetVisible( false );

		if ( !m_bMiniMode )
		{
			SetMouseInputEnabled( false );
		}
	}
}

void CTextWindow::FireGameEvent( IGameEvent *event )
{
	const char *name = event->GetName();
	if ( Q_strcmp( name, "game_newmap" ) == 0 )
	{
		m_bIgnoreMultipleShowRequests = false;
	}
}

bool CTextWindow::WantsBackgroundBlurred( void )
{
	return (!m_bMiniMode);
}
