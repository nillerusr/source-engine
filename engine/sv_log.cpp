//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "server_pch.h"
#include <time.h>
#include "server.h"
#include "sv_log.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "tier0/vcrmode.h"
#include "sv_main.h"
#include "tier0/icommandline.h"
#include <proto_oob.h>
#include "GameEventManager.h"
#include "netadr.h"
#include "zlib/zlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar sv_logsdir( "sv_logsdir", "logs", FCVAR_ARCHIVE, "Folder in the game directory where server logs will be stored." );
static ConVar sv_logfile( "sv_logfile", "1", FCVAR_ARCHIVE, "Log server information in the log file." );
static ConVar sv_logflush( "sv_logflush", "0", FCVAR_ARCHIVE, "Flush the log file to disk on each write (slow)." );
static ConVar sv_logecho( "sv_logecho", "1", FCVAR_ARCHIVE, "Echo log information to the console." );
static ConVar sv_log_onefile( "sv_log_onefile", "0", FCVAR_ARCHIVE, "Log server information to only one file." );
static ConVar sv_logbans( "sv_logbans", "0", FCVAR_ARCHIVE, "Log server bans in the server logs." ); // should sv_banid() calls be logged in the server logs?
static ConVar sv_logsecret( "sv_logsecret", "0", 0, "If set then include this secret when doing UDP logging (will use 0x53 as packet type, not usual 0x52)" ); 

static ConVar sv_logfilename_format( "sv_logfilename_format", "", FCVAR_ARCHIVE, "Log filename format. See strftime for formatting codes." );
static ConVar sv_logfilecompress( "sv_logfilecompress", "0", FCVAR_ARCHIVE, "Gzip compress logfile and rename to logfilename.log.gz on close." );

CLog g_Log;	// global Log object

CON_COMMAND( log, "Enables logging to file, console, and udp < on | off >." )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  log < on | off >\n" );

		if ( g_Log.IsActive() )
		{
			bool bHaveFirst = false;

			ConMsg( "currently logging to: " );

			if ( sv_logfile.GetInt() )
			{
				ConMsg( "file" );
				bHaveFirst = true;
			}

			if ( sv_logecho.GetInt() )
			{
				if ( bHaveFirst )
				{
					ConMsg( ", console" );
				}
				else
				{
					ConMsg( "console" );
					bHaveFirst = true;
				}
			}

			if ( g_Log.UsingLogAddress() )
			{
				if ( bHaveFirst )
				{
					ConMsg( ", udp" );
				}
				else
				{
					ConMsg( "udp" );
					bHaveFirst = true;
				}
			}
	
			if ( !bHaveFirst )
			{
				ConMsg( "no destinations! (file, console, or udp)\n" );	
				ConMsg( "check \"sv_logfile\", \"sv_logecho\", and \"logaddress_list\"" );	
			}

			ConMsg( "\n" );
		}
		else 
		{
			ConMsg( "not currently logging\n" );
		}
		return;
	}

	if ( !Q_stricmp( args[1], "off" ) || !Q_stricmp( args[1], "0" ) )
	{
		if ( g_Log.IsActive() )
		{
			g_Log.Close();
			g_Log.SetLoggingState( false );
			ConMsg( "Server logging disabled.\n" );
		}
	}
	else if ( !Q_stricmp( args[1], "on" ) || !Q_stricmp( args[1], "1" ) )
	{
		g_Log.SetLoggingState( true );
		ConMsg( "Server logging enabled.\n" );
		g_Log.Open();
	}
	else
	{
		ConMsg( "log:  unknown parameter %s, 'on' and 'off' are valid\n", args[1] );
	}
}

