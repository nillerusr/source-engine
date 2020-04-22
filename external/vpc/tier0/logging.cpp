//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Logging system definitions.
//
//===============================================================================

#include "pch_tier0.h"
#include "logging.h"

#include <string.h>
#include "dbg.h"
#include "threadtools.h"
#include "tier0_strtools.h" // this is from tier1, but only included for inline definition of V_isspace

#ifdef _PS3
#include <sys/tty.h>
#endif


//////////////////////////////////////////////////////////////////////////
// Define commonly used channels here
//////////////////////////////////////////////////////////////////////////
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_GENERAL, "General" );

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_ASSERT, "Assert" );

// Corresponds to ConMsg/ConWarning/etc. with a level <= 1.
// Only errors are spewed by default.
BEGIN_DEFINE_LOGGING_CHANNEL( LOG_CONSOLE, "Console", LCF_CONSOLE_ONLY, LS_ERROR );
ADD_LOGGING_CHANNEL_TAG( "Console" );
END_DEFINE_LOGGING_CHANNEL();

// Corresponds to DevMsg/DevWarning/etc. with a level <= 1.
// Only errors are spewed by default.
BEGIN_DEFINE_LOGGING_CHANNEL( LOG_DEVELOPER, "Developer", LCF_CONSOLE_ONLY, LS_ERROR );
ADD_LOGGING_CHANNEL_TAG( "Developer" );
END_DEFINE_LOGGING_CHANNEL();

// Corresponds to ConMsg/ConWarning/etc. with a level >= 2.
// Only errors are spewed by default.
BEGIN_DEFINE_LOGGING_CHANNEL( LOG_DEVELOPER_CONSOLE, "DeveloperConsole", LCF_CONSOLE_ONLY, LS_ERROR );
ADD_LOGGING_CHANNEL_TAG( "DeveloperVerbose" );
ADD_LOGGING_CHANNEL_TAG( "Console" );
END_DEFINE_LOGGING_CHANNEL();

// Corresponds to DevMsg/DevWarning/etc, with a level >= 2.
// Only errors are spewed by default.
BEGIN_DEFINE_LOGGING_CHANNEL( LOG_DEVELOPER_VERBOSE, "DeveloperVerbose", LCF_CONSOLE_ONLY, LS_ERROR, Color( 192, 128, 192, 255 ) );
ADD_LOGGING_CHANNEL_TAG( "DeveloperVerbose" );
END_DEFINE_LOGGING_CHANNEL();

//////////////////////////////////////////////////////////////////////////
// Globals
//////////////////////////////////////////////////////////////////////////

// The index of the logging state used by the current thread.  This defaults to 0 across all threads, 
// which indicates that the global listener set should be used (CLoggingSystem::m_nGlobalStateIndex).
//
// NOTE:
// Because our linux TLS implementation does not support embedding a thread local
// integer in a class, the logging system must use a global thread-local integer.
// This means that we can only have one instance of CLoggingSystem, although
// we could support additional instances if we are willing to lose support for
// thread-local spew handling.
// There is no other reason why this class must be a singleton, except
// for the fact that there's no reason to have more than one in existence.
bool g_bEnforceLoggingSystemSingleton = false;

#ifdef _PS3
#include "tls_ps3.h"
#else // _PS3
CTHREADLOCALINT g_nThreadLocalStateIndex;
#endif // _PS3

//////////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////////

CLoggingSystem *g_pGlobalLoggingSystem = NULL;

// This function does not get inlined due to the static variable :(
CLoggingSystem *GetGlobalLoggingSystem_Internal()
{
	static CLoggingSystem globalLoggingSystem;
	g_pGlobalLoggingSystem = &globalLoggingSystem;
	return &globalLoggingSystem;
}

// This function can get inlined
CLoggingSystem *GetGlobalLoggingSystem()
{
	return ( g_pGlobalLoggingSystem == NULL ) ? GetGlobalLoggingSystem_Internal() : g_pGlobalLoggingSystem;
}

