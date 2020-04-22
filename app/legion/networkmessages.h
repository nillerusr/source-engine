//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the networking code in the game 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef NETWORKMESSAGES_H
#define NETWORKMESSAGES_H

#ifdef _WIN32
#pragma once
#endif


#include "networksystem/inetworkmessage.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Network message group
//-----------------------------------------------------------------------------
enum LegionNetworkGroups_t
{
	LEGION_NETMESSAGE_GROUP = NETWORKSYSTEM_FIRST_GROUP,
};


//-----------------------------------------------------------------------------
// Network message types
//-----------------------------------------------------------------------------
enum LegionNetworkTypes_t
{
	CHAT_MESSAGE = 0,
};


//-----------------------------------------------------------------------------
// Chat network message
//-----------------------------------------------------------------------------
class CNetworkMessage_Chat : public CNetworkMessage
{
public:
	CNetworkMessage_Chat();

	DECLARE_BASE_MESSAGE( LEGION_NETMESSAGE_GROUP, CHAT_MESSAGE, "Chat Message" )

	CUtlString m_Message;
};

#endif // NETWORKMESSAGES_H


