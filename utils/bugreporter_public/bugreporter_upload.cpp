//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "basetypes.h"
#ifdef WIN32
#include <winsock.h>
#elif defined(POSIX)
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define closesocket close
#else
#error
#endif
#include "netadr.h"
#include "tier1/strtools.h"
#include <stdio.h>
#include <stdarg.h>
#include "utlbuffer.h"
#include "utlvector.h"
#include "filesystem_tools.h"
#include "cserserverprotocol_engine.h"
#include "mathlib/IceKey.H"
#include "bitbuf.h"
#include "blockingudpsocket.h"
#include "steamcommon.h"
#include "steam/steamclientpublic.h"

typedef unsigned int u32;
typedef unsigned char u8;
typedef unsigned short u16;

namespace BugReportHarvester
{

	enum EFileType
	{
		eFileTypeBugReport,

		eFILETYPECOUNT		// Count number of legal values
	};


	enum ESendMethod
	{
		eSendMethodWholeRawFileNoBlocks,
		eSendMethodCompressedBlocks, // TODO: Reenable compressed sending of minidumps

		eSENDMETHODCOUNT	// Count number of legal values
	};

}

using namespace BugReportHarvester;

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
	const Command_t				cuSendBugReport					= 1;
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
	const CommandResponse_t		cuInvalidBugReportContext		= 4;
	const uint							cuNumCommandResponses			= 5;
}

//#############################################################################
//
// Class declaration:	CWin32UploadBugReport
//
//#############################################################################
// 
// Authors:
//
//		Yahn Bernier
//
// Description and general notes:
//
//		Handles uploading bug report data blobs to the CSERServer
//		(Client Stats & Error Reporting Server)

typedef enum
{
	// General status
	eBugReportUploadSucceeded = 0,
	eBugReportUploadFailed,

	// Specific status
	eBugReportBadParameter,
	eBugReportUnknownStatus,
	eBugReportSendingBugReportHeaderSucceeded,
	eBugReportSendingBugReportHeaderFailed,
	eBugReportReceivingResponseSucceeded,
	eBugReportReceivingResponseFailed,
	eBugReportConnectToCSERServerSucceeded,
	eBugReportConnectToCSERServerFailed,
	eBugReportUploadingBugReportSucceeded,
	eBugReportUploadingBugReportFailed
} EBugReportUploadStatus;

struct TBugReportProgress
{
	// A text string describing the current progress
	char			m_sStatus[ 512 ];
};

typedef void ( *BUGREPORTREPORTPROGRESSFUNC )( u32 uContext, const TBugReportProgress & rBugReportProgress );

static void BugUploadProgress( u32 uContext, const TBugReportProgress & rBugReportProgress )
{
	// DevMsg( "%s\n", rBugReportProgress.m_sStatus );
}

struct TBugReportParameters
{
	// IP Address of the CSERServer to send the report to
	netadr_t				m_ipCSERServer;		

	TSteamGlobalUserID		m_userid;

	// Source Control Id (or build_number) of the product
	u32						m_uEngineBuildNumber;
	// Name of the .exe
	char					m_sExecutableName[ 64 ];
	// Game directory
	char					m_sGameDirectory[ 64 ];
	// Map name the server wants to upload statistics about
	char					m_sMapName[ 64 ];

	u32						m_uRAM;
	u32						m_uCPU;

	char					m_sProcessor[ 128 ];

	u32						m_uDXVersionHigh;
	u32						m_uDXVersionLow;
	u32						m_uDXVendorId;
	u32						m_uDXDeviceId;

	char					m_sOSVersion[ 64 ];

	char					m_sReportType[ 40 ];
	char					m_sEmail[ 80 ];
	char					m_sAccountName[ 64 ];

	char					m_sTitle[ 128 ];

	char					m_sBody[ 1024 ];

	u32						m_uAttachmentFileSize;
	char					m_sAttachmentFile[ 128 ];

	u32						m_uProgressContext;
	BUGREPORTREPORTPROGRESSFUNC	m_pOptionalProgressFunc;
};

// Note that this API is blocking, though the callback, if passed, can occur during execution.
EBugReportUploadStatus Win32UploadBugReportBlocking
( 
	const TBugReportParameters & rBugReportParameters	// Input
);

	//-----------------------------------------------------------------------------
