//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	LOCAL_CMDS.CPP
//
//	Local commands ( *xxxx ) executed by this application.
//=====================================================================================//
#include "vxconsole.h"

localCommand_t g_localCommands[] =
{
    // Command name,        Flags		Handler				Help string
    // console commands
    { "*cls",               FN_CONSOLE,	lc_cls,				": Clear the screen" },
 	{ "*connect",			FN_CONSOLE,	lc_autoConnect,		": Connect and listen until successful" },
	{ "*disconnect",        FN_CONSOLE,	lc_disconnect,		": Terminate Debug Console session" },
    { "*help",              FN_CONSOLE,	lc_help,			"[command] : List commands/usage" },
	{ "*quit",              FN_CONSOLE,	lc_quit,			": Terminate console" },
    { "*run",				FN_XBOX,	lc_run,				"[app.xex] : Run application or Reboot" },
	{ "*reset",				FN_XBOX,	lc_reset,			"Reboot" },
    { "*screenshot",		FN_XBOX,	lc_screenshot,		"<file.bmp> : Copy the screen to file.bmp" },
    { "*memory",		    FN_APP,		lc_memory,			": Dump Memory Stats" },
    { "*dir",				FN_XBOX,	lc_dir,				"<filepath> [/s]: Directory listing" },
    { "*del",				FN_XBOX,	lc_del,				"<filepath> [/s] [/q]: Delete file" },
    { "*modules",			FN_XBOX,	lc_modules,			": Lists currently loaded modules" },
    { "*sections",			FN_XBOX,	lc_sections,		"<module> : Lists the sections in the module" },
	{ "*bug",				FN_CONSOLE,	lc_bug,				": Open bug submission form" },
	{ "*clearconfigs",		FN_XBOX,	lc_ClearConfigs,	": Erase all game configs" },

    // xcommands
    { "*break",				FN_XBOX,	NULL,				" addr=<address> | 'Write'/'Read'/'Execute'=<address> size=<DataSize> ['clear']: Sets/Clears a breakpoint" },
//  { "*bye",				FN_XBOX,	NULL,				": Closes connection" },
    { "*continue",			FN_XBOX,	NULL,				" thread=<threadid>: Resumes execution of a thread which has been stopped" },
//  { "*delete",			FN_XBOX,	NULL,				" name=<remotefile>: Deletes a file on the Xbox" },
//  { "*dirlist",			FN_XBOX,	NULL,				" name=<remotedir>: Lists the items in the directory" },
    { "*getcontext",		FN_XBOX,	NULL,				" thread=<threadid> 'Control' | 'Int' | 'FP' | 'Full':  Gets the context of the thread" },
    { "*getfileattributes",	FN_XBOX,	NULL,				" name=<remotefile>: Gets attributes of a file" },
    { "*getmem",			FN_XBOX,	NULL,				" addr=<address> length=<len>: Reads memory from the Xbox" },
    { "*go",				FN_XBOX,	NULL,				": Resumes suspended title threads" },
    { "*halt",				FN_XBOX,	NULL,				" thread=<threadid> Breaks a thread" },
    { "*isstopped",			FN_XBOX,	NULL,				" thread=<threadid>: Determines if a thread is stopped and why" },
    { "*mkdir",				FN_XBOX,	NULL,				" name=<remotedir>: Creates a new directory on the Xbox" },
    { "*modlong",			FN_XBOX,	NULL,				" name=<module>: Lists the long name of the module" },
//  { "*reboot",			FN_XBOX,	NULL,				" [warm] [wait]: Reboots the xbox" },
    { "*rename",			FN_XBOX,	NULL,				" name=<remotefile> newname=<newname>: Renames a file on the Xbox" },
    { "*resume",			FN_XBOX,	NULL,				" thread=<threadid>: Resumes thread execution" },
    { "*setcontext",		FN_XBOX,	NULL,				" thread=<threadid>: Sets the context of the thread." },
    { "*setfileattributes",	FN_XBOX,	NULL,				" <remotefile> <attrs>: Sets attributes of a file" },
    { "*setmem",			FN_XBOX,	NULL,				" addr=<address> data=<rawdata>: Sets memory on the Xbox" },
    { "*stop",				FN_XBOX,	NULL,				": Stops the process" },
    { "*suspend",			FN_XBOX,	NULL,				" thread=<threadid>: Suspends the thread" },
    { "*systime",			FN_XBOX,	NULL,				": Gets the system time of the xbox" },
    { "*threadinfo",		FN_XBOX,	NULL,				" thread=<threadid>: Gets thread info" },
    { "*threads",			FN_XBOX,	lc_threads,			": Gets the thread list" },
//  { "*title",				FN_XBOX,	NULL,				" dir=<remotedir> name=<remotexex> [cmdline=<cmdline>]: Sets title to run" },
	{ "*xexinfo",			FN_XBOX,	NULL,				" name=<remotexex | 'running'>: Gets info on an xex" },
	{ "*crash",				FN_XBOX,	lc_crashdump,		" crash the console, emitting a dump" },
};
const int g_numLocalCommands = sizeof( g_localCommands )/sizeof( g_localCommands[0] );

