//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// protocol.h -- communications protocols
#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef _WIN32
#pragma once
#endif

// Used to classify entity update types in DeltaPacketEntities.
enum UpdateType
{
	EnterPVS = 0,	// Entity came back into pvs, create new entity if one doesn't exist

	LeavePVS,		// Entity left pvs

	DeltaEnt,		// There is a delta for this entity.
	PreserveEnt,	// Entity stays alive but no delta ( could be LOD, or just unchanged )
	
	Finished,		// finished parsing entities successfully
	Failed,			// parsing error occured while reading entities
};

// Flags for delta encoding header
enum
{
	FHDR_ZERO			= 0x0000,
	FHDR_LEAVEPVS		= 0x0001,
	FHDR_DELETE			= 0x0002,
	FHDR_ENTERPVS		= 0x0004,
};



#define INSTANCE_BASELINE_TABLENAME	"instancebaseline"
#define LIGHT_STYLES_TABLENAME		"lightstyles"
#define USER_INFO_TABLENAME			"userinfo"
#define SERVER_STARTUP_DATA_TABLENAME	"server_query_info"	// the name is a remnant...


//#define CURRENT_PROTOCOL    1


#define DELTA_OFFSET_BITS 5
#define DELTA_OFFSET_MAX ( ( 1 << DELTA_OFFSET_BITS ) - 1 )

#define DELTASIZE_BITS		20	// must be: 2^DELTASIZE_BITS > (NET_MAX_PAYLOAD * 8)

// Largest # of commands to send in a packet
#define NUM_NEW_COMMAND_BITS		4
#define MAX_NEW_COMMANDS			((1 << NUM_NEW_COMMAND_BITS)-1)

// Max number of history commands to send ( 2 by default ) in case of dropped packets
#define NUM_BACKUP_COMMAND_BITS		3
#define MAX_BACKUP_COMMANDS			((1 << NUM_BACKUP_COMMAND_BITS)-1)


#define PROTOCOL_AUTHCERTIFICATE 0x01   // Connection from client is using a WON authenticated certificate
#define PROTOCOL_HASHEDCDKEY     0x02	// Connection from client is using hashed CD key because WON comm. channel was unreachable
#define PROTOCOL_STEAM			 0x03	// Steam certificates
#define PROTOCOL_LASTVALID       0x03    // Last valid protocol

#define CONNECTIONLESS_HEADER			0xFFFFFFFF	// all OOB packet start with this sequence
#define STEAM_KEYSIZE				2048  // max size needed to contain a steam authentication key (both server and client)

// each channel packet has 1 byte of FLAG bits
#define PACKET_FLAG_RELIABLE			(1<<0)	// packet contains subchannel stream data
#define PACKET_FLAG_COMPRESSED			(1<<1)	// packet is compressed
#define PACKET_FLAG_ENCRYPTED			(1<<2)  // packet is encrypted
#define PACKET_FLAG_SPLIT				(1<<3)  // packet is split
#define PACKET_FLAG_CHOKED				(1<<4)  // packet was choked by sender
#define PACKET_FLAG_CHALLENGE			(1<<5)  // packet contains challenge number, use to prevent packet injection

// NOTE:  Bits 5, 6, and 7 are used to specify the # of padding bits at the end of the packet!!!
#define ENCODE_PAD_BITS( x ) ( ( x << 5 ) & 0xff )
#define DECODE_PAD_BITS( x ) ( ( x >> 5 ) & 0xff )

// shared commands used by all streams, handled by stream layer, TODO

#define	net_NOP 		0			// nop command used for padding
#define net_Disconnect	1			// disconnect, last message in connection
#define net_File		2			// file transmission message request/deny

#define net_Tick		3			// send last world tick
#define net_StringCmd	4			// a string command
#define net_SetConVar	5			// sends one/multiple convar settings
#define	net_SignonState	6			// signals current signon state

//
// server to client
//

