//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "cbase.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "filesystem.h"
#include "mxtk/mx.h"
#include "mxStatusWindow.h"
#include "filesystem.h"
#include "StudioModel.h"
#include "ControlPanel.h"
#include "MDLViewer.h"
#include "mxExpressionTray.H"
#include "viewersettings.h"
#include "tier1/strtools.h"
#include "faceposer_models.h"
#include "expressions.h"
#include "choreoview.h"
#include "choreoscene.h"
#include "vstdlib/random.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundchars.h"
#include "sentence.h"
#include "PhonemeEditor.h"
#include <vgui/ILocalize.h>
#include "filesystem_init.h"
#include "tier2/p4helpers.h"


extern vgui::ILocalize *g_pLocalize;

StudioModel *FindAssociatedModel( CChoreoScene *scene, CChoreoActor *a );


//-----------------------------------------------------------------------------
// Purpose: Takes a full path and determines if the file exists on the disk
// Input  : *filename - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FPFullpathFileExists( const char *filename )
{
	// Should be a full path
	Assert( strchr( filename, ':' ) );

	struct _stat buf;
	int result = _stat( filename, &buf );
	if ( result != -1 )
		return true;

	return false;
}

// Utility functions mostly
char *FacePoser_MakeWindowsSlashes( char *pname )
{
	static char returnString[ 4096 ];
	strcpy( returnString, pname );
	pname = returnString;

	while ( *pname ) 
	{
		if ( *pname == '/' )
		{
			*pname = '\\';
		}
		pname++;
	}

	return returnString;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int GetCloseCaptionLanguageId()
{
	return g_viewerSettings.cclanguageid;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
//-----------------------------------------------------------------------------
void SetCloseCaptionLanguageId( int id, bool force /* = false */ )
{
	Assert( id >= 0 && id < CC_NUM_LANGUAGES );
	bool changed = g_viewerSettings.cclanguageid != id;
	g_viewerSettings.cclanguageid = id;
	if ( changed || force )
	{
		// Switch languages
		char const *suffix = CSentence::NameForLanguage( id );
		if ( Q_stricmp( suffix, "unknown_language" ) )
		{
			char fn[ MAX_PATH ];
			Q_snprintf( fn, sizeof( fn ), "resource/closecaption_%s.txt", suffix );

			g_pLocalize->RemoveAll();

			if ( Q_stricmp( suffix, "english" )&&
				filesystem->FileExists( "resource/closecaption_english.txt" ) )
			{
				g_pLocalize->AddFile( "resource/closecaption_english.txt", "GAME", true );
			}

			if ( filesystem->FileExists( fn ) )
			{
				g_pLocalize->AddFile( fn, "GAME", true );
			}
			else
			{
				Con_ErrorPrintf( "PhonemeEditor::SetCloseCaptionLanguageId  Warning, can't find localization file %s\n", fn );
			}

			// Need to redraw the choreoview at least
			if ( g_pChoreoView )
			{
				g_pChoreoView->InvalidateLayout();
			}
		}
	}

	if ( g_MDLViewer )
	{
		g_MDLViewer->UpdateLanguageMenu( id );
	}
}


char *va( const char *fmt, ... )
{
	va_list args;
	static char output[32][1024];
	static int outbuffer = 0;

	outbuffer++;
	va_start( args, fmt );
	vprintf( fmt, args );
	vsprintf( output[ outbuffer & 31 ], fmt, args );
	return output[ outbuffer & 31 ];
}

void Con_Printf( const char *fmt, ... )
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

	g_pStatusWindow->StatusPrint( CONSOLE_COLOR, false, output );
}

void Con_ColorPrintf( COLORREF rgb, const char *fmt, ... )
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

	g_pStatusWindow->StatusPrint( rgb, false, output );
}

