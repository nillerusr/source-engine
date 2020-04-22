//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "vstdlib/cvar.h"
#include <ctype.h>
#include "tier0/icommandline.h"
#include "tier1/utlrbtree.h"
#include "tier1/strtools.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "tier0/vprof.h"
#include "tier1/tier1.h"
#include "tier1/utlbuffer.h"

#ifdef _X360
#include "xbox/xbox_console.h"
#endif

#ifdef POSIX
#include <wctype.h>
#include <wchar.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Default implementation  of CvarQuery
//-----------------------------------------------------------------------------
class CDefaultCvarQuery : public CBaseAppSystem< ICvarQuery >
{
public:
	virtual void *QueryInterface( const char *pInterfaceName )
	{
		if ( !Q_stricmp( pInterfaceName, CVAR_QUERY_INTERFACE_VERSION ) )
			return (ICvarQuery*)this;
		return NULL;
	
	}

	virtual bool AreConVarsLinkable( const ConVar *child, const ConVar *parent )
	{
		return true;
	}
};

static CDefaultCvarQuery s_DefaultCvarQuery;
static ICvarQuery *s_pCVarQuery = NULL;


//-----------------------------------------------------------------------------
// Default implementation
//-----------------------------------------------------------------------------
class CCvar : public ICvar
{
public:
	CCvar();

	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Inherited from ICVar
	virtual CVarDLLIdentifier_t AllocateDLLIdentifier();
	virtual void			RegisterConCommand( ConCommandBase *pCommandBase );
	virtual void			UnregisterConCommand( ConCommandBase *pCommandBase );
	virtual void			UnregisterConCommands( CVarDLLIdentifier_t id );
	virtual const char*		GetCommandLineValue( const char *pVariableName );
	virtual ConCommandBase *FindCommandBase( const char *name );
	virtual const ConCommandBase *FindCommandBase( const char *name ) const;
	virtual ConVar			*FindVar ( const char *var_name );
	virtual const ConVar	*FindVar ( const char *var_name ) const;
	virtual ConCommand		*FindCommand( const char *name );
	virtual const ConCommand *FindCommand( const char *name ) const;
	virtual ConCommandBase	*GetCommands( void );
	virtual const ConCommandBase *GetCommands( void ) const;
	virtual void			InstallGlobalChangeCallback( FnChangeCallback_t callback );
	virtual void			RemoveGlobalChangeCallback( FnChangeCallback_t callback );
	virtual void			CallGlobalChangeCallbacks( ConVar *var, const char *pOldString, float flOldValue );
	virtual void			InstallConsoleDisplayFunc( IConsoleDisplayFunc* pDisplayFunc );
	virtual void			RemoveConsoleDisplayFunc( IConsoleDisplayFunc* pDisplayFunc );
	virtual void			ConsoleColorPrintf( const Color& clr, const char *pFormat, ... ) const;
	virtual void			ConsolePrintf( const char *pFormat, ... ) const;
	virtual void			ConsoleDPrintf( const char *pFormat, ... ) const;
	virtual void			RevertFlaggedConVars( int nFlag );
	virtual void			InstallCVarQuery( ICvarQuery *pQuery );

#if defined( _X360 )
	virtual void			PublishToVXConsole( );
#endif

	virtual bool			IsMaterialThreadSetAllowed( ) const;
	virtual void			QueueMaterialThreadSetValue( ConVar *pConVar, const char *pValue );
	virtual void			QueueMaterialThreadSetValue( ConVar *pConVar, int nValue );
	virtual void			QueueMaterialThreadSetValue( ConVar *pConVar, float flValue );
	virtual bool			HasQueuedMaterialThreadConVarSets() const;
	virtual int				ProcessQueuedMaterialThreadConVarSets();
private:
	enum
	{
		CONSOLE_COLOR_PRINT = 0,
		CONSOLE_PRINT,
		CONSOLE_DPRINT,
	};

	void DisplayQueuedMessages( );

	CUtlVector< FnChangeCallback_t >	m_GlobalChangeCallbacks;
	CUtlVector< IConsoleDisplayFunc* >	m_DisplayFuncs;
	int									m_nNextDLLIdentifier;
	ConCommandBase						*m_pConCommandList;

