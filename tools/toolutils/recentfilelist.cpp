//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "toolutils/recentfilelist.h"
#include "vgui_controls/menu.h"
#include "iregistry.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



//-----------------------------------------------------------------------------
// Adds a file to the list of recent files
//-----------------------------------------------------------------------------
void CRecentFileList::Add( const char *pFileName, const char *pFileFormat )
{
	RecentFileInfo_t info;
	info.m_pFileName = pFileName;
	int idx = m_RecentFiles.Find( info );
	if ( idx != m_RecentFiles.InvalidIndex() )
	{
		// Remove from current slot so it gets added to head (most recent) below...
		m_RecentFiles.Remove( idx );
	}

	while ( m_RecentFiles.Count() >= MAX_RECENT_FILES )
	{
		// Oldest is at last slot
		m_RecentFiles.Remove( m_RecentFiles.Count() - 1 );
	}

	int i = m_RecentFiles.AddToHead( );
	m_RecentFiles[i].m_pFileName = pFileName;
	m_RecentFiles[i].m_pFileFormat = pFileFormat;
}


//-----------------------------------------------------------------------------
// Removes all files from the list
//-----------------------------------------------------------------------------
void CRecentFileList::Clear()
{
	m_RecentFiles.RemoveAll();
}


//-----------------------------------------------------------------------------
// Returns true if there's no files in the file list
//-----------------------------------------------------------------------------
bool CRecentFileList::IsEmpty() const
{
	return m_RecentFiles.Count() == 0;
}

	
//-----------------------------------------------------------------------------
// Gets the file in a particular slot
//-----------------------------------------------------------------------------
const char *CRecentFileList::GetFile( int slot ) const
{
	if ( slot < 0 || slot >= m_RecentFiles.Count() )
		return NULL;

	return m_RecentFiles[slot].m_pFileName;
}


//-----------------------------------------------------------------------------
// Gets the file in a particular slot
//-----------------------------------------------------------------------------
const char *CRecentFileList::GetFileFormat( int slot ) const
{
	if ( slot < 0 || slot >= m_RecentFiles.Count() )
		return NULL;

	return m_RecentFiles[slot].m_pFileFormat;
}


//-----------------------------------------------------------------------------
// Loads the file list from the registry 
//-----------------------------------------------------------------------------
void CRecentFileList::LoadFromRegistry( const char *pToolKeyName )
{
	Clear();

	// Iterate in reverse order so most recent files goes to top
	for ( int i = MAX_RECENT_FILES; i >= 0; --i )
	{
		char sz[ 128 ];
		char szType[ 128 ];
		Q_snprintf( sz, sizeof( sz ), "%s\\history%02i", pToolKeyName, i ); 
		Q_snprintf( szType, sizeof( szType ), "%s\\history_fileformat%02i", pToolKeyName, i ); 
		
		// NOTE: Can't call registry->ReadString twice in a row!
		char pFileName[MAX_PATH];
		Q_strncpy( pFileName, registry->ReadString( sz, "" ), sizeof(pFileName) );
		if ( pFileName && pFileName[ 0 ] )
		{
			const char *valType = registry->ReadString( szType, "" );
			const char *pFormat = (valType && valType[0]) ? valType : "dmx";
			Add( pFileName, pFormat );
		}
	}
}


//-----------------------------------------------------------------------------
// Saves file list into the registry 
//-----------------------------------------------------------------------------
void CRecentFileList::SaveToRegistry( const char *pToolKeyName ) const
{
	char sz[ 128 ];

	int i, c;
	c = m_RecentFiles.Count();
	for ( i = 0 ; i < c; ++i )
	{
		Q_snprintf( sz, sizeof( sz ), "%s\\history%02i", pToolKeyName, i ); 
		registry->WriteString( sz, m_RecentFiles[i].m_pFileName );

		Q_snprintf( sz, sizeof( sz ), "%s\\history_fileformat%02i", pToolKeyName, i ); 
		registry->WriteString( sz, m_RecentFiles[i].m_pFileFormat );
	}

	// Clear out all other registry settings
	for ( ; i < MAX_RECENT_FILES; ++i )
	{
		Q_snprintf( sz, sizeof( sz ), "%s\\history%02i", pToolKeyName, i ); 
		registry->WriteString( sz, "" );

		Q_snprintf( sz, sizeof( sz ), "%s\\history_fileformat%02i", pToolKeyName, i ); 
		registry->WriteString( sz, "" );
	}
}


//-----------------------------------------------------------------------------
// Adds the list of files to a particular menu 
//-----------------------------------------------------------------------------
void CRecentFileList::AddToMenu( vgui::Menu *menu, vgui::Panel *pActionTarget, const char *pCommandName ) const
{
	int i, c;
	c = m_RecentFiles.Count();
	for ( i = 0 ; i < c; ++i )
	{
		char sz[ 32 ];
		Q_snprintf( sz, sizeof( sz ), "%s%02i", pCommandName, i );
		char const *fn = m_RecentFiles[i].m_pFileName;
		menu->AddMenuItem( fn, new KeyValues( "Command", "command", sz ), pActionTarget );
	}
}
