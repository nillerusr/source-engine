//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DIALOGADDSERVER_H
#define DIALOGADDSERVER_H
#ifdef _WIN32
#pragma once
#endif

class CAddServerGameList;
class IGameList;

//-----------------------------------------------------------------------------
// Purpose: Dialog which lets the user add a server by IP address
//-----------------------------------------------------------------------------
class CDialogAddServer : public vgui::Frame, public ISteamMatchmakingPingResponse
{
	DECLARE_CLASS_SIMPLE( CDialogAddServer, vgui::Frame );
	friend class CAddServerGameList;

public:
	CDialogAddServer(vgui::Panel *parent, IGameList *gameList);
	~CDialogAddServer();

	void ServerResponded( gameserveritem_t &server );
	void ServerFailedToRespond();

	void ApplySchemeSettings( vgui::IScheme *pScheme );

	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
private:
	virtual void OnCommand(const char *command);

	void OnOK();

	void TestServers();
	MESSAGE_FUNC( OnTextChanged, "TextChanged" );

	virtual void FinishAddServer( gameserveritem_t &pServer );
	virtual bool AllowInvalidIPs( void ) { return false; }

protected:
	IGameList *m_pGameList;
	
	vgui::Button *m_pTestServersButton;
	vgui::Button *m_pAddServerButton;
	vgui::Button *m_pAddSelectedServerButton;
	
	vgui::PropertySheet *m_pTabPanel;
	vgui::TextEntry *m_pTextEntry;
	vgui::ListPanel *m_pDiscoveredGames;
	int m_OriginalHeight;
	CUtlVector<gameserveritem_t> m_Servers;
	CUtlVector<HServerQuery> m_Queries;
};

class CDialogAddBlacklistedServer : public CDialogAddServer 
{
	DECLARE_CLASS_SIMPLE( CDialogAddBlacklistedServer, CDialogAddServer );
public:
	CDialogAddBlacklistedServer( vgui::Panel *parent, IGameList *gameList) :
		CDialogAddServer( parent, gameList )
	{
	}

	virtual void FinishAddServer( gameserveritem_t &pServer );
	void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool AllowInvalidIPs( void ) { return true; }
};

#endif // DIALOGADDSERVER_H
