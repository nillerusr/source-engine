//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
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
#include "tier1/keyvalues.h"
#include "tier1/convar.h"
#include "tier0/vprof.h"
#include "tier1/tier1.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include "tier1/fmtstr.h"

#ifdef _X360
#include "xbox/xbox_console.h"
#elif defined( _PS3 )
#include "ps3/ps3_console.h"
#endif

#ifdef POSIX
#include <wctype.h>
#include <wchar.h>

#define VPROJ_INCREMENT_COUNTER(a, b) /* */
#define VPROJ(a) /* */
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
		if ( !V_stricmp( pInterfaceName, CVAR_QUERY_INTERFACE_VERSION ) )
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

#include "concommandhash.h"

//-----------------------------------------------------------------------------
// Default implementation
//-----------------------------------------------------------------------------
class CCvar : public CBaseAppSystem< ICvar >
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

#if defined( USE_VXCONSOLE )
	virtual void			PublishToVXConsole( );
#endif

	virtual void			SetMaxSplitScreenSlots( int nSlots );
	virtual int				GetMaxSplitScreenSlots() const;

	virtual void			AddSplitScreenConVars();
	virtual void			RemoveSplitScreenConVars( CVarDLLIdentifier_t id );

	virtual int				GetConsoleDisplayFuncCount() const;
	virtual void			GetConsoleText( int nDisplayFuncIndex, char *pchText, size_t bufSize ) const;
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
	CConCommandHash						m_CommandHash;

	// temporary console area so we can store prints before console display funs are installed
	mutable CUtlBuffer					m_TempConsoleBuffer;
	int									m_nMaxSplitScreenSlots;

protected:

	// internals for  ICVarIterator
	class CCVarIteratorInternal : public ICVarIteratorInternal
	{
	public:
		CCVarIteratorInternal( CCvar *outer ) 
			: m_pOuter( outer ), m_pHash( &outer->m_CommandHash ), // remember my CCvar,
			m_hashIter( -1, -1 ) // and invalid iterator
		{}
		virtual void		SetFirst( void );
		virtual void		Next( void );
		virtual	bool		IsValid( void );
		virtual ConCommandBase *Get( void );
	protected:
		CCvar * const m_pOuter;
		CConCommandHash * const m_pHash;
		CConCommandHash::CCommandHashIterator_t m_hashIter;
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

	struct SplitScreenConVar_t
	{
		SplitScreenConVar_t()
		{
			Reset();
		}

		void Reset()
		{
			m_VarName = "";
			m_pVar = NULL;
		}

		CUtlString					m_VarName;
		CSplitScreenAddedConVar		*m_pVar;
	};

	struct SplitScreenAddedConVars_t
	{
		SplitScreenAddedConVars_t() 
		{
			for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS - 1; ++i )
			{
				m_Vars[ i ].Reset();
			}
		}

		// Var names need "static" buffers...
		SplitScreenConVar_t		m_Vars[ MAX_SPLITSCREEN_CLIENTS - 1 ];
	};

	CUtlMap< ConVar *, SplitScreenAddedConVars_t > m_SplitScreenAddedConVarsMap;
	CUtlVector< QueuedConVarSet_t > m_QueuedConVarSets;
	bool m_bMaterialSystemThreadSetAllowed;

private:
	// Standard console commands -- DO NOT PLACE ANY HIGHER THAN HERE BECAUSE THESE MUST BE THE FIRST TO DESTRUCT
	CON_COMMAND_MEMBER_F( CCvar, "find", Find, "Find concommands with the specified string in their name/help text.", 0 )
#ifdef _DEBUG
	CON_COMMAND_MEMBER_F( CCvar, "ccvar_hash_report", HashReport, "report info on bucket distribution of internal hash.", 0 )
#endif
};

void CCvar::CCVarIteratorInternal::SetFirst( void )
{
	m_hashIter = m_pHash->First();
}

void CCvar::CCVarIteratorInternal::Next( void )
{
	m_hashIter = m_pHash->Next( m_hashIter );
}

bool CCvar::CCVarIteratorInternal::IsValid( void )
{
	return m_pHash->IsValidIterator( m_hashIter );
}

