//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Passthrough/stubbed version of Steamlib.h/Steam.c for Linux non-Steam server

#ifndef STEAMLIB_NULL_H
#define STEAMLIB_NULL_H


#include <stdlib.h>
#include <stdio.h>
#include <string.h> // for strlen()
#include <stdarg.h>
#include <dlfcn.h>
#if !defined ( _WIN32 )
#include <string.h>
#endif


typedef enum					// Filter elements returned by SteamFind{First,Next}
{
	STEAMFindLocalOnly,			// limit search to local filesystem
	STEAMFindRemoteOnly,			// limit search to remote repository
	STEAMFindAll					// do not limit search (duplicates allowed)
} STEAMFindFilter;


//	Direct drop-in replacements
#define STEAM_fopen		fopen
#define STEAM_fclose	fclose
#define STEAM_fread		fread
#define STEAM_fwrite	fwrite
#define STEAM_fprintf	fprintf
#define STEAM_fseek		fseek
#define STEAM_ftell		ftell
#define STEAM_feof		feof
#define STEAM_ferror	ferror
#define STEAM_clearerr	clearerr
#define STEAM_rewind	rewind
#define STEAM_fflush	fflush
#define STEAM_setvbuf	setvbuf


static inline int	STEAM_Mount( void )		{ return 1; }
static inline void	STEAM_Shutdown( void )	{	}

static inline unsigned int STEAM_FileSize( FILE *file )
{
	unsigned int iRet;

	if ( !file )
	{
		iRet = 0;
	}
	else
	{
		int iPos;

		iPos = ftell( file );
		fseek ( file, 0, SEEK_END );
		iRet = ftell( file );
		fseek ( file, iPos, SEEK_SET );
	}

	return iRet;
}


static inline void STEAM_strerror( FILE *stream, char * p, int maxlen )
{
	snprintf( p, maxlen, "Steam FS unavailable" );
}

static inline void STEAM_GetLocalCopy( const char * fileName )	{	}

static inline void STEAM_LogLevelLoadStarted( const char * name )		{	}
static inline void STEAM_LogLevelLoadFinished( const char * name )	{	}

static inline int STEAM_HintResourceNeed( const char * mapcycle, int forgetEverything )	{ return 1; }

static inline void STEAM_TrackProgress( int enable )	{	}
static inline void STEAM_UpdateProgress( void )		{	}

static inline void STEAM_RegisterAppProgressCallback( void( *fpProgCallBack )( void ), int freq )				{	}
static inline void STEAM_RegisterAppKeepAliveTicCallback( void( *fpKeepAliveTicCallBack )( char * scr_msg ) )	{	}

static inline void STEAM_GetInterfaceVersion( char * p, int maxlen )
{
	snprintf( p, maxlen, "0.0.0.0" );
}

static inline long STEAM_LoadLibrary( char * pszFileName )
{
	void *	hDll;
	char	szCwd[ MAX_OSPATH ];
	char	szAbsoluteLib[ MAX_OSPATH ];
    
	if ( !getcwd( szCwd, sizeof( szCwd ) ) )
		return 0;
        
	if ( szCwd[ strlen( szCwd ) -1 ] == '/' )
	{
		szCwd[ strlen( szCwd ) -1 ] = 0;
	}
        
	snprintf( szAbsoluteLib, sizeof( szAbsoluteLib ), "%s/%s", szCwd, pszFileName );
    
	hDll = dlopen( szAbsoluteLib, RTLD_NOW );

	return (long)hDll;
}


#endif	// STEAMLIB_NULL_H
