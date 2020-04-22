//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Windows Only, does a check against all other processes to see how many other instances
// of this process is running concurrently
//
//=============================================================================//

//*********************************************************************************************
// Process Check 
#include "cl_check_process.h"
#include "dbg.h"

#ifdef IS_WINDOWS_PC
#include <Windows.h>
#include <winternl.h>
#include <stdio.h>
#include <TlHelp32.h>
#include "strtools.h"
#include <Psapi.h>
#endif // IS_WINDOWS_PC

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef IS_WINDOWS_PC

#define HANDLE_QUERY_BUFFER_BLOCK_SIZE							( 1 * 1024 * 1024 )
#define SystemHandleInformation									( (SYSTEM_INFORMATION_CLASS)16 )
#define STATUS_INFO_LENGTH_MISMATCH								( (NTSTATUS)( 0xC0000004L ) )

typedef NTSTATUS (__stdcall *NtQuerySystemInformation1) 
(   
	IN ULONG SysInfoClass,   
	IN OUT PVOID SystemInformation,   
	IN ULONG SystemInformationLength,   
	OUT PULONG RetLen   
);  

typedef struct _HANDLE_INFORMATION
{
	DWORD ProcessId;
	BYTE ObjectType;
	BYTE Flags;
	USHORT Handle;
	PVOID KernelObject;
	ACCESS_MASK GrantedAccess;

} HANDLE_INFORMATION, *PHANDLE_INFORMATION;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
	ULONG HandleCount;
	HANDLE_INFORMATION HandleInfoArray[ 1 ];

} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

//
// Checks Process Count with CreateToolhelp32Snapshot
// 
int CheckOtherInstancesRunningWithSnapShot( const char *thisProcessNameShort )
{
	DWORD nLength;
	char otherProcessNameShort[ MAX_PATH ];

	nLength = MAX_PATH;

	int iSnapShotCount = 0;

	//
	HANDLE hSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( hSnapshot )
	{
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if ( Process32First( hSnapshot, &pe32 ) )
		{
			do
			{
				V_FileBase( pe32.szExeFile, otherProcessNameShort, MAX_PATH );
				if ( V_strcmp( thisProcessNameShort, otherProcessNameShort ) == 0 )
				{
//					DevMsg( "CreateToolhelp32Snapshot - Process Name [ %s ] - OtherName [ %s ] \n", thisProcessNameShort, pe32.szExeFile );
					//  We found an instance of this executable.
					iSnapShotCount++;
				}
			} while( Process32Next( hSnapshot, &pe32 ) );
		}
		CloseHandle(hSnapshot);
	}

	return iSnapShotCount;
}

//
// Checks Process Count with QueryFullProcessImageName and OpenProcess
// 
int CheckOtherInstancesWithEnumProcess( const char *thisProcessNameShort )
{
	NTSTATUS status;
	BOOL bStatus;
	DWORD nLength;
	DWORD nBufferSize;
	DWORD nLastProcess;
	DWORD i;
	HANDLE process;
	PSYSTEM_HANDLE_INFORMATION pHandleInfo = NULL;
	char otherProcessName[ MAX_PATH ];
	char otherProcessNameShort[ MAX_PATH ];
	
	HINSTANCE hInst = NULL;

	//  Start with a count of zero, since we will find ourselves too.
	int iProcessCount = 0;
	
	//  Get the path to the executable for this process.
	nLength = MAX_PATH;

	//  Query all of the handles in the system.  We have to do this in a loop, since we do
	//  not know how large of a buffer we need.
	nBufferSize = 0;
	
	// Load ntdll.dll so we can Query the system
	/* load the ntdll.dll */
	NtQuerySystemInformation1 NtQuerySystemInformation;

	//PVOID Info;
	HMODULE hModule = LoadLibrary( "ntdll.dll" );
	if (!hModule)   
	{
		iProcessCount = CHECK_PROCESS_UNSUPPORTED;
		goto Cleanup;
	}

	while ( TRUE )
	{
		//  Increase the buffer size and try the query.
		if ( pHandleInfo != NULL )
		{
			free( pHandleInfo );
		}
	
		nBufferSize += HANDLE_QUERY_BUFFER_BLOCK_SIZE;
	
		pHandleInfo = (PSYSTEM_HANDLE_INFORMATION)malloc( nBufferSize );
	
		if ( pHandleInfo == NULL )
		{
			iProcessCount = CHECK_PROCESS_UNSUPPORTED;
			goto Cleanup;
		}

		//  Query the handles in the system.
		NtQuerySystemInformation = (NtQuerySystemInformation1)GetProcAddress(hModule, "NtQuerySystemInformation");
		if ( NtQuerySystemInformation == NULL ) 
		{
			iProcessCount = CHECK_PROCESS_UNSUPPORTED;
			goto Cleanup;
		}
	
		status = NtQuerySystemInformation( SystemHandleInformation,	pHandleInfo, nBufferSize, NULL );
	
		//  If our buffer was too small, try again.
		if ( status == STATUS_INFO_LENGTH_MISMATCH )
		{
			continue;
		}
	
		//  If the query failed, return the error.
		if ( !NT_SUCCESS( status ) )
		{
			iProcessCount = CHECK_PROCESS_UNSUPPORTED;
			goto Cleanup;
		}
	
		break;
	}
	
	//
	//  Walk all of the entries looking for process IDs we have not processed yet.
	//  Note that this code assumes that handles will be grouped by process, which is
	//  what Windows does.  If that assumption ever turns out to be false, this code
	//  will have to be altered to keep a list of process IDs already seen.
	//

	// Check for the presence of GetModuleFileNameEx
	hInst = LoadLibrary( "Psapi.dll" );
	if ( !hInst )
		return CHECK_PROCESS_UNSUPPORTED;

	typedef DWORD (WINAPI *GetProcessImageFileNameFn)(HANDLE, LPTSTR, DWORD);
	GetProcessImageFileNameFn fn = (GetProcessImageFileNameFn)GetProcAddress( hInst,
#ifdef  UNICODE
		"GetProcessImageFileNameW");
#else
		"GetProcessImageFileNameA");
