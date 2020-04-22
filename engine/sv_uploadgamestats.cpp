//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifdef _WIN32

#if !defined( _X360 )
#include <winsock.h>
#else
#include "winsockx.h"
#endif

#elif POSIX
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
//$ #include <uuid/uuid.h>
typedef unsigned char uuid_t[16];
#ifdef OSX
#include <uuid/uuid.h>
#else
typedef unsigned char uuid_t[16];
#endif
#include <pwd.h>
#define closesocket close
#include "quakedef.h" // build_number()
#endif

#include "net.h"
#include "quakedef.h"
#include "sv_uploadgamestats.h"
#include "host.h"
#include "host_phonehome.h"
#include "mathlib/IceKey.H"
#include "bitbuf.h"
#include "tier0/icommandline.h"
#include "tier0/vcrmode.h"
#include "blockingudpsocket.h"
#include "cserserverprotocol_engine.h"
#include "utlbuffer.h"
#include "eiface.h"
#include "FindSteamServers.h"
#include <vstdlib/random.h>
#include "iregistry.h"
#include "filesystem_engine.h"
#include "checksum_md5.h"
#include "cl_steamauth.h"
#include "steam/steam_gameserver.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier2/tier2.h"
#include "server.h"
#include "sv_steamauth.h"
#include "host_state.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

typedef unsigned int u32;
typedef unsigned char u8;
typedef unsigned short u16;

namespace GameStatsHarvester
{

	enum EFileType
	{
		eFileTypeGameStats,

		eFILETYPECOUNT		// Count number of legal values
	};


	enum ESendMethod
	{
		eSendMethodWholeRawFileNoBlocks,
		eSendMethodCompressedBlocks, // TODO: Reenable compressed sending of minidumps

		eSENDMETHODCOUNT	// Count number of legal values
	};

}

using namespace GameStatsHarvester;

// TODO: cut protocol version down to u8 if possible, to reduce bandwidth usage 
// for very frequent but tiny commands.
typedef u32		ProtocolVersion_t;

typedef u8		ProtocolAcceptanceFlag_t;
typedef u8		ProtocolUnacceptableAck_t;

typedef u32		MessageSequenceId_t;

typedef u32		ServerSessionHandle_t;
typedef u32		ClientSessionHandle_t;

typedef u32		NetworkTransactionId_t;

// Command codes are intentionally as small as possible to minimize bandwidth usage 
// for very frequent but tiny commands (e.g. GDS 'FindServer' commands).
typedef u8		Command_t;

// ... likewise response codes are as small as possible - we use this when we 
// ... can and revert to large types on a case by case basis.
typedef u8		CommandResponse_t;


// This define our standard type for length prefix for variable length messages 
// in wire protocols.
// This is specifically used by CWSABUFWrapper::PrepareToReceiveLengthPrefixedMessage()
// and its supporting functions.
// It is defined here for generic (portable) network code to use when constructing 
// messages to be sent to peers that use the above function.
// e.g. SteamValidateUserIDTickets.dll uses this for that purpose.

// We support u16 or u32 (obviously switching between them breaks existing protocols 
// unless all components are switched simultaneously).
typedef	u32		NetworkMessageLengthPrefix_t;


// Similarly, strings should be preceeded by their length.
typedef u16		StringLengthPrefix_t;


const ProtocolAcceptanceFlag_t	cuProtocolIsNotAcceptable
								= static_cast<ProtocolAcceptanceFlag_t>( 0 );

const ProtocolAcceptanceFlag_t	cuProtocolIsAcceptable
								= static_cast<ProtocolAcceptanceFlag_t>( 1 );

const Command_t					cuMaxCommand
								= static_cast<Command_t>(255);

const CommandResponse_t			cuMaxCommandResponse
								= static_cast<CommandResponse_t>(255);

// This is for mapping requests back to error ids for placing into the database appropriately.
typedef u32								ContextID_t;

// This is the version of the protocol used by latest-build clients.
const ProtocolVersion_t			cuCurrentProtocolVersion		= 1;

// This is the minimum protocol version number that the client must 
// be able to speak in order to communicate with the server.
// The client sends its protocol version this before every command, and if we 
// don't support that version anymore then we tell it nicely.  The client 
// should respond by doing an auto-update.
const ProtocolVersion_t			cuRequiredProtocolVersion		= 1;


namespace Commands
{
	const Command_t				cuGracefulClose					= 0;
	const Command_t				cuSendGameStats					= 1;
	const Command_t				cuNumCommands					= 2;
	const Command_t				cuNoCommandReceivedYet			= cuMaxCommand;
}


namespace HarvestFileCommand
{
	typedef u32							SenderTypeId_t;
	typedef u32							SenderTypeUniqueId_t;
	typedef u32							SenderSourceCodeControlId_t;
	typedef u32							FileSize_t;

	// Legal values defined by EFileType
	typedef u32							FileType_t;

	// Legal values defined by ESendMethod
	typedef u32							SendMethod_t;

