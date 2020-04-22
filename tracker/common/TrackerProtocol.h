//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds all the protocol bits and defines used in tracker networking
//
// $NoKeywords: $
//=============================================================================

#ifndef TRACKERPROTOCOL_H
#define TRACKERPROTOCOL_H
#ifdef _WIN32
#pragma once
#endif

// failed return versions of the messages are the TMSG_FAIL_OFFSET + 10000
#define TMSG_FAIL_OFFSET	10000

//-----------------------------------------------------------------------------
// Purpose: List of all the tracker messages used
//			msgID's are 32bits big
//-----------------------------------------------------------------------------
enum TrackerMsgID_t
{
	// generic messages
	TMSG_NONE = 0,		// no message id
	TMSG_ACK = 1,		// packet acknowledgement

	// server -> Client messages
	TSVC_BASE = 1000,

	TSVC_CHALLENGE,
	TSVC_LOGINOK,
	TSVC_LOGINFAIL,
	TSVC_DISCONNECT,
	TSVC_FRIENDS,
	TSVC_FRIENDUPDATE,
	TSVC_HEARTBEAT,
	TSVC_PINGACK,		// acknowledgement of TCLS_PING packet
	TSVC_FRIENDINFO,
	TSVC_USERVALID,
	TSVC_FRIENDSFOUND,
	TSVC_NOFRIENDS,
	TSVC_MESSAGE,				// message passed through from another client
	TSVC_GAMEINFO,				// information about a friends' game
	TSVC_AUTHREQUEST,			// a user requesting auth from the receiving user
	TSVC_CONNECTIONKEEPALIVE,	// information that an attemption connect is taking time, and the user should wait
	TSVC_ROUTEMESSAGEFAILED,	// chat message failed to be routed through the servers
	TSVC_REDIRECTLOGIN,			// tells the client to redirect their login attempt to a different server

	// Client -> server messages
	TCLS_BASE = 2000,
	
	TCLS_LOGIN,			// login message
	TCLS_RESPONSE,		// response to login challenge
	TCLS_PING,
	TCLS_FRIENDSEARCH,
	TCLS_HEARTBEAT,
	TCLS_AUTHUSER,
	TCLS_REQAUTH,
	TCLS_FRIENDINFO,	// friend info request
	TCLS_SETINFO,
	TCLS_ROUTETOFRIEND, // generic reroute of a message to a friend

	// Client -> Client messages
	TCL_BASE = 3000,
	TCL_MESSAGE,		// chat text message
	TCL_USERBLOCK,		// soon to be obselete
	TCL_ADDEDTOCHAT,
	TCL_CHATADDUSER,
	TCL_CHATUSERLEAVE,
	TCL_TYPINGMESSAGE,
    TCL_FRIENDNETMESSAGE,

	// server -> server messages
	TSV_BASE = 4000,

	TSV_WHOISPRIMARY,
	TSV_PRIMARYSRV,
	TSV_REQUESTINFO,
	TSV_TOPOLOGYINFO,
	TSV_REQUESTTOPOLOGYINFO,
	TSV_SERVERPING,
	TSV_MONITORINFO,
	TSV_LOCKUSERRANGE,
	TSV_UNLOCKUSERRANGE,
	TSV_REDIRECTTOUSER,
	TSV_FORCEDISCONNECTUSER,
	TSV_USERCHECKMESSAGES,
	TSV_USERAUTHREQUEST,
	TSV_USERSTATUSCHANGED,
	TSV_USERRELOADFRIENDSLIST,
	TSV_REGISTERSERVERINNETWORK,
	TSV_SERVERSHUTTINGDOWN,
	TSV_UPDATEACTIVEUSERRANGESTATUS,

	// game server -> Client
	TCLG_BASE = 5000,

	// common msg failed ID's
	TSVC_HEARTBEAT_FAIL = TSVC_HEARTBEAT + TMSG_FAIL_OFFSET,

	TCLS_HEARTBEAT_FAIL = TCLS_HEARTBEAT + TMSG_FAIL_OFFSET,

	TCL_MESSAGE_FAIL = TCL_MESSAGE + TMSG_FAIL_OFFSET,

};

//-----------------------------------------------------------------------------
// Purpose: List of reasons explaining to user why they have been disconnected
//			from the friends network
//-----------------------------------------------------------------------------
enum TrackerLogoffReason_t
{
	TRACKER_LOGOFF_NOREASON,

	// server reasons for disconnecting user
	TRACKER_LOGOFF_LOGGEDINELSEWHERE,	// user has logged into friends at a different location
	TRACKER_LOGOFF_SERVERWORK,			// server needs to do work (like lock the user range)
	TRACKER_LOGOFF_SERVERSHUTDOWN,		// server has been shutdown
	TRACKER_LOGOFF_TIMEDOUT,			// user hasn't heartbeat'd to server recently enough
	TRACKER_LOGOFF_REQUESTED,			// user has requested to logoff
	TRACKER_LOGOFF_FIREWALL,			// users' firewall won't allow enough packets through
	TRACKER_LOGOFF_NOTCONNECTED,		// user sent server a packet that implied they think they're logged in but they're not
	TRACKER_LOGOFF_INVALIDSTEAMTICKET,	// users steam ticket is invalid

	// client reasons for being disconnected
	TRACKER_LOGOFF_JOINEDGAME,			// user has logged off because they joined a game
	TRACKER_LOGOFF_CONNECTIONTIMEOUT,	// user connection has timed out
	TRACKER_LOGOFF_SERVERMESSAGEFAIL,   // a message to the server was not successfully transmitted

	TRACKER_LOGOFF_TOOMANYATTEMPTS,		// too many login attempts have been performed
	TRACKER_LOGOFF_OFFLINE,				// steam is in offline mode so don't try to connect
};


//-----------------------------------------------------------------------------
// Purpose: List of all the reasons a login attempt may fail
//-----------------------------------------------------------------------------
enum TrackerLoginFailReason_t
{
	TRACKER_LOGINFAIL_NOREASON			= 0,
	TRACKER_LOGINFAIL_NOSUCHUSER		= -2,
	TRACKER_LOGINFAIL_ALREADLOGGEDIN	= -3,
	TRACKER_LOGINFAIL_INVALIDSTEAMTICKET = -4,
	TRACKER_LOGINFAIL_BUILDOUTOFDATE	= -5,
	TRACKER_LOGINFAIL_PLATFORMOUTOFDATE	= -6,
};

//-----------------------------------------------------------------------------
// Purpose: Holds basic status for a friend
//-----------------------------------------------------------------------------
struct FriendStatus_t
{
	unsigned int friendID;
	int status;
	unsigned int sessionID;
	unsigned int ip;
	unsigned int port;
	unsigned int serverID;
};

#endif // TRACKERPROTOCOL_H
