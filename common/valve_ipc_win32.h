//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef VALVE_IPC_WIN32
#define VALVE_IPC_WIN32
#ifdef _WIN32
#pragma once
#endif

#include <rpcdce.h>

// Fwd declarations
class CValveIpcMgr;
class CValveIpcServer;
class CValveIpcClient;
class CValveIpcChannel;

// Version of the protocol
#define VALVE_IPC_PROTOCOL_VER "1"

// Memory used for Valve IPC manager = 256 kB
#define VALVE_IPC_MGR_MEMORY 256 * 1024
// Name for Valve IPC manager
#define VALVE_IPC_MGR_NAME "VALVE_IPC_MGR_"

// Default IPC manager interaction timeout = 5 sec
#define VALVE_IPC_TIMEOUT 5 * 1000

// Valve IPC client-server pipe timeout = 5 sec
#define VALVE_IPC_CS_TIMEOUT 5 * 1000

// Valve IPC client-server pipe buffer = 64 kB
#define VALVE_IPC_CS_BUFFER 64 * 1024

#define VALVE_IPC_IMPL inline

//
// CValveIpcMgr
//		Used to discover, register, unregister IPC servers
//
//		Internally the memory is stored as:
//			[zero-terminated server name-1] [128-bit UUID]
//			[zero-terminated server name-2] [128-bit UUID]
//			 ...
//			[zero-terminated server name-N] [128-bit UUID]
//			[<empty string>] [GUID_NULL]
//
class CValveIpcMgr
{
	friend CValveIpcServer;
	friend CValveIpcClient;

protected:	// Create-able only by server/client
	CValveIpcMgr();
	~CValveIpcMgr();

public:
	BOOL Init( DWORD dwTimeout );

public:
	BOOL DiscoverServer( char const *szServerName, RPC_CSTR *pszServerUID );
	BOOL RegisterServer( char const *szServerName, RPC_CSTR *pszServerUID );
	BOOL UnregisterServer( RPC_CSTR szServerUID );

public:
	HANDLE DuplicateMemorySegmentHandle();

private:
	BOOL Shutdown();

private:
	HANDLE m_hMutex;
	HANDLE m_hMemorySegment;
	char *m_pMemory;

private:
	class Iterator
	{
	public:
		explicit Iterator( char *m_pMemory = NULL )
		{
			m_szServerName = m_pMemory ? m_pMemory : "";
			if ( *m_szServerName )
			{
				memcpy( &m_uuid, m_szServerName + strlen( m_szServerName ) + 1, sizeof( UUID ) );
			}
			else
			{
				m_uuid = GUID_NULL;
			}
		}

	public:
		BOOL IsValid() const
		{
			return m_szServerName && *m_szServerName &&
				memcmp( &m_uuid, &GUID_NULL, sizeof( UUID ) );
		}
		
		Iterator Next() const
		{
			return IsValid() ?
				Iterator( m_szServerName + strlen( m_szServerName ) + 1 + sizeof( UUID ) ) :
				Iterator( NULL );
		}

	public:
		char * WriteIntoMemory( char *pMemory ) const
		{
			size_t nLen = strlen( m_szServerName ) + 1;
			memmove( pMemory, m_szServerName, strlen( m_szServerName ) + 1 );
			memmove( pMemory + nLen, &m_uuid, sizeof( UUID ) );
			return pMemory + nLen + sizeof( UUID );
		}

	public:
		char *m_szServerName;
		UUID m_uuid;
	};
};