static BOOL	g_bAutoConnectQuiet;
static int	g_bAutoConnectWait;

//-----------------------------------------------------------------------------
//	MatchCommands
//
//-----------------------------------------------------------------------------
int MatchLocalCommands( char* cmdStr, const char* cmdList[], int maxCmds )
{
	int numCommands = 0;

	// look in local
	int matchLen = strlen( cmdStr );
	for ( int i=0; i<g_numLocalCommands; i++ )
	{
		if ( !strnicmp( cmdStr, g_localCommands[i].strCommand, matchLen ) )
		{
			cmdList[numCommands++] = g_localCommands[i].strCommand;
			if ( numCommands >= maxCmds )
				break;
		}
	}

	return ( numCommands );
}

//-----------------------------------------------------------------------------
//	DecodeRebootArgs
//
//-----------------------------------------------------------------------------
void DecodeRebootArgs( int argc, char** argv, char* xexPath, char* xexName, char* xexArgs )
{
	char	drive[MAX_PATH];
	char	dir[MAX_PATH];
	char	filename[MAX_PATH];
	char	extension[MAX_PATH];

	xexPath[0] = '\0';
	xexName[0] = '\0';
	xexArgs[0] = '\0';

	if ( !argc )
		return;

	_splitpath( argv[0], drive, dir, filename, extension );

	sprintf( xexPath, "%s%s", drive, dir );
	sprintf( xexName, "%s%s", filename, extension );

	for ( int i=1; i<argc; i++ )
	{
		strcat( xexArgs, argv[i] );
		if ( i < argc-1 )
			strcat( xexArgs, " " );
	}	
}

//-----------------------------------------------------------------------------
// Helper function. Causes a disconnect, has optional re-connect time.
// A non-zero wait will delay before attempting re-connect.
//-----------------------------------------------------------------------------
void DoDisconnect( BOOL bKeepConnection, int waitTime )
{
	// save state, user gates auto-connect ability
	int autoConnect = g_autoConnect;

	// full disconnect, disables autoconnect
	lc_disconnect( 0, NULL );

	if ( autoConnect && bKeepConnection && waitTime > 0 )
	{
		// restore autoconnect status
		lc_autoConnect( 0, NULL );

		// lets the system settle a little between contexts
		g_bAutoConnectWait = waitTime;
		g_bAutoConnectQuiet = FALSE;
	}
}

//-----------------------------------------------------------------------------
//	lc_bug
//
//-----------------------------------------------------------------------------
BOOL lc_bug( int argc, char* argv[] )
{
	if ( argc != 1 )
	{
		char* args[2] = {"*help", argv[0]};
		lc_help( 1, args );
		goto cleanUp;
	}

	BugDlg_Open();

	return TRUE;

cleanUp:
	return FALSE;
}

//-----------------------------------------------------------------------------
//	lc_dir
//
//-----------------------------------------------------------------------------
BOOL lc_dir( int argc, char* argv[] )
{
	fileNode_t				*nodePtr;
	fileNode_t				*pFileList;
	int						numFiles;
	int						numDirs;
	__int64					totalBytes;
	bool					recurse;
	char					dateTimeString[256];
	char					sizeString[64];
	char					filePath[MAX_PATH];
	char					fileName[MAX_PATH];
	char					targetName[MAX_PATH];
	char					newPath[MAX_PATH];
	SYSTEMTIME				systemTime;
	SYSTEMTIME				localTime;
	const char				*dirString;
	BOOL					errCode;
	int						nPass;
	TIME_ZONE_INFORMATION	tzInfo;

	pFileList = NULL;
	errCode   = FALSE;

	if ( argc < 2 )
	{
		char* args[2] = {"*dir", argv[0]};
		lc_help( 2, args );
		goto cleanUp;
	}

	strcpy( newPath, argv[1] );

	// seperate components
	Sys_StripFilename( newPath, filePath, sizeof( filePath ) );
	Sys_StripPath( newPath, fileName, sizeof( fileName ) );

	if ( fileName[0] )
	{
		if ( !strstr( fileName,"*" ) && !strstr( fileName,"?" ) )
		{
			// assume filename was a directory name
			strcat( newPath, "\\" );
			Sys_StripFilename( newPath, filePath, sizeof( filePath ) );
			Sys_StripPath( newPath, fileName, sizeof( fileName ) );
		}
	}

	recurse = false;
	if ( argc >= 3 )
	{
		if ( !stricmp( argv[2], "/s" ) )
			recurse = true;
	}

	if ( !GetTargetFileList_r( filePath, recurse, FA_NORMAL|FA_DIRECTORY|FA_READONLY, 0, &pFileList ) )
	{
		ConsoleWindowPrintf( RGB( 255,0,0 ), "Bad Target Path '%s'\n", filePath );
		goto cleanUp;
	}

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "\nDirectory of %s\n\n", argv[1] );

	GetTimeZoneInformation( &tzInfo );

	numFiles   = 0;
	numDirs    = 0;
	totalBytes = 0;
	for ( nPass=0; nPass<2; nPass++ )
	{
		for ( nodePtr=pFileList; nodePtr; nodePtr=nodePtr->nextPtr )
		{
			if ( !nPass && !( nodePtr->attributes & FA_DIRECTORY ) )
			{
				// first pass, dirs only
				continue;
			}
			else if ( nPass && ( nodePtr->attributes & FA_DIRECTORY ) )
			{
				// second pass, files only
				continue;
			}

			Sys_StripPath( nodePtr->filename, targetName, sizeof( targetName ) );

			if ( fileName[0] && !Sys_IsWildcardMatch( fileName, targetName, false ) )
				continue;

			FileTimeToSystemTime( &nodePtr->changeTime, &systemTime );
			SystemTimeToTzSpecificLocalTime( &tzInfo, &systemTime, &localTime );
			SystemTimeToString( &localTime, dateTimeString, sizeof( dateTimeString ) );

			__int64 fullSize = MAKEINT64( nodePtr->sizeHigh, nodePtr->sizeLow );

			if ( nodePtr->attributes & FA_DIRECTORY )
			{
				numDirs++;
				dirString = "<DIR>";
				sprintf( sizeString, "%s", "         " );
			}
			else
			{
				numFiles++;
				dirString = "     ";
				Sys_NumberToCommaString( fullSize, sizeString, sizeof( sizeString ) );
				totalBytes += fullSize;
			}

			ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%s  %s %12s %s\n", dateTimeString, dirString, sizeString, targetName );
		}
	}

	Sys_NumberToCommaString( totalBytes, sizeString, sizeof( sizeString ) );
	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%9s %d File(s) %s bytes\n", " ", numFiles, sizeString );
	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%9s %d Dir(s)\n", " ", numDirs );

	// success
	errCode = TRUE;

