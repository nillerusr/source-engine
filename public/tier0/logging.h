//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Logging system declarations.
//
// The logging system is a channel-based output mechanism which allows
// subsystems to route their text/diagnostic output to various listeners
//
//===============================================================================

#ifndef LOGGING_H
#define LOGGING_H

#if !defined(__SPU__)

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "color.h"
#include "icommandline.h"
#include <stdio.h>

// For XBX_** functions
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif

// Used by CColorizedLoggingListener
#if defined( _WIN32 ) || (defined(POSIX) && !defined(_GAMECONSOLE))
#include "tier0/win32consoleio.h"
#endif

/*
	---- Logging System ----

	The logging system is a channel-based mechanism for all code (engine,
	mod, tool) across all platforms to output information, warnings, 
	errors, etc.

	This system supersedes the existing Msg(), Warning(), Error(), DevMsg(), ConMsg() etc. functions.
	There are channels defined in the new system through which all old messages are routed;
	see LOG_GENERAL, LOG_CONSOLE, LOG_DEVELOPER, etc.

	To use the system, simply call one of the predefined macros:

		Log_Msg( ChannelID, [Color], Message, ... )
		Log_Warning( ChannelID, [Color], Message, ... )
		Log_Error( ChannelID, [Color], Message, ... )

	A ChannelID is typically created by defining a logging channel with the
	log channel macros:

		DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_ChannelName, "ChannelName", [Flags], [MinimumSeverity], [Color] );
	
		or

		BEGIN_DEFINE_LOGGING_CHANNEL( LOG_ChannelName, "ChannelName", [Flags], [MinimumSeverity], [Color] );
		ADD_LOGGING_CHANNEL_TAG( "Tag1" );
		ADD_LOGGING_CHANNEL_TAG( "Tag2" );
		END_DEFINE_LOGGING_CHANNEL();

	These macros create a global channel ID variable with the name specified 
	by the first parameter (in this example, LOG_ChannelName).  This channel ID 
	can be used by various LoggingSystem_** functions to manipulate the channel settings.

	The optional [Flags] parameter is an OR'd together set of LoggingChannelFlags_t 
	values (default: 0).

	The optional [MinimumSeverity] parameter is the lowest threshold
	above which messages will be processed (inclusive).  The default is LS_MESSAGE,
	which results in all messages, warnings, and errors being logged. 
	Variadic parameters to the Log_** functions will be ignored if a channel
	is not enabled for a given severity (for performance reasons).

	Logging channels can have their minimum severity modified by name, ID, or tag.

	Logging channels are not hierarchical since there are situations in which
	a channel needs to belong to multiple hierarchies.  Use tags to create
	categories or shallow hierarchies.

	@TODO (Feature wishlist):
	1) Callstack logging support
	2) Registering dynamic channels and unregistering channels at runtime
	3) Sentient robot to clean up the thousands of places using the old/legacy logging system.
*/

//////////////////////////////////////////////////////////////////////////
// Constants, Types, Forward Declares
//////////////////////////////////////////////////////////////////////////

class CLoggingSystem;
class CThreadFastMutex;

//-----------------------------------------------------------------------------
// Maximum length of a sprintf'ed logging message.
//-----------------------------------------------------------------------------
const int MAX_LOGGING_MESSAGE_LENGTH = 2048;

//-----------------------------------------------------------------------------
// Maximum length of a channel or tag name.
//-----------------------------------------------------------------------------
const int MAX_LOGGING_IDENTIFIER_LENGTH = 32;

//-----------------------------------------------------------------------------
// Maximum number of logging channels.  Increase if needed.
//-----------------------------------------------------------------------------
const int MAX_LOGGING_CHANNEL_COUNT = 256;

//-----------------------------------------------------------------------------
// Maximum number of logging tags across all channels.  Increase if needed.
//-----------------------------------------------------------------------------
const int MAX_LOGGING_TAG_COUNT = 1024;

//-----------------------------------------------------------------------------
// Maximum number of characters across all logging tags. Increase if needed.
//-----------------------------------------------------------------------------
const int MAX_LOGGING_TAG_CHARACTER_COUNT = 8192;

