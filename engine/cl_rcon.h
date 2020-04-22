//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//
#ifndef CL_RCON_H
#define CL_RCON_H

#ifdef _WIN32
#pragma once
#endif


#include "sv_main.h"
#include "netmessages.h"
#include "net.h"
#include "client.h"
#include "utlvector.h"
#include "utllinkedlist.h"
#include "netadr.h"
#include "sv_remoteaccess.h"
#include "sv_rcon.h"
#include "socketcreator.h"
#include "igameserverdata.h"
#include "ivprofexport.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

abstract_class IVProfData
{
public:
	virtual void OnRemoteGroupData( const void *data, int len ) = 0;
	virtual void OnRemoteData( const void *data, int len ) = 0;
};


//-----------------------------------------------------------------------------
// Used to display client perf data in showbudget
//-----------------------------------------------------------------------------
class CRConVProfExport : public IVProfExport, public IVProfData
{
	// Inherited from IVProfExport
public:
	virtual void AddListener();
	virtual void RemoveListener();
	virtual void PauseProfile();
	virtual void ResumeProfile();
	virtual void SetBudgetFlagsFilter( int filter );
	virtual int GetNumBudgetGroups();
	virtual void GetBudgetGroupInfos( CExportedBudgetGroupInfo *pInfos );
	virtual void GetBudgetGroupTimes( float times[MAX_BUDGETGROUP_TIMES] );

	// Inherited from IVProfData
public:
	virtual void OnRemoteGroupData( const void *data, int len );
	virtual void OnRemoteData( const void *data, int len );

	// Other public methods
public:
	CRConVProfExport();

private:
	void CleanupGroupData();

	CUtlVector< CExportedBudgetGroupInfo > m_Info;
	CUtlVector<float> m_Times;	// Times from the most recent snapshot.
};


class CRConClient : public ISocketCreatorListener
{
public:
	CRConClient();
	~CRConClient();

	void SetAddress( const netadr_t & netAdr );

	// Connects to the address specified by SetAddress
	bool ConnectSocket();
	void Disconnect() { CloseSocket(); }

	// Creates a listen server, connects to remote machines that connect to it
	void CreateListenSocket( const netadr_t &netAdr );
	void CloseListenSocket();

	void RunFrame();
	void SendCmd( const char *msg );
	bool IsConnected() const;
	bool IsAuthenticated() const { return m_bAuthenticated; }

	void RegisterVProfDataCallback( IVProfData *callback );
	void StopVProfData();
	void StartVProfData();

	void TakeScreenshot();
	void GrabConsoleLog();

	void SetPassword( const char *pPassword );

	void SetRemoteFileDirectory( const char *pDir );

	// Inherited from ISocketCreatorListener
	virtual bool ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t & netAdr ); 
	virtual void OnSocketAccepted( SocketHandle_t hSocket, const netadr_t & netAdr, void** ppData ); 
	virtual void OnSocketClosed( SocketHandle_t hSocket, const netadr_t & netAdr, void* pData );

private:
	SocketHandle_t GetSocketHandle() const;
	void CloseSocket();
	void Authenticate();
	void ParseReceivedData();
	void SendQueuedData();
	void SendResponse( CUtlBuffer &response, bool bAutoAuthenticate = true );
	void BuildResponse( CUtlBuffer &response, ServerDataRequestType_t msg, const char *pString1, const char *pString2 );
	void SaveRemoteScreenshot( const void* pBuffer, int nBufLen );
	void SaveRemoteConsoleLog( const void* pBuffer, int nBufLen );

	CRConVProfExport m_VProfExport;
	CSocketCreator	m_Socket;
	netadr_t		m_Address;
	int				m_iAuthRequestID;
	int				m_iReqID;
	bool			m_bAuthenticated;
	CUtlBuffer		m_RecvBuffer;
	CUtlBuffer		m_SendBuffer;
	CUtlString		m_Password;
	CUtlString		m_RemoteFileDir;
	int				m_nScreenShotIndex;
	int				m_nConsoleLogIndex;
};

inline SocketHandle_t CRConClient::GetSocketHandle() const
{
	return m_Socket.GetAcceptedSocketHandle( 0 );
}

CRConClient & RCONClient();
#ifdef ENABLE_RPT
CRConClient & RPTClient();		// used in remote perf testing
#endif // ENABLE_RPT

#endif // CL_RCON_H

