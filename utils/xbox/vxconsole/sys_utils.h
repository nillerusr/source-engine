//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SYS_UTILS.H
//
//	System Utilities.
//=====================================================================================//
#pragma once

#include "vxconsole.h"

#pragma warning(disable : 4100) // warning C4100: unreferenced formal parameter

#define MAX_SYSPRINTMSG		1024
#define MAX_SYSTOKENCHARS	1024

#define MAKEINT64( hiword, loword )	( ( __int64 )( ( ( ( __int64 )( hiword ) ) << 32 ) | ( __int64 )( loword ) ) )

extern void		Sys_MessageBox( const CHAR *title, const CHAR *format, ... );
extern void		Sys_Error( const CHAR *format, ... );

extern void		*Sys_Alloc( int size );
extern void		Sys_Free( void *ptr );
extern CHAR		*Sys_CopyString( const CHAR *str );

extern int		Sys_LoadFile( const CHAR *filename, void **bufferptr, bool bText = false );
extern bool		Sys_SaveFile( const CHAR *filename, void *buffer, long count, bool bText = false );
extern long		Sys_FileLength( const CHAR *filename, bool bText = false );
extern long		Sys_FileLength( int handle );
extern BOOL		Sys_FileTime( CHAR *filename, FILETIME *time );
extern void		Sys_CreatePath( const char* pInPath );

extern void		Sys_AddFileSeperator( CHAR *path, int outPathLen );
extern void		Sys_NormalizePath( CHAR *path, bool forceToLower );
extern void		Sys_StripFilename( const CHAR *path, CHAR *outpath, int outPathLen );
extern void		Sys_StripExtension( const CHAR *path, CHAR *outpath, int outPathLen );
extern void		Sys_StripPath( const CHAR *path, CHAR *outpath, int outPathLen );
extern void		Sys_GetExtension( const CHAR *path, CHAR *outpath, int outPathLen );
extern void		Sys_AddExtension( const CHAR *extension, CHAR *outpath, int outPathLen );
extern void		Sys_TempFilename( CHAR *outpath, int outPathLen );
extern BOOL		Sys_Exists( const CHAR *filename );

extern CHAR		*Sys_GetToken( CHAR **dataptr, BOOL crossline, int *numlines );
extern CHAR		*Sys_SkipWhitespace( CHAR *data, BOOL *hasNewLines, int *numlines );
extern void		Sys_SkipBracedSection( CHAR **dataptr, int *numlines );
extern void		Sys_SkipRestOfLine( CHAR **dataptr, int *numlines );

extern void		Sys_SetRegistryPrefix( const CHAR *pPrefix );
extern BOOL		Sys_SetRegistryString( const CHAR *key, const CHAR *value );
extern BOOL		Sys_GetRegistryString( const CHAR *key, CHAR *value, const CHAR *defValue, int valueLen );
extern BOOL		Sys_SetRegistryInteger( const CHAR *key, int value );
extern BOOL		Sys_GetRegistryInteger( const CHAR *key, int defValue, int &value );

extern DWORD	Sys_GetSystemTime( void );
extern COLORREF	Sys_ColorScale( COLORREF color, float scale );
extern bool		Sys_IsWildcardMatch( const CHAR *wildcardString, const CHAR *stringToCheck, bool caseSensitive );
extern char		*Sys_NumberToCommaString( __int64 number, char *buffer, int bufferSize );









