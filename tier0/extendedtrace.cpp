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
// ExtendedTrace.cpp
//

// Include StdAfx.h, if you're using precompiled 
// header through StdAfx.h
//#include "stdafx.h"

#if defined(_DEBUG) && defined(WIN32)

#include "tier0/valve_off.h"
#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include <ImageHlp.h>
#include "tier0/valve_on.h"
#include "ExtendedTrace.h"

#define BUFFERSIZE   0x200


extern void OutputDebugStringFormat( const char *pMsg, ... );


// Unicode safe char* -> TCHAR* conversion
void PCSTR2LPTSTR( PCSTR lpszIn, LPTSTR lpszOut )
{
#if defined(UNICODE)||defined(_UNICODE)
   ULONG index = 0; 
   PCSTR lpAct = lpszIn;
   
	for( ; ; lpAct++ )
	{
		lpszOut[index++] = (TCHAR)(*lpAct);
		if ( *lpAct == 0 )
			break;
	} 
#else
   // This is trivial :)
	strcpy( lpszOut, lpszIn );
#endif
}

// Let's figure out the path for the symbol files
// Search path= ".;%_NT_SYMBOL_PATH%;%_NT_ALTERNATE_SYMBOL_PATH%;%SYSTEMROOT%;%SYSTEMROOT%\System32;" + lpszIniPath
// Note: There is no size check for lpszSymbolPath!
void InitSymbolPath( PSTR lpszSymbolPath, PCSTR lpszIniPath )
{
	CHAR lpszPath[BUFFERSIZE];

   // Creating the default path
   // ".;%_NT_SYMBOL_PATH%;%_NT_ALTERNATE_SYMBOL_PATH%;%SYSTEMROOT%;%SYSTEMROOT%\System32;"
	strcpy( lpszSymbolPath, "." );

	// environment variable _NT_SYMBOL_PATH
	if ( GetEnvironmentVariableA( "_NT_SYMBOL_PATH", lpszPath, BUFFERSIZE ) )
	{
	   strcat( lpszSymbolPath, ";" );
		strcat( lpszSymbolPath, lpszPath );
	}

	// environment variable _NT_ALTERNATE_SYMBOL_PATH
	if ( GetEnvironmentVariableA( "_NT_ALTERNATE_SYMBOL_PATH", lpszPath, BUFFERSIZE ) )
	{
	   strcat( lpszSymbolPath, ";" );
		strcat( lpszSymbolPath, lpszPath );
	}

	// environment variable SYSTEMROOT
	if ( GetEnvironmentVariableA( "SYSTEMROOT", lpszPath, BUFFERSIZE ) )
	{
	   strcat( lpszSymbolPath, ";" );
		strcat( lpszSymbolPath, lpszPath );
		strcat( lpszSymbolPath, ";" );

		// SYSTEMROOT\System32
		strcat( lpszSymbolPath, lpszPath );
		strcat( lpszSymbolPath, "\\System32" );
	}

   // Add user defined path
	if ( lpszIniPath != NULL )
		if ( lpszIniPath[0] != '\0' )
		{
		   strcat( lpszSymbolPath, ";" );
			strcat( lpszSymbolPath, lpszIniPath );
		}
}

// Uninitialize the loaded symbol files
BOOL UninitSymInfo()
{
	return SymCleanup( GetCurrentProcess() );
}

// Initializes the symbol files
BOOL InitSymInfo( PCSTR lpszInitialSymbolPath )
{
	CHAR     lpszSymbolPath[BUFFERSIZE];
   DWORD    symOptions = SymGetOptions();

	symOptions |= SYMOPT_LOAD_LINES; 
	symOptions &= ~SYMOPT_UNDNAME;
	SymSetOptions( symOptions );

   // Get the search path for the symbol files
	InitSymbolPath( lpszSymbolPath, lpszInitialSymbolPath );

	return SymInitialize( GetCurrentProcess(), lpszSymbolPath, TRUE);
}

// Get the module name from a given address
BOOL GetModuleNameFromAddress( UINT address, LPTSTR lpszModule )
{
	BOOL              ret = FALSE;
	IMAGEHLP_MODULE   moduleInfo;

	::ZeroMemory( &moduleInfo, sizeof(moduleInfo) );
	moduleInfo.SizeOfStruct = sizeof(moduleInfo);

	if ( SymGetModuleInfo( GetCurrentProcess(), (DWORD)address, &moduleInfo ) )
	{
	   // Got it!
		PCSTR2LPTSTR( moduleInfo.ModuleName, lpszModule );
		ret = TRUE;
	}
	else
	   // Not found :(
		_tcscpy( lpszModule, _T("?") );
	
	return ret;
}

