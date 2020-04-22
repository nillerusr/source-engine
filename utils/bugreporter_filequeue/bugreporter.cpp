//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
//#define PROTECTED_THINGS_DISABLE 
#undef PROTECT_FILEIO_FUNCTIONS
#undef fopen
#include "winlite.h"
#include <time.h>
#ifdef WIN32
#include <io.h>
#include <direct.h>
#else
#include <sys/stat.h>
#define _stat stat
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "basetypes.h"

#include "utlvector.h"
#include "utlsymbol.h"
#include "utldict.h"
#include "utlbuffer.h"
#include "utlmap.h"

#include "bugreporter/bugreporter.h"
#include "filesystem_tools.h"
#include "KeyValues.h"
#include "vstdlib/random.h"

#ifdef WIN32
#define BUGSUB_CONFIG "\\\\bugbait\\bugsub\\config.txt"
#else
#ifdef OSX
#define BUGSUB_CONFIG "/Volumes/bugsub/config.txt"
#define BUGSUB_MOUNT "/Volumes/bugsub"
#define BUGSUB_MOUNT_COMMAND "mount_smbfs //guest:@bugbait.valvesoftware.com/bugsub "  BUGSUB_MOUNT
#else
// We can't do sudo here, so we rely on /etc/fstab being setup for '/mnt/bugsub'. See comment in CBugReporter::Init().
#define BUGSUB_CONFIG "/mnt/bugsub/config.txt"
#define BUGSUB_MOUNT "/mnt/bugsub"
#define BUGSUB_MOUNT_COMMAND "mount /mnt/bugsub"
#endif
#define BUGSUB_UNMOUNT_COMMAND "umount "  BUGSUB_MOUNT
#endif

class CBugReporter *g_bugreporter = NULL;

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
	virtual char const  *GetUserNameForIndex( int index );

	virtual char const	*GetDisplayNameForUserName( char const *username );
	virtual char const  *GetUserNameForDisplayName( char const *display );
	virtual char const  *GetAreaMapForArea( char const * area);

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

	virtual int			GetLevelCount(int area);
	virtual char const	*GetLevel(int area, int index );

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
	bool				SymbolLessThan(const CUtlSymbol &sym1, const CUtlSymbol &sym2);

private:

	bool				PopulateLists();
	bool				PopulateChoiceList( char const *listname, CUtlVector< CUtlSymbol >& list );

	CUtlSymbolTable				m_BugStrings;

	CUtlVector< CUtlSymbol >	m_Severity;
	CUtlVector< CUtlSymbol >	m_SortedDisplayNames;
	CUtlVector< CUtlSymbol >	m_SortedUserNames;
	CUtlVector< CUtlSymbol >	m_Priority;
	CUtlVector< CUtlSymbol >	m_Area;
	CUtlVector< CUtlSymbol >	m_AreaMap;
	CUtlVector< CUtlSymbol >	m_MapNumber;
	CUtlVector< CUtlSymbol >	m_ReportType;
	CUtlMap<CUtlSymbol, CUtlVector< CUtlSymbol > *> m_LevelMap;

	CUtlSymbol					m_UserName;

	CBug				*m_pBug;

	char						m_BugRootDirectory[512];
	KeyValues					*m_OptionsFile;

	int							m_CurrentBugID;
	char						m_CurrentBugDirectory[512];
	bool						m_bMountedBugSub;
};

bool CUtlSymbol_LessThan(const CUtlSymbol &sym1, const CUtlSymbol &sym2)
{
	return g_bugreporter->SymbolLessThan(sym1, sym2);
}

