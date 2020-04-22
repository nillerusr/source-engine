//========= Copyright Valve Corporation, All rights reserved. ============//
/**
 * Utility class for providing some basic capabilities for enumerating and specifying MOD
 * directories.
 *
 * \version 1.0
 *
 * \date 07-18-2006
 *
 * \author mdurand
 *
 * \todo 
 *
 * \bug 
 *
 */
#include "ModConfigsHelper.h"
#include <windows.h>

ModConfigsHelper::ModConfigsHelper()
{
	setSourceModBaseDir();
	EnumerateModDirs();
}


/**
* Default destructor. 
*/
ModConfigsHelper::~ModConfigsHelper()
{
	// Empty the vector of directory names
	m_ModDirs.PurgeAndDeleteElements();
}


/**
* Getter method that provides the parent directory for all MODs.
* \return parent directory for all MODs
*/
const char *ModConfigsHelper::getSourceModBaseDir()
{
	return m_sourceModBaseDir;
}

/**
* Getter method that provides a vector of the names of each MOD found.
* \return vector of the names of each MOD found
*/
const CUtlVector<char *> &ModConfigsHelper::getModDirsVector()
{
	return m_ModDirs;
}


/**
* Determines and sets the base directory for all MODs
*/
void ModConfigsHelper::setSourceModBaseDir()
{
	Q_strncpy( m_sourceModBaseDir, GetSDKLauncherBaseDirectory(), sizeof( m_sourceModBaseDir) );    // Start with the base directory
	Q_StripLastDir( m_sourceModBaseDir, sizeof( m_sourceModBaseDir ) );								// Get rid of the 'sourcesdk' directory.
	Q_StripLastDir( m_sourceModBaseDir, sizeof( m_sourceModBaseDir ) );								// Get rid of the '%USER%' directory.
	Q_strncat( m_sourceModBaseDir, "SourceMods", sizeof( m_sourceModBaseDir ), COPY_ALL_CHARACTERS ); // Add 'SourceMods'
}


/**
* Searches the parent MOD directory for child MODs and puts their names in the member vector
*/
void ModConfigsHelper::EnumerateModDirs()
{
	char szWildCardPath[MAX_PATH];
	WIN32_FIND_DATA wfd;

	Q_strncpy( szWildCardPath, m_sourceModBaseDir, sizeof( szWildCardPath ) );
	Q_AppendSlash( szWildCardPath, sizeof( szWildCardPath ) );
	Q_strncat( szWildCardPath, "*.*", sizeof( szWildCardPath ), COPY_ALL_CHARACTERS );

	HANDLE ff = FindFirstFile( szWildCardPath, &wfd );

	do
	{
		if ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			if ( wfd.cFileName[ 0 ] == '.' )
			{
				continue;
			}
			else
			{
				// They are directories not named '.' or '..' so add them to the list of mod directories
				char *dirName = new char[ strlen( wfd.cFileName ) + 1 ];
				Q_strncpy( dirName, wfd.cFileName, strlen( wfd.cFileName ) + 1 );
				m_ModDirs.AddToTail( dirName );
			}
		}
	} while ( FindNextFile( ff, &wfd ) );
}