//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "cvar.h"
#include "gl_cvars.h"

#include "tier1/convar.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "client.h"
#include "server.h"
#include "GameEventManager.h"
#include "netmessages.h"
#include "sv_main.h"
#include "demo.h"
#include <ctype.h>
#ifdef POSIX
#include <wctype.h>
#endif

#ifndef SWDS
#include <vgui_controls/Controls.h>
#include <vgui/ILocalize.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Singleton CCvarUtilities
//-----------------------------------------------------------------------------
static CCvarUtilities g_CvarUtilities;
CCvarUtilities *cv = &g_CvarUtilities;


//-----------------------------------------------------------------------------
// Purpose: Update clients/server when FCVAR_REPLICATED etc vars change
//-----------------------------------------------------------------------------
static void ConVarNetworkChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( !pOldValue )
	{
		if ( var.GetFloat() == flOldValue )
			return;
	}
	else
	{
		if ( !Q_strcmp( var.GetString(), pOldValue ) )
			return;
	}

	if ( var.IsFlagSet( FCVAR_USERINFO ) )
	{
		// Are we not a server, but a client and have a change?
		if ( cl.IsConnected() )
		{
			// send changed cvar to server
			NET_SetConVar convar( var.GetName(), var.GetString() );
			cl.m_NetChannel->SendNetMsg( convar );
		}
	} 

	// Log changes to server variables

	// Print to clients
	if ( var.IsFlagSet( FCVAR_NOTIFY ) )
	{
		IGameEvent *event = g_GameEventManager.CreateEvent( "server_cvar" );

		if ( event )
		{
			event->SetString( "cvarname", var.GetName() );

			if ( var.IsFlagSet( FCVAR_PROTECTED ) )
			{
				event->SetString("cvarvalue", "***PROTECTED***" );
			}
			else
			{
				event->SetString("cvarvalue", var.GetString() );
			}

			g_GameEventManager.FireEvent( event );
		}
	}

	// Force changes down to clients (if running server)
	if ( var.IsFlagSet( FCVAR_REPLICATED ) && sv.IsActive() )
	{
		SV_ReplicateConVarChange( static_cast< ConVar* >( pConVar ), var.GetString() );
	}
}


//-----------------------------------------------------------------------------
// Implementation of the ICvarQuery interface
//-----------------------------------------------------------------------------
class CCvarQuery : public CBaseAppSystem< ICvarQuery >
{
public:
	virtual bool Connect( CreateInterfaceFn factory )
	{
		ICvar *pCVar = (ICvar*)factory( CVAR_INTERFACE_VERSION, 0 );
		if ( !pCVar )
			return false;

		pCVar->InstallCVarQuery( this );
		return true;
	}

	virtual InitReturnVal_t Init()
	{
		// If the value has changed, notify clients/server based on ConVar flags.
		// NOTE: this will only happen for non-FCVAR_NEVER_AS_STRING vars.
		// Also, this happened in SetDirect for older clients that don't have the
		// callback interface.
		g_pCVar->InstallGlobalChangeCallback( ConVarNetworkChangeCallback );
		return INIT_OK;
	}

	virtual void Shutdown()
	{
		g_pCVar->RemoveGlobalChangeCallback( ConVarNetworkChangeCallback );
	}

	virtual void *QueryInterface( const char *pInterfaceName )
	{
		if ( !Q_stricmp( pInterfaceName, CVAR_QUERY_INTERFACE_VERSION ) )
			return (ICvarQuery*)this;
		return NULL;

	}

