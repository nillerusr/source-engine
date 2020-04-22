//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vmpi_service.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "vmpi.h"
#include "iphelpers.h"
#include "bitbuf.h"
#include "tier1/strtools.h"
#include "interface.h"
#include "ilaunchabledll.h"
#include "resource.h"
#include "consolewnd.h"
#include <io.h>
#include "utllinkedlist.h"
#include "service_helpers.h"
#include "vmpi_filesystem.h"
#include "service_conn_mgr.h"
#include "resource.h"
#include "perf_counters.h"
#include "tier0/icommandline.h"


// If we couldn't get into a job (maybe they weren't accepting more workers at the time),
// then we wait this long and retry the connection.
#define JOB_MEMORY_DURATION	60


char g_VersionString[64]; // From the IDS_VERSION_STRING string.
HKEY g_hVMPIServiceKey = NULL;	// HKML/Software/Valve/VMPI

double g_flLastKillProcessTime = 0;

char *g_pPassword = NULL;	// Set if this service is using a pw.
ISocket *g_pSocket = NULL;
int g_SocketPort = -1;		// Which port we were able to bind the port on.

char g_RunningProcess_ExeName[MAX_PATH] = {0};
char g_RunningProcess_MapName[MAX_PATH] = {0};
HANDLE g_hRunningProcess = NULL;
HANDLE g_hRunningThread = NULL;
DWORD g_dwRunningProcessId = 0;
IPerfTracker *g_pPerfTracker = NULL;	// Tracks CPU usage.

// When this is true, it will launch new processes invisibly.
bool g_bHideNewProcessWindows = true;

HINSTANCE g_hInstance = NULL;
int g_iBoundPort = -1;

bool g_bScreensaverMode = false;	// If this is true, then it'll act like the service is disabled while
									// a screensaver isn't running.

// If this is on, it runs the exes out of c:/hl2/bin instead of the network. If the exes are built in debug,
// this makes it possible to catch nasty crashes.
bool g_bSuperDebugMode = false;


bool g_bScreensaverRunning = false;	// Updated each frame to tell if the screensaver is running.


// GetTickCount() at the time the app was started.
DWORD g_AppStartTime = 0;

// GetTickCount() at the time the service ran a worker app.
DWORD g_CreateProcessTime = 0;


CIPAddr g_CurRespondAddr;
int g_CurJobID[4];
int g_CurJobPriority = -1;	// VMPI priority of the currently-running job.

// The directory we're running in.
char g_BaseAppPath[MAX_PATH];
char g_FileCachePath[MAX_PATH];	// [base app path]\vmpi_service_cache.


// Different modes this app can run in.
#define RUNMODE_INSTALL		0
#define RUNMODE_CONSOLE		1
#define RUNMODE_SERVICE		2

int g_RunMode = RUNMODE_CONSOLE;

bool g_bMinimized = false;				// true if they run with -minimized.
int g_iCurState = VMPI_SERVICE_STATE_IDLE;
char g_CurMasterName[512] = {0};



//////
// This block of variables is setup while we wait for the downloader to finish.
// When the downloading is complete, we launch the app using these variables.
//////

#ifdef _DEBUG
	#define MAX_DOWNLOADER_TIME_ALLOWED	300000	// If the downloader takes longer than this, kill it.
#else
	#define MAX_DOWNLOADER_TIME_ALLOWED	30	// If the downloader takes longer than this, kill it.
#endif

// If this is non-NULL, then there is NOT a VMPI worker app running currently.. the downloader
HANDLE g_Waiting_hProcess = NULL; 
float g_Waiting_StartTime = 0;
CUtlVector<char*> g_Waiting_Argv;
int g_Waiting_Priority = 0;
bool g_Waiting_bShowAppWindow = false;
bool g_Waiting_bPatching = 0;	// If this is nonzero, then we're downloading so we can apply a patch.


// Used to track the services browsers that have been talking to us lately.
#define SERVICES_BROWSER_TIMEOUT	10  // We remove a services browser from the list if we don't hear from it for this long.
class CServicesBrowserInfo
{
public:
	float	m_flLastPingTime;	// Last time they talked to us.
	CIPAddr	m_Addr;				// Their IP address.
};
CUtlVector<CServicesBrowserInfo> g_ServicesBrowsers;


void HandlePacket_KILL_PROCESS( const CIPAddr *ipFrom );
void KillRunningProcess( const char *pReason, bool bGoToIdle );
void LoadStateFromRegistry();
void SaveStateToRegistry();


void SetPassword( const char *pPassword )
{
	delete [] g_pPassword;
	if ( pPassword )
	{
		int len = V_strlen( pPassword ) + 1;
		g_pPassword = new char[len];
		V_strncpy( g_pPassword, pPassword, len );
	}
	else
	{
		g_pPassword = NULL;
	}
}


// ------------------------------------------------------------------------------------------ //
// This handles connection to clients.
// ------------------------------------------------------------------------------------------ //

class CVMPIServiceConnMgr : public CServiceConnMgr
{
public:

	virtual void	OnNewConnection( int id );
	virtual void	HandlePacket( const char *pData, int len );
	void			SendCurStateTo( int id );


public:

	void	AddConsoleOutput( const char *pMsg );

	void	SetAppState( int iState );
};


void CVMPIServiceConnMgr::AddConsoleOutput( const char *pMsg )
{
	// Tell clients of the new text string.
	CUtlVector<char> data;
	data.AddToTail( VMPI_SERVICE_UI_PROTOCOL_VERSION );
	data.AddToTail( VMPI_SERVICE_TO_UI_CONSOLE_TEXT );
	data.AddMultipleToTail( strlen( pMsg ) + 1, pMsg );
	SendPacket( -1, data.Base(), data.Count() );
}


void CVMPIServiceConnMgr::SetAppState( int iState )
{
	// Update our state and send it to the clients.
	g_iCurState = iState;
	SendCurStateTo( -1 );
}


void CVMPIServiceConnMgr::OnNewConnection( int id )
{
	// Send our current state to the new connection.
	Msg( "(debug) Made a new connection!\n" );
	SendCurStateTo( id );
}


void CVMPIServiceConnMgr::SendCurStateTo( int id )
{
	CUtlVector<char> data;
	data.AddToTail( VMPI_SERVICE_UI_PROTOCOL_VERSION );
	data.AddToTail( VMPI_SERVICE_TO_UI_STATE );
	data.AddMultipleToTail( sizeof( g_iCurState ), (char*)&g_iCurState );
	data.AddToTail( (char)g_bScreensaverMode );
	
	if ( g_pPassword )
		data.AddMultipleToTail( strlen( g_pPassword ) + 1, g_pPassword );
	else
		data.AddToTail( 0 );

	SendPacket( -1, data.Base(), data.Count() );
}


