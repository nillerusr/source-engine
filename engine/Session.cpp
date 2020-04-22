//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "Session.h"
#include "strtools.h"
#include "matchmaking.h"
#include "utllinkedlist.h"
#include "tslist.h"
#include "hl2orange.spa.h"
#include "dbg.h"

#ifdef IS_WINDOWS_PC
#include "winlite.h"	// for CloseHandle()
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IXboxSystem *g_pXboxSystem;

#define ASYNC_OK	0
#define ASYNC_FAIL	1

//-----------------------------------------------------------------------------
// Purpose: CSession class
//-----------------------------------------------------------------------------
CSession::CSession()
{
	m_pParent				= NULL;
	m_hSession				= INVALID_HANDLE_VALUE;
	m_SessionState			= SESSION_STATE_NONE;
	m_pRegistrationResults	= NULL;

	ResetSession();
}

CSession::~CSession()
{
	ResetSession();
}

//-----------------------------------------------------------------------------
// Purpose: Reset a session to it's initial state
//-----------------------------------------------------------------------------
void CSession::ResetSession()
{
	// Cleanup first
	switch( m_SessionState )
	{
	case SESSION_STATE_CREATING:
		CancelCreateSession();
		break;

	case SESSION_STATE_MIGRATING:
		// X360TBD:
		break;
	}

	if ( m_hSession != INVALID_HANDLE_VALUE )
	{
		Msg( "ResetSession: Destroying current session.\n" );

		DestroySession();
		m_hSession = INVALID_HANDLE_VALUE;
	}

	SwitchToState( SESSION_STATE_NONE );

	m_bIsHost		= false;
	m_bIsArbitrated	= false;
	m_bUsingQoS		= false;
	m_bIsSystemLink = false;
	Q_memset( &m_nPlayerSlots, 0, sizeof( m_nPlayerSlots ) );
	Q_memset( &m_SessionInfo, 0, sizeof( m_SessionInfo ) );

	if ( m_pRegistrationResults )
	{
		delete m_pRegistrationResults;
	}

	m_nSessionFlags	= 0;
}

//-----------------------------------------------------------------------------
// Purpose: Set session contexts and properties
//-----------------------------------------------------------------------------
void CSession::SetContext( const uint nContextId, const uint nContextValue, const bool bAsync )
{
	g_pXboxSystem->UserSetContext( XBX_GetPrimaryUserId(), nContextId, nContextValue, bAsync );
}
void CSession::SetProperty( const uint nPropertyId, const uint cbValue, const void *pvValue, const bool bAsync )
{
	g_pXboxSystem->UserSetProperty( XBX_GetPrimaryUserId(), nPropertyId, cbValue, pvValue, bAsync );
}

//-----------------------------------------------------------------------------
// Purpose: Send a notification to GameUI
//-----------------------------------------------------------------------------
void CSession::SendNotification( SESSION_NOTIFY notification )
{
	Assert( m_pParent );
	m_pParent->SessionNotification( notification );
}

//-----------------------------------------------------------------------------
// Purpose: Update the number of player slots filled
//-----------------------------------------------------------------------------
void CSession::UpdateSlots( const CClientInfo *pClient, bool bAddPlayers )
{
	int cPlayers = pClient->m_cPlayers;
	if ( bAddPlayers )
	{
		if ( pClient->m_bInvited )
		{
			// Fill private slots first, then overflow into public slots
			m_nPlayerSlots[SLOTS_FILLEDPRIVATE] += cPlayers;
			pClient->m_numPrivateSlotsUsed = cPlayers;
			cPlayers = 0;
			if ( m_nPlayerSlots[SLOTS_FILLEDPRIVATE] > m_nPlayerSlots[SLOTS_TOTALPRIVATE] )
			{
				cPlayers = m_nPlayerSlots[SLOTS_FILLEDPRIVATE] - m_nPlayerSlots[SLOTS_TOTALPRIVATE];
				pClient->m_numPrivateSlotsUsed -= cPlayers;
				m_nPlayerSlots[SLOTS_FILLEDPRIVATE] = m_nPlayerSlots[SLOTS_TOTALPRIVATE];
			}
		}
		m_nPlayerSlots[SLOTS_FILLEDPUBLIC] += cPlayers;
		if ( m_nPlayerSlots[SLOTS_FILLEDPUBLIC] > m_nPlayerSlots[SLOTS_TOTALPUBLIC] )
		{
			//Handle error
			Warning( "Too many players!\n" );
		}
	}
	else
	{
		// The cast to 'int' is needed since otherwise underflow will wrap around to very large
		// numbers and the 'max' macro will do nothing.
		m_nPlayerSlots[SLOTS_FILLEDPRIVATE] = max( 0, (int)( m_nPlayerSlots[SLOTS_FILLEDPRIVATE] - pClient->m_numPrivateSlotsUsed ) );
		m_nPlayerSlots[SLOTS_FILLEDPUBLIC]  = max( 0, (int)( m_nPlayerSlots[SLOTS_FILLEDPUBLIC]  - ( pClient->m_cPlayers - pClient->m_numPrivateSlotsUsed ) ) );
	}

}