//-----------------------------------------------------------------------------
// Maximum number of concurrent logging listeners in a given logging state.
//-----------------------------------------------------------------------------
const int MAX_LOGGING_LISTENER_COUNT = 16;

//-----------------------------------------------------------------------------
// An invalid color set on a channel to imply that it should use
// a device-dependent default color where applicable.
//-----------------------------------------------------------------------------
const Color UNSPECIFIED_LOGGING_COLOR( 0, 0, 0, 0 );

//-----------------------------------------------------------------------------
// An ID returned by the logging system to refer to a logging channel.
//-----------------------------------------------------------------------------
typedef int LoggingChannelID_t;

//-----------------------------------------------------------------------------
// A sentinel value indicating an invalid logging channel ID.
//-----------------------------------------------------------------------------
const LoggingChannelID_t INVALID_LOGGING_CHANNEL_ID = -1;

//-----------------------------------------------------------------------------
// The severity of a logging operation.
//-----------------------------------------------------------------------------
enum LoggingSeverity_t
{
	//-----------------------------------------------------------------------------
	// An informative logging message.
	//-----------------------------------------------------------------------------
	LS_MESSAGE = 0,

	//-----------------------------------------------------------------------------
	// A warning, typically non-fatal
	//-----------------------------------------------------------------------------
	LS_WARNING = 1,

	//-----------------------------------------------------------------------------
	// A message caused by an Assert**() operation.
	//-----------------------------------------------------------------------------
	LS_ASSERT = 2,

	//-----------------------------------------------------------------------------
	// An error, typically fatal/unrecoverable.
	//-----------------------------------------------------------------------------
	LS_ERROR = 3,

	//-----------------------------------------------------------------------------
	// A placeholder level, higher than any legal value.
	// Not a real severity value!
	//-----------------------------------------------------------------------------
	LS_HIGHEST_SEVERITY = 4,
};

//-----------------------------------------------------------------------------
// Action which should be taken by logging system as a result of 
// a given logged message.
//
// The logging system invokes ILoggingResponsePolicy::OnLog() on
// the specified policy object, which returns a LoggingResponse_t.
//-----------------------------------------------------------------------------
enum LoggingResponse_t
{
	LR_CONTINUE,
	LR_DEBUGGER,
	LR_ABORT,
};

//-----------------------------------------------------------------------------
// Logging channel behavior flags, set on channel creation.
//-----------------------------------------------------------------------------
enum LoggingChannelFlags_t
{
	//-----------------------------------------------------------------------------
	// Indicates that the spew is only relevant to interactive consoles.
	//-----------------------------------------------------------------------------
	LCF_CONSOLE_ONLY = 0x00000001,

	//-----------------------------------------------------------------------------
	// Indicates that spew should not be echoed to any output devices.
	// A suitable logging listener must be registered which respects this flag 
	// (e.g. a file logger).
	//-----------------------------------------------------------------------------
	LCF_DO_NOT_ECHO = 0x00000002,
};

//-----------------------------------------------------------------------------
// A callback function used to register tags on a logging channel 
// during initialization.
//-----------------------------------------------------------------------------
typedef void ( *RegisterTagsFunc )();

//-----------------------------------------------------------------------------
// A context structure passed to logging listeners and response policy classes.
//-----------------------------------------------------------------------------
struct LoggingContext_t
{
	// ID of the channel being logged to.
	LoggingChannelID_t m_ChannelID;
	// Flags associated with the channel.
	LoggingChannelFlags_t m_Flags;
	// Severity of the logging event.
	LoggingSeverity_t m_Severity;
	// Color of logging message if one was specified to Log_****() macro.
	// If not specified, falls back to channel color.
	// If channel color is not specified, this value is UNSPECIFIED_LOGGING_COLOR
	// and indicates that a suitable default should be chosen.
	Color m_Color;
};