CBugReporter::CBugReporter()
{
	m_pBug = NULL;
	m_CurrentBugID = 0;
	m_bMountedBugSub = false;
	m_LevelMap.SetLessFunc(&CUtlSymbol_LessThan);
	g_bugreporter = this;
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
	m_LevelMap.RemoveAll();

	delete m_pBug;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize and login with default username/password for this computer (from resource/bugreporter.res)
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBugReporter::Init( CreateInterfaceFn engineFactory )
{
	// Load our bugreporter_text options file 
	m_OptionsFile = new KeyValues( "OptionsFile" );

#ifdef POSIX
	// check if we can see the config file
	struct _stat mount_info;
	if (_stat(BUGSUB_CONFIG, &mount_info))
	{
		// if not we may need to mount bugbait
		if ( _stat(BUGSUB_MOUNT, &mount_info ) )
		{
			// make the mount dir if needed
			mkdir( BUGSUB_MOUNT, S_IRWXU | S_IRWXG | S_IRWXO  );
		}
		
		// now run the smbmount on it
		system( BUGSUB_MOUNT_COMMAND );

#ifdef LINUX
		if( _stat( BUGSUB_CONFIG, &mount_info ) )
		{
			Color clr( 255, 100, 50, 255 );

			// The mount failed - probably because the /etc/fstab entry is missing?
			ConColorMsg( clr, "ERROR: failed to mount '" BUGSUB_MOUNT "' with '" BUGSUB_MOUNT_COMMAND "'.\n" );
			ConColorMsg( clr, "Bugsub not set up yet? Do 'sudo apt-get install smbfs', 'sudo mkdir /mnt/bugsub', and add this /etc/fstab:\n" );
			ConColorMsg( clr, "  bugbait.valvesoftware.com/bugsub /mnt/bugsub smbfs rw,user,username=guest,password=,noauto 0 0\n" );
			return false;
		}
#endif

		m_bMountedBugSub = true;
	}
	
#elif defined(WIN32)
	
#else
#error "need to get to \\bugbait somehow"
#endif
	
	// open file old skool way to avoid steam filesystem restrictions
	struct _stat cfg_info;
	if (_stat(BUGSUB_CONFIG, &cfg_info)) {
		AssertMsg( 0, "Failed to find bugreporter options file." );
		return false;
	}

	int buf_size = (cfg_info.st_size + 1);
	char *buf = new char[buf_size];
	FILE *fp = fopen(BUGSUB_CONFIG, "rb");
	if (!fp) {
		AssertMsg(0, "failed to open bugreporter options file" );
		delete [] buf;
		return false;
	}

	fread(buf, cfg_info.st_size, 1, fp);
	fclose(fp);
	buf[cfg_info.st_size] = 0;
	if ( !m_OptionsFile->LoadFromBuffer(BUGSUB_CONFIG, buf) )
	{
		AssertMsg( 0, "Failed to load bugreporter_text options file." );
		delete [] buf;
		return false;
	}
#ifdef WIN32
	V_strncpy( m_BugRootDirectory, m_OptionsFile->GetString( "bug_directory", "." ), sizeof(m_BugRootDirectory) );
#elif defined(OSX)
	V_strncpy( m_BugRootDirectory, m_OptionsFile->GetString( "bug_directory_osx", BUGSUB_MOUNT ), sizeof(m_BugRootDirectory) );
#elif defined(LINUX)
	V_strncpy( m_BugRootDirectory, m_OptionsFile->GetString( "bug_directory_linux", BUGSUB_MOUNT ), sizeof(m_BugRootDirectory) );
#else
#error
#endif
	
	PopulateLists();

#ifdef WIN32
	m_UserName = m_BugStrings.AddString(getenv( "username" ));
#elif POSIX
	m_UserName = m_BugStrings.AddString(getenv( "USER" ));	
#else
#error
#endif
	delete [] buf;
	return true;
}

void CBugReporter::Shutdown()
{
#ifdef POSIX
	if ( m_bMountedBugSub )
	{
		system( BUGSUB_UNMOUNT_COMMAND );
	}
#endif
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

char const *CBugReporter::GetUserNameForIndex( int index )
{
	if ( index < 0 || index >= (int)m_SortedUserNames.Count() )
		return "<<Invalid>>";

	return m_BugStrings.String( m_SortedUserNames[ index ] ); 
}

char const *CBugReporter::GetDisplayNameForUserName( char const *username )
{
	CUtlSymbol username_sym = m_BugStrings.Find(username);
	FOR_EACH_VEC(m_SortedUserNames, index) {
		if (m_SortedUserNames[index] == username_sym) {
			return GetDisplayName(index);
		}
	}
	return username;
}

char const *CBugReporter::GetUserNameForDisplayName( char const *display )
{
	CUtlSymbol display_sym = m_BugStrings.Find(display);
	FOR_EACH_VEC(m_SortedDisplayNames, index) {
		if (m_SortedDisplayNames[index] == display_sym) {
			return GetUserNameForIndex(index);
		}
	}

	return display;
}

char const *CBugReporter::GetAreaMapForArea( char const *area)
{
	CUtlSymbol area_sym = m_BugStrings.Find(area);
	FOR_EACH_VEC(m_Area, index) {
		if (m_Area[index] == area_sym && index > 0) {
			char const *areamap = m_BugStrings.String(m_AreaMap[index-1]);
			return areamap+1;
		}
	}

	return "<<Invalid>>";
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

// only valid after calling CBugReporter::StartNewBugReport()
char const *CBugReporter::GetSubmissionURL( void )
{
	return m_CurrentBugDirectory;
}

int	CBugReporter::GetLevelCount(int area)
{
	CUtlSymbol area_sym = m_AreaMap[area-1];
	int area_index = m_LevelMap.Find(area_sym);
	if (m_LevelMap.IsValidIndex(area_index))
	{
		CUtlVector<CUtlSymbol> *levels = m_LevelMap[area_index];
		return levels->Count();
	}
	return 0;
}

char const *CBugReporter::GetLevel(int area, int index )
{
	CUtlSymbol area_sym = m_AreaMap[area-1];
	int area_index = m_LevelMap.Find(area_sym);
	if (m_LevelMap.IsValidIndex(area_index))
	{
		CUtlVector<CUtlSymbol> *levels = m_LevelMap[area_index];
		if (index >= 0 && index < levels->Count())
		{
			return m_BugStrings.String((*levels)[index]);
		}
	}

	return "";
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
	struct tm t;

	do
	{
		VCRHook_LocalTime( &t );

		Q_snprintf(m_CurrentBugDirectory, sizeof(m_CurrentBugDirectory), "%s%c%04i%02i%02i-%02i%02i%02i-%s", 
				   m_BugRootDirectory,
				   CORRECT_PATH_SEPARATOR,
				   t.tm_year + 1900, t.tm_mon+1, t.tm_mday,
				   t.tm_hour, t.tm_min, t.tm_sec,
				   m_BugStrings.String(m_UserName));
		if (_access(m_CurrentBugDirectory, 0) != 0)
			break;

		// sleep for a second or two then try again
		ThreadSleep(RandomInt(1000,2000));
	} while ( 1 );
	_mkdir(m_CurrentBugDirectory);
}

void CBugReporter::CancelNewBugReport()
{
	if ( !m_pBug )
		return;

	m_pBug->Clear();
	m_CurrentBugID = 0;
}

static void OutputField( CUtlBuffer &outbuf, char const *pFieldName, char const *pFieldValue )
{
	if ( pFieldValue && pFieldValue[0] )
	{
		char const *pTmpString = V_AddBackSlashesToSpecialChars( pFieldValue );
		outbuf.Printf( "%s=\"%s\"\n", pFieldName, pTmpString );
		delete[] pTmpString;
	}
}

bool CBugReporter::CommitBugReport( int& bugSubmissionId )
{
	bugSubmissionId = m_CurrentBugID;

	if ( !m_pBug )
		return false;

	// Now create the bug descriptor file, and dump all the text keys in it
	CUtlBuffer buf(0, 0, CUtlBuffer::TEXT_BUFFER);

	OutputField( buf, "Title", m_pBug->title );
	OutputField( buf, "Owner", m_pBug->owner );
	OutputField( buf, "Submitter", m_pBug->submitter );
	OutputField( buf, "Severity", m_pBug->severity );
//	OutputField( buf, "Type", m_pBug->reporttype );
//	OutputField( buf, "Priority", m_pBug->priority );
	OutputField( buf, "Area", m_pBug->area );
	OutputField( buf, "Level", m_pBug->mapnumber );	
	OutputField( buf, "Description", m_pBug->desc );

	//OutputField( buf, "Level", m_pBug->level );
	OutputField( buf, "Build", m_pBug->build );
	OutputField( buf, "Position", m_pBug->position );
	OutputField( buf, "Orientation", m_pBug->orientation );

	OutputField( buf, "Screenshot", m_pBug->screenshot_unc );
	OutputField( buf, "Savegame", m_pBug->savegame_unc );
	OutputField( buf, "Bsp", m_pBug->bsp_unc );
	OutputField( buf, "vmf", m_pBug->vmf_unc );

// 	if ( m_pBug->includedfiles.Count() > 0 )
// 	{
// 		int c = m_pBug->includedfiles.Count();
// 		for ( int i = 0 ; i < c; ++i )
// 		{
// 			buf.Printf( "include:  %s\n", m_pBug->includedfiles[ i ].name );
// 		}
// 	}
	OutputField( buf, "DriverInfo", m_pBug->driverinfo );

	OutputField( buf, "Misc", m_pBug->misc );


	// Write it out to the file
	// Need to use native calls to bypass steam filesystem
	char szBugFileName[1024];
	Q_snprintf(szBugFileName, sizeof(szBugFileName), "%s%cbug.txt", m_CurrentBugDirectory, CORRECT_PATH_SEPARATOR );
	FILE *fp = fopen(szBugFileName, "wb");
	if (!fp) 
		return false;

	fprintf(fp, "%s", (char *)buf.Base());
	fclose(fp);
	
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

void CBugReporter::SetSubmitter( char const *username /* = 0 */)
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
	char const *game = GetAreaMapForArea(area);
	Q_strncpy( m_pBug->area, game, sizeof( m_pBug->area ) );
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

bool CBugReporter::SymbolLessThan(const CUtlSymbol &sym1, const CUtlSymbol &sym2)
{
	const char *string1 = m_BugStrings.String(sym1);
	const char *string2 = m_BugStrings.String(sym2);

	return Q_stricmp(string1, string2) < 0;
}

// owner, not required <<Unassigned>>
// area <<None>>

bool CBugReporter::PopulateLists()
{
	CUtlSymbol none = m_BugStrings.AddString( "<<None>>" );
	m_Area.AddToTail( none );

	PopulateChoiceList( "Severity", m_Severity );

	// Get developer names from text file
	KeyValues *pKV = m_OptionsFile->FindKey( "Names" );
	pKV = pKV->GetFirstSubKey();
	while ( pKV )
	{
		// Fill in lookup table
		CUtlSymbol display_name_sym = m_BugStrings.AddString( pKV->GetName() );
		CUtlSymbol user_name_sym = m_BugStrings.AddString( pKV->GetString() );
		m_SortedDisplayNames.AddToTail( display_name_sym );
		m_SortedUserNames.AddToTail( user_name_sym );
		pKV = pKV->GetNextKey();
	}

	pKV = m_OptionsFile->FindKey( "Area" );
	pKV = pKV->GetFirstSubKey();
	while(pKV) 
	{
		char const *area = pKV->GetName();
		char const *game = pKV->GetString();
		char areamap[256];
		Q_snprintf(areamap, sizeof(areamap), "@%s", game);
		CUtlSymbol area_sym = m_BugStrings.AddString(area);
		CUtlSymbol area_map_sym = m_BugStrings.AddString(areamap);
		m_Area.AddToTail( area_sym );
		m_AreaMap.AddToTail( area_map_sym );
		pKV = pKV->GetNextKey();
	}

	pKV = m_OptionsFile->FindKey( "Level" );
	pKV = pKV->GetFirstSubKey();
	while(pKV) 
	{
		char const *level = pKV->GetName();
		char const *area = pKV->GetString();
		char areamap[256];
		Q_snprintf(areamap, sizeof(areamap), "@%s", area);

		CUtlSymbol level_sym = m_BugStrings.AddString(level);
		CUtlSymbol area_sym = m_BugStrings.Find(areamap);
		if (!area_sym.IsValid())
		{
			area_sym = m_BugStrings.AddString(areamap);
		}
		
		unsigned index = m_LevelMap.Find(area_sym);
		CUtlVector<CUtlSymbol> *levels = 0;

		if (m_LevelMap.IsValidIndex(index))
		{
			levels = m_LevelMap[index];
		}
		else
		{
			levels = new CUtlVector<CUtlSymbol>;
			Assert(levels);
			m_LevelMap.Insert(area_sym, levels);
		}

		if (level)
		{
			levels->AddToTail(level_sym);
		}

		pKV = pKV->GetNextKey();
	}



	return true;
}

EXPOSE_SINGLE_INTERFACE( CBugReporter, IBugReporter, INTERFACEVERSION_BUGREPORTER );