// Purpose: encrypts an 8-byte sequence
//-----------------------------------------------------------------------------
inline void Encrypt8ByteSequence( IceKey& cipher, const unsigned char *plainText, unsigned char *cipherText)
{
	cipher.encrypt(plainText, cipherText);
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void EncryptBuffer( IceKey& cipher, unsigned char *bufData, uint bufferSize)
{
	unsigned char *cipherText = bufData;
	unsigned char *plainText = bufData;
	uint bytesEncrypted = 0;

	while (bytesEncrypted < bufferSize)
	{
		// encrypt 8 byte section
		Encrypt8ByteSequence( cipher, plainText, cipherText);
		bytesEncrypted += 8;
		cipherText += 8;
		plainText += 8;
	}
}

bool UploadBugReport(
	const netadr_t& cserIP,
	const CSteamID &userid,
	int build,
	char const *title,
	char const *body,
	char const *exename,
	char const *pchGamedir,
	char const *mapname,
	char const *reporttype,
	char const *email,
	char const *accountname,
	int ram,
	int cpu,
	char const *processor,
	unsigned int high, 
	unsigned int low, 
	unsigned int vendor, 
	unsigned int device,
	char const *osversion,
	char const *attachedfile,
	unsigned int attachedfilesize
)
{
	TBugReportParameters params;
	Q_memset( &params, 0, sizeof( params ) );

	params.m_ipCSERServer = cserIP;
	params.m_userid.m_SteamLocalUserID.As64bits = userid.ConvertToUint64();

	params.m_uEngineBuildNumber		= build;
	Q_strncpy( params.m_sExecutableName, exename, sizeof( params.m_sExecutableName ) );
	Q_strncpy( params.m_sGameDirectory, pchGamedir, sizeof( params.m_sGameDirectory ) );
	Q_strncpy( params.m_sMapName, mapname, sizeof( params.m_sMapName ) );

	params.m_uRAM = ram;
	params.m_uCPU = cpu;

	Q_strncpy( params.m_sProcessor, processor, sizeof( params.m_sProcessor) );

	params.m_uDXVersionHigh = high;
	params.m_uDXVersionLow  = low;
	params.m_uDXVendorId = vendor;
	params.m_uDXDeviceId = device;

	Q_strncpy( params.m_sOSVersion, osversion, sizeof( params.m_sOSVersion ) );

	Q_strncpy( params.m_sReportType, reporttype, sizeof( params.m_sReportType ) );
	Q_strncpy( params.m_sEmail, email, sizeof( params.m_sEmail ) );
	Q_strncpy( params.m_sAccountName, accountname, sizeof( params.m_sAccountName ) );

	Q_strncpy( params.m_sTitle, title, sizeof( params.m_sTitle ) );
	Q_strncpy( params.m_sBody, body, sizeof( params.m_sBody ) );

	Q_strncpy( params.m_sAttachmentFile, attachedfile, sizeof( params.m_sAttachmentFile ) );
	params.m_uAttachmentFileSize = attachedfilesize;

	params.m_uProgressContext = 1u;
	params.m_pOptionalProgressFunc = BugUploadProgress;

	EBugReportUploadStatus result = Win32UploadBugReportBlocking( params );
	return ( result == eBugReportUploadSucceeded ) ? true : false;

}

void UpdateProgress( const TBugReportParameters & params, char const *fmt, ... )
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

	TBugReportProgress progress;
	Q_strncpy( progress.m_sStatus, outstr, sizeof( progress.m_sStatus ) );

	// Invoke the callback
	( *params.m_pOptionalProgressFunc )( params.m_uProgressContext, progress );
}

class CWin32UploadBugReport
{
public:
	explicit CWin32UploadBugReport( 
		const netadr_t & harvester, 
		const TBugReportParameters & rBugReportParameters, 
		u32 contextid );
	~CWin32UploadBugReport();

	EBugReportUploadStatus Upload( CUtlBuffer& buf );

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

	bool	CreateTCPSocket( EBugReportUploadStatus& status, CUtlBuffer& buf );
	bool	ConnectToHarvesterServer( EBugReportUploadStatus& status, CUtlBuffer& buf );
	bool	SendProtocolVersion( EBugReportUploadStatus& status, CUtlBuffer& buf );
	bool	ReceiveProtocolOkay( EBugReportUploadStatus& status, CUtlBuffer& buf );
	bool	SendUploadCommand( EBugReportUploadStatus& status, CUtlBuffer& buf );
	bool	ReceiveOKToSendFile( EBugReportUploadStatus& status, CUtlBuffer& buf );
	bool	SendWholeFile( EBugReportUploadStatus& status, CUtlBuffer& buf );
	bool	ReceiveFileUploadSuccess( EBugReportUploadStatus& status, CUtlBuffer& buf );
	bool	SendGracefulClose( EBugReportUploadStatus& status, CUtlBuffer& buf );
	bool	CloseTCPSocket( EBugReportUploadStatus& status, CUtlBuffer& buf );

