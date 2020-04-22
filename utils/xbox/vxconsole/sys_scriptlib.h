//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SYS_SCRIPTLIB.H
//
//	System Utilities.
//=====================================================================================//
#pragma once

#include "vxconsole.h"

#define	MAXTOKEN	128

extern void 	Sys_LoadScriptFile(const char* filename);
extern void		Sys_SetScriptData(const char* data, int length);
extern void 	Sys_FreeScriptFile(void);
extern char*	Sys_GetToken(bool crossline);
extern char*	Sys_GetQuotedToken(bool crossline);
extern void 	Sys_UnGetToken(void);
extern bool 	Sys_TokenAvailable(void);
extern void 	Sys_SaveParser(void);
extern void 	Sys_RestoreParser(void);
extern void		Sys_ResetParser(void);
extern void		Sys_SkipRestOfLine(void);
extern bool		Sys_EndOfScript(void);
extern char*	Sys_GetRawToken(void);
extern void		Sys_StripQuotesFromToken( char *pToken );

extern char		g_sys_token[MAXTOKEN];
extern char*	g_sys_scriptbuffer;
extern char*	g_sys_scriptptr;
extern char*	g_sys_scriptendptr;
extern int		g_sys_scriptsize;
extern int		g_sys_scriptline;
extern bool		g_sys_endofscript;