CLoggingSystem::CLoggingSystem() : 
m_nChannelCount( 0 ), 
m_nChannelTagCount( 0 ),
m_nTagNamePoolIndex( 0 ),
m_nGlobalStateIndex( 0 )
{ 
	Assert( !g_bEnforceLoggingSystemSingleton );
	g_bEnforceLoggingSystemSingleton = true;
#if !defined( _PS3 ) && !defined(POSIX) && !defined(PLATFORM_WINDOWS)
	// Due to uncertain constructor ordering (g_nThreadLocalStateIndex
	// may not be constructed yet so TLS index may not be available yet)
	// we cannot initialize the state index here without risking
	// AppVerifier errors and undefined behavior. Luckily TlsAlloc values
	// are guaranteed to be zero-initialized so we don't need to zero-init,
	// this, and in fact we can't for all threads.
	// TLS on PS3 is zero-initialized in global ELF section
	// TLS is also not accessible at this point before PRX entry point runs
	g_nThreadLocalStateIndex = 0;
#endif

	m_LoggingStates[0].m_nPreviousStackEntry = -1;

	m_LoggingStates[0].m_nListenerCount = 1;
	m_LoggingStates[0].m_RegisteredListeners[0] = &m_DefaultLoggingListener;
	m_LoggingStates[0].m_pLoggingResponse = &m_DefaultLoggingResponse;

	// Mark all other logging state blocks as unused.
	for ( int i = 1; i < MAX_LOGGING_STATE_COUNT; ++ i )
	{
		m_LoggingStates[i].m_nListenerCount = -1;
	}

	m_pStateMutex = NULL;
}

CLoggingSystem::~CLoggingSystem()
{
	g_bEnforceLoggingSystemSingleton = false;
	delete m_pStateMutex;
}

LoggingChannelID_t CLoggingSystem::RegisterLoggingChannel( const char *pChannelName, RegisterTagsFunc registerTagsFunc, int flags, LoggingSeverity_t severity, Color spewColor )
{
	if ( m_nChannelCount >= MAX_LOGGING_CHANNEL_COUNT )
	{
		// Out of logging channels... catastrophic fail!
		Log_Error( LOG_GENERAL, "Out of logging channels.\n" );
		Assert( 0 );
		return INVALID_LOGGING_CHANNEL_ID;
	}
	else
	{
		// Channels can be multiply defined, in which case return the ID of the existing channel.
		for ( int i = 0; i < m_nChannelCount; ++ i )
		{
			if ( V_tier0_stricmp( m_RegisteredChannels[i].m_Name, pChannelName ) == 0 )
			{
				// OK to call the tag registration callback; duplicates will be culled away.
				// This allows multiple people to register a logging channel, and the union of all tags will be registered.
				if ( registerTagsFunc != NULL )
				{
					registerTagsFunc();
				}

				// If a logging channel is registered multiple times, only one of the registrations should specify flags/severity/color.
				if ( m_RegisteredChannels[i].m_Flags == 0 && m_RegisteredChannels[i].m_MinimumSeverity == LS_MESSAGE && m_RegisteredChannels[i].m_SpewColor == UNSPECIFIED_LOGGING_COLOR )
				{
					m_RegisteredChannels[i].m_Flags = ( LoggingChannelFlags_t )flags;
					m_RegisteredChannels[i].m_MinimumSeverity = severity;
					m_RegisteredChannels[i].m_SpewColor = spewColor;
				}
				else
				{
					AssertMsg( flags == 0 || flags == m_RegisteredChannels[i].m_Flags, "Non-zero or mismatched flags specified in logging channel re-registration!" );
					AssertMsg( severity == LS_MESSAGE || severity == m_RegisteredChannels[i].m_MinimumSeverity, "Non-default or mismatched severity specified in logging channel re-registration!" );
					AssertMsg( spewColor == UNSPECIFIED_LOGGING_COLOR || spewColor == m_RegisteredChannels[i].m_SpewColor, "Non-default or mismatched color specified in logging channel re-registration!" );
				}

				return m_RegisteredChannels[i].m_ID;
			}
		}

		m_RegisteredChannels[m_nChannelCount].m_ID = m_nChannelCount;
		m_RegisteredChannels[m_nChannelCount].m_Flags = ( LoggingChannelFlags_t )flags;
		m_RegisteredChannels[m_nChannelCount].m_MinimumSeverity = severity;
		m_RegisteredChannels[m_nChannelCount].m_SpewColor = spewColor;
		strncpy( m_RegisteredChannels[m_nChannelCount].m_Name, pChannelName, MAX_LOGGING_IDENTIFIER_LENGTH );
		
		if ( registerTagsFunc != NULL ) 
		{
			registerTagsFunc();
		}
		return m_nChannelCount ++;
	}
}

LoggingChannelID_t CLoggingSystem::FindChannel( const char *pChannelName ) const
{
	for ( int i = 0; i < m_nChannelCount; ++ i )
	{
		if ( V_tier0_stricmp( m_RegisteredChannels[i].m_Name, pChannelName ) == 0 )
		{
			return i;
		}
	}

	return INVALID_LOGGING_CHANNEL_ID;
}

