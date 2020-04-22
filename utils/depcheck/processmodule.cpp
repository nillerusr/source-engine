//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <assert.h>
#include <time.h>
#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "depcheck_util.h"
#include "codeprocessor.h"

/*
================
UTIL_FloatTime
================
*/
double UTIL_FloatTime (void)
{
// more precise, less portable
	clock_t current;
	static clock_t base;
	static bool first = true;

	current = clock();

	if ( first )
	{
		first = false;

		base = current;
	}
	
	return (double)(current - base)/(double)CLOCKS_PER_SEC;
}

void CCodeProcessor::AddHeader( int depth, const char *filename, const char *rootmodule )
{
//	if ( depth < 1 )
//		return;
	if ( depth != 1 )
		return;

	// Check header list
	for ( int i = 0; i < m_nHeaderCount; i++ )
	{
		if ( !stricmp( m_Headers[ i ].name, filename ) )
		{
			vprint( 0, "%s included twice in module %s\n", filename, rootmodule );
			return;
		}
	}
	
	// Add to list
	strcpy( m_Headers[ m_nHeaderCount++ ].name, filename );
}

void CCodeProcessor::CreateBackup( const char *filename, bool& wasreadonly )
{
	assert( strstr( filename, ".cpp" ) );

	// attrib it, change extension, save it
	if ( GetFileAttributes( filename ) & FILE_ATTRIBUTE_READONLY )
	{
		wasreadonly = true;

		SetFileAttributes( filename, FILE_ATTRIBUTE_NORMAL );
	}
	else
	{
		wasreadonly = false;
	}

	char backupname[ 256 ];
	strcpy( backupname, filename );
	strcpy( (char *)&backupname[ strlen( filename ) - 4 ], ".bak" );

	unlink( backupname );
	rename( filename, backupname );

}

void CCodeProcessor::RestoreBackup( const char *filename, bool makereadonly )
{
	assert( strstr( filename, ".cpp" ) );

	char backupname[ 256 ];
	strcpy( backupname, filename );
	strcpy( (char *)&backupname[ strlen( filename ) - 4 ], ".bak" );

	SetFileAttributes( filename, FILE_ATTRIBUTE_NORMAL );

	unlink( filename );
	rename( backupname, filename );

	if ( makereadonly )
	{
		SetFileAttributes( filename, FILE_ATTRIBUTE_READONLY );
	}

	unlink( backupname );
}

bool CCodeProcessor::TryBuild( const char *rootdir, const char *filename, unsigned char *buffer, int filelength )
{
//	vprintf( "trying build\n" );

	FILE *fp;
	fp = fopen( filename, "wb" );
	if ( !fp )
	{
		assert( 0 );
		return false;
	}

	fwrite( buffer, filelength, 1, fp );
	fclose( fp );

	// if build is successful, return true
	//
	//	return true;
	char commandline[ 512 ];
	char directory[ 512 ];

	sprintf( directory, rootdir );

	//	sprintf( commandline, "msdev engdll.dsw /MAKE \"quiver - Win32 GL Debug\" /OUT log.txt" );

	// Builds the default configuration
	sprintf( commandline, "\"C:\\Program Files\\Microsoft Visual Studio\\Common\\MSDev98\\Bin\\msdev.exe\" %s /MAKE \"%s\" /OUT log.txt", m_szDSP, m_szConfig );

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );

	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );

	if ( !CreateProcess( NULL, commandline, NULL, NULL, TRUE, 0, NULL, directory, &si, &pi ) )
	{
LPVOID lpMsgBuf;
FormatMessage( 
    FORMAT_MESSAGE_ALLOCATE_BUFFER | 
    FORMAT_MESSAGE_FROM_SYSTEM | 
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    GetLastError(),
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    (LPTSTR) &lpMsgBuf,
    0,
    NULL 
);
// Process any inserts in lpMsgBuf.
// ...
// Display the string.
MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
// Free the buffer.
LocalFree( lpMsgBuf );
		return false;
	}

	// Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

	bool retval = false;
	DWORD exitCode = -1;
	if ( GetExitCodeProcess( pi.hProcess, &exitCode ) )
	{
		if ( !exitCode )
		{
			retval = true;
		}
	}
	
    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

	return retval;
}