ConCommandBase *CCvar::CCVarIteratorInternal::Get( void )
{
	Assert( IsValid( ) );
	return (*m_pHash)[m_hashIter];
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
CCvar::CCvar() : m_TempConsoleBuffer( 0, 1024 ), m_SplitScreenAddedConVarsMap( 0, 0, DefLessFunc( ConVar * ) )
{
	m_nNextDLLIdentifier = 0;
	m_pConCommandList = NULL;
	m_nMaxSplitScreenSlots = 1;
	m_bMaterialSystemThreadSetAllowed = false;
	m_CommandHash.Init();
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

	Assert( m_SplitScreenAddedConVarsMap.Count() == 0 );
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
	const ConCommandBase *pOther = FindCommandBase( variable->GetName() );
	if ( pOther )
	{
		if ( variable->IsCommand() || pOther->IsCommand() )
		{
			Warning( "WARNING: unable to link %s and %s because one or more is a ConCommand.\n", variable->GetName(), pOther->GetName() );
		}
		else
		{
			// This cast is ok because we make sure they're ConVars above.
			ConVar *pChildVar = const_cast< ConVar* >( static_cast< const ConVar* >( variable ) );
			ConVar *pParentVar = const_cast< ConVar* >( static_cast< const ConVar* >( pOther ) );

			// See if it's a valid linkage
			if ( s_pCVarQuery->AreConVarsLinkable( pChildVar, pParentVar ) )
			{
				// Make sure the default values are the same (but only spew about this for FCVAR_REPLICATED)
				if(  pChildVar->m_pszDefaultValue && pParentVar->m_pszDefaultValue &&
					 pChildVar->IsFlagSet( FCVAR_REPLICATED ) && pParentVar->IsFlagSet( FCVAR_REPLICATED ) )
				{
					if( V_stricmp( pChildVar->m_pszDefaultValue, pParentVar->m_pszDefaultValue ) != 0 )
					{
						Warning( "Parent and child ConVars with different default values! %s child: %s parent: %s (parent wins)\n", 
							variable->GetName(), pChildVar->m_pszDefaultValue, pParentVar->m_pszDefaultValue );
					}
				}

				pChildVar->m_pParent = pParentVar->m_pParent;

				// Absorb material thread related convar flags
				pParentVar->m_nFlags |= pChildVar->m_nFlags & ( FCVAR_MATERIAL_THREAD_MASK | FCVAR_ACCESSIBLE_FROM_THREADS );

				// Transfer children's callbacks to parent
				if ( pChildVar->m_fnChangeCallbacks.Count() )
				{
					for ( int i = 0; i < pChildVar->m_fnChangeCallbacks.Count(); ++i )
					{
						pParentVar->m_fnChangeCallbacks.AddToTail( pChildVar->m_fnChangeCallbacks[ i ] );
					}
					// Wipe child callbacks
					pChildVar->m_fnChangeCallbacks.RemoveAll();
				}

				// make sure we don't have conflicting help strings.
				if ( pChildVar->m_pszHelpString && V_strlen( pChildVar->m_pszHelpString ) != 0 )
				{
					if ( pParentVar->m_pszHelpString && V_strlen( pParentVar->m_pszHelpString ) != 0 )
					{
						if ( V_stricmp( pParentVar->m_pszHelpString, pChildVar->m_pszHelpString ) != 0 )
						{
							Warning( "Convar %s has multiple help strings:\n\tparent (wins): \"%s\"\n\tchild: \"%s\"\n", 
								variable->GetName(), pParentVar->m_pszHelpString, pChildVar->m_pszHelpString );
						}
					}
					else
					{
						pParentVar->m_pszHelpString = pChildVar->m_pszHelpString;
					}
				}

				// make sure we don't have conflicting FCVAR_*** flags.
				static int const nFlags[] =
					{ FCVAR_CHEAT, FCVAR_REPLICATED, FCVAR_DONTRECORD, FCVAR_ARCHIVE, FCVAR_ARCHIVE_GAMECONSOLE };
				static char const * const szFlags[] =
					{ "FCVAR_CHEAT", "FCVAR_REPLICATED", "FCVAR_DONTRECORD", "FCVAR_ARCHIVE", "FCVAR_ARCHIVE_GAMECONSOLE" };

				COMPILE_TIME_ASSERT( ARRAYSIZE( nFlags ) == ARRAYSIZE( szFlags ) );

				for ( int k = 0; k < ARRAYSIZE( nFlags ); ++ k )
				{
					if ( ( pChildVar->m_nFlags & nFlags[k] ) != ( pParentVar->m_nFlags & nFlags[k] ) )
					{
						Warning( "Convar %s has conflicting %s flags (child: %s%s, parent: %s%s, parent wins)\n", 
							variable->GetName(), szFlags[k],
							( pChildVar->m_nFlags & nFlags[k] ) ? "has " : "no ", szFlags[k],
							( pParentVar->m_nFlags & nFlags[k] ) ? "has " : "no ", szFlags[k] );
					}
				}
			}
		}

		variable->m_pNext = NULL;
		return;
	}

	// link the variable in
	variable->m_pNext = m_pConCommandList;
	m_pConCommandList = variable;

	AssertMsg1(FindCommandBase(variable->GetName()) == NULL, "Console command %s added twice!",
		variable->GetName());
	m_CommandHash.Insert(variable);
}

void CCvar::AddSplitScreenConVars()
{
	if ( m_nMaxSplitScreenSlots == 1 )
		return;

	for( ConCommandBase *pCommand = m_pConCommandList; pCommand; pCommand = pCommand->m_pNext )
	{
		if ( pCommand->IsCommand() )
			continue;

		ConVar *pConVar = static_cast< ConVar * >( pCommand );

		if ( !pConVar->IsFlagSet( FCVAR_SS ) )
			continue;

		// See if it's already mapped in
		int idx = m_SplitScreenAddedConVarsMap.Find( pConVar );
		if ( idx == m_SplitScreenAddedConVarsMap.InvalidIndex() )
		{
			idx = m_SplitScreenAddedConVarsMap.Insert( pConVar );
		}

		SplitScreenAddedConVars_t &info = m_SplitScreenAddedConVarsMap[ idx ];
		for ( int i = 1 ; i < m_nMaxSplitScreenSlots; ++i )
		{
			// Already registered it
			if ( info.m_Vars[ i - 1 ].m_pVar )
				continue;

			// start at name2, etc.
			info.m_Vars[ i - 1 ].m_VarName = CFmtStr( "%s%d", pConVar->GetName(), i + 1 );

			CSplitScreenAddedConVar *pVar = new CSplitScreenAddedConVar( i, info.m_Vars[ i - 1 ].m_VarName.Get(), pConVar );
			info.m_Vars[ i - 1 ].m_pVar = pVar;
			pVar->SetSplitScreenPlayerSlot( i );

			RegisterConCommand( pVar );
		}
	}

	ConCommandBase::s_pConCommandBases = NULL;
}

void CCvar::RemoveSplitScreenConVars( CVarDLLIdentifier_t id )

{
	if ( m_nMaxSplitScreenSlots == 1 )
	{
		Assert( m_SplitScreenAddedConVarsMap.Count() == 0 );
		return;
	}

	CUtlVector< ConVar * > deleted;

	FOR_EACH_MAP( m_SplitScreenAddedConVarsMap, i )
	{
		ConVar *key = m_SplitScreenAddedConVarsMap.Key( i );

		if ( key->GetDLLIdentifier() != id )
		{
			continue;
		}
		
		SplitScreenAddedConVars_t &info = m_SplitScreenAddedConVarsMap[ i ];

 

		for ( int i = 1 ; i < m_nMaxSplitScreenSlots; ++i )
		{

			if ( info.m_Vars[ i - 1 ].m_pVar )
			{
				UnregisterConCommand( info.m_Vars[ i - 1 ].m_pVar );
				delete info.m_Vars[ i - 1 ].m_pVar;
				info.m_Vars[ i - 1 ].m_pVar = NULL;
			}
		}
		deleted.AddToTail( key );
	}

	for ( int i = 0; i < deleted.Count(); ++i )
	{
		m_SplitScreenAddedConVarsMap.Remove( deleted[ i ] );
	}
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
		m_CommandHash.Remove(m_CommandHash.Find(pCommand));
		break;
	}
}

