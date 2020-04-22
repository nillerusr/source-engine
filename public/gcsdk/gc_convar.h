//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines gc-specific convars that integrate their AppIDs so that
//			two GC's in the same shell don' overwrite each other
//
//=============================================================================

#ifndef GCCONVAR_H
#define GCCONVAR_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/convar.h"


//-----------------------------------------------------------------------------
// Purpose: GC specifc ConVar
//-----------------------------------------------------------------------------
class GCConVar : public ConVar
{
public:
	GCConVar( const char *pName, const char *pDefaultValue, const char *pHelpString, int flags = 0 )
		: ConVar( pName, pDefaultValue, flags, pHelpString ) {}
	GCConVar( const char *pName, const char *pDefaultValue, int flags = 0)
		: ConVar( pName, pDefaultValue, flags) {}
	GCConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString )
		: ConVar( pName, pDefaultValue, flags, pHelpString ) {}
	GCConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax )
		: ConVar( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax ) {}
	GCConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, FnChangeCallback_t callback )
		: ConVar( pName, pDefaultValue, flags, pHelpString, callback ) {}
	GCConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax, FnChangeCallback_t callback )
		: ConVar( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax, callback ) {}

	virtual const char *GetName( void ) const;
	const char *GetBaseName() const { GetName(); return m_pchBaseName; } // returns the name without the appID suffix

protected:
	mutable CUtlString m_strGCName;
	mutable const char *m_pchBaseName;
};


//-----------------------------------------------------------------------------
// Purpose: GC specific ConCommand
//-----------------------------------------------------------------------------
class GCConCommand : public ConCommand
{
public:
	GCConCommand( const char *pName, FnCommandCallbackVoid_t callback, const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 )
		: ConCommand( pName, callback, pHelpString, flags, completionFunc ) {}
	GCConCommand( const char *pName, FnCommandCallback_t callback, const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 )
		: ConCommand( pName, callback, pHelpString, flags, completionFunc ) {}
	GCConCommand( const char *pName, ICommandCallback *pCallback, const char *pHelpString = 0, int flags = 0, ICommandCompletionCallback *pCommandCompletionCallback = 0 )
		: ConCommand( pName, pCallback, pHelpString, flags, pCommandCompletionCallback ) {}

	virtual const char *GetName( void ) const;
	const char *GetBaseName() const { GetName(); return m_pchBaseName; } // returns the name without the appID suffix

protected:
	mutable CUtlString m_strGCName;
	mutable const char *m_pchBaseName;
};

//utility function to help identify the expected number of arguments and returns false if an inappropriate number are provided and lists the help for the con command
bool BCheckArgs( int nArgs, const CCommand &args, const ConCommandBase &command );

//utility function that will determine if the appropriate GC type is running, and if not, will print out a descriptive message about which GC type it should be run on
bool BGCConCommandVerifyGCType( uint32 nGCType );

//called to declare a console command
// !FIXME DOTAMERGE
// CCommandContext
//#define GC_CON_COMMAND( name, description ) \
//	static void con_command_##name( const CCommandContext &ctx, const CCommand &args ); \
//	static GCConCommand name##_command( #name, con_command_##name, description ); \
//	static void con_command_##name( const CCommandContext &ctx, const CCommand &args )
#define GC_CON_COMMAND( name, description ) \
	static void con_command_##name( const CCommand &args ); \
	static GCConCommand name##_command( #name, con_command_##name, description ); \
	static void con_command_##name( const CCommand &args )

//declares a console command that requires at least the specified number of arguments, and if not, will print out the help and not execute. For example, if you require a single
//parameter, pass in 1 for numparams. This won't block if there are additional parameters
// !FIXME DOTAMERGE
// CCommandContext
//#define GC_CON_COMMAND_PARAMS( name, numparams, description ) \
//	static void con_command_##name##_ValidateParams( const CCommandContext &ctx, const CCommand &args ); \
//	static GCConCommand name##_command( #name, con_command_##name##_ValidateParams, description ); \
//	static void con_command_##name( const CCommandContext &ctx, const CCommand &args ); \
//	static void con_command_##name##_ValidateParams( const CCommandContext &ctx, const CCommand &args ) \
//	{ \
//		if( BCheckArgs( numparams, args, name##_command ) ) { con_command_##name( ctx, args ); } \
//	} \
//	static void con_command_##name( const CCommandContext &ctx, const CCommand &args )
#define GC_CON_COMMAND_PARAMS( name, numparams, description ) \
	static void con_command_##name##_ValidateParams( const CCommand &args ); \
	static GCConCommand name##_command( #name, con_command_##name##_ValidateParams, description ); \
	static void con_command_##name( const CCommand &args ); \
	static void con_command_##name##_ValidateParams( const CCommand &args ) \
	{ \
		if( BCheckArgs( numparams, args, name##_command ) ) { con_command_##name( args ); } \
	} \
	static void con_command_##name( const CCommand &args )

// Also see GC_CON_COMMAND_PARAMS_WEBAPI_ENABLED in gcwebapi.h

#define RESTRICT_GC_TYPE_CON_COMMAND( gctype )					\
{																\
	if ( !BGCConCommandVerifyGCType( gctype ) )					\
		return;													\
}																			//

#define AUTO_CONFIRM_CON_COMMAND()						\
{														\
	static RTime32 rtimeLastRan = 0;					\
	if ( CRTime::RTime32TimeCur() - rtimeLastRan > 3 )	\
	{													\
		rtimeLastRan = CRTime::RTime32TimeCur();		\
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Auto-confirm: Please repeat command within 3 seconds to confirm.\n" ); \
		return;											\
	}													\
	else												\
	{													\
		rtimeLastRan = 0;								\
	}													\
}														

#endif
