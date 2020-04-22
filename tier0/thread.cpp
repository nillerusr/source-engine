//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Thread management routines
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"

#include "tier0/valve_off.h"
#ifdef _WIN32
#define WIN_32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <tlhelp32.h>
#endif

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

unsigned long Plat_GetCurrentThreadID()
{
	return ThreadGetCurrentId();
}


#if defined(_WIN32) && defined(_M_IX86)

static CThreadMutex s_BreakpointStateMutex;

struct X86HardwareBreakpointState_t
{
	const void *pAddress[4];
	char nWatchBytes[4];
	bool bBreakOnRead[4];
};
static X86HardwareBreakpointState_t s_BreakpointState = { {0,0,0,0}, {0,0,0,0}, {false,false,false,false} };

static void X86ApplyBreakpointsToThread( DWORD dwThreadId )
{
	CONTEXT ctx;
	ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	X86HardwareBreakpointState_t *pState = &s_BreakpointState;
	ctx.Dr0 = (DWORD) pState->pAddress[0];
	ctx.Dr1 = (DWORD) pState->pAddress[1];
	ctx.Dr2 = (DWORD) pState->pAddress[2];
	ctx.Dr3 = (DWORD) pState->pAddress[3];
	ctx.Dr7 = (DWORD) 0;
	for ( int i = 0; i < 4; ++i )
	{
		if ( pState->pAddress[i] && pState->nWatchBytes[i] )
		{
			ctx.Dr7 |= 1 << (i*2);
			if ( pState->bBreakOnRead[i] )
				ctx.Dr7 |= 3 << (16 + i*4);
			else
				ctx.Dr7 |= 1 << (16 + i*4);
			switch ( pState->nWatchBytes[i] )
			{
			case 1: ctx.Dr7 |= 0<<(18 + i*4); break;
			case 2: ctx.Dr7 |= 1<<(18 + i*4); break;
			case 4: ctx.Dr7 |= 3<<(18 + i*4); break;
			case 8: ctx.Dr7 |= 2<<(18 + i*4); break;
			}
		}
	}

	// Freeze this thread, adjust its breakpoint state
	HANDLE hThread = OpenThread( THREAD_SUSPEND_RESUME | THREAD_SET_CONTEXT, FALSE, dwThreadId );
	if ( hThread != INVALID_HANDLE_VALUE )
	{
		if ( SuspendThread( hThread ) != -1 )
		{
			SetThreadContext( hThread, &ctx );
			ResumeThread( hThread );
		}
		CloseHandle( hThread );
	}
}

static DWORD STDCALL ThreadProcX86SetDataBreakpoints( LPVOID pvParam )
{
	if ( pvParam )
	{
		X86ApplyBreakpointsToThread( *(unsigned long*)pvParam );
		return 0;
	}

	// This function races against creation and destruction of new threads. Try to execute as quickly as possible.
	SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );

	DWORD dwProcId = GetCurrentProcessId();
	DWORD dwThisThreadId = GetCurrentThreadId();
	HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
	if ( hSnap != INVALID_HANDLE_VALUE )
	{
		THREADENTRY32 threadEntry;
		// Thread32First/Thread32Next may adjust dwSize to be smaller. It's weird. Read the doc.
		const DWORD dwMinSize = (char*)(&threadEntry.th32OwnerProcessID + 1) - (char*)&threadEntry;

		threadEntry.dwSize = sizeof( THREADENTRY32 );
		BOOL bContinue = Thread32First( hSnap, &threadEntry );
		while ( bContinue )
		{
			if ( threadEntry.dwSize >= dwMinSize )
			{
				if ( threadEntry.th32OwnerProcessID == dwProcId && threadEntry.th32ThreadID != dwThisThreadId )
				{
					X86ApplyBreakpointsToThread( threadEntry.th32ThreadID );
				}
			}

			threadEntry.dwSize = sizeof( THREADENTRY32 );			
			bContinue = Thread32Next( hSnap, &threadEntry );
		}

		CloseHandle( hSnap );
	}
	return 0;
}

void Plat_SetHardwareDataBreakpoint( const void *pAddress, int nWatchBytes, bool bBreakOnRead )
{
	Assert( pAddress );
	Assert( nWatchBytes == 0 || nWatchBytes == 1 || nWatchBytes == 2 || nWatchBytes == 4 || nWatchBytes == 8 );

	s_BreakpointStateMutex.Lock();

	if ( nWatchBytes == 0 )
	{
		for ( int i = 0; i < 4; ++i )
		{
			if ( pAddress == s_BreakpointState.pAddress[i] )
			{
				for ( ; i < 3; ++i )
				{
					s_BreakpointState.pAddress[i] = s_BreakpointState.pAddress[i+1];
					s_BreakpointState.nWatchBytes[i] = s_BreakpointState.nWatchBytes[i+1];
					s_BreakpointState.bBreakOnRead[i] = s_BreakpointState.bBreakOnRead[i+1];
				}
				s_BreakpointState.pAddress[3] = NULL;
				s_BreakpointState.nWatchBytes[3] = 0;
				s_BreakpointState.bBreakOnRead[3] = false;
				break;
			}
		}
	}
	else
	{
		// Replace first null entry or first existing entry at this address, or bump all entries down
		for ( int i = 0; i < 4; ++i )
		{
			if ( s_BreakpointState.pAddress[i] && s_BreakpointState.pAddress[i] != pAddress && i < 3 )
				continue;
		
			// Last iteration.

			if ( s_BreakpointState.pAddress[i] && s_BreakpointState.pAddress[i] != pAddress )
			{
				// Full up. Shift table down, drop least recently set
				for ( int j = 0; j < 3; ++j )
				{
					s_BreakpointState.pAddress[j] = s_BreakpointState.pAddress[j+1];
					s_BreakpointState.nWatchBytes[j] = s_BreakpointState.nWatchBytes[j+1];
					s_BreakpointState.bBreakOnRead[j] = s_BreakpointState.bBreakOnRead[j+1];
				}
			}
			s_BreakpointState.pAddress[i] = pAddress;
			s_BreakpointState.nWatchBytes[i] = nWatchBytes;
			s_BreakpointState.bBreakOnRead[i] = bBreakOnRead;
			break;
		}
	}
	

	HANDLE hWorkThread = CreateThread( NULL, NULL, &ThreadProcX86SetDataBreakpoints, NULL, 0, NULL );
	if ( hWorkThread != INVALID_HANDLE_VALUE )
	{
		WaitForSingleObject( hWorkThread, INFINITE );
		CloseHandle( hWorkThread );
	}

	s_BreakpointStateMutex.Unlock();
}

void Plat_ApplyHardwareDataBreakpointsToNewThread( unsigned long dwThreadID )
{
	s_BreakpointStateMutex.Lock();
	if ( dwThreadID != GetCurrentThreadId() )
	{
		X86ApplyBreakpointsToThread( dwThreadID );
	}
	else
	{
		HANDLE hWorkThread = CreateThread( NULL, NULL, &ThreadProcX86SetDataBreakpoints, &dwThreadID, 0, NULL );
		if ( hWorkThread != INVALID_HANDLE_VALUE )
		{
			WaitForSingleObject( hWorkThread, INFINITE );
			CloseHandle( hWorkThread );
		}

	}
	s_BreakpointStateMutex.Unlock();
}

#else

void Plat_SetHardwareDataBreakpoint( const void *pAddress, int nWatchBytes, bool bBreakOnRead )
{
	// no impl on this platform yet
}

void Plat_ApplyHardwareDataBreakpointsToNewThread( unsigned long dwThreadID )
{
	// no impl on this platform yet
}

#endif