void CCvar::UnregisterConCommands( CVarDLLIdentifier_t id )
{
	ConCommandBase	*pNewList;
	ConCommandBase  *pCommand, *pNext;

	pNewList = NULL;

	m_CommandHash.Purge( true );
	pCommand = m_pConCommandList;
	while ( pCommand )
	{
		pNext = pCommand->m_pNext;
		if ( pCommand->GetDLLIdentifier() != id )
		{
			pCommand->m_pNext = pNewList;
			pNewList = pCommand;

			m_CommandHash.Insert( pCommand );
		}
		else
		{
			// Unlink
			pCommand->m_bRegistered = false;
			pCommand->m_pNext = NULL;

		}

		pCommand = pNext;
	}

	m_pConCommandList = pNewList;
}


//-----------------------------------------------------------------------------
// Finds base commands 
//-----------------------------------------------------------------------------
const ConCommandBase *CCvar::FindCommandBase( const char *name ) const
{
	VPROF_INCREMENT_COUNTER( "CCvar::FindCommandBase", 1 );
	VPROF_BUDGET( "CCvar::FindCommandBase", VPROF_BUDGETGROUP_CVAR_FIND );

	return m_CommandHash.FindPtr( name );
}

ConCommandBase *CCvar::FindCommandBase( const char *name )
{
	VPROF_INCREMENT_COUNTER( "CCvar::FindCommandBase", 1 );
	VPROF_BUDGET( "CCvar::FindCommandBase", VPROF_BUDGETGROUP_CVAR_FIND );

	return m_CommandHash.FindPtr( name );
}


