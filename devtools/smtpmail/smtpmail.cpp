//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// SMTP RFC 821 Specifies <CR><LF> to terminate lines.
//
// <CR> = Carriage Return			C: \r	Dec: 13 Hex: 0x0d Oct: 0015
// <LF> = Line Feed	( Newline )		C: \n	Dec: 10 Hex: 0x0a Oct: 0012
//
//=============================================================================//
#pragma warning( disable : 4530 )

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "simplesocket.h"

static int g_ArgCount;
static char **g_Args;

//-----------------------------------------------------------------------------
// Purpose: simple args API
// Input  : argc - 
//			*argv[] - 
//-----------------------------------------------------------------------------
void ArgsInit( int argc, char *argv[] )
{
	g_ArgCount = argc;
	g_Args = argv;
}


//-----------------------------------------------------------------------------
// Purpose: tests for the presence of arg "pName"
// Input  : *pName - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ArgsExist( const char *pName )
{
	int i;

	if ( pName && pName[0] == '-' )
	{
		for ( i = 0; i < g_ArgCount; i++ )
		{
			// Is this a switch?
			if ( g_Args[i][0] != '-' )
				continue;

			if ( !stricmp( pName, g_Args[i] ) )
			{
				return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: looks for the arg "pName" and returns it's parameter or pDefault otherwise
// Input  : *pName - 
//			*pDefault - 
// Output : const char *
//-----------------------------------------------------------------------------
const char *ArgsGet( const char *pName, const char *pDefault )
{
	int i;

	// don't bother with the last arg, it can't be a switch with a parameter
	for ( i = 0; i < g_ArgCount-1; i++ )
	{
		// Is this a switch?
		if ( g_Args[i][0] != '-' )
			continue;

		if ( !stricmp( pName, g_Args[i] ) )
		{
			// If the next arg is not a switch, return it
			if ( g_Args[i+1][0] != '-' )
				return g_Args[i+1];
		}
	}

	return pDefault;
}


void Error( const char *pString, ... )
{
	va_list list;
	va_start( list, pString );
	vprintf( pString, list );
	printf("Usage: smtpmail -to <address(-es, ';' separated)> -from <address> -subject \"subject text\" [-verbose] [-server server.name] <FILENAME.TXT>\n" );
	exit( 1 );
}


//-----------------------------------------------------------------------------
// Purpose: simple routine to printf() all of the socket traffic for -verbose
// Input  : socket - 
//			*pData - 
//-----------------------------------------------------------------------------
void DumpSocket( HSOCKET, const char *pData )
{
	printf( "%s", pData );
	fflush( stdout );
}


static char *months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

//-----------------------------------------------------------------------------
// Purpose: mail a text file using the SMTP mail server connected to socket
// Input  : socket - 
//			*pFrom - from address
//			*pTo -  to address
//			*pSubject - subject of the mail
//			*fp - text mode file opened
//-----------------------------------------------------------------------------
void MailSendFile( HSOCKET socket, const char *pFrom, const char *pTo[], const char *pSubject, FILE *fp )
{
	int i;
	char buf[1024];

	SocketSendString( socket, "HELO\r\n" );
	SocketWait( socket, "\n" );
	sprintf( buf, "MAIL FROM: <%s>\r\n", pFrom );
	SocketSendString( socket, buf );
	SocketWait( socket, "\n" );
	for (i = 0; pTo[i] != NULL; i++)
	{
		sprintf( buf, "RCPT TO: <%s>\r\n", pTo[i] );
		SocketSendString( socket, buf );
		SocketWait( socket, "\n" );
	}
	SocketSendString( socket, "DATA\r\n" );
	SocketWait( socket, "\n" );

	time_t currentTime;
	time( &currentTime );
	struct tm *localTime = gmtime( &currentTime );
	
	sprintf( buf, "DATE: %02d %s %4d %02d:%02d:%02d\r\n", localTime->tm_mday, months[localTime->tm_mon], localTime->tm_year+1900,
			localTime->tm_hour, localTime->tm_min, localTime->tm_sec );

	SocketSendString( socket, buf );
	sprintf( buf, "FROM: %s\r\n", pFrom );
	SocketSendString( socket, buf );
	for (i = 0; pTo[i] != NULL; i++)
	{
		sprintf( buf, "TO: %s\r\n", pTo[i] );
		SocketSendString( socket, buf );
	}
	sprintf( buf, "SUBJECT: %s\r\n\r\n", pSubject );
	SocketSendString( socket, buf );

	while ( !feof( fp ) )
	{
		fgets( buf, 1024, fp );
		
		// A period on a line by itself would end the transmission
		if ( !strcmp( buf, ".\n" ) && !strcmp( buf, ".\r\n" ) )
			continue;

		SocketSendString( socket, buf );
	}
	SocketSendString( socket, "\r\n.\r\n" );
	SocketWait( socket, "\n" );
}



int main( int argc, char *argv[] )
{

	ArgsInit( argc, argv );

	const char *pServerName = ArgsGet( "-server", "smtp1.valvesoftware.com" );
	
	const char *pTo[100];
	pTo[0] = ArgsGet( "-to", NULL );
	if ( !pTo[0] )
	{
		Error( "Must specify a recipient with -to <address(es)>\n" );
	}

	// split ; separated To string into an array of strings
	int c = 0;
	char *where;
	while (((where = (char *)strchr(pTo[c++],';')) != NULL) && c < 99)
	{
		*(where++) = '\0';
		pTo[c] = where;
	}
	pTo[c] = NULL;

	const char *pFrom = ArgsGet( "-from", NULL );
	if ( !pFrom )
	{
		Error( "Must specify a sender with -from <address>\n" );
	}
	const char *pSubject = ArgsGet( "-subject", "<NONE>" );
	int port = atoi( ArgsGet( "-port", "25" ) );

	const char *pFileName = argv[ argc-1 ];

	FILE *fp = fopen( pFileName, "r" );

	if ( !fp )
	{
		Error( "Can't open %s\n", pFileName );
	}

	SocketInit();

	// in verbose mode, printf all of the traffic
	if ( ArgsExist( "-verbose" ) )
	{
		SocketReport( DumpSocket );
	}

	HSOCKET socket = SocketOpen( pServerName, port );
	if ( socket )
	{
		SocketWait( socket, "\n" );
		MailSendFile( socket, pFrom, pTo, pSubject, fp );
		SocketSendString( socket, "quit\r\n" );
		SocketWait( socket, "\n" );
		SocketClose( socket );
	}
	else
	{
		printf("Can't open socket to %s:%d\n", pServerName, port );
	}
	
	fclose( fp );

	SocketExit();
	return 0;
}
