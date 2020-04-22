//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <stdio.h>
#include <windows.h>
#include "tier0/dbg.h"
#include "utldict.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "cmdlib.h"
#include "interface.h"
#include "imysqlwrapper.h"
#include "../bugreporter/trktool.h"
#include "utlbuffer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>

bool uselogfile = false;

static bool spewed = false;

static char workingdir[ 256 ];

#define SCR_TYPE 1

#define DEFAULT_DBMS		"tracker"
#define DEFAULT_USERNAME	"PublicUser"
#define DEFAULT_PASSWORD	DEFAULT_USERNAME

#define BUG_REPOSITORY_FMT		"\\\\fileserver\\bugs\\public\\%s\\%s\\%s.zip"

struct BugField_t
{
	BugField_t()
	{
		value[ 0 ] = 0;
		numvalue = 0;
		trkfield[ 0 ] = 0;
		isdesc = false;
		isnumeric = false;
	}

	char	value[ 8192 ];
	int		numvalue;
	char	trkfield[ 256 ];
	bool	isdesc;
	bool	isnumeric;
};

#define SQL_SETVALUE( fieldname, sqlcolumn )	Q_strncpy( bug.m_pBug->fieldname, sqlcolumn.String(), sizeof( bug.m_pBug->fieldname ) );
#define SQL_SETVALUESTRING( fieldname, str )	Q_strncpy( bug.m_pBug->fieldname, str, sizeof( bug.m_pBug->fieldname ) );


class CBugReporter
{
public:

	CBugReporter();
	virtual ~CBugReporter();

	// Initialize and login with default username/password for this computer (from resource/bugreporter.res)
	virtual bool		Init( char const *projectname );
	virtual void		Shutdown();

	virtual bool		IsPublicUI() { return false; }

// Submission API
	virtual void		StartNewBugReport();
	virtual void		CancelNewBugReport();
	virtual bool		CommitBugReport( int& bugSubmissionId );

	virtual void		AddField( char const *fieldname, char const *value, bool isdesc = false );
	virtual void		AddNumericField( char const *fieldname, int value );

private:

	void				ReportError(TRK_UINT rc, char const *func, char const *msg );
	TRK_UINT			Login(TRK_HANDLE* pTrkHandle, char const *projectname );

	void				SubstituteBugId( int bugid, char *out, int outlen, CUtlBuffer& src );

	TRK_HANDLE					trkHandle;
	TRK_RECORD_HANDLE			trkRecHandle;

public:
	CUtlVector< BugField_t	>	m_Fields;
};

void CBugReporter::AddField( char const *fieldname, char const *value, bool isdesc /*=false*/ )
{
	BugField_t fld;
	Q_strncpy( fld.value, value, sizeof( fld.value ) );
	fld.isnumeric = false;
	Q_strncpy( fld.trkfield, fieldname, sizeof( fld.trkfield ) );
	fld.isdesc = isdesc;

	m_Fields.AddToTail( fld );
}

void CBugReporter::AddNumericField( char const *fieldname, int value )
{
	BugField_t fld;
	fld.numvalue = value;
	fld.isnumeric = true;
	Q_strncpy( fld.trkfield, fieldname, sizeof( fld.trkfield ) );
	fld.isdesc = false;

	m_Fields.AddToTail( fld );
}

CBugReporter::CBugReporter()
{
	trkHandle		= (TRK_HANDLE)0;
	trkRecHandle	= (TRK_RECORD_HANDLE)0;
}

CBugReporter::~CBugReporter()
{
	m_Fields.RemoveAll();
}

struct TRKELookup
{
	unsigned int id;
	char const *str;
};

#define TRKERROR( id )	{ id, #id }