	typedef bool ( CWin32UploadBugReport::*pfnProtocolStateHandler )( EBugReportUploadStatus& status, CUtlBuffer& buf );
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
	const TBugReportParameters	&m_rBugReportParameters; //lint !e1725
	u32								m_ContextID;
};

CWin32UploadBugReport::CWin32UploadBugReport( 
	const netadr_t & harvester, 
	const TBugReportParameters & rBugReportParameters, 
	u32 contextid ) :
	m_States(),
	m_uCurrentState( eCreateTCPSocket ),
	m_HarvesterSockAddr(),
	m_SocketTCP( 0 ),
	m_rBugReportParameters( rBugReportParameters ),
	m_ContextID( contextid )
{
	harvester.ToSockadr( (struct sockaddr *)&m_HarvesterSockAddr );

	AddState( eCreateTCPSocket, &CWin32UploadBugReport::CreateTCPSocket );
	AddState( eConnectToHarvesterServer, &CWin32UploadBugReport::ConnectToHarvesterServer );
	AddState( eSendProtocolVersion, &CWin32UploadBugReport::SendProtocolVersion );
	AddState( eReceiveProtocolOkay, &CWin32UploadBugReport::ReceiveProtocolOkay );
	AddState( eSendUploadCommand, &CWin32UploadBugReport::SendUploadCommand );
	AddState( eReceiveOKToSendFile, &CWin32UploadBugReport::ReceiveOKToSendFile );
	AddState( eSendWholeFile, &CWin32UploadBugReport::SendWholeFile );
	AddState( eReceiveFileUploadSuccess, &CWin32UploadBugReport::ReceiveFileUploadSuccess );
	AddState( eSendGracefulClose, &CWin32UploadBugReport::SendGracefulClose );
	AddState( eCloseTCPSocket, &CWin32UploadBugReport::CloseTCPSocket );
}

CWin32UploadBugReport::~CWin32UploadBugReport()
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
bool CWin32UploadBugReport::DoBlockingReceive( uint bytesExpected, CUtlBuffer& buf )
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

void CWin32UploadBugReport::AddState( uint StateIndex, pfnProtocolStateHandler handler )
{
	FSMState_t newState( StateIndex, handler );
	m_States.AddToTail( newState );
}

EBugReportUploadStatus CWin32UploadBugReport::Upload( CUtlBuffer& buf )
{
	UpdateProgress( m_rBugReportParameters, "Commencing bug report upload connection." );

	EBugReportUploadStatus result = eBugReportUploadSucceeded;
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

void CWin32UploadBugReport::SetNextState( uint StateIndex )
{
	Assert( StateIndex > m_uCurrentState );
	m_uCurrentState = StateIndex;
}

bool CWin32UploadBugReport::CreateTCPSocket( EBugReportUploadStatus& status, CUtlBuffer& /*buf*/ )
{
	UpdateProgress( m_rBugReportParameters, "Creating bug report upload socket." );

	m_SocketTCP = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( m_SocketTCP == (uint)SOCKET_ERROR )
	{
		UpdateProgress( m_rBugReportParameters, "Socket creation failed." );

		status = eBugReportUploadFailed;
		return false;
	}

	SetNextState( eConnectToHarvesterServer );
	return true;
}

bool CWin32UploadBugReport::ConnectToHarvesterServer( EBugReportUploadStatus& status, CUtlBuffer& /*buf*/ )
{
	UpdateProgress( m_rBugReportParameters, "Connecting to bug report harvesting server." );

	if ( connect( m_SocketTCP, (const sockaddr *)&m_HarvesterSockAddr, sizeof( m_HarvesterSockAddr ) ) == SOCKET_ERROR )
	{
		UpdateProgress( m_rBugReportParameters, "Connection failed." );

		status = eBugReportConnectToCSERServerFailed;
		return false;
	}

	SetNextState( eSendProtocolVersion );
	return true;
}

bool CWin32UploadBugReport::SendProtocolVersion( EBugReportUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rBugReportParameters, "Sending bug report harvester protocol info." );
	buf.SetBigEndian( true );
	// Send protocol version
	buf.Purge();
	buf.PutInt( cuCurrentProtocolVersion );

	if ( send( m_SocketTCP, (const char *)buf.Base(), (int)buf.TellPut(), 0 ) == SOCKET_ERROR )
	{
		UpdateProgress( m_rBugReportParameters, "Send failed." );

		status = eBugReportUploadFailed;
		return false;
	}

	SetNextState( eReceiveProtocolOkay );
	return true;
}

