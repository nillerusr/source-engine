//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVCLIENTSTATE_H
#define HLTVCLIENTSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include "baseclientstate.h"

class CClientFrame;
class CHLTVServer;

extern ConVar tv_name;

class CHLTVClientState : public CBaseClientState
{

friend class CHLTVServer;

public:
	CHLTVClientState();
	virtual ~CHLTVClientState();

public:
	
	const char *GetCDKeyHash() { return "HLTVHLTVHLTVHLTVHLTVHLTVHLTVHLTV"; }; // haha
	bool SetSignonState ( int state, int count );
	void SendClientInfo( void );
	void PacketEnd( void );
	void Clear( void );
	void RunFrame ( void );
	void InstallStringTableCallback( char const *tableName );
	virtual bool HookClientStringTable( char const *tableName );
	virtual const char *GetClientName() { return tv_name.GetString(); }

	void ConnectionCrashed( const char * reason );
	void ConnectionClosing( const char * reason );
	int GetConnectionRetryNumber() const;

	void ReadEnterPVS( CEntityReadInfo &u );
	void ReadLeavePVS( CEntityReadInfo &u );
	void ReadDeltaEnt( CEntityReadInfo &u );
	void ReadPreserveEnt( CEntityReadInfo &u );
	void ReadDeletions( CEntityReadInfo &u );

	void CopyNewEntity( CEntityReadInfo &u,	int iClass,	int iSerialNum );

public: // IServerMessageHandlers

	PROCESS_NET_MESSAGE( StringCmd );
	PROCESS_NET_MESSAGE( SetConVar );
		
	PROCESS_SVC_MESSAGE( ServerInfo );
	PROCESS_SVC_MESSAGE( ClassInfo );
	PROCESS_SVC_MESSAGE( VoiceInit );
	PROCESS_SVC_MESSAGE( VoiceData );
	PROCESS_SVC_MESSAGE( Sounds );
	PROCESS_SVC_MESSAGE( FixAngle );
	PROCESS_SVC_MESSAGE( CrosshairAngle );
	PROCESS_SVC_MESSAGE( BSPDecal );
	PROCESS_SVC_MESSAGE( GameEvent );
	PROCESS_SVC_MESSAGE( UserMessage );
	PROCESS_SVC_MESSAGE( EntityMessage );
	PROCESS_SVC_MESSAGE( SetView );
	PROCESS_SVC_MESSAGE( PacketEntities );
	PROCESS_SVC_MESSAGE( TempEntities );
	PROCESS_SVC_MESSAGE( Prefetch );
	PROCESS_SVC_MESSAGE( GameEventList );
	PROCESS_SVC_MESSAGE( Menu );

public:
	void SendPacket();
	void UpdateStats();

	CClientFrame	*m_pNewClientFrame; // not NULL if we just got a packet with a new entity frame
	CClientFrame	*m_pCurrentClientFrame; // NULL or pointer to last entity frame
	bool			m_bSaveMemory; //compress data as much as possible to keep whole demos in memory
	float			m_fNextSendUpdateTime;
	CHLTVServer		*m_pHLTV;	// HLTV server this client state belongs too.
};

#endif // HLTVCLIENTSTATE_H
