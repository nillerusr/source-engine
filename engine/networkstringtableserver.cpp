//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "quakedef.h"
#include "networkstringtable.h"
#include "networkstringtableitem.h"
#include "networkstringtable.h"
#include "utlvector.h"
#include "eiface.h"
#include "server.h"
#include "framesnapshot.h"
#include "utlsymbol.h"
#include "utlrbtree.h"
#include "host.h"
#include "LocalNetworkBackdoor.h"
#include "demo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CNetworkStringTableContainer s_NetworkStringTableServer;
CNetworkStringTableContainer *networkStringTableContainerServer = &s_NetworkStringTableServer;

// Expose interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CNetworkStringTableContainerServer, INetworkStringTableContainer, INTERFACENAME_NETWORKSTRINGTABLESERVER, s_NetworkStringTableServer );

#ifdef SHARED_NET_STRING_TABLES
	// Expose same interface to client .dll as client string tables
	EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CNetworkStringTableContainer, INetworkStringTableContainer, INTERFACENAME_NETWORKSTRINGTABLECLIENT, s_NetworkStringTableServer );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SV_CreateNetworkStringTables( void )
{
	// Remove any existing tables
	s_NetworkStringTableServer.RemoveAllTables();

	// Unset timing guard and create tables
	s_NetworkStringTableServer.AllowCreation( true );

	// Create engine tables
	sv.CreateEngineStringTables();

	// Create game code tables
	serverGameDLL->CreateNetworkStringTables();

	s_NetworkStringTableServer.AllowCreation( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SV_PrintStringTables( void )
{
	s_NetworkStringTableServer.Dump();
}