void CVMPIServiceConnMgr::HandlePacket( const char *pData, int len )
{
	switch( pData[0] )
	{
		case VMPI_KILL_PROCESS:
		{
			HandlePacket_KILL_PROCESS( NULL );
		}
		break;
		
		case VMPI_SERVICE_DISABLE:
		{
			KillRunningProcess( "Got a VMPI_SERVICE_DISABLE packet", true );
			SetAppState( VMPI_SERVICE_STATE_DISABLED );
			SaveStateToRegistry();
		}
		break;

		case VMPI_SERVICE_ENABLE:
		{
			if ( g_iCurState == VMPI_SERVICE_STATE_DISABLED )
			{
				SetAppState( VMPI_SERVICE_STATE_IDLE );
			}
			SaveStateToRegistry();
		}
		break;

		case VMPI_SERVICE_UPDATE_PASSWORD:
		{
			const char *pStr = pData + 1;
			SetPassword( pStr );
			
			// Send out the new state.
			SendCurStateTo( -1 );
		}
		break;

		case VMPI_SERVICE_SCREENSAVER_MODE:
		{
			g_bScreensaverMode = (pData[1] != 0);
			SendCurStateTo( -1 );
			SaveStateToRegistry();
		}
		break;

		case VMPI_SERVICE_EXIT:
		{
			Msg( "Got a VMPI_SERVICE_EXIT packet.\n ");
			ServiceHelpers_ExitEarly();
		}
		break;
	}
}

// This is allocated by the service thread and only used in there.
CVMPIServiceConnMgr *g_pConnMgr = NULL;


// ------------------------------------------------------------------------------------------ //
// Persistent state stuff.
// ------------------------------------------------------------------------------------------ //

void LoadStateFromRegistry()
{
	if ( g_hVMPIServiceKey )
	{
		DWORD val = 0;
		DWORD type = REG_DWORD;
		DWORD size = sizeof( val );

		if ( RegQueryValueEx( 
			g_hVMPIServiceKey,
			"ScreensaverMode",
			0,
			&type,
			(unsigned char*)&val,
			&size ) == ERROR_SUCCESS && 
			type == REG_DWORD && 
			size == sizeof( val ) )
		{
			g_bScreensaverMode = (val != 0);
		}

		if ( RegQueryValueEx( 
			g_hVMPIServiceKey,
			"Disabled",
			0,
			&type,
			(unsigned char*)&val,
			&size ) == ERROR_SUCCESS && 
			type == REG_DWORD && 
			size == sizeof( val ) &&
			val != 0 )
		{
			if ( g_pConnMgr )
				g_pConnMgr->SetAppState( VMPI_SERVICE_STATE_DISABLED );
		}
	}
}

void SaveStateToRegistry()
{
	if ( g_hVMPIServiceKey )
	{
		DWORD val;
		 
		val = g_bScreensaverMode;
		RegSetValueEx( 
			g_hVMPIServiceKey,
			"ScreensaverMode",
			0,
			REG_DWORD,
			(unsigned char*)&val,
			sizeof( val ) );

		val = (g_iCurState == VMPI_SERVICE_STATE_DISABLED);
		RegSetValueEx( 
			g_hVMPIServiceKey,
			"Disabled",
			0,
			REG_DWORD,
			(unsigned char*)&val,
			sizeof( val ) );
	}
}
 

// ------------------------------------------------------------------------------------------ //
// Helper functions.
// ------------------------------------------------------------------------------------------ //

char* FindArg( int argc, char **argv, const char *pArgName, char *pDefaultValue="" )
{
	for ( int i=0; i < argc; i++ )
	{
		if ( stricmp( argv[i], pArgName ) == 0 )
		{
			if ( (i+1) >= argc )
				return pDefaultValue;
			else
				return argv[i+1];
		}
	}
	return NULL;
}


SpewRetval_t MySpewOutputFunc( SpewType_t spewType, const char *pMsg )
{
	// Put the message in status.txt.
#ifdef VMPI_SERVICE_LOGS
	static FILE *fp = fopen( "c:\\vmpi_service.log", "wt" );
	if ( fp )
	{
		fprintf( fp, "%s", pMsg );
		fflush( fp );
	}
#endif


	// Print it to the console.
	if ( g_pConnMgr )
		g_pConnMgr->AddConsoleOutput( pMsg );
	
	// Output to the debug console.
	OutputDebugString( pMsg );

	if ( spewType == SPEW_ASSERT )
		return SPEW_DEBUGGER;
	else if( spewType == SPEW_ERROR )
		return SPEW_ABORT;
	else
		return SPEW_CONTINUE;
}


char* CopyString( const char *pStr )
{
	int len = V_strlen( pStr ) + 1;
	char *pRet = new char[len];
	V_strncpy( pRet, pStr, len );
	return pRet;
}

void AppendArg( CUtlVector<char*> &newArgv, const char *pIn )
{
	newArgv.AddToTail( CopyString( pIn ) );
}


void SendStartStatus( bool bStatus )
{	
	for ( int i=0; i < 3; i++ )
	{
		char data[4096];
		bf_write dataBuf( data, sizeof( data ) );
		dataBuf.WriteByte( VMPI_PROTOCOL_VERSION );
		dataBuf.WriteByte( VMPI_NOTIFY_START_STATUS );
		dataBuf.WriteBytes( g_CurJobID, sizeof( g_CurJobID ) );
		dataBuf.WriteByte( bStatus );
		g_pSocket->SendTo( &g_CurRespondAddr, data, dataBuf.GetNumBytesWritten() );
		
		Sleep( 50 );
	}
}


void SendEndStatus()
{
	for ( int i=0; i < 3; i++ )
	{
		char data[4096];
		bf_write dataBuf( data, sizeof( data ) );
		dataBuf.WriteByte( VMPI_PROTOCOL_VERSION );
		dataBuf.WriteByte( VMPI_NOTIFY_END_STATUS );
		dataBuf.WriteBytes( g_CurJobID, sizeof( g_CurJobID ) );
		g_pSocket->SendTo( &g_CurRespondAddr, data, dataBuf.GetNumBytesWritten() );
		
		Sleep( 50 );
	}
}


void KillRunningProcess( const char *pReason, bool bGoToIdle )
{
	// Kill the downloader if it's running.
	if ( g_Waiting_hProcess )
	{
		TerminateProcess( g_Waiting_hProcess, 1 );
		CloseHandle( g_Waiting_hProcess );
		g_Waiting_hProcess = NULL;
	}

	if ( !g_hRunningProcess )
		return;

	if ( pReason )
		Msg( pReason );

	SendEndStatus();
	TerminateProcess( g_hRunningProcess, 1 );
	g_RunningProcess_ExeName[0] = 0;
	g_RunningProcess_MapName[0] = 0;

	// Yep. Now we can start a new one.
	CloseHandle( g_hRunningThread );
	g_hRunningThread = NULL;
	
	CloseHandle( g_hRunningProcess );
	g_hRunningProcess = NULL;

	g_CurJobPriority = -1;

	if ( bGoToIdle )
		if ( g_pConnMgr )
			g_pConnMgr->SetAppState( VMPI_SERVICE_STATE_IDLE );
}


// ------------------------------------------------------------------------------------------ //
// Job memory stuff.
// ------------------------------------------------------------------------------------------ //