	// Purpose: Returns true if the commands can be aliased to one another
	//  Either game/client .dll shared with engine, 
	//  or game and client dll shared and marked FCVAR_REPLICATED
	virtual bool AreConVarsLinkable( const ConVar *child, const ConVar *parent )
	{
		// Both parent and child must be marked replicated for this to work
		bool repchild = child->IsFlagSet( FCVAR_REPLICATED );
		bool repparent = parent->IsFlagSet( FCVAR_REPLICATED );

		if ( repchild && repparent )
		{
			// Never on protected vars
			if ( child->IsFlagSet( FCVAR_PROTECTED ) || parent->IsFlagSet( FCVAR_PROTECTED ) )
			{
				ConMsg( "FCVAR_REPLICATED can't also be FCVAR_PROTECTED (%s)\n", child->GetName() );
				return false;
			}

			// Only on ConVars
			if ( child->IsCommand() || parent->IsCommand() )
			{
				ConMsg( "FCVAR_REPLICATED not valid on ConCommands (%s)\n", child->GetName() );
				return false;
			}

			// One must be in client .dll and the other in the game .dll, or both in the engine
			if ( child->IsFlagSet( FCVAR_GAMEDLL ) && !parent->IsFlagSet( FCVAR_CLIENTDLL ) )
			{
				ConMsg( "For FCVAR_REPLICATED, ConVar must be defined in client and game .dlls (%s)\n", child->GetName() );
				return false;
			}

			if ( child->IsFlagSet( FCVAR_CLIENTDLL ) && !parent->IsFlagSet( FCVAR_GAMEDLL ) )
			{
				ConMsg( "For FCVAR_REPLICATED, ConVar must be defined in client and game .dlls (%s)\n", child->GetName() );
				return false;
			}

			// Allowable
			return true;
		}

		// Otherwise need both to allow linkage
		if ( repchild || repparent )
		{
			ConMsg( "Both ConVars must be marked FCVAR_REPLICATED for linkage to work (%s)\n", child->GetName() );
			return false;
		}

		if ( parent->IsFlagSet( FCVAR_CLIENTDLL ) )
		{
			ConMsg( "Parent cvar in client.dll not allowed (%s)\n", child->GetName() );
			return false;
		}

		if ( parent->IsFlagSet( FCVAR_GAMEDLL ) )
		{
			ConMsg( "Parent cvar in server.dll not allowed (%s)\n", child->GetName() );
			return false;
		}

		return true;
	}
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CCvarQuery s_CvarQuery;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CCvarQuery, ICvarQuery, CVAR_QUERY_INTERFACE_VERSION, s_CvarQuery );