//-----------------------------------------------------------------------------
// Interface for classes to handle logging output.
//
// The Log() function of this class is called synchronously and serially
// by the logging system on all registered instances of ILoggingListener
// in the current "logging state".
//
// Derived classes may do whatever they want with the message (write to disk,
// write to console, send over the network, drop on the floor, etc.).
//
// In general, derived classes should do one, simple thing with the output
// to allow callers to register multiple, orthogonal logging listener classes.
//-----------------------------------------------------------------------------
class ILoggingListener
{
public:
	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage ) = 0;
};

//-----------------------------------------------------------------------------
// Interface for policy classes which determine how to behave when a 
// message is logged.
//
// Can return:
//   LR_CONTINUE (continue execution)
//   LR_DEBUGGER (break into debugger if one is present, otherwise continue)
//   LR_ABORT (terminate process immediately with a failure code of 1)
//-----------------------------------------------------------------------------
class ILoggingResponsePolicy
{
public:
	virtual LoggingResponse_t OnLog( const LoggingContext_t *pContext ) = 0;
};

//////////////////////////////////////////////////////////////////////////
// Common Logging Listeners & Logging Response Policies
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// A basic logging listener which prints to stdout and the debug channel.
//-----------------------------------------------------------------------------
class CSimpleLoggingListener : public ILoggingListener
{
public:
	CSimpleLoggingListener( bool bQuietPrintf = false, bool bQuietDebugger = false ) : 
	  m_bQuietPrintf( bQuietPrintf ), 
		  m_bQuietDebugger( bQuietDebugger ) 
	  { 
	  }

	  virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	  {
#ifdef _X360
		  if ( !m_bQuietDebugger && XBX_IsConsoleConnected() )
		  {
			  // send to console
			  XBX_DebugString( XMAKECOLOR( 0,0,0 ), pMessage );
		  }
		  else
#endif
		  {
#if !defined( _CERT ) && !defined( DBGFLAG_STRINGS_STRIP )
			  if ( !m_bQuietPrintf )
			  {
				  _tprintf( _T("%s"), pMessage );
			  }
#endif

#ifdef _WIN32
			  if ( !m_bQuietDebugger && Plat_IsInDebugSession() )
			  {
				  Plat_DebugString( pMessage );
			  }
#endif
		  }
	  }

	  // If set to true, does not print anything to stdout.
	  bool m_bQuietPrintf;
	  // If set to true, does not print anything to debugger.
	  bool m_bQuietDebugger;
};

//-----------------------------------------------------------------------------
// A basic logging listener for GUI applications
//-----------------------------------------------------------------------------
class CSimpleWindowsLoggingListener : public ILoggingListener
{
public:
	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
		if ( Plat_IsInDebugSession() )
		{
			Plat_DebugString( pMessage );
		}
		if ( pContext->m_Severity == LS_ERROR )
		{
			if ( Plat_IsInDebugSession() )
				DebuggerBreak();

			Plat_MessageBox( "Error", pMessage );
		}
	}
};


//-----------------------------------------------------------------------------
// ** NOTE FOR INTEGRATION **
// This was copied over from source 2 rather than integrated because 
// source 2 has more significantly refactored tier0 logging.
//
// A logging listener with Win32 console API color support which which prints 
// to stdout and the debug channel.
//-----------------------------------------------------------------------------
#if !defined(_GAMECONSOLE)
class CColorizedLoggingListener : public CSimpleLoggingListener
{
public:
	CColorizedLoggingListener( bool bQuietPrintf = false, bool bQuietDebugger = false ) : CSimpleLoggingListener( bQuietPrintf, bQuietDebugger )
	{
		InitWin32ConsoleColorContext( &m_ColorContext );
	}

	virtual void Log( const LoggingContext_t *pContext, const tchar *pMessage )
	{
		if ( !m_bQuietPrintf )
		{
			int nPrevColor = -1;

			if ( pContext->m_Color != UNSPECIFIED_LOGGING_COLOR )
			{
				nPrevColor = SetWin32ConsoleColor( &m_ColorContext,
					pContext->m_Color.r(), pContext->m_Color.g(), pContext->m_Color.b(), 
					MAX( MAX( pContext->m_Color.r(), pContext->m_Color.g() ), pContext->m_Color.b() ) > 128 );
			}

			_tprintf( _T("%s"), pMessage );

			if ( nPrevColor >= 0 )
			{
				RestoreWin32ConsoleColor( &m_ColorContext, nPrevColor );
			}
		}

#ifdef _WIN32
		if ( !m_bQuietDebugger && Plat_IsInDebugSession() )
		{
			Plat_DebugString( pMessage );
		}
#endif
	}