	const CommandResponse_t		cuOkToSendFile					= 0;
	const CommandResponse_t		cuFileTooBig					= 1;
	const CommandResponse_t		cuInvalidSendMethod				= 2;
	const CommandResponse_t		cuInvalidMaxCompressedChunkSize	= 3;
	const CommandResponse_t		cuInvalidGameStatsContext		= 4;
	const uint							cuNumCommandResponses			= 5;
}

//#############################################################################
//
// Class declaration:	CWin32UploadGameStats
//
//#############################################################################
// 
// Authors:
//
//		Yahn Bernier
//
// Description and general notes:
//
//		Handles uploading game stats data blobs to the CSERServer
//		(Client Stats & Error Reporting Server)

typedef enum
{
	// General status
	eGameStatsUploadSucceeded = 0,
	eGameStatsUploadFailed,

	// Specific status
	eGameStatsBadParameter,
	eGameStatsUnknownStatus,
	eGameStatsSendingGameStatsHeaderSucceeded,
	eGameStatsSendingGameStatsHeaderFailed,
	eGameStatsReceivingResponseSucceeded,
	eGameStatsReceivingResponseFailed,
	eGameStatsConnectToCSERServerSucceeded,
	eGameStatsConnectToCSERServerFailed,
	eGameStatsUploadingGameStatsSucceeded,
	eGameStatsUploadingGameStatsFailed
} EGameStatsUploadStatus;

struct TGameStatsProgress
{
	// A text string describing the current progress
	char			m_sStatus[ 512 ];
};

typedef void ( *GAMESTATSREPORTPROGRESSFUNC )( u32 uContext, const TGameStatsProgress & rGameStatsProgress );

struct TGameStatsParameters
{
	TGameStatsParameters() : 
		m_uAppId( 0 )
	{
	}

	// IP Address of the CSERServer to send the report to
	netadr_t				m_ipCSERServer;		

	// Source Control Id (or build_number) of the product
	u32						m_uEngineBuildNumber;
	// Name of the .exe
	char					m_sExecutableName[ 64 ];
	// Game directory
	char					m_sGameDirectory[ 64 ];
	// Map name the server wants to upload statistics about
	char					m_sMapName[ 64 ];

	// Version id for stats blob
	u32						m_uStatsBlobVersion;

	u32						m_uStatsBlobSize;
	void					*m_pStatsBlobData;

	u32						m_uProgressContext;
	GAMESTATSREPORTPROGRESSFUNC	m_pOptionalProgressFunc;

	u32						m_uAppId;
};

// Note that this API is blocking, though the callback, if passed, can occur during execution.
EGameStatsUploadStatus Win32UploadGameStatsBlocking
( 
	const TGameStatsParameters & rGameStatsParameters	// Input
);

class CUploadGameStats : public IUploadGameStats
{
public:

	#define GAMESTATSUPLOADER_CONNECT_RETRY_TIME	1.0

	CUploadGameStats() : m_bConnected(false), m_flNextConnectAttempt(0) {}

	//-----------------------------------------------------------------------------
	// Purpose: Initializes the connection to the CSER
	//-----------------------------------------------------------------------------
	void InitConnection( void )
	{
		AsyncUpload_Shutdown();

		m_bConnected = false;
		m_Adr.Clear();
		m_Adr.SetType( NA_IP );
		m_flNextConnectAttempt = 0;
		// don't call UpdateConnection here, does bad things
	}

	void UpdateConnection( void )
	{
		if ( m_bConnected || HostState_IsShuttingDown() )
			return;

		// try getting client SteamUtils interface
		ISteamUtils *pSteamUtils = NULL;
#ifndef SWDS
		pSteamUtils = Steam3Client().SteamUtils();
#endif
		// if that fails, try the game server SteamUtils interface
		if ( !pSteamUtils )
		{
			pSteamUtils = Steam3Server().SteamGameServerUtils();
		}

		// can't determine CSER if Steam not running
		if ( !pSteamUtils )
			return;

		float curTime = Sys_FloatTime();

		if ( curTime < m_flNextConnectAttempt )
			return;

		uint32 unIP = 0;
		uint16 usPort = 0;
#if !defined( NO_VCR )
		if ( VCRGetMode() != VCR_Playback )
#endif
		{
			pSteamUtils->GetCSERIPPort( &unIP, &usPort );
		}
#if !defined( NO_VCR )
		VCRGenericValue( "a", &unIP, sizeof( unIP ) );	
		VCRGenericValue( "b", &usPort, sizeof( usPort ) );	
#endif	

		if ( unIP == 0 )
		{
			m_flNextConnectAttempt = curTime + GAMESTATSUPLOADER_CONNECT_RETRY_TIME;
			return; 
		}
		else
		{
			m_Adr.SetIP( unIP );
			m_Adr.SetPort( usPort );
			m_Adr.SetType( NA_IP );
			m_bConnected = true;
		}
	}

	virtual bool UploadGameStats( char const *mapname,
		unsigned int blobversion, unsigned int blobsize, const void *pvBlobData )
	{
		AsyncUpload_QueueData( mapname, blobversion, blobsize, pvBlobData );
		return true;
		//return UploadGameStatsInternal( mapname, blobversion, blobsize, pvBlobData );
	}

