//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	REMOTE_CMDS.CPP
//
//	Remote commands received by an external application and dispatched.
//=====================================================================================//
#include "vxconsole.h"

remoteCommand_t		*g_remoteCommands[MAX_RCMDS];
int					g_numRemoteCommands;

//-----------------------------------------------------------------------------
//	MatchRemoteCommands
//
//-----------------------------------------------------------------------------
int MatchRemoteCommands( char *pCmdStr, const char *cmdList[], int maxCmds )
{
	int numCommands = 0;

	// look in local
	int matchLen = strlen( pCmdStr );
	for ( int i=0; i<g_numRemoteCommands; i++ )
	{
		if ( !strnicmp( pCmdStr, g_remoteCommands[i]->strCommand, matchLen ) )
		{
			cmdList[numCommands++] = g_remoteCommands[i]->strCommand;
			if ( numCommands >= maxCmds )
				break;
		}
	}

	return ( numCommands );
}

//-----------------------------------------------------------------------------
// GetToken
//
//-----------------------------------------------------------------------------
char *GetToken( char **ppTokenStream )
{
	static char token[MAX_TOKENCHARS];
	int			len;
	char		c;
	char		*pData;

	len = 0;
	token[0] = 0;	

	if ( !ppTokenStream )
		return NULL;

	pData = *ppTokenStream;		

	// skip whitespace
skipwhite:
	while ( ( c = *pData ) <= ' ' )
	{
		if ( !c )
			goto cleanup;
		pData++;
	}
	
	// skip // comments
	if ( c=='/' && pData[1] == '/' )
	{
		while ( *pData && *pData != '\n' )
			pData++;
		goto skipwhite;
	}
	
	// handle quoted strings specially
	if ( c == '\"' )
	{
		pData++;
		while ( 1 )
		{
			c = *pData++;
			if ( c=='\"' || !c )
				goto cleanup;

			token[len] = c;
			len++;
			if ( len > MAX_TOKENCHARS-1 )
				goto cleanup;
		}
	}

	// parse a regular word
	do
	{
		token[len] = c;
		pData++;
		len++;
		if ( len > MAX_TOKENCHARS-1 )
			break;
		c = *pData;
	}
	while ( c > ' ' && c <= '~' );
	
cleanup:
	token[len] = 0;
	*ppTokenStream = pData;
	return ( token );
}

//-----------------------------------------------------------------------------
// CommandCompleted
//
//-----------------------------------------------------------------------------
void CommandCompleted( int errCode )
{
	char	cmdString[MAX_PATH];

	// send command complete
	sprintf( cmdString, "%s!__complete__%d", VXCONSOLE_COMMAND_PREFIX, errCode );
	DmAPI_SendCommand( cmdString, true );
}

//-----------------------------------------------------------------------------
// DebugCommand
//-----------------------------------------------------------------------------
void DebugCommand( const char *pStrFormat, ... )
{
	char	buffer[MAX_QUEUEDSTRINGLEN];
	va_list arglist;

	if ( !g_debugCommands )
		return;

    va_start( arglist, pStrFormat );
    _vsnprintf( buffer, MAX_QUEUEDSTRINGLEN, pStrFormat, arglist );
    va_end( arglist );

	PrintToQueue( RGB( 0,128,0 ), "[CMD]: %s", buffer );
}

//-----------------------------------------------------------------------------
// Remote_NotifyPrintFunc
//
//-----------------------------------------------------------------------------
DWORD __stdcall Remote_NotifyPrintFunc( const CHAR *pStrNotification )
{
	int	color;

	if ( !strnicmp( pStrNotification, VXCONSOLE_PRINT_PREFIX, strlen( VXCONSOLE_PRINT_PREFIX ) ) )
	{
		// skip past prefix!
		pStrNotification += strlen( VXCONSOLE_PRINT_PREFIX )+1;
	}

	color = XBX_CLR_DEFAULT;

	if ( !strnicmp( pStrNotification, VXCONSOLE_COLOR_PREFIX, strlen( VXCONSOLE_COLOR_PREFIX ) ) )
	{
		// skip past prefix[12345678]
		char	buff[16];
		pStrNotification += strlen( VXCONSOLE_COLOR_PREFIX );
		memcpy( buff, pStrNotification, 10 );
		if ( buff[0] == '[' && buff[9] == ']' )
		{
			buff[0] = ' ';
			buff[9] = ' ';
			buff[10] = '\0';

			sscanf( buff, "%x", &color );
			pStrNotification += 10;
		}
	}

    PrintToQueue( color, "%s\n", pStrNotification );
    return S_OK;
}

