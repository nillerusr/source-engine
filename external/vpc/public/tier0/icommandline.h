//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef TIER0_ICOMMANDLINE_H
#define TIER0_ICOMMANDLINE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"


//-----------------------------------------------------------------------------
// Purpose: Interface to engine command line
//-----------------------------------------------------------------------------
abstract_class ICommandLine
{
public:
	virtual void		CreateCmdLine( const char *commandline ) = 0;
	virtual void		CreateCmdLine( int argc, char **argv ) = 0;
	virtual const char	*GetCmdLine( void ) const = 0;

	// Check whether a particular parameter exists
	virtual	const char	*CheckParm( const char *psz, const char **ppszValue = 0 ) const = 0;
	virtual void		RemoveParm( const char *parm ) = 0;
	virtual void		AppendParm( const char *pszParm, const char *pszValues ) = 0;

	// Returns the argument after the one specified, or the default if not found
	virtual const char	*ParmValue( const char *psz, const char *pDefaultVal = 0 ) const = 0;
	virtual int			ParmValue( const char *psz, int nDefaultVal ) const = 0;
	virtual float		ParmValue( const char *psz, float flDefaultVal ) const = 0;

	// Gets at particular parameters
	virtual int			ParmCount() const = 0;
	virtual int			FindParm( const char *psz ) const = 0;	// Returns 0 if not found.
	virtual const char* GetParm( int nIndex ) const = 0;
	
	// copies the string passwed
	virtual void SetParm( int nIndex, char const *pNewParm ) =0;
};

//-----------------------------------------------------------------------------
// Gets a singleton to the commandline interface
// NOTE: The #define trickery here is necessary for backwards compat:
// this interface used to lie in the vstdlib library.
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE ICommandLine *CommandLine();


//-----------------------------------------------------------------------------
// Process related functions
//-----------------------------------------------------------------------------
PLATFORM_INTERFACE const tchar *Plat_GetCommandLine();
#ifndef _WIN32
// helper function for OS's that don't have a ::GetCommandLine() call
PLATFORM_INTERFACE void Plat_SetCommandLine( const char *cmdLine );
#endif
PLATFORM_INTERFACE const char *Plat_GetCommandLineA();


#endif // TIER0_ICOMMANDLINE_H