	// If user has disabled stats tracking, do nothing
	virtual bool IsGameStatsLoggingEnabled()
	{
		if ( CommandLine()->FindParm( "-nogamestats" ) )
			return false;

#ifdef SWDS
		return true;
#else
		IRegistry *temp = InstanceRegistry( "Steam" );
		Assert( temp );
		// Check registry
		int iDisable = temp->ReadInt( "DisableGameStats", 0 );
		
		ReleaseInstancedRegistry( temp );

		if ( iDisable != 0 )
		{
			return false;
		}

		return true;
#endif
	}
    
	// Gets a non-personally identifiable unique ID for this steam user, used for tracking total gameplay time across
	//  multiple stats sessions, but isn't trackable back to their Steam account or id.
	// Buffer should be 16 bytes, ID will come back as a hexadecimal string version of a GUID
	virtual void GetPseudoUniqueId( char *buf, size_t bufsize )
	{
		Q_memset( buf, 0, bufsize );

#ifndef SWDS
		IRegistry *temp = InstanceRegistry( "Steam" );
		Assert( temp );
		// Check registry
		char const *uuid = temp->ReadString( "PseudoUUID", "" );
		
		if ( !uuid || !*uuid )
		{
			// Create a new one
#ifdef WIN32
			UUID newId;
			UuidCreate( &newId );
#elif defined(POSIX)
			uuid_t newId;
#ifdef OSX
			uuid_generate( newId );
#endif
#else
#error
#endif
			char hex[ 17 ];
			Q_memset( hex, 0, sizeof( hex ) );
			Q_binarytohex( (const byte *)&newId, sizeof( newId ), hex, sizeof( hex ) );

			// If running at Valve, copy in the users name here
			if ( Steam3Client().SteamUtils() && ( Steam3Client().SteamUser()->BLoggedOn() ) && 
				( k_EUniverseBeta == Steam3Client().SteamUtils()->GetConnectedUniverse() ) )
			{
				bool bOk = true;
				char username[ 64 ];
#if defined( _WIN32 )
				Q_memset( username, 0, sizeof( username ) );
				DWORD length = sizeof( username ) - 1;
				if ( !GetUserName( username, &length ) )
				{
					bOk = false;
				}
#else
				struct passwd *pass = getpwuid( getuid() );
				if ( pass )
				{
					Q_strncpy( username, pass->pw_name, sizeof( username ) );
				}
				else
				{
					bOk = false;
				}
				username[sizeof(username)-1] = '\0';
#endif
				if ( bOk )
				{
					int nBytesToCopy = min( (size_t)Q_strlen( username ), sizeof( hex ) - 1 );
					// NOTE:  This doesn't copy the NULL terminator from username because we want the "random" bits after the name
					Q_memcpy( hex, username, nBytesToCopy );					
				}
			}

			temp->WriteString( "PseudoUUID", hex );

			Q_strncpy( buf, hex, bufsize );
		}
		else
		{
			Q_strncpy( buf, uuid, bufsize );
		}

	    ReleaseInstancedRegistry( temp );
#endif		

		if ( ( buf[0] == 0 ) && sv.IsDedicated() )
		{
			// For Linux dedicated servers, where we won't get a unique ID: set the ID to "unknown" so we have something.  (If there's no ID,
			// stats don't get sent.)  This will later get altered to be a hash of IP&port, but this gets called early before IP is determined
			// so we can't make the hash now.
			Q_strncpy( buf, "unknown", bufsize );
		}
	}

	virtual bool IsCyberCafeUser( void )
	{
		// TODO: convert this to be aware of proper Steam3'ified cafes once we actually implement that
		return false;
	}

	// Only works in single player
	virtual bool IsHDREnabled( void )
	{
#if defined( SWDS ) || defined( _X360 )
		return false;
#else
		return g_pMaterialSystemHardwareConfig->GetHDREnabled();
#endif
	}

	bool UploadGameStatsInternal( char const *mapname,
		unsigned int blobversion, unsigned int blobsize, const void *pvBlobData )
	{
		// Attempt connection, for backwards compatibility
		UpdateConnection();

		if ( !m_bConnected )
			return false;

		unsigned int useAppId = GetSteamAppID();
		if ( useAppId == 0 )
			return false;

		TGameStatsParameters params;
		Q_memset( &params, 0, sizeof( params ) );

		params.m_ipCSERServer = m_Adr;

		params.m_uEngineBuildNumber		= build_number();
		Q_strncpy( params.m_sExecutableName, "hl2.exe", sizeof( params.m_sExecutableName ) );
		Q_FileBase( com_gamedir, params.m_sGameDirectory, sizeof( params.m_sGameDirectory ) );
		Q_FileBase( mapname, params.m_sMapName, sizeof( params.m_sMapName ) );
		params.m_uStatsBlobVersion		= blobversion;
		params.m_uStatsBlobSize			= blobsize;
		params.m_pStatsBlobData			= ( void * )pvBlobData;

		////////////////////////////////////////////////////////////////////////////
		// New protocol sorts things by Steam AppId (4/6/06 ywb)
		params.m_uAppId = useAppId;
		////////////////////////////////////////////////////////////////////////////

		EGameStatsUploadStatus result = Win32UploadGameStatsBlocking( params );
		return ( result == eGameStatsUploadSucceeded ) ? true : false;
	}
private:
	netadr_t	m_Adr;
	float		m_flNextConnectAttempt;
	bool		m_bConnected;
};