// changed log_addaddress back to logaddress_add to be consistent with GoldSrc
CON_COMMAND( logaddress_add, "Set address and port for remote host <ip:port>." )
{
	netadr_t adr;
	const char *pszIP, *pszPort;

	if ( args.ArgC() != 4 && args.ArgC() != 2 )
	{
		ConMsg( "Usage:  logaddress_add ip:port\n" );
		return;
	}

	pszIP = args[1];

	if ( args.ArgC() == 4 )
	{
		pszPort	= args[3];
	}
	else
	{
		pszPort = Q_strstr( pszIP, ":" );

		// if we have "IP:port" as one argument inside quotes
		if ( pszPort )
		{
			// add one to remove the :
			pszPort++;
		}
		else
		{
			// default port
			pszPort = "27015";
		}
	}

	if ( !Q_atoi( pszPort ) )
	{
		ConMsg( "logaddress_add:  must specify a valid port\n" );
		return;
	}

	if ( !pszIP || !pszIP[0] )
	{
		ConMsg( "logaddress_add:  unparseable address\n" );
		return;
	}

	char szAdr[32];
	Q_snprintf( szAdr, sizeof( szAdr ), "%s:%s", pszIP, pszPort );

	if ( NET_StringToAdr( szAdr, &adr ) )
	{
		if ( g_Log.AddLogAddress( adr ) )
		{
			ConMsg( "logaddress_add:  %s\n", adr.ToString() );
		}
		else
		{
			ConMsg( "logaddress_add:  %s is already in the list\n", adr.ToString() );
		}
	}
	else
	{
		ConMsg( "logaddress_add:  unable to resolve %s\n", szAdr );
	}
}

CON_COMMAND( logaddress_delall, "Remove all udp addresses being logged to" )
{
	g_Log.DelAllLogAddress();
}

CON_COMMAND( logaddress_del, "Remove address and port for remote host <ip:port>." )
{
	netadr_t adr;
	const char *pszIP, *pszPort;

	if ( args.ArgC() != 4 && args.ArgC() != 2 )
	{
		ConMsg( "Usage:  logaddress_del ip:port\n" );
		return;
	}

	pszIP = args[1];

	if ( args.ArgC() == 4 )
	{
		pszPort	= args[3];
	}
	else
	{
		pszPort = Q_strstr( pszIP, ":" );

		// if we have "IP:port" as one argument inside quotes
		if ( pszPort )
		{
			// add one to remove the :
			pszPort++;
		}
		else
		{
			// default port
			pszPort = "27015";
		}
	}

	if ( !Q_atoi( pszPort ) )
	{
		ConMsg( "logaddress_del:  must specify a valid port\n" );
		return;
	}

	if ( !pszIP || !pszIP[0] )
	{
		ConMsg( "logaddress_del:  unparseable address\n" );
		return;
	}

	char szAdr[32];
	Q_snprintf( szAdr, sizeof( szAdr ), "%s:%s", pszIP, pszPort );

	if ( NET_StringToAdr( szAdr, &adr ) )
	{
		if ( g_Log.DelLogAddress( adr ) )
		{
			ConMsg( "logaddress_del:  %s\n", adr.ToString() );
		}
		else
		{
			ConMsg( "logaddress_del:  address %s not found in the list\n", adr.ToString() );
		}
	}
	else
	{
		ConMsg( "logaddress_del:  unable to resolve %s\n", szAdr );
	}
}

CON_COMMAND( logaddress_list, "List all addresses currently being used by logaddress." )
{
	g_Log.ListLogAddress();
}

CLog::CLog()
{
	Reset();
}

CLog::~CLog()
{

}

void CLog::Reset( void )	// reset all logging streams
{
	m_LogAddresses.RemoveAll();

	m_hLogFile = FILESYSTEM_INVALID_HANDLE;
	m_LogFilename = NULL;

	m_bActive = false;
	m_flLastLogFlush = realtime;
	m_bFlushLog = false;
#ifndef _XBOX
	if ( CommandLine()->CheckParm( "-flushlog" ) )
	{
		m_bFlushLog = true;
	}
#endif
}

void CLog::Init( void )
{
	Reset();

	// listen to these events
	g_GameEventManager.AddListener( this, "server_spawn", true );
	g_GameEventManager.AddListener( this, "server_shutdown", true );
	g_GameEventManager.AddListener( this, "server_cvar", true );
	g_GameEventManager.AddListener( this, "server_message", true );
	g_GameEventManager.AddListener( this, "server_addban", true );
	g_GameEventManager.AddListener( this, "server_removeban", true );
}