static TRKELookup g_Lookup[] =
{
	TRKERROR( TRK_SUCCESS ),
	TRKERROR( TRK_E_VERSION_MISMATCH ),
	TRKERROR( TRK_E_OUT_OF_MEMORY ),
	TRKERROR( TRK_E_BAD_HANDLE ),
	TRKERROR( TRK_E_BAD_INPUT_POINTER ),
	TRKERROR( TRK_E_BAD_INPUT_VALUE ),
	TRKERROR( TRK_E_DATA_TRUNCATED ),
	TRKERROR( TRK_E_NO_MORE_DATA ),
	TRKERROR( TRK_E_LIST_NOT_INITIALIZED ),
	TRKERROR( TRK_E_END_OF_LIST ),
	TRKERROR( TRK_E_NOT_LOGGED_IN ),
	TRKERROR( TRK_E_SERVER_NOT_PREPARED ),
	TRKERROR( TRK_E_BAD_DATABASE_VERSION ),
	TRKERROR( TRK_E_UNABLE_TO_CONNECT ),
	TRKERROR( TRK_E_UNABLE_TO_DISCONNECT ),
	TRKERROR( TRK_E_UNABLE_TO_START_TIMER ),
	TRKERROR( TRK_E_NO_DATA_SOURCES ),
	TRKERROR( TRK_E_NO_PROJECTS ),
	TRKERROR( TRK_E_WRITE_FAILED ),
	TRKERROR( TRK_E_PERMISSION_DENIED ),
	TRKERROR( TRK_E_SET_FIELD_DENIED ),
	TRKERROR( TRK_E_ITEM_NOT_FOUND ),
	TRKERROR( TRK_E_CANNOT_ACCESS_DATABASE ),
	TRKERROR( TRK_E_CANNOT_ACCESS_QUERY ),
	TRKERROR( TRK_E_CANNOT_ACCESS_INTRAY ),
	TRKERROR( TRK_E_CANNOT_OPEN_FILE ),
	TRKERROR( TRK_E_INVALID_DBMS_TYPE ),
	TRKERROR( TRK_E_INVALID_RECORD_TYPE ),
	TRKERROR( TRK_E_INVALID_FIELD ),
	TRKERROR( TRK_E_INVALID_CHOICE ),
	TRKERROR( TRK_E_INVALID_USER ),
	TRKERROR( TRK_E_INVALID_SUBMITTER ),
	TRKERROR( TRK_E_INVALID_OWNER ),
	TRKERROR( TRK_E_INVALID_DATE ),
	TRKERROR( TRK_E_INVALID_STORED_QUERY ),
	TRKERROR( TRK_E_INVALID_MODE ),
	TRKERROR( TRK_E_INVALID_MESSAGE ),
	TRKERROR( TRK_E_VALUE_OUT_OF_RANGE ),
	TRKERROR( TRK_E_WRONG_FIELD_TYPE ),
	TRKERROR( TRK_E_NO_CURRENT_RECORD ),
	TRKERROR( TRK_E_NO_CURRENT_NOTE ),
	TRKERROR( TRK_E_NO_CURRENT_ATTACHED_FILE ),
	TRKERROR( TRK_E_NO_CURRENT_ASSOCIATION ),
	TRKERROR( TRK_E_NO_RECORD_BEGIN ),
	TRKERROR( TRK_E_NO_MODULE ),
	TRKERROR( TRK_E_USER_CANCELLED ),
	TRKERROR( TRK_E_SEMAPHORE_TIMEOUT ),
	TRKERROR( TRK_E_SEMAPHORE_ERROR ),
	TRKERROR( TRK_E_INVALID_SERVER_NAME ),
	TRKERROR( TRK_E_NOT_LICENSED )
};

void CBugReporter::ReportError(TRK_UINT rc, char const *func, char const *msg )
{
    if ( rc != TRK_SUCCESS )
	{
		switch (rc)
		{
		case TRK_E_ITEM_NOT_FOUND:
		    Msg( "%s %s was not found!\n", func,  msg );
			break;
		case TRK_E_INVALID_FIELD:
			Msg( "%s %s Invalid field!\n", func,  msg );
			break;
		default:
			int i = 0;
			for ( i; i < ARRAYSIZE( g_Lookup ) ; ++i )
			{
				if ( g_Lookup[ i ].id == rc )
				{
					Msg( "%s returned %i - %s (%s)!\n", func,  rc, g_Lookup[ i ].str, msg );
					break;
				}
			}

			if ( i >= ARRAYSIZE( g_Lookup ) )
			{
				Msg( "%s returned %i - %s! (%s)\n", func,  rc, "???", msg );
			}
			break;
		}
    }
}