void CLoggingSystem::AddTagToCurrentChannel( const char *pTagName )
{
	// Add tags at the head of the tag-list of the most recently added channel.
	LoggingChannel_t *pChannel = &m_RegisteredChannels[m_nChannelCount];

	// First check for duplicates
	if ( pChannel->HasTag( pTagName ) )
	{
		return;
	}
	
	LoggingTag_t *pTag = AllocTag( pTagName );
	
	pTag->m_pNextTag = pChannel->m_pFirstTag;
	pChannel->m_pFirstTag = pTag;
}

void CLoggingSystem::SetChannelSpewLevel( LoggingChannelID_t channelID, LoggingSeverity_t minimumSeverity )
{
	GetChannel( channelID )->SetSpewLevel( minimumSeverity );
}

void CLoggingSystem::SetChannelSpewLevelByName( const char *pName, LoggingSeverity_t minimumSeverity )
{
	for ( int i = 0; i < m_nChannelCount; ++ i )
	{
		if ( V_tier0_stricmp( m_RegisteredChannels[i].m_Name, pName ) == 0 )
		{
			m_RegisteredChannels[i].SetSpewLevel( minimumSeverity );
		}
	}
}

void CLoggingSystem::SetChannelSpewLevelByTag( const char *pTag, LoggingSeverity_t minimumSeverity )
{
	for ( int i = 0; i < m_nChannelCount; ++ i )
	{
		if ( m_RegisteredChannels[i].HasTag( pTag ) )
		{
			m_RegisteredChannels[i].SetSpewLevel( minimumSeverity );
		}
	}
}

void CLoggingSystem::PushLoggingState( bool bThreadLocal, bool bClearState )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new CThreadFastMutex();

	m_pStateMutex->Lock();
	
	int nNewState = FindUnusedStateIndex();
	// Ensure we're not out of state blocks.
	Assert( nNewState != -1 );

	int nCurrentState = bThreadLocal ? (int)g_nThreadLocalStateIndex : m_nGlobalStateIndex;

	if ( bClearState )
	{
		m_LoggingStates[nNewState].m_nListenerCount = 0;
		m_LoggingStates[nNewState].m_pLoggingResponse = &m_DefaultLoggingResponse;	
	}
	else
	{
		m_LoggingStates[nNewState] = m_LoggingStates[nCurrentState];
	}

	m_LoggingStates[nNewState].m_nPreviousStackEntry = nCurrentState;

	if ( bThreadLocal )
	{
		g_nThreadLocalStateIndex = nNewState;
	}
	else
	{
		m_nGlobalStateIndex = nNewState;
	}

	m_pStateMutex->Unlock();
}

void CLoggingSystem::PopLoggingState( bool bThreadLocal )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new CThreadFastMutex();

	m_pStateMutex->Lock();

	int nCurrentState = bThreadLocal ? (int)g_nThreadLocalStateIndex : m_nGlobalStateIndex;
	
	// Shouldn't be less than 0 (implies error during Push()) or 0 (implies that Push() was never called)
	Assert( nCurrentState > 0 );

	// Mark the current state as unused.
	m_LoggingStates[nCurrentState].m_nListenerCount = -1;

	if ( bThreadLocal )
	{
		g_nThreadLocalStateIndex = m_LoggingStates[nCurrentState].m_nPreviousStackEntry;
	}
	else
	{
		m_nGlobalStateIndex = m_LoggingStates[nCurrentState].m_nPreviousStackEntry;
	}

	m_pStateMutex->Unlock();
}

void CLoggingSystem::RegisterLoggingListener( ILoggingListener *pListener )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new CThreadFastMutex();

	m_pStateMutex->Lock();
	LoggingState_t *pState = GetCurrentState();
	if ( pState->m_nListenerCount > MAX_LOGGING_CHANNEL_COUNT )
	{
		// Out of logging listener slots... catastrophic fail!
		Assert( 0 );
	}
	else
	{
		pState->m_RegisteredListeners[pState->m_nListenerCount] = pListener;
		++ pState->m_nListenerCount;
	}
	m_pStateMutex->Unlock();
}

