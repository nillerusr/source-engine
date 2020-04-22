//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "cbase.h"
#include "StatusWindow.h"
#include "cmdlib.h"
#include <sys/stat.h>
#include "workspace.h"
#include "workspacemanager.h"
#include "workspacebrowser.h"
#include "tier2/riff.h"
#include "sentence.h"
#include "utlbuffer.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include <KeyValues.h>
#include "MultipleRequest.h"

bool SceneManager_HasWindowStyle( mxWindow *w, int bits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_STYLE );
	return ( style & bits ) ? true : false;
}

bool SceneManager_HasWindowExStyle( mxWindow *w, int bits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_EXSTYLE );
	return ( style & bits ) ? true : false;
}

void SceneManager_AddWindowStyle( mxWindow *w, int addbits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_STYLE );
	style |= addbits;
	SetWindowLong( wnd, GWL_STYLE, style );
}

void SceneManager_AddWindowExStyle( mxWindow *w, int addbits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_EXSTYLE );
	style |= addbits;
	SetWindowLong( wnd, GWL_EXSTYLE, style );
}

void SceneManager_RemoveWindowStyle( mxWindow *w, int removebits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_STYLE );
	style &= ~removebits;
	SetWindowLong( wnd, GWL_STYLE, style );
}

void SceneManager_RemoveWindowExStyle( mxWindow *w, int removebits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_EXSTYLE );
	style &= ~removebits;
	SetWindowLong( wnd, GWL_EXSTYLE, style );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *w - 
//-----------------------------------------------------------------------------
void SceneManager_MakeToolWindow( mxWindow *w, bool smallcaption )
{
	SceneManager_AddWindowStyle( w, WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );

	if ( smallcaption )
	{
		SceneManager_AddWindowExStyle( w, WS_EX_OVERLAPPEDWINDOW );
		SceneManager_AddWindowExStyle( w, WS_EX_TOOLWINDOW );
	}
	else
	{
		SceneManager_RemoveWindowStyle( w, WS_SYSMENU );
	}
}

char *va( const char *fmt, ... )
{
	va_list args;
	static char output[4][1024];
	static int outbuffer = 0;

	outbuffer++;
	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output[ outbuffer & 3 ], fmt, args );
	return output[ outbuffer & 3 ];
}

void Con_Overprintf( const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output, fmt, args );

	if ( !g_pStatusWindow )
	{
		return;
	}

	g_pStatusWindow->StatusPrint( CONSOLE_R, CONSOLE_G, CONSOLE_B, true, output );
}

void Con_Printf( const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
//	vprintf( fmt, args );
	vsprintf( output, fmt, args );
	va_end( args );

	if ( !g_pStatusWindow )
	{
		return;
	}

	g_pStatusWindow->StatusPrint( CONSOLE_R, CONSOLE_G, CONSOLE_B, false, output );
}

void Con_ColorPrintf( int r, int g, int b, const char *fmt, ... )
{
	va_list args;
	static char output[1024];

	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output, fmt, args );

	if ( !g_pStatusWindow )
	{
		return;
	}

	g_pStatusWindow->StatusPrint( r, g, b, false, output );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pname - 