	// temporary console area so we can store prints before console display funs are installed
	mutable CUtlBuffer					m_TempConsoleBuffer;
protected:

	// internals for  ICVarIterator
	class CCVarIteratorInternal : public ICVarIteratorInternal
	{
	public:
		CCVarIteratorInternal( CCvar *outer ) 
			: m_pOuter( outer )
			//, m_pHash( &outer->m_CommandHash ), // remember my CCvar,
			//m_hashIter( -1, -1 ) // and invalid iterator
			, m_pCur( NULL )
		{}
		virtual void		SetFirst( void );
		virtual void		Next( void );
		virtual	bool		IsValid( void );
		virtual ConCommandBase *Get( void );
	protected:
		CCvar * const m_pOuter;
		//CConCommandHash * const m_pHash;
		//CConCommandHash::CCommandHashIterator_t m_hashIter;
		ConCommandBase *m_pCur;
	};

	virtual ICVarIteratorInternal	*FactoryInternalIterator( void );
	friend class CCVarIteratorInternal;

	enum ConVarSetType_t
	{
		CONVAR_SET_STRING = 0,
		CONVAR_SET_INT,
		CONVAR_SET_FLOAT,
	};
	struct QueuedConVarSet_t
	{
		ConVar *m_pConVar;
		ConVarSetType_t m_nType;
		int m_nInt;
		float m_flFloat;
		CUtlString m_String;
	};
	CUtlVector< QueuedConVarSet_t > m_QueuedConVarSets;
	bool m_bMaterialSystemThreadSetAllowed;

private:
	// Standard console commands -- DO NOT PLACE ANY HIGHER THAN HERE BECAUSE THESE MUST BE THE FIRST TO DESTRUCT
	CON_COMMAND_MEMBER_F( CCvar, "find", Find, "Find concommands with the specified string in their name/help text.", 0 )
};

void CCvar::CCVarIteratorInternal::SetFirst( void ) RESTRICT
{
	//m_hashIter = m_pHash->First();
	m_pCur = m_pOuter->GetCommands();
}

void CCvar::CCVarIteratorInternal::Next( void ) RESTRICT
{
	//m_hashIter = m_pHash->Next( m_hashIter );
	if ( m_pCur )
		m_pCur = m_pCur->GetNext();
}

bool CCvar::CCVarIteratorInternal::IsValid( void ) RESTRICT
{
	//return m_pHash->IsValidIterator( m_hashIter );
	return m_pCur != NULL;
}

ConCommandBase *CCvar::CCVarIteratorInternal::Get( void ) RESTRICT
{
	Assert( IsValid( ) );
	//return (*m_pHash)[m_hashIter];
	return m_pCur;
}

ICvar::ICVarIteratorInternal *CCvar::FactoryInternalIterator( void )
{
	return new CCVarIteratorInternal( this );
}

//-----------------------------------------------------------------------------
// Factor for CVars 
//-----------------------------------------------------------------------------
static CCvar s_Cvar;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CCvar, ICvar, CVAR_INTERFACE_VERSION, s_Cvar );


