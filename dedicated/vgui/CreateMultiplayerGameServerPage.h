//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CREATEMULTIPLAYERGAMESERVERPAGE_H
#define CREATEMULTIPLAYERGAMESERVERPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>

//-----------------------------------------------------------------------------
// Purpose: Data describing a single server
//-----------------------------------------------------------------------------
struct serveritem_t
{
	serveritem_t()
	{
		pings[0] = 0;
		pings[1] = 0;
		pings[2] = 0;
	}

	unsigned char ip[4];
	int port;
	int received;
	float time;
	int ping;			// current ping time, derived from pings[]
	int pings[3];		// last 3 ping times
	bool hadSuccessfulResponse;					// server has responded successfully in the past
	bool doNotRefresh;									// server is marked as not responding and should no longer be refreshed
	char gameDir[32];									// current game directory
	char map[32];											// current map
	char gameDescription[64];						// game description
	char name[64];											// server name
	int players;
	int maxPlayers;
	int botPlayers;
	bool proxy;
	bool password;
	bool secure;
	bool loadedFromFile;		// true if this entry was loaded from file rather than comming from the master
	unsigned int serverID;
	int listEntryID;
	char rconPassword[64];	// the rcon password for this server
};

//-----------------------------------------------------------------------------
// Purpose: server options page of the create game server dialog
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameServerPage : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CCreateMultiplayerGameServerPage, vgui::Frame ); 
public:
	CCreateMultiplayerGameServerPage(vgui::Panel *parent, const char *name);
	~CCreateMultiplayerGameServerPage();

	// returns currently entered information about the server
	int GetMaxPlayers()			{ return m_iMaxPlayers; }
	const char *GetPassword()	{ return m_szPassword; }
	const char *GetHostName()	{ return m_szHostName; }
	const char *GetMapName();
	const char *GetModName()	{ return m_szMod; }
	const char *GetGameName()	{ return m_szGameName; }
	void LoadMapList();
	int LoadMaps( const char *pszMod );

	virtual void OnCommand(const char *cmd);
	
	virtual void OnResetData(); 

	void GetServer(serveritem_t &s);
	const char *GetRconPassword();

private:
	enum { MAX_PLAYERS = 32 };
	enum { DATA_STR_LENGTH = 64 };

	void LoadConfig();
	void SaveConfig();
	void SetConfig(const char *serverName, const char *rconPassword, int maxPlayers, const char *map, const char *mod, int network, int secure, int port);

	bool LaunchOldDedicatedServer( KeyValues *pGameInfo );

	//void OnCommand(const char *text);
	void LoadMODList();
	void LoadModListInDirectory( const char *pDirectoryName );
	void LoadPossibleMod( const char *pGameDirName );
	void AddMod( const char *pGameDirName, const char *pGameInfoFilename, KeyValues *pGameInfo );

	bool BadRconChars(const char *pass);

	vgui::ComboBox *m_pGameCombo;
	vgui::ComboBox *m_pMapList;
	vgui::ComboBox *m_pNumPlayers;
	vgui::ComboBox *m_pNetworkCombo;
	vgui::Button *m_pStartServerButton;
	vgui::Button *m_pCancelButton;
	vgui::CheckButton *m_pSecureCheck;

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	
	serveritem_t m_iServer;

	char m_szHostName[DATA_STR_LENGTH];
	char m_szPassword[DATA_STR_LENGTH];
	char m_szMapName[DATA_STR_LENGTH];
	char m_szMod[DATA_STR_LENGTH];
	char m_szGameName[DATA_STR_LENGTH];
	char m_szExtra[DATA_STR_LENGTH*2];
	int m_iMaxPlayers;
	int m_iPort;

	vgui::Panel *m_MainPanel;
	KeyValues *m_pSavedData; // data to save away
	KeyValues *m_pGameInfo;
};


#endif // CREATEMULTIPLAYERGAMESERVERPAGE_H