void CCodeProcessor::ProcessModule( bool forcequiet, int depth, int& maxdepth, int& numheaders, int& skippedfiles, const char *baseroot, const char *root, const char *module )
{
	char filename[ 256 ];

	bool checkroot = false;

	if ( depth > maxdepth )
	{
		maxdepth = depth;
	}

	// Load the base module
	sprintf( filename, "%s\\%s", root, module );
	strlwr( filename );

	bool firstheader = true;
retry:

	// Check module list
	for ( int i = 0; i < m_nModuleCount; i++ )
	{
		if ( !stricmp( m_Modules[ i ].name, filename ) )
		{
			if ( forcequiet )
			{
				m_nHeadersProcessed++;
				numheaders++;

				if ( m_Modules[ i ].skipped )
				{
					skippedfiles++;
				}
			}

			AddHeader( depth, filename, m_szCurrentCPP );

			return;
		}
	}

	int filelength;
	char *buffer = (char *)COM_LoadFile( filename, &filelength );
	if ( !buffer )
	{
		if ( !checkroot )
		{
			checkroot = true;
			// Load the base module
			sprintf( filename, "%s\\%s", baseroot, module );
			goto retry;
		}
		m_Modules[ m_nModuleCount ].skipped = true;
		strcpy( m_Modules[ m_nModuleCount++ ].name, filename );
		
		skippedfiles++;
		return;
	}

	m_nBytesProcessed += filelength;

	m_Modules[ m_nModuleCount ].skipped = false;
	strcpy( m_Modules[ m_nModuleCount++ ].name, filename );

	bool readonly = false;
	bool madechanges = false;
	CreateBackup( filename, readonly );

	if ( !forcequiet )
	{
		strcpy( m_szCurrentCPP, filename );
		
		vprint( 0, "- %s\n", (char *)&filename[ m_nOffset ] );
	}

	// Parse tokens looking for #include directives or class starts
	char *current = buffer;
	char *startofline;

	current = CC_ParseToken( current );
	while ( current )
	{
		// No more tokens
		if ( strlen( com_token ) <= 0 )
			break;

		if ( !stricmp( com_token, "#include" ) )
		{
			startofline = current - strlen( "#include" );

			current = CC_ParseToken( current );

			if ( strlen( com_token ) > 0)
			{
				vprint( 1, "#include %s", com_token );
				m_nHeadersProcessed++;
				numheaders++;				

				AddHeader( depth, filename, m_szCurrentCPP );

				bool dobuild = true;
				if ( firstheader )
				{
					if ( !stricmp( com_token, "cbase.h" ) )
					{
						dobuild = false;
					}

					if ( !TryBuild( baseroot, filename, (unsigned char *)buffer, filelength ) )
					{
						// build is broken, stop
						assert( 0 );
					}
				}

				firstheader = false;

				if ( dobuild )
				{
					// Try removing the header and compiling
					char saveinfo[2];
					memcpy( saveinfo, startofline, 2 );
					startofline[ 0 ] = '/';
					startofline[ 1 ] = '/';

					if ( TryBuild( baseroot, filename, (unsigned char *)buffer, filelength ) )
					{
						vprint( 0, ", unnecessary\n" );
						madechanges = true;
					}
					else
					{
						// Restore line
						memcpy( startofline, saveinfo, 2 );
						vprint( 0, "\n" );
					}
				}
				else
				{
					vprint( 0, "\n" );
				}
			}
		}

		current = CC_ParseToken( current );
	}

	// Save out last set of changes
	{
		FILE *fp;
		fp = fopen( filename, "wb" );
		if ( fp )
		{
			fwrite( buffer, filelength, 1, fp );
			fclose( fp );
		}
	}

	COM_FreeFile( (unsigned char *)buffer );

	if ( !madechanges )
	{
		RestoreBackup( filename, readonly );
	}

	if ( !forcequiet && !GetQuiet() )
	{
		vprint( 0, " %s: headers (%i)", (char *)&filename[ m_nOffset ], numheaders );
		if ( maxdepth > 1 )
		{
			vprint( 0, ", depth %i", maxdepth );
		}
		vprint( 0, "\n" );
	}

	m_nLinesOfCode += linesprocessed;
	linesprocessed = 0;
}

void CCodeProcessor::ProcessModules( const char *root, const char *rootmodule )
{
	m_nFilesProcessed++;

	// Reset header list per module
	m_nHeaderCount = 0;
	m_nModuleCount = 0;

	int numheaders = 0;
	int maxdepth = 0;
	int skippedfiles = 0;

	bool canstart = false;

	if ( strstr( root, "tf2_hud" ) )
	{
		canstart = true;
	}
	if ( !canstart )
	{
		vprint( 0, "skipping %s\n", rootmodule );
		return;
	}
	ProcessModule( false, 0, maxdepth, numheaders, skippedfiles, root, root, rootmodule );
}