void CLoggingSystem::UnregisterLoggingListener( ILoggingListener *pListener )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new CThreadFastMutex();

	m_pStateMutex->Lock();
	LoggingState_t *pState = GetCurrentState();
	for ( int i = 0; i < pState->m_nListenerCount; ++ i )
	{
		if ( pState->m_RegisteredListeners[i] == pListener )
		{
			// Shuffle all the listeners ahead over these, and reduce the count.
			for ( int j = i; j < (pState->m_nListenerCount-1); ++ j )
			{
				pState->m_RegisteredListeners[j] = pState->m_RegisteredListeners[j+1];
			}
			pState->m_nListenerCount--;
			break;
		}
	}
	m_pStateMutex->Unlock();
}

bool CLoggingSystem::IsListenerRegistered( ILoggingListener *pListener )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new CThreadFastMutex();

	m_pStateMutex->Lock();
	const LoggingState_t *pState = GetCurrentState();
	bool bFound = false;
	for ( int i = 0; i < pState->m_nListenerCount; ++ i )
	{
		if ( pState->m_RegisteredListeners[i] == pListener )
		{
			bFound = true;
			break;
		}
	}
	m_pStateMutex->Unlock();
	return bFound;
}

void CLoggingSystem::ResetCurrentLoggingState()
{
	if ( !m_pStateMutex )
		m_pStateMutex = new CThreadFastMutex();

	m_pStateMutex->Lock();
	LoggingState_t *pState = GetCurrentState();
	pState->m_nListenerCount = 0;
	pState->m_pLoggingResponse = &m_DefaultLoggingResponse;
	m_pStateMutex->Unlock();
}

void CLoggingSystem::SetLoggingResponsePolicy( ILoggingResponsePolicy *pLoggingResponse )
{
	if ( !m_pStateMutex )
		m_pStateMutex = new CThreadFastMutex();

	m_pStateMutex->Lock();
	LoggingState_t *pState = GetCurrentState();
	if ( pLoggingResponse == NULL )
	{
		pState->m_pLoggingResponse = &m_DefaultLoggingResponse;
	}
	else
	{
		pState->m_pLoggingResponse = pLoggingResponse;
	}
	m_pStateMutex->Unlock();
}

LoggingResponse_t CLoggingSystem::LogDirect( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color color, const tchar *pMessage )
{
	Assert( IsValidChannelID( channelID ) );
	if ( !IsValidChannelID( channelID ) )
		return LR_CONTINUE;

	LoggingContext_t context;
	context.m_ChannelID = channelID;
	context.m_Flags = m_RegisteredChannels[channelID].m_Flags;
	context.m_Severity = severity;
	context.m_Color = ( color == UNSPECIFIED_LOGGING_COLOR ) ? m_RegisteredChannels[channelID].m_SpewColor : color;
	
	// It is assumed that the mutex is reentrant safe on all platforms.
	if ( !m_pStateMutex )
		m_pStateMutex = new CThreadFastMutex();

	m_pStateMutex->Lock();

	LoggingState_t *pState = GetCurrentState();
	
	for ( int i = 0; i < pState->m_nListenerCount; ++ i )
	{
		pState->m_RegisteredListeners[i]->Log( &context, pMessage );
	}

#if defined( _PS3 ) && !defined( _CERT )
	if ( !pState->m_nListenerCount )
	{
		unsigned int unBytesWritten;
		sys_tty_write( SYS_TTYP15, pMessage, strlen( pMessage ), &unBytesWritten );
	}
#endif
	
	LoggingResponse_t response = pState->m_pLoggingResponse->OnLog( &context );

	m_pStateMutex->Unlock();

	switch( response )
	{
	case LR_DEBUGGER:
		// Asserts put the debug break in the macro itself so the code breaks at the failure point.
		if ( severity != LS_ASSERT )
		{
			DebuggerBreakIfDebugging();
		}
		break;

	case LR_ABORT:
		Log_Msg( LOG_DEVELOPER_VERBOSE, "Exiting due to logging LR_ABORT request.\n" );
		Plat_ExitProcess( EXIT_FAILURE );
		break;
	}

	return response;
}

CLoggingSystem::LoggingChannel_t *CLoggingSystem::GetChannel( LoggingChannelID_t channelID )
{
	Assert( IsValidChannelID( channelID ) );
	return &m_RegisteredChannels[channelID];
}

const CLoggingSystem::LoggingChannel_t *CLoggingSystem::GetChannel( LoggingChannelID_t channelID ) const
{
	Assert( IsValidChannelID( channelID ) );
	return &m_RegisteredChannels[channelID];
}

