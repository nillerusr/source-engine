//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SYS_UTILS.H
//
//	System Utilities.
//=====================================================================================//
#pragma once

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define MAX_SYSTOKENCHARS	1024

extern void		Sys_NormalizePath( CHAR *path, bool forceToLower );
extern CHAR*	Sys_PeekToken( CHAR *dataptr, BOOL bAllowLineBreaks );
extern CHAR		*Sys_GetToken( CHAR **dataptr, BOOL crossline, int *numlines );
extern CHAR		*Sys_SkipWhitespace( CHAR *data, BOOL *hasNewLines, int *numlines );
extern void		Sys_SkipBracedSection( CHAR **dataptr, int *numlines );
extern void		Sys_SkipRestOfLine( CHAR **dataptr, int *numlines );
extern void		Sys_StripQuotesFromToken( CHAR *pToken );
extern BOOL		Sys_SetRegistryString( const CHAR *key, const CHAR *value );
extern BOOL		Sys_GetRegistryString( const CHAR *key, CHAR *value, const CHAR *defValue, int valueLen );
extern BOOL		Sys_SetRegistryInteger( const CHAR *key, int value );
extern BOOL		Sys_GetRegistryInteger( const CHAR *key, int defValue, int &value );









