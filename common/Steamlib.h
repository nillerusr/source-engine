//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// Steam header file includes all definitions for Grid.lib
#ifndef INCLUDED_STEAMLIBAPI_H
#define INCLUDED_STEAMLIBAPI_H

#ifndef _WIN32	// Linux from here for temporary non-Steam server
#include "Steamlib_null.h"

#else	// Win32 from here

#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern int STEAM_Startup(void);
extern int STEAM_Mount(const char *szMountPath);
extern int STEAM_Unmount(void);
extern void STEAM_Shutdown(void);

extern FILE *STEAM_fopen(const char *filename, const char *options);
extern int STEAM_fclose(FILE *file);
extern unsigned int STEAM_fread(void *buffer, unsigned int rdsize, unsigned int count,FILE *file);
extern int STEAM_fgetc(FILE *file);
extern unsigned int STEAM_fwrite(void *buffer, unsigned int rdsize, unsigned int count,FILE *file);
extern int STEAM_fprintf( FILE *fp, const char *format, ... );
extern int STEAM_vfprintf( FILE *fp, const char *format, va_list argptr);
extern int STEAM_fseek(FILE *file, long offset, int method);
extern long STEAM_ftell(FILE *file);
extern int STEAM_stat(const char *path, struct _stat *buf);
extern int STEAM_feof( FILE *stream );
extern int STEAM_ferror( FILE *stream );
extern void STEAM_clearerr( FILE *stream );
extern void STEAM_strerror( FILE *stream, char *p, int maxlen );
extern void STEAM_rewind( FILE *stream );
extern int STEAM_fflush( FILE *stream );
extern int STEAM_flushall( void );
extern unsigned int STEAM_FileSize( FILE *file );
extern void STEAM_setbuf( FILE *stream, char *buffer);
extern int STEAM_setvbuf( FILE *stream, char *buffer, int mode, size_t size);
extern char *STEAM_fgets(char *string, int n, FILE *stream);
extern int STEAM_fputc(int c, FILE *stream);
extern int STEAM_fputs(const char *string, FILE *stream);


extern FILE *STEAM_tmpfile(void);

typedef enum					// Filter elements returned by SteamFind{First,Next}
{
	STEAMFindLocalOnly,			// limit search to local filesystem
	STEAMFindRemoteOnly,			// limit search to remote repository
	STEAMFindAll					// do not limit search (duplicates allowed)
} STEAMFindFilter;

extern HANDLE STEAM_FindFirstFile(char *pszMatchName, STEAMFindFilter filter, WIN32_FIND_DATA *findInfo);
extern int STEAM_FindNextFile(HANDLE dir, WIN32_FIND_DATA *findInfo);
extern int STEAM_FindClose(HANDLE dir);

extern long STEAM_findfirst(char *pszMatchName, STEAMFindFilter filter, struct _finddata_t *fileinfo );
extern int STEAM_findnext(long dir, struct _finddata_t *fileinfo );
extern int STEAM_findclose(long dir);

extern HINSTANCE STEAM_LoadLibrary( const char *dllName );
extern void STEAM_GetLocalCopy( const char *fileName );

extern void STEAM_LogLevelLoadStarted( const char *name );
extern void STEAM_LogLevelLoadFinished( const char *name );

extern int STEAM_HintResourceNeed( const char *mapcycle, int forgetEverything );
extern int STEAM_PauseResourcePreloading(void);
extern int STEAM_ForgetAllResourceHints(void);
extern int STEAM_ResumeResourcePreloading(void);
extern int STEAM_BlockForResources( const char *hintlist );

extern void STEAM_UseDaemon(int enable);

extern unsigned int STEAM_FileSize( FILE *file );

extern void STEAM_TrackProgress(int enable);
extern STEAM_RegisterAppProgressCallback( void(*fpProgCallBack)(void), int freq );
extern STEAM_RegisterAppKeepAliveTicCallback( void(*fpKeepAliveTicCallBack)(char* scr_msg) );
extern void STEAM_UpdateProgress( void );
extern int STEAM_ProgressCounter(void);

extern void STEAM_GetInterfaceVersion( char *p, int maxlen );

extern int STEAM_FileIsAvailLocal( const char *file );

#ifdef __cplusplus
}
#endif

#endif	// ndef _WIN32

#endif /* #ifndef INCLUDED_STEAMLIBAPI_H */