#define	svc_Print			7		// print text to console
#define	svc_ServerInfo		8		// first message from server about game, map etc
#define svc_SendTable		9		// sends a sendtable description for a game class
#define svc_ClassInfo		10		// Info about classes (first byte is a CLASSINFO_ define).							
#define	svc_SetPause		11		// tells client if server paused or unpaused


#define	svc_CreateStringTable	12	// inits shared string tables
#define	svc_UpdateStringTable	13	// updates a string table

#define svc_VoiceInit		14		// inits used voice codecs & quality
#define svc_VoiceData		15		// Voicestream data from the server

// #define svc_HLTV			16		// HLTV control messages

#define	svc_Sounds			17		// starts playing sound

#define	svc_SetView			18		// sets entity as point of view
#define	svc_FixAngle		19		// sets/corrects players viewangle
#define	svc_CrosshairAngle	20		// adjusts crosshair in auto aim mode to lock on traget

#define	svc_BSPDecal		21		// add a static decal to the worl BSP
// NOTE: This is now unused!
//#define	svc_TerrainMod		22		// modification to the terrain/displacement

// Message from server side to client side entity
#define svc_UserMessage		23	// a game specific message 
#define svc_EntityMessage	24	// a message for an entity
#define	svc_GameEvent		25	// global game event fired

#define	svc_PacketEntities	26  // non-delta compressed entities

#define	svc_TempEntities	27	// non-reliable event object

#define svc_Prefetch		28	// only sound indices for now

#define svc_Menu			29	// display a menu from a plugin

#define svc_GameEventList	30	// list of known games events and fields

#define svc_GetCvarValue	31	// Server wants to know the value of a cvar on the client

#define svc_CmdKeyValues	32	// Server submits KeyValues command for the client
#define svc_SetPauseTimed	33	// Timed pause - to avoid breaking demos

#define SVC_LASTMSG			33	// last known server messages

//
// client to server
//

#define clc_ClientInfo			8		// client info (table CRC etc)
#define	clc_Move				9		// [CUserCmd]
#define clc_VoiceData			10      // Voicestream data from a client
#define clc_BaselineAck			11		// client acknowledges a new baseline seqnr
#define clc_ListenEvents		12		// client acknowledges a new baseline seqnr
#define clc_RespondCvarValue	13		// client is responding to a svc_GetCvarValue message.
#define clc_FileCRCCheck		14		// client is sending a file's CRC to the server to be verified.
#define clc_SaveReplay			15		// client is sending a save replay request to the server.
#define clc_CmdKeyValues		16
#define clc_FileMD5Check		17		// client is sending a file's MD5 to the server to be verified.

#define CLC_LASTMSG			17		//	last known client message

#define RES_FATALIFMISSING	(1<<0)   // Disconnect if we can't get this file.
#define RES_PRELOAD			(1<<1)  // Load on client rather than just reserving name

#define SIGNONSTATE_NONE		0	// no state yet, about to connect
#define SIGNONSTATE_CHALLENGE	1	// client challenging server, all OOB packets
#define SIGNONSTATE_CONNECTED	2	// client is connected to server, netchans ready
#define SIGNONSTATE_NEW			3	// just got serverinfo and string tables
#define SIGNONSTATE_PRESPAWN	4	// received signon buffers
#define SIGNONSTATE_SPAWN		5	// ready to receive entity packets
#define SIGNONSTATE_FULL		6	// we are fully connected, first non-delta packet received
#define SIGNONSTATE_CHANGELEVEL	7	// server is changing level, please wait

//
// matchmaking
//

#define mm_Heartbeat		16		// send a mm_Heartbeat
#define mm_ClientInfo		17		// information about a player
#define mm_JoinResponse		18		// response to a matchmaking join request
#define mm_RegisterResponse	19		// response to a matchmaking join request
#define mm_Migrate			20		// tell a client to migrate
#define mm_Mutelist			21		// send mutelist info to other clients
#define mm_Checkpoint		22		// game state checkpoints (start, connect, etc)

#define MM_LASTMSG			22		// last known matchmaking message

#endif // PROTOCOL_H