static CUploadGameStats g_UploadGameStats;
IUploadGameStats *g_pUploadGameStats = &g_UploadGameStats;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CUploadGameStats, IUploadGameStats, INTERFACEVERSION_UPLOADGAMESTATS, g_UploadGameStats );

void UpdateProgress( const TGameStatsParameters & params, char const *fmt, ... )
{
	if ( !params.m_pOptionalProgressFunc )
	{
		return;
	}

	char str[ 2048 ];
	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf( str, sizeof( str ) - 1, fmt, argptr );
	va_end( argptr );

	char outstr[ 2060 ];
	Q_snprintf( outstr, sizeof( outstr ), "(%u): %s", params.m_uProgressContext, str );

	TGameStatsProgress progress;
	Q_strncpy( progress.m_sStatus, outstr, sizeof( progress.m_sStatus ) );

	// Invoke the callback
	( *params.m_pOptionalProgressFunc )( params.m_uProgressContext, progress );
}

class CWin32UploadGameStats
{
public:
	explicit CWin32UploadGameStats( 
		const netadr_t & harvester, 
		const TGameStatsParameters & rGameStatsParameters, 
		u32 contextid );
	~CWin32UploadGameStats();

	EGameStatsUploadStatus Upload( CUtlBuffer& buf );

private:

	enum States
	{
		eCreateTCPSocket =  0,
		eConnectToHarvesterServer,
		eSendProtocolVersion,
		eReceiveProtocolOkay,
		eSendUploadCommand,
		eReceiveOKToSendFile,
		eSendWholeFile, // This could push chunks onto the wire, but we'll just use a whole buffer for now.
		eReceiveFileUploadSuccess,
		eSendGracefulClose,
		eCloseTCPSocket
	};