	Win32ConsoleColorContext_t m_ColorContext;
};
#endif // !_GAMECONSOLE


//-----------------------------------------------------------------------------
// Default logging response policy used when one is not specified.
//-----------------------------------------------------------------------------
class CDefaultLoggingResponsePolicy : public ILoggingResponsePolicy
{
public:
	virtual LoggingResponse_t OnLog( const LoggingContext_t *pContext )
	{
		if ( pContext->m_Severity == LS_ASSERT && !CommandLine()->FindParm( "-noassert" ) ) 
		{
			return LR_DEBUGGER;
		}
		else if ( pContext->m_Severity == LS_ERROR )
		{
			return LR_ABORT;
		}
		else
		{
			return LR_CONTINUE;
		}
	}
};

//-----------------------------------------------------------------------------
// A logging response policy which never terminates the process, even on error.
//-----------------------------------------------------------------------------
class CNonFatalLoggingResponsePolicy : public ILoggingResponsePolicy
{
public:
	virtual LoggingResponse_t OnLog( const LoggingContext_t *pContext )
	{
		if ( ( pContext->m_Severity == LS_ASSERT && !CommandLine()->FindParm( "-noassert" ) ) || pContext->m_Severity == LS_ERROR )
		{
			return LR_DEBUGGER;
		}
		else
		{
			return LR_CONTINUE;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// Central Logging System
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// The central logging system.
//
// Multiple instances can exist, though all exported tier0 functionality
// specifically works with a single global instance 
// (via GetGlobalLoggingSystem()).
//-----------------------------------------------------------------------------
class CLoggingSystem
{
public:
	struct LoggingChannel_t;

	CLoggingSystem();
	~CLoggingSystem();

	//-----------------------------------------------------------------------------
	// Register a logging channel with the logging system.
	// The same channel can be registered multiple times, but the parameters
	// in each call to RegisterLoggingChannel must either match across all calls
	// or be set to defaults on any given call
	//
	// This function is not thread-safe and should generally only be called 
	// by a single thread. Using the logging channel definition macros ensures 
	// that this is called on the static initialization thread.
	//-----------------------------------------------------------------------------
	LoggingChannelID_t RegisterLoggingChannel( const char *pChannelName, RegisterTagsFunc registerTagsFunc, int flags = 0, LoggingSeverity_t minimumSeverity = LS_MESSAGE, Color spewColor = UNSPECIFIED_LOGGING_COLOR );

	//-----------------------------------------------------------------------------
	// Gets a channel ID from a string name.
	// Performs a simple linear search; cache the value whenever possible
	// or re-register the logging channel to get a global ID.
	//-----------------------------------------------------------------------------
	LoggingChannelID_t FindChannel( const char *pChannelName ) const;

	int GetChannelCount() const { return m_nChannelCount; }

	//-----------------------------------------------------------------------------
	// Gets a pointer to the logging channel description.
	//-----------------------------------------------------------------------------
	LoggingChannel_t *GetChannel( LoggingChannelID_t channelID );
	const LoggingChannel_t *GetChannel( LoggingChannelID_t channelID ) const;
	
	//-----------------------------------------------------------------------------
	// Returns true if the given channel has the specified tag.
	//-----------------------------------------------------------------------------
	bool HasTag( LoggingChannelID_t channelID, const char *pTag ) const { return GetChannel( channelID )->HasTag( pTag ); }
	
	//-----------------------------------------------------------------------------
	// Returns true if the given channel has been initialized.
	// The main purpose is catching m_nChannelCount being zero because no channels have been registered.
	//-----------------------------------------------------------------------------
	bool IsValidChannelID( LoggingChannelID_t channelID ) const { return ( channelID >= 0 ) && ( channelID < m_nChannelCount ); }
	
	//-----------------------------------------------------------------------------
	// Returns true if the given channel will spew at the given severity level.
	//-----------------------------------------------------------------------------
	bool IsChannelEnabled( LoggingChannelID_t channelID, LoggingSeverity_t severity ) const { return IsValidChannelID( channelID ) && GetChannel( channelID )->IsEnabled( severity ); }

	//-----------------------------------------------------------------------------
	// Functions to set the spew level of a channel either directly by ID or 
	// string name, or for all channels with a given tag.
	//
	// These functions are not technically thread-safe but calling them across 
	// multiple threads should cause no significant problems 
	// (the underlying data types being changed are 32-bit/atomic).
	//-----------------------------------------------------------------------------
	void SetChannelSpewLevel( LoggingChannelID_t channelID, LoggingSeverity_t minimumSeverity );
	void SetChannelSpewLevelByName( const char *pName, LoggingSeverity_t minimumSeverity );
	void SetChannelSpewLevelByTag( const char *pTag, LoggingSeverity_t minimumSeverity );
	void SetGlobalSpewLevel( LoggingSeverity_t minimumSeverity );

	//-----------------------------------------------------------------------------
	// Gets or sets the color of a logging channel.
	// (The functions are not thread-safe, but the consequences are not 
	// significant.)
	//-----------------------------------------------------------------------------
	Color GetChannelColor( LoggingChannelID_t channelID ) const { return GetChannel( channelID )->m_SpewColor; }
	void SetChannelColor( LoggingChannelID_t channelID, Color color ) { GetChannel( channelID )->m_SpewColor = color; }

	//-----------------------------------------------------------------------------
	// Gets or sets the flags on a logging channel.
	// (The functions are not thread-safe, but the consequences are not 
	// significant.)
	//-----------------------------------------------------------------------------
	LoggingChannelFlags_t GetChannelFlags( LoggingChannelID_t channelID ) const { return GetChannel( channelID )->m_Flags; }
	void SetChannelFlags( LoggingChannelID_t channelID, LoggingChannelFlags_t flags ) { GetChannel( channelID )->m_Flags = flags; }

	//-----------------------------------------------------------------------------
	// Adds a string tag to a channel.
	// This is not thread-safe and should only be called by a RegisterTagsFunc
	// callback passed in to RegisterLoggingChannel (via the 
	// channel definition macros).
	//-----------------------------------------------------------------------------
	void AddTagToCurrentChannel( const char *pTagName );

	//-----------------------------------------------------------------------------
	// Functions to save/restore the current logging state.  
	// Set bThreadLocal to true on a matching Push/Pop call if the intent
	// is to override the logging listeners on the current thread only.
	//
	// Pushing the current logging state onto the state stack results
	// in the current state being cleared by default (no listeners, default logging response policy).
	// Set bClearState to false to copy the existing listener pointers to the new state.
	//
	// These functions which mutate logging state ARE thread-safe and are 
	// guarded by m_StateMutex.
	//-----------------------------------------------------------------------------
	void PushLoggingState( bool bThreadLocal = false, bool bClearState = true );
	void PopLoggingState( bool bThreadLocal = false );

	//-----------------------------------------------------------------------------
	// Registers a logging listener (a class which handles logged messages).
	//-----------------------------------------------------------------------------
	void RegisterLoggingListener( ILoggingListener *pListener );
	
	//-----------------------------------------------------------------------------
	// Returns whether the specified logging listener is registered.
	//-----------------------------------------------------------------------------
	bool IsListenerRegistered( ILoggingListener *pListener );

	//-----------------------------------------------------------------------------
	// Clears out all of the current logging state (removes all listeners, 
	// sets the response policy to the default).
	//-----------------------------------------------------------------------------
	void ResetCurrentLoggingState();

	//-----------------------------------------------------------------------------
	// Sets a policy class to decide what should happen when messages of a 
	// particular severity are logged 
	// (e.g. exit on error, break into debugger).
	// If pLoggingResponse is NULL, uses the default response policy class.
	//-----------------------------------------------------------------------------
	void SetLoggingResponsePolicy( ILoggingResponsePolicy *pLoggingResponse );
	
	//-----------------------------------------------------------------------------
	// Logs a message to the specified channel using a given severity and 
	// spew color.  Passing in UNSPECIFIED_LOGGING_COLOR for 'color' allows
	// the logging listeners to provide a default.
	// NOTE: test 'IsChannelEnabled(channelID,severity)' before calling this!
	//-----------------------------------------------------------------------------
	LoggingResponse_t LogDirect( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color color, const tchar *pMessage );
	
	// Internal data to represent a logging tag
	struct LoggingTag_t
	{
		const char *m_pTagName;
		LoggingTag_t *m_pNextTag;
	};

	// Internal data to represent a logging channel.
	struct LoggingChannel_t
	{	
		bool HasTag( const char *pTag ) const
		{
			LoggingTag_t *pCurrentTag = m_pFirstTag;
			while( pCurrentTag != NULL )
			{
				if ( stricmp( pCurrentTag->m_pTagName, pTag ) == 0 )
				{
					return true;
				}
				pCurrentTag = pCurrentTag->m_pNextTag;
			}
			return false;
		}
		bool IsEnabled( LoggingSeverity_t severity ) const { return severity >= m_MinimumSeverity; }
		void SetSpewLevel( LoggingSeverity_t severity ) { m_MinimumSeverity = severity; }

		LoggingChannelID_t m_ID;
		LoggingChannelFlags_t m_Flags; // an OR'd combination of LoggingChannelFlags_t
		LoggingSeverity_t m_MinimumSeverity; // The minimum severity level required to activate this channel.
		Color m_SpewColor;
		char m_Name[MAX_LOGGING_IDENTIFIER_LENGTH];
		LoggingTag_t *m_pFirstTag;
	};

private:
	// Represents the current state of the logger (registered listeners, response policy class, etc.) and can
	// vary from thread-to-thread.  It can also be pushed/popped to save/restore listener/response state.
	struct LoggingState_t
	{
		// Index of the previous entry on the listener set stack.
		int m_nPreviousStackEntry;

		// Number of active listeners in this set.  Cannot exceed MAX_LOGGING_LISTENER_COUNT.
		// If set to -1, implies that this state structure is not in use.
		int m_nListenerCount;
		// Array of registered logging listener objects.
		ILoggingListener *m_RegisteredListeners[MAX_LOGGING_LISTENER_COUNT];

		// Specific policy class to determine behavior of logging system under specific message types.
		ILoggingResponsePolicy *m_pLoggingResponse;
	};

	// These state functions to assume the caller has already grabbed the mutex.
	LoggingState_t *GetCurrentState();
	const LoggingState_t *GetCurrentState() const;

	int FindUnusedStateIndex();
	LoggingTag_t *AllocTag( const char *pTagName );

	int m_nChannelCount;
	LoggingChannel_t m_RegisteredChannels[MAX_LOGGING_CHANNEL_COUNT];

	int m_nChannelTagCount;
	LoggingTag_t m_ChannelTags[MAX_LOGGING_TAG_COUNT];
	
	// Index to first free character in name pool.
	int m_nTagNamePoolIndex;
	// Pool of character data used for tag names.
	char m_TagNamePool[MAX_LOGGING_TAG_CHARACTER_COUNT];

	// Protects all data in this class except the registered channels 
	// (which are supposed to be registered using the macros at static/global init time).
	// It is assumed that this mutex is reentrant safe on all platforms.
	CThreadFastMutex *m_pStateMutex;
	
	// The index of the current "global" state of the logging system.  By default, all threads use this state
	// for logging unless a given thread has pushed the logging state with bThreadLocal == true.
	// If a thread-local state has been pushed, g_nThreadLocalStateIndex (a global thread-local integer) will be non-zero.
	// By default, g_nThreadLocalStateIndex is 0 for all threads.
	int m_nGlobalStateIndex;
	
	// A pool of logging states used to store a stack (potentially per-thread).
	static const int MAX_LOGGING_STATE_COUNT = 16;
	LoggingState_t m_LoggingStates[MAX_LOGGING_STATE_COUNT];

	// Default policy class which determines behavior.
	CDefaultLoggingResponsePolicy m_DefaultLoggingResponse;

	// Default spew function.
	CSimpleLoggingListener m_DefaultLoggingListener;
	
};

//////////////////////////////////////////////////////////////////////////
// Logging Macros
//////////////////////////////////////////////////////////////////////////

// This macro will resolve to the most appropriate overload of LoggingSystem_Log() depending on the number of parameters passed in.
#ifdef DBGFLAG_STRINGS_STRIP
#define InternalMsg( Channel, Severity, /* [Color], Message, */ ... ) do { if ( Severity == LS_ERROR && LoggingSystem_IsChannelEnabled( Channel, Severity ) ) LoggingSystem_Log( Channel, Severity, /* [Color], Message, */ ##__VA_ARGS__ ); } while( 0 )
#else
#define InternalMsg( Channel, Severity, /* [Color], Message, */ ... ) do { if ( LoggingSystem_IsChannelEnabled( Channel, Severity ) ) LoggingSystem_Log( Channel, Severity, /* [Color], Message, */ ##__VA_ARGS__ ); } while( 0 )
#endif

//-----------------------------------------------------------------------------
// New macros, use these!
//
// The macros take an optional Color parameter followed by the message 
// and the message formatting.
// We rely on the variadic macro (__VA_ARGS__) operator to paste in the 
// extra parameters and resolve to the appropriate overload.
//-----------------------------------------------------------------------------
#define Log_Msg( Channel, /* [Color], Message, */ ... ) InternalMsg( Channel, LS_MESSAGE, /* [Color], Message, */ ##__VA_ARGS__ )
#define Log_Warning( Channel, /* [Color], Message, */ ... ) InternalMsg( Channel, LS_WARNING, /* [Color], Message, */ ##__VA_ARGS__ )
#define Log_Error( Channel, /* [Color], Message, */ ... ) InternalMsg( Channel, LS_ERROR, /* [Color], Message, */ ##__VA_ARGS__ )
#ifdef DBGFLAG_STRINGS_STRIP
#define Log_Assert( ... ) LR_CONTINUE
#else
#define Log_Assert( Message, ... ) LoggingSystem_LogAssert( Message, ##__VA_ARGS__ )
#endif


#define DECLARE_LOGGING_CHANNEL( Channel ) extern LoggingChannelID_t Channel

#define DEFINE_LOGGING_CHANNEL_NO_TAGS( Channel, ChannelName, /* [Flags], [Severity], [Color] */ ... ) \
	LoggingChannelID_t Channel = LoggingSystem_RegisterLoggingChannel( ChannelName, NULL, ##__VA_ARGS__ )

#define BEGIN_DEFINE_LOGGING_CHANNEL( Channel, ChannelName, /* [Flags], [Severity], [Color] */ ... ) \
	static void Register_##Channel##_Tags(); \
	LoggingChannelID_t Channel = LoggingSystem_RegisterLoggingChannel( ChannelName, Register_##Channel##_Tags, ##__VA_ARGS__ ); \
	void Register_##Channel##_Tags() \
	{

#define ADD_LOGGING_CHANNEL_TAG( Tag ) LoggingSystem_AddTagToCurrentChannel( Tag )

#define END_DEFINE_LOGGING_CHANNEL() \
	}
	
//////////////////////////////////////////////////////////////////////////
// DLL Exports
//////////////////////////////////////////////////////////////////////////

// For documentation on these functions, please look at the corresponding function
// in CLoggingSystem (unless otherwise specified).
PLATFORM_INTERFACE LoggingChannelID_t LoggingSystem_RegisterLoggingChannel( const char *pName, RegisterTagsFunc registerTagsFunc, int flags = 0, LoggingSeverity_t severity = LS_MESSAGE, Color color = UNSPECIFIED_LOGGING_COLOR ); 

PLATFORM_INTERFACE void LoggingSystem_RegisterLoggingListener( ILoggingListener *pListener );
PLATFORM_INTERFACE void LoggingSystem_UnregisterLoggingListener(ILoggingListener *pListener);
PLATFORM_INTERFACE void LoggingSystem_ResetCurrentLoggingState();
PLATFORM_INTERFACE void LoggingSystem_SetLoggingResponsePolicy( ILoggingResponsePolicy *pResponsePolicy );
// NOTE: PushLoggingState() saves the current logging state on a stack and results in a new clear state
// (no listeners, default logging response policy).
PLATFORM_INTERFACE void LoggingSystem_PushLoggingState( bool bThreadLocal = false, bool bClearState = true );
PLATFORM_INTERFACE void LoggingSystem_PopLoggingState( bool bThreadLocal = false );

PLATFORM_INTERFACE void LoggingSystem_AddTagToCurrentChannel( const char *pTagName );

// Returns INVALID_LOGGING_CHANNEL_ID if not found
PLATFORM_INTERFACE LoggingChannelID_t LoggingSystem_FindChannel( const char *pChannelName );
PLATFORM_INTERFACE int LoggingSystem_GetChannelCount();
PLATFORM_INTERFACE LoggingChannelID_t LoggingSystem_GetFirstChannelID();
// Returns INVALID_LOGGING_CHANNEL_ID when there are no channels remaining.
PLATFORM_INTERFACE LoggingChannelID_t LoggingSystem_GetNextChannelID( LoggingChannelID_t channelID );
PLATFORM_INTERFACE const CLoggingSystem::LoggingChannel_t *LoggingSystem_GetChannel( LoggingChannelID_t channelID );

PLATFORM_INTERFACE bool LoggingSystem_HasTag( LoggingChannelID_t channelID, const char *pTag );

PLATFORM_INTERFACE bool LoggingSystem_IsChannelEnabled( LoggingChannelID_t channelID, LoggingSeverity_t severity );
PLATFORM_INTERFACE void LoggingSystem_SetChannelSpewLevel( LoggingChannelID_t channelID, LoggingSeverity_t minimumSeverity );
PLATFORM_INTERFACE void LoggingSystem_SetChannelSpewLevelByName( const char *pName, LoggingSeverity_t minimumSeverity );
PLATFORM_INTERFACE void LoggingSystem_SetChannelSpewLevelByTag( const char *pTag, LoggingSeverity_t minimumSeverity );
PLATFORM_INTERFACE void LoggingSystem_SetGlobalSpewLevel( LoggingSeverity_t minimumSeverity );

// Color is represented as an int32 due to C-linkage restrictions
PLATFORM_INTERFACE int32 LoggingSystem_GetChannelColor( LoggingChannelID_t channelID );
PLATFORM_INTERFACE void LoggingSystem_SetChannelColor( LoggingChannelID_t channelID, int color );

PLATFORM_INTERFACE LoggingChannelFlags_t LoggingSystem_GetChannelFlags( LoggingChannelID_t channelID );
PLATFORM_INTERFACE void LoggingSystem_SetChannelFlags( LoggingChannelID_t channelID, LoggingChannelFlags_t flags );

//-----------------------------------------------------------------------------
// Logs a variable-argument to a given channel with the specified severity.
// NOTE: if adding overloads to this function, remember that the Log_***
// macros simply pass their variadic parameters through to LoggingSystem_Log().
// Therefore, you need to ensure that the parameters are in the same general 
// order and that there are no ambiguities with the overload.
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE LoggingResponse_t LoggingSystem_Log( LoggingChannelID_t channelID, LoggingSeverity_t severity, PRINTF_FORMAT_STRING const char *pMessageFormat, ... ) FMTFUNCTION( 3, 4 );
PLATFORM_OVERLOAD LoggingResponse_t LoggingSystem_Log( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color spewColor, PRINTF_FORMAT_STRING const char *pMessageFormat, ... ) FMTFUNCTION( 4, 5 );

PLATFORM_INTERFACE LoggingResponse_t LoggingSystem_LogDirect( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color spewColor, const char *pMessage );
PLATFORM_INTERFACE LoggingResponse_t LoggingSystem_LogAssert( PRINTF_FORMAT_STRING const char *pMessageFormat, ... ) FMTFUNCTION( 1, 2 );

#endif //#if !defined(__SPU__)

#endif // LOGGING_H