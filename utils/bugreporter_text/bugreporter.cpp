//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#define PROTECTED_THINGS_DISABLE
#undef PROTECT_FILEIO_FUNCTIONS
#undef fopen
#include <windows.h>
#include "basetypes.h"

#include "utlvector.h"
#include "utlsymbol.h"
#include "utldict.h"
#include "utlbuffer.h"

#include "bugreporter/bugreporter.h"
#include "filesystem_tools.h"
#include "KeyValues.h"

IFileSystem *g_filesystem = NULL;

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

	virtual bool		IsPublicUI() { return false; }

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

	virtual char const *GetRepositoryURL( void );
	virtual char const *GetSubmissionURL( void );

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

	virtual void		SetZipAttachmentName( char const *zipfilename ) {} // only used by public bug reporter

	virtual void		SetDriverInfo( char const *info );

	virtual void		SetMiscInfo( char const *info );

	// These are stubbed here, but are used by the public version...
	virtual void		SetCSERAddress( const struct netadr_s& adr ) {}
	virtual void		SetExeName( char const *exename ) {}
	virtual void		SetGameDirectory( char const *pchGamedir ) {}
	virtual void		SetRAM( int ram ) {}
	virtual void		SetCPU( int cpu ) {}
	virtual void		SetProcessor( char const *processor ) {}
	virtual void		SetDXVersion( unsigned int high, unsigned int low, unsigned int vendor, unsigned int device ) {}
	virtual void		SetOSVersion( char const *osversion ) {}
	virtual void		SetSteamUserID( void *steamid, int idsize ) {};
private:

	bool				PopulateLists();
	bool				PopulateChoiceList( char const *listname, CUtlVector< CUtlSymbol >& list );

	void				SubstituteBugId( int bugid, char *out, int outlen, CUtlBuffer& src );

	CUtlSymbolTable				m_BugStrings;

	CUtlVector< CUtlSymbol >	m_Severity;
	CUtlVector< CUtlSymbol >	m_SortedDisplayNames;
	CUtlVector< CUtlSymbol >	m_Priority;
	CUtlVector< CUtlSymbol >	m_Area;
	CUtlVector< CUtlSymbol >	m_AreaMap;
	CUtlVector< CUtlSymbol >	m_MapNumber;
	CUtlVector< CUtlSymbol >	m_ReportType;

	CUtlSymbol					m_UserName;

	CBug				*m_pBug;

	char						m_BugRootDirectory[512];
	KeyValues					*m_OptionsFile;

	int							m_CurrentBugID;
	char						m_CurrentBugDirectory[512];
};

CBugReporter::CBugReporter()
{
	m_pBug = NULL;
	m_CurrentBugID = 0;
}

CBugReporter::~CBugReporter()
{
	m_BugStrings.RemoveAll();
	m_Severity.Purge();
	m_SortedDisplayNames.Purge();
	m_Priority.Purge();
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
	if ( engineFactory )
	{
		g_filesystem = (IFileSystem *)engineFactory( FILESYSTEM_INTERFACE_VERSION, NULL );
		if ( !g_filesystem )
		{
			AssertMsg( 0, "Failed to create/get IFileSystem" );
			return false;
		}
	}

	// Load our bugreporter_text options file 
	m_OptionsFile = new KeyValues( "OptionsFile" );
	if ( !m_OptionsFile->LoadFromFile( g_filesystem, "bugreporter_text.txt", "EXECUTABLE_PATH" ) )
	{
		AssertMsg( 0, "Failed to load bugreporter_text options file." );
		return false;
	}

	strncpy( m_BugRootDirectory, m_OptionsFile->GetString( "bug_directory", "." ), sizeof(m_BugRootDirectory) );

	PopulateLists();

	return true;
}

void CBugReporter::Shutdown()
{
}

char const *CBugReporter::GetUserName()
{
	return m_BugStrings.String( m_UserName );
}

char const *CBugReporter::GetUserName_Display()
{
	return GetDisplayNameForUserName( GetUserName() );
}

int CBugReporter::GetNameCount()
{
	return GetDisplayNameCount();
}

char const *CBugReporter::GetName( int index )
{
	return GetDisplayName(index);
}

