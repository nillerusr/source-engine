//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CMDLIST_H
#define CMDLIST_H
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

class CCMDList: public IResponse 
{
public:

	CCMDList(IResponse *target,serveritem_t &server, const char *rconPassword);
	~CCMDList();

	serveritem_t &GetServer();
	void RunFrame();
	bool QueryCommand(char *cmd);
	bool GotCommands();

	void SetPassword(const char *newPass);

	virtual void ServerResponded();
	virtual void ServerFailedToRespond();


private:

	serveritem_t m_Server;
	CUtlVector<char *> m_CMDList;
	IResponse *m_pResponseTarget;

	CRcon *m_pRcon;

	bool m_bGotCommands;
	char m_szRconPassword[100];

};


#endif // CMDLIST_H