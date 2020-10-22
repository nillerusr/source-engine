//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "p4helpers.h"
#include "tier2/tier2.h"
#include "p4lib/ip4.h"

#ifdef PLATFORM_WINDOWS_PC
#include <Windows.h>
#endif // PLATFORM_WINDOWS_PC

//////////////////////////////////////////////////////////////////////////
//
// CP4File implementation
//
//////////////////////////////////////////////////////////////////////////

CP4File::CP4File( char const *szFilename )
{
#ifdef PLATFORM_WINDOWS_PC

	// On windows, get the pathname of the file on disk first before using that as a perforce path
	// this avoids invalid Adds().  Have to go through GetShortPathName and then GetLongPathName from
	// the short path name

	TCHAR szShortPathName[ MAX_PATH ] = TEXT( "" );
	const DWORD shortRetVal = GetShortPathName( szFilename, szShortPathName, ARRAYSIZE( szShortPathName ) );

	if ( shortRetVal > 0 && shortRetVal <= ARRAYSIZE( szShortPathName ) )
	{
		TCHAR szLongPathName[ MAX_PATH ] = TEXT( "" );

		const DWORD longRetVal = GetLongPathName( szShortPathName, szLongPathName, ARRAYSIZE( szLongPathName ) );

		if ( longRetVal > 0 && longRetVal <= ARRAYSIZE( szLongPathName ) )
		{
			m_sFilename = szLongPathName;
			return;
		}
	}

#endif // PLATFORM_WINDOWS_PC

	m_sFilename = szFilename;
}

CP4File::~CP4File()
{
}

bool CP4File::Edit( void )
{
	if ( !p4 )
		return true;

	return p4->OpenFileForEdit( m_sFilename.String() );
}

bool CP4File::Add( void )
{
	if ( !p4 )
		return true;

	return p4->OpenFileForAdd( m_sFilename.String() );
}

bool CP4File::Revert( void )
{
	if ( !p4 )
		return true;

	return p4->RevertFile( m_sFilename.String() );
}

// Is the file in perforce?
bool CP4File::IsFileInPerforce()
{
	if ( !p4 )
		return false;

	return p4->IsFileInPerforce( m_sFilename.String() );
}

bool CP4File::SetFileType(const CUtlString& desiredFileType)
{
	/*if ( !p4 )
		return false;

	return p4->SetFileType( m_sFilename.String(), desiredFileType.String() );*/
	Error("CP4File::SetFileType is unimplemented");
	return false;
}


//////////////////////////////////////////////////////////////////////////
//
// CP4Factory implementation
//
//////////////////////////////////////////////////////////////////////////


CP4Factory::CP4Factory()
{
}

CP4Factory::~CP4Factory()
{
}

bool CP4Factory::SetDummyMode( bool bDummyMode )
{
	bool bOld = m_bDummyMode;
	m_bDummyMode = bDummyMode;
	return bOld;
}

void CP4Factory::SetOpenFileChangeList( const char *szChangeListName )
{
	if ( !m_bDummyMode && p4 )
		p4->SetOpenFileChangeList( szChangeListName );
}

CP4File *CP4Factory::AccessFile( char const *szFilename ) const
{
	if ( !m_bDummyMode )
		return new CP4File( szFilename );
	else
		return new CP4File_Dummy( szFilename );
}


// Default p4 factory
static CP4Factory s_static_p4_factory;
CP4Factory *g_p4factory = &s_static_p4_factory; // NULL before the factory constructs

