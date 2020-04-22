//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef BASEAUTOCOMPLETEFILELIST_H
#define BASEAUTOCOMPLETEFILELIST_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/convar.h"

//-----------------------------------------------------------------------------
// Purpose: Simple helper class for doing autocompletion of all files in a specific directory by extension
//-----------------------------------------------------------------------------
class CBaseAutoCompleteFileList
{
public:
	CBaseAutoCompleteFileList( const char *cmdname, const char *subdir, const char *extension )
	{
		m_pszCommandName	= cmdname;
		m_pszSubDir			= subdir;
		m_pszExtension		= extension;
	}

	int AutoCompletionFunc( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );

private:
	const char	*m_pszCommandName;
	const char	*m_pszSubDir;
	const char	*m_pszExtension;
};

#define DECLARE_AUTOCOMPLETION_FUNCTION( command, subdirectory, extension )							\
	static int g_##command##_CompletionFunc( const char *partial,									\
			char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )		\
	{																								\
		static CBaseAutoCompleteFileList command##Complete( #command, subdirectory, #extension );	\
		return command##Complete.AutoCompletionFunc( partial, commands );							\
	}

#define AUTOCOMPLETION_FUNCTION( command )	\
	g_##command##_CompletionFunc

//-----------------------------------------------------------------------------
// Purpose: Utility to quicky generate a simple console command with file name autocompletion
//-----------------------------------------------------------------------------
#if !defined( _XBOX )
#define CON_COMMAND_AUTOCOMPLETEFILE( name, func, description, subdirectory, extension )				\
   DECLARE_AUTOCOMPLETION_FUNCTION( name, subdirectory, extension )										\
   static ConCommand name##_command( #name, func, description, 0, AUTOCOMPLETION_FUNCTION( name ) ); 

#else
	#define CON_COMMAND_AUTOCOMPLETEFILE( name, func, description, subdirectory, extension )				\
	static ConCommand name##_command( #name, func, description ); 

#endif
#endif // BASEAUTOCOMPLETEFILELIST_H