bool CWin32UploadBugReport::ReceiveProtocolOkay( EBugReportUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rBugReportParameters, "Receiving harvesting protocol acknowledgement." );
	buf.Purge();

	// Now receive the protocol is acceptable token from the server
	if ( !DoBlockingReceive( 1, buf ) )
	{
		UpdateProgress( m_rBugReportParameters, "Didn't receive protocol failure data." );

		status = eBugReportUploadFailed;
		return false;
	}

	bool protocolokay = buf.GetChar() ? true : false;
	if ( !protocolokay )
	{
		UpdateProgress( m_rBugReportParameters, "Server rejected protocol." );

		status = eBugReportUploadFailed;
		return false;
	}

	UpdateProgress( m_rBugReportParameters, "Protocol OK." );

	SetNextState( eSendUploadCommand );
	return true;
}

bool CWin32UploadBugReport::SendUploadCommand( EBugReportUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rBugReportParameters, "Sending harvesting protocol upload request." );
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
	buf.PutChar( Commands::cuSendBugReport );
	buf.PutInt( (int)m_ContextID );

	buf.PutInt( (int)m_rBugReportParameters.m_uAttachmentFileSize );
	buf.PutInt( static_cast<HarvestFileCommand::SendMethod_t>( eSendMethodWholeRawFileNoBlocks ) );
	buf.PutInt( static_cast<HarvestFileCommand::FileSize_t>( 0 ) );

	// Send command to server
	if ( send( m_SocketTCP, (const char *)buf.Base(), (int)buf.TellPut(), 0 ) == SOCKET_ERROR )
	{
		UpdateProgress( m_rBugReportParameters, "Send failed." );

		status = eBugReportUploadFailed;
		return false;
	}

	SetNextState( eReceiveOKToSendFile );
	return true;
}

bool CWin32UploadBugReport::ReceiveOKToSendFile( EBugReportUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rBugReportParameters, "Receive bug report harvesting protocol upload permissible." );

	// Now receive the protocol is acceptable token from the server
	if ( !DoBlockingReceive( 1, buf ) )
	{
		UpdateProgress( m_rBugReportParameters, "Receive failed." );
		status = eBugReportUploadFailed;
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
	case HarvestFileCommand::cuInvalidBugReportContext:
	default:
		break;
	}

	if ( !dosend )
	{
		UpdateProgress( m_rBugReportParameters, "Server rejected upload command." );

		status = eBugReportUploadFailed;
		return false;
	}
	
	SetNextState( eSendWholeFile );
	return true;
}

bool CWin32UploadBugReport::SendWholeFile( EBugReportUploadStatus& status, CUtlBuffer& /*buf*/ )
{
	UpdateProgress( m_rBugReportParameters, "Uploading bug report data." );
	// Send to server
	char *filebuf = NULL;
	size_t sizeactual = g_pFileSystem->Size( m_rBugReportParameters.m_sAttachmentFile );
	if ( sizeactual > 0 )
	{
		filebuf = new char[ sizeactual + 1 ];
		if ( filebuf )
		{
			FileHandle_t fh;
			fh = g_pFileSystem->Open(  m_rBugReportParameters.m_sAttachmentFile, "rb" );
			if ( FILESYSTEM_INVALID_HANDLE != fh )
			{		
				g_pFileSystem->Read( (void *)filebuf, sizeactual, fh );

				g_pFileSystem->Close( fh );
			}
			filebuf[ sizeactual ] = 0;
		}
	}
	if ( !sizeactual || !filebuf )
	{
		UpdateProgress( m_rBugReportParameters, "bug .zip file size zero or unable to allocate memory for file." );

		status = eBugReportUploadFailed;
		if ( filebuf )
		{
			delete[] filebuf;
		}
		return false;
	}

	// Send to server
	bool bret = true;
	if ( send( m_SocketTCP, filebuf, (int)sizeactual, 0 ) == SOCKET_ERROR )
	{
		bret = false;
		UpdateProgress( m_rBugReportParameters, "Send failed." );

		status = eBugReportUploadFailed;
	}
	else
	{
		SetNextState( eReceiveFileUploadSuccess );
	}

	delete[] filebuf;

	return bret;
}