//
// CValveIpcServer
//		Used to host an IPC server
//
class CValveIpcServer
{
public:
	explicit CValveIpcServer( char const *szServerName );
	~CValveIpcServer();

public:
	BOOL Register();
	BOOL Unregister();

public:
	virtual BOOL ExecuteCommand( char *bufCommand, DWORD numCommandBytes, char *bufResult, DWORD &numResultBytes ) = 0;

public:
	BOOL Start();
	BOOL Stop();
	BOOL IsRunning() const { return m_hThread && m_bRunning; }

public:
	BOOL EnsureRegisteredAndRunning()
	{
		if ( IsRunning() )
			return TRUE;
		
		if ( Register() &&
			 Start() &&
			 IsRunning() )
			return TRUE;

		Unregister();
		return FALSE;
	}
	BOOL EnsureStoppedAndUnregistered()
	{
		Unregister();
		return TRUE;
	}

public:
	static DWORD WINAPI RunDelegate( LPVOID lpvParam );
	DWORD RunImpl();

protected:
	BOOL WaitForEvent();
	BOOL WaitForClient();
	BOOL WaitForCommand();
	BOOL WaitForResult();

protected:
	char *m_szServerName;
	RPC_CSTR m_szServerUID;
	
	HANDLE m_hMemorySegment;
	HANDLE m_hServerAlive;

	HANDLE m_hServerPipe;
	HANDLE m_hPipeEvent;

	HANDLE m_hThread;
	BOOL m_bRunning;

	char *m_pBufferRead;
	DWORD m_cbBufferRead;

	char *m_pBufferWrite;
	DWORD m_cbBufferWrite;
};

//
// CValveIpcClient
//		Used to discover a server and establish a comm channel
//
class CValveIpcClient
{
public:
	explicit CValveIpcClient( char const *szServerName );
	~CValveIpcClient();

public:
	BOOL Connect();
	BOOL Disconnect();

public:
	BOOL ExecuteCommand( LPVOID bufIn, DWORD numInBytes, LPVOID bufOut, DWORD &numOutBytes );

protected:
	char *m_szServerName;
	RPC_CSTR m_szServerUID;

	HANDLE m_hClientPipe;
};


#ifdef UTLBUFFER_H


class CValveIpcServerUtl : public CValveIpcServer
{
public:
	explicit CValveIpcServerUtl( char const *szServerName ) : CValveIpcServer( szServerName ) {}
	virtual BOOL ExecuteCommand( char *bufCommand, DWORD numCommandBytes, char *bufResult, DWORD &numResultBytes )
	{
		CUtlBuffer cmd( bufCommand, numCommandBytes, CUtlBuffer::READ_ONLY );
		CUtlBuffer res( bufResult, VALVE_IPC_CS_BUFFER, int( 0 ) );
		if ( !ExecuteCommand( cmd, res ) )
			return FALSE;
		numResultBytes = res.TellPut();
		return TRUE;
	}
	virtual BOOL ExecuteCommand( CUtlBuffer &cmd, CUtlBuffer &res ) = 0;
};

class CValveIpcClientUtl : public CValveIpcClient
{
public:
	explicit CValveIpcClientUtl( char const *szServerName ) : CValveIpcClient( szServerName ) {}
	using CValveIpcClient::ExecuteCommand;
	BOOL ExecuteCommand( CUtlBuffer &cmd, CUtlBuffer &res )
	{
		DWORD numResBytes = res.Size();
		if ( !ExecuteCommand( cmd.Base(), cmd.TellPut(), res.Base(), numResBytes ) )
			return FALSE;
		res.SeekPut( CUtlBuffer::SEEK_HEAD, numResBytes );
		return TRUE;
	}
};


#endif // UTLBUFFER_H





//////////////////////////////////////////////////////////////////////////
//
// Implementation section of CValveIpcMgr
//
//////////////////////////////////////////////////////////////////////////


VALVE_IPC_IMPL CValveIpcMgr::CValveIpcMgr() :
	m_hMutex( NULL ),
	m_hMemorySegment( NULL ),
	m_pMemory( NULL )
{
}

VALVE_IPC_IMPL CValveIpcMgr::~CValveIpcMgr()
{
	Shutdown();
}

