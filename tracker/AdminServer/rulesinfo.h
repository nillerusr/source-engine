//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#ifndef RULESINFO_H
#define RULESINFO_H
#ifdef _WIN32
#pragma once
#endif

#include "server.h"
#include "netadr.h"

class CSocket;
class IResponse;

#include <VGUI_PropertyPage.h>
#include <VGUI_Frame.h>
#include <VGUI_ListPanel.h>
#include <VGUI_KeyValues.h>


class CRulesInfo 
{

public:

	CRulesInfo(IResponse *target,serveritem_t &server);
	~CRulesInfo();

	// send an rcon command to a server
	void Query();

	void Refresh();

	bool IsRefreshing();
	serveritem_t &GetServer();
	void RunFrame();
	bool Refreshed();

	void UpdateServer(netadr_t *adr, CUtlVector<vgui::KeyValues *> *Rules);
	
	CUtlVector<vgui::KeyValues *> *Rules();

	int serverID;
	int received;

private:

	serveritem_t m_Server;
	CSocket	*m_pQuery;	// Game server query socket
	
	IResponse *m_pResponseTarget;

	bool m_bIsRefreshing;
	float m_fSendTime;
	bool m_bRefreshed;

	CUtlVector<vgui::KeyValues *> *m_vRules;

};


#endif // RULESINFO_H