#endif

	if ( !fn )
		return CHECK_PROCESS_UNSUPPORTED;

	nLastProcess = 0;
	
	for ( i = 0; i < pHandleInfo->HandleCount; i++ )
	{
		if ( pHandleInfo->HandleInfoArray[ i ].ProcessId != nLastProcess )
		{
			//nLastProcess = pHandleInfo->HandleInfoArray[ i ].ProcessId;
			nLastProcess = pHandleInfo->HandleInfoArray[ i ].ProcessId;
	
			//
			//  Try to open a handle to this process.  Note that we may not have
			//  access to all processes, so we ignore errors.
			//
			process = OpenProcess( PROCESS_QUERY_INFORMATION,
				FALSE,
				nLastProcess );
	
			if ( process != NULL )
			{
				//  Query the name of the executable for the process we opened.  If the query
				//  fails, we ignore this process.
				nLength = MAX_PATH;
	
				bStatus = fn( process, otherProcessName, nLength );
				
				if ( bStatus )
				{
					//
					//  We have the process name.  See if it is the same name as our process.
					//
					V_FileBase( otherProcessName, otherProcessNameShort, MAX_PATH );
	
					if ( V_strcmp( thisProcessNameShort, otherProcessNameShort ) == 0 )
					{
						//  We found an instance of this executable.
//						DevMsg( "EnumProcess - Process Name [ %s ] - OtherName [ %s ] \n", thisProcessNameShort, otherProcessName );
						iProcessCount++;
					}
				}
	
				CloseHandle( process );
			}
		}
	}
	
Cleanup:

	//  Free allocated resources.
	if ( pHandleInfo != NULL )
	{
		free( pHandleInfo );
	}

	if ( hInst != NULL )
	{
		FreeLibrary( hInst );
	}

	if ( hModule != NULL )
	{
		FreeLibrary( hModule );
	}

	return iProcessCount;
}
#endif // IS_WINDOWS_PC

int CheckOtherInstancesRunning( void )
{
#ifdef IS_WINDOWS_PC

	BOOL bStatus = 0;
	DWORD nLength = MAX_PATH;
	char thisProcessName[ MAX_PATH ];
	char thisProcessNameShort[ MAX_PATH ];

	// Load the pspapi to get our current process' name
	HINSTANCE hInst = LoadLibrary( "Psapi.dll" );
	if ( hInst )
	{
		typedef DWORD (WINAPI *GetProcessImageFileNameFn)(HANDLE, LPTSTR, DWORD);
		GetProcessImageFileNameFn fn = (GetProcessImageFileNameFn)GetProcAddress( hInst,
#ifdef  UNICODE
			"GetProcessImageFileNameW");
#else
			"GetProcessImageFileNameA");
#endif
		if ( fn )
		{
			bStatus = fn( GetCurrentProcess(), thisProcessName, nLength );
		}

		FreeLibrary( hInst );
	}

	if ( !bStatus )
	{
		return CHECK_PROCESS_UNSUPPORTED;
	}

	V_FileBase( thisProcessName, thisProcessNameShort, MAX_PATH );

//	Msg( "Checking Other Instances Running : ProcessShortName [ %s - %s ] \n", thisProcessName, thisProcessNameShort );

	int iSnapShotCount = CheckOtherInstancesRunningWithSnapShot( thisProcessNameShort );
	if ( iSnapShotCount > 1 )
	{
		return iSnapShotCount;
	}

	int iEnumCount = CheckOtherInstancesWithEnumProcess( thisProcessNameShort );
	return iEnumCount > iSnapShotCount ? iEnumCount : iSnapShotCount;
#endif // IS_WINDOWS_PC

	return CHECK_PROCESS_UNSUPPORTED;		// -1 UNSUPPORTED
}