cleanUp:
	if ( pFileList )
		FreeTargetFileList( pFileList );

	return errCode;
}

//-----------------------------------------------------------------------------
//	lc_del
//
//-----------------------------------------------------------------------------
BOOL lc_del( int argc, char* argv[] )
{
	HRESULT		hr;
	fileNode_t	*nodePtr;
	fileNode_t	*pFileList;
	int			numDeleted;
	int			numErrors;
	bool		recurse;
	char		filePath[MAX_PATH];
	char		fileName[MAX_PATH];
	char		targetName[MAX_PATH];
	BOOL		errCode;

	pFileList = NULL;
	errCode   = FALSE;

	if ( argc < 2 )
	{
		char* args[2] = {"*del", argv[0]};
		lc_help( 2, args );
		goto cleanUp;
	}

	// seperate components
	Sys_StripFilename( argv[1], filePath, sizeof( filePath ) );
	Sys_StripPath( argv[1], fileName, sizeof( fileName ) );

	bool bQuiet = false;
	recurse = false;
	if ( argc >= 3 )
	{
		for ( int i = 2; i < argc; i++ )
		{
			if ( !V_stricmp( argv[i], "/s" ) )
			{
				recurse = true;
			}
			else if ( !V_stricmp( argv[i], "/q" ) )
			{
				// silence errors
				bQuiet = true;
			}
		}
	}

	if ( !GetTargetFileList_r( filePath, recurse, FA_NORMAL|FA_READONLY|FA_DIRECTORY, 0, &pFileList ) )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "Bad Target Path '%s'\n", filePath );
		goto cleanUp;
	}

	numErrors  = 0;
	numDeleted = 0;
	for ( nodePtr=pFileList; nodePtr; nodePtr=nodePtr->nextPtr )
	{
		Sys_StripPath( nodePtr->filename, targetName, sizeof( targetName ) );

		if ( fileName[0] && !Sys_IsWildcardMatch( fileName, targetName, false ) )
			continue;

		hr = DmDeleteFile( nodePtr->filename, ( nodePtr->attributes & FA_DIRECTORY ) != 0 );
		if ( hr != XBDM_NOERR )
		{
			if ( !bQuiet )
			{
				ConsoleWindowPrintf( XBX_CLR_RED, "Error Deleting '%s'\n", nodePtr->filename );
			}
			numErrors++;
		}
		else
		{
			ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Deleted '%s'\n", nodePtr->filename );
			numDeleted++;
		}
	}

	if ( !numDeleted && !numErrors )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "No Files found for '%s'\n", argv[1] );
	}
	else
	{
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%d files deleted.\n", numDeleted );
	}

	// success
	errCode = TRUE;

cleanUp:
	if ( pFileList )
		FreeTargetFileList( pFileList );

	return errCode;
}

//-----------------------------------------------------------------------------
//	lc_memory
//
//-----------------------------------------------------------------------------
BOOL lc_memory( int argc, char* argv[] )
{
	HRESULT	hr;

	hr = DmAPI_SendCommand( VXCONSOLE_COMMAND_PREFIX "!" "__memory__", true );
	if ( FAILED( hr ) )
		return FALSE;

	// success
	return TRUE;
}

