//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef SM_PROTOCOL_H
#define SM_PROTOCOL_H
#ifdef _WIN32
#pragma once
#endif

enum
{
	CONNECTIONLESS_HEADER = 0xffffffff,
};

// Connectionless messages
enum
{
	c2s_connect = 1,

	c2s_num_messages
};

enum
{
	s2c_connect_accept = 1,
	s2c_connect_reject,

	s2c_num_messages
};

enum NetworkMessageGroup_t
{
	net_group_networksystem = 0
};

// Networksystem internal messages for use during valid connection
enum SystemNetworkMessageType_t
{
	net_nop 		= 0,			// nop command used for padding
	net_disconnect	= 1,			// disconnect, last message in connection

	net_num_messages
};


#endif // SM_PROTOCOL_H
