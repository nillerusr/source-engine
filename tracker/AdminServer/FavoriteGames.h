//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FAVORITEGAMES_H
#define FAVORITEGAMES_H
#ifdef _WIN32
#pragma once
#endif

#include "BaseGamesPage.h"

#include "IGameList.h"
#include "IServerRefreshResponse.h"
#include "server.h"

class KeyValues;

namespace vgui
{
class ListPanel;
class MessageBox;
};

class CUtlBuffer;

//-----------------------------------------------------------------------------
// Purpose: Favorite games list
//-----------------------------------------------------------------------------
class CFavoriteGames : public CBaseGamesPage
{
public:
	CFavoriteGames(vgui::Panel *parent);
	~CFavoriteGames();

	// favorites list, loads/saves into keyvalues
	void LoadFavoritesList(vgui::KeyValues *favoritesData,bool loadrcon);
	void SaveFavoritesList(vgui::KeyValues *favoritesData,bool savercon);

	// property page handlers
//	virtual void OnPageShow();
//	virtual void OnPageHide();

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem(InterfaceItem_e item);

	// starts the servers refreshing
	virtual void StartRefresh();

	// gets a new server list
	virtual void GetNewServerList();

	// stops current refresh/GetNewServerList()
	virtual void StopRefresh();

	// returns true if the list is currently refreshing servers
	virtual bool IsRefreshing();

	// gets information about specified server
//	virtual serveritem_t &GetServer(unsigned int serverID);

	// adds a new server to list
	virtual void AddNewServer(serveritem_t &server);

	// marks that server list has been fully received
	virtual void ListReceived(bool moreAvailable, int lastUnique);

	// called when Connect button is pressed
	virtual void OnBeginConnect();

	// called to look at game info
	virtual void OnViewGameInfo();

	// reapplies filters (does nothing with this)
	virtual void ApplyFilters();

	// IServerRefreshResponse handlers
	// called when the server has successfully responded
	virtual void ServerResponded(serveritem_t &server);

	// called when a server response has timed out
	virtual void ServerFailedToRespond(serveritem_t &server);

	// called when the current refresh list is complete
	virtual void RefreshComplete();

	virtual void ImportFavorites();
	virtual void OnImportFavoritesFile();

	// updates the rconPassword field for this server
	void UpdateServer(serveritem_t &serverIn);


private:
	// context menu message handlers
	void OnOpenContextMenu(int row);
	void OnRefreshServer(int serverID);
	void OnRemoveFromFavorites();
	void OnAddServerByName();

	void OnCommand(const char *command);
	bool LoadAFavoriteServer (CUtlBuffer &buf);
	bool CheckForCorruption(char *data, const char *checkString);

	int m_iServerRefreshCount;	// number of servers refreshed
	bool m_bSaveRcon;

	DECLARE_PANELMAP();
	typedef CBaseGamesPage BaseClass;

	// message that favorites are being imported
	vgui::MessageBox *m_pImportFavoritesdlg;

};


#endif // FAVORITEGAMES_H