int CBugReporter::GetDisplayNameCount()
{
	return m_SortedDisplayNames.Count();
}

char const *CBugReporter::GetDisplayName( int index )
{
	if ( index < 0 || index >= (int)m_SortedDisplayNames.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_SortedDisplayNames[ index ] ); 
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
	return m_Priority.Count();
}

char const *CBugReporter::GetPriority( int index )
{
	if ( index < 0 || index >= m_Priority.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_Priority[ index ] );
}

int CBugReporter::GetAreaCount()
{
	return m_Area.Count();
}

char const *CBugReporter::GetArea( int index )
{
	if ( index < 0 || index >= m_Area.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_Area[ index ] );
}

int CBugReporter::GetAreaMapCount()
{
	return m_AreaMap.Count();
}

char const *CBugReporter::GetAreaMap( int index )
{
	if ( index < 0 || index >= m_AreaMap.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_AreaMap[ index ] );
}

int CBugReporter::GetMapNumberCount()
{
	return m_MapNumber.Count();
}

char const *CBugReporter::GetMapNumber( int index )
{
	if ( index < 0 || index >= m_MapNumber.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_MapNumber[ index ] );
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

char const *CBugReporter::GetRepositoryURL( void )
{
	return m_BugRootDirectory;
}

char const *CBugReporter::GetSubmissionURL( void )
{
  	return m_CurrentBugDirectory;
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

	// Find the first available bug number by looking
	// for the next directory that doesn't exist.
	m_CurrentBugID = 0;
	do
	{
		m_CurrentBugID++;
		Q_snprintf(m_CurrentBugDirectory, sizeof(m_CurrentBugDirectory), "%s/%d", m_BugRootDirectory, m_CurrentBugID );
	} while ( g_filesystem->FileExists( m_CurrentBugDirectory ) );
	g_filesystem->CreateDirHierarchy( m_CurrentBugDirectory );
}

void CBugReporter::CancelNewBugReport()
{
	if ( !m_pBug )
		return;

	m_pBug->Clear();
	m_CurrentBugID = 0;
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
	bugSubmissionId = m_CurrentBugID;

	if ( !m_pBug )
		return false;

	// Now create the bug descriptor file, and dump all the text keys in it
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.Printf( "%s\n\n", m_pBug->title );
	buf.Printf( "Owner:      %s\n", m_pBug->owner );
	buf.Printf( "Submitter:  %s\n", m_pBug->submitter );
	buf.Printf( "Severity:   %s\n", m_pBug->severity );
	buf.Printf( "Type:       %s\n", m_pBug->reporttype );
	buf.Printf( "Priority:   %s\n", m_pBug->priority );
	buf.Printf( "Area:       %s\n", m_pBug->area );
	//buf.Printf( "Map Number: %s\n", m_pBug->mapnumber );		// Ignoring, since the bsp name is in the title
	buf.Printf( "\n%s\n\n", m_pBug->desc );

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
	if ( m_pBug->driverinfo[ 0 ] )
	{
		buf.Printf( "%s\n", m_pBug->driverinfo );
	}
	if ( m_pBug->misc[ 0 ] )
	{
		buf.Printf( "%s\n", m_pBug->misc );
	}
	buf.PutChar( 0 );

	// Replace the "BugId" strings in the buffer with the bug ID
	int textbuflen = 2 * buf.TellPut() + 1;
	char *textbuf = new char [ textbuflen ];
	Q_memset( textbuf, 0, textbuflen );
	SubstituteBugId( bugSubmissionId, textbuf, textbuflen, buf );
	buf.Clear();
	buf.PutString( textbuf );

	// Write it out to the file
	char szBugFileName[1024];
	Q_snprintf(szBugFileName, sizeof(szBugFileName), "%s/bug.txt", m_CurrentBugDirectory );
	g_filesystem->WriteFile( szBugFileName, NULL, buf );

	// Clear the bug
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
	if ( !username )
	{
		username = GetUserName();
	}

	Assert( m_pBug );
	Q_strncpy( m_pBug->submitter, username, sizeof( m_pBug->submitter ) );
}

void CBugReporter::SetOwner( char const *username )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->owner, username, sizeof( m_pBug->owner ) );
}

