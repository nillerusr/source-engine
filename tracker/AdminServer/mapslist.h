//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MAPSLIST_H
#define MAPSLIST_H
#ifdef _WIN32
#pragma once
#endif

#include "server.h"
#include "netadr.h"
#include "maps.h"
#include "rcon.h"
#include "utlvector.h"
#include "Iresponse.h"

class CSocket;
class IResponse;


class CMapsList: public IResponse
{
public:

	CMapsList(IResponse *target,serveritem_t &server, const char *rconPassword,const char *mod);
	~CMapsList();

	// send an rcon command to a server
	void SendQuery();

	void Refresh();

	bool IsRefreshing();
	serveritem_t &GetServer();
	void RunFrame();

	void UpdateServer();
	CUtlVector<Maps_t> *GetMapsList();
	bool NewMapsList();

	void ServerResponded();

	// called when a server response has timed out
	void ServerFailedToRespond();

	void SetPassword(const char *newPass);


private:

	serveritem_t m_Server;
	CUtlVector<Maps_t> m_MapsList;

	IResponse *m_pResponseTarget;
	CRcon *m_pRcon;
	char m_sMod[64];
	
	bool m_bIsRefreshing;
	bool m_bNewMapsList;
	bool m_bRconFailed;
	char m_szRconPassword[100];

};


#endif // MAPSLIST_H