	bool	CreateTCPSocket( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	bool	ConnectToHarvesterServer( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	bool	SendProtocolVersion( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	bool	ReceiveProtocolOkay( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	bool	SendUploadCommand( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	bool	ReceiveOKToSendFile( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	bool	SendWholeFile( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	bool	ReceiveFileUploadSuccess( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	bool	SendGracefulClose( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	bool	CloseTCPSocket( EGameStatsUploadStatus& status, CUtlBuffer& buf );

	typedef bool ( CWin32UploadGameStats::*pfnProtocolStateHandler )( EGameStatsUploadStatus& status, CUtlBuffer& buf );
	struct FSMState_t
	{
		FSMState_t( uint f, pfnProtocolStateHandler s ) :
			first( f ),
			second( s )
		{
		}

		uint						first;
		pfnProtocolStateHandler		second;
	};

	void	AddState( uint StateIndex, pfnProtocolStateHandler handler );
	void	SetNextState( uint StateIndex );
	bool	DoBlockingReceive( uint bytesExpected, CUtlBuffer& buf );

	CUtlVector< FSMState_t >		m_States;
	uint							m_uCurrentState;
	struct sockaddr_in				m_HarvesterSockAddr;
	uint							m_SocketTCP;
	const TGameStatsParameters	&m_rCrashParameters; //lint !e1725
	u32								m_ContextID;
};

CWin32UploadGameStats::CWin32UploadGameStats( 
	const netadr_t & harvester, 
	const TGameStatsParameters & rGameStatsParameters, 
	u32 contextid ) :
	m_States(),
	m_uCurrentState( eCreateTCPSocket ),
	m_HarvesterSockAddr(),
	m_SocketTCP( 0 ),
	m_rCrashParameters( rGameStatsParameters ),
	m_ContextID( contextid )
{
	harvester.ToSockadr( (struct sockaddr *)&m_HarvesterSockAddr );

	AddState( eCreateTCPSocket, &CWin32UploadGameStats::CreateTCPSocket );
	AddState( eConnectToHarvesterServer, &CWin32UploadGameStats::ConnectToHarvesterServer );
	AddState( eSendProtocolVersion, &CWin32UploadGameStats::SendProtocolVersion );
	AddState( eReceiveProtocolOkay, &CWin32UploadGameStats::ReceiveProtocolOkay );
	AddState( eSendUploadCommand, &CWin32UploadGameStats::SendUploadCommand );
	AddState( eReceiveOKToSendFile, &CWin32UploadGameStats::ReceiveOKToSendFile );
	AddState( eSendWholeFile, &CWin32UploadGameStats::SendWholeFile );
	AddState( eReceiveFileUploadSuccess, &CWin32UploadGameStats::ReceiveFileUploadSuccess );
	AddState( eSendGracefulClose, &CWin32UploadGameStats::SendGracefulClose );
	AddState( eCloseTCPSocket, &CWin32UploadGameStats::CloseTCPSocket );
}

CWin32UploadGameStats::~CWin32UploadGameStats()
{
	if ( m_SocketTCP != 0 )
	{
		closesocket( m_SocketTCP ); //lint !e534
		m_SocketTCP = 0;
	}
}

//-----------------------------------------------------------------------------
// 
// Function:	DoBlockingReceive()
// 
//-----------------------------------------------------------------------------
bool CWin32UploadGameStats::DoBlockingReceive( uint bytesExpected, CUtlBuffer& buf )
{
	uint totalReceived = 0;

	buf.Purge();
	for ( ;; )
	{
		char temp[ 8192 ];

		int bytesReceived = recv( m_SocketTCP, temp, sizeof( temp ), 0 );
		if ( bytesReceived <= 0 )
			return false;

		buf.Put( ( const void * )temp, (u32)bytesReceived );
		totalReceived = buf.TellPut();
		if ( totalReceived >= bytesExpected )
			break;

	}
	return true;
}

void CWin32UploadGameStats::AddState( uint StateIndex, pfnProtocolStateHandler handler )
{
	FSMState_t newState( StateIndex, handler );
	m_States.AddToTail( newState );
}

EGameStatsUploadStatus CWin32UploadGameStats::Upload( CUtlBuffer& buf )
{
	UpdateProgress( m_rCrashParameters, "Commencing game stats upload connection." );

	EGameStatsUploadStatus result = eGameStatsUploadSucceeded;
	// Run the state machine
	while ( 1 )
	{
		Assert( m_States[ m_uCurrentState ].first == m_uCurrentState );
		pfnProtocolStateHandler handler = m_States[ m_uCurrentState ].second;

		if ( !(this->*handler)( result, buf ) )
		{
			return result;
		}
	}
}

void CWin32UploadGameStats::SetNextState( uint StateIndex )
{
	Assert( StateIndex > m_uCurrentState );
	m_uCurrentState = StateIndex;
}

bool CWin32UploadGameStats::CreateTCPSocket( EGameStatsUploadStatus& status, CUtlBuffer& /*buf*/ )
{
	UpdateProgress( m_rCrashParameters, "Creating game stats upload socket." );

	m_SocketTCP = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( m_SocketTCP == (uint)SOCKET_ERROR )
	{
		UpdateProgress( m_rCrashParameters, "Socket creation failed." );

		status = eGameStatsUploadFailed;
		return false;
	}

	SetNextState( eConnectToHarvesterServer );
	return true;
}

bool CWin32UploadGameStats::ConnectToHarvesterServer( EGameStatsUploadStatus& status, CUtlBuffer& /*buf*/ )
{
	UpdateProgress( m_rCrashParameters, "Connecting to game stats harvesting server." );

	if ( connect( m_SocketTCP, (const sockaddr *)&m_HarvesterSockAddr, sizeof( m_HarvesterSockAddr ) ) == SOCKET_ERROR )
	{
		UpdateProgress( m_rCrashParameters, "Connection failed." );

		status = eGameStatsConnectToCSERServerFailed;
		return false;
	}

	SetNextState( eSendProtocolVersion );
	return true;
}

bool CWin32UploadGameStats::SendProtocolVersion( EGameStatsUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rCrashParameters, "Sending game stats harvester protocol info." );
	buf.SetBigEndian( true );
	// Send protocol version
	buf.Purge();
	buf.PutInt( cuCurrentProtocolVersion );

	if ( send( m_SocketTCP, (const char *)buf.Base(), (int)buf.TellPut(), 0 ) == SOCKET_ERROR )
	{
		UpdateProgress( m_rCrashParameters, "Send failed." );

		status = eGameStatsUploadFailed;
		return false;
	}

	SetNextState( eReceiveProtocolOkay );
	return true;
}

bool CWin32UploadGameStats::ReceiveProtocolOkay( EGameStatsUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rCrashParameters, "Receiving harvesting protocol acknowledgement." );
	buf.Purge();

	// Now receive the protocol is acceptable token from the server
	if ( !DoBlockingReceive( 1, buf ) )
	{
		UpdateProgress( m_rCrashParameters, "Didn't receive protocol failure data." );

		status = eGameStatsUploadFailed;
		return false;
	}

	bool protocolokay = buf.GetChar() ? true : false;
	if ( !protocolokay )
	{
		UpdateProgress( m_rCrashParameters, "Server rejected protocol." );

		status = eGameStatsUploadFailed;
		return false;
	}

	UpdateProgress( m_rCrashParameters, "Protocol OK." );

	SetNextState( eSendUploadCommand );
	return true;
}

bool CWin32UploadGameStats::SendUploadCommand( EGameStatsUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rCrashParameters, "Sending harvesting protocol upload request." );
// Send upload command
	buf.Purge();
	
	NetworkMessageLengthPrefix_t messageSize
		(
				sizeof( Command_t )
			+	sizeof( ContextID_t )
			+	sizeof( HarvestFileCommand::FileSize_t )
			+	sizeof( HarvestFileCommand::SendMethod_t )
			+	sizeof( HarvestFileCommand::FileSize_t )
		);

	// Prefix the length to the command
	buf.PutInt( (int)messageSize ); 
	buf.PutChar( Commands::cuSendGameStats );
	buf.PutInt( (int)m_ContextID );

	buf.PutInt( (int)m_rCrashParameters.m_uStatsBlobSize );
	buf.PutInt( static_cast<HarvestFileCommand::SendMethod_t>( eSendMethodWholeRawFileNoBlocks ) );
	buf.PutInt( static_cast<HarvestFileCommand::FileSize_t>( 0 ) );

	// Send command to server
	if ( send( m_SocketTCP, (const char *)buf.Base(), (int)buf.TellPut(), 0 ) == SOCKET_ERROR )
	{
		UpdateProgress( m_rCrashParameters, "Send failed." );

		status = eGameStatsUploadFailed;
		return false;
	}

	SetNextState( eReceiveOKToSendFile );
	return true;
}

bool CWin32UploadGameStats::ReceiveOKToSendFile( EGameStatsUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rCrashParameters, "Receive game stats harvesting protocol upload permissible." );

	// Now receive the protocol is acceptable token from the server
	if ( !DoBlockingReceive( 1, buf ) )
	{
		UpdateProgress( m_rCrashParameters, "Receive failed." );
		status = eGameStatsUploadFailed;
		return false;
	}

	bool dosend = false;
	CommandResponse_t cmd = (CommandResponse_t)buf.GetChar();
	switch ( cmd )
	{
	case HarvestFileCommand::cuOkToSendFile:
		{
			dosend = true;
		}
		break;
	case HarvestFileCommand::cuFileTooBig:
	case HarvestFileCommand::cuInvalidSendMethod:
	case HarvestFileCommand::cuInvalidMaxCompressedChunkSize:
	case HarvestFileCommand::cuInvalidGameStatsContext:
	default:
		break;
	}

	if ( !dosend )
	{
		UpdateProgress( m_rCrashParameters, "Server rejected upload command." );

		status = eGameStatsUploadFailed;
		return false;
	}
	
	SetNextState( eSendWholeFile );
	return true;
}

bool CWin32UploadGameStats::SendWholeFile( EGameStatsUploadStatus& status, CUtlBuffer& /*buf*/ )
{
	UpdateProgress( m_rCrashParameters, "Uploading game stats data." );
	// Send to server
	bool bret = true;
	if ( send( m_SocketTCP, (const char *)m_rCrashParameters.m_pStatsBlobData, (int)m_rCrashParameters.m_uStatsBlobSize, 0 ) == SOCKET_ERROR )
	{
		bret = false;
		UpdateProgress( m_rCrashParameters, "Send failed." );

		status = eGameStatsUploadFailed;
	}
	else
	{
		SetNextState( eReceiveFileUploadSuccess );
	}

	return bret;
}

bool CWin32UploadGameStats::ReceiveFileUploadSuccess( EGameStatsUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rCrashParameters, "Receiving game stats upload success/fail message." );

	// Now receive the protocol is acceptable token from the server
	if ( !DoBlockingReceive( 1, buf ) )
	{
		UpdateProgress( m_rCrashParameters, "Receive failed." );

		status = eGameStatsUploadFailed;
		return false;
	}

	bool success = buf.GetChar() == 1 ? true : false;
	if ( !success )
	{
		UpdateProgress( m_rCrashParameters, "Upload failed." );

		status = eGameStatsUploadFailed;
		return false;
	}

	UpdateProgress( m_rCrashParameters, "Upload OK." );

	SetNextState( eSendGracefulClose );
	return true;
}

bool CWin32UploadGameStats::SendGracefulClose( EGameStatsUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rCrashParameters, "Closing connection to server." );

	// Now send disconnect command
	buf.Purge();

	size_t messageSize = sizeof( Command_t );

	buf.PutInt( (int)messageSize );
	buf.PutChar( Commands::cuGracefulClose );

	if ( send( m_SocketTCP, (const char *)buf.Base(), (int)buf.TellPut(), 0 ) == SOCKET_ERROR )
	{
		UpdateProgress( m_rCrashParameters, "Send failed." );

		status = eGameStatsUploadFailed;
		return false;
	}

	SetNextState( eCloseTCPSocket );
	return true;
}

bool CWin32UploadGameStats::CloseTCPSocket( EGameStatsUploadStatus& status, CUtlBuffer& /*buf*/ )
{
	UpdateProgress( m_rCrashParameters, "Closing socket, upload succeeded." );

	closesocket( m_SocketTCP );//lint !e534
	m_SocketTCP = 0;

	status = eGameStatsUploadSucceeded;
	// NOTE:  Returning false here ends the state machine!!!
	return false;
}

EGameStatsUploadStatus Win32UploadGameStatsBlocking
( 
	const TGameStatsParameters & rGameStatsParameters
)
{
	EGameStatsUploadStatus status = eGameStatsUploadSucceeded;

	CUtlBuffer buf( rGameStatsParameters.m_uStatsBlobSize + 4096 );

	UpdateProgress( rGameStatsParameters, "Creating initial report." );

	buf.SetBigEndian( false );

	buf.Purge();
	buf.PutChar( C2M_REPORT_GAMESTATISTICS );
	buf.PutChar( '\n' );
	buf.PutChar( C2M_REPORT_GAMESTATISTICS_PROTOCOL_VERSION );

	// See CSERServerProtocol.h for format

	if ( 0 ) // This is the old protocol
	{
		buf.PutInt( (int)rGameStatsParameters.m_uEngineBuildNumber );
		buf.PutString( rGameStatsParameters.m_sExecutableName );  // exe name
		buf.PutString( rGameStatsParameters.m_sGameDirectory );	 // gamedir
		buf.PutString( rGameStatsParameters.m_sMapName );
	
		buf.PutInt( (int)rGameStatsParameters.m_uStatsBlobVersion ); // game stats blob version
		buf.PutInt( (int)rGameStatsParameters.m_uStatsBlobSize );  // game stats blob size
	}
	else
	{
		buf.PutInt( (int)rGameStatsParameters.m_uAppId );
		buf.PutInt( (int)rGameStatsParameters.m_uStatsBlobSize );  // game stats blob size
	}

	CBlockingUDPSocket bcs;
	if ( !bcs.IsValid() )
	{
		return eGameStatsUploadFailed;
	}

	struct sockaddr_in sa;
	rGameStatsParameters.m_ipCSERServer.ToSockadr( (struct sockaddr *)&sa );

	UpdateProgress( rGameStatsParameters, "Sending game stats to server %s.", rGameStatsParameters.m_ipCSERServer.ToString() );

	bcs.SendSocketMessage( sa, (const u8 *)buf.Base(), buf.TellPut() ); //lint !e534

	UpdateProgress( rGameStatsParameters, "Waiting for response." );

	if ( bcs.WaitForMessage( 2.0f ) )
	{
		UpdateProgress( rGameStatsParameters, "Received response." );

		struct sockaddr_in replyaddress;
		buf.EnsureCapacity( 4096 );

		uint bytesReceived = bcs.ReceiveSocketMessage( &replyaddress, (u8 *)buf.Base(), 4096 );
		if ( bytesReceived > 0 )
		{
			// Fixup actual size
			buf.SeekPut( CUtlBuffer::SEEK_HEAD, bytesReceived );

			UpdateProgress( rGameStatsParameters, "Checking response." );

			// Parse out data
			u8 msgtype = (u8)buf.GetChar();
			if ( M2C_ACKREPORT_GAMESTATISTICS != msgtype  )
			{
				UpdateProgress( rGameStatsParameters, "Request denied, invalid message type." );
				return eGameStatsSendingGameStatsHeaderFailed;
			}
			bool validProtocol = (u8)buf.GetChar() == 1 ? true : false;
			if ( !validProtocol )
			{
				UpdateProgress( rGameStatsParameters, "Request denied, invalid message protocol." );
				return eGameStatsSendingGameStatsHeaderFailed;
			}

			u8 disposition = (u8)buf.GetChar();
			if ( GS_UPLOAD_REQESTED != disposition )
			{
				// Server doesn't want a gamestats, oh well
				UpdateProgress( rGameStatsParameters, "Stats report accepted, data upload skipped." );

				return eGameStatsUploadSucceeded;
			}

			// Read in the game stats info parameters
			u32 harvester_ip	= (u32)buf.GetInt();
			u16 harvester_port	= (u16)buf.GetShort();
			u32 dumpcontext		= (u32)buf.GetInt();

			sockaddr_in adr;
			adr.sin_family = AF_INET;
			adr.sin_port = htons( harvester_port );
#ifdef _WIN32
			adr.sin_addr.S_un.S_addr = harvester_ip;
#elif POSIX
			adr.sin_addr.s_addr = harvester_ip;
#endif

			netadr_t GameStatsHarvesterFSMIPAddress;
			GameStatsHarvesterFSMIPAddress.SetFromSockadr( (struct sockaddr *)&adr );

			UpdateProgress( rGameStatsParameters, "Server requested game stats upload to %s.", GameStatsHarvesterFSMIPAddress.ToString() );

			// Keep using the same scratch buffer for messaging
			CWin32UploadGameStats uploader( GameStatsHarvesterFSMIPAddress, rGameStatsParameters, dumpcontext );
			status = uploader.Upload( buf );
		}
	}
	else
	{
		UpdateProgress( rGameStatsParameters, "No response from server." );
	}

	return status;
}
//////////////////////////////////////////////////////////////////////////
//
// Implementation of async uploading
//

class CAsyncUploaderThread
{
public:
	CAsyncUploaderThread()
		: m_hThread( NULL ), m_bRunning( false ), m_eventQueue(false), m_eventInitShutdown(false) {}

	ThreadHandle_t m_hThread;

protected:
	
	struct DataEntry
	{
		char const *szMapName;
		uint uiBlobVersion;
		uint uiBlobSize;
		void const *pvBlob;

		DataEntry *AllocCopy() const;
		void Free() { delete [] ( (char*)this ); }
	};

	CThreadEvent m_eventQueue;
	CThreadEvent m_eventInitShutdown;
	CThreadFastMutex m_mtx;
	CUtlVector< DataEntry * > m_queue;
	bool m_bRunning;
	
	void ThreadProc();
	
	enum {
		SLEEP_ENTRY_UPLOADED	= 10 * 1000
	};

public:
	static unsigned CallbackThreadProc( void *pvParam ) { ((CAsyncUploaderThread*) pvParam)->ThreadProc(); return 0; }
	void QueueData( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob );
	void TerminateAndSelfDelete();
};

static CAsyncUploaderThread *g_pAsyncUploader = NULL;

CAsyncUploaderThread::DataEntry * CAsyncUploaderThread::DataEntry::AllocCopy() const
{
	// Find out how much memory we would need
	uint lenMapName = ( szMapName ? strlen( szMapName ) : 0 );
	uint numBytes = sizeof( DataEntry ) + uiBlobSize + lenMapName + 1;

	char *pbData = new char[ numBytes ];
	DataEntry *pNew = ( DataEntry * )( pbData );
	if ( !pNew )
		return NULL;

	pNew->uiBlobVersion = uiBlobVersion;
	pNew->uiBlobSize = uiBlobSize;
	
	char *pbWriteMapName = ( char * )( pNew + 1 );
	pNew->szMapName = pbWriteMapName;
	memcpy( pbWriteMapName, szMapName, lenMapName );
	pbWriteMapName[ lenMapName ] = 0;

	char *pbWriteBlob = pbWriteMapName + lenMapName + 1;
	pNew->pvBlob = pbWriteBlob;
	memcpy( pbWriteBlob, pvBlob, uiBlobSize );

	return pNew;
}

void CAsyncUploaderThread::QueueData( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob )
{
	// DevMsg( 3, "AsyncUploaderThread: Queue [%.*s]\n", uiBlobSize, pvBlob );

	if ( !m_hThread )
	{
		// Start the thread and wait for it to be running.
		// There is a slightly complicated two-event negotiation:
		// ThreadProc signals eventInitShutdown then waits on eventQueue
		Assert( m_bRunning == false );
		m_hThread = CreateSimpleThread( CallbackThreadProc, this );
		m_eventInitShutdown.Wait();
		Assert( m_bRunning == true );
		// At this point, both events are unsignaled and the thread
		// is in its main loop.
	}

	// Prepare for a DataEntry
	DataEntry de = { szMapName, uiBlobVersion, uiBlobSize, pvBlob };
	if ( DataEntry *pNew = de.AllocCopy() )
	{
		{
			AUTO_LOCK( m_mtx );
			m_queue.AddToTail( pNew );
		}
		m_eventQueue.Set();
	}
}

void CAsyncUploaderThread::TerminateAndSelfDelete()
{
	ThreadHandle_t hThread = m_hThread;
	if ( hThread )
	{
		m_bRunning = false;
		m_eventQueue.Set();
		m_eventInitShutdown.Set(); // MUST BE LAST MEMBER ACCESS, other thread may now delete this

		// Wait for a while for upload to finish, but don't wait forever
		ThreadJoin( hThread, 10 * 1000 );
		ReleaseThreadHandle( hThread );
	}
	else
	{
		delete this;
	}
}

void CAsyncUploaderThread::ThreadProc()
{
	bool bQueueDrained = true;
	bool bGotShutdownEvent = false;

	m_bRunning = true;
	m_eventInitShutdown.Set();

	while ( m_bRunning )
	{
		if ( bQueueDrained )
		{
			m_eventQueue.Wait();
		}

		DataEntry *pUpload = NULL;
		{
			AUTO_LOCK( m_mtx );
			if ( m_queue.Count() )
			{
				pUpload = m_queue[0];
				m_queue.Remove( 0 );
			}
			bQueueDrained = ( m_queue.Count() == 0 );
		}

		if ( m_bRunning && pUpload )
		{
			// DevMsg( 3, "AsyncUploaderThread: Uploading [%.*s]\n", pUpload->uiBlobSize, pUpload->pvBlob );

			// Attempt to upload the data until successful
			bool bSuccess = g_UploadGameStats.UploadGameStatsInternal( pUpload->szMapName, pUpload->uiBlobVersion, pUpload->uiBlobSize, pUpload->pvBlob );
			bSuccess;

			pUpload->Free();

			// After the data entry got uploaded, grab the next one
			// DevMsg( 3, "AsyncUploaderThread: Upload finished (status=%d) for data [%.*s]\n", bSuccess, pUpload->uiBlobSize, pUpload->pvBlob );
			
			// Using an event as an interruptable sleep; signaled if m_bRunning is cleared
			bGotShutdownEvent |= m_eventInitShutdown.Wait( SLEEP_ENTRY_UPLOADED );
		}
	}

	Assert( !m_bRunning );

	// Deletes self at end of execution!
	if ( !bGotShutdownEvent )
	{
		m_eventInitShutdown.Wait();
	}
	delete this;
}

void AsyncUpload_QueueData( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob )
{
	if ( !g_pAsyncUploader )
	{
		g_pAsyncUploader = new CAsyncUploaderThread;
	}
	g_pAsyncUploader->QueueData( szMapName, uiBlobVersion, uiBlobSize, pvBlob );
}

void AsyncUpload_Shutdown()
{
	if ( g_pAsyncUploader )
	{
		g_pAsyncUploader->TerminateAndSelfDelete();
		g_pAsyncUploader = NULL;
	}
}
