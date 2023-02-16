//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FRIENDSGAMES_H
#define FRIENDSGAMES_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Favorite games list
//-----------------------------------------------------------------------------
class CFriendsGames : public CBaseGamesPage
{
	DECLARE_CLASS_SIMPLE( CFriendsGames, CBaseGamesPage );

public:
	CFriendsGames(vgui::Panel *parent);
	~CFriendsGames();

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem(InterfaceItem_e item);

	// called when the current refresh list is complete
	virtual void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response );

private:
	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );

	int m_iServerRefreshCount;	// number of servers refreshed
};

#endif // FRIENDSGAMES_H