void Con_ErrorPrintf( const char *fmt, ... )
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

	g_pStatusWindow->StatusPrint( ERROR_COLOR, false, output );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void MakeFileWriteable( const char *filename )
{
	Assert( filesystem );
	char pFullPathBuf[ 512 ];
	char *pFullPath;
	if ( !Q_IsAbsolutePath( filename ) )
	{
		pFullPath = (char*)filesystem->RelativePathToFullPath( filename, NULL, pFullPathBuf, sizeof(pFullPathBuf) );
	}
	else
	{
		Q_strncpy( pFullPathBuf, filename, sizeof(pFullPathBuf) );
		pFullPath = pFullPathBuf;
	}

	if ( pFullPath )
	{
		Q_FixSlashes( pFullPath );
		SetFileAttributes( pFullPath, FILE_ATTRIBUTE_NORMAL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool IsFileWriteable( const char *filename )
{
	Assert( filesystem );
	char pFullPathBuf[ 512 ];
	char *pFullPath;
	if ( !Q_IsAbsolutePath( filename ) )
	{
		pFullPath = (char*)filesystem->RelativePathToFullPath( filename, NULL, pFullPathBuf, sizeof(pFullPathBuf) );
	}
	else
	{
		Q_strncpy( pFullPathBuf, filename, sizeof(pFullPathBuf) );
		pFullPath = pFullPathBuf;
	}

	if ( pFullPath )
	{
		Q_FixSlashes( pFullPath );
		DWORD attrib = GetFileAttributes( pFullPath );
		return ( ( attrib & FILE_ATTRIBUTE_READONLY ) == 0 );
	}

	// Doesn't seem to exist, so yeah, it's writable
	return true;
}

bool MakeFileWriteablePrompt( const char *filename, char const *promptTitle )
{
	if ( !IsFileWriteable( filename ) )
	{
		int retval = mxMessageBox( NULL, va( "File '%s' is Read-Only, make writable?", filename ),
			promptTitle, MX_MB_WARNING | MX_MB_YESNO );

		// Didn't pick yes, bail
		if ( retval != 0 )
			return false;

		MakeFileWriteable( filename );
	}

	return true;
}

void FPCopyFile( const char *source, const char *dest, bool bCheckOut )
{
	Assert( filesystem );
	char fullpaths[ MAX_PATH ];
	char fullpathd[ MAX_PATH ];

	if ( !Q_IsAbsolutePath( source ) )
	{
		filesystem->RelativePathToFullPath( source, NULL, fullpaths, sizeof(fullpaths) );
	}
	else
	{
		Q_strncpy( fullpaths, source, sizeof(fullpaths) );
	}

	Q_strncpy( fullpathd, fullpaths, MAX_PATH );
	char *pSubdir = Q_stristr( fullpathd, source );
	if ( pSubdir )
	{
		*pSubdir = 0;
	}
	Q_AppendSlash( fullpathd, MAX_PATH );
	Q_strncat( fullpathd, dest, MAX_PATH, MAX_PATH );

	Q_FixSlashes( fullpaths );
	Q_FixSlashes( fullpathd );

	if ( bCheckOut )
	{
		CP4AutoEditAddFile checkout( fullpathd );
		CopyFile( fullpaths, fullpathd, FALSE );
	}
	else
	{
		CopyFile( fullpaths, fullpathd, FALSE );
	}
}

bool FacePoser_HasWindowStyle( mxWindow *w, int bits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_STYLE );
	return ( style & bits ) ? true : false;
}

bool FacePoser_HasWindowExStyle( mxWindow *w, int bits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_EXSTYLE );
	return ( style & bits ) ? true : false;
}

void FacePoser_AddWindowStyle( mxWindow *w, int addbits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_STYLE );
	style |= addbits;
	SetWindowLong( wnd, GWL_STYLE, style );
}

void FacePoser_AddWindowExStyle( mxWindow *w, int addbits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_EXSTYLE );
	style |= addbits;
	SetWindowLong( wnd, GWL_EXSTYLE, style );
}

void FacePoser_RemoveWindowStyle( mxWindow *w, int removebits )
{
	HWND wnd = (HWND)w->getHandle();
	DWORD style = GetWindowLong( wnd, GWL_STYLE );
	style &= ~removebits;
	SetWindowLong( wnd, GWL_STYLE, style );
}