// CJobMemory is used to track which jobs we ran (or tried to run).
// We remember which jobs we did because Winsock likes to queue up the job packets on
// our socket, so if we don't remember which jobs we ran, we'd run the job a bunch of times.
class CJobMemory
{
public:
	int		m_ID[4];		// Random ID that comes from the server.
	float	m_Time;
};
															  
CUtlLinkedList<CJobMemory, int> g_JobMemories;


bool FindJobMemory( int id[4] )
{
	int iNext;
	for ( int i=g_JobMemories.Head(); i != g_JobMemories.InvalidIndex(); i=iNext )
	{
		iNext = g_JobMemories.Next( i );
		
		CJobMemory *pJob = &g_JobMemories[i];
		if ( memcmp( pJob->m_ID, id, sizeof( pJob->m_ID ) ) == 0 )
			return true;
	}
	return false;
}


void TimeoutJobIDs()
{
	double flCurTime = Plat_FloatTime();
	
	int iNext;
	for ( int i=g_JobMemories.Head(); i != g_JobMemories.InvalidIndex(); i=iNext )
	{
		iNext = g_JobMemories.Next( i );
		
		if ( (flCurTime - g_JobMemories[i].m_Time) > JOB_MEMORY_DURATION )
			g_JobMemories.Remove( i );
	}
}


void AddJobMemory( int id[4] )
{
	TimeoutJobIDs();
	
	CJobMemory job;
	memcpy( job.m_ID, id, sizeof( job.m_ID ) );
	job.m_Time = Plat_FloatTime();
	g_JobMemories.AddToTail( job );
}


bool CheckJobID( bf_read &buf, int jobID[4] )
{
	TimeoutJobIDs();
	
	jobID[0] = buf.ReadLong();
	jobID[1] = buf.ReadLong();
	jobID[2] = buf.ReadLong();
	jobID[3] = buf.ReadLong();
	if ( FindJobMemory( jobID ) || buf.IsOverflowed() )
	{
		return false;
	}
	
	return true;
}




// ------------------------------------------------------------------------------------------ //
// The main VMPI code.
// ------------------------------------------------------------------------------------------ //

void VMPI_Waiter_Term()
{
	KillRunningProcess( NULL, false );
	if ( g_pConnMgr )
	{
		g_pConnMgr->Term();
		delete g_pConnMgr;
		g_pConnMgr = NULL;
	}

	if ( g_pSocket )
	{
		g_pSocket->Release();
		g_pSocket = NULL;
	}
	
	g_pPerfTracker->Release();
	g_pPerfTracker = NULL;
}


bool VMPI_Waiter_Init()
{
	// Run as idle priority.
	HKEY hKey = NULL;
	RegCreateKey( HKEY_LOCAL_MACHINE, VMPI_SERVICE_KEY, &hKey );
	DWORD dwVal = 0;
	DWORD dummyType = REG_DWORD;
	DWORD dwValLen = sizeof( dwVal );
	if ( RegQueryValueEx( hKey, "LowPriority", NULL, &dummyType, (LPBYTE)&dwVal, &dwValLen ) == ERROR_SUCCESS )
	{
		if ( dwVal )
		{
			SetPriorityClass( GetCurrentProcess(), IDLE_PRIORITY_CLASS );
		}
	}
	else
	{
		RegSetValueEx( hKey, "LowPriority", 0, REG_DWORD, (unsigned char*)&dwVal, sizeof( dwVal ) );
	}

	g_pConnMgr = new CVMPIServiceConnMgr;

	if ( !g_pConnMgr->InitServer() )
		Msg( "ERROR INITIALIZING CONNMGR\n" );

	g_pSocket = CreateIPSocket();
	if ( !g_pSocket )
	{
		Msg( "Error creating a socket.\n" );
		return false;
	}
	
	// Bind to the first port we find in the range [VMPI_SERVICE_PORT, VMPI_LAST_SERVICE_PORT].
	int iTest;
	for ( iTest=VMPI_SERVICE_PORT; iTest <= VMPI_LAST_SERVICE_PORT; iTest++ )
	{
		g_SocketPort = iTest;
		if ( g_pSocket->BindToAny( iTest ) )
			break;
	}
	if ( iTest == VMPI_LAST_SERVICE_PORT )
	{	
		Msg( "Error binding a socket to port %d.\n", VMPI_SERVICE_PORT );
		VMPI_Waiter_Term();
		return false;
	}
	
	g_iBoundPort = iTest;
	g_pPerfTracker = CreatePerfTracker();
	return true;
}


void RunInDLL( const char *pFilename, CUtlVector<char*> &newArgv )
{
	if ( g_pConnMgr )						   
		g_pConnMgr->SetAppState( VMPI_SERVICE_STATE_BUSY );

	bool bSuccess = false;
	CSysModule *pModule = Sys_LoadModule( pFilename );
	if ( pModule )
	{
		CreateInterfaceFn fn = Sys_GetFactory( pModule );
		if ( fn )
		{
			ILaunchableDLL *pDLL = (ILaunchableDLL*)fn( LAUNCHABLE_DLL_INTERFACE_VERSION, NULL );
			if( pDLL )
			{
				// Do this here because the executables we would have launched usually would do it.
				CommandLine()->CreateCmdLine( newArgv.Count(), newArgv.Base() );
				pDLL->main( newArgv.Count(), newArgv.Base() );
				bSuccess = true;
				SpewOutputFunc( MySpewOutputFunc );
			}
		}

		Sys_UnloadModule( pModule );
	}
	
	if ( !bSuccess )
	{
		Msg( "Error running VRAD (or VVIS) out of DLL '%s'\n", pFilename );
	}

	if ( g_pConnMgr )
		g_pConnMgr->SetAppState( VMPI_SERVICE_STATE_IDLE );
}


void GetArgsFromBuffer( 
	bf_read &buf, 
	CUtlVector<char*> &newArgv, 
	bool *bShowAppWindow )
{
	int nArgs = buf.ReadWord();

	bool bSpewArgs = false;
	
	for ( int iArg=0; iArg < nArgs; iArg++ )
	{
		char argStr[512];
		buf.ReadString( argStr, sizeof( argStr ) );

		AppendArg( newArgv, argStr );
		if ( stricmp( argStr, "-mpi_verbose" ) == 0 )
			bSpewArgs = true;

		if ( stricmp( argStr, "-mpi_ShowAppWindow" ) == 0 )
			*bShowAppWindow = true;
	}

	if ( bSpewArgs )
	{
		Msg( "nArgs: %d\n", newArgv.Count() );
		for ( int i=0; i < newArgv.Count(); i++ )
			Msg( "Arg %d: %s\n", i, newArgv[i] );
	}
}