//-----------------------------------------------------------------------------
//	lc_screenshot
//
//-----------------------------------------------------------------------------
BOOL lc_screenshot( int argc, char* argv[] )
{
	char			filename[MAX_PATH];
	char			filepath[MAX_PATH];
	static int		shot = 0;
	struct _stat	dummyStat;	

	if ( argc <= 1 )
	{
		strcpy( filepath, g_localPath );
		Sys_AddFileSeperator( filepath, sizeof( filepath ) );

		// spin up to available file
		while ( 1 )
		{
			sprintf( filename, "%sscreenshot_%4.4d.bmp", filepath, shot );
			if ( _stat( filename, &dummyStat ) == -1 )
			{
				// filename not in use
				break;
			}
			shot++;
		}
	}
	else if ( argc == 2 )
	{
		strcpy( filename, argv[1] );
		Sys_AddExtension( ".bmp", filename, sizeof( filename ) );
	}
	else if ( argc > 2 )
	{
		char* args[2] = {"*help", argv[0]};
		lc_help( 2, args );
		goto cleanUp;
	}

	HRESULT hr = DmScreenShot( filename );
	if ( FAILED( hr ) )
    {
        DmAPI_DisplayError( "lc_screenshot(): DmScreenShot() failure", hr );
		goto cleanUp;
    }

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Screenshot saved to %s\n", filename );

	// advance the shot
	shot++;

	// success
	return TRUE;

cleanUp:
	return FALSE;
}

//-----------------------------------------------------------------------------
//	lc_modules
//
//-----------------------------------------------------------------------------
BOOL lc_modules( int argc, char* argv[] )
{
	HRESULT						hr;
	PDM_WALK_MODULES			pWalkMod = NULL;
	CUtlVector< DMN_MODLOAD >	list;

	// add a fake module at 0xFFFFFFFF to make sorting simple
	DMN_MODLOAD modLoad = { 0 };
	modLoad.BaseAddress = (VOID*)0xFFFFFFFF;
	list.AddToTail( modLoad );

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Modules:\n" );
	while ( 1 ) 
	{
		hr = DmWalkLoadedModules( &pWalkMod, &modLoad );
		if ( hr == XBDM_ENDOFLIST )
		{
			hr = XBDM_NOERR;
			break;
		}
		else if ( FAILED( hr ) )
		{
			DmAPI_DisplayError( "lc_modules(): DmWalkLoadedModules() failure", hr );
			break;
		}

		// add in ascending address order
		for ( int i = 0; i < list.Count(); i++ )
		{
			if ( modLoad.BaseAddress <= list[i].BaseAddress )
			{
				list.InsertBefore( i, modLoad );
				break;
			}
		}
	}

	unsigned int total = 0;
	for ( int i = 0; i < list.Count()-1; i++ )
	{
		DMN_MODLOAD *pModule = &list[i];
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Base: 0x%8.8x, Size: %5.2f MB, [%s]\n", pModule->BaseAddress, pModule->Size/( 1024.0f*1024.0f ), pModule->Name );

		total += pModule->Size;
	}

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Total: %.2f MB\n\n", total/( 1024.0f*1024.0f ) );

	if ( pWalkMod )
	{
		DmCloseLoadedModules( pWalkMod );
	}

	if ( !FAILED( hr ) )
	{
		// success
		return TRUE;
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
//	lc_sections
//
//-----------------------------------------------------------------------------
BOOL lc_sections( int argc, char* argv[] )
{
	char				moduleName[MAX_PATH];
	HRESULT				hr;
	PDM_WALK_MODSECT	pWalkModSect = NULL;
	DMN_SECTIONLOAD		sectLoad;

	if ( argc != 2 )
	{
		char* args[2] = {"*help", argv[0]};
		lc_help( 2, args );
		goto cleanUp;
	}

	strcpy( moduleName, argv[1] );

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Sections:\n" );
	while ( 1 ) 
	{
		hr = DmWalkModuleSections( &pWalkModSect, moduleName, &sectLoad );
		if ( hr == XBDM_ENDOFLIST )
		{
			hr = XBDM_NOERR;
			break;
		}
		else if ( FAILED( hr ) )
		{
			DmAPI_DisplayError( "lc_sections(): DmWalkModuleSections() failure", hr );
			break;
		}

		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "[%s]:\n", sectLoad.Name );
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "  Base:  0x%8.8x\n", sectLoad.BaseAddress );
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "  Size:  %.2f MB ( %d bytes )\n", sectLoad.Size/( 1024.0f*1024.0f ), sectLoad.Size );
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "  Index: %d\n", sectLoad.Index );
	}

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "End.\n\n" );

	if ( pWalkModSect )
		DmCloseModuleSections( pWalkModSect );

	if ( !FAILED( hr ) )
	{
		// success
		return TRUE;
	}

cleanUp:
	return FALSE;
}

