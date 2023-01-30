//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: interface to steam managing game server/client match making
//
//=============================================================================

#ifndef ISERVERSINFO_H
#define ISERVERSINFO_H
#ifdef _WIN32
#pragma once
#endif

#define MAX_GAME_DESCRIPTION 8192
#define MAX_SERVER_NAME 2048

enum NServerResponse
{
	nServerResponded = 0,
	nServerFailedToRespond,
	nNoServersListedOnMasterServer,
};

class newgameserver_t
{
public:
	newgameserver_t() = default;

	netadr_t m_NetAdr;								///< IP/Query Port/Connection Port for this server
	int m_nPing;											///< current ping time in milliseconds
	int m_nProtocolVersion;
	bool m_bHadSuccessfulResponse;	///< server has responded successfully in the past
	bool m_bDoNotRefresh;						///< server is marked as not responding and should no longer be refreshed
	char m_szGameDir[MAX_PATH];				 ///< current game directory
	char m_szMap[MAX_PATH];					///< current map
	char m_szGameTags[MAX_PATH];
	char m_szGameDescription[MAX_GAME_DESCRIPTION]; ///< game description

	int m_nPlayers;
	int m_nMaxPlayers;										///< Maximum players that can join this server
	int m_nBotPlayers;										///< Number of bots (i.e simulated players) on this server
	bool m_bPassword;										///< true if this server needs a password to join

	int m_iFlags;

	/// Game server name
	char m_szServerName[MAX_SERVER_NAME];
};

class IServerListResponse
{
public:
	// Server has responded ok with updated data
	virtual void ServerResponded( newgameserver_t &server ) = 0;
	virtual void RefreshComplete( NServerResponse response ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Callback interface for receiving responses after pinging an individual server 
//
class IServerPingResponse
{
public:
	// Server has responded successfully and has updated data
	virtual void ServerResponded( newgameserver_t &server ) = 0;
};
//-----------------------------------------------------------------------------
// Purpose: Callback interface for receiving responses after requesting details on
// who is playing on a particular server.
//
class IServerPlayersResponse
{
public:
	// Got data on a new player on the server -- you'll get this callback once per player
	// on the server which you have requested player data on.
	virtual void AddPlayerToList( const char *pchName, int nScore, float flTimePlayed ) = 0;

	// The server failed to respond to the request for player details
	virtual void PlayersFailedToRespond() = 0;

	// The server has finished responding to the player details request
	virtual void PlayersRefreshComplete() = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Functions for match making services for clients to get to game lists and details
//-----------------------------------------------------------------------------
class IServersInfo
{
public:
	virtual void RequestInternetServerList( const char *gamedir, IServerListResponse *response ) = 0;
	virtual void RequestLANServerList( const char *gamedir, IServerListResponse *response ) = 0;

	//virtual HServerQuery PingServer( uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse *pRequestServersResponse ) = 0; 
	//virtual HServerQuery PlayerDetails( uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse *pRequestServersResponse ) = 0;

};
#define SERVERLIST_INTERFACE_VERSION "ServerList001"

extern IServersInfo *g_pServersInfo;

#endif // ISERVERSINFO_H
