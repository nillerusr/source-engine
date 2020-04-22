//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $NoKeywords: $
//===========================================================================//

#ifndef CONVAR_H
#define CONVAR_H

#if _WIN32
#pragma once
#endif

#include "tier0/dbg.h"
#include "tier1/iconvar.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"
#include "color.h"
#include "icvar.h"

#ifdef _WIN32
#define FORCEINLINE_CVAR FORCEINLINE
#elif POSIX
#define FORCEINLINE_CVAR inline
#elif defined(_PS3)
#define FORCEINLINE_CVAR __attribute__((always_inline)) FORCEINLINE 
#else
#error "implement me"
#endif


//-----------------------------------------------------------------------------
// Uncomment me to test for threading issues for material system convars
// NOTE: You want to disable all threading when you do this
// +host_thread_mode 0 +r_threaded_particles 0 +sv_parallel_packentities 0 +sv_disable_querycache 0
//-----------------------------------------------------------------------------
//#define CONVAR_TEST_MATERIAL_THREAD_CONVARS 1


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ConVar;
class CCommand;
class ConCommand;
class ConCommandBase;
struct characterset_t;



//-----------------------------------------------------------------------------
// Any executable that wants to use ConVars need to implement one of
// these to hook up access to console variables.
//-----------------------------------------------------------------------------
class IConCommandBaseAccessor
{
public:
	// Flags is a combination of FCVAR flags in cvar.h.
	// hOut is filled in with a handle to the variable.
	virtual bool RegisterConCommandBase( ConCommandBase *pVar ) = 0;
};


//-----------------------------------------------------------------------------
// Helper method for console development
//-----------------------------------------------------------------------------
#if defined( USE_VXCONSOLE )
void ConVar_PublishToVXConsole();
#else
inline void ConVar_PublishToVXConsole() {}
#endif


//-----------------------------------------------------------------------------
// Called when a ConCommand needs to execute
//-----------------------------------------------------------------------------
typedef void ( *FnCommandCallbackV1_t )( void );
typedef void ( *FnCommandCallback_t )( const CCommand &command );

#define COMMAND_COMPLETION_MAXITEMS		64
#define COMMAND_COMPLETION_ITEM_LENGTH	64

//-----------------------------------------------------------------------------
// Returns 0 to COMMAND_COMPLETION_MAXITEMS worth of completion strings
//-----------------------------------------------------------------------------
typedef int  ( *FnCommandCompletionCallback )( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );


//-----------------------------------------------------------------------------
// Interface version
//-----------------------------------------------------------------------------
class ICommandCallback
{
public:
	virtual void CommandCallback( const CCommand &command ) = 0;
};

