//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vgui_controls/ScrollBar.h"

#include "IEngineVGUI.h"
#include "EngineInterface.h"
#include "VGameLobby.h"
#include "VGameLobbyChat.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

static Color g_NameColor( 0, 151, 151, 255 );
static Color g_TextColor( 126, 126, 126, 255 );
static Color g_LeaderColor( 240, 240, 0, 255 );


//-----------------------------------------------------------------------------
// CGameLobbyChatEntry
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CGameLobbyChatEntry::CGameLobbyChatEntry( vgui::Panel *parent, char const *panelName, GameLobby *pLobby )
: BaseClass( parent, panelName )
{
	SetCatchEnterKey( true );
	SetAllowNonAsciiCharacters( true );
	SetDrawLanguageIDAtLeft( true );
	m_pGameLobby = pLobby;
}

//-----------------------------------------------------------------------------
void CGameLobbyChatEntry::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBorderEnabled( false );
}

void CGameLobbyChatEntry::OnKeyCodePressed( vgui::KeyCode code )
{
	switch( code )
	{
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_UP:
	case KEY_XBUTTON_UP:
		if ( GetParent() )
		{
			GetParent()->NavigateUp();
		}
		break;
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_DOWN:
	case KEY_XBUTTON_DOWN:
		if ( GetParent() )
		{
			GetParent()->NavigateDown();
		}
		break;

	default:
		{
			BaseClass::OnKeyCodePressed( code );
		}
	}
}

void CGameLobbyChatEntry::OnKeyTyped( wchar_t unichar )
{
	wchar_t defaultConsoleToggle = L'`';

	char const *pToggleConsoleBinding = engine->Key_LookupBinding( "toggleconsole" );
	if ( pToggleConsoleBinding )
	{
		defaultConsoleToggle = (wchar_t)*pToggleConsoleBinding;
	}

	if ( unichar != defaultConsoleToggle )
	{
		BaseClass::OnKeyTyped( unichar );
	}
}

//-----------------------------------------------------------------------------
void CGameLobbyChatEntry::OnKeyCodeTyped(vgui::KeyCode code)
{
	GameLobby* gameLobby = static_cast<GameLobby*>( CBaseModPanel::GetSingleton().GetWindow( WT_GAMELOBBY ) );
	if ( gameLobby )
	{
		gameLobby->NotifyLobbyNotIdleActivity();
	}

	bool ctrl = (input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL));

	if ( !ctrl )
	{
		switch ( code )
		{
		case KEY_ENTER:
		case KEY_PAD_ENTER:
		case KEY_ESCAPE:
			{
				if ( code != KEY_ESCAPE )
				{
					wchar_t wszText[1024] = {0};
					GetText( wszText, ARRAYSIZE( wszText ) );
					if ( wszText[0] )
					{
						IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
						if ( pIMatchSession )
						{
							KeyValues *pRequest = new KeyValues( "Game::Chat" );
							KeyValues::AutoDelete autodelete( pRequest );

							int iController = 0;
#ifdef _X360
							int iSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
							iController = XBX_GetUserId( iSlot );
#endif
							IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()
								->GetLocalPlayer( iController );
							XUID xuid = pPlayer->GetXUID();
							char const *szName = pPlayer->GetName();

							pRequest->SetString( "run", "all" );
							pRequest->SetUint64( "xuid", xuid );
							pRequest->SetString( "name", szName );
							pRequest->SetWString( "chat", wszText );


							pIMatchSession->Command( pRequest );
						}
					}
					RequestFocus();
				}
			
				// End message mode.
				if ( m_pGameLobby )
				{
					SetText( L"" );
				}
			}
			break;
		case KEY_TAB:
			if ( GetParent() )
			{
				if (input()->IsKeyDown(KEY_LSHIFT) || input()->IsKeyDown(KEY_RSHIFT))
				{
					GetParent()->NavigateUp();
				}
				else
				{
					GetParent()->NavigateDown();
				}
			}
			break;
		case KEY_UP:
			if ( GetParent() )
			{
				GetParent()->NavigateUp();
			}
			break;
		case KEY_DOWN:
			if ( GetParent() )
			{
				GetParent()->NavigateDown();
			}
			break;
		default:
			{
				BaseClass::OnKeyCodeTyped( code );
			}
		}
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}

//-----------------------------------------------------------------------------
// CGameLobbyChatInputLine
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CGameLobbyChatInputLine::CGameLobbyChatInputLine( GameLobby *parent, char const *panelName ) 
: BaseClass( parent, panelName ), m_pOnceNavUp( NULL ), m_pOnceNavDown( NULL ), m_pOnceNavFrom( NULL )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevguifuncs->GetPanel( PANEL_GAMEUIDLL ), "resource/ChatScheme.res", "ChatScheme");
	SetScheme(scheme);

	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );

	m_pPrompt = new vgui::Label( this, "ChatInputPrompt", "#L4D360UI_Lobby_ChatPrompt" );

	m_pInput = new CGameLobbyChatEntry( this, "ChatInput", parent );	
	m_pInput->SetMaximumCharCount( 127 );
}

