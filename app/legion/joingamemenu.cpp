//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Menu responsible for allowing players to join a game  
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "menumanager.h"
#include "basemenu.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/textentry.h"
#include "vgui_controls/button.h"
#include "networkmessages.h"
#include "networkmanager.h"
#include "tier1/KeyValues.h"


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
class CJoinGameMenu : public CBaseMenu, public INetworkMessageListener
{
	DECLARE_CLASS_SIMPLE( CJoinGameMenu, CBaseMenu );

public:
	CJoinGameMenu( vgui::Panel *pParent, const char *pPanelName );
	virtual ~CJoinGameMenu();

	// Called when a particular network message occurs
	virtual void OnNetworkMessage( NetworkMessageRoute_t route, INetworkMessage *pNetworkMessage );
	virtual void OnCommand( const char *pCommand );

	MESSAGE_FUNC( OnTextNewLine, "TextNewLine" );

private:
	vgui::ListPanel *m_pPlayerList;
	vgui::TextEntry *m_pChatLog;
	vgui::TextEntry *m_pServerName;
	vgui::TextEntry *m_pServerPort;
	vgui::TextEntry *m_pChatEntry;
	vgui::TextEntry *m_pPlayerName;
	vgui::Button *m_pJoinGame;
	bool m_bJoiningGame;
};


//-----------------------------------------------------------------------------
// Hooks the menu into the menu manager
//-----------------------------------------------------------------------------
REGISTER_MENU( "JoinGameMenu", CJoinGameMenu );


//-----------------------------------------------------------------------------
// Sort by player name
//-----------------------------------------------------------------------------
static int __cdecl PlayerNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("player");
	const char *string2 = item2.kv->GetString("player");
	return stricmp( string1, string2 );
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CJoinGameMenu::CJoinGameMenu( vgui::Panel *pParent, const char *pPanelName ) :
	BaseClass( pParent, pPanelName )
{
	m_pPlayerList = new vgui::ListPanel( this, "PlayerList" );
 	m_pPlayerList->AddColumnHeader( 0, "color", "Color", 52, 0 );
	m_pPlayerList->AddColumnHeader( 1, "player", "Player Name", 128, 0 );
    m_pPlayerList->SetSelectIndividualCells( false );
	m_pPlayerList->SetEmptyListText( "No Players" );
 	m_pPlayerList->SetDragEnabled( false );
 	m_pPlayerList->AddActionSignalTarget( this );
	m_pPlayerList->SetSortFunc( 0, PlayerNameSortFunc );
	m_pPlayerList->SetSortFunc( 1, PlayerNameSortFunc );
	m_pPlayerList->SetSortColumn( 1 );

	m_pServerName = new vgui::TextEntry( this, "ServerName" );

	m_pServerPort = new vgui::TextEntry( this, "ServerPort" );
	char pInitialPort[16];
	Q_snprintf( pInitialPort, sizeof(pInitialPort), "%d", NETWORKSYSTEM_DEFAULT_SERVER_PORT );
	m_pServerPort->SetText( pInitialPort );

	m_pPlayerName = new vgui::TextEntry( this, "PlayerName" );
	m_pPlayerName->SetMultiline( false );

	m_pChatLog = new vgui::TextEntry( this, "ChatLog" );
	m_pChatLog->SetMultiline( true );
	m_pChatLog->SetVerticalScrollbar( true );

	m_pChatEntry = new vgui::TextEntry( this, "ChatEntry" );
	m_pChatEntry->AddActionSignalTarget( this );
	m_pChatEntry->SetMultiline( false );
    m_pChatEntry->SendNewLine( true );

	m_pJoinGame = new vgui::Button( this, "JoinGame", "Join Game", this );

	LoadControlSettings( "resource/joingamemenu.res", "GAME" ); 

	m_pPlayerName->SetText( "Unnamed" );
	m_pChatEntry->SetEnabled( false );

	if ( !g_pNetworkManager->StartClient() )
	{
		m_pJoinGame->SetEnabled( false );
		return;
	}

	g_pNetworkManager->AddListener( NETWORK_MESSAGE_SERVER_TO_CLIENT, LEGION_NETMESSAGE_GROUP, CHAT_MESSAGE, this );
}

CJoinGameMenu::~CJoinGameMenu()
{
	g_pNetworkManager->RemoveListener( NETWORK_MESSAGE_SERVER_TO_CLIENT, LEGION_NETMESSAGE_GROUP, CHAT_MESSAGE, this );
}


//-----------------------------------------------------------------------------
// Called when a particular network message occurs
//-----------------------------------------------------------------------------
void CJoinGameMenu::OnNetworkMessage( NetworkMessageRoute_t route, INetworkMessage *pNetworkMessage )
{
	CNetworkMessage_Chat *pChatMsg = static_cast<CNetworkMessage_Chat*>( pNetworkMessage );
	m_pChatLog->InsertString( pChatMsg->m_Message.Get() );
	m_pChatLog->InsertChar( '\n' );
}


//-----------------------------------------------------------------------------
// Called when the enter key is hit in the chat entry window
//-----------------------------------------------------------------------------
void CJoinGameMenu::OnTextNewLine()
{
	CNetworkMessage_Chat msg;
	 
	int nLen = m_pChatEntry->GetTextLength();
	if ( nLen > 0 )
	{
		char *pText = (char*)_alloca( (nLen+1) * sizeof(char) );
		m_pChatEntry->GetText( pText, nLen+1 );
		m_pChatEntry->SetText( "" );

		int nLenName = m_pPlayerName->GetTextLength();
		char *pName = (char*)_alloca( (nLenName+8) * sizeof(char) );
		if ( nLenName == 0 )
		{
			nLenName = 7;
			Q_strcpy( pName, "unnamed" );
		}
		else
		{
			m_pPlayerName->GetText( pName, nLenName+1 );
		}

		int nTotalLen = nLen + nLenName;
		msg.m_Message.SetLength( nTotalLen + 3 );
		Q_snprintf( msg.m_Message.Get(), nTotalLen+3, "[%s] %s", pName, pText );

		g_pNetworkManager->PostClientToServerMessage( &msg );
	}
}


//-----------------------------------------------------------------------------
// Called when the enter key is hit in the chat entry window
//-----------------------------------------------------------------------------
void CJoinGameMenu::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		g_pNetworkManager->ShutdownClient();
		g_pMenuManager->PopMenu();
		return;
	}

	if ( !Q_stricmp( pCommand, "JoinGame" ) )
	{
		if ( !m_bJoiningGame )
		{
			g_pNetworkManager->DisconnectClientFromServer();
			m_pChatEntry->SetEnabled( false );
			m_pChatEntry->SetText( "" );
			m_bJoiningGame = true;
			m_pJoinGame->SetText( "Join Game" );
		}
		else
		{
			int nLen = m_pServerName->GetTextLength();
			char *pServer = (char*)_alloca( (nLen+1) * sizeof(char) );
			m_pServerName->GetText( pServer, nLen+1 );

			char pPort[32];
			m_pServerPort->GetText( pPort, sizeof(pPort) );

			if ( g_pNetworkManager->ConnectClientToServer( pServer, atoi( pPort ) ) )
			{
				m_pChatEntry->SetEnabled( true );
				m_bJoiningGame = false;
				m_pJoinGame->SetText( "Leave Game" );
			}
		}
		return;
	}

	BaseClass::OnCommand( pCommand );
}


