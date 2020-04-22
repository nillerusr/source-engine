//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles all the functions for implementing remote access to the engine
//
//===========================================================================//
#ifndef SV_REMOTEACCESS_H
#define SV_REMOTEACCESS_H

#ifdef _WIN32
#pragma once
#endif

#include "igameserverdata.h"
#include "utlbuffer.h"
#include "utllinkedlist.h"

class CRConServer;


class CServerRemoteAccess : public IGameServerData
{
public:
	CServerRemoteAccess();

	// handles a request
	virtual void WriteDataRequest( ra_listener_id listener, const void *buffer, int bufferSize );
	void WriteDataRequest( CRConServer *pNetworkListener, ra_listener_id listener, const void *buffer, int bufferSize);

	// gets return value from the server
	// returns the number of bytes read
	virtual int ReadDataResponse( ra_listener_id listener, void *buffer, int bufferSize);
	int GetDataResponseSize( ra_listener_id listener );

	// sends a message to all the watching admin UI's
	void SendMessageToAdminUI( ra_listener_id listenerID, const char *message);

	void SendVProfData( ra_listener_id listenerID, bool bGroupData, void *data, int len );

	virtual ra_listener_id GetNextListenerID( bool authConnection, const netadr_t *adr = NULL );
	virtual void RegisterAdminUIID( ra_listener_id listener ) { m_AdminUIID = listener; }

	ra_listener_id GetAdminUIID() { return m_AdminUIID; }
	void GetStatsString(char *buf, int bufSize); // also used by the 'stats' command

	void UploadScreenshot( const char *pFileName );

private:
	void RespondString( ra_listener_id listener, int requestID, const char *pString );
	void RequestValue( ra_listener_id listener, int requestID, const char *variable );
	void SetValue(const char *variable, const char *value);
	void ExecCommand(const char *cmdString);
	bool LookupValue(const char *variable, CUtlBuffer &value);
	const char *LookupStringValue(const char *variable);
	bool IsAuthenticated( ra_listener_id listener );
	void CheckPassword( CRConServer *pNetworkListener, ra_listener_id listener, int requestID, const char *password );
	void BadPassword( CRConServer *pNetworkListener, ra_listener_id listener );
	void LogCommand(  ra_listener_id listener, const char *msg );
	void SendResponseToClient( ra_listener_id listenerID, ServerDataResponseType_t type, void *pData, int nDataLen );

	// specific value requests
	void GetUserBanList(CUtlBuffer &value);
	void GetPlayerList(CUtlBuffer &value);
	void GetMapList(CUtlBuffer &value);

	// list of responses waiting to be sent
	struct DataResponse_t
	{
		CUtlBuffer packet;
		ra_listener_id responderID;
	};
	CUtlLinkedList<DataResponse_t, int> m_ResponsePackets;

	struct ListenerStore_t
	{
		ra_listener_id	listenerID;
		bool			authenticated;
		bool			m_bHasAddress;
		netadr_t		adr;
	};
	
	CUtlLinkedList<ListenerStore_t, int>	m_ListenerIDs;
	ra_listener_id				m_NextListenerID;

	int m_nScreenshotListener;
	int m_iBytesSent;
	int m_iBytesReceived;
	ra_listener_id				m_AdminUIID;
};

extern CServerRemoteAccess g_ServerRemoteAccess;
extern "C" void NotifyDedicatedServerUI(const char *message);

#endif // SV_REMOTEACCESS_H

