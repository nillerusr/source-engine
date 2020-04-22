//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ServerListCompare.h"
#include "server.h"
#include "serverpage.h"

#include <VGUI_ListPanel.h>
#include <VGUI_KeyValues.h>


//-----------------------------------------------------------------------------
// Purpose: List password column sort function
//-----------------------------------------------------------------------------
int __cdecl PasswordCompare(vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	serveritem_t &s1 = CServerPage::GetInstance()->GetServer(item1.userData);
	serveritem_t &s2 = CServerPage::GetInstance()->GetServer(item2.userData);

	if (s1.password < s2.password)
		return 1;
	else if (s1.password > s2.password)
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Ping comparison function
//-----------------------------------------------------------------------------
int __cdecl PingCompare(vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	serveritem_t &s1 = CServerPage::GetInstance()->GetServer(item1.userData);
	serveritem_t &s2 = CServerPage::GetInstance()->GetServer(item2.userData);

	int ping1 = s1.ping;
	int ping2 = s2.ping;

	if ( ping1 < ping2 )
		return -1;
	else if ( ping1 > ping2 )
		return 1;
	
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Map comparison function
//-----------------------------------------------------------------------------
int __cdecl MapCompare(vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	serveritem_t &s1 = CServerPage::GetInstance()->GetServer(item1.userData);
	serveritem_t &s2 = CServerPage::GetInstance()->GetServer(item2.userData);

	return stricmp(s1.map, s2.map);
}

//-----------------------------------------------------------------------------
// Purpose: Game dir comparison function
//-----------------------------------------------------------------------------
int __cdecl GameCompare(vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	serveritem_t &s1 = CServerPage::GetInstance()->GetServer(item1.userData);
	serveritem_t &s2 = CServerPage::GetInstance()->GetServer(item2.userData);

	// make sure we haven't added the same server to the list twice
	assert(p1->userData != p2->userData);

	return stricmp(s1.gameDescription, s2.gameDescription);
}

//-----------------------------------------------------------------------------
// Purpose: Server name comparison function
//-----------------------------------------------------------------------------
int __cdecl ServerNameCompare(vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	serveritem_t &s1 = CServerPage::GetInstance()->GetServer(item1.userData);
	serveritem_t &s2 = CServerPage::GetInstance()->GetServer(item2.userData);

	return stricmp(s1.name, s2.name);
}

//-----------------------------------------------------------------------------
// Purpose: Player number comparison function
//-----------------------------------------------------------------------------
int __cdecl PlayersCompare(vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	serveritem_t &s1 = CServerPage::GetInstance()->GetServer(item1.userData);
	serveritem_t &s2 = CServerPage::GetInstance()->GetServer(item2.userData);

	int s1p = s1.players;
	int s1m = s1.maxPlayers;
	int s2p = s2.players;
	int s2m = s2.maxPlayers;

	// compare number of players
	if (s1p > s2p)
		return -1;
	if (s1p < s2p)
		return 1;

	// compare max players if number of current players is equal
	if (s1m > s2m)
		return -1;
	if (s1m < s2m)
		return 1;

	return 0;
}