void CLog::Shutdown()
{
	Close();
	Reset();
	g_GameEventManager.RemoveListener( this );
}

void CLog::SetLoggingState( bool state )
{
	m_bActive = state;
}

void CLog::RunFrame() 
{
	if ( m_bFlushLog && m_hLogFile != FILESYSTEM_INVALID_HANDLE && ( realtime - m_flLastLogFlush ) > 1.0f )
	{
		m_flLastLogFlush = realtime;
		g_pFileSystem->Flush( m_hLogFile );
	}
}

bool CLog::AddLogAddress(netadr_t addr)
{
	int i = 0;
	
	for ( i = 0; i < m_LogAddresses.Count(); ++i )
	{
		if ( m_LogAddresses.Element(i).CompareAdr(addr, false) )
		{
			// found!
			break;
		}
	}

	if ( i < m_LogAddresses.Count() )
	{
		// already in the list
		return false;
	}

	m_LogAddresses.AddToTail( addr );
	return true;
}

bool CLog::DelLogAddress(netadr_t addr)
{
	int i = 0;
	
	for ( i = 0; i < m_LogAddresses.Count(); ++i )
	{
		if ( m_LogAddresses.Element(i).CompareAdr(addr, false) )
		{
			// found!
			break;
		}
	}

	if ( i < m_LogAddresses.Count() )
	{
		m_LogAddresses.Remove(i);
		return true;
	}

	return false;
}

void CLog::ListLogAddress( void )
{
	netadr_t *pElement;
	const char *pszAdr;
	int count = m_LogAddresses.Count();

	if ( count <= 0 )
	{
		ConMsg( "logaddress_list:  no addresses in the list\n" );
	}
	else
	{
		if ( count == 1 )
		{
			ConMsg( "logaddress_list: %i entry\n", count );
		}
		else
		{
			ConMsg( "logaddress_list: %i entries\n", count );
		}

		for ( int i = 0 ; i < count ; ++i )
		{
			pElement = &m_LogAddresses.Element(i);
			pszAdr = pElement->ToString();
			
			ConMsg( "%s\n", pszAdr );
		}
	}
}

bool CLog::UsingLogAddress( void )
{
	return ( m_LogAddresses.Count() > 0 );
}

void CLog::DelAllLogAddress( void )
{
	if ( m_LogAddresses.Count() > 0 )
	{
		ConMsg( "logaddress_delall:  all addresses cleared\n" );
		m_LogAddresses.RemoveAll();
	}
	else
	{
		ConMsg( "logaddress_delall:  no addresses in the list\n" );
	}
}

/*
==================
Log_PrintServerVars

==================
*/
void CLog::PrintServerVars( void )
{
	const ConCommandBase	*var;			// Temporary Pointer to cvars

	if ( !IsActive() )
	{
		return;
	}

	Printf( "server cvars start\n" );
	// Loop through cvars...
	for ( var= g_pCVar->GetCommands() ; var ; var=var->GetNext() )
	{
		if ( var->IsCommand() )
			continue;

		if ( !( var->IsFlagSet( FCVAR_NOTIFY ) ) )
			continue;

		Printf( "\"%s\" = \"%s\"\n", var->GetName(), ((ConVar*)var)->GetString() );
	}

	Printf( "server cvars end\n" );
}

bool CLog::IsActive( void )
{
	return m_bActive;
}

/*
==================
Log_Printf

Prints a log message to the server's log file, console, and possible a UDP address
==================
*/
void CLog::Printf( const char *fmt, ... )
{
	va_list			argptr;
	static char		string[1024];
	
	if ( !IsActive() )
	{
		return;
	}

	va_start ( argptr, fmt );
	Q_vsnprintf ( string, sizeof( string ), fmt, argptr );
	va_end   ( argptr );

	Print( string );
}