//-----------------------------------------------------------------------------
// Returns a CVar dictionary for tool usage
//-----------------------------------------------------------------------------
CreateInterfaceFn VStdLib_GetICVarFactory()
{
	return Sys_GetFactoryThis();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CCvar::CCvar() : m_TempConsoleBuffer( 0, 1024 )
{
	m_nNextDLLIdentifier = 0;
	m_pConCommandList = NULL;

	m_bMaterialSystemThreadSetAllowed = false;
}


//-----------------------------------------------------------------------------
// Methods of IAppSystem
//-----------------------------------------------------------------------------
bool CCvar::Connect( CreateInterfaceFn factory )
{
	ConnectTier1Libraries( &factory, 1 );

	s_pCVarQuery = (ICvarQuery*)factory( CVAR_QUERY_INTERFACE_VERSION, NULL );
	if ( !s_pCVarQuery )
	{
		s_pCVarQuery = &s_DefaultCvarQuery;
	}

	ConVar_Register();
	return true;
}

void CCvar::Disconnect()
{
	ConVar_Unregister();
	s_pCVarQuery = NULL;
	DisconnectTier1Libraries();
}

InitReturnVal_t CCvar::Init()
{
	return INIT_OK;
}

void CCvar::Shutdown()
{
}

void *CCvar::QueryInterface( const char *pInterfaceName )
{
	// We implement the ICvar interface
	if ( !V_strcmp( pInterfaceName, CVAR_INTERFACE_VERSION ) )
		return (ICvar*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Method allowing the engine ICvarQuery interface to take over
//-----------------------------------------------------------------------------
void CCvar::InstallCVarQuery( ICvarQuery *pQuery )
{
	Assert( s_pCVarQuery == &s_DefaultCvarQuery );
	s_pCVarQuery = pQuery ? pQuery : &s_DefaultCvarQuery;
}


//-----------------------------------------------------------------------------
// Used by DLLs to be able to unregister all their commands + convars 
//-----------------------------------------------------------------------------
CVarDLLIdentifier_t CCvar::AllocateDLLIdentifier()
{
	return m_nNextDLLIdentifier++;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *variable - 
//-----------------------------------------------------------------------------
void CCvar::RegisterConCommand( ConCommandBase *variable )
{
	// Already registered
	if ( variable->IsRegistered() )
		return;

	variable->m_bRegistered = true;

	const char *pName = variable->GetName();
	if ( !pName || !pName[0] )
	{
		variable->m_pNext = NULL;
		return;
	}

	// If the variable is already defined, then setup the new variable as a proxy to it.
	const ConCommandBase *pOther = FindVar( variable->GetName() );
	if ( pOther )
	{
		if ( variable->IsCommand() || pOther->IsCommand() )
		{
			Warning( "WARNING: unable to link %s and %s because one or more is a ConCommand.\n", variable->GetName(), pOther->GetName() );
		}
		else
		{
			// This cast is ok because we make sure they're ConVars above.
			const ConVar *pChildVar = static_cast< const ConVar* >( variable );
			const ConVar *pParentVar = static_cast< const ConVar* >( pOther );

			// See if it's a valid linkage
			if ( s_pCVarQuery->AreConVarsLinkable( pChildVar, pParentVar ) )
			{
				// Make sure the default values are the same (but only spew about this for FCVAR_REPLICATED)
				if(  pChildVar->m_pszDefaultValue && pParentVar->m_pszDefaultValue &&
					 pChildVar->IsFlagSet( FCVAR_REPLICATED ) && pParentVar->IsFlagSet( FCVAR_REPLICATED ) )
				{
					if( Q_stricmp( pChildVar->m_pszDefaultValue, pParentVar->m_pszDefaultValue ) != 0 )
					{
						Warning( "Parent and child ConVars with different default values! %s child: %s parent: %s (parent wins)\n", 
							variable->GetName(), pChildVar->m_pszDefaultValue, pParentVar->m_pszDefaultValue );
					}
				}

				const_cast<ConVar*>( pChildVar )->m_pParent = const_cast<ConVar*>( pParentVar )->m_pParent;

				// Absorb material thread related convar flags
				const_cast<ConVar*>( pParentVar )->m_nFlags |= pChildVar->m_nFlags & ( FCVAR_MATERIAL_THREAD_MASK | FCVAR_ACCESSIBLE_FROM_THREADS );

				// check the parent's callbacks and slam if doesn't have, warn if both have callbacks
				if(  pChildVar->m_fnChangeCallback )
				{
					if ( !pParentVar->m_fnChangeCallback )
					{
						const_cast<ConVar*>( pParentVar )->m_fnChangeCallback = pChildVar->m_fnChangeCallback;
					}
					else
					{
						Warning( "Convar %s has multiple different change callbacks\n", variable->GetName() );
					}
				}

				// make sure we don't have conflicting help strings.
				if ( pChildVar->m_pszHelpString && Q_strlen( pChildVar->m_pszHelpString ) != 0 )
				{
					if ( pParentVar->m_pszHelpString && Q_strlen( pParentVar->m_pszHelpString ) != 0 )
					{
						if ( Q_stricmp( pParentVar->m_pszHelpString, pChildVar->m_pszHelpString ) != 0 )
						{
							Warning( "Convar %s has multiple help strings:\n\tparent (wins): \"%s\"\n\tchild: \"%s\"\n", 
								variable->GetName(), pParentVar->m_pszHelpString, pChildVar->m_pszHelpString );
						}
					}
					else
					{
						const_cast<ConVar *>( pParentVar )->m_pszHelpString = pChildVar->m_pszHelpString;
					}
				}

				// make sure we don't have conflicting FCVAR_CHEAT flags.
				if ( ( pChildVar->m_nFlags & FCVAR_CHEAT ) != ( pParentVar->m_nFlags & FCVAR_CHEAT ) )
				{
					Warning( "Convar %s has conflicting FCVAR_CHEAT flags (child: %s, parent: %s, parent wins)\n", 
						variable->GetName(), ( pChildVar->m_nFlags & FCVAR_CHEAT ) ? "FCVAR_CHEAT" : "no FCVAR_CHEAT",
						( pParentVar->m_nFlags & FCVAR_CHEAT ) ? "FCVAR_CHEAT" : "no FCVAR_CHEAT" );
				}

				// make sure we don't have conflicting FCVAR_REPLICATED flags.
				if ( ( pChildVar->m_nFlags & FCVAR_REPLICATED ) != ( pParentVar->m_nFlags & FCVAR_REPLICATED ) )
				{
					Warning( "Convar %s has conflicting FCVAR_REPLICATED flags (child: %s, parent: %s, parent wins)\n", 
						variable->GetName(), ( pChildVar->m_nFlags & FCVAR_REPLICATED ) ? "FCVAR_REPLICATED" : "no FCVAR_REPLICATED",
						( pParentVar->m_nFlags & FCVAR_REPLICATED ) ? "FCVAR_REPLICATED" : "no FCVAR_REPLICATED" );
				}

				// make sure we don't have conflicting FCVAR_DONTRECORD flags.
				if ( ( pChildVar->m_nFlags & FCVAR_DONTRECORD ) != ( pParentVar->m_nFlags & FCVAR_DONTRECORD ) )
				{
					Warning( "Convar %s has conflicting FCVAR_DONTRECORD flags (child: %s, parent: %s, parent wins)\n", 
						variable->GetName(), ( pChildVar->m_nFlags & FCVAR_DONTRECORD ) ? "FCVAR_DONTRECORD" : "no FCVAR_DONTRECORD",
						( pParentVar->m_nFlags & FCVAR_DONTRECORD ) ? "FCVAR_DONTRECORD" : "no FCVAR_DONTRECORD" );
				}
			}
		}

		variable->m_pNext = NULL;
		return;
	}

	// link the variable in
	variable->m_pNext = m_pConCommandList;
	m_pConCommandList = variable;
}

void CCvar::UnregisterConCommand( ConCommandBase *pCommandToRemove )
{
	// Not registered? Don't bother
	if ( !pCommandToRemove->IsRegistered() )
		return;

	pCommandToRemove->m_bRegistered = false;

	// FIXME: Should we make this a doubly-linked list? Would remove faster
	ConCommandBase *pPrev = NULL;
	for( ConCommandBase *pCommand = m_pConCommandList; pCommand; pCommand = pCommand->m_pNext )
	{
		if ( pCommand != pCommandToRemove )
		{
			pPrev = pCommand;
			continue;
		}

		if ( pPrev == NULL )
		{
			m_pConCommandList = pCommand->m_pNext;
		}
		else
		{
			pPrev->m_pNext = pCommand->m_pNext;
		}
		pCommand->m_pNext = NULL;
		break;
	}
}

// Crash here in TF2, so I'm adding some debugging stuff.
#ifdef WIN32
#pragma optimize( "", off )
#endif
void CCvar::UnregisterConCommands( CVarDLLIdentifier_t id )
{
	ConCommandBase	*pNewList;
	ConCommandBase  *pCommand, *pNext;

	int iCommandsLooped = 0;

	pNewList = NULL;
	pCommand = m_pConCommandList;
	while ( pCommand )
	{
		pNext = pCommand->m_pNext;
		if ( pCommand->GetDLLIdentifier() != id )
		{
			pCommand->m_pNext = pNewList;
			pNewList = pCommand;
		}
		else
		{
			// Unlink
			pCommand->m_bRegistered = false;
			pCommand->m_pNext = NULL;
		}

		pCommand = pNext;
		iCommandsLooped++;
	}

	m_pConCommandList = pNewList;
}
#ifdef WIN32
#pragma optimize( "", on )
#endif


//-----------------------------------------------------------------------------
// Finds base commands 
//-----------------------------------------------------------------------------
const ConCommandBase *CCvar::FindCommandBase( const char *name ) const
{
	const ConCommandBase *cmd = GetCommands();
	for ( ; cmd; cmd = cmd->GetNext() )
	{
		if ( !Q_stricmp( name, cmd->GetName() ) )
			return cmd;
	}
	return NULL;
}

ConCommandBase *CCvar::FindCommandBase( const char *name )
{
	ConCommandBase *cmd = GetCommands();
	for ( ; cmd; cmd = cmd->GetNext() )
	{
		if ( !Q_stricmp( name, cmd->GetName() ) )
			return cmd;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose Finds ConVars
//-----------------------------------------------------------------------------
const ConVar *CCvar::FindVar( const char *var_name ) const
{
	VPROF_INCREMENT_COUNTER( "CCvar::FindVar", 1 );
	VPROF( "CCvar::FindVar" );
	const ConCommandBase *var = FindCommandBase( var_name );
	if ( !var || var->IsCommand() )
		return NULL;
	
	return static_cast<const ConVar*>(var);
}

ConVar *CCvar::FindVar( const char *var_name )
{
	VPROF_INCREMENT_COUNTER( "CCvar::FindVar", 1 );
	VPROF( "CCvar::FindVar" );
	ConCommandBase *var = FindCommandBase( var_name );
	if ( !var || var->IsCommand() )
		return NULL;
	
	return static_cast<ConVar*>( var );
}


//-----------------------------------------------------------------------------
// Purpose Finds ConCommands
//-----------------------------------------------------------------------------
const ConCommand *CCvar::FindCommand( const char *pCommandName ) const
{
	const ConCommandBase *var = FindCommandBase( pCommandName );
	if ( !var || !var->IsCommand() )
		return NULL;

	return static_cast<const ConCommand*>(var);
}

ConCommand *CCvar::FindCommand( const char *pCommandName )
{
	ConCommandBase *var = FindCommandBase( pCommandName );
	if ( !var || !var->IsCommand() )
		return NULL;

	return static_cast<ConCommand*>( var );
}


const char* CCvar::GetCommandLineValue( const char *pVariableName )
{
	int nLen = Q_strlen(pVariableName);
	char *pSearch = (char*)stackalloc( nLen + 2 );
	pSearch[0] = '+';
	memcpy( &pSearch[1], pVariableName, nLen + 1 );
	return CommandLine()->ParmValue( pSearch );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConCommandBase *CCvar::GetCommands( void )
{
	return m_pConCommandList;
}

const ConCommandBase *CCvar::GetCommands( void ) const
{
	return m_pConCommandList;
}


//-----------------------------------------------------------------------------
// Install, remove global callbacks
//-----------------------------------------------------------------------------
void CCvar::InstallGlobalChangeCallback( FnChangeCallback_t callback )
{
	Assert( callback && m_GlobalChangeCallbacks.Find( callback ) < 0 );
	m_GlobalChangeCallbacks.AddToTail( callback );
}

void CCvar::RemoveGlobalChangeCallback( FnChangeCallback_t callback )
{
	Assert( callback );
	m_GlobalChangeCallbacks.FindAndRemove( callback );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCvar::CallGlobalChangeCallbacks( ConVar *var, const char *pOldString, float flOldValue )
{
	int nCallbackCount = m_GlobalChangeCallbacks.Count();
	for ( int i = 0; i < nCallbackCount; ++i )
	{
		(*m_GlobalChangeCallbacks[i])( var, pOldString, flOldValue );
	}
}


//-----------------------------------------------------------------------------
// Sets convars containing the flags to their default value
//-----------------------------------------------------------------------------
void CCvar::RevertFlaggedConVars( int nFlag )
{
	for (const ConCommandBase *var= GetCommands() ; var ; var=var->GetNext())
	{
		if ( var->IsCommand() )
			continue;

		ConVar *pCvar = ( ConVar * )var;

		if ( !pCvar->IsFlagSet( nFlag ) )
			continue;

		// It's == to the default value, don't count
		if ( !Q_stricmp( pCvar->GetDefault(), pCvar->GetString() ) )
			continue;

		pCvar->Revert();

		// DevMsg( "%s = \"%s\" (reverted)\n", cvar->GetName(), cvar->GetString() );
	}
}


//-----------------------------------------------------------------------------
// Deal with queued material system convars
//-----------------------------------------------------------------------------
bool CCvar::IsMaterialThreadSetAllowed( ) const
{
	Assert( ThreadInMainThread() );
	return m_bMaterialSystemThreadSetAllowed;
}

void CCvar::QueueMaterialThreadSetValue( ConVar *pConVar, const char *pValue )
{
	Assert( ThreadInMainThread() );
	int j = m_QueuedConVarSets.AddToTail();
	m_QueuedConVarSets[j].m_pConVar = pConVar;
	m_QueuedConVarSets[j].m_nType = CONVAR_SET_STRING;
	m_QueuedConVarSets[j].m_String = pValue;
}

void CCvar::QueueMaterialThreadSetValue( ConVar *pConVar, int nValue )
{
	Assert( ThreadInMainThread() );
	int j = m_QueuedConVarSets.AddToTail();
	m_QueuedConVarSets[j].m_pConVar = pConVar;
	m_QueuedConVarSets[j].m_nType = CONVAR_SET_INT;
	m_QueuedConVarSets[j].m_nInt = nValue;
}

void CCvar::QueueMaterialThreadSetValue( ConVar *pConVar, float flValue )
{
	Assert( ThreadInMainThread() );
	int j = m_QueuedConVarSets.AddToTail();
	m_QueuedConVarSets[j].m_pConVar = pConVar;
	m_QueuedConVarSets[j].m_nType = CONVAR_SET_FLOAT;
	m_QueuedConVarSets[j].m_flFloat = flValue;
}

bool CCvar::HasQueuedMaterialThreadConVarSets() const
{
	Assert( ThreadInMainThread() );
	return m_QueuedConVarSets.Count() > 0;
}

int CCvar::ProcessQueuedMaterialThreadConVarSets()
{
	Assert( ThreadInMainThread() );
	m_bMaterialSystemThreadSetAllowed = true;

	int nUpdateFlags = 0;
	int nCount = m_QueuedConVarSets.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const QueuedConVarSet_t& set = m_QueuedConVarSets[i];
		switch( set.m_nType )
		{
		case CONVAR_SET_FLOAT:
			set.m_pConVar->SetValue( set.m_flFloat );
			break;
		case CONVAR_SET_INT:
			set.m_pConVar->SetValue( set.m_nInt );
			break;
		case CONVAR_SET_STRING:
			set.m_pConVar->SetValue( set.m_String );
			break;
		}

		nUpdateFlags |= set.m_pConVar->GetFlags() & FCVAR_MATERIAL_THREAD_MASK;
	}

	m_QueuedConVarSets.RemoveAll(); 
	m_bMaterialSystemThreadSetAllowed = false;
	return nUpdateFlags;
}


//-----------------------------------------------------------------------------
// Display queued messages
//-----------------------------------------------------------------------------
void CCvar::DisplayQueuedMessages( )
{
	// Display any queued up messages
	if ( m_TempConsoleBuffer.TellPut() == 0 )
		return;

	Color clr;
	int nStringLength;
	while( m_TempConsoleBuffer.IsValid() )
	{
		int nType = m_TempConsoleBuffer.GetChar();
		if ( nType == CONSOLE_COLOR_PRINT )
		{
			clr.SetRawColor( m_TempConsoleBuffer.GetInt() );
		}
		nStringLength = m_TempConsoleBuffer.PeekStringLength();
		char* pTemp = (char*)stackalloc( nStringLength + 1 );
		m_TempConsoleBuffer.GetStringManualCharCount( pTemp, nStringLength + 1 );

		switch( nType )
		{
		case CONSOLE_COLOR_PRINT:
			ConsoleColorPrintf( clr, pTemp );
			break;

		case CONSOLE_PRINT:
			ConsolePrintf( pTemp );
			break;

		case CONSOLE_DPRINT:
			ConsoleDPrintf( pTemp );
			break;
		}
	}

	m_TempConsoleBuffer.Purge();
}


//-----------------------------------------------------------------------------
// Install a console printer
//-----------------------------------------------------------------------------
void CCvar::InstallConsoleDisplayFunc( IConsoleDisplayFunc* pDisplayFunc )
{
	Assert( m_DisplayFuncs.Find( pDisplayFunc ) < 0 );
	m_DisplayFuncs.AddToTail( pDisplayFunc );
	DisplayQueuedMessages();
}

void CCvar::RemoveConsoleDisplayFunc( IConsoleDisplayFunc* pDisplayFunc )
{
	m_DisplayFuncs.FindAndRemove( pDisplayFunc );
}

void CCvar::ConsoleColorPrintf( const Color& clr, const char *pFormat, ... ) const
{
	char temp[ 8192 ];
	va_list argptr;
	va_start( argptr, pFormat );
	_vsnprintf( temp, sizeof( temp ) - 1, pFormat, argptr );
	va_end( argptr );
	temp[ sizeof( temp ) - 1 ] = 0;

	int c = m_DisplayFuncs.Count();
	if ( c == 0 )
	{
		m_TempConsoleBuffer.PutChar( CONSOLE_COLOR_PRINT );
		m_TempConsoleBuffer.PutInt( clr.GetRawColor() );
		m_TempConsoleBuffer.PutString( temp );
		return;
	}

	for ( int i = 0 ; i < c; ++i )
	{
		m_DisplayFuncs[ i ]->ColorPrint( clr, temp );
	}
}

void CCvar::ConsolePrintf( const char *pFormat, ... ) const
{
	char temp[ 8192 ];
	va_list argptr;
	va_start( argptr, pFormat );
	_vsnprintf( temp, sizeof( temp ) - 1, pFormat, argptr );
	va_end( argptr );
	temp[ sizeof( temp ) - 1 ] = 0;

	int c = m_DisplayFuncs.Count();
	if ( c == 0 )
	{
		m_TempConsoleBuffer.PutChar( CONSOLE_PRINT );
		m_TempConsoleBuffer.PutString( temp );
		return;
	}

	for ( int i = 0 ; i < c; ++i )
	{
		m_DisplayFuncs[ i ]->Print( temp );
	}
}

void CCvar::ConsoleDPrintf( const char *pFormat, ... ) const
{
	char temp[ 8192 ];
	va_list argptr;
	va_start( argptr, pFormat );
	_vsnprintf( temp, sizeof( temp ) - 1, pFormat, argptr );
	va_end( argptr );
	temp[ sizeof( temp ) - 1 ] = 0;

	int c = m_DisplayFuncs.Count();
	if ( c == 0 )
	{
		m_TempConsoleBuffer.PutChar( CONSOLE_DPRINT );
		m_TempConsoleBuffer.PutString( temp );
		return;
	}

	for ( int i = 0 ; i < c; ++i )
	{
		m_DisplayFuncs[ i ]->DPrint( temp );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
#if defined( _X360 )

void CCvar::PublishToVXConsole()
{
	const char *commands[4096];
	const char *helptext[4096];
	const ConCommandBase *pCur;
	int	numCommands = 0;

	// iterate and publish commands to the remote console
	for ( pCur = m_pConCommandList; pCur; pCur=pCur->GetNext() )
	{
		// add unregistered commands to list
		if ( numCommands < sizeof(commands)/sizeof(commands[0]) )
		{
			commands[numCommands] = pCur->GetName();
			helptext[numCommands] = pCur->GetHelpText();
			numCommands++;
		}
	}

	if ( numCommands )
	{
		XBX_rAddCommands( numCommands, commands, helptext );
	}
}

#endif


//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------
void CCvar::Find( const CCommand &args )
{
	const char *search;
	const ConCommandBase *var;

	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  find <string>\n" );
		return;
	}

	// Get substring to find
	search = args[1];
				 
	// Loop through vars and print out findings
	for ( var = GetCommands(); var; var=var->GetNext() )
	{
		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		if ( !Q_stristr( var->GetName(), search ) &&
			!Q_stristr( var->GetHelpText(), search ) )
			continue;

		ConVar_PrintDescription( var );	
	}	
}


