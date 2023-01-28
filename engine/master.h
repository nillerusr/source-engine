//======== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#ifndef MASTER_H
#define MASTER_H
#ifdef _WIN32
#pragma once
#endif

#include "engine/iserversinfo.h"

#define DEFAULT_MASTER_ADDRESS "185.192.97.130:27010"

//-----------------------------------------------------------------------------
// Purpose: Implements a master server interface.
//-----------------------------------------------------------------------------
class IMaster
{
public:
	// Allow master server to register cvars/commands
	virtual void Init( void ) = 0;
	// System is shutting down
	virtual void Shutdown( void ) = 0;
	// Server is shutting down
	virtual void ShutdownConnection( void ) = 0;
	// Sends the actual heartbeat to the master ( after challenge value is parsed )
	virtual void SendHeartbeat( struct adrlist_s *p ) = 0;
	// Add server to global master list
	virtual void AddServer( struct netadr_s *adr ) = 0;
	// If parsing for server, etc. fails, always have at least one server around to use.
	virtual void UseDefault ( void ) = 0;
	// See if it's time to send the next heartbeat
	virtual void CheckHeartbeat( void ) = 0;
	// Master sent back a challenge value, read it and send the actual heartbeat
	virtual void RespondToHeartbeatChallenge( netadr_t &from, bf_read &msg ) = 0;
	// Console command to set/remove master server
	virtual void SetMaster_f( void ) = 0;
	// Force a heartbeat to be issued right away
	virtual void Heartbeat_f( void ) = 0;

	virtual void ProcessConnectionlessPacket( netpacket_t *packet ) = 0;

	virtual void RunFrame( void ) = 0;
};

extern IMaster *master;
extern IServersInfo *g_pServersInfo;

#endif // MASTER_H
