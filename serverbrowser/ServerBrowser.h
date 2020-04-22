//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERBROWSER_H
#define SERVERBROWSER_H
#ifdef _WIN32
#pragma once
#endif

class CServerBrowserDialog;

//-----------------------------------------------------------------------------
// Purpose: Handles the UI and pinging of a half-life game server list
//-----------------------------------------------------------------------------
class CServerBrowser : public IServerBrowser, public IVGuiModule
{
public:
	CServerBrowser();
	~CServerBrowser();

	// IVGui module implementation
	virtual bool Initialize(CreateInterfaceFn *factorylist, int numFactories);
	virtual bool PostInitialize(CreateInterfaceFn *modules, int factoryCount);
	virtual vgui::VPANEL GetPanel();
	virtual bool Activate();
	virtual bool IsValid();
	virtual void Shutdown();
	virtual void Deactivate();
	virtual void Reactivate();
	virtual void SetParent(vgui::VPANEL parent);

	// IServerBrowser implementation
	// joins a specified game - game info dialog will only be opened if the server is fully or passworded
	virtual bool JoinGame( uint32 unGameIP, uint16 usGamePort, const char *pszConnectCode );
	virtual bool JoinGame( uint64 ulSteamIDFriend, const char *pszConnectCode );

	// opens a game info dialog to watch the specified server; associated with the friend 'userName'
	virtual bool OpenGameInfoDialog( uint64 ulSteamIDFriend, const char *pszConnectCode );

	// forces the game info dialog closed
	virtual void CloseGameInfoDialog( uint64 ulSteamIDFriend );

	// closes all the game info dialogs
	virtual void CloseAllGameInfoDialogs();

	virtual const char *GetMapFriendlyNameAndGameType( const char *pszMapName, char *szFriendlyMapName, int cchFriendlyName ) OVERRIDE;

	// methods
	virtual void CreateDialog();
	virtual void Open();

	// true if the user can't play a game
	bool IsVACBannedFromGame( int nAppID );

	// Enable filtering of workshop maps, requires the game/tool loading us to feed subscription data. This is a
	// slightly ugly workaround to TF2 not yet having native workshop UI in quickplay, once that is in place this should
	// either be stripped back out or expanded to be directly aware of the steam workshop without being managed.
	virtual void SetWorkshopEnabled( bool bManaged ) OVERRIDE;
	virtual void AddWorkshopSubscribedMap( const char *pszMapName ) OVERRIDE;
	virtual void RemoveWorkshopSubscribedMap( const char *pszMapName ) OVERRIDE;

	bool IsWorkshopEnabled();
	bool IsWorkshopSubscribedMap( const char *pszMapName );
private:
	vgui::DHANDLE<CServerBrowserDialog> m_hInternetDlg;

	bool m_bWorkshopEnabled;
	CUtlVector< CUtlString > m_vecWorkshopSubscribedMaps;
};

// singleton accessor
CServerBrowser &ServerBrowser();

class CSteamAPIContext;
extern CSteamAPIContext *steamapicontext;


#endif // SERVERBROWSER_H