bool GetDLLFilename( CUtlVector<char*> &newArgv, char pDLLFilename[MAX_PATH] )
{
	char *argStr = newArgv[0];
	int argLen = strlen( argStr );
	if ( argLen <= 4 )
		return false;

	if ( Q_stricmp( &argStr[argLen-4], ".exe" ) != 0 )
		return false;
		
	char baseFilename[MAX_PATH];
	Q_strncpy( baseFilename, argStr, MAX_PATH );
	baseFilename[ min( MAX_PATH-1, argLen-4 ) ] = 0;
	
	// First try _dll.dll (src_main), then try .dll (rel).
	V_snprintf( pDLLFilename, MAX_PATH, "%s_dll.dll", baseFilename );
	if ( _access( pDLLFilename, 0 ) != 0 )
	{
		V_snprintf( pDLLFilename, MAX_PATH, "%s.dll", baseFilename );
	}

	return true;
}

void BuildCommandLineFromArgs( CUtlVector<char*> &newArgv, char *pOut, int outLen )
{
	pOut[0] = 0;

	for ( int i=0; i < newArgv.Count(); i++ )
	{
		char argStr[512];
		if ( strlen( newArgv[i] ) > 0 && newArgv[i][strlen(newArgv[i])-1] == '\\' )
			Q_snprintf( argStr, sizeof( argStr ), "\"%s\\\" ", newArgv[i] );
		else
			Q_snprintf( argStr, sizeof( argStr ), "\"%s\" ", newArgv[i] );

		Q_strncat( pOut, argStr, outLen, COPY_ALL_CHARACTERS );
	}
}

bool RunProcessFromArgs( CUtlVector<char*> &newArgv, bool bShowAppWindow, bool bCreateSuspended, const char *pWorkingDir, PROCESS_INFORMATION *pOut )
{
	char commandLine[2048];
	BuildCommandLineFromArgs( newArgv, commandLine, sizeof( commandLine ) );

	Msg( "Running '%s'\n", commandLine );

	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );

	memset( pOut, 0, sizeof( *pOut ) );

	DWORD dwFlags = 0;//IDLE_PRIORITY_CLASS;
	if ( bShowAppWindow )
		dwFlags |= CREATE_NEW_CONSOLE;
	else
		dwFlags |= CREATE_NO_WINDOW;

	if ( bCreateSuspended )
		dwFlags |= CREATE_SUSPENDED;

	UINT oldMode = SetErrorMode( SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS );

	BOOL bRet = CreateProcess( 
		NULL, 
		commandLine, 
		NULL,							// security
		NULL,
		TRUE,
		dwFlags | IDLE_PRIORITY_CLASS,	// flags
		NULL,							// environment
		pWorkingDir,	
		&si,
		pOut );
	
	SetErrorMode( oldMode );
	return (bRet != FALSE);
}


void RunProcessAtCommandLine( 
	CUtlVector<char*> &newArgv, 
	bool bShowAppWindow,
	bool bCreateSuspended,
	int iPriority )
{
	// current directory (use c:\\ because we don't want it to accidentally share
	// DLLs like vstdlib with us).	PROCESS_INFORMATION pi;
	PROCESS_INFORMATION pi;
	if ( RunProcessFromArgs( newArgv, bShowAppWindow, bCreateSuspended, g_FileCachePath, &pi ) )
	{
		if ( g_pConnMgr )
			g_pConnMgr->SetAppState( VMPI_SERVICE_STATE_BUSY );

		if ( newArgv.Count() > 0 && newArgv[0] )
		{
			V_FileBase( newArgv[0], g_RunningProcess_ExeName, sizeof( g_RunningProcess_ExeName ) );
			
			if ( V_stricmp( g_RunningProcess_ExeName, "vrad" ) == 0 || V_stricmp( g_RunningProcess_ExeName, "vvis" ) == 0 )
				V_FileBase( newArgv[newArgv.Count()-1], g_RunningProcess_MapName, sizeof( g_RunningProcess_MapName ) );
		}

		g_hRunningProcess = pi.hProcess;
		g_hRunningThread = pi.hThread;
		g_dwRunningProcessId = pi.dwProcessId;
		g_pPerfTracker->Init( g_dwRunningProcessId );
		g_CurJobPriority = iPriority;
		g_CreateProcessTime = GetTickCount();
		
		SendStartStatus( true );
	}
	else
	{
		Msg( " - ERROR in CreateProcess (%s)!\n", GetLastErrorString() );
		SendStartStatus( false );
		g_CurJobPriority = -1;
		g_RunningProcess_ExeName[0] = 0;
		g_RunningProcess_MapName[0] = 0;
	}
}


bool WaitForProcessToExit()
{
	if ( g_hRunningProcess )
	{
		// Did the process complete yet?
		if ( WaitForSingleObject( g_hRunningProcess, 0 ) == WAIT_TIMEOUT )
		{
			// Nope.. keep waiting.
			return true;
		}
		else
		{
			Msg( "Finished!\n ");
			
			SendEndStatus();

			// Change back to the 'waiting' icon.
			if ( g_pConnMgr )
				g_pConnMgr->SetAppState( VMPI_SERVICE_STATE_IDLE );
			g_CurJobPriority = -1;

			// Yep. Now we can start a new one.
			CloseHandle( g_hRunningThread );
			CloseHandle( g_hRunningProcess );
			g_hRunningProcess = g_hRunningThread = NULL;
			g_RunningProcess_ExeName[0] = g_RunningProcess_MapName[0] = 0;
		}
	}

	return false;
}


void HandleWindowMessages()
{
	MSG msg;
	while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
}

void GetRunningProcessStats( int &processorPercentage, int &memoryUsageMegabytes )
{
	static int lastProcessorPercentage = 0;
	static int lastMemory = 0;
	if ( g_hRunningProcess )
	{
		// Only update this every couple seconds. It's not too expensive (about 800 microseconds), but we don't
		// need to do it a whole lot.
		static DWORD lastReturnTime = GetTickCount();
		DWORD curTime = GetTickCount();
		if ( (curTime - lastReturnTime) >= 1000 )
		{
			lastReturnTime = curTime;
			g_pPerfTracker->GetPerfData( lastProcessorPercentage, lastMemory );
		}
	}
	else
	{
		lastProcessorPercentage = lastMemory = 0;
	}

	processorPercentage = lastProcessorPercentage;
	memoryUsageMegabytes = lastMemory;
}