//-----------------------------------------------------------------------------
//	lc_threads
//
//-----------------------------------------------------------------------------
BOOL lc_threads( int argc, char* argv[] )
{	
	char	nameBuff[256];
	char	suspendBuff[32];

	DWORD threadList[256];
	DWORD numThreads = 256;
	memset( &threadList, 0, sizeof( threadList ) );
	HRESULT hr = DmGetThreadList( threadList, &numThreads );
	if ( FAILED( hr ) )
		return FALSE;

	// enumerate threads and display sorted by processor
	DM_THREADINFOEX threadInfoEx;
	for ( int core = 0; core < 3; core++ )
	{
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "\n--- CORE %d ---\n", core );

		for ( int unit = 0; unit < 2; unit++ )
		{
			for ( int i = 0; i < (int)numThreads; i++ )
			{
				threadInfoEx.Size = sizeof( DM_THREADINFOEX );
				hr = DmGetThreadInfoEx( threadList[i], &threadInfoEx );
				if ( FAILED( hr ) )
					return FALSE;

				if ( threadInfoEx.CurrentProcessor != core*2 + unit )
				{
					continue;
				}		
		
				nameBuff[0] = '\0';
				DmGetMemory( threadInfoEx.ThreadNameAddress, threadInfoEx.ThreadNameLength, nameBuff, NULL );
				if ( !nameBuff[0] )
				{
					strcpy( nameBuff, "???" );
				}

				suspendBuff[0] = '\0';
				if ( threadInfoEx.SuspendCount )
				{
					sprintf( suspendBuff, "(Suspend: %d)", threadInfoEx.SuspendCount );
				}

				ConsoleWindowPrintf( XBX_CLR_DEFAULT, "   Id: 0x%8.8x Pri: %2d Proc: %1d Stack: %7.2f KB [%s] %s\n", 
					threadList[i], 
					threadInfoEx.Priority,
					threadInfoEx.CurrentProcessor,
					(float)( (unsigned int)threadInfoEx.StackBase - (unsigned int)threadInfoEx.StackLimit )/1024.0f,
					nameBuff,
					suspendBuff );
			}
		}
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
//	lc_run
//
//-----------------------------------------------------------------------------
BOOL lc_run( int argc, char* argv[] )
{
	HRESULT hr;
	char	xexDrive[MAX_PATH];
	char	xexPath[MAX_PATH];
	char	xexName[MAX_PATH];
	char	xexArgs[MAX_PATH];

	if ( !argc )
	{
		char* args[2] = {"*help", argv[0]};
		lc_help( 2, args );
		goto cleanUp;
	}
	
	// copy args
	g_rebootArgc = argc-1;
	for ( int i=1; i<argc; i++ )
	{
		if ( i==1 )
		{
			strcpy( xexPath, argv[i] );
			Sys_AddExtension( ".xex", xexPath, sizeof( xexPath ) );
			_splitpath( xexPath, xexDrive, NULL, NULL, NULL );
			if ( !xexDrive[0] )
			{
				char szTempPath[MAX_PATH];
				V_strncpy( szTempPath, "e:\\", sizeof( szTempPath ) );
				V_strncat( szTempPath, xexPath, sizeof( szTempPath ) );
				V_strncpy( xexPath, szTempPath, sizeof( xexPath ) );
			}

			g_rebootArgv[i-1] = Sys_CopyString( xexPath );
		}
		else
		{
			g_rebootArgv[i-1] = Sys_CopyString( argv[i] );
		}
	}

	if ( !g_rebootArgc )
	{
		// reboot
		hr = DmReboot( DMBOOT_COLD );
	}
	else
	{
		DecodeRebootArgs( g_rebootArgc, g_rebootArgv, xexPath, xexName, xexArgs );

		// trial set title - ensure title is present
		hr = DmSetTitle( xexPath, xexName, xexArgs );
		if ( FAILED( hr ) )
		{
			DmAPI_DisplayError( "lc_Run(): DmSetTitle() failure", hr );
			goto cleanUp;
		}
 
		// reboot - wait for 15s to connect and run title
		hr = DmReboot( DMBOOT_WAIT );
	}
	if ( FAILED( hr ) )
    {
        DmAPI_DisplayError( "lc_Run(): DmReboot() failure", hr );
        goto cleanUp;
    }

	// set reboot state
	g_reboot = true;

	return TRUE;

cleanUp:
	// free args
	for ( int i=0; i<g_rebootArgc; i++ )
		Sys_Free( g_rebootArgv[i] );
	g_rebootArgc = 0;

	return FALSE;
}

//-----------------------------------------------------------------------------
//	lc_reset
//
//-----------------------------------------------------------------------------
BOOL lc_reset( int argc, char* argv[] )
{
	if ( !argc )
	{
		char* args[2] = {"*help", argv[0]};
		lc_help( 2, args );
		return FALSE;
	}

	HRESULT hr = DmReboot( DMBOOT_COLD );

	if ( FAILED( hr ) )
    {
        DmAPI_DisplayError( "lc_Run(): DmReboot() failure", hr );
        return FALSE;
    }

	// set reboot state
	g_reboot = true;

	return TRUE;
}

//-----------------------------------------------------------------------------
//	lc_help
//
//	Handles the "help" command.  If no args, prints a list of built-in
//	and remote commands ( remote only if connected ).  If a command
//	is specified, prints detailed help for that command
//-----------------------------------------------------------------------------
BOOL lc_help( int argc, char* argv[] )
{
    if ( argc <= 1 )
    {
        // no arguments - print out our list of commands
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "\n" );
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Console Commands:\n" );
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "---------------\n" );
        for ( int i = 0; i < g_numLocalCommands; i++ )
        {
            ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%s\n", g_localCommands[i].strCommand );
		}

		if ( g_connectedToApp )
		{
			ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Remote Commands: ( %d )\n", g_numRemoteCommands );
			ConsoleWindowPrintf( XBX_CLR_DEFAULT, "----------------\n" );
 			if ( !g_numRemoteCommands )
			{
				ConsoleWindowPrintf( XBX_CLR_DEFAULT, "( None )\n" );
			}
			else
			{
				for ( int i=0; i<g_numRemoteCommands; i++ )
					ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%s\n", g_remoteCommands[i]->strCommand );
			}
			ConsoleWindowPrintf( XBX_CLR_DEFAULT, "\n" );
		}
    }
    else
    {
		// match as many as possible
        int cch = lstrlenA( argv[1] );

        // print help description for all local matches
        for ( int i=0; i<g_numLocalCommands; i++ )
        {
            if ( !_strnicmp( g_localCommands[i].strCommand, argv[1], cch ) && g_localCommands[i].strHelp )
            {
                ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%s %s\n", g_localCommands[i].strCommand, g_localCommands[i].strHelp );
            }
        }

		// print help description for all remote matches
		if ( g_connectedToApp )
		{
			for ( int i=0; i<g_numRemoteCommands; i++ )
			{
				if ( !_strnicmp( g_remoteCommands[i]->strCommand, argv[1], cch ) && g_remoteCommands[i]->strHelp )
				{
					ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%s %s\n", g_remoteCommands[i]->strCommand, g_remoteCommands[i]->strHelp );
				}
			}
        }
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "\n" );
    }

    return TRUE;
 }

