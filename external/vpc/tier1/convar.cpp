//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "basetypes.h"
#include "tier1/convar.h"
#include "tier1/strtools.h"
#include "tier1/characterset.h"
#include "tier1/utlbuffer.h"
#include "tier1/tier1.h"
#include "tier1/convar_serverbounded.h"
#include "icvar.h"
#include "tier0/dbg.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif
#include "tier0/memdbgon.h"


// Comment this out when we release.
//#define ALLOW_DEVELOPMENT_CVARS
// This enables the l4d style of culling all cvars that are not marked FCVAR_RELEASE :
// #define CULL_ALL_CVARS_NOT_FCVAR_RELEASE



//-----------------------------------------------------------------------------
// Statically constructed list of ConCommandBases, 
// used for registering them with the ICVar interface
//-----------------------------------------------------------------------------
ConCommandBase			*ConCommandBase::s_pConCommandBases = NULL;
IConCommandBaseAccessor	*ConCommandBase::s_pAccessor = NULL;
static int s_nCVarFlag = 0;
static int s_nDLLIdentifier = -1;	// A unique identifier indicating which DLL this convar came from
static bool s_bRegistered = false;

class CDefaultAccessor : public IConCommandBaseAccessor
{
public:
	virtual bool RegisterConCommandBase( ConCommandBase *pVar )
	{
		// Link to engine's list instead
		g_pCVar->RegisterConCommand( pVar );
		return true;
	}
};

static CDefaultAccessor s_DefaultAccessor;

//-----------------------------------------------------------------------------
// Called by the framework to register ConCommandBases with the ICVar
//-----------------------------------------------------------------------------
void ConVar_Register( int nCVarFlag, IConCommandBaseAccessor *pAccessor )
{
	if ( !g_pCVar || s_bRegistered )
		return;

	Assert( s_nDLLIdentifier < 0 );
	s_bRegistered = true;
	s_nCVarFlag = nCVarFlag;
	s_nDLLIdentifier = g_pCVar->AllocateDLLIdentifier();

	ConCommandBase *pCur, *pNext;

	ConCommandBase::s_pAccessor = pAccessor ? pAccessor : &s_DefaultAccessor;
	pCur = ConCommandBase::s_pConCommandBases;
	while ( pCur )
	{
		pNext = pCur->m_pNext;
		pCur->AddFlags( s_nCVarFlag );
		pCur->Init();
		pCur = pNext;
	}

	g_pCVar->AddSplitScreenConVars();
	g_pCVar->ProcessQueuedMaterialThreadConVarSets();

	ConCommandBase::s_pConCommandBases = NULL;
}

void ConVar_Unregister( )
{
	if ( !g_pCVar || !s_bRegistered )
		return;

	Assert( s_nDLLIdentifier >= 0 );

	// Do this after unregister!!!
	g_pCVar->RemoveSplitScreenConVars( s_nDLLIdentifier );
	g_pCVar->UnregisterConCommands( s_nDLLIdentifier );
	s_nDLLIdentifier = -1;
	s_bRegistered = false;
}