void BuildPingHeader( CUtlVector<char> &data, char packetID, int iState )
{
	// Figure out the computer's name.
	char computerName[128];
	DWORD computerNameLen = sizeof( computerName );
	GetComputerName( computerName, &computerNameLen );	

	// Ping back at them.
	data.AddToTail( VMPI_PROTOCOL_VERSION );
	data.AddToTail( packetID );
	data.AddToTail( (char)iState );
	
	DWORD liveTime = GetTickCount() - g_AppStartTime;
	data.AddMultipleToTail( sizeof( liveTime ), (char*)&liveTime );

	data.AddMultipleToTail( sizeof( g_SocketPort ), (char*)&g_SocketPort );
	data.AddMultipleToTail( strlen( computerName ) + 1, computerName );

	if ( g_hRunningProcess )
		data.AddMultipleToTail( strlen( g_CurMasterName ) + 1, g_CurMasterName );
	else
		data.AddMultipleToTail( 1, "" );

	// Write in how long the worker app has been running.
	DWORD appRunTime = 0;
	if ( g_hRunningProcess )
		appRunTime = GetTickCount() - g_CreateProcessTime;

	data.AddMultipleToTail( sizeof( appRunTime ), (char*)&appRunTime );

	// Finally, write the password.
	if ( g_pPassword )
		data.AddMultipleToTail( strlen( g_pPassword ) + 1, g_pPassword );
	else
		data.AddToTail( 0 );

	data.AddMultipleToTail( V_strlen( g_VersionString ) + 1, g_VersionString );

	int processorPercentage, memoryUsageMegabytes;
	GetRunningProcessStats( processorPercentage, memoryUsageMegabytes );

	// Write processor percentage.
	data.AddToTail( (char)processorPercentage );

	// Write the EXE name.
	data.AddMultipleToTail( V_strlen( g_RunningProcess_ExeName ) + 1, g_RunningProcess_ExeName );
							
	// Write memory usage.
	short memUsageShort = (short)memoryUsageMegabytes;
	data.AddMultipleToTail( sizeof( memUsageShort ), (const char*)&memUsageShort );

	// Write the map name.
	data.AddMultipleToTail( V_strlen( g_RunningProcess_MapName ) + 1, g_RunningProcess_MapName );
}


// This tracks a list 
void AddServicesBrowserIP( const CIPAddr &ipFrom )
{
	for ( int i=0; i < g_ServicesBrowsers.Count(); i++ )
	{
		if ( g_ServicesBrowsers[i].m_Addr == ipFrom )
		{
			g_ServicesBrowsers[i].m_flLastPingTime = Plat_FloatTime();
			return;
		}
	}
	CServicesBrowserInfo info;
	info.m_Addr = ipFrom;
	info.m_flLastPingTime = Plat_FloatTime();
	g_ServicesBrowsers.AddToTail( info );
}


void UpdateServicesBrowserIPs()
{
	double curTime = Plat_FloatTime();
	for ( int i=0; i < g_ServicesBrowsers.Count(); i++ )
	{
		if ( (curTime - g_ServicesBrowsers[i].m_flLastPingTime) >= SERVICES_BROWSER_TIMEOUT )
		{
			g_ServicesBrowsers.Remove( i );
			--i;
			break;
		}
	}
}


void SendStateToServicesBrowsers()
{
	int curState;
	if ( g_hRunningProcess )
	{
		if ( g_Waiting_bPatching )
			curState = VMPI_STATE_PATCHING;
		else
			curState = VMPI_STATE_BUSY;
	}
	else if ( g_Waiting_hProcess )
	{
		if ( g_Waiting_bPatching )
			curState = VMPI_STATE_PATCHING;
		else
			curState = VMPI_STATE_DOWNLOADING;
	}
	else if ( g_iCurState == VMPI_SERVICE_STATE_DISABLED )
	{
		curState = VMPI_STATE_DISABLED;
	}
	else if ( g_bScreensaverMode && !g_bScreensaverRunning )
	{
		curState = VMPI_STATE_SCREENSAVER_DISABLED;
	}
	else
	{
		curState = VMPI_STATE_IDLE;
	}

	CUtlVector<char> data;
	BuildPingHeader( data, VMPI_PING_RESPONSE, curState );
	
	for ( int i=0; i < g_ServicesBrowsers.Count(); i++ )
	{
		g_pSocket->SendTo( &g_ServicesBrowsers[i].m_Addr, data.Base(), data.Count() );
	}
}


void StopUI()
{
	char cPacket[2] = {VMPI_SERVICE_UI_PROTOCOL_VERSION, VMPI_SERVICE_TO_UI_EXIT};
	if ( g_pConnMgr )
		g_pConnMgr->SendPacket( -1, &cPacket, sizeof( cPacket ) );

	// Wait for a bit for the connection to go away.
	DWORD startTime = GetTickCount();
	while ( GetTickCount()-startTime < 2000 )
	{
		if ( g_pConnMgr )
		{
			g_pConnMgr->Update();
			if ( !g_pConnMgr->IsConnected() )
				break;
			else
				Sleep( 10 );
		}
	}
}


void CheckScreensaverRunning()
{
	// We want to let patching finish even if we're in screensaver mode.
	if ( g_Waiting_hProcess && g_Waiting_bPatching )
		return;
	
	BOOL bRunning = false;
	SystemParametersInfo( SPI_GETSCREENSAVERRUNNING, 0, &bRunning, 0 );

	g_bScreensaverRunning = (bRunning != 0);
	if ( !g_bScreensaverRunning && g_bScreensaverMode )
	{
		KillRunningProcess( "Screensaver not running", true );
	}
}


void AdjustSuperDebugArgs( CUtlVector<char*> &args )
{
	// Get the directory this exe was run out of.
	char filename[512];
	if ( GetModuleFileName( GetModuleHandle( NULL ), filename, sizeof( filename ) ) == 0 )
		return;
	
	char *pLastSlash = filename;
	char *pCurPos = filename;
	while ( *pCurPos )
	{
		if ( *pCurPos == '/' || *pCurPos == '\\' )
			pLastSlash = pCurPos;
		++pCurPos;
	}
	*pLastSlash = 0;
	
	// In superdebug mode, run it out of c:/hl2/bin.
	const char *pBase = args[0];
	const char *pBaseCur = pBase;
	while ( *pBaseCur )
	{
		if ( *pBaseCur == '/' || *pBaseCur == '\\' || *pBaseCur == ':' )
		{
			pBase = pBaseCur+1;
			pBaseCur = pBase;
		}
		++pBaseCur;
	}
	
	int maxLen = 64 + strlen( pBase ) + 1;
	char *pNewFilename = new char[maxLen];
	_snprintf( pNewFilename, maxLen, "%s\\%s", filename, pBase );
	delete args[0];
	args[0] = pNewFilename;


	// Now insert -allowdebug.
	const char *pAllowDebug = "-allowdebug";
	char *pToInsert = new char[ strlen( pAllowDebug ) + 1 ];
	strcpy( pToInsert, pAllowDebug );
	args.InsertAfter( 0, pToInsert );

}


