//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#define PROTECTED_THINGS_DISABLE
#undef PROTECT_FILEIO_FUNCTIONS
#undef fopen
#include "winlite.h"
#include "basetypes.h"

#include "utlvector.h"
#include "utlsymbol.h"
#include "utldict.h"
#include "utlbuffer.h"
#include "steamcommon.h"

#include "bugreporter/bugreporter.h"
#include "filesystem_tools.h"
#include "KeyValues.h"
#include "netadr.h"
#include "steam/steamclientpublic.h"

bool UploadBugReport(
	const netadr_t& cserIP,
	const CSteamID& userid,
	int build,
	char const *title,
	char const *body,
	char const *exename,
	char const *gamedir,
	char const *mapname,
	char const *reporttype,
	char const *email,
	char const *accountname,
	int ram,
	int cpu,
	char const *processor,
	unsigned int high, 
	unsigned int low, 
	unsigned int vendor, 
	unsigned int device,
	char const *osversion,
	char const *attachedfile,
	unsigned int attachedfilesize
);

IBaseFileSystem *g_pFileSystem = NULL;

class CBug
{
public:
	CBug()
	{
		Clear();
	}

	void Clear()
	{
		Q_memset( title, 0, sizeof( title ) );
		Q_memset( desc, 0, sizeof( desc ) );
		Q_memset( submitter, 0, sizeof( submitter ) );
		Q_memset( owner, 0, sizeof( owner ) );
		Q_memset( severity, 0, sizeof( severity ) );
		Q_memset( priority, 0, sizeof( priority ) );
		Q_memset( area, 0, sizeof( area ) );
		Q_memset( mapnumber, 0, sizeof( mapnumber) );
		Q_memset( reporttype, 0, sizeof( reporttype ) );
		Q_memset( level, 0, sizeof( level ) );
		Q_memset( build, 0, sizeof( build ) );
		Q_memset( position, 0, sizeof( position ) );
		Q_memset( orientation, 0, sizeof( orientation ) );
		Q_memset( screenshot_unc, 0, sizeof( screenshot_unc ) );
		Q_memset( savegame_unc, 0, sizeof( savegame_unc ) );
		Q_memset( bsp_unc, 0, sizeof( bsp_unc ) );
		Q_memset( vmf_unc, 0, sizeof( vmf_unc ) );
		Q_memset( driverinfo, 0, sizeof( driverinfo ) );
		Q_memset( misc, 0, sizeof( misc ) );
		Q_memset( zip, 0, sizeof( zip ) );

		Q_memset( exename, 0, sizeof( exename ) );
		Q_memset( gamedir, 0, sizeof( gamedir ) );
		ram = 0;
		cpu = 0;
		Q_memset( processor, 0, sizeof( processor ) );
		dxversionhigh = 0;
		dxversionlow = 0;
		dxvendor = 0;
		dxdevice = 0;
		Q_memset( osversion, 0, sizeof( osversion ) );

		includedfiles.Purge();
	}

	char			title[ 256 ];
	char			desc[ 8192 ];
	char			owner[ 256 ];
	char			submitter[ 256 ];
	char			severity[ 256 ];
	char			priority[ 256 ];
	char			area[ 256 ];
	char			mapnumber[ 256 ];
	char			reporttype[ 256 ];
	char			level[ 256 ];
	char			build[ 256 ];
	char			position[ 256 ];
	char			orientation[ 256 ];
	char			screenshot_unc[ 256 ];
	char			savegame_unc[ 256 ];
	char			bsp_unc[ 256 ];
	char			vmf_unc[ 256 ];
	char			driverinfo[ 2048 ];
	char			misc[ 1024 ];

	char			exename[ 256 ];
	char			gamedir[ 256 ];
	unsigned int	ram;
	unsigned int	cpu;
	char			processor[ 256 ];
	unsigned int	dxversionhigh;
	unsigned int	dxversionlow;
	unsigned int	dxvendor;
	unsigned int	dxdevice;
	char			osversion[ 256 ];

	char			zip[ 256 ];

	struct incfile
	{
		char name[ 256 ];
	};

	CUtlVector< incfile >	includedfiles;
};

class CBugReporter : public IBugReporter
{
public:

	CBugReporter();
	virtual ~CBugReporter();

	// Initialize and login with default username/password for this computer (from resource/bugreporter.res)
	virtual bool		Init( CreateInterfaceFn engineFactory );
	virtual void		Shutdown();

	virtual bool		IsPublicUI() { return true; }

	virtual char const	*GetUserName();
	virtual char const	*GetUserName_Display();

	virtual int			GetNameCount();
	virtual char const	*GetName( int index );

	virtual int			GetDisplayNameCount();
	virtual char const  *GetDisplayName( int index );

	virtual char const	*GetDisplayNameForUserName( char const *username );
	virtual char const  *GetUserNameForDisplayName( char const *display );

