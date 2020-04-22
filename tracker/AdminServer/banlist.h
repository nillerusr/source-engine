//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BANLIST_H
#define BANLIST_H
#ifdef _WIN32
#pragma once
#endif

#include "ban.h"
#include "netadr.h"
#include "utlvector.h"
#include "playermsghandler.h"
#include "Iresponse.h"

class CSocket;
class IResponse;


class CBanList: public IResponse
{
public:

	CBanList(IResponse *target,serveritem_t &server, const char *rconPassword);
	~CBanList();

	// send an rcon command to a server
	void SendQuery();

	void Refresh();

	bool IsRefreshing();
	serveritem_t &GetServer();
	void RunFrame();

	CUtlVector<Bans_t> *GetBanList();
	bool NewBanList();

	void ServerResponded();

	// called when a server response has timed out
	void ServerFailedToRespond();

	void SetPassword(const char *newPass);


private:

	serveritem_t m_Server;
	CUtlVector<Bans_t> m_BanList;

	IResponse *m_pResponseTarget;
	CRcon *m_pRcon;

	bool m_bIsRefreshing;
	bool m_bGotIPs;
	bool m_bNewBanList;
	bool m_bRconFailed;
	char m_szRconPassword[100];

};


#endif // BANLIST_H