//====== Copyright (C), Valve Corporation, All rights reserved. =======
//
// Purpose: This file defines all of our over-the-wire net protocols for the
//			Game Coordinator that are mod-independent.  Note that we never use types
//			with undefined length (like int).  Always use an explicit type 
//			(like int32).
//
//=============================================================================

#ifndef GENERIC_GCMESSAGES_H
#define GENERIC_GCMESSAGES_H
#ifdef _WIN32
#pragma once
#endif


enum EGCMsg
{
	k_EMsgGCInvalid =							0,
	k_EMsgGCMulti =								1,

	k_EMsgGCGenericReply =						10,

	k_EMsgGCBase =								1000,
	k_EMsgGCKVCommand =							k_EMsgGCBase + 1,
	k_EMsgGCKVCommandResponse =					k_EMsgGCBase + 2,

	k_EMsgGCModBase =							2000,
};

// generic zero-length message struct
struct MsgGCEmpty_t
{

};

// k_EMsgGCKVCommand
struct MsgGCGenericKV_t
{
	// Variable length data:
	//	A serialized KeyValues structure
};

// k_EMsgGCKVCommandResponse
struct MsgGCGenericKVResponse_t
{
	bool	m_bSuccess;
	// Variable length data:
	//	A serialized KeyValues structure
};

#endif