//-----------------------------------------------------------------------------
// lc_cls
//-----------------------------------------------------------------------------
BOOL lc_cls( int argc, char* argv[] )
{
    SetWindowText( g_hwndOutputWindow, "" );

    // non't let the compiler complain about unused parameters
    ( VOID )argc;
    ( VOID )argv;

    return TRUE;
}

//-----------------------------------------------------------------------------
//	lc_connect
//
//	Connect to XBox
//-----------------------------------------------------------------------------
BOOL lc_connect( int argc, char* argv[] )
{
	HRESULT hr;
	BOOL	connected = FALSE;

	if ( g_connectedToXBox )
	{
		if ( !lc_disconnect( 0, NULL ) )
			return FALSE;
	}

	if ( argc >= 1 && argv[0][0] )
	{
		hr = DmSetXboxName( argv[0] );
		if ( FAILED( hr ) )
		{
			char message[255];
			sprintf( message, "ConnectToXBox(): DmSetXboxName( %s ) failure", argv[0] );
			DmAPI_DisplayError( message, hr );
			goto cleanUp;
		}
	}

	// open connection
    hr = DmOpenConnection( &g_pdmConnection );
    if ( FAILED( hr ) )
    {
        DmAPI_DisplayError( "ConnectToXBox(): DmOpenConnection() failure", hr );
        goto cleanUp;
    }
	connected = TRUE;

	DWORD namelen = MAX_XBOXNAMELEN;
	hr = DmGetXboxName( g_xboxName, &namelen );
	if ( FAILED( hr ) )
	{
		DmAPI_DisplayError( "ConnectToXBox(): DmGetXboxName() failure", hr );
		goto cleanUp;
	}
	hr = DmResolveXboxName( &g_xboxAddress );
	if ( FAILED( hr ) )
	{
		DmAPI_DisplayError( "ConnectToXBox(): DmResolveXboxName() failure", hr );
		goto cleanUp;
	}

	// success
	g_connectedToXBox = TRUE;
	g_connectFailure = 0;

	if ( !g_connectCount )
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Connected To: '%s'(%d.%d.%d.%d)\n", g_xboxName, ( ( byte* )&g_xboxAddress )[3], ( ( byte* )&g_xboxAddress )[2], ( ( byte* )&g_xboxAddress )[1], ( ( byte* )&g_xboxAddress )[0] );
	g_connectCount++;

	SetConnectionIcon( ICON_CONNECTED_XBOX );

	if ( g_connectCount == 1 )
	{
		// inital connect
	}

	return TRUE;

cleanUp:
	if ( connected )
		DmCloseConnection( g_pdmConnection );

	return FALSE;
}