VALVE_IPC_IMPL BOOL CValveIpcMgr::Init( DWORD dwTimeout )
{
	if ( m_pMemory )
		return TRUE;

	m_hMutex = ::CreateMutex( NULL, FALSE, VALVE_IPC_MGR_NAME "_MTX_" VALVE_IPC_PROTOCOL_VER  );
	DWORD dwWaitResult = m_hMutex ? ::WaitForSingleObject( m_hMutex, dwTimeout ) : WAIT_FAILED;

	if ( dwWaitResult == WAIT_OBJECT_0 ||
		 dwWaitResult == WAIT_ABANDONED_0 )
	{
		// We own the mgr segment

		m_hMemorySegment = ::CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, VALVE_IPC_MGR_MEMORY,
			VALVE_IPC_MGR_NAME "_MEM_" VALVE_IPC_PROTOCOL_VER );

		if ( m_hMemorySegment )
		{
			LPVOID lpvMemSegment = ::MapViewOfFile( m_hMemorySegment, FILE_MAP_ALL_ACCESS, 0, 0, 0 );

			if ( lpvMemSegment )
			{
				m_pMemory = ( char * ) lpvMemSegment;
				return TRUE;
			}
		}
	}

	if ( dwWaitResult == WAIT_OBJECT_0 ||
		 dwWaitResult == WAIT_ABANDONED_0 )
	{
		::ReleaseMutex( m_hMutex );
		::CloseHandle( m_hMutex );
		m_hMutex = NULL;
	}
	
	// Otherwise shutdown due to an init error
	Shutdown();
	return FALSE;
}

VALVE_IPC_IMPL BOOL CValveIpcMgr::Shutdown()
{
	if ( m_pMemory )
	{
		::UnmapViewOfFile( m_pMemory );
		m_pMemory = NULL;
	}

	if ( m_hMemorySegment )
	{
		::CloseHandle( m_hMemorySegment );
		m_hMemorySegment = NULL;
	}

	if ( m_hMutex )
	{
		::ReleaseMutex( m_hMutex );
		::CloseHandle( m_hMutex );
		m_hMutex = NULL;
	}

	return TRUE;
}

VALVE_IPC_IMPL BOOL CValveIpcMgr::DiscoverServer( char const *szServerName, RPC_CSTR *pszServerUID )
{
	if ( !szServerName || !*szServerName )
		return FALSE;

	for ( Iterator it( m_pMemory ); it.IsValid(); it = it.Next() )
	{
		if ( !stricmp( szServerName, it.m_szServerName ) )
		{
			if ( pszServerUID )
			{
				UuidToString( &it.m_uuid, pszServerUID );
			}
			return TRUE;
		}
	}
	return FALSE;
}

VALVE_IPC_IMPL BOOL CValveIpcMgr::RegisterServer( char const *szServerName, RPC_CSTR *pszServerUID )
{
	if ( !szServerName || !*szServerName )
		return FALSE;

	Iterator it( m_pMemory );
	for ( ; it.IsValid(); it = it.Next() )
	{
		if ( !stricmp( szServerName, it.m_szServerName ) )
		{
			// Server with same name already registered,
			// check if it is alive
			char chAliveName[ MAX_PATH ];
				RPC_CSTR szBaseName;
				UuidToString( &it.m_uuid, &szBaseName );
			sprintf( chAliveName, "%s" "_ALIVE_" VALVE_IPC_PROTOCOL_VER, szBaseName );
				RpcStringFree( &szBaseName );
			HANDLE hAliveTest = ::OpenMutex( MUTEX_ALL_ACCESS, FALSE, chAliveName );
			if ( hAliveTest )
			{
				::CloseHandle( hAliveTest );
				return FALSE; // Server is alive, can't register again
			}
			else
			{
				// Server is dead, re-use its UID
				if ( pszServerUID )
				{
					UuidToString( &it.m_uuid, pszServerUID );
				}
				return TRUE;
			}
		}
	}

	// Iterator points at the last element in the list, write the new server info
	Iterator itNewServer;
	itNewServer.m_szServerName = const_cast< char * >( szServerName );
	UuidCreate( &itNewServer.m_uuid );

	// Check that there's enough memory left in the storage
	char *pUpdateMemory = it.m_szServerName;
	if ( pUpdateMemory + strlen( szServerName ) + 1 + 32 + 2 * sizeof( UUID ) >
		 m_pMemory + VALVE_IPC_MGR_MEMORY )
	{
		return FALSE;
	}

	// Insert the new server in the list
	pUpdateMemory = itNewServer.WriteIntoMemory( pUpdateMemory );
	pUpdateMemory = Iterator().WriteIntoMemory( pUpdateMemory );

	if ( pszServerUID )
	{
		UuidToString( &itNewServer.m_uuid, pszServerUID );
	}

	return TRUE;
}