//-----------------------------------------------------------------------------
void CGameLobbyChatInputLine::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	vgui::HFont hFont = pScheme->GetFont( "ChatFont", true );

	m_pPrompt->SetFont( hFont );
	m_pInput->SetFont( hFont );

	m_pInput->SetFgColor( Color( 126, 126, 126, 255 ) );
//	m_pInput->SetFgColor( pScheme->GetColor( "Chat.TypingText", pScheme->GetColor( "Panel.FgColor", Color( 255, 255, 255, 255 ) ) ) );

	SetPaintBackgroundEnabled( true );
	m_pPrompt->SetPaintBackgroundEnabled( true );
	m_pPrompt->SetContentAlignment( vgui::Label::a_west );
	m_pPrompt->SetTextInset( 2, 0 );

	m_pInput->SetMouseInputEnabled( true );

//	SetBgColor( Color( 32, 32, 32, 100 ) );

	StartChat();
}

void CGameLobbyChatInputLine::StartChat()
{
	ClearEntry();
	SetPrompt( "#L4D360UI_Lobby_ChatPrompt" );
	SetPaintBorderEnabled( true );
	SetMouseInputEnabled( true );

	GetInputPanel()->RequestFocus();
}

//-----------------------------------------------------------------------------
void CGameLobbyChatInputLine::SetPrompt( const wchar_t *prompt )
{
	Assert( m_pPrompt );
	m_pPrompt->SetText( prompt );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
void CGameLobbyChatInputLine::SetPrompt( const char *prompt )
{
	Assert( m_pPrompt );
	m_pPrompt->SetText( prompt );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
void CGameLobbyChatInputLine::ClearEntry( void )
{
	Assert( m_pInput );
	SetEntry( L"" );
}

//-----------------------------------------------------------------------------
void CGameLobbyChatInputLine::SetEntry( const wchar_t *entry )
{
	Assert( m_pInput );
	Assert( entry );

	m_pInput->SetText( entry );
	if ( entry && wcslen( entry ) > 0 )
	{
		m_pInput->GotoEndOfLine();
	}
}

//-----------------------------------------------------------------------------
void CGameLobbyChatInputLine::GetMessageText( wchar_t *buffer, int buffersizebytes )
{
	m_pInput->GetText( buffer, buffersizebytes);
}

//-----------------------------------------------------------------------------
void CGameLobbyChatInputLine::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide, tall;
	GetSize( wide, tall );

	int w,h;
	m_pPrompt->GetContentSize( w, h); 
	m_pPrompt->SetBounds( 0, 0, w, tall );

	m_pInput->SetBounds( w + 2, 0, wide - w - 2 , tall );
}

//-----------------------------------------------------------------------------
vgui::Panel *CGameLobbyChatInputLine::GetInputPanel( void )
{
	return m_pInput;
}

void CGameLobbyChatInputLine::SetOnceNavUp( Panel* navUp )
{
	m_pOnceNavUp = navUp;
}

void CGameLobbyChatInputLine::SetOnceNavDown( Panel* navDown )
{
	m_pOnceNavDown = navDown;
}

void CGameLobbyChatInputLine::SetOnceNavFrom( Panel* navFrom )
{
	m_pOnceNavFrom = navFrom;
}

Panel* CGameLobbyChatInputLine::NavigateUp()
{
	if( m_pOnceNavUp )
	{
		if( m_pOnceNavFrom )
		{
			m_pOnceNavFrom->NavigateFrom();
			m_pOnceNavFrom = NULL;
		}
		Panel *pRetVal = m_pOnceNavUp;
		m_pOnceNavUp = NULL;
		m_pOnceNavDown = NULL;
		NavigateFrom();
		pRetVal->NavigateTo();
		return pRetVal;
	}

	return BaseClass::NavigateUp();
}

Panel* CGameLobbyChatInputLine::NavigateDown()
{
	if( m_pOnceNavDown )
	{
		if( m_pOnceNavFrom )
		{
			m_pOnceNavFrom->NavigateFrom();
			m_pOnceNavFrom = NULL;
		}
		Panel *pRetVal = m_pOnceNavDown;
		m_pOnceNavUp = NULL;
		m_pOnceNavDown = NULL;
		NavigateFrom();
		pRetVal->NavigateTo();
		return pRetVal;
	}

	return BaseClass::NavigateDown();
}

//-----------------------------------------------------------------------------
// CGameLobbyChatHistory
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
CGameLobbyChatHistory::CGameLobbyChatHistory( vgui::Panel *pParent, const char *panelName )
 : BaseClass( pParent, panelName )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx( enginevguifuncs->GetPanel( PANEL_GAMEUIDLL ), "resource/ChatScheme.res", "ChatScheme");
	SetScheme(scheme);

	SetMaximumCharCount( 127 * 100 );

	memset( m_chLastAccessPrinted, 0, sizeof( m_chLastAccessPrinted ) );

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

//	InsertFade( -1, -1 );
}