bool CWin32UploadBugReport::ReceiveFileUploadSuccess( EBugReportUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rBugReportParameters, "Receiving bug report upload success/fail message." );

	// Now receive the protocol is acceptable token from the server
	if ( !DoBlockingReceive( 1, buf ) )
	{
		UpdateProgress( m_rBugReportParameters, "Receive failed." );

		status = eBugReportUploadFailed;
		return false;
	}

	bool success = buf.GetChar() == 1 ? true : false;
	if ( !success )
	{
		UpdateProgress( m_rBugReportParameters, "Upload failed." );

		status = eBugReportUploadFailed;
		return false;
	}

	UpdateProgress( m_rBugReportParameters, "Upload OK." );

	SetNextState( eSendGracefulClose );
	return true;
}

bool CWin32UploadBugReport::SendGracefulClose( EBugReportUploadStatus& status, CUtlBuffer& buf )
{
	UpdateProgress( m_rBugReportParameters, "Closing connection to server." );

	// Now send disconnect command
	buf.Purge();

	size_t messageSize = sizeof( Command_t );

	buf.PutInt( (int)messageSize );
	buf.PutChar( Commands::cuGracefulClose );

	if ( send( m_SocketTCP, (const char *)buf.Base(), (int)buf.TellPut(), 0 ) == SOCKET_ERROR )
	{
		UpdateProgress( m_rBugReportParameters, "Send failed." );

		status = eBugReportUploadFailed;
		return false;
	}

	SetNextState( eCloseTCPSocket );
	return true;
}

bool CWin32UploadBugReport::CloseTCPSocket( EBugReportUploadStatus& status, CUtlBuffer& /*buf*/ )
{
	UpdateProgress( m_rBugReportParameters, "Closing socket, upload succeeded." );

	closesocket( m_SocketTCP );//lint !e534
	m_SocketTCP = 0;

	status = eBugReportUploadSucceeded;
	// NOTE:  Returning false here ends the state machine!!!
	return false;
}

