//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Class that contains a list of recent files
//
//=============================================================================

#ifndef RECENTFILELIST_H
#define RECENTFILELIST_H

#ifdef _WIN32
#pragma once
#endif


#include "tier1/utlvector.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Menu;
	class Panel;
}


//-----------------------------------------------------------------------------
// Max # of recent files
//-----------------------------------------------------------------------------
#define MAX_RECENT_FILES	20


//-----------------------------------------------------------------------------
// Class that contains a list of recent files
//-----------------------------------------------------------------------------
class CRecentFileList
{
public:
	// Adds a file to the list
	void		Add( const char *pFileName, const char *pFileFormat );

	// Gets the file in a particular slot
	const char	*GetFile( int slot ) const;
	const char	*GetFileFormat( int slot ) const;

	// Removes all files from the list
	void		Clear();

	// Load, store to registry
	void		LoadFromRegistry( const char *pToolKeyName );
	void		SaveToRegistry( const char *pToolKeyName ) const;

	// Adds all files in a list to a menu.
	// Will generate the commands '<pCommandName>01', '<pCommandName>02', etc.
	// depending on which one is selected
	void		AddToMenu( vgui::Menu *menu, vgui::Panel *actionTarget, const char *pCommandName ) const;

	// Returns true if there's no files in the file list
	bool		IsEmpty() const;

private:
	struct RecentFileInfo_t
	{
		CUtlString m_pFileName;
		CUtlString m_pFileFormat;

		bool operator==( const RecentFileInfo_t& src ) const
		{
			return m_pFileName == src.m_pFileName;
		}
	};

	CUtlVector< RecentFileInfo_t >	m_RecentFiles;
};


#endif // RECENTFILELIST_H