VALVE_IPC_IMPL BOOL CValveIpcMgr::UnregisterServer( RPC_CSTR szServerUID )
{
	if ( !szServerUID || !*szServerUID )
		return FALSE;

	RPC_STATUS rpcS;
	UUID uuid;
	if ( RPC_S_OK != UuidFromString( szServerUID, &uuid ) )
		return FALSE;

	for ( Iterator it( m_pMemory ); it.IsValid(); it = it.Next() )
	{
		if ( UuidEqual( &it.m_uuid, &uuid, &rpcS ) )
		{
			// This is our server, remove it from the list
			char *pMemoryUpdate = it.m_szServerName;
			for ( Iterator itRemaining = it.Next(); itRemaining.IsValid(); )
			{
				Iterator itNext = itRemaining.Next();
				pMemoryUpdate = itRemaining.WriteIntoMemory( pMemoryUpdate );
				itRemaining = itNext;
			}
			Iterator().WriteIntoMemory( pMemoryUpdate );

			return TRUE;
		}
	}
	return FALSE;
}

VALVE_IPC_IMPL HANDLE CValveIpcMgr::DuplicateMemorySegmentHandle()
{
	if ( !m_hMemorySegment )
		return NULL;

	HANDLE hDup = NULL;
	::DuplicateHandle( GetCurrentProcess(), m_hMemorySegment,
		GetCurrentProcess(), &hDup, DUPLICATE_SAME_ACCESS,
		FALSE, DUPLICATE_SAME_ACCESS );

	return hDup;
}



//////////////////////////////////////////////////////////////////////////
//
// Implementation section of CValveIpcServer
//
//////////////////////////////////////////////////////////////////////////

VALVE_IPC_IMPL CValveIpcServer::CValveIpcServer( char const *szServerName )
{
	// Copy server name
	size_t nLen = szServerName ? strlen( szServerName ) : 0;
	m_szServerName = new char[ nLen + 1 ];
	strcpy( m_szServerName, szServerName ? szServerName : "" );

	// Init remaining
	m_szServerUID = NULL;
	m_hMemorySegment = NULL;
	m_hServerAlive = NULL;
	m_hServerPipe = NULL;
	m_hPipeEvent = NULL;
	m_hThread = NULL;
	m_bRunning = FALSE;
	
	m_pBufferRead = NULL;
	m_cbBufferRead = 0;
	m_pBufferWrite = NULL;
	m_cbBufferWrite = 0;
}

VALVE_IPC_IMPL CValveIpcServer::~CValveIpcServer()
{
	Unregister();

	if ( m_szServerName )
	{
		delete [] m_szServerName;
		m_szServerName = NULL;
	}
}