//-----------------------------------------------------------------------------
// Remote_NotifyDebugString
//
// Print as [DBG]:xxxx
//-----------------------------------------------------------------------------
DWORD __stdcall Remote_NotifyDebugString( ULONG dwNotification, DWORD dwParam )
{
    if ( g_captureDebugSpew )
    {
        PDMN_DEBUGSTR p = ( PDMN_DEBUGSTR )dwParam;
		int len;

		// strip all terminating cr
		len = p->Length-1;
		while ( len > 0 )
		{
			if ( p->String[len] != '\n' )
			{
				len++;
				break;
			}
			len--;
		}

		// for safety, terminate
		CHAR* strTemp = new CHAR[len+1];
		memcpy( strTemp, p->String, len*sizeof( CHAR ) );
		strTemp[len] = '\0';

		PrintToQueue( RGB( 0,0,255 ), "[DBG]: %s\n", strTemp );

		delete[] strTemp;
    }

    // Don't let the compiler complain about unused parameters
    ( VOID )dwNotification;

    return 0;
}

//-----------------------------------------------------------------------------
//	Remote_CompareCommands
//
//-----------------------------------------------------------------------------
int Remote_CompareCommands( const void *pElem1, const void *pElem2 )
{
	remoteCommand_t	*pCmd1;
	remoteCommand_t	*pCmd2;

	pCmd1 = *( remoteCommand_t** )( pElem1 );
	pCmd2 = *( remoteCommand_t** )( pElem2 );

	return ( strcmp( pCmd1->strCommand, pCmd2->strCommand ) );
}

//-----------------------------------------------------------------------------
//	Remote_DeleteCommands
//
//-----------------------------------------------------------------------------
void Remote_DeleteCommands()
{
	if ( !g_numRemoteCommands )
		return;

	for ( int i=0; i<g_numRemoteCommands; i++ )
	{
		delete [] g_remoteCommands[i]->strCommand;
		delete [] g_remoteCommands[i]->strHelp;
		delete g_remoteCommands[i];
	
		g_remoteCommands[i] = NULL;
	}
	
	g_numRemoteCommands = 0;
}

//-----------------------------------------------------------------------------
//	Remote_AddCommand
//
//-----------------------------------------------------------------------------
bool Remote_AddCommand( char *command, char *helptext )
{
	if ( g_numRemoteCommands == MAX_RCMDS )
	{
		// full
		return false;
	}

	// look for duplicate
	int i;
	for ( i = 0; i < g_numRemoteCommands; i++ )
	{
		if ( !stricmp( command, g_remoteCommands[i]->strCommand ) )
			break;
	}
	if ( i < g_numRemoteCommands )
	{
		// already in list, skip - not an error
		return true;
	}

	// add new command to list
	g_remoteCommands[g_numRemoteCommands] = new remoteCommand_t;
	g_remoteCommands[g_numRemoteCommands]->strCommand = new char[strlen( command )+1];
	strcpy( g_remoteCommands[g_numRemoteCommands]->strCommand, command );
		
	g_remoteCommands[g_numRemoteCommands]->strHelp = new char[strlen( helptext )+1];
	strcpy( g_remoteCommands[g_numRemoteCommands]->strHelp, helptext );

	g_numRemoteCommands++;

	// success
	return true;
}