//-----------------------------------------------------------------------------
//	lc_listen
//
//	Open listen session with App
//-----------------------------------------------------------------------------
BOOL lc_listen( int argc, char* argv[] )
{
    HRESULT hr;
	BOOL	success;
	BOOL	sessionStarted;
	BOOL	sessionValid;
	char	*args[1];
	char	cmdStr[256];

	if ( g_connectedToXBox || g_connectedToApp )
	{
		if ( !lc_disconnect( 0, NULL ) )
			return ( FALSE );
	}

	if ( !g_connectedToXBox )
	{
		// connect to xbox
		args[0] = g_xboxTargetName;
		if ( !lc_connect( 1, args ) )
			return FALSE;
	}

	// until otherwise
	success        = FALSE;
	sessionStarted = FALSE;
	sessionValid   = FALSE;

	// init session
	hr = DmOpenNotificationSession( 0, &g_pdmnSession );
	if ( FAILED( hr ) )
	{
		DmAPI_DisplayError( "lc_session(): DmOpenNotificationSession() failure", hr );
		goto cleanUp;
	}
	sessionStarted = TRUE;

	// get notifications of app debugging output
	hr = DmNotify( g_pdmnSession, DM_DEBUGSTR, Remote_NotifyDebugString );
	if ( FAILED( hr ) )
	{
		DmAPI_DisplayError( "lc_session(): DmNotify() failure", hr );
		goto cleanUp;
	}

	// get command notifications
	hr = DmRegisterNotificationProcessor( g_pdmnSession, VXCONSOLE_COMMAND_PREFIX, Remote_NotifyCommandFunc );
	if ( FAILED( hr ) )
	{
		DmAPI_DisplayError( "lc_session(): DmRegisterNotificationProcessor() failure", hr );
		goto cleanUp;
	}

	// get print notifications
	hr = DmRegisterNotificationProcessor( g_pdmnSession, VXCONSOLE_PRINT_PREFIX, Remote_NotifyPrintFunc );
	if ( FAILED( hr ) )
	{
		DmAPI_DisplayError( "lc_session(): DmRegisterNotificationProcessor() failure", hr );
		goto cleanUp;
	}
	sessionValid = TRUE;

    // Send initial connect command to the External Command Processor so it knows we're here
	sprintf( cmdStr, "%s %d", VXCONSOLE_COMMAND_PREFIX "!" "__connect__", VXCONSOLE_PROTOCOL_VERSION );
    hr = DmAPI_SendCommand( cmdStr, true );
    if ( FAILED( hr ) )
	{
		if ( !g_autoConnect )
			ConsoleWindowPrintf( RGB( 255,0,0 ), "Couldn't Find Application\n" );
		goto cleanUp;
    }
    else 
    {
		// connected
		success          = TRUE;
        g_connectedToApp = TRUE;
		g_connectedTime  = Sys_GetSystemTime();
		SetConnectionIcon( ICON_CONNECTED_APP1 );

		if ( g_clsOnConnect )
		{
			if ( g_bPlayTestMode )
			{
				// demarcate the log
				ConsoleWindowPrintf( CLR_DEFAULT, "\n******** CONNECTION ********\n" );
			}

			lc_cls( 0, NULL );
			CpuProfile_Clear();
			TimeStampLog_Clear();
		}

		goto cleanUp;
    }

cleanUp:
	if ( !success )
	{
		if ( sessionValid )
			DmNotify( g_pdmnSession, DM_NONE, NULL );

		if ( sessionStarted )
			DmCloseNotificationSession( g_pdmnSession );
	}

	return ( success );
}

//-----------------------------------------------------------------------------
// AutoConnectTimerProc
//
//-----------------------------------------------------------------------------
void AutoConnectTimerProc( HWND hwnd, UINT_PTR idEvent )
{
	static BOOL busy;
	int			icon;
	char*		cmdStr;
	BOOL		bKeepConnection = TRUE;

	// blink the connection icon
	if ( g_connectedToApp && (! g_bSuppressBlink ) )
	{
		if ( g_currentIcon == ICON_CONNECTED_APP0 )
			icon = ICON_CONNECTED_APP1;
		else
			icon = ICON_CONNECTED_APP0;
		SetConnectionIcon( icon );
	}

	if ( busy )
	{
		// not ready for new tick
		return;
	}

	if ( g_bAutoConnectWait > 0 )
	{
		if ( g_bAutoConnectWait && !g_bAutoConnectQuiet )
			ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Waiting... %d seconds remaining\n", g_bAutoConnectWait );
		g_bAutoConnectWait--;
		return;
	}

	// no more ticks until ready
	busy = TRUE;

	if ( !g_connectedToApp )
	{
		// looking for application - must force re-connect every time
		if ( g_connectedToXBox )
		{
			// temporary "partial" disconnect
			DmCloseConnection( g_pdmConnection );
			g_connectedToXBox = FALSE;
		}

		// attempt to start or re-establish connection and session
		lc_listen( 0, NULL );

		if ( !g_connectedToXBox )
		{
			SetConnectionIcon( ICON_DISCONNECTED );
			g_connectFailure++;
		}

		if ( g_reboot && g_connectedToXBox )
		{
			char xexPath[MAX_PATH];
			char xexName[MAX_PATH];
			char xexArgs[MAX_PATH];

			DecodeRebootArgs( g_rebootArgc, g_rebootArgv, xexPath, xexName, xexArgs );

			if ( g_rebootArgc )
			{
				// free args
				for ( int i=0; i<g_rebootArgc; i++ )
					Sys_Free( g_rebootArgv[i] );
				g_rebootArgc = 0;

				HRESULT hr = DmSetTitle( xexPath, xexName, xexArgs );
				if ( FAILED( hr ) )
					DmAPI_DisplayError( "Reboot: DmSetTitle() failure", hr );
				else
				{
					hr = DmGo();
					if ( FAILED( hr ) )
						DmAPI_DisplayError( "Reboot: DmGo() failure", hr );
				}
			}

			g_reboot = false;
		}

		if ( !g_connectFailure )
		{
			// quietly attempt re-connection or ping every 3 seconds
			g_bAutoConnectWait = 3;
			g_bAutoConnectQuiet = TRUE;
			busy = FALSE;
		}
		else
		{
			if ( g_connectFailure == 1 )
			{
				// console may be rebooting, allow sufficient dvd boot up delay, then attempt re-connection
				// 5 seconds barely covers the xbox splash
				g_bAutoConnectWait = 15;
				g_bAutoConnectQuiet = FALSE;
				busy = FALSE;
			}
			else
			{
				// a sustained connection failure means the xbox is just not there
				// re-trying is too cpu intensive and causes pc to appear locked
				// warn and stop auto connecting, user must fix
				bKeepConnection = FALSE;
				goto disconnect;
			}
		}
		return;
	}
	else
	{
		// try to send ping across open connection at an idle interval
		cmdStr = VXCONSOLE_COMMAND_PREFIX "!";
		HRESULT hr = DmAPI_SendCommand( cmdStr, false );
		if ( FAILED( hr ) && hr != XBDM_UNDEFINED )
			goto disconnect;

		// quietly ping
		g_bAutoConnectWait = 3;
		g_bAutoConnectQuiet = TRUE;
		busy = FALSE;
		return;
	}

disconnect:		
	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Connection To Xbox Lost.\n" );

	DoDisconnect( bKeepConnection, 3 );

	busy = FALSE;
}