void CBugReporter::SetSeverity( char const *severity )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->severity, severity, sizeof( m_pBug->severity ) );
}

void CBugReporter::SetPriority( char const *priority )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->priority, priority, sizeof( m_pBug->priority ) );
}

void CBugReporter::SetArea( char const *area )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->area, area, sizeof( m_pBug->area ) );
}

void CBugReporter::SetMapNumber( char const *mapnumber )
{
	Assert( m_pBug);
	Q_strncpy( m_pBug->mapnumber, mapnumber, sizeof( m_pBug->mapnumber ) );
}

void CBugReporter::SetReportType( char const *reporttype )
{
	Assert( m_pBug );
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
	Assert( m_pBug );
	Q_strncpy( m_pBug->bsp_unc, bsp_unc_address, sizeof( m_pBug->bsp_unc ) );
}

void CBugReporter::SetVMFName( char const *vmf_unc_address )
{
	Assert( m_pBug );
	Q_strncpy( m_pBug->vmf_unc, vmf_unc_address, sizeof( m_pBug->vmf_unc ) );
}

void CBugReporter::AddIncludedFile( char const *filename )
{
	CBug::incfile includedfile;
	Q_strncpy( includedfile.name, filename, sizeof( includedfile.name ) );
	m_pBug->includedfiles.AddToTail( includedfile );
}

void CBugReporter::ResetIncludedFiles()
{
	m_pBug->includedfiles.Purge();
}

bool CBugReporter::PopulateChoiceList( char const *listname, CUtlVector< CUtlSymbol >& list )
{
	// Read choice lists from text file
	KeyValues *pKV = m_OptionsFile->FindKey( listname );
	pKV = pKV->GetFirstSubKey();
	while ( pKV )
	{
		CUtlSymbol sym = m_BugStrings.AddString( pKV->GetName() );
		list.AddToTail( sym );

		pKV = pKV->GetNextKey();
	}

	return true;
}

// owner, not required <<Unassigned>>
// area <<None>>

bool CBugReporter::PopulateLists()
{
	CUtlSymbol unassigned = m_BugStrings.AddString( "<<Unassigned>>" );
	CUtlSymbol none = m_BugStrings.AddString( "<<None>>" );

	m_Area.AddToTail( none );
	m_MapNumber.AddToTail( none );
	m_SortedDisplayNames.AddToTail( unassigned );

	PopulateChoiceList( "Severity", m_Severity );
	PopulateChoiceList( "Report Type", m_ReportType );
	PopulateChoiceList( "Area", m_Area );
	PopulateChoiceList( "Area@Dir%Map", m_AreaMap );
	PopulateChoiceList( "Map Number", m_MapNumber );
	PopulateChoiceList( "Priority", m_Priority );

	// Get developer names from text file

	KeyValues *pKV = m_OptionsFile->FindKey( "Names" );
	pKV = pKV->GetFirstSubKey();
	while ( pKV )
	{
		// Fill in lookup table
		CUtlSymbol display_name_sym = m_BugStrings.AddString( pKV->GetName() );
		m_SortedDisplayNames.AddToTail( display_name_sym );

		pKV = pKV->GetNextKey();
	}

/*
	// Now sort display names
	int c = m_SortedDisplayNames.Count();
	for ( int i = 1 ; i < c; i++ )
	{
		for ( int j = i + 1; j < c; j++ )
		{
			char const *p1 = m_BugStrings.String( m_SortedDisplayNames[ i ] );
			char const *p2 = m_BugStrings.String( m_SortedDisplayNames[ j ] );

			int cmp = Q_stricmp( p1, p2 );
			if ( cmp > 0 )
			{
				CUtlSymbol t = m_SortedDisplayNames[ i ];
				m_SortedDisplayNames[ i ] =  m_SortedDisplayNames[ j ];
				m_SortedDisplayNames[ j ] = t;
			}
		}
	}
*/

	return true;
}

EXPOSE_SINGLE_INTERFACE( CBugReporter, IBugReporter, INTERFACEVERSION_BUGREPORTER );
