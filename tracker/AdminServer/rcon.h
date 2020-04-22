//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: defines a RCon class used to send rcon commands to remote servers
//
// $NoKeywords: $
//=============================================================================

#ifndef RCON_H
#define RCON_H
#ifdef _WIN32
#pragma once
#endif

#include "server.h"
#include "netadr.h"

class CSocket;
class IResponse;

typedef struct 
{
	char queued[1024];
} queue_requests_t;


class CRcon 
{

public:

	CRcon(IResponse *target,serveritem_t &server, const char *password);
	~CRcon();

	// resets the state of the object, will cause it to get the challenge id again
	void Reset();

	// send an rcon command to a server
	void SendRcon(const char *cmd);

	// does nothing
	void Refresh();

	bool IsRefreshing();
	serveritem_t &GetServer();
	void RunFrame();

	// return the response from the server
	const char *RconResponse();

	// called when the message handler gets a packet back
	void UpdateServer(netadr_t *adr, int challenge,const char *resp);

	// returns the challenge id
	bool Challenge(); 

	// returns if a new rcon result is waiting
	bool NewRcon();

	// returns if the password failed
	bool PasswordFail();

	// called when a bad password is used
	void BadPassword(const char *info);

	// returns whether this rcon is disabled (due to bad passwords)
	bool Disabled(); 

	//set the password to use in the rcon request
	void SetPassword(const char *newPass);
	
private:

	// sends the actual request
	void RconRequest(const char *command, int challenge);
	// requests a challenge value from the server
	void GetChallenge(); 

	serveritem_t m_Server;
	CSocket	*m_pQuery;	// Game server query socket
	
	IResponse *m_pResponseTarget;

	bool m_bIsRefreshing; // whether we are currently performing an rcon command
	bool m_bChallenge; // whether we are currently GETTING a challenge id
	bool m_bNewRcon; // whether an rcon response is waiting to be picked up
	bool m_bPasswordFail; // whether the password failed
	bool m_bDisable; // whether rcon is disabled due to password failures
	bool m_bGotChallenge; // whether we have a valid challenge id stored away

	CUtlVector<queue_requests_t> requests;

	char m_sPassword[100];
	char m_sCmd[1024];
	char m_sRconResponse[2048];
	int m_iChallenge;
	float m_fQuerySendTime;
};


#endif // RCON_H