class ICommandCompletionCallback
{
public:
	virtual int  CommandCompletionCallback( const char *pPartial, CUtlVector< CUtlString > &commands ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: The base console invoked command/cvar interface
//-----------------------------------------------------------------------------
class ConCommandBase
{
	friend class CCvar;
	friend class ConVar;
	friend class ConCommand;
	friend void ConVar_Register( int nCVarFlag, IConCommandBaseAccessor *pAccessor );
	friend void ConVar_PublishToVXConsole();

	// FIXME: Remove when ConVar changes are done
	friend class CDefaultCvar;

public:
								ConCommandBase( void );
								ConCommandBase( const char *pName, const char *pHelpString = 0, 
									int flags = 0 );

	virtual						~ConCommandBase( void );

	virtual	bool				IsCommand( void ) const;

	// Check flag
	virtual bool				IsFlagSet( int flag ) const;
	// Set flag
	virtual void				AddFlags( int flags );
	// Clear flag
	virtual void				RemoveFlags( int flags );

	virtual int					GetFlags() const;

	// Return name of cvar
	virtual const char			*GetName( void ) const;

	// Return help text for cvar
	virtual const char			*GetHelpText( void ) const;

	// Deal with next pointer
	const ConCommandBase		*GetNext( void ) const;
	ConCommandBase				*GetNext( void );
	
	virtual bool				IsRegistered( void ) const;

	// Returns the DLL identifier
	virtual CVarDLLIdentifier_t	GetDLLIdentifier() const;

protected:
	virtual void				Create( const char *pName, const char *pHelpString = 0, 
									int flags = 0 );

	// Used internally by OneTimeInit to initialize/shutdown
	virtual void				Init();
	void						Shutdown();

	// Internal copy routine ( uses new operator from correct module )
	char						*CopyString( const char *from );

private:
	// Next ConVar in chain
	// Prior to register, it points to the next convar in the DLL.
	// Once registered, though, m_pNext is reset to point to the next
	// convar in the global list
	ConCommandBase				*m_pNext;

	// Has the cvar been added to the global list?
	bool						m_bRegistered;

	// Static data
	const char 					*m_pszName;
	const char 					*m_pszHelpString;
	
	// ConVar flags
	int							m_nFlags;

protected:
	// ConVars add themselves to this list for the executable. 
	// Then ConVar_Register runs through  all the console variables 
	// and registers them into a global list stored in vstdlib.dll
	static ConCommandBase		*s_pConCommandBases;

	// ConVars in this executable use this 'global' to access values.
	static IConCommandBaseAccessor	*s_pAccessor;
};


//-----------------------------------------------------------------------------
// Command tokenizer
//-----------------------------------------------------------------------------
class CCommand
{
public:
	CCommand();
	CCommand( int nArgC, const char **ppArgV );
	bool Tokenize( const char *pCommand, characterset_t *pBreakSet = NULL );
	void Reset();

	int ArgC() const;
	const char **ArgV() const;
	const char *ArgS() const;					// All args that occur after the 0th arg, in string form
	const char *GetCommandString() const;		// The entire command in string form, including the 0th arg
	const char *operator[]( int nIndex ) const;	// Gets at arguments
	const char *Arg( int nIndex ) const;		// Gets at arguments
	
	// Helper functions to parse arguments to commands.
	const char* FindArg( const char *pName ) const;
	int FindArgInt( const char *pName, int nDefaultVal ) const;

	static int MaxCommandLength();
	static characterset_t* DefaultBreakSet();

private:
	enum
	{
		COMMAND_MAX_ARGC = 64,
		COMMAND_MAX_LENGTH = 512,
	};

	int		m_nArgc;
	int		m_nArgv0Size;
	char	m_pArgSBuffer[ COMMAND_MAX_LENGTH ];
	char	m_pArgvBuffer[ COMMAND_MAX_LENGTH ];
	const char*	m_ppArgv[ COMMAND_MAX_ARGC ];
};

inline int CCommand::MaxCommandLength()
{
	return COMMAND_MAX_LENGTH - 1;
}

inline int CCommand::ArgC() const
{
	return m_nArgc;
}

inline const char **CCommand::ArgV() const
{
	return m_nArgc ? (const char**)m_ppArgv : NULL;
}

inline const char *CCommand::ArgS() const
{
	return m_nArgv0Size ? &m_pArgSBuffer[m_nArgv0Size] : "";
}

inline const char *CCommand::GetCommandString() const
{
	return m_nArgc ? m_pArgSBuffer : "";
}

inline const char *CCommand::Arg( int nIndex ) const
{
	// FIXME: Many command handlers appear to not be particularly careful
	// about checking for valid argc range. For now, we're going to
	// do the extra check and return an empty string if it's out of range
	if ( nIndex < 0 || nIndex >= m_nArgc )
		return "";
	return m_ppArgv[nIndex];
}

inline const char *CCommand::operator[]( int nIndex ) const
{
	return Arg( nIndex );
}


//-----------------------------------------------------------------------------
// Purpose: The console invoked command
//-----------------------------------------------------------------------------
class ConCommand : public ConCommandBase
{
friend class CCvar;

public:
	typedef ConCommandBase BaseClass;

	ConCommand( const char *pName, FnCommandCallbackV1_t callback, 
		const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 );
	ConCommand( const char *pName, FnCommandCallback_t callback, 
		const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 );
	ConCommand( const char *pName, ICommandCallback *pCallback, 
		const char *pHelpString = 0, int flags = 0, ICommandCompletionCallback *pCommandCompletionCallback = 0 );

	virtual ~ConCommand( void );

	virtual	bool IsCommand( void ) const;

	virtual int AutoCompleteSuggest( const char *partial, CUtlVector< CUtlString > &commands );

	virtual bool CanAutoComplete( void );

	// Invoke the function
	virtual void Dispatch( const CCommand &command );

private:
	// NOTE: To maintain backward compat, we have to be very careful:
	// All public virtual methods must appear in the same order always
	// since engine code will be calling into this code, which *does not match*
	// in the mod code; it's using slightly different, but compatible versions
	// of this class. Also: Be very careful about adding new fields to this class.
	// Those fields will not exist in the version of this class that is instanced
	// in mod code.

	// Call this function when executing the command
	union
	{
		FnCommandCallbackV1_t m_fnCommandCallbackV1;
		FnCommandCallback_t m_fnCommandCallback;
		ICommandCallback *m_pCommandCallback; 
	};

	union
	{
		FnCommandCompletionCallback	m_fnCompletionCallback;
		ICommandCompletionCallback *m_pCommandCompletionCallback;
	};

	bool m_bHasCompletionCallback : 1;
	bool m_bUsingNewCommandCallback : 1;
	bool m_bUsingCommandCallbackInterface : 1;
};


//-----------------------------------------------------------------------------
// Purpose: A console variable
//-----------------------------------------------------------------------------
class ConVar : public ConCommandBase, public IConVar
{
friend class CCvar;
friend class ConVarRef;
friend class SplitScreenConVarRef;

public:
	typedef ConCommandBase BaseClass;

								ConVar( const char *pName, const char *pDefaultValue, int flags = 0);

								ConVar( const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString );
								ConVar( const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax );
								ConVar( const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString, FnChangeCallback_t callback );
								ConVar( const char *pName, const char *pDefaultValue, int flags, 
									const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax,
									FnChangeCallback_t callback );

	virtual						~ConVar( void );

	virtual bool				IsFlagSet( int flag ) const;
	virtual const char*			GetHelpText( void ) const;
	virtual bool				IsRegistered( void ) const;
	virtual const char			*GetName( void ) const;
	// Return name of command (usually == GetName(), except in case of FCVAR_SS_ADDED vars
	virtual const char			*GetBaseName( void ) const;
	virtual int					GetSplitScreenPlayerSlot() const;

	virtual void				AddFlags( int flags );
	virtual int					GetFlags() const;
	virtual	bool				IsCommand( void ) const;

	// Install a change callback (there shouldn't already be one....)
	void InstallChangeCallback( FnChangeCallback_t callback, bool bInvoke = true );
	void RemoveChangeCallback( FnChangeCallback_t callbackToRemove );

	int GetChangeCallbackCount() const { return m_pParent->m_fnChangeCallbacks.Count(); }
	FnChangeCallback_t GetChangeCallback( int slot ) const { return m_pParent->m_fnChangeCallbacks[ slot ]; }

	// Retrieve value
	FORCEINLINE_CVAR float			GetFloat( void ) const;
	FORCEINLINE_CVAR int			GetInt( void ) const;
	FORCEINLINE_CVAR Color			GetColor( void ) const;
	FORCEINLINE_CVAR bool			GetBool() const {  return !!GetInt(); }
	FORCEINLINE_CVAR char const	   *GetString( void ) const;

	// Compiler driven selection for template use
	template <typename T> T Get( void ) const;
	template <typename T> T Get( T * ) const;

	// Any function that allocates/frees memory needs to be virtual or else you'll have crashes
	//  from alloc/free across dll/exe boundaries.
	
	// These just call into the IConCommandBaseAccessor to check flags and set the var (which ends up calling InternalSetValue).
	virtual void				SetValue( const char *value );
	virtual void				SetValue( float value );
	virtual void				SetValue( int value );
	virtual void				SetValue( Color value );
	
	// Reset to default value
	void						Revert( void );

	// True if it has a min/max setting
	bool						HasMin() const;
	bool						HasMax() const;

	bool						GetMin( float& minVal ) const;
	bool						GetMax( float& maxVal ) const;

	float						GetMinValue() const;
	float						GetMaxValue() const;

	const char					*GetDefault( void ) const;
	void						SetDefault( const char *pszDefault );

	// Value
	struct CVValue_t
	{
		char						*m_pszString;
		int							m_StringLength;

		// Values
		float						m_fValue;
		int							m_nValue;
	};

	FORCEINLINE_CVAR CVValue_t &GetRawValue()
	{
		return m_Value;
	}
	FORCEINLINE_CVAR const CVValue_t &GetRawValue() const
	{
		return m_Value;
	}

private:
	bool						InternalSetColorFromString( const char *value );
	// Called by CCvar when the value of a var is changing.
	virtual void				InternalSetValue(const char *value);
	// For CVARs marked FCVAR_NEVER_AS_STRING
	virtual void				InternalSetFloatValue( float fNewValue );
	virtual void				InternalSetIntValue( int nValue );
	virtual void				InternalSetColorValue( Color value );

	virtual bool				ClampValue( float& value );
	virtual void				ChangeStringValue( const char *tempVal, float flOldValue );

	virtual void				Create( const char *pName, const char *pDefaultValue, int flags = 0,
									const char *pHelpString = 0, bool bMin = false, float fMin = 0.0,
									bool bMax = false, float fMax = false, FnChangeCallback_t callback = 0 );

	// Used internally by OneTimeInit to initialize.
	virtual void				Init();

protected:

	// This either points to "this" or it points to the original declaration of a ConVar.
	// This allows ConVars to exist in separate modules, and they all use the first one to be declared.
	// m_pParent->m_pParent must equal m_pParent (ie: m_pParent must be the root, or original, ConVar).
	ConVar						*m_pParent;

	// Static data
	const char					*m_pszDefaultValue;
	
	CVValue_t					m_Value;

	// Min/Max values
	bool						m_bHasMin;
	float						m_fMinVal;
	bool						m_bHasMax;
	float						m_fMaxVal;
	
	// Call this function when ConVar changes
	CUtlVector< FnChangeCallback_t > m_fnChangeCallbacks;
};


//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float
// Output : float
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR float ConVar::GetFloat( void ) const
{
#ifdef CONVAR_TEST_MATERIAL_THREAD_CONVARS
	Assert( ThreadInMainThread() || IsFlagSet( FCVAR_MATERIAL_THREAD_MASK | FCVAR_ACCESSIBLE_FROM_THREADS ) );
#endif
	return m_pParent->m_Value.m_fValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
// Output : int
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR int ConVar::GetInt( void ) const 
{
#ifdef CONVAR_TEST_MATERIAL_THREAD_CONVARS
	Assert( ThreadInMainThread() || IsFlagSet( FCVAR_MATERIAL_THREAD_MASK | FCVAR_ACCESSIBLE_FROM_THREADS ) );
#endif
	return m_pParent->m_Value.m_nValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a color
// Output : Color
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR Color ConVar::GetColor( void ) const 
{
#ifdef CONVAR_TEST_MATERIAL_THREAD_CONVARS
	Assert( ThreadInMainThread() || IsFlagSet( FCVAR_MATERIAL_THREAD_MASK | FCVAR_ACCESSIBLE_FROM_THREADS ) );
#endif
	unsigned char *pColorElement = ((unsigned char *)&m_pParent->m_Value.m_nValue);
	return Color( pColorElement[0], pColorElement[1], pColorElement[2], pColorElement[3] );
}


//-----------------------------------------------------------------------------

template <> FORCEINLINE_CVAR float			ConVar::Get<float>( void ) const		{ return GetFloat(); }
template <> FORCEINLINE_CVAR int			ConVar::Get<int>( void ) const			{ return GetInt(); }
template <> FORCEINLINE_CVAR bool			ConVar::Get<bool>( void ) const			{ return GetBool(); }
template <> FORCEINLINE_CVAR const char *	ConVar::Get<const char *>( void ) const	{ return GetString(); }
template <> FORCEINLINE_CVAR float			ConVar::Get<float>( float *p ) const				{ return ( *p = GetFloat() ); }
template <> FORCEINLINE_CVAR int			ConVar::Get<int>( int *p ) const					{ return ( *p = GetInt() ); }
template <> FORCEINLINE_CVAR bool			ConVar::Get<bool>( bool *p ) const					{ return ( *p = GetBool() ); }
template <> FORCEINLINE_CVAR const char *	ConVar::Get<const char *>( char const **p ) const	{ return ( *p = GetString() ); }

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string, return "" for bogus string pointer, etc.
// Output : const char *
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR const char *ConVar::GetString( void ) const 
{
#ifdef CONVAR_TEST_MATERIAL_THREAD_CONVARS
	Assert( ThreadInMainThread() || IsFlagSet( FCVAR_MATERIAL_THREAD_MASK | FCVAR_ACCESSIBLE_FROM_THREADS ) );
#endif
	if ( m_nFlags & FCVAR_NEVER_AS_STRING )
		return "FCVAR_NEVER_AS_STRING";
	
	char const *str = m_pParent->m_Value.m_pszString;
	return str ? str : "";
}

class CSplitScreenAddedConVar : public ConVar
{
	typedef ConVar BaseClass;
public:
	CSplitScreenAddedConVar( int nSplitScreenSlot, const char *pName, const ConVar *pBaseVar ) :
		BaseClass
		( 
			pName, 
			pBaseVar->GetDefault(), 
			// Keep basevar flags, except remove _SS and add _SS_ADDED instead
			( pBaseVar->GetFlags() & ~FCVAR_SS ) | FCVAR_SS_ADDED, 
			pBaseVar->GetHelpText(), 
			pBaseVar->HasMin(),
			pBaseVar->GetMinValue(),
			pBaseVar->HasMax(),
			pBaseVar->GetMaxValue()
		),
		m_pBaseVar( pBaseVar ),
		m_nSplitScreenSlot( nSplitScreenSlot )
	{
		for ( int i = 0; i < pBaseVar->GetChangeCallbackCount(); ++i )
		{
			InstallChangeCallback( pBaseVar->GetChangeCallback( i ), false );
		}
		Assert( nSplitScreenSlot >= 1 );
		Assert( nSplitScreenSlot < MAX_SPLITSCREEN_CLIENTS );
		Assert( m_pBaseVar );
		Assert( IsFlagSet( FCVAR_SS_ADDED ) );
		Assert( !IsFlagSet( FCVAR_SS ) );
	}

	const ConVar *GetBaseVar() const;
	virtual const char *GetBaseName() const;
	void SetSplitScreenPlayerSlot( int nSlot );
	virtual int GetSplitScreenPlayerSlot() const;

protected:

	const ConVar	*m_pBaseVar;
	int		m_nSplitScreenSlot;
};

FORCEINLINE_CVAR const ConVar *CSplitScreenAddedConVar::GetBaseVar() const 
{ 
	Assert( m_pBaseVar );
	return m_pBaseVar; 
}

FORCEINLINE_CVAR const char *CSplitScreenAddedConVar::GetBaseName() const 
{ 
	Assert( m_pBaseVar );
	return m_pBaseVar->GetName(); 
}

FORCEINLINE_CVAR void CSplitScreenAddedConVar::SetSplitScreenPlayerSlot( int nSlot ) 
{ 
	m_nSplitScreenSlot = nSlot; 
}

FORCEINLINE_CVAR int CSplitScreenAddedConVar::GetSplitScreenPlayerSlot() const 
{ 
	return m_nSplitScreenSlot; 
}

//-----------------------------------------------------------------------------
// Used to read/write convars that already exist (replaces the FindVar method)
//-----------------------------------------------------------------------------
class ConVarRef
{
public:
	ConVarRef( const char *pName );
	ConVarRef( const char *pName, bool bIgnoreMissing );
	ConVarRef( IConVar *pConVar );

	void Init( const char *pName, bool bIgnoreMissing );
	bool IsValid() const;
	bool IsFlagSet( int nFlags ) const;
	IConVar *GetLinkedConVar();

	// Get/Set value
	float GetFloat( void ) const;
	int GetInt( void ) const;
	Color GetColor( void ) const;
	bool GetBool() const { return !!GetInt(); }
	const char *GetString( void ) const;

	void SetValue( const char *pValue );
	void SetValue( float flValue );
	void SetValue( int nValue );
	void SetValue( Color value );
	void SetValue( bool bValue );

	const char *GetName() const;

	const char *GetDefault() const;

	const char *GetBaseName() const;

	int	GetSplitScreenPlayerSlot() const;

private:
	// High-speed method to read convar data
	IConVar *m_pConVar;
	ConVar *m_pConVarState;
};


//-----------------------------------------------------------------------------
// Did we find an existing convar of that name?
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR bool ConVarRef::IsFlagSet( int nFlags ) const
{
	return ( m_pConVar->IsFlagSet( nFlags ) != 0 );
}

FORCEINLINE_CVAR IConVar *ConVarRef::GetLinkedConVar()
{
	return m_pConVar;
}

FORCEINLINE_CVAR const char *ConVarRef::GetName() const
{
	return m_pConVar->GetName();
}

FORCEINLINE_CVAR const char *ConVarRef::GetBaseName() const
{
	return m_pConVar->GetBaseName();
}

FORCEINLINE_CVAR int ConVarRef::GetSplitScreenPlayerSlot() const
{
	return m_pConVar->GetSplitScreenPlayerSlot();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR float ConVarRef::GetFloat( void ) const
{
	return m_pConVarState->m_Value.m_fValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR int ConVarRef::GetInt( void ) const 
{
	return m_pConVarState->m_Value.m_nValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a color
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR Color ConVarRef::GetColor( void ) const 
{
	return m_pConVarState->GetColor();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string, return "" for bogus string pointer, etc.
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR const char *ConVarRef::GetString( void ) const 
{
	Assert( !IsFlagSet( FCVAR_NEVER_AS_STRING ) );
	return m_pConVarState->m_Value.m_pszString;
}


FORCEINLINE_CVAR void ConVarRef::SetValue( const char *pValue )
{
	m_pConVar->SetValue( pValue );
}

FORCEINLINE_CVAR void ConVarRef::SetValue( float flValue )
{
	m_pConVar->SetValue( flValue );
}

FORCEINLINE_CVAR void ConVarRef::SetValue( int nValue )
{
	m_pConVar->SetValue( nValue );
}

FORCEINLINE_CVAR void ConVarRef::SetValue( Color value )
{
	m_pConVar->SetValue( value );
}

FORCEINLINE_CVAR void ConVarRef::SetValue( bool bValue )
{
	m_pConVar->SetValue( bValue ? 1 : 0 );
}

FORCEINLINE_CVAR const char *ConVarRef::GetDefault() const
{
	return m_pConVarState->m_pszDefaultValue;
}

//-----------------------------------------------------------------------------
// Helper for referencing splitscreen convars (i.e., "name" and "name2")
//-----------------------------------------------------------------------------
class SplitScreenConVarRef
{
public:
	SplitScreenConVarRef( const char *pName );
	SplitScreenConVarRef( const char *pName, bool bIgnoreMissing );
	SplitScreenConVarRef( IConVar *pConVar );

	void Init( const char *pName, bool bIgnoreMissing );
	bool IsValid() const;
	bool IsFlagSet( int nFlags ) const;

	// Get/Set value
	float GetFloat( int nSlot ) const;
	int GetInt( int nSlot ) const;
	Color GetColor( int nSlot ) const;
	bool GetBool( int nSlot ) const { return !!GetInt( nSlot ); }
	const char *GetString( int nSlot  ) const;

	void SetValue( int nSlot, const char *pValue );
	void SetValue( int nSlot, float flValue );
	void SetValue( int nSlot, int nValue );
	void SetValue( int nSlot, Color value );
	void SetValue( int nSlot, bool bValue );

	const char *GetName( int nSlot ) const;

	const char *GetDefault() const;

	const char *GetBaseName() const;

private:
	struct cv_t
	{
		IConVar *m_pConVar;
		ConVar *m_pConVarState;
	};

	cv_t	m_Info[ MAX_SPLITSCREEN_CLIENTS ];
};

//-----------------------------------------------------------------------------
// Did we find an existing convar of that name?
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR bool SplitScreenConVarRef::IsFlagSet( int nFlags ) const
{
	return ( m_Info[ 0 ].m_pConVar->IsFlagSet( nFlags ) != 0 );
}

FORCEINLINE_CVAR const char *SplitScreenConVarRef::GetName( int nSlot ) const
{
	return m_Info[ nSlot ].m_pConVar->GetName();
}

FORCEINLINE_CVAR const char *SplitScreenConVarRef::GetBaseName() const
{
	return m_Info[ 0 ].m_pConVar->GetBaseName();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR float SplitScreenConVarRef::GetFloat( int nSlot ) const
{
	return m_Info[ nSlot ].m_pConVarState->m_Value.m_fValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR int SplitScreenConVarRef::GetInt( int nSlot ) const 
{
	return m_Info[ nSlot ].m_pConVarState->m_Value.m_nValue;
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR Color SplitScreenConVarRef::GetColor( int nSlot ) const 
{
	return m_Info[ nSlot ].m_pConVarState->GetColor();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string, return "" for bogus string pointer, etc.
//-----------------------------------------------------------------------------
FORCEINLINE_CVAR const char *SplitScreenConVarRef::GetString( int nSlot ) const 
{
	Assert( !IsFlagSet( FCVAR_NEVER_AS_STRING ) );
	return m_Info[ nSlot ].m_pConVarState->m_Value.m_pszString;
}


FORCEINLINE_CVAR void SplitScreenConVarRef::SetValue( int nSlot, const char *pValue )
{
	m_Info[ nSlot ].m_pConVar->SetValue( pValue );
}

FORCEINLINE_CVAR void SplitScreenConVarRef::SetValue( int nSlot, float flValue )
{
	m_Info[ nSlot ].m_pConVar->SetValue( flValue );
}

FORCEINLINE_CVAR void SplitScreenConVarRef::SetValue( int nSlot, int nValue )
{
	m_Info[ nSlot ].m_pConVar->SetValue( nValue );
}

FORCEINLINE_CVAR void SplitScreenConVarRef::SetValue( int nSlot, Color value )
{
	m_Info[ nSlot ].m_pConVar->SetValue( value );
}

FORCEINLINE_CVAR void SplitScreenConVarRef::SetValue( int nSlot, bool bValue )
{
	m_Info[ nSlot ].m_pConVar->SetValue( bValue ? 1 : 0 );
}

FORCEINLINE_CVAR const char *SplitScreenConVarRef::GetDefault() const
{
	return m_Info[ 0 ].m_pConVarState->m_pszDefaultValue;
}

//-----------------------------------------------------------------------------
// Called by the framework to register ConCommands with the ICVar
//-----------------------------------------------------------------------------
void ConVar_Register( int nCVarFlag = 0, IConCommandBaseAccessor *pAccessor = NULL );
void ConVar_Unregister( );


//-----------------------------------------------------------------------------
// Utility methods 
//-----------------------------------------------------------------------------
void ConVar_PrintDescription( const ConCommandBase *pVar );


//-----------------------------------------------------------------------------
// Purpose: Utility class to quickly allow ConCommands to call member methods
//-----------------------------------------------------------------------------
#pragma warning (disable : 4355 )

template< class T >
class CConCommandMemberAccessor : public ConCommand, public ICommandCallback, public ICommandCompletionCallback
{
	typedef ConCommand BaseClass;
	typedef void ( T::*FnMemberCommandCallback_t )( const CCommand &command );
	typedef int  ( T::*FnMemberCommandCompletionCallback_t )( const char *pPartial, CUtlVector< CUtlString > &commands );

public:
	CConCommandMemberAccessor( T* pOwner, const char *pName, FnMemberCommandCallback_t callback, const char *pHelpString = 0,
		int flags = 0, FnMemberCommandCompletionCallback_t completionFunc = 0 ) :
		BaseClass( pName, this, pHelpString, flags, ( completionFunc != 0 ) ? this : NULL )
	{
		m_pOwner = pOwner;
		m_Func = callback;
		m_CompletionFunc = completionFunc;
	}

	~CConCommandMemberAccessor()
	{
		Shutdown();
	}

	void SetOwner( T* pOwner )
	{
		m_pOwner = pOwner;
	}

	virtual void CommandCallback( const CCommand &command )
	{
		Assert( m_pOwner && m_Func );
		(m_pOwner->*m_Func)( command );
	}

	virtual int  CommandCompletionCallback( const char *pPartial, CUtlVector< CUtlString > &commands )
	{
		Assert( m_pOwner && m_CompletionFunc );
		return (m_pOwner->*m_CompletionFunc)( pPartial, commands );
	}

private:
	T* m_pOwner;
	FnMemberCommandCallback_t m_Func;
	FnMemberCommandCompletionCallback_t m_CompletionFunc;
};

#pragma warning ( default : 4355 )


//-----------------------------------------------------------------------------
// Purpose: Utility macros to quicky generate a simple console command
//-----------------------------------------------------------------------------
#define CON_COMMAND( name, description ) \
   static void name( const CCommand &args ); \
   static ConCommand name##_command( #name, name, description ); \
   static void name( const CCommand &args )

#define CON_COMMAND_F( name, description, flags ) \
   static void name( const CCommand &args ); \
   static ConCommand name##_command( #name, name, description, flags ); \
   static void name( const CCommand &args )

#define CON_COMMAND_F_COMPLETION( name, description, flags, completion ) \
	static void name( const CCommand &args ); \
	static ConCommand name##_command( #name, name, description, flags, completion ); \
	static void name( const CCommand &args )

#define CON_COMMAND_EXTERN( name, _funcname, description ) \
	void _funcname( const CCommand &args ); \
	static ConCommand name##_command( #name, _funcname, description ); \
	void _funcname( const CCommand &args )

#define CON_COMMAND_EXTERN_F( name, _funcname, description, flags ) \
	void _funcname( const CCommand &args ); \
	static ConCommand name##_command( #name, _funcname, description, flags ); \
	void _funcname( const CCommand &args )

#define CON_COMMAND_MEMBER_F( _thisclass, name, _funcname, description, flags ) \
	void _funcname( const CCommand &args );						\
	friend class CCommandMemberInitializer_##_funcname;			\
	class CCommandMemberInitializer_##_funcname					\
	{															\
	public:														\
		CCommandMemberInitializer_##_funcname() : m_ConCommandAccessor( NULL, name, &_thisclass::_funcname, description, flags )	\
		{														\
			m_ConCommandAccessor.SetOwner( GET_OUTER( _thisclass, m_##_funcname##_register ) );	\
		}														\
	private:													\
		CConCommandMemberAccessor< _thisclass > m_ConCommandAccessor;	\
	};															\
																\
	CCommandMemberInitializer_##_funcname m_##_funcname##_register;		\


#endif // CONVAR_H