void CCodeProcessor::PrintResults( void )
{
	vprint( 0, "\nChecking for errors and totaling...\n\n" );

	vprint( 0, "parsed from %i files ( %i headers parsed )\n",
		m_nFilesProcessed,
		m_nHeadersProcessed );

	vprint( 0, "%.3f K lines of code processed\n",
		(double)m_nLinesOfCode / 1024.0 );
	
	double elapsed = ( m_flEnd - m_flStart );

	if ( elapsed > 0.0 )
	{
		vprint( 0, "%.2f K processed in %.3f seconds, throughput %.2f KB/sec\n\n",
			(double)m_nBytesProcessed / 1024.0, elapsed, (double)m_nBytesProcessed / ( 1024.0 * elapsed ) );
	}
}

CCodeProcessor::CCodeProcessor( void )
{
	m_nModuleCount					= 0;

	m_bQuiet						= false;
	m_bLogToFile					= false;

	m_nFilesProcessed				= 0;
	m_nHeadersProcessed				= 0;
	m_nOffset						= 0;
	m_nBytesProcessed				= 0;
	m_nLinesOfCode					= 0;
	m_flStart						= 0.0;
	m_flEnd							= 0.0;

	m_szCurrentCPP[ 0 ]				= 0;
}

CCodeProcessor::~CCodeProcessor( void )
{
}

char const *stristr( char const *src, char const *search )
{
	char buf1[ 512 ];
	char buf2[ 512 ];

	strcpy( buf1, src );
	_strlwr( buf1 );

	strcpy( buf2, search );
	_strlwr( buf2 );

	char *p =  strstr( buf1, buf2 );
	if ( p )
	{
		int len = p - buf1;
		return src + len;
	}
	return NULL;
}

void CCodeProcessor::ConstructModuleList_R( int level, const char *gamespecific, const char *root )
{
	char directory[ 256 ];
	char filename[ 256 ];
	WIN32_FIND_DATA wfd;
	HANDLE ff;

	sprintf( directory, "%s\\*.*", root );

	if ( ( ff = FindFirstFile( directory, &wfd ) ) == INVALID_HANDLE_VALUE )
		return;

	do
	{
		if ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{

			if ( wfd.cFileName[ 0 ] == '.' )
				continue;

			// Once we descend down a branch, don't keep looking for hl2/tf2 in name, just recurse through all children
			if ( level == 0 && !stristr( wfd.cFileName, gamespecific ) )
				continue;

			// Recurse down directory
			sprintf( filename, "%s\\%s", root, wfd.cFileName );
			ConstructModuleList_R( level+1, gamespecific, filename );
		}
		else
		{
			if ( strstr( wfd.cFileName, ".cpp" ) )
			{
				ProcessModules( root, wfd.cFileName );
			}
		}
	} while ( FindNextFile( ff, &wfd ) );
}

void CCodeProcessor::Process( const char *gamespecific, const char *root, const char *dsp, const char *config )
{
	strcpy( m_szDSP, dsp );
	strcpy( m_szConfig, config );

	m_nBytesProcessed	= 0;
	m_nFilesProcessed	= 0;
	m_nHeadersProcessed = 0;
	m_nLinesOfCode		= 0;

	linesprocessed		= 0;

	m_nOffset = strlen( root ) + 1;

	m_nModuleCount = 0;
	m_nHeaderCount = 0;
	m_flStart = UTIL_FloatTime();

	vprint( 0, "--- Processing %s\n\n", root );

	ConstructModuleList_R( 0, gamespecific, root );

	m_flEnd = UTIL_FloatTime();

	PrintResults();
}

void CCodeProcessor::SetQuiet( bool quiet )
{
	m_bQuiet = quiet;
}

bool CCodeProcessor::GetQuiet( void ) const
{
	return m_bQuiet;
}

void CCodeProcessor::SetLogFile( bool log )
{
	m_bLogToFile = log;
}

bool CCodeProcessor::GetLogFile( void ) const
{
	return m_bLogToFile;
}

static CCodeProcessor g_Processor;
ICodeProcessor *processor = ( ICodeProcessor * )&g_Processor;