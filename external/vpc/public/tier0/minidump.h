//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MINIDUMP_H
#define MINIDUMP_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"

// writes out a minidump of the current stack trace with a unique filename
PLATFORM_INTERFACE void WriteMiniDump();

typedef void (*FnWMain)( int , tchar *[] );

#ifdef IS_WINDOWS_PC

// calls the passed in function pointer and catches any exceptions/crashes thrown by it, and writes a minidump
// use from wmain() to protect the whole program

PLATFORM_INTERFACE void CatchAndWriteMiniDump( FnWMain pfn, int argc, tchar *argv[] );

// The ExceptionInfo_t struct is a typeless data struct
// which is OS-dependent and should never be used by external code.
// Just pass it back into MinidumpSetUnhandledExceptionFunction 
struct ExceptionInfo_t;


// Replaces the current function pointer with the one passed in.
// Returns the previously-set function.
// The function is called internally by WriteMiniDump() and CatchAndWriteMiniDump()
// The default is the built-in function that uses DbgHlp.dll's MiniDumpWriteDump function
typedef void (*FnMiniDump)( unsigned int uStructuredExceptionCode, ExceptionInfo_t * pExceptionInfo );
PLATFORM_INTERFACE FnMiniDump SetMiniDumpFunction( FnMiniDump pfn );

// Use this to write a minidump explicitly.
// Some of the tools choose to catch the minidump themselves instead of using CatchAndWriteMinidump
// so they can show their own dialog.
//
// ptchMinidumpFileNameBuffer if not-NULL should be a writable tchar buffer of length at
// least _MAX_PATH and on return will contain the name of the minidump file written.
// If ptchMinidumpFileNameBuffer is NULL the name of the minidump file written will not
// be available after the function returns.
//


// NOTE: Matches windows.h
enum MinidumpType_t 
{
	MINIDUMP_Normal                           = 0x00000000,
	MINIDUMP_WithDataSegs                     = 0x00000001,
	MINIDUMP_WithFullMemory                   = 0x00000002,
	MINIDUMP_WithHandleData                   = 0x00000004,
	MINIDUMP_FilterMemory                     = 0x00000008,
	MINIDUMP_ScanMemory                       = 0x00000010,
	MINIDUMP_WithUnloadedModules              = 0x00000020,
	MINIDUMP_WithIndirectlyReferencedMemory   = 0x00000040,
	MINIDUMP_FilterModulePaths                = 0x00000080,
	MINIDUMP_WithProcessThreadData            = 0x00000100,
	MINIDUMP_WithPrivateReadWriteMemory       = 0x00000200,
	MINIDUMP_WithoutOptionalData              = 0x00000400,
	MINIDUMP_WithFullMemoryInfo               = 0x00000800,
	MINIDUMP_WithThreadInfo                   = 0x00001000,
	MINIDUMP_WithCodeSegs                     = 0x00002000 
};

PLATFORM_INTERFACE bool WriteMiniDumpUsingExceptionInfo( 
	unsigned int uStructuredExceptionCode,
	ExceptionInfo_t *pExceptionInfo, 
	uint32 nMinidumpTypeFlags,	// OR-ed together MinidumpType_t flags
	tchar *ptchMinidumpFileNameBuffer = NULL
	);

PLATFORM_INTERFACE void MinidumpSetUnhandledExceptionFunction( FnMiniDump pfn );

#endif

#endif // MINIDUMP_H