void CLog::Print( const char * text )
{
	if ( !IsActive() || !text || !text[0] )
	{
		return;
	}

	tm today;
	VCRHook_LocalTime( &today );

	if ( Q_strlen( text ) > 1024 )
	{
		// Spew a warning, but continue and print the truncated stuff we have.
		DevMsg( 1, "CLog::Print: string too long (>1024 bytes)." );
	}

	static char	string[1100];
	V_sprintf_safe( string, "L %02i/%02i/%04i - %02i:%02i:%02i: %s",
		today.tm_mon+1, today.tm_mday, 1900 + today.tm_year,
		today.tm_hour, today.tm_min, today.tm_sec, text );

	// Echo to server console
	if ( sv_logecho.GetInt() ) 
	{
		ConMsg( "%s", string );
	}

	// Echo to log file
	if ( sv_logfile.GetInt() && ( m_hLogFile != FILESYSTEM_INVALID_HANDLE ) )
	{
		g_pFileSystem->FPrintf( m_hLogFile, "%s", string );
		if ( sv_logflush.GetBool() )
		{
			g_pFileSystem->Flush( m_hLogFile );
		}
	}

	// Echo to UDP port
	if ( m_LogAddresses.Count() > 0 )
	{
		// out of band sending
		for ( int i = 0 ; i < m_LogAddresses.Count() ; i++ )
		{
			if ( sv_logsecret.GetInt() != 0 )
				NET_OutOfBandPrintf(NS_SERVER, m_LogAddresses.Element(i), "%c%s%s", S2A_LOGSTRING2, sv_logsecret.GetString(), string );
			else
				NET_OutOfBandPrintf(NS_SERVER, m_LogAddresses.Element(i), "%c%s", S2A_LOGSTRING, string );
			
		}
	}
}

void CLog::FireGameEvent( IGameEvent *event )
{
	if ( !IsActive() )
		return;

	// log server events

	const char * name = event->GetName();
	if ( !name || !name[0])
		return;

	if ( Q_strcmp(name, "server_spawn") == 0 )
	{
		Printf( "Started map \"%s\" (CRC \"%s\")\n", sv.GetMapName(), MD5_Print( sv.worldmapMD5.bits, MD5_DIGEST_LENGTH ) );
	}

	else if ( Q_strcmp(name, "server_shutdown") == 0 )
	{
		Printf( "server_message: \"%s\"\n", event->GetString("reason") );
	}

	else if ( Q_strcmp(name, "server_cvar") == 0 )
	{
		Printf( "server_cvar: \"%s\" \"%s\"\n", event->GetString("cvarname"), event->GetString("cvarvalue")  );
	}

	else if ( Q_strcmp(name, "server_message") == 0 )
	{
		Printf( "server_message: \"%s\"\n", event->GetString("text") );
	}
	
	else if ( Q_strcmp(name, "server_addban") == 0 )
	{
		if ( sv_logbans.GetInt() > 0 )
		{
			const int userid = event->GetInt( "userid" );
			const char *pszName = event->GetString( "name" );
			const char *pszNetworkid = event->GetString( "networkid" );	
			const char *pszIP = event->GetString( "ip" );	
			const char *pszDuration = event->GetString( "duration" );
			const char *pszCmdGiver = event->GetString( "by" );
			const char *pszResult = NULL;
			
			if ( Q_strlen( pszIP ) > 0 )
			{
				pszResult = event->GetInt( "kicked" ) > 0 ? "was kicked and banned by IP" : "was banned by IP";

				if ( userid > 0 )
				{
					Printf( "Addip: \"%s<%i><%s><>\" %s \"%s\" by \"%s\" (IP \"%s\")\n", 
										pszName,
										userid, 
										pszNetworkid, 
										pszResult,
										pszDuration,
										pszCmdGiver,
										pszIP );
				}
				else
				{
					Printf( "Addip: \"<><><>\" %s \"%s\" by \"%s\" (IP \"%s\")\n", 
										pszResult,
										pszDuration,
										pszCmdGiver,
										pszIP );
				}
			}
			else
			{
				pszResult = event->GetInt( "kicked" ) > 0 ? "was kicked and banned" : "was banned";

				if ( userid > 0 )
				{
					Printf( "Banid: \"%s<%i><%s><>\" %s \"%s\" by \"%s\"\n", 
										pszName,
										userid, 
										pszNetworkid, 
										pszResult,
										pszDuration,
										pszCmdGiver );
				}
				else
				{
					Printf( "Banid: \"<><%s><>\" %s \"%s\" by \"%s\"\n", 
										pszNetworkid, 
										pszResult,
										pszDuration,
										pszCmdGiver );
				}
			}
		}
	}

	else if ( Q_strcmp(name, "server_removeban") == 0 )
	{
		if ( sv_logbans.GetInt() > 0 )
		{
			const char *pszNetworkid = event->GetString( "networkid" );	
			const char *pszIP = event->GetString( "ip" );	
			const char *pszCmdGiver = event->GetString( "by" );
			
			if ( Q_strlen( pszIP ) > 0 )
			{
				Printf( "Removeip: \"<><><>\" was unbanned by \"%s\" (IP \"%s\")\n", 
								pszCmdGiver,
								pszIP );
			}
			else
			{
				Printf( "Removeid: \"<><%s><>\" was unbanned by \"%s\"\n", 
								pszNetworkid, 
								pszCmdGiver );
			}
		}		
	}
}

