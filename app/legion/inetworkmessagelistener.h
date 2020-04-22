//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the networking code in the game 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef INETWORKMESSAGELISTENER_H
#define INETWORKMESSAGELISTENER_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class INetworkMessage;


//-----------------------------------------------------------------------------
// Message route type
//-----------------------------------------------------------------------------
enum NetworkMessageRoute_t
{
	NETWORK_MESSAGE_SERVER_TO_CLIENT = 0,
	NETWORK_MESSAGE_CLIENT_TO_SERVER,

	NETWORK_MESSAGE_ROUTE_COUNT,
};


//-----------------------------------------------------------------------------
// Interface used to listen to network messages
//-----------------------------------------------------------------------------
abstract_class INetworkMessageListener
{
public:
	// Called when a particular network message occurs
	virtual void OnNetworkMessage( NetworkMessageRoute_t route, INetworkMessage *pNetworkMessage ) = 0;
};


#endif // INETWORKMESSAGELISTENER_H