//-----------------------------------------------------------------------------
//	rc_AddCommands
//
//	Exposes an app's list of remote commands
//-----------------------------------------------------------------------------
int rc_AddCommands( char *commandPtr )
{
	char*			cmdToken;
	int				numCommands;
	int				cmdList;
	int				retAddr;
	int				retVal;
	int				errCode;
	xrCommand_t*	locallist;

	errCode = -1;

	// pacifier for lengthy operation
	ConsoleWindowPrintf( RGB( 0, 0, 0 ), "Receiving Console Commands From Game..." );

	// get number of commands
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &numCommands );

	// get command list
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &cmdList );

	// get retAddr
	cmdToken = GetToken( &commandPtr );
	if ( !cmdToken[0] )
		goto cleanUp;
	sscanf( cmdToken, "%x", &retAddr );

	locallist = new xrCommand_t[numCommands];
	memset( locallist, 0, numCommands*sizeof( xrCommand_t ) );

	// get the caller's command list 
	DmGetMemory( ( void* )cmdList, numCommands*sizeof( xrCommand_t ), locallist, NULL );

	int numAdded = 0;
	for ( int i=0; i<numCommands; i++ )
	{
		if ( Remote_AddCommand( locallist[i].nameString, locallist[i].helpString ) )
			numAdded++;
	}

	// sort the list
	qsort( g_remoteCommands, g_numRemoteCommands, sizeof( remoteCommand_t* ), Remote_CompareCommands );

	ConsoleWindowPrintf( RGB( 0, 0, 0 ), "Completed.\n" );

	// return the result
	retVal = numAdded;
	int xboxRetVal = BigDWord( retVal );
	DmSetMemory( ( void* )retAddr, sizeof( int ), &xboxRetVal, NULL );

	DebugCommand( "0x%8.8x = AddCommands( 0x%8.8x, 0x%8.8x )\n", retVal, numCommands, cmdList );

	delete [] locallist;

	// success
	errCode = 0;

	if ( g_bPlayTestMode )
	{
		if ( g_connectedToApp )
		{
			// send the developer command
			ProcessCommand( "developer 1" );
		}
	}

cleanUp:	
	return ( errCode );
}

//-----------------------------------------------------------------------------
// Remote_NotifyCommandFunc
//
//-----------------------------------------------------------------------------
DWORD __stdcall Remote_NotifyCommandFunc( const CHAR *strNotification )
{
	CHAR*	commandPtr;
	CHAR*	cmdToken;
	int		errCode;
	bool	async;

	// skip over the command prefix and the exclamation mark
    strNotification += strlen( VXCONSOLE_COMMAND_PREFIX ) + 1;
	commandPtr = ( CHAR* )strNotification;	

	// failure until otherwise
	errCode = -1;

	// default synchronous
	async = false;

	cmdToken = GetToken( &commandPtr );

	if ( cmdToken && !stricmp( cmdToken, "AddCommands()" ) )
	{
		errCode = rc_AddCommands( commandPtr );
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "SetProfile()" ) )
	{
		// first arg dictates routing
		cmdToken = GetToken( &commandPtr );
		if ( cmdToken && !stricmp( cmdToken, "cpu" ) )
			errCode = rc_SetCpuProfile( commandPtr );
		else if ( cmdToken && !stricmp( cmdToken, "texture" ) )
			errCode = rc_SetTexProfile( commandPtr );

		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "SetProfileData()" ) )
	{
		// first arg dictates routing
		cmdToken = GetToken( &commandPtr );
		if ( cmdToken && !stricmp( cmdToken, "cpu" ) )
			errCode = rc_SetCpuProfileData( commandPtr );
		else if ( cmdToken && !stricmp( cmdToken, "texture" ) )
			errCode = rc_SetTexProfileData( commandPtr );

		async = true;
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "TextureList()" ) )
	{
		errCode = rc_TextureList( commandPtr );
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "MaterialList()" ) )
	{
		errCode = rc_MaterialList( commandPtr );
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "SoundList()" ) )
	{
		errCode = rc_SoundList( commandPtr );
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "TimeStampLog()" ) )
	{
		errCode = rc_TimeStampLog( commandPtr );
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "MemDump()" ) )
	{
		errCode = rc_MemDump( commandPtr );
		async = true;
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "MapInfo()" ) )
	{
		errCode = rc_MapInfo( commandPtr );
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "Assert()" ) )
	{
		errCode = rc_Assert( commandPtr );
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "FreeMemory()" ) )
	{
		errCode = rc_FreeMemory( commandPtr );
		async = true;
		goto cleanUp;
	}
	else if ( cmdToken && !stricmp( cmdToken, "Disconnect()" ) )
	{
		// disconnect requires specialized processing
		// send command status first while connection valid, then do actual disconnect
		// disconnect is always assumed to be valid, can't be denied
		CommandCompleted( 0 );
		DoDisconnect( TRUE );
		return S_OK;
	}
	else
	{
		// unknown command
	    PrintToQueue( RGB( 255,0,0 ), "Unknown Command: %s\n", strNotification );
		goto cleanUp;
	}

cleanUp:
	if ( !async )
		CommandCompleted( errCode );

	return ( S_OK );
}