struct TempFilename_t
{
	bool IsGzip;
	CUtlString Filename;
	union
	{
		FileHandle_t file;
		gzFile gzfile;
	} fh;
};

// Given a base filename and an extension, try to find a file that doesn't exist which we can use. This is
//  accomplished by appending 000, 001, etc. Set IsGzip to use gzopen instead of filesystem open.
static bool CreateTempFilename( TempFilename_t &info, const char *filenameBase, const char *ext, bool IsGzip )
{
	// Check if a logfilename format has been specified - if it has, kick in new behavior.
	const char *logfilename_format = sv_logfilename_format.GetString();
	bool bHaveLogfilenameFormat = logfilename_format && logfilename_format[ 0 ];

	info.fh.file = NULL;
	info.fh.gzfile = 0;
	info.IsGzip = IsGzip;

	CUtlString fname = CUtlString( filenameBase ).StripExtension();

	for ( int i = 0; i < 1000; i++ )
	{
		if ( bHaveLogfilenameFormat )
		{
			// For the first pass, let's try not adding the index.
			if ( i == 0 )
				info.Filename.Format( "%s.%s", fname.Get(), ext );
			else
				info.Filename.Format( "%s_%03i.%s", fname.Get(), i - 1, ext );
		}
		else
		{
			info.Filename.Format( "%s%03i.%s", fname.Get(), i, ext );
		}

		// Make sure the path exists.
		info.Filename.FixSlashes();
		COM_CreatePath( info.Filename );

		if ( !g_pFileSystem->FileExists( info.Filename, "LOGDIR" ) )
		{
			// If the path doesn't exist, try opening the file. If that succeeded, return our filehandle and filename.
			if ( !IsGzip )
			{
				info.fh.file = g_pFileSystem->Open( info.Filename, "wt", "LOGDIR" );
				if ( info.fh.file )
					return true;
			}
			else
			{
				info.fh.gzfile = gzopen( info.Filename, "wb6" );
				if ( info.fh.gzfile )
					return true;
			}
		}
	}

	info.Filename = NULL;
	return false;
}

// Gzip Filename to Filename.gz.
static bool gzip_file_compress( const CUtlString &Filename )
{
	bool bRet = false;

	// Try to find a unique temp filename.
	TempFilename_t info;
	bRet = CreateTempFilename( info, Filename, "log.gz", true );
	if ( !bRet )
		return false;

	Msg( "Compressing %s to %s...\n", Filename.Get(), info.Filename.Get() );

	FILE *in = fopen( Filename, "rb" );
	if ( in )
	{
		for (;;)
		{
			char buf[ 16384 ];
			size_t len = fread( buf, 1, sizeof( buf ), in );
			if ( ferror( in ) )
			{
				Msg( "%s: fread failed.\n", __FUNCTION__ );
				break;
			}
			if (len == 0)
			{
				bRet = true;
				break;
			}

			if ( (size_t)gzwrite( info.fh.gzfile, buf, len ) != len )
			{
				Msg( "%s: gzwrite failed.\n", __FUNCTION__ );
				break;
			}
		}

		if ( gzclose( info.fh.gzfile ) != Z_OK )
		{
			Msg( "%s: gzclose failed.\n", __FUNCTION__ );
			bRet = false;
		}

		fclose( in );
	}

	return bRet;
}