// Get function prototype and parameter info from ip address and stack address
BOOL GetFunctionInfoFromAddresses( ULONG fnAddress, ULONG stackAddress, LPTSTR lpszSymbol )
{
	BOOL              ret = FALSE;
	DWORD             dwDisp = 0;
	DWORD             dwSymSize = 10000;
   TCHAR             lpszUnDSymbol[BUFFERSIZE]=_T("?");
	CHAR              lpszNonUnicodeUnDSymbol[BUFFERSIZE]="?";
	LPTSTR            lpszParamSep = NULL;
	LPCTSTR           lpszParsed = lpszUnDSymbol;
	PIMAGEHLP_SYMBOL  pSym = (PIMAGEHLP_SYMBOL)GlobalAlloc( GMEM_FIXED, dwSymSize );

	::ZeroMemory( pSym, dwSymSize );
	pSym->SizeOfStruct = dwSymSize;
	pSym->MaxNameLength = dwSymSize - sizeof(IMAGEHLP_SYMBOL);

   // Set the default to unknown
	_tcscpy( lpszSymbol, _T("?") );

	// Get symbol info for IP
	if ( SymGetSymFromAddr( GetCurrentProcess(), (ULONG)fnAddress, &dwDisp, pSym ) )
	{
	   // Make the symbol readable for humans
		UnDecorateSymbolName( pSym->Name, lpszNonUnicodeUnDSymbol, BUFFERSIZE, 
			UNDNAME_COMPLETE | 
			UNDNAME_NO_THISTYPE |
			UNDNAME_NO_SPECIAL_SYMS |
			UNDNAME_NO_MEMBER_TYPE |
			UNDNAME_NO_MS_KEYWORDS |
			UNDNAME_NO_ACCESS_SPECIFIERS );

      // Symbol information is ANSI string
		PCSTR2LPTSTR( lpszNonUnicodeUnDSymbol, lpszUnDSymbol );

      // I am just smarter than the symbol file :)
		if ( _tcscmp(lpszUnDSymbol, _T("_WinMain@16")) == 0 )
			_tcscpy(lpszUnDSymbol, _T("WinMain(HINSTANCE,HINSTANCE,LPCTSTR,int)"));
		else
		if ( _tcscmp(lpszUnDSymbol, _T("_main")) == 0 )
			_tcscpy(lpszUnDSymbol, _T("main(int,TCHAR * *)"));
		else
		if ( _tcscmp(lpszUnDSymbol, _T("_mainCRTStartup")) == 0 )
			_tcscpy(lpszUnDSymbol, _T("mainCRTStartup()"));
		else
		if ( _tcscmp(lpszUnDSymbol, _T("_wmain")) == 0 )
			_tcscpy(lpszUnDSymbol, _T("wmain(int,TCHAR * *,TCHAR * *)"));
		else
		if ( _tcscmp(lpszUnDSymbol, _T("_wmainCRTStartup")) == 0 )
			_tcscpy(lpszUnDSymbol, _T("wmainCRTStartup()"));

		lpszSymbol[0] = _T('\0');

      // Let's go through the stack, and modify the function prototype, and insert the actual
      // parameter values from the stack
		if ( _tcsstr( lpszUnDSymbol, _T("(void)") ) == NULL && _tcsstr( lpszUnDSymbol, _T("()") ) == NULL)
		{
			ULONG index = 0;
			for( ; ; index++ )
			{
				lpszParamSep = _tcschr( lpszParsed, _T(',') );
				if ( lpszParamSep == NULL )
					break;

				*lpszParamSep = _T('\0');

				_tcscat( lpszSymbol, lpszParsed );
				_stprintf( lpszSymbol + _tcslen(lpszSymbol), _T("=0x%08X,"), *((ULONG*)(stackAddress) + 2 + index) );

				lpszParsed = lpszParamSep + 1;
			}

			lpszParamSep = _tcschr( lpszParsed, _T(')') );
			if ( lpszParamSep != NULL )
			{
				*lpszParamSep = _T('\0');

				_tcscat( lpszSymbol, lpszParsed );
				_stprintf( lpszSymbol + _tcslen(lpszSymbol), _T("=0x%08X)"), *((ULONG*)(stackAddress) + 2 + index) );

				lpszParsed = lpszParamSep + 1;
			}
		}

		_tcscat( lpszSymbol, lpszParsed );
   
		ret = TRUE;
	}

	GlobalFree( pSym );

	return ret;
}

// Get source file name and line number from IP address
// The output format is: "sourcefile(linenumber)" or
//                       "modulename!address" or
//                       "address"
BOOL GetSourceInfoFromAddress( UINT address, LPTSTR lpszSourceInfo )
{
	BOOL           ret = FALSE;
	IMAGEHLP_LINE  lineInfo;
	DWORD          dwDisp;
	TCHAR          lpszFileName[BUFFERSIZE] = _T("");
	TCHAR          lpModuleInfo[BUFFERSIZE] = _T("");

	_tcscpy( lpszSourceInfo, _T("?(?)") );

	::ZeroMemory( &lineInfo, sizeof( lineInfo ) );
	lineInfo.SizeOfStruct = sizeof( lineInfo );

	if ( SymGetLineFromAddr( GetCurrentProcess(), address, &dwDisp, &lineInfo ) )
	{
	   // Got it. Let's use "sourcefile(linenumber)" format
		PCSTR2LPTSTR( lineInfo.FileName, lpszFileName );
		_stprintf( lpszSourceInfo, _T("%s(%d)"), lpszFileName, lineInfo.LineNumber );
		ret = TRUE;
	}
	else
	{
      // There is no source file information. :(
      // Let's use the "modulename!address" format
	  	GetModuleNameFromAddress( address, lpModuleInfo );

		if ( lpModuleInfo[0] == _T('?') || lpModuleInfo[0] == _T('\0'))
		   // There is no modulename information. :((
         // Let's use the "address" format
			_stprintf( lpszSourceInfo, _T("?") );
		else
			_stprintf( lpszSourceInfo, _T("%s"), lpModuleInfo );

		ret = FALSE;
	}
	
	return ret;
}

