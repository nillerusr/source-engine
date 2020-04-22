//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


#include <Windows.h>	// GetModuleFileName, ShellExecute


// Valve includes
#include "strtools.h"
#include "tier0/icommandline.h"
#include "tier1/utlstring.h"


// Last include
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void RunExe()
{
	CUtlString sParameters;
	ICommandLine *pCmdLine = CommandLine();

	for ( int i = 1; i < pCmdLine->ParmCount(); ++i )
	{
		if ( i > 1 )
		{
			sParameters += " ";
		}

		const char *pszParm = pCmdLine->GetParm( i );
		if ( strchr( pszParm, ' ' ) || strchr( pszParm, '\t' ) )
		{
			sParameters += "\"";
			sParameters += pszParm;
			sParameters += "\"";
		}
		else
		{
			sParameters += pszParm;
		}
	}

	// Invoked with no command-line args: run in GUI mode.
	char szExeName[ MAX_PATH ];
	GetModuleFileName( NULL, szExeName, ARRAYSIZE( szExeName ) );
	V_SetExtension( szExeName, ".exe", ARRAYSIZE( szExeName ) );
	ShellExecute( NULL, _T( "open" ), szExeName, sParameters.String(), NULL, SW_SHOWNORMAL );
}