CGameLobbyChatHistory::~CGameLobbyChatHistory()
{
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

//-----------------------------------------------------------------------------
void CGameLobbyChatHistory::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFont( pScheme->GetFont( "ChatFont" ) );
	SetAlpha( 255 );

 //   SetBgColor( Color( 32,32,32,100 ) );
	SetPaintBorderEnabled( false );
}

void CGameLobbyChatHistory::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "Command::Game::Chat", szEvent ) )
	{
		XUID xuid = pEvent->GetUint64( "xuid" );
		char const *szName = pEvent->GetString( "name" );
		szName = CUIGameData::Get()->GetPlayerName( xuid, szName );

		InsertString( "\n" );
		InsertColorChange( g_NameColor );
		InsertString( szName );
		InsertColorChange( g_TextColor );
		InsertString( ": " );
		InsertString( pEvent->GetWString( "chat" ) );
	}
	else if ( !Q_stricmp( "OnPlayerUpdated", szEvent ) )
	{
		if ( !Q_stricmp( "joined", pEvent->GetString( "state" ) ) )
		{
			// Note that the player has joined the lobby
			KeyValues *pPlayer = ( KeyValues * ) pEvent->GetPtr( "player" );
			NotifyPlayerChange( pPlayer, "#L4D360UI_Lobby_Player_Joined" );
		}
	}
	else if ( !Q_stricmp( "OnPlayerRemoved", szEvent ) )
	{
		KeyValues *pPlayer = pEvent->FindKey( "player" );
		NotifyPlayerChange( pPlayer, "#L4D360UI_Lobby_Player_Left" );
	}
	else if ( !Q_stricmp( "OnPlayerLeaderChanged", szEvent ) )
	{
		char const *szState = pEvent->GetString( "state", "" );
		bool bHost = !Q_stricmp( "host", szState );
		char const *szFmt = bHost ? "#L4D360UI_Lobby_Become_Leader_Self" : "#L4D360UI_Lobby_Become_Leader";
		if ( bHost )
		{
			// Squawk so you pay attention
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
		}

		if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
		{
			XUID xuidLeader = pMatchSession->GetSessionSystemData()->GetUint64( "xuidHost", 0ull );
			if ( !xuidLeader )
				xuidLeader = pMatchSession->GetSessionSettings()->GetUint64( "members/machine0/id", 0ull );
			
			KeyValues *pPlayerLobbyLeader = SessionMembersFindPlayer( pMatchSession->GetSessionSettings(), xuidLeader );

			NotifyPlayerChange( pPlayerLobbyLeader, szFmt );
		}
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		char const *szState = pEvent->GetString( "state" );
		if ( !Q_stricmp( szState, "updated" ) )
		{
			// Some properties of the session got updated
			// We care only of the properties that got updated
			if ( char const *szAccess = pEvent->GetString( "update/system/access", NULL ) )
			{
				if ( Q_stricmp( szAccess, m_chLastAccessPrinted ) )
				{
					InsertString( "\n" );
					InsertColorChange( g_LeaderColor );
					InsertString( CFmtStr( "#L4D360UI_Lobby_Access_Changed_%s", szAccess ) );
					Q_strncpy( m_chLastAccessPrinted, szAccess, sizeof( m_chLastAccessPrinted ) );
				}
			}
		}
	}
}

void CGameLobbyChatHistory::NotifyPlayerChange( KeyValues *pPlayer, char const *szFmtName )
{
	if ( !pPlayer || !szFmtName )
		return;
	
	wchar_t *fmt = g_pVGuiLocalize->Find( szFmtName );
	if ( !fmt )
		return;

	char const *szName = pPlayer->GetString( "name", "" );
	uint64 xuid = pPlayer->GetUint64( "xuid", 0ull );

	szName = CUIGameData::Get()->GetPlayerName( xuid, szName );

	wchar_t nameBuffer[MAX_PLAYER_NAME_LENGTH];
	g_pVGuiLocalize->ConvertANSIToUnicode( szName, nameBuffer, sizeof(nameBuffer) );

	wchar_t joinMsg[MAX_PATH];
	g_pVGuiLocalize->ConstructString( joinMsg, sizeof( joinMsg ), fmt, 1, nameBuffer );

	InsertString( "\n" );
	InsertColorChange( g_LeaderColor );
	InsertString( joinMsg );
}