	virtual int			GetSeverityCount();
	virtual char const	*GetSeverity( int index );

	virtual int			GetPriorityCount();
	virtual char const	*GetPriority( int index );

	virtual int			GetAreaCount();
	virtual char const	*GetArea( int index );

	virtual int			GetAreaMapCount();
	virtual char const	*GetAreaMap( int index );

	virtual int			GetMapNumberCount();
	virtual char const	*GetMapNumber( int index );

	virtual int			GetReportTypeCount();
	virtual char const	*GetReportType( int index );

	virtual char const *GetRepositoryURL( void ) { return NULL; }
	virtual char const *GetSubmissionURL( void ) { return NULL; }

	virtual int			GetLevelCount(int area) { return 0; }
	virtual char const	*GetLevel(int area, int index ) { return ""; }

// Submission API
	virtual void		StartNewBugReport();
	virtual void		CancelNewBugReport();
	virtual bool		CommitBugReport( int& bugSubmissionId );

	virtual void		SetTitle( char const *title );
	virtual void		SetDescription( char const *description );

	// NULL for current user
	virtual void		SetSubmitter( char const *username = 0 );
	virtual void		SetOwner( char const *username );
	virtual void		SetSeverity( char const *severity );
	virtual void		SetPriority( char const *priority );
	virtual void		SetArea( char const *area );
	virtual void		SetMapNumber ( char const *mapnumber );
	virtual void		SetReportType( char const *reporttype );

	virtual void		SetLevel( char const *levelnamne );
	virtual void		SetPosition( char const *position );
	virtual void		SetOrientation( char const *pitch_yaw_roll );
	virtual void		SetBuildNumber( char const *build_num );

	virtual void		SetScreenShot( char const *screenshot_unc_address );
	virtual void		SetSaveGame( char const *savegame_unc_address );

	virtual void		SetBSPName( char const *bsp_unc_address );
	virtual void		SetVMFName( char const *vmf_unc_address );

	virtual void		AddIncludedFile( char const *filename );
	virtual void		ResetIncludedFiles();

	virtual void		SetDriverInfo( char const *info );

	virtual void		SetZipAttachmentName( char const *zipfilename );

	virtual void		SetMiscInfo( char const *info );

	virtual void		SetCSERAddress( const struct netadr_s& adr );
	virtual void		SetExeName( char const *exename );
	virtual void		SetGameDirectory( char const *gamedir );
	virtual void		SetRAM( int ram );
	virtual void		SetCPU( int cpu );
	virtual void		SetProcessor( char const *processor );
	virtual void		SetDXVersion( unsigned int high, unsigned int low, unsigned int vendor, unsigned int device );
	virtual void		SetOSVersion( char const *osversion );
	virtual void		SetSteamUserID( void *steamid, int idsize );

private:

	void				SubstituteBugId( int bugid, char *out, int outlen, CUtlBuffer& src );

	CUtlSymbolTable				m_BugStrings;

	CUtlVector< CUtlSymbol >	m_Severity;
	CUtlVector< CUtlSymbol >	m_Area;
	CUtlVector< CUtlSymbol >	m_MapNumber;
	CUtlVector< CUtlSymbol >	m_ReportType;

	CUtlSymbol					m_UserName;

	CBug						*m_pBug;
	netadr_t					m_cserIP;
	CSteamID			m_SteamID;
};

CBugReporter::CBugReporter()
{
	Q_memset( &m_cserIP, 0, sizeof( m_cserIP ) );
	m_pBug = NULL;

	m_Severity.AddToTail( m_BugStrings.AddString( "Zero" ) );
	m_Severity.AddToTail( m_BugStrings.AddString( "Low" ) );
	m_Severity.AddToTail( m_BugStrings.AddString( "Medium" ) );
	m_Severity.AddToTail( m_BugStrings.AddString( "High" ) );
	m_Severity.AddToTail( m_BugStrings.AddString( "Showstopper" ) );

	
	m_ReportType.AddToTail( m_BugStrings.AddString( "<<Choose Item>>" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Video / Display Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Network / Connectivity Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Download / Installation Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "In-game Crash" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Game play / Strategy Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Steam Problems" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Unlisted Bug" ) );
	m_ReportType.AddToTail( m_BugStrings.AddString( "Feature Request / Suggestion" ) );
}

CBugReporter::~CBugReporter()
{
	m_BugStrings.RemoveAll();
	m_Severity.Purge();
	m_Area.Purge();
	m_MapNumber.Purge();
	m_ReportType.Purge();

	delete m_pBug;
}
//-----------------------------------------------------------------------------
// Purpose: Initialize and login with default username/password for this computer (from resource/bugreporter.res)
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBugReporter::Init( CreateInterfaceFn engineFactory )
{
	g_pFileSystem = (IFileSystem *)engineFactory( FILESYSTEM_INTERFACE_VERSION, NULL );
	if ( !g_pFileSystem )
	{
		AssertMsg( 0, "Failed to create/get IFileSystem" );
		return false;
	}
	return true;
}