void FacePoser_RemoveWindowExStyle( mxWindow *w, int removebits )
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
void FacePoser_MakeToolWindow( mxWindow *w, bool smallcaption )
{
	FacePoser_AddWindowStyle( w, WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
	if ( smallcaption )
	{
		FacePoser_AddWindowExStyle( w, WS_EX_OVERLAPPEDWINDOW );
		FacePoser_AddWindowExStyle( w, WS_EX_TOOLWINDOW );
	}
}

bool LoadViewerSettingsInt( char const *keyname, int *value );
bool SaveViewerSettingsInt ( const char *keyname, int value );

void FacePoser_LoadWindowPositions( char const *name, bool& visible, int& x, int& y, int& w, int& h, bool& locked, bool& zoomed )
{
	char subkey[ 512 ];
	int v;

	Q_snprintf( subkey, sizeof( subkey ), "%s - visible", name );
	LoadViewerSettingsInt( subkey, &v );
	visible = v ? true : false;
	
	Q_snprintf( subkey, sizeof( subkey ), "%s - locked", name );
	LoadViewerSettingsInt( subkey, &v );
	locked = v ? true : false;

	Q_snprintf( subkey, sizeof( subkey ), "%s - zoomed", name );
	LoadViewerSettingsInt( subkey, &v );
	zoomed = v ? true : false;

	Q_snprintf( subkey, sizeof( subkey ), "%s - x", name );
	LoadViewerSettingsInt( subkey, &x );
	Q_snprintf( subkey, sizeof( subkey ), "%s - y", name );
	LoadViewerSettingsInt( subkey, &y );
	Q_snprintf( subkey, sizeof( subkey ), "%s - width", name );
	LoadViewerSettingsInt( subkey, &w );
	Q_snprintf( subkey, sizeof( subkey ), "%s - height", name );
	LoadViewerSettingsInt( subkey, &h );
}

void FacePoser_SaveWindowPositions( char const *name, bool visible, int x, int y, int w, int h, bool locked, bool zoomed )
{
	char subkey[ 512 ];
	Q_snprintf( subkey, sizeof( subkey ), "%s - visible", name );
	SaveViewerSettingsInt( subkey, visible );
	Q_snprintf( subkey, sizeof( subkey ), "%s - locked", name );
	SaveViewerSettingsInt( subkey, locked );
	Q_snprintf( subkey, sizeof( subkey ), "%s - x", name );
	SaveViewerSettingsInt( subkey, x );
	Q_snprintf( subkey, sizeof( subkey ), "%s - y", name );
	SaveViewerSettingsInt( subkey, y );
	Q_snprintf( subkey, sizeof( subkey ), "%s - width", name );
	SaveViewerSettingsInt( subkey, w );
	Q_snprintf( subkey, sizeof( subkey ), "%s - height", name );
	SaveViewerSettingsInt( subkey, h );
	Q_snprintf( subkey, sizeof( subkey ), "%s - zoomed", name );
	SaveViewerSettingsInt( subkey, zoomed );
}

static char g_PhonemeRoot[ MAX_PATH ] = { 0 };
void FacePoser_SetPhonemeRootDir( char const *pchRootDir )
{
	Q_strncpy( g_PhonemeRoot, pchRootDir, sizeof( g_PhonemeRoot ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void FacePoser_EnsurePhonemesLoaded( void )
{
	// Don't bother unless a model is loaded, at least...
	CStudioHdr *hdr = models->GetActiveStudioModel()->GetStudioHdr();
	if ( !hdr )
	{
		return;
	}

	char const *ext[] = 
	{
		"",
		"_strong",
		"_weak",
	};

	for ( int i = 0 ; i < ARRAYSIZE( ext ); ++i )
	{
		char clname[ 256 ];
		Q_snprintf( clname, sizeof( clname ), "%sphonemes%s", g_PhonemeRoot, ext[ i ] );
		Q_FixSlashes( clname );
		Q_strlower( clname );

		if ( !expressions->FindClass( clname, false ) )
		{
			char clfile[ MAX_PATH ];
			Q_snprintf( clfile, sizeof( clfile ), "expressions/%sphonemes%s.txt", g_PhonemeRoot, ext[ i ] );
			Q_FixSlashes( clfile );
			Q_strlower( clfile );

			if ( g_pFileSystem->FileExists( clfile ) )
				{
				expressions->LoadClass( clfile );
				CExpClass *cl = expressions->FindClass( clname, false );
				if ( !cl )
				{
					Con_Printf( "FacePoser_EnsurePhonemesLoaded:  %s missing!!!\n", clfile );
				}
			}
		}
	}
}

bool FacePoser_ShowFileNameDialog( bool openFile, char *relative, size_t bufsize, char const *subdir, char const *wildcard )
{
	Assert( relative );
	relative[ 0 ] = 0 ;
	Assert( subdir );
	Assert( wildcard );

	char workingdir[ 256 ];
	Q_getwd( workingdir, sizeof( workingdir ) );
	strlwr( workingdir );
	Q_FixSlashes( workingdir, '/' );

	// Show file io
	bool inWorkingDirectoryAlready = false;
	if ( Q_stristr_slash( workingdir, va( "%s%s", GetGameDirectory(), subdir ) ) )
	{
		inWorkingDirectoryAlready = true;
	}

// Show file io
	const char *fullpath = NULL;
	
	if ( openFile )
	{
		fullpath = mxGetOpenFileName( 
			0, 
			inWorkingDirectoryAlready ? "." : FacePoser_MakeWindowsSlashes( va( "%s%s/", GetGameDirectory(), subdir ) ), 
			wildcard );
	}
	else
	{
		fullpath = mxGetSaveFileName( 
			0, 
			inWorkingDirectoryAlready ? "." : FacePoser_MakeWindowsSlashes( va( "%s%s/", GetGameDirectory(), subdir ) ), 
			wildcard );
	}
	if ( !fullpath || !fullpath[ 0 ] )
		return false;

	Q_strncpy( relative, fullpath, bufsize );
	return true;
}

bool FacePoser_ShowOpenFileNameDialog( char *relative, size_t bufsize, char const *subdir, char const *wildcard )
{
	return FacePoser_ShowFileNameDialog( true, relative, bufsize, subdir, wildcard );
}

bool FacePoser_ShowSaveFileNameDialog( char *relative, size_t bufsize, char const *subdir, char const *wildcard )
{
	return FacePoser_ShowFileNameDialog( false, relative, bufsize, subdir, wildcard );
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

//-----------------------------------------------------------------------------
// Purpose: If FPS is set and "using grid", snap to proper fractional time value
// Input  : t - 
// Output : float
//-----------------------------------------------------------------------------
float FacePoser_SnapTime( float t )
{
	if ( !g_pChoreoView )
		return t;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return t;

	return scene->SnapTime( t );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : t - 
// Output : char const
//-----------------------------------------------------------------------------
char const *FacePoser_DescribeSnappedTime( float t )
{
	static char desc[ 128 ];
	Q_snprintf( desc, sizeof( desc ), "%.3f", t );

	if ( !g_pChoreoView )
		return desc;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return desc;

	t = scene->SnapTime( t );

	int fps = scene->GetSceneFPS();

	int ipart = (int)t;
	int fracpart = (int)( ( t - (float)ipart ) * (float)fps + 0.5f );

	int frame = ipart * fps + fracpart;

	if ( fracpart == 0 )
	{
		Q_snprintf( desc, sizeof( desc ), "frame %i (time %i s.)", frame, ipart );
	}
	else
	{
		Q_snprintf( desc, sizeof( desc ), "frame %i (time %i + %i/%i s.)", 
			frame, ipart,fracpart, fps );
	}

	return desc;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int FacePoser_GetSceneFPS( void )
{
	if ( !g_pChoreoView )
		return 1000;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return 1000;

	return scene->GetSceneFPS();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool FacePoser_IsSnapping( void )
{
	if ( !g_pChoreoView )
		return false;

	CChoreoScene *scene = g_pChoreoView->GetScene();
	if ( !scene )
		return false;

	return scene->IsUsingFrameSnap();
}

char const *FacePoser_TranslateSoundNameGender( char const *soundname, gender_t gender )
{
	if ( Q_stristr( soundname, ".wav" ) )
		return PSkipSoundChars( soundname );

	return PSkipSoundChars( soundemitter->GetWavFileForSound( soundname, gender ) );
}

char const *FacePoser_TranslateSoundName( char const *soundname, StudioModel *model /*= NULL*/ )
{
	if ( Q_stristr( soundname, ".wav" ) )
		return PSkipSoundChars( soundname );

	static char temp[ 256 ];

	if ( model )
	{
		Q_strncpy( temp, PSkipSoundChars( soundemitter->GetWavFileForSound( soundname, model->GetFileName() ) ), sizeof( temp ) );
	}
	else
	{
		Q_strncpy( temp, PSkipSoundChars( soundemitter->GetWavFileForSound( soundname, NULL ) ), sizeof( temp ) );
	}
	return temp;
}

char const *FacePoser_TranslateSoundName( CChoreoEvent *event )
{
	char const *soundname = event->GetParameters();
	if ( Q_stristr( soundname, ".wav" ) )
		return PSkipSoundChars( soundname );

	// See if we can figure out the .mdl associated to this event's actor
	static char temp[ 256 ];
	temp[ 0 ] = 0;
	StudioModel *model = NULL;

	CChoreoActor *a = event->GetActor();
	CChoreoScene *s = event->GetScene();

	if ( a != NULL && 
		 s != NULL )
	{
		model = FindAssociatedModel( s, a );
	}

	Q_strncpy( temp, PSkipSoundChars( soundemitter->GetWavFileForSound( soundname, model ? model->GetFileName() : NULL ) ), sizeof( temp ) );
	return temp;
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

static CUniformRandomStream g_Random;
IUniformRandomStream *random = &g_Random;
