//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#if !defined( CVAR_H )
#define CVAR_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class ConVar;
class ConCommandBase;
class CCommand;
class CUtlBuffer;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCvarUtilities
{
public:
	bool IsCommand( const CCommand &args );

	// Writes lines containing "set variable value" for all variables
	// with the archive flag set to true.
	void WriteVariables( CUtlBuffer &buff, bool bAllVars );

	// Returns the # of cvars with the server flag set.
	int	CountVariablesWithFlags( int flags );

	// Lists cvars to console
	void CvarList( const CCommand &args );

	// Prints help text for cvar
	void CvarHelp( const CCommand &args );

	// Revert all cvar values
	void CvarRevert( const CCommand &args );

	// Revert all cvar values
	void CvarDifferences( const CCommand &args );

	// Toggles a cvar on/off, or cycles through a set of values
	void CvarToggle( const CCommand &args );

	// Finds commands with a specified flag.
	void CvarFindFlags_f( const CCommand &args );

private:
	// just like Cvar_set, but optimizes out the search
	void SetDirect( ConVar *var, const char *value );

	bool IsValidToggleCommand( const char *cmd );
};

extern CCvarUtilities *cv;


#endif // CVAR_H