void CBugReporter::Shutdown()
{
}

char const *CBugReporter::GetUserName()
{
	return m_UserName.String();
}

char const *CBugReporter::GetUserName_Display()
{
	return GetUserName();
}

int CBugReporter::GetNameCount()
{
	return 1;
}

char const *CBugReporter::GetName( int index )
{
	if ( index < 0 || index >= 1 )
		return "<<Invalid>>";

	return GetUserName();
}

int CBugReporter::GetDisplayNameCount()
{
	return 1;
}

char const *CBugReporter::GetDisplayName( int index )
{
	if ( index < 0 || index >= 1 )
		return "<<Invalid>>";

	return GetUserName(); 
}

char const *CBugReporter::GetDisplayNameForUserName( char const *username )
{
	return username;
}

char const *CBugReporter::GetUserNameForDisplayName( char const *display )
{
	return display;
}

int CBugReporter::GetSeverityCount()
{
	return m_Severity.Count() ;
}

char const *CBugReporter::GetSeverity( int index )
{
	if ( index < 0 || index >= m_Severity.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_Severity[ index ] );
}

int CBugReporter::GetPriorityCount()
{
	return 0;
}

char const *CBugReporter::GetPriority( int index )
{
	return "<<Invalid>>";
}

int CBugReporter::GetAreaCount()
{
	return 0;
}

char const *CBugReporter::GetArea( int index )
{
	return "<<Invalid>>";
}

int CBugReporter::GetAreaMapCount()
{
	return 0;
}

char const *CBugReporter::GetAreaMap( int index )
{
	return "<<Invalid>>";
}

int CBugReporter::GetMapNumberCount()
{
	return 0;
}

char const *CBugReporter::GetMapNumber( int index )
{
	return "<<Invalid>>";
}

int CBugReporter::GetReportTypeCount()
{
	return m_ReportType.Count();
}

char const *CBugReporter::GetReportType( int index )
{
	if ( index < 0 || index >= m_ReportType.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_ReportType[ index ] );
}

void CBugReporter::StartNewBugReport()
{
	if ( !m_pBug )
	{
		m_pBug = new CBug();
	}
	else
	{
		m_pBug->Clear();
	}
}

void CBugReporter::CancelNewBugReport()
{
	if ( !m_pBug )
		return;

	m_pBug->Clear();
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

	if ( !m_pBug )
		return false;

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.Printf( "%s\n\n", m_pBug->desc );

	buf.Printf( "level:  %s\nbuild:  %s\nposition:  setpos %s; setang %s\n",
		m_pBug->level,
		m_pBug->build,
		m_pBug->position,
		m_pBug->orientation );

	if ( m_pBug->screenshot_unc[ 0 ] )
	{
		buf.Printf( "screenshot:  %s\n", m_pBug->screenshot_unc );
	}
	if ( m_pBug->savegame_unc[ 0 ] )
	{
		buf.Printf( "savegame:  %s\n", m_pBug->savegame_unc );
	}
	/*
	if ( m_pBug->bsp_unc[ 0 ] )
	{
		buf.Printf( "bsp:  %s\n", m_pBug->bsp_unc );
	}
	if ( m_pBug->vmf_unc[ 0 ] )
	{
		buf.Printf( "vmf:  %s\n", m_pBug->vmf_unc );
	}
	if ( m_pBug->includedfiles.Count() > 0 )
	{
		int c = m_pBug->includedfiles.Count();
		for ( int i = 0 ; i < c; ++i )
		{
			buf.Printf( "include:  %s\n", m_pBug->includedfiles[ i ].name );
		}
	}
	*/
	if ( m_pBug->driverinfo[ 0 ] )
	{
		buf.Printf( "%s\n", m_pBug->driverinfo );
	}
	if ( m_pBug->misc[ 0 ] )
	{
		buf.Printf( "%s\n", m_pBug->misc );
	}

	buf.PutChar( 0 );

	// Store desc

	int bugId = 0;

	bugSubmissionId = (int)bugId;

	int attachedfilesize = ( m_pBug->zip[ 0 ] == 0 ) ? 0 : g_pFileSystem->Size( m_pBug->zip );

	if ( !UploadBugReport( 
			m_cserIP,
			m_SteamID,
			atoi( m_pBug->build ),
			m_pBug->title,
			(char const *)buf.Base(),
			m_pBug->exename,
			m_pBug->gamedir,
			m_pBug->level,
			m_pBug->reporttype,
			m_pBug->owner,
			m_pBug->submitter,
			m_pBug->ram,
			m_pBug->cpu,
			m_pBug->processor,
			m_pBug->dxversionhigh, 
			m_pBug->dxversionlow, 
			m_pBug->dxvendor, 
			m_pBug->dxdevice,
			m_pBug->osversion,
			m_pBug->zip,
			attachedfilesize
		) )
	{
		Msg( "Unable to upload bug...\n" );
	}

	m_pBug->Clear();

	return true;
}