// Output : char
//-----------------------------------------------------------------------------
char *SceneManager_MakeWindowsSlashes( char *pname )
{
	static char returnString[ 4096 ];
	strcpy( returnString, pname );
	pname = returnString;

	while ( *pname ) {
		if ( *pname == '/' )
			*pname = '\\';
		pname++;
	}

	return returnString;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *SceneManager_GetGameDirectory( void )
{
	// Todo: Make SceneManager only use the filesystem read/write paths.
	return gamedir;
}

//-----------------------------------------------------------------------------
// Purpose: Takes a full path and determines if the file exists on the disk
// Input  : *filename - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool SceneManager_FullpathFileExists( const char *filename )
{
	// Should be a full path
	Assert( strchr( filename, ':' ) );

	struct _stat buf;
	int result = _stat( filename, &buf );
	if ( result != -1 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: converts an english string to unicode
//-----------------------------------------------------------------------------
int ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSize)
{
	return ::MultiByteToWideChar(CP_ACP, 0, ansi, -1, unicode, unicodeBufferSize);
}

//-----------------------------------------------------------------------------
// Purpose: converts an unicode string to an english string
//-----------------------------------------------------------------------------
int ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize)
{
	return ::WideCharToMultiByte(CP_ACP, 0, unicode, -1, ansi, ansiBufferSize, NULL, NULL);
}

int Sys_Exec( const char *pProgName, const char *pCmdLine, bool verbose )
{
#if 0
	int count = 0;
	char cmdLine[1024];
	STARTUPINFO si;

	memset( &si, 0, sizeof(si) );
	si.cb = sizeof(si);
	//GetStartupInfo( &si );

	sprintf( cmdLine, "%s %s", pProgName, pCmdLine );

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;

	if ( CreateProcess( NULL, cmdLine, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi ) )
	{
		WaitForSingleObject( pi.hProcess, INFINITE );
		/*
		do 
		{
			WaitForInputIdle( pi.hProcess, 100 );
			count++;
		} while ( count < 100 );
		*/

		/*
		DWORD exitCode = STILL_ACTIVE;
		do 
		{
			BOOL ok = GetExitCodeProcess( pi.hProcess, &exitCode );
			if ( !ok )
				break;
			Sleep( 100 );
		} while ( exitCode == STILL_ACTIVE );
		*/
		
		DWORD exitCode;
		
		GetExitCodeProcess( pi.hProcess, &exitCode );

		Con_Printf( "Finished\n" );
		
		CloseHandle( pi.hProcess );
		return (int)exitCode;
	}
	else
	{
			char *lpMsgBuf;
			
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

			Con_Printf( "error %s\n", lpMsgBuf );

			LocalFree( (HLOCAL)lpMsgBuf );
	}

	return false;
#else
	char tmp[1024];
	sprintf( tmp, "%s %s\n", pProgName, pCmdLine );

	_strlwr( tmp );

	int iret = system( tmp );
	if ( iret != 0 && verbose )
	{
		Con_Printf( "Execution failed: %s\n", tmp );
	}
	
	return iret;
#endif
}

//-----------------------------------------------------------------------------
// Checks it out/ checks it in baby
//-----------------------------------------------------------------------------

static void SceneManager_VSSCheckout( char const *pUserName, char const *pProjectDir, 
	char const* pRelativeDir, char const* pDestPath, char const* pFileNameWithExtension )
{
	char buf[1024];

	// Check for the existence of the file in source safe...
	sprintf( buf, "filetype %s/%s%s -O- -y%s\n",
		pProjectDir, pRelativeDir, pFileNameWithExtension,
		pUserName );
	int retVal = Sys_Exec( "ss.exe", buf, false );
	if (retVal != 0 )
	{
		Con_Printf( "File %s missing from VSS\n", pFileNameWithExtension );
		return;
	}

	// It's there, try to check it out
	sprintf( buf, "checkout %s/%s%s -GL%s -GWA -O- -y%s\n",
		pProjectDir, pRelativeDir, pFileNameWithExtension,
		pDestPath, pUserName );
	Sys_Exec( "ss.exe", buf, true );
}

//-----------------------------------------------------------------------------
// Checks it out/ checks it in baby
//-----------------------------------------------------------------------------

static void SceneManager_VSSCheckin( char const *pUserName, char const *pProjectDir, 
	char const* pRelativeDir, char const* pDestPath, char const* pFileNameWithExtension )
{
	char buf[1024];

	// Check for the existence of the file on disk. If it's not there, don't bother
	sprintf( buf, "%s%s", pDestPath, pFileNameWithExtension );
	struct _stat statbuf;
	int result = _stat( buf, &statbuf );
	if (result != 0)
		return;

	// Check for the existence of the file in source safe...
	sprintf( buf, "filetype %s/%s%s -O- -y%s\n",
		pProjectDir, pRelativeDir, pFileNameWithExtension,
		pUserName );
	int retVal = Sys_Exec( "ss.exe", buf, false );
	if (retVal != 0)
	{
		sprintf( buf, "Cp %s -O- -y%s\n",
			pProjectDir ,
			pUserName );
		Sys_Exec( "ss.exe", buf, true );

		// Try to add the file to source safe...
		sprintf( buf, "add %s%s -GL%s -O- -I- -y%s\n",
			pRelativeDir, pFileNameWithExtension,
			pDestPath, pUserName );
		Sys_Exec( "ss.exe", buf, true );
	}
	else
	{
		// It's there, just check it in
		sprintf( buf, "checkin %s/%s%s -GL%s -O- -I- -y%s\n",
			pProjectDir, pRelativeDir, pFileNameWithExtension,
			pDestPath, pUserName );
		Sys_Exec( "ss.exe", buf, true );
	}
}

void SplitFileName( char const *in, char *path, int maxpath, char *filename, int maxfilename )
{
   char drive[_MAX_DRIVE];
   char dir[_MAX_DIR];
   char fname[_MAX_FNAME];
   char ext[_MAX_EXT];

   _splitpath( in, drive, dir, fname, ext );

   if ( dir[0] )
   {
		Q_snprintf( path, maxpath, "\\%s", dir );
   }
   else
   {
	   path[0] = 0;
   }
   Q_snprintf( filename, maxfilename, "%s%s", fname, ext );
}


void VSS_Checkout( char const *name, bool updatestaticons /*= true*/ )
{
	CWorkspace *ws = GetWorkspaceManager()->GetBrowser()->GetWorkspace();
	if ( !ws )
	{
		return;
	}

	if ( filesystem->IsFileWritable( name ) )
	{
		return;
	}

	char path[ 256 ];
	char filename[ 256 ];

	SplitFileName( name, path, sizeof( path ), filename, sizeof( filename ) );

	Con_ColorPrintf( 200, 200, 100, "VSS Checkout:  '%s'\n", name );

	SceneManager_VSSCheckout( 
		ws->GetVSSUserName(),
		ws->GetVSSProject(),
		path,
		va( "%s%s", gamedir, path ),
		filename );

	if ( updatestaticons )
	{
		GetWorkspaceManager()->RefreshBrowsers();
	}
}

void VSS_Checkin( char const *name, bool updatestaticons /*= true*/ )
{
	CWorkspace *ws = GetWorkspaceManager()->GetBrowser()->GetWorkspace();
	if ( !ws )
	{
		return;
	}

	if ( !filesystem->IsFileWritable( name ) )
	{
		return;
	}

	char path[ 256 ];
	char filename[ 256 ];

	SplitFileName( name, path, sizeof( path ), filename, sizeof( filename ) );

	Con_ColorPrintf( 200, 200, 100, "VSS Checkin:  '%s'\n", name );

	SceneManager_VSSCheckin( 
		ws->GetVSSUserName(),
		ws->GetVSSProject(),
		path,
		va( "%s%s", gamedir, path ),
		filename );

	if ( updatestaticons )
	{
		GetWorkspaceManager()->RefreshBrowsers();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Implements the RIFF i/o interface on stdio
//-----------------------------------------------------------------------------
class StdIOReadBinary : public IFileReadBinary
{
public:
	int open( const char *pFileName )
	{
		return (int)filesystem->Open( pFileName, "rb" );
	}

	int read( void *pOutput, int size, int file )
	{
		if ( !file )
			return 0;

		return filesystem->Read( pOutput, size, (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		if ( !file )
			return;

		filesystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		if ( !file )
			return 0;

		return filesystem->Tell( (FileHandle_t)file );
	}

	unsigned int size( int file )
	{
		if ( !file )
			return 0;

		return filesystem->Size( (FileHandle_t)file );
	}

	void close( int file )
	{
		if ( !file )
			return;

		filesystem->Close( (FileHandle_t)file );
	}
};

class StdIOWriteBinary : public IFileWriteBinary
{
public:
	int create( const char *pFileName )
	{
		return (int)filesystem->Open( pFileName, "wb" );
	}

	int write( void *pData, int size, int file )
	{
		return filesystem->Write( pData, size, (FileHandle_t)file );
	}

	void close( int file )
	{
		filesystem->Close( (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		filesystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		return filesystem->Tell( (FileHandle_t)file );
	}
};

static StdIOReadBinary io_in;
static StdIOWriteBinary io_out;

#define RIFF_WAVE			MAKEID('W','A','V','E')
#define WAVE_FMT			MAKEID('f','m','t',' ')
#define WAVE_DATA			MAKEID('d','a','t','a')
#define WAVE_FACT			MAKEID('f','a','c','t')
#define WAVE_CUE			MAKEID('c','u','e',' ')

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &walk - 
//-----------------------------------------------------------------------------
static void SceneManager_ParseSentence( CSentence& sentence, IterateRIFF &walk )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.EnsureCapacity( walk.ChunkSize() );
	walk.ChunkRead( buf.Base() );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, walk.ChunkSize() );

	sentence.InitFromDataChunk( buf.Base(), buf.TellPut() );
}

bool SceneManager_LoadSentenceFromWavFileUsingIO( char const *wavfile, CSentence& sentence, IFileReadBinary& io )
{
	sentence.Reset();

	InFileRIFF riff( wavfile, io );

	// UNDONE: Don't use printf to handle errors
	if ( riff.RIFFName() != RIFF_WAVE )
	{
		return false;
	}

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	// This chunk must be first as it contains the wave's format
	// break out when we've parsed it
	bool found = false;
	while ( walk.ChunkAvailable() && !found )
	{
		switch( walk.ChunkName() )
		{
		case WAVE_VALVEDATA:
			{
				found = true;
				SceneManager_ParseSentence( sentence, walk );
			}
			break;
		}
		walk.ChunkNext();
	}

	return true;
}

bool SceneManager_LoadSentenceFromWavFile( char const *wavfile, CSentence& sentence )
{
	return SceneManager_LoadSentenceFromWavFileUsingIO( wavfile, sentence, io_in );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : store - 
//-----------------------------------------------------------------------------
static void SceneManager_StoreValveDataChunk( CSentence& sentence, IterateOutputRIFF& store )
{
	// Buffer and dump data
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	sentence.SaveToBuffer( buf );

	// Copy into store
	store.ChunkWriteData( buf.Base(), buf.TellPut() );
}

bool SceneManager_SaveSentenceToWavFile( char const *wavfile, CSentence& sentence )
{
	char tempfile[ 512 ];

	Q_StripExtension( wavfile, tempfile, sizeof( tempfile ) );
	Q_DefaultExtension( tempfile, ".tmp", sizeof( tempfile ) );

	if ( filesystem->FileExists( tempfile, "GAME" ) )
	{
		filesystem->RemoveFile( tempfile, "GAME" );
	}

	if ( !filesystem->IsFileWritable( wavfile ) )
	{
		int retval = MultipleRequest( va( "Check out '%s'?", wavfile ) );
		if ( retval != 0 )
			return false;

		VSS_Checkout( wavfile );
	}

	if ( !filesystem->IsFileWritable( wavfile ) )
	{
		Con_Printf( "%s is not writable, can't save sentence data to file\n", wavfile );
		return false;
	}
	
	// Rename original wavfile to temp
	filesystem->RenameFile( wavfile, tempfile, "GAME" );

	// NOTE:  Put this in it's own scope so that the destructor for outfileRFF actually closes the file!!!!
	{
		// Read from Temp
		InFileRIFF riff( tempfile, io_in );
		Assert( riff.RIFFName() == RIFF_WAVE );

		// set up the iterator for the whole file (root RIFF is a chunk)
		IterateRIFF walk( riff, riff.RIFFSize() );

		// And put data back into original wavfile by name
		OutFileRIFF riffout( wavfile, io_out );

		IterateOutputRIFF store( riffout );

		bool wordtrackwritten = false;

		// Walk input chunks and copy to output
		while ( walk.ChunkAvailable() )
		{
			store.ChunkStart( walk.ChunkName() );

			switch ( walk.ChunkName() )
			{
			case WAVE_VALVEDATA:
				{
					// Overwrite data
					SceneManager_StoreValveDataChunk( sentence, store );
					wordtrackwritten = true;
				}
				break;
			default:
				store.CopyChunkData( walk );
				break;
			}

			store.ChunkFinish();

			walk.ChunkNext();
		}

		// If we didn't write it above, write it now
		if ( !wordtrackwritten )
		{
			store.ChunkStart( WAVE_VALVEDATA );
			SceneManager_StoreValveDataChunk( sentence, store );
			store.ChunkFinish();
		}
	}

	// Remove temp file
	filesystem->RemoveFile( tempfile, NULL );

	return true;
}


void SceneManager_LoadWindowPositions( KeyValues *kv, mxWindow *wnd )
{
	bool zoomed = kv->GetInt( "zoomed", 0 ) ? true : false;
	int x = kv->GetInt( "x", 0 );
	int y = kv->GetInt( "y", 0 );
	int w = kv->GetInt( "w", 400 );
	int h = kv->GetInt( "h", 300 );

	wnd->setBounds( x, y, w, h );
	if ( zoomed )
	{
		ShowWindow( (HWND)wnd->getHandle(), SW_SHOWMAXIMIZED );
	}
}

static void Indent( CUtlBuffer& buf, int numtabs )
{
	for ( int i = 0 ; i < numtabs; i++ )
	{
		buf.Printf( "\t" );
	}
}

void SceneManager_SaveWindowPositions( CUtlBuffer& buf, int indent, mxWindow *wnd )
{
	int x, y, w, h;

	x = wnd->x();
	y = wnd->y();
	w = wnd->w();
	h = wnd->h();

	// xpos and ypos are screen space
	POINT pt;
	pt.x = x;
	pt.y = y;

	// Convert from screen space to relative to client area of parent window so
	//  the setBounds == MoveWindow call will offset to the same location
	if ( wnd->getParent() )
	{
		ScreenToClient( (HWND)wnd->getParent()->getHandle(), &pt );
		x = (short)pt.x;
		y = (short)pt.y;
	}


	Indent( buf, indent );
	buf.Printf( "\"x\"\t\"%i\"\n", x );

	Indent( buf, indent );
	buf.Printf( "\"y\"\t\"%i\"\n", y );

	Indent( buf, indent );
	buf.Printf( "\"w\"\t\"%i\"\n", w );

	Indent( buf, indent );
	buf.Printf( "\"h\"\t\"%i\"\n", h );

	bool zoomed = IsZoomed( (HWND)wnd->getHandle() ) ? true : false;

	Indent( buf, indent );
	buf.Printf( "\"zoomed\"\t\"%i\"\n", zoomed ? 1 : 0 );

}
#if defined( _WIN32 ) || defined( WIN32 )
#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')
#else	//_WIN32
#define PATHSEPARATOR(c) ((c) == '/')
#endif	//_WIN32

static bool charsmatch( char c1, char c2 )
{
	if ( tolower( c1 ) == tolower( c2 ) )
		return true;
	if ( PATHSEPARATOR( c1 ) && PATHSEPARATOR( c2 ) )
		return true;
	return false;
}


char *Q_stristr_slash( char const *pStr, char const *pSearch )
{
	AssertValidStringPtr(pStr);
	AssertValidStringPtr(pSearch);

	if (!pStr || !pSearch) 
		return 0;

	char const* pLetter = pStr;

	// Check the entire string
	while (*pLetter != 0)
	{
		// Skip over non-matches
		if ( charsmatch( *pLetter, *pSearch ) )
		{
			// Check for match
			char const* pMatch = pLetter + 1;
			char const* pTest = pSearch + 1;
			while (*pTest != 0)
			{
				// We've run off the end; don't bother.
				if (*pMatch == 0)
					return 0;

				if ( !charsmatch( *pMatch, *pTest ) )
					break;

				++pMatch;
				++pTest;
			}

			// Found a match!
			if (*pTest == 0)
				return (char *)pLetter;
		}

		++pLetter;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void MakeFileWriteable( const char *filename )
{
	Assert( filesystem );
	char fullpath[ 512 ];
	if (filesystem->GetLocalPath( filename, fullpath, sizeof(fullpath) ))
	{
		Q_FixSlashes( fullpath );
		SetFileAttributes( fullpath, FILE_ATTRIBUTE_NORMAL );
	}
}