// -------------------------------------------------------------------------------- //
// Purpose: Launches vmpi_transfer.exe to download the required
// files from the master so we can launch.
//
// If successful, it sets hProcess to the process handle of the downloader.
// When that process terminates, we look for [cache dir]\ReadyToGo.txt and if it's
// there, then we start the job.
// -------------------------------------------------------------------------------- //
bool StartDownloadingAppFiles( 
	CUtlVector<char*> &newArgv, 
	char *cacheDir, 
	int cacheDirLen, 
	bool bShowAppWindow, 
	HANDLE *hProcess, 
	bool bPatching )
{
	*hProcess = NULL;
	
	V_strncpy( cacheDir, g_FileCachePath, cacheDirLen );
	
	// For now, cache dir is always the same. It's [current directory]\cache.
	if ( _access( cacheDir, 0 ) != 0 )
	{
		if ( !CreateDirectory( cacheDir, NULL ) && GetLastError() != ERROR_ALREADY_EXISTS )
		{
			Warning( "Unable to create cache directory: %s.\n", cacheDir );
			return false;
		}
	}

	// Clear all the files in the directory.
	char searchStr[MAX_PATH];
	V_ComposeFileName( cacheDir, "*.*", searchStr, sizeof( searchStr ) );
	_finddata_t findData;
	intptr_t ret = _findfirst( searchStr, &findData );
	if ( ret != -1 )
	{
		do
		{
			if ( findData.name[0] == '.' )
				continue;
			
			char fullFilename[MAX_PATH];
			V_ComposeFileName( cacheDir, findData.name, fullFilename, sizeof( fullFilename ) );
			if ( _unlink( fullFilename ) != 0 )
			{
				Warning( "_unlink( %s ) failed.\n", fullFilename );
				return false;
			}
		} while ( _findnext( ret, &findData ) == 0 );
		
		_findclose( ret );
	}

	// Change the EXE name to an absolute path to exactly where it is in the cache directory.
	int maxExeNameLen = 1024;
	char *pExeName = new char[maxExeNameLen];
	if ( bPatching )
	{
		V_ComposeFileName( cacheDir, "vmpi_service_install.exe", pExeName, maxExeNameLen );
		
		// Add args for the installer.
		newArgv.InsertAfter( 0, CopyString( "-DontTouchUI" ) );

		// When patching, we can't start the UI and the installer can't because we're running in the local system account
		// and the UI is running on the account of whoever logged in. So what we do is send a message to the UI telling it
		// to run <cacheDir>\WaitAndRestart and restart itself in N seconds.
		newArgv.InsertAfter( 0, CopyString( "-Install_Quiet" ) );
	}
	else
	{
		V_ComposeFileName( cacheDir, newArgv[0], pExeName, maxExeNameLen );
	}
		
	delete newArgv[0];
	newArgv[0] = pExeName;

	char fullExeFilename[MAX_PATH];
	V_ComposeFileName( g_BaseAppPath, "vmpi_transfer.exe", fullExeFilename, sizeof( fullExeFilename ) );
	
	CUtlVector<char*> downloaderArgs;
	downloaderArgs.AddToTail( fullExeFilename );
#if defined( _DEBUG )
	downloaderArgs.AddToTail( "-allowdebug" );
#endif
	downloaderArgs.AddToTail( "-CachePath" );		// Tell it where to download the files to.
	downloaderArgs.AddToTail( cacheDir );
	
	// Pass all the -mpi_worker, -mpi_file, -mpi_filebase args into the downloader app.
	for ( int i=1; i < (int)newArgv.Count()-1; i++ )
	{
		if ( V_stricmp( newArgv[i], "-mpi_filebase" ) == 0 || V_stricmp( newArgv[i], "-mpi_file" ) == 0 )
		{
			downloaderArgs.AddToTail( newArgv[i] );
			downloaderArgs.AddToTail( newArgv[i+1] );
			newArgv.Remove( i );
			newArgv.Remove( i );
			--i;
		}
		else if ( V_stricmp( newArgv[i], "-mpi_worker" ) == 0 )
		{
			// We need this arg so it knows what IP to connect to, but we want to leave it in the final launch args too.
			downloaderArgs.AddToTail( newArgv[i] );
			downloaderArgs.AddToTail( newArgv[i+1] );
			++i;
		}
	}	
	
	// Transfer each file.
	PROCESS_INFORMATION pi;
	if ( !RunProcessFromArgs( downloaderArgs, bShowAppWindow, false, g_BaseAppPath, &pi ) )
		return false;
	
	*hProcess = pi.hProcess;
	return true;
}


void SendPatchCommandToUIs( DWORD dwInstallerProcessId )
{
	Msg( "SendPatchCommandToUIs\n ");
	
	CUtlVector<char> data;
	data.AddToTail( VMPI_SERVICE_UI_PROTOCOL_VERSION );
	data.AddToTail( VMPI_SERVICE_TO_UI_PATCHING );

	// This arg tells the UI whether to exit after running the command or not.
	data.AddToTail( 1 );
	
	// First argument is the working directory, which is the cache path in this case.
	data.AddMultipleToTail( V_strlen( g_FileCachePath ) + 1, g_FileCachePath );
	
	// Second argument is the command line.
	char waitAndRestartExe[MAX_PATH], serviceUIExe[MAX_PATH], commandLine[1024 * 8];
	V_ComposeFileName( g_FileCachePath, "WaitAndRestart.exe", waitAndRestartExe, sizeof( waitAndRestartExe ) );
	V_ComposeFileName( g_BaseAppPath, "vmpi_service_ui.exe", serviceUIExe, sizeof( serviceUIExe ) ); // We're running the UI from the same directory this exe is in.
	char strSeconds[64];
	V_snprintf( strSeconds, sizeof( strSeconds ), "*%lu", dwInstallerProcessId );

	// IMPORTANT to use BuildCommandLineFromArgs here because it'll handle slashes and quotes correctly.
	// If we don't do that, the command often won't work.
	CUtlVector<char*> args;
	args.AddToTail( waitAndRestartExe );
	args.AddToTail( strSeconds );
	args.AddToTail( g_BaseAppPath );
	args.AddToTail( serviceUIExe );
	BuildCommandLineFromArgs( args, commandLine, sizeof( commandLine ) );
	data.AddMultipleToTail( V_strlen( commandLine ) + 1, commandLine );
	
	if ( g_pConnMgr )
	{
		g_pConnMgr->SendPacket( -1, data.Base(), data.Count() );
		Sleep( 1000 );	// Make sure this packet goes out.
	}
}


