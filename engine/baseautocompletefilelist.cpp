//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:	  
//
// $NoKeywords: $
//=============================================================================//

#include "tier0/platform.h"
#include "qlimits.h"
#include "sys.h"
#include "baseautocompletefilelist.h"
#include "utlsymbol.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Fills in a list of commands based on specified subdirectory and extension into the format:
//  commandname subdir/filename.ext
//  commandname subdir/filename2.ext
// Returns number of files in list for autocompletion
//-----------------------------------------------------------------------------
int CBaseAutoCompleteFileList::AutoCompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = m_pszCommandName;

	char *substring = (char *)partial;
	if ( Q_strstr( partial, cmdname ) )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
	}

	// Search the directory structure.
	char searchpath[MAX_QPATH];
	if ( m_pszSubDir && m_pszSubDir[0] && Q_strcasecmp( m_pszSubDir, "NULL" ) )
	{
		Q_snprintf(searchpath,sizeof(searchpath),"%s/*.%s", m_pszSubDir, m_pszExtension );
	}
	else
	{
		Q_snprintf(searchpath,sizeof(searchpath),"*.%s", m_pszExtension );
	}

	CUtlSymbolTable entries( 0, 0, true );
	CUtlVector< CUtlSymbol > symbols;

	char const *findfn = Sys_FindFirst( searchpath, NULL, 0 );
	while ( findfn )
	{
		char sz[ MAX_QPATH ];
		Q_snprintf( sz, sizeof( sz ), "%s", findfn );

		bool add = false;
		// Insert into lookup
		if ( substring[0] )
		{
			if ( !Q_strncasecmp( findfn, substring, strlen( substring ) ) )
			{
				add = true;
			}
		}
		else
		{
			add = true;
		}

		if ( add )
		{
			CUtlSymbol sym = entries.AddString( findfn );

			int idx = symbols.Find( sym );
			if ( idx == symbols.InvalidIndex() )
			{
				symbols.AddToTail( sym );
			}
		}

		findfn = Sys_FindNext( NULL, 0 );

		// Too many
		if ( symbols.Count() >= COMMAND_COMPLETION_MAXITEMS )
			break;
	}

	Sys_FindClose();

	for ( int i = 0; i < symbols.Count(); i++ )
	{
		char const *filename = entries.String( symbols[ i ] );

		Q_snprintf( commands[ i ], sizeof( commands[ i ] ), "%s %s", cmdname, filename );
		// Remove .dem
		commands[ i ][ strlen( commands[ i ] ) - 4 ] = 0;
	}

	return symbols.Count();
}