CLoggingSystem::LoggingState_t *CLoggingSystem::GetCurrentState()
{
	// Assume the caller grabbed the mutex.
	int nState = g_nThreadLocalStateIndex;
	if ( nState != 0 )
	{
		Assert( nState > 0 && nState < MAX_LOGGING_STATE_COUNT );
		return &m_LoggingStates[nState];
	}
	else
	{
		Assert( m_nGlobalStateIndex >= 0 && m_nGlobalStateIndex < MAX_LOGGING_STATE_COUNT );
		return &m_LoggingStates[m_nGlobalStateIndex];
	}
}

const CLoggingSystem::LoggingState_t *CLoggingSystem::GetCurrentState() const
{
	// Assume the caller grabbed the mutex.
	int nState = g_nThreadLocalStateIndex;
	if ( nState != 0 )
	{
		Assert( nState > 0 && nState < MAX_LOGGING_STATE_COUNT );
		return &m_LoggingStates[nState];
	}
	else
	{
		Assert( m_nGlobalStateIndex >= 0 && m_nGlobalStateIndex < MAX_LOGGING_STATE_COUNT );
		return &m_LoggingStates[m_nGlobalStateIndex];
	}
}

int CLoggingSystem::FindUnusedStateIndex()
{
	for ( int i = 0; i < MAX_LOGGING_STATE_COUNT; ++ i )
	{
		if ( m_LoggingStates[i].m_nListenerCount < 0 )
		{
			return i;
		}
	}
	return -1;
}

CLoggingSystem::LoggingTag_t *CLoggingSystem::AllocTag( const char *pTagName )
{
	Assert( m_nChannelTagCount < MAX_LOGGING_TAG_COUNT );
	LoggingTag_t *pTag = &m_ChannelTags[m_nChannelTagCount ++];
	
	pTag->m_pNextTag = NULL;
	pTag->m_pTagName = m_TagNamePool + m_nTagNamePoolIndex;
	
	// Copy string into pool.
	size_t nTagLength = strlen( pTagName );
	Assert( m_nTagNamePoolIndex + nTagLength + 1 <= MAX_LOGGING_TAG_CHARACTER_COUNT );
	strcpy( m_TagNamePool + m_nTagNamePoolIndex, pTagName );
	m_nTagNamePoolIndex += ( int )nTagLength + 1;

	return pTag;
}

LoggingChannelID_t LoggingSystem_RegisterLoggingChannel( const char *pName, RegisterTagsFunc registerTagsFunc, int flags, LoggingSeverity_t severity, Color color )
{
	return GetGlobalLoggingSystem()->RegisterLoggingChannel( pName, registerTagsFunc, flags, severity, color );
}

void LoggingSystem_ResetCurrentLoggingState()
{
	GetGlobalLoggingSystem()->ResetCurrentLoggingState();
}

void LoggingSystem_RegisterLoggingListener( ILoggingListener *pListener )
{
	GetGlobalLoggingSystem()->RegisterLoggingListener( pListener );
}

void LoggingSystem_UnregisterLoggingListener( ILoggingListener *pListener )
{
	GetGlobalLoggingSystem()->UnregisterLoggingListener( pListener );
}

void LoggingSystem_SetLoggingResponsePolicy( ILoggingResponsePolicy *pResponsePolicy )
{
	GetGlobalLoggingSystem()->SetLoggingResponsePolicy( pResponsePolicy );
}

void LoggingSystem_PushLoggingState( bool bThreadLocal, bool bClearState )
{
	GetGlobalLoggingSystem()->PushLoggingState( bThreadLocal, bClearState );
}

void LoggingSystem_PopLoggingState( bool bThreadLocal )
{
	GetGlobalLoggingSystem()->PopLoggingState( bThreadLocal );
}

void LoggingSystem_AddTagToCurrentChannel( const char *pTagName )
{
	GetGlobalLoggingSystem()->AddTagToCurrentChannel( pTagName );
}

LoggingChannelID_t LoggingSystem_FindChannel( const char *pChannelName )
{
	return GetGlobalLoggingSystem()->FindChannel( pChannelName );
}

int LoggingSystem_GetChannelCount()
{
	return GetGlobalLoggingSystem()->GetChannelCount();
}

LoggingChannelID_t LoggingSystem_GetFirstChannelID()
{
	return ( GetGlobalLoggingSystem()->GetChannelCount() > 0 ) ? 0 : INVALID_LOGGING_CHANNEL_ID;
}

LoggingChannelID_t LoggingSystem_GetNextChannelID( LoggingChannelID_t channelID )
{
	int nChannelCount = GetGlobalLoggingSystem()->GetChannelCount();
	int nNextChannel = channelID + 1;
	return ( nNextChannel < nChannelCount ) ? nNextChannel : INVALID_LOGGING_CHANNEL_ID;
}