//-----------------------------------------------------------------------------
//	lc_autoConnect
//-----------------------------------------------------------------------------
BOOL lc_autoConnect( int argc, char* argv[] )
{
	if ( !g_autoConnect )
	{
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Enabling Auto Connect.\n" );
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Looking for Connection...\n" );
		g_autoConnect = TRUE;
	}
	else
	{
		// already enabled
		return ( TRUE );
	}

	if ( !g_autoConnectTimer )
	{
		UINT_PTR timer = TIMERID_AUTOCONNECT;
		g_autoConnectTimer = SetTimer( g_hDlgMain, timer, 1000, NULL );
	}

	return ( TRUE );
}

//-----------------------------------------------------------------------------
//	lc_disconnect
//-----------------------------------------------------------------------------
BOOL lc_disconnect( int argc, char* argv[] )
{
	if ( g_autoConnect )
	{
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Disabling Auto Connect.\n" );
		if ( g_autoConnectTimer )
		{
			UINT_PTR timer = TIMERID_AUTOCONNECT;
			KillTimer( g_hDlgMain, timer );
		}
		g_autoConnectTimer = 0;
		g_autoConnect = FALSE;
	}

	if ( g_connectedToApp )
	{
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Closing Session.\n" );

		DmAPI_SendCommand( VXCONSOLE_COMMAND_PREFIX "!" "__disconnect__", false );
	
		// close session
		DmNotify( g_pdmnSession, DM_NONE, NULL );
		DmCloseNotificationSession( g_pdmnSession );

		g_connectedToApp = FALSE;
	}

	if ( g_connectedToXBox )
	{
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Closing Connection.\n" );

		// close connection
	    DmCloseConnection( g_pdmConnection );

		// set the command ready mutex 
		SetEvent( g_hCommandReadyEvent );

		g_connectedToXBox = FALSE;
		g_xboxName[0] = '\0';
		g_xboxAddress = 0;
	}

	SetConnectionIcon( ICON_DISCONNECTED );

	g_connectCount = 0;

	// free remote commands
	Remote_DeleteCommands();

    // Don't let the compiler complain about unused parameters
    ( VOID )argc;
    ( VOID )argv;

    return TRUE;
}

//-----------------------------------------------------------------------------
//	lc_crashdump
//-----------------------------------------------------------------------------
BOOL lc_crashdump( int argc, char* argv[] )
{
	DmCrashDump();

	// Don't let the compiler complain about unused parameters
	( VOID )argc;
	( VOID )argv;

	return TRUE;
}

//-----------------------------------------------------------------------------
//	lc_quit
//-----------------------------------------------------------------------------
BOOL lc_quit( int argc, char* argv[] )
{
    PostMessage( g_hDlgMain, WM_CLOSE, 0, 0 );

    // don't let the compiler complain about unused parameters
    ( VOID )argc;
    ( VOID )argv;

    return TRUE;
}

//-----------------------------------------------------------------------------
//	lc_ClearConfigs
//
//-----------------------------------------------------------------------------
BOOL lc_ClearConfigs( int argc, char* argv[] )
{
	if ( argc != 1 )
	{
		char* args[2] = {"*help", argv[0]};
		lc_help( 1, args );
		goto cleanUp;
	}

	// delete any configurations (ignore errors)
	char szTempFilename[MAX_PATH];
	V_ComposeFileName( "HDD:\\Content", "*.*", szTempFilename, sizeof( szTempFilename ) );
	char *pArgs[4];
	pArgs[0] = "*del";
	pArgs[1] = szTempFilename;
	pArgs[2] = "/s";
	pArgs[3] = "/q";
	lc_del( 4, pArgs );

	return TRUE;

cleanUp:
	return FALSE;
}
