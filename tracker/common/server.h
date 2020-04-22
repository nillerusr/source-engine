//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVER_H
#define SERVER_H

#ifdef _WIN32
#pragma once
#endif

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
	unsigned int serverID;
	int listEntryID;
	char rconPassword[64];	// the rcon password for this server
	bool loadedFromFile;		// true if this entry was loaded from file rather than comming from the master
};


#endif // SERVER_H