const CLoggingSystem::LoggingChannel_t *LoggingSystem_GetChannel( LoggingChannelID_t channelIndex )
{
	return GetGlobalLoggingSystem()->GetChannel( channelIndex );
}

bool LoggingSystem_HasTag( LoggingChannelID_t channelID, const char *pTag )
{
	return GetGlobalLoggingSystem()->HasTag( channelID, pTag );
}

bool LoggingSystem_IsChannelEnabled( LoggingChannelID_t channelID, LoggingSeverity_t severity )
{
	return GetGlobalLoggingSystem()->IsChannelEnabled( channelID, severity );
}

void LoggingSystem_SetChannelSpewLevel( LoggingChannelID_t channelID, LoggingSeverity_t minimumSeverity )
{
	GetGlobalLoggingSystem()->SetChannelSpewLevel( channelID, minimumSeverity );
}

void LoggingSystem_SetChannelSpewLevelByName( const char *pName, LoggingSeverity_t minimumSeverity )
{
	GetGlobalLoggingSystem()->SetChannelSpewLevelByName( pName, minimumSeverity );
}

void LoggingSystem_SetChannelSpewLevelByTag( const char *pTag, LoggingSeverity_t minimumSeverity )
{
	GetGlobalLoggingSystem()->SetChannelSpewLevelByTag( pTag, minimumSeverity );
}

int32 LoggingSystem_GetChannelColor( LoggingChannelID_t channelID )
{
	return GetGlobalLoggingSystem()->GetChannelColor( channelID ).GetRawColor();
}

void LoggingSystem_SetChannelColor( LoggingChannelID_t channelID, int color )
{
	Color c;
	c.SetRawColor( color );
	GetGlobalLoggingSystem()->SetChannelColor( channelID, c );
}

LoggingChannelFlags_t LoggingSystem_GetChannelFlags( LoggingChannelID_t channelID )
{
	return GetGlobalLoggingSystem()->GetChannelFlags( channelID );
}

void LoggingSystem_SetChannelFlags( LoggingChannelID_t channelID, LoggingChannelFlags_t flags )
{
	GetGlobalLoggingSystem()->SetChannelFlags( channelID, flags );
}

LoggingResponse_t LoggingSystem_Log( LoggingChannelID_t channelID, LoggingSeverity_t severity, const char *pMessageFormat, ... )
{
	if ( !GetGlobalLoggingSystem()->IsChannelEnabled( channelID, severity ) )
		return LR_CONTINUE;

	tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH];

	va_list args;
	va_start( args, pMessageFormat );
	Tier0Internal_vsntprintf( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, pMessageFormat, args );
	va_end( args );

	return GetGlobalLoggingSystem()->LogDirect( channelID, severity, UNSPECIFIED_LOGGING_COLOR, formattedMessage );
}

LoggingResponse_t LoggingSystem_Log( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color spewColor, const char *pMessageFormat, ... ) 
{
	if ( !GetGlobalLoggingSystem()->IsChannelEnabled( channelID, severity ) )
		return LR_CONTINUE;

	tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH];

	va_list args;
	va_start( args, pMessageFormat );
	Tier0Internal_vsntprintf( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, pMessageFormat, args );
	va_end( args );

	return GetGlobalLoggingSystem()->LogDirect( channelID, severity, spewColor, formattedMessage );
}

LoggingResponse_t LoggingSystem_LogDirect( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color spewColor, const char *pMessage )
{
	if ( !GetGlobalLoggingSystem()->IsChannelEnabled( channelID, severity ) )
		return LR_CONTINUE;
	return GetGlobalLoggingSystem()->LogDirect( channelID, severity, spewColor, pMessage );
}

LoggingResponse_t LoggingSystem_LogAssert( const char *pMessageFormat, ... ) 
{
	if ( !GetGlobalLoggingSystem()->IsChannelEnabled( LOG_ASSERT, LS_ASSERT ) )
		return LR_CONTINUE;

	tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH];

	va_list args;
	va_start( args, pMessageFormat );
	Tier0Internal_vsntprintf( formattedMessage, MAX_LOGGING_MESSAGE_LENGTH, pMessageFormat, args );
	va_end( args );

	return GetGlobalLoggingSystem()->LogDirect( LOG_ASSERT, LS_ASSERT, UNSPECIFIED_LOGGING_COLOR, formattedMessage );
}