static void FixupInvalidPathChars( char *filename )
{
	if ( !filename )
		return;

	for ( ; filename[ 0 ]; filename++ )
	{
		switch ( filename[ 0 ] )
		{
		case ':':
		case '\n':
		case '\r':
		case '\t':
		case '.':
		case '\\':
		case '/':
			filename[ 0 ] = '_';
			break;
		}
	}
}

/*
====================
Log_Close

Close logging file
====================
*/
void CLog::Close( void )
{
	if ( m_hLogFile != FILESYSTEM_INVALID_HANDLE )
	{
		Printf( "Log file closed.\n" );
		g_pFileSystem->Close( m_hLogFile );

		if ( sv_logfilecompress.GetBool() )
		{
			// Try to compress m_LogFilename to m_LogFilename.gz.
			if ( gzip_file_compress( m_LogFilename ) )
			{
				Msg( "  Success. Removing %s.\n", m_LogFilename.Get() );
				g_pFileSystem->RemoveFile( m_LogFilename, "LOGDIR" );
			}
		}
	}

	m_hLogFile = FILESYSTEM_INVALID_HANDLE;
	m_LogFilename = NULL;
}

/*
====================
Log_Flush

Flushes the log file to disk
====================
*/
void CLog::Flush( void )
{
	if ( m_hLogFile != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Flush( m_hLogFile );
	}
}

/*
====================
Log_Open

Open logging file
====================
*/
void CLog::Open( void )
{
	if ( !m_bActive || !sv_logfile.GetBool() )
		return;

	// Do we already have a log file (and we only want one)?
	if ( m_hLogFile && sv_log_onefile.GetBool() )
		return;

	Close();

	// Find a new log file slot.
	tm today;
	VCRHook_LocalTime( &today );

	// safety check for invalid paths
	const char *pszLogsDir = sv_logsdir.GetString();
	if ( !COM_IsValidPath( pszLogsDir ) )
		pszLogsDir = "logs";

	// Get the logfilename format string.
	char szLogFilename[ MAX_OSPATH ];
	szLogFilename[ 0 ] = 0;

	const char *logfilename_format = sv_logfilename_format.GetString();
	if ( logfilename_format && logfilename_format[ 0 ] )
	{
		// Call strftime with the logfilename format.
		strftime( szLogFilename, sizeof( szLogFilename ), logfilename_format, &today );

		// Make sure it's nil terminated.
		szLogFilename[ sizeof( szLogFilename ) - 1 ] = 0;

		// Trim any leading and trailing whitespace.
		Q_AggressiveStripPrecedingAndTrailingWhitespace( szLogFilename );
	}
	if ( !szLogFilename[ 0 ] )
	{
		// If we got nothing, default to old month / day of month behavior.
		V_sprintf_safe( szLogFilename, "L%02i%02i", today.tm_mon + 1, today.tm_mday );
	}

	// Replace any screwy characters with underscores.
	FixupInvalidPathChars( szLogFilename );

	char szFileBase[ MAX_OSPATH ];
	V_sprintf_safe( szFileBase, "%s/%s", pszLogsDir, szLogFilename );

	// Try to get a free file.
	TempFilename_t info;
	if ( !CreateTempFilename( info, szFileBase, "log", false ) )
	{
		ConMsg( "Unable to open logfiles under %s\nLogging disabled\n", szFileBase );
		return;
	}

	m_hLogFile = info.fh.file;
	m_LogFilename = info.Filename;

	ConMsg( "Server logging data to file %s\n", m_LogFilename.Get() );
	Printf( "Log file started (file \"%s\") (game \"%s\") (version \"%i\")\n", m_LogFilename.Get(), com_gamedir, build_number() );
}
