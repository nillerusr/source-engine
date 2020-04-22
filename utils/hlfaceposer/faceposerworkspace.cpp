//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <windows.h>
#include <stdio.h>
#include "tier1/strtools.h"
#include "ifaceposerworkspace.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWorkspaceFiles : public IWorkspaceFiles
{
public:
					CWorkspaceFiles( void );
					~CWorkspaceFiles( void );

	virtual void	Init( char const *pchShortName );

	// Restore
	int				GetNumStoredFiles( int type );
	const char		*GetStoredFile( int type, int number );

	// Save
	void			StartStoringFiles( int type );
	void			FinishStoringFiles( int type );
	void			StoreFile( int type, const char *filename );

private:
	static const char *NameForType( int type );
	static int		TypeForName( const char *name );

	LONG			CreateWorkspaceKey( char const *pchGameName, PHKEY phKey );
	bool			ReadInt( const char *szSubKey, int *value );
	bool			WriteInt( const char *szSubKey, int value );
	bool			ReadString( const char *szSubKey, char *value, int bufferlen );
	bool			WriteString( const char *szSubKey, const char *value );

	HKEY			m_hKeyMain;

	int				m_nStoredFiles[ NUM_FILE_TYPES ];
};

static CWorkspaceFiles g_WorkspaceFiles;
IWorkspaceFiles *workspacefiles = ( IWorkspaceFiles * )&g_WorkspaceFiles;

CWorkspaceFiles::CWorkspaceFiles( void ) :
	m_hKeyMain( (HKEY)0 )
{
	memset( m_nStoredFiles, 0, sizeof( m_nStoredFiles ) );
}

CWorkspaceFiles::~CWorkspaceFiles( void )
{
	if ( (HKEY)0 != m_hKeyMain )
	{
		RegCloseKey( m_hKeyMain );
	}
}

void CWorkspaceFiles::Init( char const *pchShortName )
{
	CreateWorkspaceKey( pchShortName, &m_hKeyMain );
}

const char *CWorkspaceFiles::NameForType( int type )
{
	switch ( type )
	{
	case EXPRESSION:
		return "expressionfiles";
	case CHOREODATA:
		return "choreodatafiles";
	case MODELDATA:
		return "modelfiles";
	default:
		break;
	}

	return "unknown";
}

int CWorkspaceFiles::TypeForName( const char *name )
{
	if ( !Q_stricmp( name, "expressionfiles" ) )
	{
		return EXPRESSION;
	}
	else if ( !Q_stricmp( name, "choreodatafiles" ) )
	{
		return CHOREODATA;
	}
	else if ( !Q_stricmp( name, "modelfiles" ) )
	{
		return MODELDATA;
	}
	return -1;
}


int CWorkspaceFiles::GetNumStoredFiles( int type )
{
	char szKeyName[ 256 ];
	Q_snprintf( szKeyName, sizeof( szKeyName ), "%s\\total", NameForType( type ) );

	int num = 0;
	ReadInt( szKeyName, &num );
	return num;
}

const char *CWorkspaceFiles::GetStoredFile( int type, int number )
{
	char szKeyName[ 256 ];
	sprintf( szKeyName, "%s\\%04i", NameForType( type ), number );

	static char filename[ 256 ];
	filename[ 0 ] = 0;
	ReadString( szKeyName, filename, 256 );
	return filename;
}

void CWorkspaceFiles::StartStoringFiles( int type )
{
	m_nStoredFiles[ type ] = 0;
}

void CWorkspaceFiles::FinishStoringFiles( int type )
{
	char szKeyName[ 256 ];
	sprintf( szKeyName, "%s\\total", NameForType( type ) );

	WriteInt( szKeyName, m_nStoredFiles[ type ] );
}

void CWorkspaceFiles::StoreFile( int type, const char *filename )
{
	char szKeyName[ 256 ];
	sprintf( szKeyName, "%s\\%04i", NameForType( type ), m_nStoredFiles[ type ]++ );

	WriteString( szKeyName, filename );
}

LONG CWorkspaceFiles::CreateWorkspaceKey( char const *pchGameName, PHKEY phKey )
{
	DWORD disp;

	char sz[ 512 ];
	Q_snprintf( sz, sizeof( sz ), "Software\\Valve\\faceposer\\workspace\\%s", pchGameName );

	return RegCreateKeyEx(
		HKEY_CURRENT_USER,	// handle of open key 
		sz, //				address of name of subkey to open 
		0,					// DWORD ulOptions,	  // reserved 
		NULL,				// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		phKey,				// Key we are creating
		&disp );			// Type of creation
}

bool CWorkspaceFiles::ReadInt( const char *szSubKey, int *value )
{
	LONG lResult;           // Registry function result code
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	dwSize = sizeof( DWORD );

	lResult = RegQueryValueEx(
		m_hKeyMain,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)value,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	if (dwType != REG_DWORD)
		return false;

	return true;
}


bool CWorkspaceFiles::WriteInt( const char *szSubKey, int value )
{
	LONG lResult;           // Registry function result code
	DWORD dwSize;           // Size of element data

	dwSize = sizeof( DWORD );

	lResult = RegSetValueEx(
		m_hKeyMain,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		REG_DWORD,		// type buffer
		(LPBYTE)&value,    // data buffer
		dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	return true;
}

bool CWorkspaceFiles::ReadString( const char *szSubKey, char *value, int buffersize )
{
	LONG lResult;           // Registry function result code
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	dwSize = buffersize;

	lResult = RegQueryValueEx(
		m_hKeyMain,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		&dwType,    // type buffer
		(LPBYTE)value,    // data buffer
		&dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	if (dwType != REG_SZ)
		return false;

	return true;
}


bool CWorkspaceFiles::WriteString( const char *szSubKey, const char *value )
{
	LONG lResult;           // Registry function result code
	DWORD dwSize;           // Size of element data

	dwSize = strlen( value ) + 1;

	lResult = RegSetValueEx(
		m_hKeyMain,		// handle to key
		szSubKey,	// value name
		0,			// reserved
		REG_SZ,		// type buffer
		(LPBYTE)value,    // data buffer
		dwSize );  // size of data buffer

	if (lResult != ERROR_SUCCESS)  // Failure
		return false;

	return true;
}