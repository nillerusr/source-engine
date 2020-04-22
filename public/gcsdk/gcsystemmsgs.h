//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This file defines all of our over-the-wire net protocols for the
//			global system messages used by the GC. These are usually sent by
//			the GC Host so be very careful of versioning issues when you consider
//			changing them.  Note that we never use types with undefined length 
//			(like int).  Always use an explicit type (like int32).
//
//=============================================================================

#ifndef GCSYSTEMMSGS_H
#define GCSYSTEMMSGS_H
#ifdef _WIN32
#pragma once
#endif

// Protobuf headers interfere with the valve min/max/malloc overrides. so we need to do all
// this funky wrapping to make the include happy.
#include <tier0/valve_minmax_off.h>
#include "gcsystemmsgs.pb.h"
#include <tier0/valve_minmax_on.h>

namespace GCSDK
{


#pragma pack( push, 8 ) // this is a 8 instead of a 1 to maintain backward compatibility with Steam


// generic zero-length message struct
struct MsgGCEmpty_t
{

};

// k_EGCMsgAchievementAwarded 
struct MsgGCAchievementAwarded_t
{
	uint16 m_usStatID;
	uint8 m_ubBit;
	// var data:
	//    string data: name of achievement earned
};

// k_EGCMsgConCommand
struct MsgGCConCommand_t
{
	// var data:
	//		string: the command as typed into the console
};


// k_EGCMsgStartPlaying
struct MsgGCStartPlaying_t
{
	CSteamID m_steamID;
	CSteamID m_steamIDGS;
	uint32 m_unServerAddr;
	uint16 m_usServerPort;
};


// k_EGCMsgStartPlaying
// k_EGCMsgStopGameserver
struct MsgGCStopSession_t
{
	CSteamID m_steamID;
};


// k_EGCMsgStartGameserver
struct MsgGCStartGameserver_t
{
	CSteamID m_steamID;
	uint32 m_unServerAddr;
	uint16 m_usServerPort;
};

// k_EGCMsgWGRequest
struct MsgGCWGRequest_t
{
	uint64 m_ulSteamID;		//SteamID of auth'd WG user
	uint32 m_unPrivilege;	// The EGCWebApiPrivilege value that the request was made with
	uint32  m_cubKeyValues;	// length of the key values data blob in message (starts after string request name data)
	// var data - 
	//		request name
	//		binary key values of web request
};

// k_EGCMsgWGResponse
struct MsgGCWGResponse_t
{
	bool m_bResult;			// True if the request was successful
	uint32  m_cubKeyValues;	// length of the key values data blob in message
	// var data - 
	//		binary key values of web response
};


// k_EGCMsgGetUserGameStatsSchemaResponse
struct MsgGetUserGameStatsSchemaResponse_t
{
	bool m_bSuccess;		// True is the request was successful
	// var data -
	//		binary key values containing the User Game Stats schema
};


// k_EGCMsgGetUserStats
struct MsgGetUserStats_t
{
	uint64	m_ulSteamID;	// SteamID the stats are requested for
	uint16  m_cStatIDs;		// A count of the number of statIDs requested
	// var data -
	//		Array of m_cStatIDs 16-bit StatIDs
};


// k_EGCMsgGetUserStatsResponse
struct MsgGetUserStatsResponse_t
{
	uint64	m_ulSteamID;	// SteamID the stats were requested for
	bool	m_bSuccess;		// True is the request was successful
	uint16	m_cStats;		// Number of stats returned in the message
	// var data -
	//		m_cStats instances of:
	//			uint16 usStatID - Stat ID
	//			uint32 unData   - Stat value
};

// k_EGCMsgValidateSession
struct MsgGCValidateSession_t
{
	uint64	m_ulSteamID;	// SteamID to validate
};

// k_EGCMsgValidateSessionResponse
struct MsgGCValidateSessionResponse_t
{
	uint64 m_ulSteamID;
	uint64 m_ulSteamIDGS;
	uint32 m_unServerAddr;
	uint16 m_usServerPort;
	bool m_bOnline;
};

// response to k_EGCMsgLookupAccountFromInput
struct MsgGCLookupAccountResponse
{
	uint64	m_ulSteamID;
};

// k_EGCMsgSendHTTPRequest
struct MsgGCSendHTTPRequest_t
{
	// Variable data:
	//	- Serialized CHTTPRequest
};

// k_EGCMsgSendHTTPRequestResponse
struct MsgGCSendHTTPRequestResponse_t
{
	bool m_bCompleted;
	// Variable data:
	//	- if m_bCompleted is true, Serialized CHTTPResponse
};


// k_EGCMsgRecordSupportAction
struct MsgGCRecordSupportAction_t
{
	uint32 m_unAccountID;		// which  account is affected (object)
	uint32 m_unActorID;		// who made the change (subject)
	// Variable data:
	//	- string - Custom data for the event
	//  - string - A note with the reason for the change
};


// k_EGCMsgWebAPIRegisterInterfaces
struct MsgGCWebAPIRegisterInterfaces_t
{
	uint32 m_cInterfaces;
	// Variable data:
	// - KeyValues for interface - one per interface
};

// k_EGCMsgGetAccountDetails
struct MsgGCGetAccountDetails_t
{
	uint64	m_ulSteamID;	// SteamID to validate
};


// Used by k_EGCMsgFindAccounts
enum EAccountFindType
{
	k_EFindAccountTypeInvalid = 0, 
	k_EFindAccountTypeAccountName = 1,
	k_EFindAccountTypeEmail,
	k_EFindAccountTypePersonaName,
	k_EFindAccountTypeURL,
	k_EFindAccountTypeAllOnline,
	k_EFindAccountTypeAll,
	k_EFindClanTypeClanName,
	k_EFindClanTypeURL,
	k_EFindClanTypeOfficialURL,
	k_EFindClanTypeAppID,
};


} // namespace GCSDK

#pragma pack( pop )

#endif // GCSYSTEMMSGS_H