//-----------------------------------------------------------------------------
// Purpose: Default constructor
//-----------------------------------------------------------------------------
ConCommandBase::ConCommandBase( void )
{
	m_bRegistered   = false;
	m_pszName       = NULL;
	m_pszHelpString = NULL;

	m_nFlags = 0;
	m_pNext  = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The base console invoked command/cvar interface
// Input  : *pName - name of variable/command
//			*pHelpString - help text
//			flags - flags
//-----------------------------------------------------------------------------
ConCommandBase::ConCommandBase( const char *pName, const char *pHelpString /*=0*/, int flags /*= 0*/ )
{
	Create( pName, pHelpString, flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConCommandBase::~ConCommandBase( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ConCommandBase::IsCommand( void ) const
{ 
//	Assert( 0 ); This can't assert. . causes a recursive assert in Sys_Printf, etc.
	return true;
}


//-----------------------------------------------------------------------------
// Returns the DLL identifier
//-----------------------------------------------------------------------------
CVarDLLIdentifier_t ConCommandBase::GetDLLIdentifier() const
{
	return s_nDLLIdentifier;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
//			callback - 
//			*pHelpString - 
//			flags - 
//-----------------------------------------------------------------------------
void ConCommandBase::Create( const char *pName, const char *pHelpString /*= 0*/, int flags /*= 0*/ )
{
	static char *empty_string = "";

	m_bRegistered = false;

	// Name should be static data
	Assert( pName );
	m_pszName = pName;
	m_pszHelpString = pHelpString ? pHelpString : empty_string;

	m_nFlags = flags;

#ifdef ALLOW_DEVELOPMENT_CVARS
	m_nFlags &= ~FCVAR_DEVELOPMENTONLY;
#endif

	if ( !( m_nFlags & FCVAR_UNREGISTERED ) )
	{
		m_pNext = s_pConCommandBases;
		s_pConCommandBases = this;
	}
	else
	{
		// It's unregistered
		m_pNext = NULL;
	}

	// If s_pAccessor is already set (this ConVar is not a global variable),
	//  register it.
	if ( s_pAccessor )
	{
		Init();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Used internally by OneTimeInit to initialize.
//-----------------------------------------------------------------------------
void ConCommandBase::Init()
{
	if ( s_pAccessor )
	{
		s_pAccessor->RegisterConCommandBase( this );
	}
}

void ConCommandBase::Shutdown()
{
	if ( g_pCVar )
	{
		g_pCVar->UnregisterConCommand( this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return name of the command/var
// Output : const char
//-----------------------------------------------------------------------------
const char *ConCommandBase::GetName( void ) const
{
	return m_pszName;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flag - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ConCommandBase::IsFlagSet( int flag ) const
{
	return ( flag & m_nFlags ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
//-----------------------------------------------------------------------------
void ConCommandBase::AddFlags( int flags )
{
	m_nFlags |= flags;

#ifdef ALLOW_DEVELOPMENT_CVARS
	m_nFlags &= ~FCVAR_DEVELOPMENTONLY;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: removes specified flags
//-----------------------------------------------------------------------------
void ConCommandBase::RemoveFlags( int flags )
{
	m_nFlags &= ~flags;
}

// Returns current flags
int ConCommandBase::GetFlags() const
{
	return m_nFlags;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const ConCommandBase
//-----------------------------------------------------------------------------
const ConCommandBase *ConCommandBase::GetNext( void ) const
{
	return m_pNext;
}

ConCommandBase *ConCommandBase::GetNext( void )
{
	return m_pNext;
}


//-----------------------------------------------------------------------------
// Purpose: Copies string using local new/delete operators
// Input  : *from - 
// Output : char
//-----------------------------------------------------------------------------
char *ConCommandBase::CopyString( const char *from )
{
	int		len;
	char	*to;

	len = strlen( from );
	if ( len <= 0 )
	{
		to = new char[1];
		to[0] = 0;
	}
	else
	{
		to = new char[len+1];
		V_strncpy( to, from, len+1 );
	}
	return to;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *ConCommandBase::GetHelpText( void ) const
{
	return m_pszHelpString;
}

//-----------------------------------------------------------------------------
// Purpose: Has this cvar been registered
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ConCommandBase::IsRegistered( void ) const
{
	return m_bRegistered;
}


//-----------------------------------------------------------------------------
//
// Con Commands start here
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Global methods
//-----------------------------------------------------------------------------
static characterset_t s_BreakSet;
static bool s_bBuiltBreakSet = false;


//-----------------------------------------------------------------------------
// Tokenizer class
//-----------------------------------------------------------------------------
CCommand::CCommand()
{
	if ( !s_bBuiltBreakSet )
	{
		s_bBuiltBreakSet = true;
		CharacterSetBuild( &s_BreakSet, "{}()':" );
	}

	Reset();
}

CCommand::CCommand( int nArgC, const char **ppArgV )
{
	Assert( nArgC > 0 );

	if ( !s_bBuiltBreakSet )
	{
		s_bBuiltBreakSet = true;
		CharacterSetBuild( &s_BreakSet, "{}()':" );
	}

	Reset();

	char *pBuf = m_pArgvBuffer;
	char *pSBuf = m_pArgSBuffer;
	m_nArgc = nArgC;
	for ( int i = 0; i < nArgC; ++i )
	{
		m_ppArgv[i] = pBuf;
		int nLen = V_strlen( ppArgV[i] );
		memcpy( pBuf, ppArgV[i], nLen+1 );
		if ( i == 0 )
		{
			m_nArgv0Size = nLen;
		}
		pBuf += nLen+1;

		bool bContainsSpace = strchr( ppArgV[i], ' ' ) != NULL;
		if ( bContainsSpace )
		{
			*pSBuf++ = '\"';
		}
		memcpy( pSBuf, ppArgV[i], nLen );
		pSBuf += nLen;
		if ( bContainsSpace )
		{
			*pSBuf++ = '\"';
		}

		if ( i != nArgC - 1 )
		{
			*pSBuf++ = ' ';
		}
	}
}

void CCommand::Reset()
{
	m_nArgc = 0;
	m_nArgv0Size = 0;
	m_pArgSBuffer[0] = 0;
}

characterset_t* CCommand::DefaultBreakSet()
{
	return &s_BreakSet;
}

bool CCommand::Tokenize( const char *pCommand, characterset_t *pBreakSet )
{
	Reset();
	if ( !pCommand )
		return false;

	// Use default break set
	if ( !pBreakSet )
	{
		pBreakSet = &s_BreakSet;
	}

	// Copy the current command into a temp buffer
	// NOTE: This is here to avoid the pointers returned by DequeueNextCommand
	// to become invalid by calling AddText. Is there a way we can avoid the memcpy?
	int nLen = V_strlen( pCommand );
	if ( nLen >= COMMAND_MAX_LENGTH - 1 )
	{
		Warning( "CCommand::Tokenize: Encountered command which overflows the tokenizer buffer.. Skipping!\n" );
		return false;
	}

	memcpy( m_pArgSBuffer, pCommand, nLen + 1 );

	// Parse the current command into the current command buffer
	CUtlBuffer bufParse( m_pArgSBuffer, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY ); 
	int nArgvBufferSize = 0;
	while ( bufParse.IsValid() && ( m_nArgc < COMMAND_MAX_ARGC ) )
	{
		char *pArgvBuf = &m_pArgvBuffer[nArgvBufferSize];
		int nMaxLen = COMMAND_MAX_LENGTH - nArgvBufferSize;
		int nStartGet = bufParse.TellGet();
		int	nSize = bufParse.ParseToken( pBreakSet, pArgvBuf, nMaxLen );
		if ( nSize < 0 )
			break;

		// Check for overflow condition
		if ( nMaxLen == nSize )
		{
			Reset();
			return false;
		}

		if ( m_nArgc == 1 )
		{
			// Deal with the case where the arguments were quoted
			m_nArgv0Size = bufParse.TellGet();
			bool bFoundEndQuote = m_pArgSBuffer[m_nArgv0Size-1] == '\"';
			if ( bFoundEndQuote )
			{
				--m_nArgv0Size;
			}
			m_nArgv0Size -= nSize;
			Assert( m_nArgv0Size != 0 );

			// The StartGet check is to handle this case: "foo"bar
			// which will parse into 2 different args. ArgS should point to bar.
			bool bFoundStartQuote = ( m_nArgv0Size > nStartGet ) && ( m_pArgSBuffer[m_nArgv0Size-1] == '\"' );
			Assert( bFoundEndQuote == bFoundStartQuote );
			if ( bFoundStartQuote )
			{
				--m_nArgv0Size;
			}
		}

		m_ppArgv[ m_nArgc++ ] = pArgvBuf;
		if( m_nArgc >= COMMAND_MAX_ARGC )
		{
			Warning( "CCommand::Tokenize: Encountered command which overflows the argument buffer.. Clamped!\n" );
		}

		nArgvBufferSize += nSize + 1;
		Assert( nArgvBufferSize <= COMMAND_MAX_LENGTH );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Helper function to parse arguments to commands.
//-----------------------------------------------------------------------------
const char* CCommand::FindArg( const char *pName ) const
{
	int nArgC = ArgC();
	for ( int i = 1; i < nArgC; i++ )
	{
		if ( !V_stricmp( Arg(i), pName ) )
			return (i+1) < nArgC ? Arg( i+1 ) : "";
	}
	return 0;
}

int CCommand::FindArgInt( const char *pName, int nDefaultVal ) const
{
	const char *pVal = FindArg( pName );
	if ( pVal )
		return atoi( pVal );
	else
		return nDefaultVal;
}


//-----------------------------------------------------------------------------
// Default console command autocompletion function 
//-----------------------------------------------------------------------------
int DefaultCompletionFunc( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Constructs a console command
//-----------------------------------------------------------------------------
//ConCommand::ConCommand()
//{
//	m_bIsNewConCommand = true;
//}

ConCommand::ConCommand( const char *pName, FnCommandCallbackV1_t callback, const char *pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/ )
{
	// Set the callback
	m_fnCommandCallbackV1 = callback;
	m_bUsingNewCommandCallback = false;
	m_bUsingCommandCallbackInterface = false;
	m_fnCompletionCallback = completionFunc ? completionFunc : DefaultCompletionFunc;
	m_bHasCompletionCallback = completionFunc != 0 ? true : false;

	// Setup the rest
	BaseClass::Create( pName, pHelpString, flags );
}

ConCommand::ConCommand( const char *pName, FnCommandCallback_t callback, const char *pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/ )
{
	// Set the callback
	m_fnCommandCallback = callback;
	m_bUsingNewCommandCallback = true;
	m_fnCompletionCallback = completionFunc ? completionFunc : DefaultCompletionFunc;
	m_bHasCompletionCallback = completionFunc != 0 ? true : false;
	m_bUsingCommandCallbackInterface = false;

	// Setup the rest
	BaseClass::Create( pName, pHelpString, flags );
}

ConCommand::ConCommand( const char *pName, ICommandCallback *pCallback, const char *pHelpString /*= 0*/, int flags /*= 0*/, ICommandCompletionCallback *pCompletionCallback /*= 0*/ )
{
	// Set the callback
	m_pCommandCallback = pCallback;
	m_bUsingNewCommandCallback = false;
	m_pCommandCompletionCallback = pCompletionCallback;
	m_bHasCompletionCallback = ( pCompletionCallback != 0 );
	m_bUsingCommandCallbackInterface = true;

	// Setup the rest
	BaseClass::Create( pName, pHelpString, flags );
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
ConCommand::~ConCommand( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if this is a command 
//-----------------------------------------------------------------------------
bool ConCommand::IsCommand( void ) const
{ 
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Invoke the function if there is one
//-----------------------------------------------------------------------------
void ConCommand::Dispatch( const CCommand &command )
{
	if ( m_bUsingNewCommandCallback )
	{
		if ( m_fnCommandCallback )
		{
			( *m_fnCommandCallback )( command );
			return;
		}
	}
	else if ( m_bUsingCommandCallbackInterface )
	{
		if ( m_pCommandCallback )
		{
			m_pCommandCallback->CommandCallback( command );
			return;
		}
	}
	else
	{
		if ( m_fnCommandCallbackV1 )
		{
			( *m_fnCommandCallbackV1 )();
			return;
		}
	}

	// Command without callback!!!
	AssertMsg1( 0, "Encountered ConCommand '%s' without a callback!\n", GetName() );
}


//-----------------------------------------------------------------------------
// Purpose: Calls the autocompletion method to get autocompletion suggestions
//-----------------------------------------------------------------------------
int	ConCommand::AutoCompleteSuggest( const char *partial, CUtlVector< CUtlString > &commands )
{
	if ( m_bUsingCommandCallbackInterface )
	{
		if ( !m_pCommandCompletionCallback )
			return 0;
		return m_pCommandCompletionCallback->CommandCompletionCallback( partial, commands );
	}

	Assert( m_fnCompletionCallback );
	if ( !m_fnCompletionCallback )
		return 0;

	char rgpchCommands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ];
	int iret = ( m_fnCompletionCallback )( partial, rgpchCommands );
	for ( int i = 0 ; i < iret; ++i )
	{
		CUtlString str = rgpchCommands[ i ];
		commands.AddToTail( str );
	}
	return iret;
}


//-----------------------------------------------------------------------------
// Returns true if the console command can autocomplete 
//-----------------------------------------------------------------------------
bool ConCommand::CanAutoComplete( void )
{
	return m_bHasCompletionCallback;
}



//-----------------------------------------------------------------------------
//
// Console Variables
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Various constructors
//-----------------------------------------------------------------------------
ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags /* = 0 */ )
{
	Create( pName, pDefaultValue, flags );
}

ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString )
{
	Create( pName, pDefaultValue, flags, pHelpString );
}

ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax )
{
	Create( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax );
}

ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, FnChangeCallback_t callback )
{
	Create( pName, pDefaultValue, flags, pHelpString, false, 0.0, false, 0.0, callback );
}

ConVar::ConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax, FnChangeCallback_t callback )
{
	Create( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax, callback );
}


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
ConVar::~ConVar( void )
{
	if ( m_Value.m_pszString )
	{
		delete[] m_Value.m_pszString;
		m_Value.m_pszString = NULL;
	}
}


//-----------------------------------------------------------------------------
// Install a change callback (there shouldn't already be one....)
//-----------------------------------------------------------------------------
void ConVar::InstallChangeCallback( FnChangeCallback_t callback, bool bInvoke /*=true*/ )
{
	if ( !callback )
	{
		Warning( "InstallChangeCallback called with NULL callback, ignoring!!!\n" );
		return;
	}

	if ( m_pParent->m_fnChangeCallbacks.Find( callback ) != m_pParent->m_fnChangeCallbacks.InvalidIndex() )
	{
		// Same ptr added twice, sigh...
		Warning( "InstallChangeCallback ignoring duplicate change callback!!!\n" );
		return;
	}

	m_pParent->m_fnChangeCallbacks.AddToTail( callback );

	// Call it immediately to set the initial value...
	if ( bInvoke )
	{
		callback( this, m_Value.m_pszString, m_Value.m_fValue );
	}
}

void ConVar::RemoveChangeCallback( FnChangeCallback_t callback )
{
	m_pParent->m_fnChangeCallbacks.FindAndRemove( callback );
}

bool ConVar::IsFlagSet( int flag ) const
{
	return ( flag & m_pParent->m_nFlags ) ? true : false;
}

int ConVar::GetFlags() const
{
	return m_pParent->m_nFlags;
}

const char *ConVar::GetHelpText( void ) const
{
	return m_pParent->m_pszHelpString;
}

void ConVar::AddFlags( int flags )
{
	m_pParent->m_nFlags |= flags;

#ifdef ALLOW_DEVELOPMENT_CVARS
	m_pParent->m_nFlags &= ~FCVAR_DEVELOPMENTONLY;
#endif
}

bool ConVar::IsRegistered( void ) const
{
	return m_pParent->m_bRegistered;
}

const char *ConVar::GetName( void ) const
{
	return m_pParent->m_pszName;
}

const char *ConVar::GetBaseName( void ) const
{
	return m_pParent->m_pszName;
}

int ConVar::GetSplitScreenPlayerSlot( void ) const
{
	// Default implementation (certain FCVAR_USERINFO derive a new type of convar and set this)
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ConVar::IsCommand( void ) const
{ 
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void ConVar::Init()
{
	BaseClass::Init();
}

bool ConVar::InternalSetColorFromString( const char *value )
{
	bool bColor = false;

	// Try pulling RGBA color values out of the string
	int nRGBA[4];
	int nParamsRead = sscanf( value, "%i %i %i %i", &(nRGBA[0]), &(nRGBA[1]), &(nRGBA[2]), &(nRGBA[3]) );

	if ( nParamsRead >= 3 )
	{
		// This is probably a color!
		if ( nParamsRead == 3 )
		{
			// Assume they wanted full alpha
			nRGBA[3] = 255;
		}

		if ( nRGBA[0] >= 0 && nRGBA[0] <= 255 && 
			 nRGBA[1] >= 0 && nRGBA[1] <= 255 && 
			 nRGBA[2] >= 0 && nRGBA[2] <= 255 && 
			 nRGBA[3] >= 0 && nRGBA[3] <= 255 )
		{
			// This is definitely a color!
			bColor = true;

			// Stuff all the values into each byte of our int
			unsigned char *pColorElement = ((unsigned char*)&m_Value.m_nValue);
			pColorElement[0] = nRGBA[0];
			pColorElement[1] = nRGBA[1];
			pColorElement[2] = nRGBA[2];
			pColorElement[3] = nRGBA[3];

			// Copy that value into a float (even though this has little meaning)
			m_Value.m_fValue = ( float )( m_Value.m_nValue );
		}
	}

	return bColor;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::InternalSetValue( const char *value )
{
	if ( IsFlagSet( FCVAR_MATERIAL_THREAD_MASK ) )
	{
		if ( g_pCVar && !g_pCVar->IsMaterialThreadSetAllowed() )
		{
			g_pCVar->QueueMaterialThreadSetValue( this, value );
			return;
		}
	}

	char  tempVal[ 32 ];
	char  *val;

	Assert(m_pParent == this); // Only valid for root convars.

	float flOldValue = m_Value.m_fValue;
	val = (char *)value;
	if ( !val )
		val = "";

	if ( !InternalSetColorFromString( value ) )
	{
		// Not a color, do the standard thing
		float fNewValue = ( float )atof( value );
		if ( !IsFinite( fNewValue ) )
		{
			Warning( "Warning:  %s = '%s' is infinite, clamping value.\n", GetName(), value );
			fNewValue = FLT_MAX;
		}

		if ( ClampValue( fNewValue ) )
		{
			V_snprintf( tempVal,sizeof(tempVal), "%f", fNewValue );
			val = tempVal;
		}

		// Redetermine value
		m_Value.m_fValue = fNewValue;
		m_Value.m_nValue = ( int )( m_Value.m_fValue );
	}

	if ( !( m_nFlags & FCVAR_NEVER_AS_STRING ) )
	{
		ChangeStringValue( val, flOldValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *tempVal - 
//-----------------------------------------------------------------------------
void ConVar::ChangeStringValue( const char *tempVal, float flOldValue )
{
	Assert( !( m_nFlags & FCVAR_NEVER_AS_STRING ) );

 	char* pszOldValue = (char*)stackalloc( m_Value.m_StringLength );
	memcpy( pszOldValue, m_Value.m_pszString, m_Value.m_StringLength );
	
	int len = V_strlen(tempVal) + 1;

	if ( len > m_Value.m_StringLength)
	{
		if (m_Value.m_pszString)
		{
			delete[] m_Value.m_pszString;
		}

		m_Value.m_pszString	= new char[len];
		m_Value.m_StringLength = len;
	}

	memcpy( m_Value.m_pszString, tempVal, len );

	// Invoke any necessary callback function
	for ( int i = 0; i < m_fnChangeCallbacks.Count(); ++i )
	{
		m_fnChangeCallbacks[ i ]( this, pszOldValue, flOldValue );
	}

	if ( g_pCVar )
	{
		g_pCVar->CallGlobalChangeCallbacks( this, pszOldValue, flOldValue );
	}

	stackfree( pszOldValue );
}

//-----------------------------------------------------------------------------
// Purpose: Check whether to clamp and then perform clamp
// Input  : value - 
// Output : Returns true if value changed
//-----------------------------------------------------------------------------
bool ConVar::ClampValue( float& value )
{
	if ( m_bHasMin && ( value < m_fMinVal ) )
	{
		value = m_fMinVal;
		return true;
	}
	
	if ( m_bHasMax && ( value > m_fMaxVal ) )
	{
		value = m_fMaxVal;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::InternalSetFloatValue( float fNewValue )
{
	if ( fNewValue == m_Value.m_fValue )
		return;

	if ( IsFlagSet( FCVAR_MATERIAL_THREAD_MASK ) )
	{
		if ( g_pCVar && !g_pCVar->IsMaterialThreadSetAllowed() )
		{
			g_pCVar->QueueMaterialThreadSetValue( this, fNewValue );
			return;
		}
	}

	Assert( m_pParent == this ); // Only valid for root convars.

	// Check bounds
	ClampValue( fNewValue );

	// Redetermine value
	float flOldValue = m_Value.m_fValue;
	m_Value.m_fValue		= fNewValue;
	m_Value.m_nValue		= ( int )m_Value.m_fValue;

	if ( !( m_nFlags & FCVAR_NEVER_AS_STRING ) )
	{
		char tempVal[ 32 ];
		V_snprintf( tempVal, sizeof( tempVal), "%f", m_Value.m_fValue );
		ChangeStringValue( tempVal, flOldValue );
	}
	else
	{
		Assert( m_fnChangeCallbacks.Count() == 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::InternalSetIntValue( int nValue )
{
	if ( nValue == m_Value.m_nValue )
		return;

	if ( IsFlagSet( FCVAR_MATERIAL_THREAD_MASK ) )
	{
		if ( g_pCVar && !g_pCVar->IsMaterialThreadSetAllowed() )
		{
			g_pCVar->QueueMaterialThreadSetValue( this, nValue );
			return;
		}
	}

	Assert( m_pParent == this ); // Only valid for root convars.

	float fValue = (float)nValue;
	if ( ClampValue( fValue ) )
	{
		nValue = ( int )( fValue );
	}

	// Redetermine value
	float flOldValue = m_Value.m_fValue;
	m_Value.m_fValue		= fValue;
	m_Value.m_nValue		= nValue;

	if ( !( m_nFlags & FCVAR_NEVER_AS_STRING ) )
	{
		char tempVal[ 32 ];
		V_snprintf( tempVal, sizeof( tempVal ), "%d", m_Value.m_nValue );
		ChangeStringValue( tempVal, flOldValue );
	}
	else
	{
		Assert( m_fnChangeCallbacks.Count() == 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::InternalSetColorValue( Color value )
{
	// Stuff color values into an int
	int nValue;

	unsigned char *pColorElement = ((unsigned char*)&nValue);
	pColorElement[0] = value[0];
	pColorElement[1] = value[1];
	pColorElement[2] = value[2];
	pColorElement[3] = value[3];

	// Call the int internal set
	InternalSetIntValue( nValue );
}

//-----------------------------------------------------------------------------
// Purpose: Private creation
//-----------------------------------------------------------------------------
void ConVar::Create( const char *pName, const char *pDefaultValue, int flags /*= 0*/,
	const char *pHelpString /*= NULL*/, bool bMin /*= false*/, float fMin /*= 0.0*/,
	bool bMax /*= false*/, float fMax /*= false*/, FnChangeCallback_t callback /*= NULL*/ )
{
	static char *empty_string = "";

	m_pParent = this;

	// Name should be static data
	m_pszDefaultValue = pDefaultValue ? pDefaultValue : empty_string;
	Assert( m_pszDefaultValue );

	m_bHasMin = bMin;
	m_fMinVal = fMin;
	m_bHasMax = bMax;
	m_fMaxVal = fMax;

	if ( callback )
	{
		m_fnChangeCallbacks.AddToTail( callback );
	}

	m_Value.m_StringLength = strlen( m_pszDefaultValue ) + 1;
	m_Value.m_pszString = new char[m_Value.m_StringLength];
	memcpy( m_Value.m_pszString, m_pszDefaultValue, m_Value.m_StringLength );

	if ( !InternalSetColorFromString( m_Value.m_pszString ) )
	{
		m_Value.m_fValue = ( float )atof( m_Value.m_pszString );
		if ( !IsFinite( m_Value.m_fValue ) )
		{
			Warning( "ConVar(%s) defined with infinite float value (%s)\n", pName, m_Value.m_pszString );
			m_Value.m_fValue = FLT_MAX;
			Assert( 0 );
		}

		// Bounds Check, should never happen, if it does, no big deal
		if ( m_bHasMin && ( m_Value.m_fValue < m_fMinVal ) )
		{
			Assert( 0 );
		}

		if ( m_bHasMax && ( m_Value.m_fValue > m_fMaxVal ) )
		{
			Assert( 0 );
		}

		m_Value.m_nValue = ( int )m_Value.m_fValue;
	}

	//If we're not tagged as cheat, archive or release then hide us.
#if CULL_ALL_CVARS_NOT_FCVAR_RELEASE
// FIXMEL4DTOMAINMERGE: will need to assess if this hides too many convars for TF and other projects in main
	if ( !( flags & ( FCVAR_CHEAT | FCVAR_ARCHIVE | FCVAR_RELEASE | FCVAR_USERINFO ) ) )
	{
		flags |= FCVAR_DEVELOPMENTONLY;
	}
#else
	
#endif


	BaseClass::Create( pName, pHelpString, flags );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *value - 
//-----------------------------------------------------------------------------
void ConVar::SetValue(const char *value)
{
	ConVar *var = ( ConVar * )m_pParent;
	var->InternalSetValue( value );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : value - 
//-----------------------------------------------------------------------------
void ConVar::SetValue( float value )
{
	ConVar *var = ( ConVar * )m_pParent;
	var->InternalSetFloatValue( value );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : value - 
//-----------------------------------------------------------------------------
void ConVar::SetValue( int value )
{
	ConVar *var = ( ConVar * )m_pParent;
	var->InternalSetIntValue( value );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : value - 
//-----------------------------------------------------------------------------
void ConVar::SetValue( Color value )
{
	ConVar *var = ( ConVar * )m_pParent;
	var->InternalSetColorValue( value );
}

//-----------------------------------------------------------------------------
// Purpose: Reset to default value
//-----------------------------------------------------------------------------
void ConVar::Revert( void )
{
	// Force default value again
	ConVar *var = ( ConVar * )m_pParent;
	var->SetValue( var->m_pszDefaultValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : minVal - 
// Output : true if there is a min set
//-----------------------------------------------------------------------------
bool ConVar::GetMin( float& minVal ) const
{
	minVal = m_pParent->m_fMinVal;
	return m_pParent->m_bHasMin;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : maxVal - 
//-----------------------------------------------------------------------------
bool ConVar::GetMax( float& maxVal ) const
{
	maxVal = m_pParent->m_fMaxVal;
	return m_pParent->m_bHasMax;
}

float ConVar::GetMinValue() const
{
	return m_pParent->m_fMinVal;
}

float ConVar::GetMaxValue() const
{
	return m_pParent->m_fMaxVal;;
}

bool ConVar::HasMin() const 
{ 
	return m_pParent->m_bHasMin; 
}

bool ConVar::HasMax() const 
{ 
	return m_pParent->m_bHasMax; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *ConVar::GetDefault( void ) const
{
	return m_pParent->m_pszDefaultValue;
}

void ConVar::SetDefault( const char *pszDefault ) 
{ 
	static char *empty_string = "";
	m_pszDefaultValue = pszDefault ? pszDefault : empty_string;
	Assert( m_pszDefaultValue );
}

//-----------------------------------------------------------------------------
// This version is simply used to make reading convars simpler.
// Writing convars isn't allowed in this mode
//-----------------------------------------------------------------------------
class CEmptyConVar : public ConVar
{
public:
	CEmptyConVar() : ConVar( "", "0" ) {}
	// Used for optimal read access
	virtual void SetValue( const char *pValue ) {}
	virtual void SetValue( float flValue ) {}
	virtual void SetValue( int nValue ) {}
	virtual const char *GetName( void ) const { return ""; }
	virtual bool IsFlagSet( int nFlags ) const { return false; }
};

static CEmptyConVar s_EmptyConVar;

ConVarRef::ConVarRef( const char *pName )
{
	Init( pName, false );
}

ConVarRef::ConVarRef( const char *pName, bool bIgnoreMissing )
{
	Init( pName, bIgnoreMissing );
}

void ConVarRef::Init( const char *pName, bool bIgnoreMissing )
{
	m_pConVar = g_pCVar ? g_pCVar->FindVar( pName ) : &s_EmptyConVar;
	if ( !m_pConVar )
	{
		m_pConVar = &s_EmptyConVar;
	}
	m_pConVarState = static_cast< ConVar * >( m_pConVar );
	if( !IsValid() )
	{
		static bool bFirst = true;
		if ( g_pCVar || bFirst )
		{
			if ( !bIgnoreMissing )
			{
				Warning( "ConVarRef %s doesn't point to an existing ConVar\n", pName );
			}
			bFirst = false;
		}
	}
}

ConVarRef::ConVarRef( IConVar *pConVar )
{
	m_pConVar = pConVar ? pConVar : &s_EmptyConVar;
	m_pConVarState = static_cast< ConVar * >( m_pConVar );
}

bool ConVarRef::IsValid() const
{
	return m_pConVar != &s_EmptyConVar;
}

// Helper for splitscreen ConVars
SplitScreenConVarRef::SplitScreenConVarRef( const char *pName )
{
	Init( pName, false );
}

SplitScreenConVarRef::SplitScreenConVarRef( const char *pName, bool bIgnoreMissing )
{
	Init( pName, bIgnoreMissing );
}

void SplitScreenConVarRef::Init( const char *pName, bool bIgnoreMissing )
{
	for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		cv_t &info = m_Info[ i ];
		char pchName[ 256 ];
		if ( i != 0 )
		{
			V_snprintf( pchName, sizeof( pchName ), "%s%d", pName, i + 1 );
		}
		else
		{
			V_strncpy( pchName, pName, sizeof( pchName ) );
		}

		info.m_pConVar = g_pCVar ? g_pCVar->FindVar( pchName ) : &s_EmptyConVar;
		if ( !info.m_pConVar )
		{
			info.m_pConVar = &s_EmptyConVar;
			if ( i > 0 )
			{
				// Point at slot zero instead, in case we got in here with a non FCVAR_SS var...
				info.m_pConVar = m_Info[ 0 ].m_pConVar;
			}
		}
		info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );
	}

	if ( !IsValid() )
	{
		static bool bFirst = true;
		if ( g_pCVar || bFirst )
		{
			if ( !bIgnoreMissing )
			{
				Warning( "ConVarRef %s doesn't point to an existing ConVar\n", pName );
			}
			bFirst = false;
		}
	}
}

SplitScreenConVarRef::SplitScreenConVarRef( IConVar *pConVar )
{
	cv_t &info = m_Info[ 0 ];
	info.m_pConVar = pConVar ? pConVar : &s_EmptyConVar;
	info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );

	for ( int i = 1; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		info = m_Info[ i ];
		char pchName[ 256 ];
		V_snprintf( pchName, sizeof( pchName ), "%s%d", pConVar->GetName(), i + 1 );

		info.m_pConVar = g_pCVar ? g_pCVar->FindVar( pchName ) : &s_EmptyConVar;
		if ( !info.m_pConVar )
		{
			info.m_pConVar = &s_EmptyConVar;
			if ( i > 0 )
			{
				// Point at slot zero instead, in case we got in here with a non FCVAR_SS var...
				info.m_pConVar = m_Info[ 0 ].m_pConVar;
			}
		}
		info.m_pConVarState = static_cast< ConVar * >( info.m_pConVar );
	}
}

bool SplitScreenConVarRef::IsValid() const
{
	return m_Info[ 0 ].m_pConVar != &s_EmptyConVar;
}

struct PrintConVarFlags_t
{
	int flag;
	const char *desc;
};

static PrintConVarFlags_t g_PrintConVarFlags[] =
{
	{ FCVAR_GAMEDLL, "game" },
	{ FCVAR_CLIENTDLL, "client" },
	{ FCVAR_ARCHIVE, "archive" },
	{ FCVAR_NOTIFY, "notify" },
	{ FCVAR_SPONLY, "singleplayer" },
	{ FCVAR_NOT_CONNECTED, "notconnected" },
	{ FCVAR_CHEAT, "cheat" },
	{ FCVAR_REPLICATED, "replicated" },
	{ FCVAR_SERVER_CAN_EXECUTE, "server_can_execute" },
	{ FCVAR_CLIENTCMD_CAN_EXECUTE, "clientcmd_can_execute" },
	{ FCVAR_USERINFO, "user" },
	{ FCVAR_SS, "ss" },
	{ FCVAR_SS_ADDED, "ss_added" },
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ConVar_AppendFlags( const ConCommandBase *var, char *buf, size_t bufsize )
{
	for ( int i = 0; i < ARRAYSIZE( g_PrintConVarFlags ) ; ++i )
	{
		const PrintConVarFlags_t &info =  g_PrintConVarFlags[ i ];
		if ( var->IsFlagSet( info.flag ) )
		{
			char append[ 128 ];
			V_snprintf( append, sizeof( append ), " %s", info.desc );
			V_strncat( buf, append, bufsize, COPY_ALL_CHARACTERS );
		}
	}
}

static void AppendPrintf( char *buf, size_t bufsize, char const *fmt, ... )
{
	char scratch[ 1024 ];
	va_list argptr;
	va_start( argptr, fmt );
	_vsnprintf( scratch, sizeof( scratch ) - 1, fmt, argptr );
	va_end( argptr );
	scratch[ sizeof( scratch ) - 1 ] = 0;

	V_strncat( buf, scratch, bufsize, COPY_ALL_CHARACTERS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ConVar_PrintDescription( const ConCommandBase *pVar )
{
	bool bMin, bMax;
	float fMin, fMax;
	const char *pStr;

	Assert( pVar );

	Color clr;
	clr.SetColor( 255, 100, 100, 255 );

	char outstr[ 4096 ];
	outstr[ 0 ] = 0;

	if ( !pVar->IsCommand() )
	{
		ConVar *var = ( ConVar * )pVar;
		const ConVar_ServerBounded *pBounded = dynamic_cast<const ConVar_ServerBounded*>( var );

		bMin = var->GetMin( fMin );
		bMax = var->GetMax( fMax );

		const char *value = NULL;
		char tempVal[ 32 ];

		if ( pBounded || var->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
		{
			value = tempVal;
			
			int intVal = pBounded ? pBounded->GetInt() : var->GetInt();
			float floatVal = pBounded ? pBounded->GetFloat() : var->GetFloat();

			if ( fabs( (float)intVal - floatVal ) < 0.000001 )
			{
				V_snprintf( tempVal, sizeof( tempVal ), "%d", intVal );
			}
			else
			{
				V_snprintf( tempVal, sizeof( tempVal ), "%f", floatVal );
			}
		}
		else
		{
			value = var->GetString();
		}

		if ( value )
		{
			AppendPrintf( outstr, sizeof( outstr ), "\"%s\" = \"%s\"", var->GetName(), value );

			if ( V_stricmp( value, var->GetDefault() ) )
			{
				AppendPrintf( outstr, sizeof( outstr ), " ( def. \"%s\" )", var->GetDefault() );
			}
		}

		if ( bMin )
		{
			AppendPrintf( outstr, sizeof( outstr ), " min. %f", fMin );
		}
		if ( bMax )
		{
			AppendPrintf( outstr, sizeof( outstr ), " max. %f", fMax );
		}

		// Handle virtualized cvars.
		if ( pBounded && fabs( pBounded->GetFloat() - var->GetFloat() ) > 0.0001f )
		{
			AppendPrintf( outstr, sizeof( outstr ), " [%.3f server clamped to %.3f]",
				var->GetFloat(), pBounded->GetFloat() );
		}
	}
	else
	{
		ConCommand *var = ( ConCommand * )pVar;

		AppendPrintf( outstr, sizeof( outstr ), "\"%s\" ", var->GetName() );
	}

	ConVar_AppendFlags( pVar, outstr, sizeof( outstr ) );

	pStr = pVar->GetHelpText();
	if ( pStr && *pStr )
	{
		ConMsg( "%-80s - %.80s\n", outstr, pStr );
	}
	else
	{
	 	ConMsg( "%-80s\n", outstr );
	}
}
