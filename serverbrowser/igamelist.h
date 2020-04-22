//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef IGAMELIST_H
#define IGAMELIST_H
#ifdef _WIN32
#pragma once
#endif

class gameserveritem_t;
#if defined( STEAM )
#include "steam2common.h"
#include "FindSteam2Servers.h"
#else
#include "steamcommon.h"
#include "FindSteamServers.h"
#endif
#include "netadr.h"


typedef enum
{
	SERVERVERSION_SAME_VERSION = 0,
	SERVERVERSION_SERVER_OLD,
	SERVERVERSION_SERVER_NEWER
} SERVERVERSION;

struct serverdisplay_t
{
	serverdisplay_t()
	{
		m_iListID = -1;
		m_iServerID = -1;
		m_bDoNotRefresh = true;
	}
	int			m_iListID;		// the VGUI2 list panel index for displaying this server
	int			m_iServerID;	// the matchmaking interface index for this server
	bool		m_bDoNotRefresh; 
	bool operator==( const serverdisplay_t &rhs ) const { return rhs.m_iServerID == m_iServerID; }

};

//-----------------------------------------------------------------------------
// Purpose: Interface to accessing a game list
//-----------------------------------------------------------------------------
class IGameList
{
public:

	enum InterfaceItem_e
	{
		FILTERS,
		GETNEWLIST,
		ADDSERVER,
		ADDCURRENTSERVER,
	};

	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem(InterfaceItem_e item) = 0;

	// starts the servers refreshing
	virtual void StartRefresh() = 0;

	// gets a new server list
	virtual void GetNewServerList() = 0;

	// stops current refresh/GetNewServerList()
	virtual void StopRefresh() = 0;

	// returns true if the list is currently refreshing servers
	virtual bool IsRefreshing() = 0;

	// gets information about specified server
	virtual gameserveritem_t *GetServer(unsigned int serverID) = 0;

	// called when Connect button is pressed
	virtual void OnBeginConnect() = 0;

	// invalid server index
	virtual int GetInvalidServerListID() = 0;

	// Get code to use for tracking how people are connecting to servers
	virtual const char *GetConnectCode() = 0;
};


#endif // IGAMELIST_H