TRK_UINT CBugReporter::Login(TRK_HANDLE* pTrkHandle, char const *projectname )
{
	char dbms[50] = DEFAULT_DBMS;

	char username[ 50 ];
	char password[ 50 ];

	Q_strncpy( username, DEFAULT_USERNAME, sizeof( username ) );
	Q_strncpy( password, DEFAULT_PASSWORD, sizeof( password ) );

	TRK_UINT rc = TrkProjectLogin(*pTrkHandle,
		username,
		password,
		projectname,
		NULL,
		NULL,
		NULL,
		NULL,
		TRK_USE_INI_FILE_DBMS_LOGIN);

	if (rc != TRK_SUCCESS)
	{
		rc = TrkProjectLogin(*pTrkHandle,
			username,
			"",
			projectname,
			NULL,
			NULL,
			NULL,
			NULL,
			TRK_USE_INI_FILE_DBMS_LOGIN);
		if (rc != TRK_SUCCESS)
		{
			Msg("Bug reporter init failed: Your tracker password must be your user name or blank.\n");
			return rc;
		}
	}

	TrkGetLoginDBMSName(*pTrkHandle, sizeof(dbms), dbms );

	char projout[ 256 ];

	TrkGetLoginProjectName(*pTrkHandle, sizeof( projout ), projout );
	
	Msg( "Project:  %s\n", projout );
	Msg( "Server:  %s\n", dbms );

	return rc;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize and login with default username/password for this computer (from resource/bugreporter.res)
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBugReporter::Init( char const *projectname )
{
	TRK_UINT			rc;
	rc = TrkHandleAlloc( TRK_VERSION_ID, &trkHandle);
	if ( rc != TRK_SUCCESS )
	{
		ReportError(rc, "TrkHandleAlloc", "Failed to Allocate Tracker Handle!");
		return false;
	}

	// Login to default project out of INI file.
	rc = Login( &trkHandle, projectname );
	if (rc != TRK_SUCCESS)
	{
		return false;
	}

	rc = TrkRecordHandleAlloc(trkHandle, &trkRecHandle);
	if (rc != TRK_SUCCESS)
	{
		ReportError(rc, "TrkRecordHandleAlloc", 
			"Failed to Allocate Tracker Record Handle!");
		return false;
	}

	return true;
}

void CBugReporter::Shutdown()
{
	TRK_UINT			rc;
	
	if ( trkRecHandle )
	{
		rc = TrkRecordHandleFree(&trkRecHandle);
		if (rc != TRK_SUCCESS)
		{
			ReportError(rc, "TrkRecordHandleFree", "Failed to Free Tracker Record Handle!");
		}
	}

	if ( trkHandle )
	{
		rc = TrkProjectLogout(trkHandle);		
		if (rc != TRK_SUCCESS)
		{
			ReportError(rc, "TrkProjectLogout", "Failed to Logout of Project!");
		}
		else
		{
			rc = TrkHandleFree(&trkHandle);     
			if (rc != TRK_SUCCESS)
			{
				ReportError(rc, "TrkHandleFree", "Failed to Free Tracker Handle!");
			}
		}
	}
}

void CBugReporter::StartNewBugReport()
{
	m_Fields.RemoveAll();
}

void CBugReporter::CancelNewBugReport()
{
	m_Fields.RemoveAll();
}

void CBugReporter::SubstituteBugId( int bugid, char *out, int outlen, CUtlBuffer& src )
{
	out[ 0 ] = 0;

	char *dest = out;

	src.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );

	char const *replace = "\\BugId\\";
	int replace_len = Q_strlen( replace );

	for ( int pos = 0; pos <= src.TellPut() && ( ( dest - out ) < outlen ); )
	{
		char const *str = ( char const * )src.PeekGet( pos );
		if ( !Q_strnicmp( str, replace, replace_len ) )
		{
			*dest++ = '\\';

			char num[ 32 ];
			Q_snprintf( num, sizeof( num ), "%i", bugid );
			char *pnum = num;
			while ( *pnum )
			{
				*dest++ = *pnum++;
			}

			*dest++ = '\\';
			pos += replace_len;
			continue;
		}

		*dest++ = *str;
		++pos;
	}
	*dest = 0;
}