EBugReportUploadStatus Win32UploadBugReportBlocking
( 
	const TBugReportParameters & rBugReportParameters
)
{
	EBugReportUploadStatus status = eBugReportUploadFailed;

	CUtlBuffer buf( 2048 );

	UpdateProgress( rBugReportParameters, "Creating initial report." );

	buf.SetBigEndian( false );

	buf.Purge();
	buf.PutChar( C2M_BUGREPORT );
	buf.PutChar( '\n' );
	buf.PutChar( C2M_BUGREPORT_PROTOCOL_VERSION );

	// See CSERServerProtocol.h for format

	// encryption object
	IceKey cipher(1); /* medium encryption level */
	unsigned char ucEncryptionKey[8] = { 200,145,10,149,195,190,108,243 };

	cipher.set( ucEncryptionKey );

	CUtlBuffer encrypted( 2000 );

	u8 corruption_identifier = 0x01;

	encrypted.PutChar( corruption_identifier );

	encrypted.PutInt( rBugReportParameters.m_uEngineBuildNumber ); // build_identifier

	encrypted.PutString( rBugReportParameters.m_sExecutableName );
	encrypted.PutString( rBugReportParameters.m_sGameDirectory );
	encrypted.PutString( rBugReportParameters.m_sMapName );

	encrypted.PutInt( rBugReportParameters.m_uRAM );					// ram mb
	encrypted.PutInt( rBugReportParameters.m_uCPU );		// cpu mhz

	encrypted.PutString( rBugReportParameters.m_sProcessor );

	encrypted.PutInt( rBugReportParameters.m_uDXVersionHigh );			// driver version high part
	encrypted.PutInt( rBugReportParameters.m_uDXVersionLow );			// driver version low part
	encrypted.PutInt( rBugReportParameters.m_uDXVendorId );			// dxvendor id
	encrypted.PutInt( rBugReportParameters.m_uDXDeviceId );			// dxdevice id

	encrypted.PutString( rBugReportParameters.m_sOSVersion );

	encrypted.PutInt( rBugReportParameters.m_uAttachmentFileSize );

	// protocol version 2 stuff
	{
		encrypted.PutString( rBugReportParameters.m_sReportType );
		encrypted.PutString( rBugReportParameters.m_sEmail );
		encrypted.PutString( rBugReportParameters.m_sAccountName );
	}

	// protocol version 3 stuff
	{
		encrypted.Put( &rBugReportParameters.m_userid, sizeof( rBugReportParameters.m_userid ) );
	}

	encrypted.PutString( rBugReportParameters.m_sTitle );

	int bodylen = Q_strlen( rBugReportParameters.m_sBody ) + 1;

	encrypted.PutInt( bodylen );
	encrypted.Put( rBugReportParameters.m_sBody, bodylen );

	while ( encrypted.TellPut() % 8 )
	{
		encrypted.PutChar( 0 );
	}

	EncryptBuffer( cipher, (unsigned char *)encrypted.Base(), encrypted.TellPut() );

	buf.PutShort( (int)encrypted.TellPut() );
	buf.Put( (unsigned char *)encrypted.Base(), encrypted.TellPut() );

	CBlockingUDPSocket bcs;
	if ( !bcs.IsValid() )
	{
		return eBugReportUploadFailed;
	}

	struct sockaddr_in sa;
	rBugReportParameters.m_ipCSERServer.ToSockadr( (struct sockaddr *)&sa );

	UpdateProgress( rBugReportParameters, "Sending bug report to server." );

	bcs.SendSocketMessage( sa, (const u8 *)buf.Base(), buf.TellPut() ); //lint !e534

	UpdateProgress( rBugReportParameters, "Waiting for response." );

	if ( bcs.WaitForMessage( 2.0f ) )
	{
		UpdateProgress( rBugReportParameters, "Received response." );

		struct sockaddr_in replyaddress;
		buf.EnsureCapacity( 2048 );

		uint bytesReceived = bcs.ReceiveSocketMessage( &replyaddress, (u8 *)buf.Base(), 2048 );
		if ( bytesReceived > 0 )
		{
			// Fixup actual size
			buf.SeekPut( CUtlBuffer::SEEK_HEAD, bytesReceived );

			UpdateProgress( rBugReportParameters, "Checking response." );

			// Parse out data
			u8 msgtype = (u8)buf.GetChar();
			if ( M2C_ACKBUGREPORT != msgtype  )
			{
				UpdateProgress( rBugReportParameters, "Request denied, invalid message type." );
				return eBugReportSendingBugReportHeaderFailed;
			}
			bool validProtocol = (u8)buf.GetChar() == 1 ? true : false;
			if ( !validProtocol )
			{
				UpdateProgress( rBugReportParameters, "Request denied, invalid message protocol." );
				return eBugReportSendingBugReportHeaderFailed;
			}

			u8 disposition = (u8)buf.GetChar();
			if ( BR_REQEST_FILES != disposition )
			{
				// Server doesn't want a bug report, oh well
				if ( rBugReportParameters.m_uAttachmentFileSize > 0 )
				{
					UpdateProgress( rBugReportParameters, "Bug report accepted, attachment rejected (server too busy)" );
				}
				else
				{
					UpdateProgress( rBugReportParameters, "Bug report accepted." );
				}

				return eBugReportUploadSucceeded;
			}

			// Read in the bug report info parameters
			u32 harvester_ip	= (u32)buf.GetInt();
			u16 harvester_port	= (u16)buf.GetShort();
			u32 dumpcontext		= (u32)buf.GetInt();

			sockaddr_in adr;
			adr.sin_family = AF_INET;
			adr.sin_port = htons( harvester_port );
#ifdef WIN32
			adr.sin_addr.S_un.S_addr = harvester_ip;
#else
			adr.sin_addr.s_addr = harvester_ip;			
#endif
			netadr_t BugReportHarvesterFSMIPAddress;
			BugReportHarvesterFSMIPAddress.SetFromSockadr( (struct sockaddr *)&adr );

			UpdateProgress( rBugReportParameters, "Server requested bug report upload." );

			// Keep using the same scratch buffer for messaging
			CWin32UploadBugReport uploader( BugReportHarvesterFSMIPAddress, rBugReportParameters, dumpcontext );
			status = uploader.Upload( buf );
		}
	}
	else
	{
		UpdateProgress( rBugReportParameters, "No response from server." );
	}

	return status;
}