// Returns true if the service was just patched and should exit.
bool CheckDownloaderFinished()
{
	if ( !g_Waiting_hProcess )
		return false;
	
	// Check if the downloader has timed out and kill it if necessary.
	if ( Plat_FloatTime() - g_Waiting_StartTime > MAX_DOWNLOADER_TIME_ALLOWED )
	{
		TerminateProcess( g_Waiting_hProcess, 1 );
		CloseHandle( g_Waiting_hProcess );
		g_Waiting_hProcess = NULL;
		return false;
	}

	// Check if it's done.
	if ( WaitForSingleObject( g_Waiting_hProcess, 0 ) != WAIT_OBJECT_0 )
		return false;

	CloseHandle( g_Waiting_hProcess );
	g_Waiting_hProcess = NULL;

	// Ok, it's done. Did it finish successfully?
	char testFilename[MAX_PATH];
	V_ComposeFileName( g_FileCachePath, "ReadyToGo.txt", testFilename, sizeof( testFilename ) );
	if ( _access( testFilename, 0 ) != 0 )
		return false;
		
	// Ok, the downloader finished successfully. Run the worker app.
	if ( g_bSuperDebugMode )
		AdjustSuperDebugArgs( g_Waiting_Argv );

	// Figure out the name of the master machine.
	V_strncpy( g_CurMasterName, "<unknown>", sizeof( g_CurMasterName ) );
	for ( int iArg=1; iArg < g_Waiting_Argv.Count()-1; iArg++ )
	{
		if ( stricmp( g_Waiting_Argv[iArg], "-mpi_MasterName" ) == 0 )
		{
			Q_strncpy( g_CurMasterName, g_Waiting_Argv[iArg+1], sizeof( g_CurMasterName ) );
		}
	}

	char DLLFilename[MAX_PATH];
	if ( FindArg( __argc, __argv, "-TryDLLMode" ) && 
		g_RunMode == RUNMODE_CONSOLE && 
		GetDLLFilename( g_Waiting_Argv, DLLFilename ) &&
		!g_Waiting_bPatching )
	{
		// This is just a helper for debugging. If it's VRAD, we can run it
		// in-process as a DLL instead of running it as a separate EXE.
		RunInDLL( DLLFilename, g_Waiting_Argv );
	}
	else
	{
		// Run the (hopefully!) MPI app they specified.
		RunProcessAtCommandLine( g_Waiting_Argv, g_Waiting_bShowAppWindow, g_Waiting_bPatching, g_Waiting_Priority );
		
		if ( g_Waiting_bPatching )
		{														
			// Tell any currently-running UI apps to patch themselves and quit ASAP so the installer can finish.
			SendPatchCommandToUIs( g_dwRunningProcessId );

			ResumeThread( g_hRunningThread ); // We started the installer suspended so we could make sure we'd send out the patch command.
			
			// We just ran the installer, but let's forget about it, otherwise we'll kill its process when we exit here.
			CloseHandle( g_hRunningProcess );
			CloseHandle( g_hRunningThread ) ;
			g_hRunningProcess = g_hRunningThread = NULL;
			g_RunningProcess_ExeName[0] = 0;
			g_RunningProcess_MapName[0] = 0;

			ServiceHelpers_ExitEarly();
			return true;
		}
	}
	
	g_Waiting_Argv.PurgeAndDeleteElements();
	return false;
}


void HandlePacket_LOOKING_FOR_WORKERS( bf_read &buf, const CIPAddr &ipFrom )
{
	// If we're downloading files for a job request, don't process any more "looking for workers" packets.
	if ( g_Waiting_hProcess )
		return;
	
	// This will be a nonzero-length string if patching.
	char versionString[512];
	buf.ReadString( versionString, sizeof( versionString ) );

	int iPort = buf.ReadShort();
	int iPriority = buf.ReadShort();

	// Make sure we don't run the same job more than once.
	if ( !CheckJobID( buf, g_CurJobID ) )
		return;

	CUtlVector<char*> newArgv;
	GetArgsFromBuffer( buf, newArgv, &g_Waiting_bShowAppWindow );

	bool bForcePatch = false;
	if ( buf.GetNumBytesLeft() >= 1 )
		bForcePatch = (buf.ReadByte() != 0);

	int iDownloaderPort = iPort;
	if  ( buf.GetNumBytesLeft() >= 2 )
		iDownloaderPort = buf.ReadShort();

	// Add these arguments after the executable filename to tell the program
	// that it's an MPI worker and who to connect to. 
	char strDownloaderIP[128], strMainIP[128];
	V_snprintf( strDownloaderIP, sizeof( strDownloaderIP ), "%d.%d.%d.%d:%d", ipFrom.ip[0], ipFrom.ip[1], ipFrom.ip[2], ipFrom.ip[3], iDownloaderPort );
	V_snprintf( strMainIP, sizeof( strMainIP ), "%d.%d.%d.%d:%d", ipFrom.ip[0], ipFrom.ip[1], ipFrom.ip[2], ipFrom.ip[3], iPort );

	// (-mpi is already on the command line of whoever ran the app).
	// AppendArg( commandLine, sizeof( commandLine ), "-mpi" );
	newArgv.InsertAfter( 0, CopyString( "-mpi_worker" ) );
	newArgv.InsertAfter( 1, CopyString( strDownloaderIP ) );


	// If the version string is set, then this is a patch.
	bool bPatching = false;
	if ( versionString[0] != 0 )
	{
		bPatching = true;
		
		// Check that we haven't applied this patch version yet. This case usually happens right after we've applied a patch
		// and we're restarting. The vmpi_transfer master is still pinging us telling us to patch, but we don't want to
		// reapply this patch.
		if ( atof( versionString ) <= atof( g_VersionString ) && !bForcePatch )
		{
			newArgv.PurgeAndDeleteElements();
			return;
		}
		
		// Ok, it's a new version. Get rid of whatever was running before.
		KillRunningProcess( "Starting a patch..", true );
	}
								 
	// If there's already a job running, only interrupt it if this new one has a higher priority.
	if ( WaitForProcessToExit() )
	{
		if ( iPriority > g_CurJobPriority )
		{
			KillRunningProcess( "Interrupted by a higher priority process", true );
		}
		else
		{
			// This means we're already running a job with equal to or greater priority than
			// the one that has been requested. We're going to ignore this request.
			newArgv.PurgeAndDeleteElements();
			return;
		}
	}

	// Responses go here.
	g_CurRespondAddr = ipFrom;
	
	// Also look for -mpi_ShowAppWindow in the args to the service.
	if ( !g_Waiting_bShowAppWindow && FindArg( __argc, __argv, "-mpi_ShowAppWindow" ) )
		g_Waiting_bShowAppWindow = true;

	// Copy all the files from the master and put them in our cache dir to run with.
	char cacheDir[MAX_PATH];
	if ( StartDownloadingAppFiles( newArgv, cacheDir, sizeof( cacheDir ), g_Waiting_bShowAppWindow, &g_Waiting_hProcess, bPatching ) )
	{
		// After it's downloaded, we want it to switch to the main connection port.
		if ( newArgv.Count() >= 3 && V_stricmp( newArgv[2], strDownloaderIP ) == 0 )
		{
			delete newArgv[2];
			newArgv[2] = CopyString( strMainIP );
		}
		
		g_Waiting_StartTime = Plat_FloatTime();
		g_Waiting_Argv.PurgeAndDeleteElements();
		g_Waiting_Argv = newArgv;
		g_Waiting_Priority = iPriority;
		g_Waiting_bPatching = bPatching;
		newArgv.Purge();
	}
	else
	{
		newArgv.PurgeAndDeleteElements();
	}

	// Remember that we tried to run this job so we don't try to run it again.
	AddJobMemory( g_CurJobID );
	
	SendStateToServicesBrowsers();
}


void HandlePacket_STOP_SERVICE( bf_read &buf, const CIPAddr &ipFrom )
{
	Msg( "Got a STOP_SERVICE packet. Shutting down...\n" );

	CWaitTimer timer( 1 );
	while ( 1 )
	{
		AddServicesBrowserIP( ipFrom );
		SendStateToServicesBrowsers();

		if ( timer.ShouldKeepWaiting() )
			Sleep( 200 );
		else
			break;
	}

	StopUI();
	ServiceHelpers_ExitEarly();
}