bool CBugReporter::CommitBugReport( int& bugSubmissionId )
{
	bugSubmissionId = -1;

	int fieldCount = m_Fields.Count();
	if ( fieldCount == 0 )
		return false;

	TRK_UINT rc = 0;
	rc = TrkNewRecordBegin( trkRecHandle, SCR_TYPE );
	if ( rc != TRK_SUCCESS )
	{
		ReportError(rc, "TrkNewRecordBegin", 
			"Failed to TrkNewRecordBegin!");
		return false;
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	for ( int i = 0; i < fieldCount; ++i )
	{
		BugField_t& fld = m_Fields[ i ];

		if ( !fld.isdesc )
		{
			// Populate fields
			if ( !fld.isnumeric )
			{
				rc = TrkSetStringFieldValue( 
					trkRecHandle,
					fld.trkfield,
					fld.value );
			}
			else
			{
				rc = TrkSetNumericFieldValue( 
					trkRecHandle,
					fld.trkfield,
					fld.numvalue );
			}
			if ( rc != TRK_SUCCESS )
			{
				char es[ 256 ];
				Q_snprintf( es, sizeof( es ), "Failed to add '%s'", fld.trkfield );

				ReportError( rc, "TrkSetStringFieldValue",  es );
				return false;
			}
		}
		else
		{
			buf.Printf( "%s\n", fld.value );

			buf.PutChar( 0 );

			rc = TrkSetDescriptionData( trkRecHandle,
				buf.TellPut(),
				(const char * )buf.Base(),
				0 );
			if ( rc != TRK_SUCCESS )
			{
				ReportError(rc, "TrkSetDescriptionData", 
					"Failed to set description data!");
				return false;
			}
		}

	}

	TRK_TRANSACTION_ID id;
	rc = TrkNewRecordCommit( trkRecHandle, &id );
	if ( rc != TRK_SUCCESS )
	{
		ReportError(rc, "TrkNewRecordCommit", 
			"Failed to TrkNewRecordCommit!");
		return false;
	}

	TRK_UINT bugId;
	rc = TrkGetNumericFieldValue( trkRecHandle, "Id", &bugId );
	if ( rc != TRK_SUCCESS )
	{
		ReportError(rc, "TrkGetNumericFieldValue", 
			"Failed to TrkGetNumericFieldValue for bug Id #!");
	}
	else
	{
		bugSubmissionId = (int)bugId;
	}

	rc = TrkGetSingleRecord( trkRecHandle, bugId, SCR_TYPE );
	if ( rc != TRK_SUCCESS )
	{
		ReportError( rc, "TrkGetSingleRecord",
			"Failed to open bug id for update" );
		return false;
	}

	rc = TrkUpdateRecordBegin( trkRecHandle );
	if ( rc != TRK_SUCCESS )
	{
		ReportError( rc, "TrkUpdateRecordBegin",
			"Failed to open bug id for update" );
		return false;
	}
	else
	{
		int textbuflen = 2 * buf.TellPut() + 1;

		char *textbuf = new char [ textbuflen ];
		Q_memset( textbuf, 0, textbuflen );

		SubstituteBugId( (int)bugId, textbuf, textbuflen, buf );

		// Update the description with the substituted text!!!
		rc = TrkSetDescriptionData( trkRecHandle,
			Q_strlen( textbuf ) + 1,
			(const char * )textbuf,
			0 );

		delete[] textbuf;

		if ( rc != TRK_SUCCESS )
		{
			ReportError(rc, "TrkSetDescriptionData(update)", 
				"Failed to set description data!");
			return false;
		}

		rc = TrkUpdateRecordCommit( trkRecHandle, &id );
		if ( rc != TRK_SUCCESS )
		{
			ReportError(rc, "TrkUpdateRecordCommit", 
				"Failed to TrkUpdateRecordCommit for bug Id #!");
			return false;
		}
	}

	m_Fields.RemoveAll();

	return true;
}

SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	spewed = true;

	printf( "%s", pMsg );
	OutputDebugString( pMsg );
	
	if ( type == SPEW_ERROR )
	{
		printf( "\n" );
		OutputDebugString( "\n" );
	}

	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : depth - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void vprint( int depth, const char *fmt, ... )
{
	char string[ 8192 ];
	va_list va;
	va_start( va, fmt );
	vsprintf( string, fmt, va );
	va_end( va );

	FILE *fp = NULL;

	if ( uselogfile )
	{
		fp = fopen( "log.txt", "ab" );
	}

	while ( depth-- > 0 )
	{
		printf( "  " );
		OutputDebugString( "  " );
		if ( fp )
		{
			fprintf( fp, "  " );
		}
	}

	::printf( "%s", string );
	OutputDebugString( string );

	if ( fp )
	{
		char *p = string;
		while ( *p )
		{
			if ( *p == '\n' )
			{
				fputc( '\r', fp );
			}
			fputc( *p, fp );
			p++;
		}
		fclose( fp );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void printusage( void )
{
	vprint( 0, "usage:  getbugs pvcsproject hostname database username password contentadminexe <startbug endbug>\n\
		\ne.g.:  getbugs \"Steam Beta\" steamweb cserr username password \"u:/p4clients/yahn_steam_work/projects/gazelleproto/tools/contentadmin/vc70_debug_static/contentadmin.exe\" 1 10\n" );

	// Exit app
	exit( 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckLogFile( void )
{
	if ( uselogfile )
	{
		_unlink( "log.txt" );
		vprint( 0, "    Outputting to log.txt\n" );
	}
}

void PrintHeader()
{
	vprint( 0, "Valve Software - getbugs.exe (%s)\n", __DATE__ );
	vprint( 0, "--- Pulls public bugreporter bugs into PVCS tracker ---\n" );
}

bool GetBugZip( int bugnum, char const *admin )
{
	bool retval = false;

	char commandline[ 512 ];
	char directory[ 512 ];

	Q_strncpy( directory, admin, sizeof( directory ) );
	Q_StripFilename( directory );


	//	sprintf( commandline, "msdev engdll.dsw /MAKE \"quiver - Win32 GL Debug\" /OUT log.txt" );

	// Builds the default configuration
	sprintf( commandline, "\"%s\" bugreport %i", admin, bugnum );

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );

	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );

	if ( !CreateProcess( NULL, commandline, NULL, NULL, TRUE, 0, NULL, directory, &si, &pi ) )
	{
		LPVOID lpMsgBuf;
		FormatMessage( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM | 
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPTSTR) &lpMsgBuf,
			0,
			NULL 
		);
		// Process any inserts in lpMsgBuf.
		// ...
		// Display the string.
		MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
		// Free the buffer.
		LocalFree( lpMsgBuf );
		return retval;
	}

	// Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

	DWORD exitCode = (DWORD)-1;
	if ( GetExitCodeProcess( pi.hProcess, &exitCode ) )
	{
		if ( exitCode == 0 )
		{
			retval = true;
		}
	}
	
    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

	return retval;
}

void CreateDirHierarchy(const char *path)
{
	char temppath[512];
	Q_strncpy( temppath, path, sizeof(temppath) );
	
	for (char *ofs = temppath+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/' || *ofs == '\\')
		{       // create the directory
			char old = *ofs;
			*ofs = 0;
			_mkdir (temppath);
			*ofs = old;
		}
	}
}


void GetBugInfo( CBugReporter& bug, char const *host, char const *database, char const *username, char const *password, char const *admin, int startbug, int endbug )
{
	// Now connect to steamweb and update the engineaccess table
	CSysModule *sql = Sys_LoadModule( "mysql_wrapper" );
	if ( sql )
	{
		CreateInterfaceFn factory = Sys_GetFactory( sql );
		if ( factory )
		{
			IMySQL *mysql = ( IMySQL * )factory( MYSQL_WRAPPER_VERSION_NAME, NULL );
			if ( mysql )
			{
				if ( mysql->InitMySQL( database, host, username, password ) )
				{
					char q[ 512 ];

					/*
					Q_snprintf( q, sizeof( q ), "insert into engineaccess (BuildIdentifier,AllowAccess) values (\"%s\",1);",
						argv[2] );

					int retcode = mysql->Execute( q );
					if ( retcode != 0 )
					{
						printf( "Query %s failed\n", q );
					}
					else
					{
						printf( "Successfully added buildidentifier '%s' to %s:%s\n",
							argv[ 2 ], argv[ 3 ], argv[ 4 ] );
					}
					*/

					char where[ 256 ];
					where[ 0 ] = 0;
					if ( startbug != -1 && endbug != -1 )
					{
						Q_snprintf( where, sizeof( where ), "BugId>=%i and BugId<=%i and ", startbug, endbug );
					}

					Q_snprintf( q, sizeof( q ), "select BugId, Time, BuildNumber, ExeName, GameDirectory, MapName, Title, Description, IP,"
						"BaseIP, RAM, CPU, ProcessorVendor, DXVersionHighPart, DXVersionLowPart, DXVendorID, DXDeviceID, OSVersion, "
						"BugReportFilePath, ReportType, EMail, AccountName, SteamID, Processed, Important from bugreports where %s ( Processed is NULL or Processed = 0 );", where );


					int retcode = mysql->Execute( q );
					if ( retcode != 0 )
					{
						vprint( 0, "Query %s failed\n", q );
					}
					else
					{
						IMySQLRowSet *rows = mysql->DuplicateRowSet();

						CUtlVector< int >	processed;

						if ( rows && rows->NumFields() > 0 )
						{
							while ( rows->NextRow() )
							{
								CColumnValue BugId = rows->GetColumnValue( "BugId" );
								CColumnValue Time = rows->GetColumnValue( "Time" );
								CColumnValue BuildNumber = rows->GetColumnValue( "BuildNumber" );
								CColumnValue ExeName = rows->GetColumnValue( "ExeName" );
								CColumnValue GameDirectory = rows->GetColumnValue( "GameDirectory" );
								CColumnValue MapName = rows->GetColumnValue( "MapName" );
								CColumnValue Title = rows->GetColumnValue( "Title" );
								CColumnValue Description = rows->GetColumnValue( "Description" );
								CColumnValue IP = rows->GetColumnValue( "IP" );
								CColumnValue BaseIP = rows->GetColumnValue( "BaseIP" );
								CColumnValue RAM = rows->GetColumnValue( "RAM" );
								CColumnValue CPU = rows->GetColumnValue( "CPU" );
								CColumnValue ProcessorVendor = rows->GetColumnValue( "ProcessorVendor" );
								CColumnValue DXVersionHighPart = rows->GetColumnValue( "DXVersionHighPart" );
								CColumnValue DXVersionLowPart = rows->GetColumnValue( "DXVersionLowPart" );
								CColumnValue DXVendorID = rows->GetColumnValue( "DXVendorID" );
								CColumnValue DXDeviceID = rows->GetColumnValue( "DXDeviceID" );
								CColumnValue OSVersion = rows->GetColumnValue( "OSVersion" );
								CColumnValue BugReportFilePath = rows->GetColumnValue( "BugReportFilePath" );
								CColumnValue ReportType = rows->GetColumnValue( "ReportType" );
								CColumnValue EMail = rows->GetColumnValue( "EMail" );
								CColumnValue Accountname = rows->GetColumnValue( "Accountname" );
								CColumnValue SteamID = rows->GetColumnValue( "SteamID" );
								CColumnValue Processed = rows->GetColumnValue( "Processed" );
								CColumnValue Important = rows->GetColumnValue( "Important" );

								bug.StartNewBugReport();
								
								vprint( 1, "Processing bug %i\n", BugId.Int32() );

								bug.AddField( "Owner", "PublicUser" );
								bug.AddField( "Submitter", "PublicUser" );
								bug.AddField( "Title", Title.String() );


								bug.AddNumericField( "BugId", BugId.Int32() );
								bug.AddNumericField( "Build", BuildNumber.Int32() );
								bug.AddField( "Exe", ExeName.String() );
								bug.AddField( "Gamedir", GameDirectory.String() );
								bug.AddField( "Map", MapName.String() );
								bug.AddField( "Operating System", OSVersion.String() );
								bug.AddField( "Processor", ProcessorVendor.String() );
								bug.AddNumericField( "Memory", RAM.Int32() );
								bug.AddField( "SteamID", SteamID.String() );
								bug.AddNumericField( "CPU", CPU.Int32() );

								bug.AddField( "Time", Time.String() );

								char dxversion[ 512 ];
								int vhigh = DXVersionHighPart.Int32();
								int vlow = DXVersionLowPart.Int32();

								Q_snprintf( dxversion, sizeof( dxversion ), "%u.%u.%u.%u", ( vhigh >> 16 ) , vhigh & 0xffff, ( vlow >> 16 ), vlow & 0xffff );

								bug.AddField( "DXVersion", dxversion);

								bug.AddNumericField( "DXDevice", DXDeviceID.Int32() );
								bug.AddNumericField( "DXVendor", DXVendorID.Int32() );

								bug.AddField( "BugType", ReportType.String() );
								bug.AddField( "E-Mail Address", EMail.String() );
								bug.AddField( "Account Name", Accountname.String() );

								char zipurl[ 512 ];
								zipurl[ 0 ] = 0;

								char basepath[ 512 ];
								Q_FileBase( BugReportFilePath.String(), basepath, sizeof( basepath ) );

								char desc[ 8192 ];
								
								if ( BugReportFilePath.String()[ 0 ] )
								{
									Q_snprintf( zipurl, sizeof( zipurl ), BUG_REPOSITORY_FMT, database, "BugId", basepath );

									Q_snprintf( desc, sizeof( desc ), "%s\n\nzip url:  %s\n", Description.String(), zipurl );
								}
								else
								{
									Q_strncpy( desc, Description.String(), sizeof( desc ) );
								}

								bug.AddField( "Description", desc, true );

								int trackerBugId = -1;

								bool success = bug.CommitBugReport( trackerBugId );
								if ( success )
								{
									// The public UI handles uploading on it's own...
									// Fixup URL
									if ( zipurl[ 0 ] )
									{
										char zipurlfixed[ 512 ];
										char id[ 32 ];
										Q_snprintf( id, sizeof( id ), "%i", trackerBugId );

										// The upload destination
										Q_snprintf( zipurlfixed, sizeof( zipurlfixed ), BUG_REPOSITORY_FMT, database, id, basepath );
										Q_strlower( zipurlfixed );
										Q_FixSlashes( zipurlfixed );

										char admindir[ 512 ];
										Q_strncpy( admindir, admin, sizeof( admindir ) );
										Q_StripFilename( admindir );
										Q_strlower( admindir );
										Q_FixSlashes( admindir );


										char zipurllocal[ 512 ];
										Q_snprintf( zipurllocal, sizeof( zipurllocal ), "%s\\%s.zip", admindir, basepath );
										Q_strlower( zipurllocal );
										Q_FixSlashes( zipurllocal );

										// Get local copy of .zip file

										if ( GetBugZip( BugId.Int32(), admin ) )
										{
											struct _stat statbuf;

											if ( _stat( zipurllocal, &statbuf ) == 0 )
											{
												CreateDirHierarchy( zipurlfixed );

												MoveFile( zipurllocal, zipurlfixed );

												// Mark processed
												processed.AddToTail( BugId.Int32() );
											}
										}
										else
										{
											Warning( "Unable to retrieve bug file for %i\n", BugId.Int32() );
										}
									}
									else
									{
										processed.AddToTail( BugId.Int32() );
									}

								}
								else
								{
									Warning( "Unable to post bug report to database\n" );
								}
							}

							// Discard memory
							rows->Release();

							int c = processed.Count();
							for ( int i = 0; i < c; ++i )
							{
								Q_snprintf( q, sizeof( q ), "update bugreports set Processed=1 where BugId=%i;", processed[ i ] );

								retcode = mysql->Execute( q );
								if ( retcode != 0 )
								{
									Msg( "Query failed '%s'\n", q );
								}
							}
						}
					}
				}
				else
				{
					vprint( 0, "InitMySQL failed\n" );
				}

				mysql->Release();
			}
			else
			{
				vprint( 0, "Unable to connect via mysql_wrapper\n");
			}
		}
		else
		{
			vprint( 0, "Unable to get factory from mysql_wrapper.dll, not updating access mysql table!!!" );
		}

		Sys_UnloadModule( sql );
	}
	else
	{
		vprint( 0, "Unable to load mysql_wrapper.dll, not updating access mysql table!!!" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : argc - 
//			argv[] - 
// Output : int
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	SpewOutputFunc( SpewFunc );
	SpewActivate( "getbugs", 2 );

	uselogfile = true;
	verbose = true;

	if ( argc != 7 && argc != 9  )
	{
		PrintHeader();
		printusage();
	}

	CheckLogFile();

	PrintHeader();

	vprint( 0, "    Getting bugs...\n" );

	workingdir[0] = 0;
	Q_getwd( workingdir, sizeof( workingdir ) );

	// If they didn't specify -game on the command line, use VPROJECT.
	CmdLib_InitFileSystem( workingdir );


	CBugReporter bugreporter;
	if ( !bugreporter.Init( argv[ 1 ] ) )
	{
		vprint( 0, "Couldn't init bug reporter\n" );
		return 0;
	}

	if ( argc == 9 )
	{
		GetBugInfo( bugreporter, argv[2], argv[3], argv[4], argv[5], argv[ 6 ], atoi( argv[ 7 ] ), atoi( argv[ 8 ] ) );
	}
	else
	{
		GetBugInfo( bugreporter, argv[2], argv[3], argv[4], argv[5], argv[ 6 ], -1, -1 );
	}


	bugreporter.Shutdown();
	CmdLib_TermFileSystem();

	return 0;
}