VALVE_IPC_IMPL BOOL CValveIpcServer::Register()
{
	if ( m_szServerUID )
		return TRUE;

	CValveIpcMgr mgr;
	if ( !mgr.Init( VALVE_IPC_TIMEOUT ) )
		return FALSE;

	// Try registering the server
	if ( !mgr.RegisterServer( m_szServerName, &m_szServerUID ) )
		return FALSE;
	
	// Server got registered, duplicate memory segment handle
	m_hMemorySegment = mgr.DuplicateMemorySegmentHandle();
	
	// create the "server alive" object
	char chAliveName[ MAX_PATH ];
	sprintf( chAliveName, "%s" "_ALIVE_" VALVE_IPC_PROTOCOL_VER, m_szServerUID );
	m_hServerAlive = ::CreateMutex( NULL, FALSE, chAliveName );
	if ( !m_hServerAlive )
	{
		Unregister();
		return FALSE;
	}

	// Create the server pipe event
	m_hPipeEvent = ::CreateEvent( NULL, TRUE, FALSE, NULL );
	if ( !m_hPipeEvent )
	{
		Unregister();
		return FALSE;
	}

	// Create the server end of the pipe
	char chPipeName[ MAX_PATH ];
	sprintf( chPipeName, "\\\\.\\pipe\\" "%s" "_PIPE_" VALVE_IPC_PROTOCOL_VER, m_szServerUID  );
	m_hServerPipe = ::CreateNamedPipe(
		chPipeName,
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_WRITE_THROUGH,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
		1,
		VALVE_IPC_CS_BUFFER, VALVE_IPC_CS_BUFFER,
		VALVE_IPC_CS_TIMEOUT,
		NULL
		);
	if ( !m_hServerPipe )
	{
		Unregister();
		return FALSE;
	}

	// Allocate the pipe buffer
	m_pBufferRead = new char [ VALVE_IPC_CS_BUFFER ];
	if ( !m_pBufferRead )
	{
		Unregister();
		return FALSE;
	}
	m_pBufferWrite = new char [ VALVE_IPC_CS_BUFFER ];
	if ( !m_pBufferWrite )
	{
		Unregister();
		return FALSE;
	}

	return TRUE;
}

VALVE_IPC_IMPL BOOL CValveIpcServer::Unregister()
{
	if ( !m_szServerUID )
		return FALSE;
	else
	{
		CValveIpcMgr mgr;
		if ( mgr.Init( VALVE_IPC_TIMEOUT ) )
		{
			mgr.UnregisterServer( m_szServerUID );
		}
	}

	// Stop the server if it is running
	Stop();

	m_cbBufferRead = 0;
	delete [] m_pBufferRead;
	m_pBufferRead = NULL;

	m_cbBufferWrite = 0;
	delete [] m_pBufferWrite;
	m_pBufferWrite = NULL;

	if ( m_hServerPipe )
	{
		::CloseHandle( m_hServerPipe );
		m_hServerPipe = NULL;
	}

	if ( m_hPipeEvent )
	{
		::CloseHandle( m_hPipeEvent );
		m_hPipeEvent = NULL;
	}

	if ( m_hServerAlive )
	{
		::CloseHandle( m_hServerAlive );
		m_hServerAlive = NULL;
	}

	if ( m_hMemorySegment )
	{
		::CloseHandle( m_hMemorySegment );
		m_hMemorySegment = NULL;
	}

	if ( m_szServerUID )
	{
		RpcStringFree( &m_szServerUID );
		m_szServerUID = NULL;
	}

	return TRUE;
}

VALVE_IPC_IMPL BOOL CValveIpcServer::WaitForEvent()
{
	for ( ; m_bRunning ; )
	{
		DWORD dwWaitResult = ::WaitForSingleObject( m_hPipeEvent, 50 );
		switch ( dwWaitResult )
		{
		case WAIT_TIMEOUT:
			continue;
		
		case WAIT_OBJECT_0:
		case WAIT_ABANDONED_0:
			::ResetEvent( m_hPipeEvent );
			return m_bRunning;
		
		default:
			return FALSE;
		}
	}

	return FALSE;
}

