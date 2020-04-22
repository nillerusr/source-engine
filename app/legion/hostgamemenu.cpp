//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The menu that allows a player to start a game  
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "menumanager.h"
#include "basemenu.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/textentry.h"
#include "vgui_controls/Button.h"
#include "tier1/KeyValues.h"
#include "networkmanager.h"
#include "legion.h"
#include "inetworkmessagelistener.h"
#include "networkmessages.h"
#include "tier2/tier2.h"


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
class CHostGameMenu : public CBaseMenu, public INetworkMessageListener
{
	DECLARE_CLASS_SIMPLE( CHostGameMenu, CBaseMenu );

public:
	CHostGameMenu( vgui::Panel *pParent, const char *pPanelName );
	virtual ~CHostGameMenu();

	// Called when a particular network message occurs
	virtual void OnNetworkMessage( NetworkMessageRoute_t route, INetworkMessage *pNetworkMessage );
	virtual void OnCommand( const char *pCommand );

	MESSAGE_FUNC( OnTextNewLine, "TextNewLine" );

private:
	vgui::ListPanel *m_pPlayerList;
	vgui::TextEntry *m_pServerIP;
	vgui::TextEntry *m_pServerName;
	vgui::TextEntry *m_pChatLog;
	vgui::TextEntry *m_pChatEntry;
	vgui::TextEntry *m_pPlayerName;
	vgui::Button *m_pStartGame;
};


//-----------------------------------------------------------------------------
// Hooks the menu into the menu system
//-----------------------------------------------------------------------------
REGISTER_MENU( "HostGameMenu", CHostGameMenu );


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
CHostGameMenu::CHostGameMenu( vgui::Panel *pParent, const char *pPanelName ) :
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

	m_pServerIP = new vgui::TextEntry( this, "ServerIP" );
	m_pServerName = new vgui::TextEntry( this, "ServerName" );

	m_pPlayerName = new vgui::TextEntry( this, "PlayerName" );
	m_pPlayerName->SetMultiline( false );

	m_pChatLog = new vgui::TextEntry( this, "ChatLog" );
	m_pChatLog->SetMultiline( true );
	m_pChatLog->SetVerticalScrollbar( true );

	m_pChatEntry = new vgui::TextEntry( this, "ChatEntry" );
	m_pChatEntry->AddActionSignalTarget( this );
	m_pChatEntry->SetMultiline( false );
    m_pChatEntry->SendNewLine( true );

	m_pStartGame = new vgui::Button( this, "StartGame", "Start Game", this );

	LoadControlSettings( "resource/hostgamemenu.res", "GAME" );

	m_pPlayerName->SetText( "Unnamed" );

	if ( !g_pNetworkManager->HostGame() )
	{
		m_pStartGame->SetEnabled( false );
		return;
	}

	m_pServerIP->SetText( g_pNetworkSystem->GetLocalAddress() );
	m_pServerName->SetText( g_pNetworkSystem->GetLocalHostName() );

	g_pNetworkManager->AddListener( NETWORK_MESSAGE_SERVER_TO_CLIENT, LEGION_NETMESSAGE_GROUP, CHAT_MESSAGE, this );
	g_pNetworkManager->AddListener( NETWORK_MESSAGE_CLIENT_TO_SERVER, LEGION_NETMESSAGE_GROUP, CHAT_MESSAGE, this );
}

CHostGameMenu::~CHostGameMenu()
{
	g_pNetworkManager->RemoveListener( NETWORK_MESSAGE_SERVER_TO_CLIENT, LEGION_NETMESSAGE_GROUP, CHAT_MESSAGE, this );
	g_pNetworkManager->RemoveListener( NETWORK_MESSAGE_CLIENT_TO_SERVER, LEGION_NETMESSAGE_GROUP, CHAT_MESSAGE, this );
}


//-----------------------------------------------------------------------------
// Called when a particular network message occurs
//-----------------------------------------------------------------------------
void CHostGameMenu::OnNetworkMessage( NetworkMessageRoute_t route, INetworkMessage *pNetworkMessage )
{
	if ( route == NETWORK_MESSAGE_SERVER_TO_CLIENT )
	{
		CNetworkMessage_Chat *pChatMsg = static_cast<CNetworkMessage_Chat*>( pNetworkMessage );
		m_pChatLog->InsertString( pChatMsg->m_Message.Get() );
		m_pChatLog->InsertChar( '\n' );
	}
	else
	{
		// If this message was received from an client, broadcast it to all other clients
		g_pNetworkManager->BroadcastServerToClientMessage( pNetworkMessage );
	}
}


//-----------------------------------------------------------------------------
// Called when the enter key is hit in the chat entry window
//-----------------------------------------------------------------------------
void CHostGameMenu::OnTextNewLine()
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
void CHostGameMenu::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "CancelHostGame" ) )
	{
		g_pNetworkManager->StopHostingGame();
		g_pMenuManager->PopMenu();
		return;
	}

	if ( !Q_stricmp( pCommand, "StartGame" ) )
	{
		IGameManager::StartNewLevel();
		g_pMenuManager->PopAllMenus();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