void CBugReporter::SetTitle( char const *title )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->title, title, sizeof( m_pBug->title ) );
}

void CBugReporter::SetDescription( char const *description )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->desc, description, sizeof( m_pBug->desc ) );
}

void CBugReporter::SetSubmitter( char const *username /*= 0*/ )
{
	m_UserName = username;
	/*
	if ( !username )
	{
		username = GetUserName();
	}
	*/

	Assert( m_pBug );
	Q_strncpy( m_pBug->submitter, username ? username : "", sizeof( m_pBug->submitter ) );
}

void CBugReporter::SetOwner( char const *username )
{
	Q_strncpy( m_pBug->owner, username, sizeof( m_pBug->owner ) );
}

void CBugReporter::SetSeverity( char const *severity )
{
}

void CBugReporter::SetPriority( char const *priority )
{
}

void CBugReporter::SetArea( char const *area )
{
}

void CBugReporter::SetMapNumber( char const *mapnumber )
{
}

void CBugReporter::SetReportType( char const *reporttype )
{
	Q_strncpy( m_pBug->reporttype, reporttype, sizeof( m_pBug->reporttype ) );
}

void CBugReporter::SetLevel( char const *levelnamne )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->level, levelnamne, sizeof( m_pBug->level ) );
}

void CBugReporter::SetDriverInfo( char const *info )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->driverinfo, info, sizeof( m_pBug->driverinfo ) );
}

void CBugReporter::SetZipAttachmentName( char const *zipfilename )
{
	Assert( m_pBug );

	Q_strncpy( m_pBug->zip, zipfilename, sizeof( m_pBug->zip ) );
}

void CBugReporter::SetMiscInfo( char const *info )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->misc, info, sizeof( m_pBug->misc ) );
}

void CBugReporter::SetPosition( char const *position )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->position, position, sizeof( m_pBug->position ) );
}

void CBugReporter::SetOrientation( char const *pitch_yaw_roll )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->orientation, pitch_yaw_roll, sizeof( m_pBug->orientation ) );
}

void CBugReporter::SetBuildNumber( char const *build_num )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->build, build_num, sizeof( m_pBug->build ) );
}

void CBugReporter::SetScreenShot( char const *screenshot_unc_address )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->screenshot_unc, screenshot_unc_address, sizeof( m_pBug->screenshot_unc ) );
}

void CBugReporter::SetSaveGame( char const *savegame_unc_address )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->savegame_unc, savegame_unc_address, sizeof( m_pBug->savegame_unc ) );
}

void CBugReporter::SetBSPName( char const *bsp_unc_address )
{
}

void CBugReporter::SetVMFName( char const *vmf_unc_address )
{
}

void CBugReporter::AddIncludedFile( char const *filename )
{
}

void CBugReporter::ResetIncludedFiles()
{
}

void CBugReporter::SetExeName( char const *exename )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->exename, exename, sizeof( m_pBug->exename ) );
}

void CBugReporter::SetGameDirectory( char const *pchGamedir )
{
	Assert( m_pBug );

	Q_FileBase( pchGamedir, m_pBug->gamedir, sizeof( m_pBug->gamedir ) );
}

void CBugReporter::SetRAM( int ram )
{
	Assert( m_pBug );
	m_pBug->ram = ram;
}

void CBugReporter::SetCPU( int cpu )
{
	Assert( m_pBug );
	m_pBug->cpu = cpu;
}

void CBugReporter::SetProcessor( char const *processor )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->processor, processor, sizeof( m_pBug->processor ) );
}

void CBugReporter::SetDXVersion( unsigned int high, unsigned int low, unsigned int vendor, unsigned int device )
{
	Assert( m_pBug );
	m_pBug->dxversionhigh = high;
	m_pBug->dxversionlow = low;
	m_pBug->dxvendor = vendor;
	m_pBug->dxdevice = device;
}

void CBugReporter::SetOSVersion( char const *osversion )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->osversion, osversion, sizeof( m_pBug->osversion ) );
}

void CBugReporter::SetCSERAddress( const struct netadr_s& adr )
{
	m_cserIP = adr;
}

void CBugReporter::SetSteamUserID( void *steamid, int idsize )
{
	Assert( idsize == sizeof( uint64 ) );
	m_SteamID.SetFromUint64( *((uint64*)steamid) );
}

EXPOSE_SINGLE_INTERFACE( CBugReporter, IBugReporter, INTERFACEVERSION_BUGREPORTER );
