//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PLAYERLIST_H
#define PLAYERLIST_H
#ifdef _WIN32
#pragma once
#endif

#include "server.h"
#include "netadr.h"
#include "utlvector.h"
#include "playermsghandler.h"
#include "Iresponse.h"

class CSocket;
class IResponse;


class CPlayerList: public IResponse
{
public:

	CPlayerList(IResponse *target,serveritem_t &server, const char *rconPassword);
	~CPlayerList();

	// send an rcon command to a server
	void SendQuery();

	void Refresh();

	bool IsRefreshing();
	serveritem_t &GetServer();
	void RunFrame();

	void UpdateServer();
	CUtlVector<Players_t> *GetPlayerList();
	bool NewPlayerList();

	void ServerResponded();

	// called when a server response has timed out
	void ServerFailedToRespond();

	void SetPassword(const char *newPass);


private:

	serveritem_t m_Server;
	CSocket	*m_pQuery;	// Game server query socket
	CUtlVector<Players_t> m_PlayerList;

	IResponse *m_pResponseTarget;
	CRcon *m_pRcon;
	CPlayerMsgHandlerDetails *m_pPlayerInfoMsg;

	bool m_bIsRefreshing;
	bool m_bNewPlayerList;
	bool m_bRconFailed;
	char m_szRconPassword[100];

};


#endif // PLAYERLIST_H