// TRACE message with source link. 
// The format is: sourcefile(linenumber) : message
void SrcLinkTrace( LPCTSTR lpszMessage, LPCTSTR lpszFileName, ULONG nLineNumber )
{
	OutputDebugStringFormat( 
			_T("%s(%d) : %s"), 
			lpszFileName, 
			nLineNumber, 
			lpszMessage );
}

void StackTrace( HANDLE hThread, LPCTSTR lpszMessage )
{
	STACKFRAME     callStack;
	BOOL           bResult;
	CONTEXT        context;
	TCHAR          symInfo[BUFFERSIZE] = _T("?");
	TCHAR          srcInfo[BUFFERSIZE] = _T("?");
	HANDLE         hProcess = GetCurrentProcess();

   // If it's not this thread, let's suspend it, and resume it at the end
	if ( hThread != GetCurrentThread() )
		if ( SuspendThread( hThread ) == -1 )
		{
		   // whaaat ?!
		   OutputDebugStringFormat( _T("Call stack info(thread=0x%X) failed.\n") );
			return;
		}

	::ZeroMemory( &context, sizeof(context) );
	context.ContextFlags = CONTEXT_FULL;

	if ( !GetThreadContext( hThread, &context ) )
	{
      OutputDebugStringFormat( _T("Call stack info(thread=0x%X) failed.\n") );
	   return;
	}
	
	::ZeroMemory( &callStack, sizeof(callStack) );
	callStack.AddrPC.Offset    = context.Eip;
	callStack.AddrStack.Offset = context.Esp;
	callStack.AddrFrame.Offset = context.Ebp;
	callStack.AddrPC.Mode      = AddrModeFlat;
	callStack.AddrStack.Mode   = AddrModeFlat;
	callStack.AddrFrame.Mode   = AddrModeFlat;

	for( ULONG index = 0; ; index++ ) 
	{
		bResult = StackWalk(
			IMAGE_FILE_MACHINE_I386,
			hProcess,
			hThread,
	      &callStack,
			NULL, 
			NULL,
			SymFunctionTableAccess,
			SymGetModuleBase,
			NULL);

		if ( index == 0 )
		   continue;

		if( !bResult || callStack.AddrFrame.Offset == 0 ) 
			break;
	
		GetFunctionInfoFromAddresses( callStack.AddrPC.Offset, callStack.AddrFrame.Offset, symInfo );
		GetSourceInfoFromAddress( callStack.AddrPC.Offset, srcInfo );

		OutputDebugStringFormat( _T("     %s : %s\n"), srcInfo, symInfo );
	}

	if ( hThread != GetCurrentThread() )
		ResumeThread( hThread );
}

void FunctionParameterInfo()
{
	STACKFRAME     callStack;
	BOOL           bResult = FALSE;
	CONTEXT        context;
	TCHAR          lpszFnInfo[BUFFERSIZE];
	HANDLE         hProcess = GetCurrentProcess();
	HANDLE         hThread = GetCurrentThread();

	::ZeroMemory( &context, sizeof(context) );
	context.ContextFlags = CONTEXT_FULL;

	if ( !GetThreadContext( hThread, &context ) )
	{
	   OutputDebugStringFormat( _T("Function info(thread=0x%X) failed.\n") );
		return;
	}
	
	::ZeroMemory( &callStack, sizeof(callStack) );
	callStack.AddrPC.Offset    = context.Eip;
	callStack.AddrStack.Offset = context.Esp;
	callStack.AddrFrame.Offset = context.Ebp;
	callStack.AddrPC.Mode      = AddrModeFlat;
	callStack.AddrStack.Mode   = AddrModeFlat;
	callStack.AddrFrame.Mode   = AddrModeFlat;

	for( ULONG index = 0; index < 2; index++ ) 
	{
		bResult = StackWalk(
			IMAGE_FILE_MACHINE_I386,
			hProcess,
			hThread,
			&callStack,
			NULL, 
			NULL,
			SymFunctionTableAccess,
			SymGetModuleBase,
			NULL);
	}

	if ( bResult && callStack.AddrFrame.Offset != 0) 
	{
	   GetFunctionInfoFromAddresses( callStack.AddrPC.Offset, callStack.AddrFrame.Offset, lpszFnInfo );
	   OutputDebugStringFormat( _T("Function info(thread=0x%X) : %s\n"), GetCurrentThreadId(), lpszFnInfo );
	}
	else
	   OutputDebugStringFormat( _T("Function info(thread=0x%X) failed.\n") );
}

#endif //_DEBUG && WIN32