//-----------------------------------------------------------------------------
// Purpose Finds ConVars
//-----------------------------------------------------------------------------
const ConVar *CCvar::FindVar( const char *var_name ) const
{
	const ConCommandBase *var = FindCommandBase( var_name );
	if ( !var )
	{
		return NULL;
	}
	else
	{
		if (var->IsCommand())
		{
			AssertMsg1( false , "Tried to look up command %s as if it were a variable.\n", var_name );
			return NULL;
		}
	}
	
	return static_cast<const ConVar*>(var);
}

ConVar *CCvar::FindVar( const char *var_name )
{
	ConCommandBase *var = FindCommandBase( var_name );
	if ( !var )
	{
		return NULL;
	}
	else
	{
		if (var->IsCommand())
		{
			AssertMsg1( false , "Tried to look up command %s as if it were a variable.\n", var_name );
			return NULL;
		}
	}
	
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
	int nLen = V_strlen(pVariableName);
	char *pSearch = (char*)stackalloc( nLen + 2 );
	pSearch[0] = '+';
	memcpy( &pSearch[1], pVariableName, nLen + 1 );
	return CommandLine()->ParmValue( pSearch );
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
	for ( CConCommandHash::CCommandHashIterator_t i = m_CommandHash.First() ;
		  m_CommandHash.IsValidIterator( i ) ; 
		  i = m_CommandHash.Next( i ) )
	{
		ConCommandBase *var = m_CommandHash[ i ];
		if ( var->IsCommand() )
			continue;

		ConVar *cvar = ( ConVar * )var;

		if ( !cvar->IsFlagSet( nFlag ) )
			continue;

		// It's == to the default value, don't count
		if ( !V_stricmp( cvar->GetDefault(), cvar->GetString() ) )
			continue;

		cvar->Revert();
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
		m_TempConsoleBuffer.GetString( pTemp );

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

int CCvar::GetConsoleDisplayFuncCount() const
{
	return m_DisplayFuncs.Count();
}

void CCvar::GetConsoleText( int nDisplayFuncIndex, char *pchText, size_t bufSize ) const
{
	m_DisplayFuncs[ nDisplayFuncIndex ]->GetConsoleText( pchText, bufSize );
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
#if defined( USE_VXCONSOLE )
#ifdef _PS3 
/*
Here's a terrible hack.
In porting the part of the game that speaks to VXConsole, EA chose to
write it as a cluster of global functions, instead of a class interface like
Aaron did with IXboxConsole. Some of these globals need access to symbols 
inside the engine, so they are defined there. However, CCvar is inside vstdlib.
In the EA build this didn't make a difference because everything was a huge
monolithic executable, and you could just access any symbol from anywhere.
In our build, with its PRXes, that doesn't fly.
So, the proper solution to this problem is to wrap all of the PS3 vxconsole
stuff in an interface, put it inside vstlib, create the dcim connection there,
and then export the interface pointer. The engine meanwhile would export the 
symbols the vxlib needs, and then we give that interface class inside
vstlib a pointer to the engine once the engine is available. 
Right now however I just want to get the thing working with as little modification
as possible so I can fix the vxconsole windows app itself and hopefully get 
bidirectional TTY to our game. So, instead of the proper solution,
I'm just duct-taping everything together by simply passing a pointer to the engine
symbol this function needs whenever I call it. 
Blech. I'll fix it later. 
-egr 4/29/10. (is it later than September 2010? go call egr and make fun of him.)
*/
void CCvar::PublishToVXConsole()
#else
void CCvar::PublishToVXConsole()
#endif
{
	const char *commands[6*1024];
	const char *helptext[6*1024];
	int	numCommands = 0;

	// iterate and publish commands to the remote console
	for ( CConCommandHash::CCommandHashIterator_t i = m_CommandHash.First() ;
		m_CommandHash.IsValidIterator( i ) ; 
		i = m_CommandHash.Next( i ) )
	{
		ConCommandBase *pCur = m_CommandHash[ i ];
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
#ifdef _PS3
		g_pValvePS3Console->AddCommands( numCommands, commands, helptext );
#else
		XBX_rAddCommands( numCommands, commands, helptext );
#endif
	}
}

#endif


static bool ConVarSortFunc( ConCommandBase * const &lhs, ConCommandBase * const &rhs )
{
	return CaselessStringLessThan( lhs->GetName(), rhs->GetName() );
}

//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------
void CCvar::Find( const CCommand &args )
{
	const char *search;

	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  find <string>\n" );
		return;
	}

	// Get substring to find
	search = args[1];

	CUtlRBTree< ConCommandBase *, int > sorted( 0, 0, ConVarSortFunc );
				 
	// Loop through vars and print out findings
	for ( CConCommandHash::CCommandHashIterator_t i = m_CommandHash.First() ;
		m_CommandHash.IsValidIterator(i) ; 
		i = m_CommandHash.Next(i) )
	{
		ConCommandBase *var = m_CommandHash[ i ];
		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		if ( !V_stristr( var->GetName(), search ) &&
			!V_stristr( var->GetHelpText(), search ) )
			continue;

		sorted.Insert( var );
	}	

	for ( int i = sorted.FirstInorder(); i != sorted.InvalidIndex(); i = sorted.NextInorder( i ) )
	{
		ConVar_PrintDescription( sorted[ i ] );	
	}
}

#ifdef _DEBUG
void CCvar::HashReport( const CCommand &args )
{
	m_CommandHash.Report();
}
#endif


void CCvar::SetMaxSplitScreenSlots( int nSlots )
{
	m_nMaxSplitScreenSlots = nSlots;

	AddSplitScreenConVars();
}

int CCvar::GetMaxSplitScreenSlots() const
{
	return m_nMaxSplitScreenSlots;
}



//-----------------------------------------------------------------------------
// Console command hash data structure
//-----------------------------------------------------------------------------
CConCommandHash::CConCommandHash()
{
	Purge( true );
}

CConCommandHash::~CConCommandHash()
{
	Purge( false );
}

void CConCommandHash::Purge( bool bReinitialize )
{
	m_aBuckets.Purge();
	m_aDataPool.Purge();
	if ( bReinitialize )
	{
		Init();
	}
}

// Initialize.
void CConCommandHash::Init( void )
{
	// kNUM_BUCKETS must be a power of two.
	COMPILE_TIME_ASSERT((kNUM_BUCKETS & ( kNUM_BUCKETS - 1 )) == 0);

	// Set the bucket size.
	m_aBuckets.SetSize( kNUM_BUCKETS );
	for ( int iBucket = 0; iBucket < kNUM_BUCKETS; ++iBucket )
	{
		m_aBuckets[iBucket] = m_aDataPool.InvalidIndex();
	}

	// Calculate the grow size.
	int nGrowSize = 4 * kNUM_BUCKETS;
	m_aDataPool.SetGrowSize( nGrowSize );
}

//-----------------------------------------------------------------------------
// Purpose: Insert data into the hash table given its key (unsigned int), 
//			WITH a check to see if the element already exists within the hash.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::Insert( ConCommandBase *cmd )
{
	// Check to see if that key already exists in the buckets (should be unique).
	CCommandHashHandle_t hHash = Find( cmd );
	if( hHash != InvalidHandle() )
		return hHash;

	return FastInsert( cmd );
}
//-----------------------------------------------------------------------------
// Purpose: Insert data into the hash table given its key (unsigned int),
//          WITHOUT a check to see if the element already exists within the hash.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::FastInsert( ConCommandBase *cmd )
{
	// Get a new element from the pool.
	int iHashData = m_aDataPool.Alloc( true );
	HashEntry_t * pHashData = &m_aDataPool[iHashData];
	if ( !pHashData )
		return InvalidHandle();

	HashKey_t key = Hash(cmd);

	// Add data to new element.
	pHashData->m_uiKey = key;
	pHashData->m_Data = cmd;

	// Link element.
	int iBucket = key & kBUCKETMASK ; // HashFuncs::Hash( uiKey, m_uiBucketMask );
	m_aDataPool.LinkBefore( m_aBuckets[iBucket], iHashData );
	m_aBuckets[iBucket] = iHashData;

	return iHashData;	
}

//-----------------------------------------------------------------------------
// Purpose: Remove a given element from the hash.
//-----------------------------------------------------------------------------
void CConCommandHash::Remove( CCommandHashHandle_t hHash )
{
	HashEntry_t * entry = &m_aDataPool[hHash];
	HashKey_t iBucket = entry->m_uiKey & kBUCKETMASK ;
	if ( m_aBuckets[iBucket] == hHash )
	{
		// It is a bucket head.
		m_aBuckets[iBucket] = m_aDataPool.Next( hHash );
	}
	else
	{
		// Not a bucket head.
		m_aDataPool.Unlink( hHash );
	}

	// Remove the element.
	m_aDataPool.Remove( hHash );
}

//-----------------------------------------------------------------------------
// Purpose: Remove all elements from the hash
//-----------------------------------------------------------------------------
void CConCommandHash::RemoveAll( void )
{
	m_aBuckets.RemoveAll();
	m_aDataPool.RemoveAll();
}

//-----------------------------------------------------------------------------
// Find hash entry corresponding to a string name
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const char *name, HashKey_t hashkey) const
{
	// hash the "key" - get the correct hash table "bucket"
	int iBucket = hashkey & kBUCKETMASK;

	for ( datapool_t::IndexLocalType_t iElement = m_aBuckets[iBucket]; iElement != m_aDataPool.InvalidIndex(); iElement = m_aDataPool.Next( iElement ) )
	{
		const HashEntry_t &element = m_aDataPool[iElement];
		if ( element.m_uiKey == hashkey && // if hashes of strings match,
			 V_stricmp( name, element.m_Data->GetName() ) == 0) // then test the actual strings
		{
			return iElement;
		}
	}

	// found nuffink
	return InvalidHandle();
}

//-----------------------------------------------------------------------------
// Find a command in the hash.
//-----------------------------------------------------------------------------
CConCommandHash::CCommandHashHandle_t CConCommandHash::Find( const ConCommandBase *cmd ) const
{
	// Set this #if to 1 if the assert at bottom starts whining --
	// that indicates that a console command is being double-registered,
	// or something similarly nonfatally bad. With this #if 1, we'll search
	// by name instead of by pointer, which is more robust in the face
	// of double registered commands, but obviously slower.
#if 0 
	return Find(cmd->GetName());
#else
	HashKey_t hashkey = Hash(cmd);
	int iBucket = hashkey & kBUCKETMASK;

	// hunt through all entries in that bucket
	for ( datapool_t::IndexLocalType_t iElement = m_aBuckets[iBucket]; iElement != m_aDataPool.InvalidIndex(); iElement = m_aDataPool.Next( iElement ) )
	{
		const HashEntry_t &element = m_aDataPool[iElement];
		if ( element.m_uiKey == hashkey && // if the hashes match... 
			 element.m_Data  == cmd	) // and the pointers...
		{
			// in debug, test to make sure we don't have commands under the same name
			// or something goofy like that
			AssertMsg1( iElement == Find(cmd->GetName()),
				"ConCommand %s had two entries in the hash!", cmd->GetName() );
			
			// return this element
			return iElement;
		}
	}

	// found nothing.
#ifdef DBGFLAG_ASSERT // double check against search by name
	CCommandHashHandle_t dbghand = Find(cmd->GetName());

	AssertMsg1( InvalidHandle() == dbghand,
		"ConCommand %s couldn't be found by pointer, but was found by name!", cmd->GetName() );
#endif
	return InvalidHandle();
#endif
}


#ifdef _DEBUG
// Dump a report to MSG
void CConCommandHash::Report( void )
{
	Msg("Console command hash bucket load:\n");
	int total = 0;
	for ( int iBucket = 0 ; iBucket < kNUM_BUCKETS ; ++iBucket )
	{
		int count = 0;
		CCommandHashHandle_t iElement = m_aBuckets[iBucket]; // get the head of the bucket
		while ( iElement != m_aDataPool.InvalidIndex() )
		{
			++count;
			iElement = m_aDataPool.Next( iElement );
		}

		Msg( "%d: %d\n", iBucket, count );
		total += count;
	}

	Msg("\tAverage: %.1f\n", total / ((float)(kNUM_BUCKETS)));
}
#endif