void HandlePacket_KILL_PROCESS( const CIPAddr *ipFrom )
{
	if ( Plat_FloatTime() - g_flLastKillProcessTime > 5 )
	{
		KillRunningProcess( "Got a KILL_PROCESS packet. Stopping the worker executable.\n", true );
		
		if ( ipFrom )
		{
			AddServicesBrowserIP( *ipFrom );
			SendStateToServicesBrowsers();
		}
		
		g_flLastKillProcessTime = Plat_FloatTime();
	}
}


void HandlePacket_FORCE_PASSWORD_CHANGE( bf_read &buf, const CIPAddr &ipFrom )
{
	char newPassword[512];
	buf.ReadString( newPassword, sizeof( newPassword ) );
	
	Msg( "Got a FORCE_PASSWORD_CHANGE (%s) packet.\n", newPassword );
	
	SetPassword( newPassword );
	if ( g_pConnMgr )
		g_pConnMgr->SendCurStateTo( -1 );
}


void VMPI_Waiter_Update()
{
	CheckScreensaverRunning();
	HandleWindowMessages();
	UpdateServicesBrowserIPs();
	
	while ( 1 )
	{
		WaitForProcessToExit();
		if ( CheckDownloaderFinished() )
			return;

		// Recv off the socket first so it clears the queue while we're waiting for the process to exit.
		char data[4096];
		CIPAddr ipFrom;
		int len = g_pSocket->RecvFrom( data, sizeof( data ), &ipFrom );

		// Any incoming packets?
		if ( len <= 0 )
			break;

		bf_read buf( data, len );
		if ( buf.ReadByte() != VMPI_PROTOCOL_VERSION )
			continue;

		// Only handle packets with the right password.
		char pwString[256];
		buf.ReadString( pwString, sizeof( pwString ) );

		int packetID = buf.ReadByte();
		
		if ( pwString[0] == VMPI_PASSWORD_OVERRIDE )
		{
			// Always process these packets regardless of the password (these usually come from
			// the installer when it is trying to stop a previously-running instance).
		}
		else if ( packetID == VMPI_LOOKING_FOR_WORKERS )
		{
			if ( pwString[0] == 0 )
			{
				if ( g_pPassword && g_pPassword[0] != 0 )
					continue;
			}
			else
			{
				if ( !g_pPassword || stricmp( g_pPassword, pwString ) != 0 )
					continue;
			}
		}

		// VMPI_KILL_PROCESS is checked before everything.
		if ( packetID == VMPI_KILL_PROCESS )
		{
			HandlePacket_KILL_PROCESS( &ipFrom );
		}
		else if ( packetID == VMPI_PING_REQUEST )
		{
			AddServicesBrowserIP( ipFrom );
			SendStateToServicesBrowsers();
		}
		else if ( packetID == VMPI_STOP_SERVICE )
		{
			HandlePacket_STOP_SERVICE( buf, ipFrom );
			return;
		}
		else if ( packetID == VMPI_SERVICE_PATCH )
		{
			// The key to doing this here is that we ignore whether we're disabled or in screensaver mode.. we always handle
			// the patch command (unless we've already handled this job ID OR if we've already applied this patch version).
			HandlePacket_LOOKING_FOR_WORKERS( buf, ipFrom );
		}
		else if ( packetID == VMPI_FORCE_PASSWORD_CHANGE )
		{
			HandlePacket_FORCE_PASSWORD_CHANGE( buf, ipFrom );
		}

		// If they've told us not to wait for jobs, then ignore the packet.
		if ( g_iCurState == VMPI_SERVICE_STATE_DISABLED || (g_bScreensaverMode && !g_bScreensaverRunning) )
			continue;

		if ( packetID == VMPI_LOOKING_FOR_WORKERS )
		{
			HandlePacket_LOOKING_FOR_WORKERS( buf, ipFrom );
		}
	}
}


// ------------------------------------------------------------------------------------------------ //
// Startup and service code.
// ------------------------------------------------------------------------------------------------ //

void RunMainLoop()
{
	// This is the service's main loop.
	while ( 1 )
	{
		// If the service has been told to exit, then just exit.
		if ( ServiceHelpers_ShouldExit() )
			break;

		VMPI_Waiter_Update();
		g_pConnMgr->Update();
		
		Sleep( 50 );
	}
}


void InternalRunService()
{
	if ( !VMPI_Waiter_Init() )
		return;

	RunMainLoop();
	VMPI_Waiter_Term();
}


// This function runs us as a console app. Useful for debugging or if you want to run more
// than one instance of VRAD on the same machine.
void RunAsNonServiceApp()
{
	InternalRunService();
}


// This function runs inside the service thread.
void ServiceThreadFn( void *pParam )
{
	InternalRunService();
}


// This function works with the service manager and runs as a system service.
void RunService()
{
	if( !ServiceHelpers_StartService( VMPI_SERVICE_NAME_INTERNAL, ServiceThreadFn, NULL ) )
	{
		Msg( "Service manager not started. Running as console app.\n" );
		g_RunMode = RUNMODE_CONSOLE;
		InternalRunService();
	}
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	// Hook spew output.
	SpewOutputFunc( MySpewOutputFunc );

	// Get access to the registry..
	RegCreateKey( HKEY_LOCAL_MACHINE, VMPI_SERVICE_KEY, &g_hVMPIServiceKey );

	// Setup our version string.
	LoadString( hInstance, VMPI_SERVICE_IDS_VERSION_STRING, g_VersionString, sizeof( g_VersionString ) );

	// Setup the base app path.
	if ( !GetModuleFileName( GetModuleHandle( NULL ), g_BaseAppPath, sizeof( g_BaseAppPath ) ) )
	{
		Warning( "GetModuleFileName failed.\n" );
		return false;
	}
	V_StripLastDir( g_BaseAppPath, sizeof( g_BaseAppPath ) );

	// Setup the cache path.
	V_ComposeFileName( g_BaseAppPath, "vmpi_service_cache", g_FileCachePath, sizeof( g_FileCachePath ) );


	const char *pArg = FindArg( __argc, __argv, "-mpi_pw", NULL );
	SetPassword( pArg );
	
	if ( FindArg( __argc, __argv, "-console" ) )
	{					
		g_RunMode = RUNMODE_CONSOLE;
	}
	else
	{
		g_RunMode = RUNMODE_SERVICE;
	}

	if ( FindArg( __argc, __argv, "-superdebug" ) )
		g_bSuperDebugMode = true;

	g_AppStartTime = GetTickCount();
	g_bMinimized = FindArg( __argc, __argv, "-minimized" ) != NULL;

	ServiceHelpers_Init(); 	
	g_hInstance = hInstance;

	LoadStateFromRegistry();

	// Install the service?
	if ( g_RunMode == RUNMODE_CONSOLE )
	{					
		RunAsNonServiceApp();
	}
	else
	{
		RunService();
	}

	return 0;
}