//-----------------------------------------------------------------------------
//
// CVar utilities begins here
//  
//-----------------------------------------------------------------------------
static bool IsAllSpaces( const wchar_t *str )
{
	const wchar_t *p = str;
	while ( p && *p )
	{
		if ( !iswspace( *p ) )
			return false;

		++p;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *var - 
//			*value - 
//-----------------------------------------------------------------------------
void CCvarUtilities::SetDirect( ConVar *var, const char *value )
{
	const char *pszValue;
	char szNew[ 1024 ];

	// Bail early if we're trying to set a FCVAR_USERINFO cvar on a dedicated server
	if ( var->IsFlagSet( FCVAR_USERINFO ) )
	{
		if ( sv.IsDedicated() )
		{
			return;
		}
	} 

	pszValue = value;
	// This cvar's string must only contain printable characters.
	// Strip out any other crap.
	// We'll fill in "empty" if nothing is left
	if ( var->IsFlagSet( FCVAR_PRINTABLEONLY ) )
	{
		wchar_t unicode[ 512 ];
#ifndef SWDS
		if ( sv.IsDedicated() )
		{
			// Dedicated servers don't have g_pVGuiLocalize, so fall back
			V_UTF8ToUnicode( pszValue, unicode, sizeof( unicode ) );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pszValue, unicode, sizeof( unicode ) );
		}
#else
		V_UTF8ToUnicode( pszValue, unicode, sizeof( unicode ) );
#endif
		wchar_t newUnicode[ 512 ];

		const wchar_t *pS;
		wchar_t *pD;

		// Clear out new string
		newUnicode[0] = L'\0';

		pS = unicode;
		pD = newUnicode;

		// Step through the string, only copying back in characters that are printable
		while ( *pS )
		{
			if ( iswcntrl( *pS ) || *pS == '~' )
			{
				pS++;
				continue;
			}

			*pD++ = *pS++;
		}

		// Terminate the new string
		*pD = L'\0';

		// If it's empty or all spaces, then insert a marker string
		if ( !wcslen( newUnicode ) || IsAllSpaces( newUnicode ) )
		{
			wcsncpy( newUnicode, L"#empty", ( sizeof( newUnicode ) / sizeof( wchar_t ) ) - 1 );
			newUnicode[ ( sizeof( newUnicode ) / sizeof( wchar_t ) ) - 1 ] = L'\0';
		}

#ifndef SWDS
		if ( sv.IsDedicated() )
		{
			V_UnicodeToUTF8( newUnicode, szNew, sizeof( szNew ) );
		}
		else
		{
			g_pVGuiLocalize->ConvertUnicodeToANSI( newUnicode, szNew, sizeof( szNew ) );
		}
#else
		V_UnicodeToUTF8( newUnicode, szNew, sizeof( szNew ) );
#endif
		// Point the value here.
		pszValue = szNew;
	}

	if ( var->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
	{
		var->SetValue( (float)atof( pszValue ) );
	}
	else
	{
		var->SetValue( pszValue );
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

// If you are changing this, please take a look at IsValidToggleCommand()
bool CCvarUtilities::IsCommand( const CCommand &args )
{
	int c = args.ArgC();
	if ( c == 0 )
		return false;

	ConVar			*v;

	// check variables
	v = g_pCVar->FindVar( args[0] );
	if ( !v )
		return false;

	// NOTE: Not checking for 'HIDDEN' here so we can actually set hidden convars
	if ( v->IsFlagSet(FCVAR_DEVELOPMENTONLY) )
		return false;

	// perform a variable print or set
	if ( c == 1 )
	{
		ConVar_PrintDescription( v );
		return true;
	}

	if ( v->IsFlagSet( FCVAR_SPONLY ) )
	{
#ifndef SWDS
		// Connected to server?
		if ( cl.IsConnected() )
		{
			// Is it not a single player game?
			if ( cl.m_nMaxClients > 1 )
			{
				ConMsg( "Can't set %s in multiplayer\n", v->GetName() );
				return true;
			}
		}
#endif
	}

	if ( v->IsFlagSet( FCVAR_NOT_CONNECTED ) )
	{
#ifndef SWDS
		// Connected to server?
		if ( cl.IsConnected() )
		{
			extern IBaseClientDLL *g_ClientDLL;
			if ( v->IsFlagSet( FCVAR_USERINFO ) && g_ClientDLL && g_ClientDLL->IsConnectedUserInfoChangeAllowed( v ) )
			{
				// Client.dll is allowing the convar change
			}
			else
			{
				ConMsg( "Can't change %s when playing, disconnect from the server or switch team to spectators\n", v->GetName() );
				return true;
			}
		}
#endif
	}

	// Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats on
	if ( v->IsFlagSet( FCVAR_CHEAT ) )
	{
		if ( !Host_IsSinglePlayerGame() && !CanCheat() 
#if !defined(SWDS)
			&& !cl.ishltv
#if defined( REPLAY_ENABLED )
			&& !cl.isreplay
#endif
			&& !demoplayer->IsPlayingBack() 
#endif
			)
		{
			ConMsg( "Can't use cheat cvar %s in multiplayer, unless the server has sv_cheats set to 1.\n", v->GetName() );
			return true;
		}
	}

	// Text invoking the command was typed into the console, decide what to do with it
	//  if this is a replicated ConVar, except don't worry about restrictions if playing a .dem file
	if ( v->IsFlagSet( FCVAR_REPLICATED ) 
#if !defined(SWDS)
		&& !demoplayer->IsPlayingBack()
#endif
		)
	{
		// If not running a server but possibly connected as a client, then
		//  if the message came from console, don't process the command
		if ( !sv.IsActive() &&
			!sv.IsLoading() &&
			(cmd_source == src_command) &&
			cl.IsConnected() )
		{
			ConMsg( "Can't change replicated ConVar %s from console of client, only server operator can change its value\n", v->GetName() );
			return true;
		}

		// FIXME:  Do we need a case where cmd_source == src_client?
		Assert( cmd_source != src_client );
	}

	// Note that we don't want the tokenized list, send down the entire string
	// except for surrounding quotes
	char remaining[1024];
	const char *pArgS = args.ArgS();
	int nLen = Q_strlen( pArgS );
	bool bIsQuoted = pArgS[0] == '\"';
	if ( !bIsQuoted )
	{
		Q_strncpy( remaining, args.ArgS(), sizeof(remaining) );
	}
	else
	{
		--nLen;
		Q_strncpy( remaining, &pArgS[1], sizeof(remaining) );
	}

	// Now strip off any trailing spaces
	char *p = remaining + nLen - 1;
	while ( p >= remaining )
	{
		if ( *p > ' ' )
			break;

		*p-- = 0;
	}

	// Strip off ending quote
	if ( bIsQuoted && p >= remaining )
	{
		if ( *p == '\"' )
		{
			*p = 0;
		}
	}

	SetDirect( v, remaining );
	return true;
}

// This is a band-aid copied directly from IsCommand().  
bool CCvarUtilities::IsValidToggleCommand( const char *cmd )
{
	ConVar			*v;

	// check variables
	v = g_pCVar->FindVar ( cmd );
	if (!v)
	{
		ConMsg( "%s is not a valid cvar\n", cmd );
		return false;
	}

	if ( v->IsFlagSet(FCVAR_DEVELOPMENTONLY) || v->IsFlagSet(FCVAR_HIDDEN) )
		return false;

	if ( v->IsFlagSet( FCVAR_SPONLY ) )
	{
#ifndef SWDS
		// Connected to server?
		if ( cl.IsConnected() )
		{
			// Is it not a single player game?
			if ( cl.m_nMaxClients > 1 )
			{
				ConMsg( "Can't set %s in multiplayer\n", v->GetName() );
				return false;
			}
		}
#endif
	}

	if ( v->IsFlagSet( FCVAR_NOT_CONNECTED ) )
	{
#ifndef SWDS
		// Connected to server?
		if ( cl.IsConnected() )
		{
			extern IBaseClientDLL *g_ClientDLL;
			if ( v->IsFlagSet( FCVAR_USERINFO ) && g_ClientDLL && g_ClientDLL->IsConnectedUserInfoChangeAllowed( v ) )
			{
				// Client.dll is allowing the convar change
			}
			else
			{
				ConMsg( "Can't change %s when playing, disconnect from the server or switch team to spectators\n", v->GetName() );
				return false;
			}
		}
#endif
	}

	// Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats on
	if ( v->IsFlagSet( FCVAR_CHEAT ) )
	{
		if ( !Host_IsSinglePlayerGame() && !CanCheat() 
#if !defined(SWDS) && !defined(_XBOX)
			&& !demoplayer->IsPlayingBack() 
#endif
			)
		{
			ConMsg( "Can't use cheat cvar %s in multiplayer, unless the server has sv_cheats set to 1.\n", v->GetName() );
			return false;
		}
	}

	// Text invoking the command was typed into the console, decide what to do with it
	//  if this is a replicated ConVar, except don't worry about restrictions if playing a .dem file
	if ( v->IsFlagSet( FCVAR_REPLICATED ) 
#if !defined(SWDS) && !defined(_XBOX)
		&& !demoplayer->IsPlayingBack()
#endif
		)
	{
		// If not running a server but possibly connected as a client, then
		//  if the message came from console, don't process the command
		if ( !sv.IsActive() &&
			!sv.IsLoading() &&
			(cmd_source == src_command) &&
			cl.IsConnected() )
		{
			ConMsg( "Can't change replicated ConVar %s from console of client, only server operator can change its value\n", v->GetName() );
			return false;
		}
	}

	// FIXME:  Do we need a case where cmd_source == src_client?
	Assert( cmd_source != src_client );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *f - 
//-----------------------------------------------------------------------------
void CCvarUtilities::WriteVariables( CUtlBuffer &buff, bool bAllVars )
{
	const ConCommandBase	*var;

	for (var = g_pCVar->GetCommands() ; var ; var = var->GetNext())
	{
		if ( var->IsCommand() )
			continue;

		bool archive = var->IsFlagSet( IsX360() ? FCVAR_ARCHIVE_XBOX : FCVAR_ARCHIVE );
		if ( archive )
		{
			const ConVar *pConvar = assert_cast<const ConVar *>( var );
			// Only write out values that differ from the defaults.
			if ( bAllVars || Q_strcmp( pConvar->GetString(), pConvar->GetDefault() ) != 0 )
			{
				buff.Printf( "%s \"%s\"\n", var->GetName(), ((ConVar *)var)->GetString() );
			}
		}
	}
}

static char *StripTabsAndReturns( const char *inbuffer, char *outbuffer, int outbufferSize )
{
	char *out = outbuffer;
	const char *i = inbuffer;
	char *o = out;

	out[ 0 ] = 0;

	while ( *i && o - out < outbufferSize - 1 )
	{
		if ( *i == '\n' ||
			*i == '\r' ||
			*i == '\t' )
		{
			*o++ = ' ';
			i++;
			continue;
		}
		if ( *i == '\"' )
		{
			*o++ = '\'';
			i++;
			continue;
		}

		*o++ = *i++;
	}

	*o = '\0';

	return out;
}

static char *StripQuotes( const char *inbuffer, char *outbuffer, int outbufferSize )
{	
	char *out = outbuffer;
	const char *i = inbuffer;
	char *o = out;

	out[ 0 ] = 0;

	while ( *i && o - out < outbufferSize - 1 )
	{
		if ( *i == '\"' )
		{
			*o++ = '\'';
			i++;
			continue;
		}

		*o++ = *i++;
	}

	*o = '\0';

	return out;
}

struct ConVarFlags_t
{
	int			bit;
	const char	*desc;
	const char	*shortdesc;
};

#define CONVARFLAG( x, y )	{ FCVAR_##x, #x, #y }

static ConVarFlags_t g_ConVarFlags[]=
{
	//	CONVARFLAG( UNREGISTERED, "u" ),
	CONVARFLAG( ARCHIVE, "a" ),
	CONVARFLAG( SPONLY, "sp" ),
	CONVARFLAG( GAMEDLL, "sv" ),
	CONVARFLAG( CHEAT, "cheat" ),
	CONVARFLAG( USERINFO, "user" ),
	CONVARFLAG( NOTIFY, "nf" ),
	CONVARFLAG( PROTECTED, "prot" ),
	CONVARFLAG( PRINTABLEONLY, "print" ),
	CONVARFLAG( UNLOGGED, "log" ),
	CONVARFLAG( NEVER_AS_STRING, "numeric" ),
	CONVARFLAG( REPLICATED, "rep" ),
	CONVARFLAG( DEMO, "demo" ),
	CONVARFLAG( DONTRECORD, "norecord" ),
	CONVARFLAG( SERVER_CAN_EXECUTE, "server_can_execute" ),
	CONVARFLAG( CLIENTCMD_CAN_EXECUTE, "clientcmd_can_execute" ),
	CONVARFLAG( CLIENTDLL, "cl" ),
};

static void PrintListHeader( FileHandle_t& f )
{
	char csvflagstr[ 1024 ];

	csvflagstr[ 0 ] = 0;

	int c = ARRAYSIZE( g_ConVarFlags );
	for ( int i = 0 ; i < c; ++i )
	{
		char csvf[ 64 ];

		ConVarFlags_t & entry = g_ConVarFlags[ i ];
		Q_snprintf( csvf, sizeof( csvf ), "\"%s\",", entry.desc );
		Q_strncat( csvflagstr, csvf, sizeof( csvflagstr ), COPY_ALL_CHARACTERS );
	}

	g_pFileSystem->FPrintf( f,"\"%s\",\"%s\",%s,\"%s\"\n", "Name", "Value", csvflagstr, "Help Text" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *var - 
//			*f - 
//-----------------------------------------------------------------------------
static void PrintCvar( const ConVar *var, bool logging, FileHandle_t& fh )
{
	char flagstr[ 128 ];
	char csvflagstr[ 1024 ];

	flagstr[ 0 ] = 0;
	csvflagstr[ 0 ] = 0;

	int c = ARRAYSIZE( g_ConVarFlags );
	for ( int i = 0 ; i < c; ++i )
	{
		char f[ 32 ];
		char csvf[ 64 ];

		ConVarFlags_t & entry = g_ConVarFlags[ i ];
		if ( var->IsFlagSet( entry.bit ) )
		{
			Q_snprintf( f, sizeof( f ), ", %s", entry.shortdesc );

			Q_strncat( flagstr, f, sizeof( flagstr ), COPY_ALL_CHARACTERS );

			Q_snprintf( csvf, sizeof( csvf ), "\"%s\",", entry.desc );
		}
		else
		{
			Q_snprintf( csvf, sizeof( csvf ), "," );
		}

		Q_strncat( csvflagstr, csvf, sizeof( csvflagstr ), COPY_ALL_CHARACTERS );
	}


	char valstr[ 32 ];
	char tempbuff[512] = { 0 };

	// Clean up integers
	if ( var->GetInt() == (int)var->GetFloat() )   
	{
		Q_snprintf(valstr, sizeof( valstr ), "%-8i", var->GetInt() );
	}
	else
	{
		Q_snprintf(valstr, sizeof( valstr ), "%-8.3f", var->GetFloat() );
	}

	// Print to console
	ConMsg( "%-40s : %-8s : %-16s : %s\n", var->GetName(), valstr, flagstr, StripTabsAndReturns( var->GetHelpText(), tempbuff, sizeof(tempbuff) ) );
	if ( logging )
	{
		g_pFileSystem->FPrintf( fh,"\"%s\",\"%s\",%s,\"%s\"\n", var->GetName(), valstr, csvflagstr, StripQuotes( var->GetHelpText(), tempbuff, sizeof(tempbuff) ) );
	}
}

static void PrintCommand( const ConCommand *cmd, bool logging, FileHandle_t& f )
{
	// Print to console
	char tempbuff[512] = { 0 };
	ConMsg ("%-40s : %-8s : %-16s : %s\n",cmd->GetName(), "cmd", "", StripTabsAndReturns( cmd->GetHelpText(), tempbuff, sizeof(tempbuff) ) );
	if ( logging )
	{
		char emptyflags[ 256 ];

		emptyflags[ 0 ] = 0;

		int c = ARRAYSIZE( g_ConVarFlags );
		for ( int i = 0; i < c; ++i )
		{
			char csvf[ 64 ];
			Q_snprintf( csvf, sizeof( csvf ), "," );
			Q_strncat( emptyflags, csvf, sizeof( emptyflags ), COPY_ALL_CHARACTERS );
		}
		// Names staring with +/- need to be wrapped in single quotes
		char name[ 256 ];
		Q_snprintf( name, sizeof( name ), "%s", cmd->GetName() );
		if ( name[ 0 ] == '+' || name[ 0 ] == '-' )
		{
			Q_snprintf( name, sizeof( name ), "'%s'", cmd->GetName() );
		}
		g_pFileSystem->FPrintf( f, "\"%s\",\"%s\",%s,\"%s\"\n", name, "cmd", emptyflags, StripQuotes( cmd->GetHelpText(), tempbuff, sizeof(tempbuff) ) );
	}
}

static bool ConCommandBaseLessFunc( const ConCommandBase * const &lhs, const ConCommandBase * const &rhs )
{ 
	const char *left = lhs->GetName();
	const char *right = rhs->GetName();

	if ( *left == '-' || *left == '+' )
		left++;
	if ( *right == '-' || *right == '+' )
		right++;

	return ( Q_stricmp( left, right ) < 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void CCvar::CvarList_f
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarList( const CCommand &args )
{
	const ConCommandBase	*var;	// Temporary Pointer to cvars
	int iArgs;						// Argument count
	const char *partial = NULL;		// Partial cvar to search for...
									// E.eg
	int ipLen = 0;					// Length of the partial cvar

	FileHandle_t f = FILESYSTEM_INVALID_HANDLE;         // FilePointer for logging
	bool bLogging = false;
	// Are we logging?
	iArgs = args.ArgC();		// Get count

	// Print usage?
	if ( iArgs == 2 && !Q_strcasecmp( args[1],"?" ) )
	{
		ConMsg( "cvarlist:  [log logfile] [ partial ]\n" );
		return;         
	}

	if ( !Q_strcasecmp( args[1],"log" ) && iArgs >= 3 )
	{
		char fn[256];
		Q_snprintf( fn, sizeof( fn ), "%s", args[2] );
		f = g_pFileSystem->Open( fn,"wb" );
		if ( f )
		{
			bLogging = true;
		}
		else
		{
			ConMsg( "Couldn't open '%s' for writing!\n", fn );
			return;
		}

		if ( iArgs == 4 )
		{
			partial = args[ 3 ];
			ipLen = Q_strlen( partial );
		}
	}
	else
	{
		partial = args[ 1 ];   
		ipLen = Q_strlen( partial );
	}

	// Banner
	ConMsg( "cvar list\n--------------\n" );

	CUtlRBTree< const ConCommandBase * > sorted( 0, 0, ConCommandBaseLessFunc );

	// Loop through cvars...
	for ( var= g_pCVar->GetCommands(); var; var=var->GetNext() )
	{
		bool print = false;

		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		if (partial)  // Partial string searching?
		{
			if ( !Q_strncasecmp( var->GetName(), partial, ipLen ) )
			{
				print = true;
			}
		}
		else		  
		{
			print = true;
		}

		if ( !print )
			continue;

		sorted.Insert( var );
	}

	if ( bLogging )
	{
		PrintListHeader( f );
	}
	for ( int i = sorted.FirstInorder(); i != sorted.InvalidIndex(); i = sorted.NextInorder( i ) )
	{
		var = sorted[ i ];
		if ( var->IsCommand() )
		{
			PrintCommand( (ConCommand *)var, bLogging, f );
		}
		else
		{
			PrintCvar( (ConVar *)var, bLogging, f );
		}
	}


	// Show total and syntax help...
	if ( partial && partial[0] )
	{
		ConMsg("--------------\n%3i convars/concommands for [%s]\n", sorted.Count(), partial );
	}
	else
	{
		ConMsg("--------------\n%3i total convars/concommands\n", sorted.Count() );
	}

	if ( bLogging )
	{
		g_pFileSystem->Close( f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : int
//-----------------------------------------------------------------------------
int CCvarUtilities::CountVariablesWithFlags( int flags )
{
	int i = 0;
	const ConCommandBase *var;

	for ( var = g_pCVar->GetCommands(); var; var = var->GetNext() )
	{
		if ( var->IsCommand() )
			continue;

		if ( var->IsFlagSet( flags ) )
		{
			i++;
		}
	}

	return i;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarHelp( const CCommand &args )
{
	const char *search;
	const ConCommandBase *var;

	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  help <cvarname>\n" );
		return;
	}

	// Get name of var to find
	search = args[1];

	// Search for it
	var = g_pCVar->FindCommandBase( search );
	if ( !var )
	{
		ConMsg( "help:  no cvar or command named %s\n", search );
		return;
	}

	// Show info
	ConVar_PrintDescription( var );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarDifferences( const CCommand &args )
{
	const ConCommandBase *var;

	// Loop through vars and print out findings
	for ( var = g_pCVar->GetCommands(); var; var=var->GetNext() )
	{
		if ( var->IsCommand( ) )
			continue;
		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		if ( !Q_stricmp( ((ConVar *)var)->GetDefault(), ((ConVar *)var)->GetString() ) )
			continue;

		ConVar_PrintDescription( (ConVar *)var );	
	}
}


//-----------------------------------------------------------------------------
// Purpose: Toggles a cvar on/off, or cycles through a set of values
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarToggle( const CCommand &args )
{
	int i;

	int c = args.ArgC();
	if ( c < 2 )
	{
		ConMsg( "Usage:  toggle <cvarname> [value1] [value2] [value3]...\n" );
		return;
	}

	ConVar *var = g_pCVar->FindVar( args[1] );
	
	if ( !IsValidToggleCommand( args[1] ) )
	{
		return;
	}

	if ( c == 2 )
	{
		// just toggle it on and off
		var->SetValue( !var->GetBool() );
		ConVar_PrintDescription( var );
	}
	else
	{
		// look for the current value in the command arguments
		for( i = 2; i < c; i++ )
		{
			if ( !Q_strcmp( var->GetString(), args[ i ] ) )
				break;
		}

		// choose the next one
		i++;

		// if we didn't find it, or were at the last value in the command arguments, use the 1st argument
		if ( i >= c )
		{
			i = 2;
		}

		var->SetValue( args[ i ] );
		ConVar_PrintDescription( var );
	}
}

void CCvarUtilities::CvarFindFlags_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  findflags <string>\n" );
		ConMsg( "Available flags to search for: \n" );

		for ( int i=0; i < ARRAYSIZE( g_ConVarFlags ); i++ )
		{
			ConMsg( "   - %s\n", g_ConVarFlags[i].desc );
		}
		return;
	}

	// Get substring to find
	const char *search = args[1];
	const ConCommandBase *var;

	// Loop through vars and print out findings
	for (var=g_pCVar->GetCommands() ; var ; var=var->GetNext())
	{
		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		for ( int i=0; i < ARRAYSIZE( g_ConVarFlags ); i++ )
		{
			if ( !var->IsFlagSet( g_ConVarFlags[i].bit ) )
				continue;
			
			if ( !V_stristr( g_ConVarFlags[i].desc, search ) )
				continue;

			ConVar_PrintDescription( var );	
		}
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Hook to command
//-----------------------------------------------------------------------------
CON_COMMAND( findflags, "Find concommands by flags." )
{
	cv->CvarFindFlags_f( args );
}


//-----------------------------------------------------------------------------
// Purpose: Hook to command
//-----------------------------------------------------------------------------
CON_COMMAND( cvarlist, "Show the list of convars/concommands." )
{
	cv->CvarList( args );
}


//-----------------------------------------------------------------------------
// Purpose: Print help text for cvar
//-----------------------------------------------------------------------------
CON_COMMAND( help, "Find help about a convar/concommand." )
{
	cv->CvarHelp( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( differences, "Show all convars which are not at their default values." )
{
	cv->CvarDifferences( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( toggle, "Toggles a convar on or off, or cycles through a set of values." )
{
	cv->CvarToggle( args );
}


//-----------------------------------------------------------------------------
// Purpose: Send the cvars to VXConsole
//-----------------------------------------------------------------------------
#if defined( _X360 )
CON_COMMAND( getcvars, "" )
{
	g_pCVar->PublishToVXConsole();
}
#endif

