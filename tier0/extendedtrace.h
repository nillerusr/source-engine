//========= Copyright Valve Corporation, All rights reserved. ============//
//////////////////////////////////////////////////////////////////////////////////////
//
// Written by Zoltan Csizmadia, zoltan_csizmadia@yahoo.com
// For companies(Austin,TX): If you would like to get my resume, send an email.
//
// The source is free, but if you want to use it, mention my name and e-mail address
//
// History:
//    1.0      Initial version                  Zoltan Csizmadia
//
//////////////////////////////////////////////////////////////////////////////////////
//
// ExtendedTrace.h
//

#ifndef EXTENDEDTRACE_H_INCLUDED
#define EXTENDEDTRACE_H_INCLUDED

#if defined(_DEBUG) && defined(WIN32)


#pragma comment( lib, "imagehlp.lib" )

#if defined(_AFX) || defined(_AFXDLL)
#define TRACEF									         TRACE
#else
#define TRACEF									         OutputDebugStringFormat
void OutputDebugStringFormat( PRINTF_FORMAT_STRING LPCTSTR, ... );
#endif

#define EXTENDEDTRACEINITIALIZE( IniSymbolPath )	InitSymInfo( IniSymbolPath )
#define EXTENDEDTRACEUNINITIALIZE()			         UninitSymInfo()
#define SRCLINKTRACECUSTOM( Msg, File, Line)       SrcLinkTrace( Msg, File, Line )
#define SRCLINKTRACE( Msg )                        SrcLinkTrace( Msg, __FILE__, __LINE__ )
#define FNPARAMTRACE()							         FunctionParameterInfo()
#define STACKTRACEMSG( Msg )					         StackTrace( Msg )
#define STACKTRACE()							            StackTrace( GetCurrentThread(), _T("") )
#define THREADSTACKTRACEMSG( hThread, Msg )		   StackTrace( hThread, Msg )
#define THREADSTACKTRACE( hThread )				      StackTrace( hThread, _T("") )

BOOL InitSymInfo( PCSTR );
BOOL UninitSymInfo();
void SrcLinkTrace( LPCTSTR, LPCTSTR, ULONG );
void StackTrace( HANDLE, LPCTSTR );
void FunctionParameterInfo();

#else

#define EXTENDEDTRACEINITIALIZE( IniSymbolPath )   ((void)0)
#define EXTENDEDTRACEUNINITIALIZE()			         ((void)0)
#define TRACEF									            ((void)0)
#define SRCLINKTRACECUSTOM( Msg, File, Line)	      ((void)0)
#define SRCLINKTRACE( Msg )						      ((void)0)
#define FNPARAMTRACE()							         ((void)0)
#define STACKTRACEMSG( Msg )					         ((void)0)
#define STACKTRACE()						         	   ((void)0)
#define THREADSTACKTRACEMSG( hThread, Msg )		   ((void)0)
#define THREADSTACKTRACE( hThread )				      ((void)0)

#endif

#endif