VALVE_IPC_IMPL BOOL CValveIpcServer::WaitForClient()
{
	OVERLAPPED ov;
	memset( &ov, 0, sizeof( ov ) );
	ov.hEvent = m_hPipeEvent;
	
	BOOL bResult = ::ConnectNamedPipe( m_hServerPipe, &ov );
	if ( bResult )	// Overlapped "ConnectNamedPipe" always returns FALSE
		return FALSE;

	switch ( GetLastError() )
	{
	case ERROR_IO_PENDING:
		// Wait for client to connect
		break;

	case ERROR_PIPE_CONNECTED:
		SetEvent( ov.hEvent );
		return TRUE;

	default:
		return FALSE;
	}

	if ( !WaitForEvent() )
		return FALSE;

	DWORD dwConnectDummy;
	if ( !::GetOverlappedResult( m_hServerPipe, &ov, &dwConnectDummy, FALSE ) )
		return FALSE;

	return TRUE;
}

VALVE_IPC_IMPL BOOL CValveIpcServer::WaitForCommand()
{
	OVERLAPPED ov;
	memset( &ov, 0, sizeof( ov ) );
	ov.hEvent = m_hPipeEvent;

	m_cbBufferRead = 0;
	BOOL bRead = ::ReadFile( m_hServerPipe, m_pBufferRead, VALVE_IPC_CS_BUFFER, &m_cbBufferRead, &ov );

	if ( bRead &&
		 m_cbBufferRead )
		return TRUE;

	if ( !bRead &&
		 GetLastError() == ERROR_IO_PENDING )
	{
		if ( !WaitForEvent() )
			return FALSE;

		bRead = GetOverlappedResult( m_hServerPipe, &ov, &m_cbBufferRead, FALSE );
		if ( !bRead || !m_cbBufferRead )
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

VALVE_IPC_IMPL BOOL CValveIpcServer::WaitForResult()
{
	OVERLAPPED ov;
	memset( &ov, 0, sizeof( ov ) );
	ov.hEvent = m_hPipeEvent;

	DWORD cbWrite;
	BOOL bWrite = ::WriteFile( m_hServerPipe, m_pBufferWrite, m_cbBufferWrite, &cbWrite, &ov );
	
	if ( bWrite &&
		 cbWrite == m_cbBufferWrite )
		return TRUE;

	if ( !bWrite &&
		 GetLastError() == ERROR_IO_PENDING )
	{
		if ( !WaitForEvent() )
			return FALSE;

		bWrite = GetOverlappedResult( m_hServerPipe, &ov, &cbWrite, FALSE );
		if ( !bWrite ||
			 cbWrite != m_cbBufferWrite )
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

VALVE_IPC_IMPL DWORD WINAPI CValveIpcServer::RunDelegate( LPVOID lpvParam )
{
	return reinterpret_cast< CValveIpcServer * >( lpvParam )->RunImpl();
}

VALVE_IPC_IMPL DWORD CValveIpcServer::RunImpl()
{
	for ( ; WaitForClient() ; )
	{
		for ( ; WaitForCommand() ; )
		{
			m_cbBufferWrite = 0;
			BOOL bResult = ExecuteCommand( m_pBufferRead, m_cbBufferRead, m_pBufferWrite, m_cbBufferWrite );
			if ( !bResult || !m_cbBufferWrite )
				break;

			bResult = WaitForResult();
			if ( !bResult )
				break;
		}

		::DisconnectNamedPipe( m_hServerPipe );
	}
	
	m_bRunning = FALSE;
	return FALSE;
}

VALVE_IPC_IMPL BOOL CValveIpcServer::Start()
{
	if ( m_hThread )
		return FALSE;

	m_hThread = ::CreateThread( NULL, 0, RunDelegate, this, CREATE_SUSPENDED, NULL );
	if ( !m_hThread )
		return FALSE;

	m_bRunning = TRUE;
	::ResumeThread( m_hThread );
	
	return TRUE;
}

VALVE_IPC_IMPL BOOL CValveIpcServer::Stop()
{
	if ( !m_hThread )
		return FALSE;

	m_bRunning = FALSE;
	::WaitForSingleObject( m_hThread, INFINITE );

	::CloseHandle( m_hThread );
	m_hThread = NULL;

	return TRUE;
}


//////////////////////////////////////////////////////////////////////////
//
// Implementation section of CValveIpcClient
//
//////////////////////////////////////////////////////////////////////////


VALVE_IPC_IMPL CValveIpcClient::CValveIpcClient( char const *szServerName )
{
	// Copy server name
	size_t nLen = szServerName ? strlen( szServerName ) : 0;
	m_szServerName = new char[ nLen + 1 ];
	strcpy( m_szServerName, szServerName ? szServerName : "" );

	// Init remaining
	m_szServerUID = NULL;
	m_hClientPipe = NULL;
}

VALVE_IPC_IMPL CValveIpcClient::~CValveIpcClient()
{
	Disconnect();

	if ( m_szServerName )
	{
		delete [] m_szServerName;
		m_szServerName = NULL;
	}
}

VALVE_IPC_IMPL BOOL CValveIpcClient::Connect()
{
	if ( m_szServerUID )
		return TRUE;

	CValveIpcMgr mgr;
	if ( !mgr.Init( VALVE_IPC_TIMEOUT ) )
		return FALSE;

	// Try discovering the server
	if ( !mgr.DiscoverServer( m_szServerName, &m_szServerUID ) )
		return FALSE;

	// Server got discovered
	// check the "server alive" object
	char chAliveName[ MAX_PATH ];
	sprintf( chAliveName, "%s" "_ALIVE_" VALVE_IPC_PROTOCOL_VER, m_szServerUID );
	
	HANDLE hServerAlive = ::OpenMutex( MUTEX_ALL_ACCESS, FALSE, chAliveName );
	if ( !hServerAlive )
	{
		Disconnect();
		return FALSE;
	}
	else
	{
		::CloseHandle( hServerAlive );
		hServerAlive = NULL;
	}

	// Connect the server pipe
	char chPipeName[ MAX_PATH ];
	sprintf( chPipeName, "\\\\.\\pipe\\" "%s" "_PIPE_" VALVE_IPC_PROTOCOL_VER, m_szServerUID  );
	m_hClientPipe = ::CreateFile(
		chPipeName,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_WRITE_THROUGH,
		NULL
		);
	if ( !m_hClientPipe )
	{
		Disconnect();
		return FALSE;
	}
	
	DWORD dwPipeMode = PIPE_READMODE_MESSAGE;
	SetNamedPipeHandleState( m_hClientPipe, &dwPipeMode, NULL, NULL );

	return TRUE;
}

VALVE_IPC_IMPL BOOL CValveIpcClient::Disconnect()
{
	if ( !m_szServerUID )
		return FALSE;

	if ( m_hClientPipe )
	{
		::CloseHandle( m_hClientPipe );
		m_hClientPipe = NULL;
	}

	if ( m_szServerUID )
	{
		RpcStringFree( &m_szServerUID );
		m_szServerUID = NULL;
	}

	return TRUE;
}

VALVE_IPC_IMPL BOOL CValveIpcClient::ExecuteCommand( LPVOID bufIn, DWORD numInBytes, LPVOID bufOut, DWORD &numOutBytes )
{
	if ( !m_szServerUID || !m_hClientPipe )
		return FALSE;

	BOOL bTransact = TransactNamedPipe( m_hClientPipe,
		bufIn, numInBytes,
		bufOut, numOutBytes,
		&numOutBytes,
		NULL );
	if ( !bTransact &&
		 GetLastError() == ERROR_MORE_DATA )
	{
		bTransact = TRUE;
	}
	return bTransact;
}




#endif // #ifndef VALVE_IPC_WIN32