//-----------------------------------------------------------------------------
// Purpose: Join players on the local client
//-----------------------------------------------------------------------------
void CSession::JoinLocal( const CClientInfo *pClient )
{
	uint  nUserIndex[MAX_PLAYERS_PER_CLIENT] = {0};
	bool  bPrivate[MAX_PLAYERS_PER_CLIENT] = {0};

	for( int i = 0; i < pClient->m_cPlayers; ++i )
	{
		nUserIndex[i] = pClient->m_iControllers[i];
		bPrivate[i] = pClient->m_bInvited;
	}

	// X360TBD: Make async?
	uint ret = g_pXboxSystem->SessionJoinLocal( m_hSession, pClient->m_cPlayers, nUserIndex, bPrivate, false );
	if ( ret != ERROR_SUCCESS )
	{
		// Handle error
		Warning( "Failed to add local players\n" );
	}
	else
	{
		UpdateSlots( pClient, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Join players on a remote client
//-----------------------------------------------------------------------------
void CSession::JoinRemote( const CClientInfo *pClient )
{
	XUID  xuids[MAX_PLAYERS_PER_CLIENT] = {0};
	bool  bPrivate[MAX_PLAYERS_PER_CLIENT] = {0};

	for( int i = 0; i < pClient->m_cPlayers; ++i )
	{
		xuids[i] = pClient->m_xuids[i];
		bPrivate[i] = pClient->m_bInvited;
	}

	// X360TBD: Make async?
	uint ret = g_pXboxSystem->SessionJoinRemote( m_hSession, pClient->m_cPlayers, xuids, bPrivate, false );
	if ( ret != ERROR_SUCCESS )
	{
		// Handle error
		Warning( "Join Remote Error\n" );
	}
	else
	{
		UpdateSlots( pClient, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove players on the local client
//-----------------------------------------------------------------------------
void CSession::RemoveLocal( const CClientInfo *pClient )
{
	uint  nUserIndex[MAX_PLAYERS_PER_CLIENT] = {0};

	for( int i = 0; i < pClient->m_cPlayers; ++i )
	{
		nUserIndex[i] = pClient->m_iControllers[i];
	}

	// X360TBD: Make async?
	uint ret = g_pXboxSystem->SessionLeaveLocal( m_hSession, pClient->m_cPlayers, nUserIndex, false );
	if ( ret != ERROR_SUCCESS )
	{
		// Handle error
		Warning( "Failed to remove local players\n" );
	}
	else
	{
		UpdateSlots( pClient, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove players on a remote client
//-----------------------------------------------------------------------------
void CSession::RemoveRemote( const CClientInfo *pClient )
{
	XUID  xuids[MAX_PLAYERS_PER_CLIENT] = {0};

	for( int i = 0; i < pClient->m_cPlayers; ++i )
	{
		xuids[i] = pClient->m_xuids[i];
	}

	// X360TBD: Make async?
	uint ret = g_pXboxSystem->SessionLeaveRemote( m_hSession, pClient->m_cPlayers, xuids, false );
	if ( ret != ERROR_SUCCESS )
	{
		// Handle error
		Warning( "Failed to remove remote players\n" );
	}
	else
	{
		UpdateSlots( pClient, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create a new session
//-----------------------------------------------------------------------------
bool CSession::CreateSession()
{
	if( INVALID_HANDLE_VALUE != m_hSession )
	{
		Warning( "CreateSession called on existing session!" );
		DestroySession();
		m_hSession = INVALID_HANDLE_VALUE;
	}

	uint flags = m_nSessionFlags;
	if( m_bIsHost )
	{
		flags |= XSESSION_CREATE_HOST;
	}

	if ( flags & XSESSION_CREATE_USES_ARBITRATION )
	{
		m_bIsArbitrated = true;
	}

	m_hCreateHandle = g_pXboxSystem->CreateAsyncHandle();

	// Create the session
 	uint ret = g_pXboxSystem->CreateSession( flags,
											 XBX_GetPrimaryUserId(),
											 m_nPlayerSlots[SLOTS_TOTALPUBLIC],
											 m_nPlayerSlots[SLOTS_TOTALPRIVATE],
											 &m_SessionNonce,
											 &m_SessionInfo,
											 &m_hSession,
											 true,
											 &m_hCreateHandle );

	if( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING )
	{
		Warning( "XSessionCreate failed with error %d\n", ret );
		return false;
	}

	SwitchToState( SESSION_STATE_CREATING );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check for completion while creating a new session
//-----------------------------------------------------------------------------
void CSession::UpdateCreating()
{
	DWORD ret = g_pXboxSystem->GetOverlappedResult( m_hCreateHandle, NULL, false );
	if ( ret == ERROR_IO_INCOMPLETE )
	{
		// Still waiting
		return;
	}
	else
	{
		SESSION_STATE nextState = SESSION_STATE_IDLE;

		// Operation completed
		SESSION_NOTIFY notification = IsHost() ? SESSION_NOTIFY_CREATED_HOST : SESSION_NOTIFY_CREATED_CLIENT;
		if ( ret != ERROR_SUCCESS )
		{
			Warning( "CSession: CreateSession failed. Error %d\n", ret );

			nextState = SESSION_STATE_NONE;
			notification = SESSION_NOTIFY_FAIL_CREATE;
		}

		g_pXboxSystem->ReleaseAsyncHandle( m_hCreateHandle );

		SendNotification( notification );
		SwitchToState( nextState );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cancel async session creation
//-----------------------------------------------------------------------------
void CSession::CancelCreateSession()
{
	if ( m_SessionState != SESSION_STATE_CREATING )
		return;

	g_pXboxSystem->CancelOverlappedOperation( &m_hCreateHandle );
	g_pXboxSystem->ReleaseAsyncHandle( m_hCreateHandle );

#ifndef POSIX
	if( INVALID_HANDLE_VALUE != m_hSession )
	{
		CloseHandle( m_hSession );
		m_hSession = INVALID_HANDLE_VALUE;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Close an existing session
//-----------------------------------------------------------------------------
void CSession::DestroySession()
{	
	// TODO: Make this async
	uint ret = g_pXboxSystem->DeleteSession( m_hSession, false );
	if ( ret != ERROR_SUCCESS )
	{
		Warning( "Failed to delete session with error %d.\n", ret );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Register for arbritation in a ranked match
//-----------------------------------------------------------------------------
void CSession::RegisterForArbitration()
{
	uint bytes = 0;
	m_pRegistrationResults = NULL;

	// Call once to determine size of results buffer
	int ret = g_pXboxSystem->SessionArbitrationRegister( m_hSession, 0, m_SessionNonce, &bytes, NULL, false );
	if ( ret != ERROR_INSUFFICIENT_BUFFER )
	{
		Warning( "Failed registering for arbitration\n" );
		return;
	}

	m_pRegistrationResults = (XSESSION_REGISTRATION_RESULTS*)new byte[ bytes ];

	m_hRegisterHandle = g_pXboxSystem->CreateAsyncHandle();

	ret = g_pXboxSystem->SessionArbitrationRegister( m_hSession, 0, m_SessionNonce, &bytes, m_pRegistrationResults, true, &m_hRegisterHandle );
	if( ret != ERROR_IO_PENDING )
	{
		Warning( "Failed registering for arbitration\n" );
	}

	m_SessionState = SESSION_STATE_REGISTERING;
}

//-----------------------------------------------------------------------------
// Purpose: Check for completion of arbitration registration
//-----------------------------------------------------------------------------
void CSession::UpdateRegistering()
{
	DWORD ret = g_pXboxSystem->GetOverlappedResult( m_hRegisterHandle, NULL, false );
	if ( ret == ERROR_IO_INCOMPLETE )
	{
		// Still waiting
		return;
	}
	else
	{
		SESSION_STATE nextState = SESSION_STATE_IDLE;

		// Operation completed
		SESSION_NOTIFY notification = SESSION_NOTIFY_REGISTER_COMPLETED;
		if ( ret != ERROR_SUCCESS )
		{
			Warning( "CSession: Registration failed. Error %d\n", ret );

			nextState = SESSION_STATE_NONE;
			notification = SESSION_NOTIFY_FAIL_REGISTER;
		}

		g_pXboxSystem->ReleaseAsyncHandle( m_hRegisterHandle );

		SendNotification( notification );
		SwitchToState( nextState );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Migrate the session to a new host
//-----------------------------------------------------------------------------
bool CSession::MigrateHost()
{
	if ( IsHost() )
	{
		// Migrate call will fill this in for us
		Q_memcpy( &m_NewSessionInfo, &m_SessionInfo, sizeof( m_NewSessionInfo ) );
	}

	m_hMigrateHandle = g_pXboxSystem->CreateAsyncHandle();

	int ret = g_pXboxSystem->SessionMigrate( m_hSession, m_nOwnerId, &m_NewSessionInfo, true, &m_hMigrateHandle );
	if ( ret != ERROR_IO_PENDING )
	{
		return false;
	}

	SwitchToState( SESSION_STATE_MIGRATING );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check for completion while migrating a session
//-----------------------------------------------------------------------------
void CSession::UpdateMigrating()
{
	DWORD ret = g_pXboxSystem->GetOverlappedResult( m_hMigrateHandle, NULL, false );
	if ( ret == ERROR_IO_INCOMPLETE )
	{
		// Still waiting
		return;
	}
	else
	{
		SESSION_STATE nextState = SESSION_STATE_IDLE;

		// Operation completed
		SESSION_NOTIFY notification = SESSION_NOTIFY_MIGRATION_COMPLETED;
		if ( ret != ERROR_SUCCESS )
		{
			Warning( "CSession: MigrateSession failed. Error %d\n", ret );

			nextState = SESSION_STATE_NONE;
			notification = SESSION_NOTIFY_FAIL_MIGRATE;
		}

		g_pXboxSystem->ReleaseAsyncHandle( m_hMigrateHandle );

		SendNotification( notification );
		SwitchToState( nextState );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Change state
//-----------------------------------------------------------------------------
void CSession::SwitchToState( SESSION_STATE newState )
{
	m_SessionState = newState;
}

//-----------------------------------------------------------------------------
// Purpose: Per-frame update
//-----------------------------------------------------------------------------
void CSession::RunFrame()
{
 	switch( m_SessionState )
 	{
 	case SESSION_STATE_CREATING:
		UpdateCreating();
 		break;
 
	case SESSION_STATE_REGISTERING:
		UpdateRegistering();
		break;

	case SESSION_STATE_MIGRATING:
		UpdateMigrating();
		break;

 	default:
 		break;
 	}
}

//-----------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------
HANDLE CSession::GetSessionHandle()
{
	return m_hSession;
}
void CSession::SetSessionInfo( XSESSION_INFO *pInfo )
{
	memcpy( &m_SessionInfo, pInfo, sizeof( XSESSION_INFO ) );
}
void CSession::SetNewSessionInfo( XSESSION_INFO *pInfo )
{
	memcpy( &m_NewSessionInfo, pInfo, sizeof( XSESSION_INFO ) );
}
void CSession::GetSessionInfo( XSESSION_INFO *pInfo )
{
	Assert( pInfo );
	memcpy( pInfo, &m_SessionInfo, sizeof( XSESSION_INFO ) );
}
void CSession::GetNewSessionInfo( XSESSION_INFO *pInfo )
{
	Assert( pInfo );
	memcpy( pInfo, &m_NewSessionInfo, sizeof( XSESSION_INFO ) );
}
void CSession::SetSessionNonce( int64 nonce )
{
	m_SessionNonce = nonce;
}
uint64 CSession::GetSessionNonce()
{
	return m_SessionNonce;
}
XNKID CSession::GetSessionId()
{
	return m_SessionInfo.sessionID;
}
void CSession::SetSessionSlots(unsigned int nSlot, unsigned int nPlayers)
{
	Assert( nSlot < SLOTS_LAST );
	m_nPlayerSlots[nSlot] = nPlayers;
}
unsigned int CSession::GetSessionSlots( unsigned int nSlot )
{
	Assert( nSlot < SLOTS_LAST ); 
	return m_nPlayerSlots[nSlot];
}
void CSession::SetSessionFlags( uint flags )
{
	m_nSessionFlags = flags;
}
uint CSession::GetSessionFlags()
{
	return m_nSessionFlags;
}
int CSession::GetPlayerCount()
{
	return m_nPlayerSlots[SLOTS_FILLEDPRIVATE] + m_nPlayerSlots[SLOTS_FILLEDPUBLIC];
}
void CSession::SetFlag( uint nFlag )
{
	m_nSessionFlags |= nFlag;
}
void CSession::SetIsHost( bool bHost )
{
	m_bIsHost = bHost;
}
void CSession::SetIsSystemLink( bool bSystemLink )
{
	m_bIsSystemLink = bSystemLink;
}
void CSession::SetOwnerId( uint id )
{
	m_nOwnerId = id;
}
bool CSession::IsHost()
{
	return m_bIsHost;
}
bool CSession::IsFull()
{
	return ( GetSessionSlots( SLOTS_TOTALPRIVATE ) == GetSessionSlots( SLOTS_FILLEDPRIVATE ) &&
		GetSessionSlots( SLOTS_TOTALPUBLIC ) == GetSessionSlots( SLOTS_FILLEDPUBLIC ) );
}
bool CSession::IsArbitrated()
{
	return m_bIsArbitrated;
}
bool CSession::IsSystemLink()
{
	return m_bIsSystemLink;
}
void CSession::SetParent( CMatchmaking *pParent )
{
	m_pParent = pParent;
}
double CSession::GetTime()
{
	return Plat_FloatTime();
}

