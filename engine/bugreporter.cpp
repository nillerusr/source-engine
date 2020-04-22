//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#undef PROTECT_FILEIO_FUNCTIONS
#ifndef _LINUX
#undef fopen
#endif
#if defined( WIN32 ) && !defined( _X360 )
#include "winlite.h"
#include <winsock2.h> // INADDR_ANY defn
#include <direct.h>
#elif defined(POSIX)
#include <sys/stat.h>

#ifdef OSX
#include <copyfile.h>
#import <mach/mach_host.h>
#import <sys/sysctl.h>
#elif defined(LINUX)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif
#define GetLastError() errno
#elif defined( _X360 )
#else
#error
#endif

#include <time.h>

#include "client.h"
#include <vgui_controls/Frame.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>
#include <vgui_controls/BuildGroup.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/FileOpenDialog.h>
#include "vgui_controls/DirectorySelectDialog.h"
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextEntry.h>
#include "enginebugreporter.h"
#include "vgui_baseui_interface.h"
#include <vgui_controls/FileOpenDialog.h>
#include "ivideomode.h"
#include "cl_main.h"
#include "gl_model_private.h"
#include "tier2/tier2.h"
#include "tier1/utlstring.h"
#include "tier1/callqueue.h"
#include "vstdlib/jobthread.h"

#include "utlsymbol.h"
#include "utldict.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "icliententitylist.h"
#include "bugreporter/bugreporter.h"
#include "icliententity.h"
#include "tier0/vcrmode.h"
#include "tier0/platform.h"
#include "net.h"
#include "host_phonehome.h"
#include "tier0/icommandline.h"
#include "stdstring.h"
#include "sv_main.h"
#include "server.h"
#include "eiface.h"
#include "gl_matsysiface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "FindSteamServers.h"
#include "vstdlib/random.h"
#ifndef SWDS
#include "cl_steamauth.h"
#endif

#include "zip/XZip.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define DENY_SOUND		"common/bugreporter_failed"
#define SUCCEED_SOUND	"common/bugreporter_succeeded"

// Fixme, move these to buguiddata.res script file?
#ifdef WIN32
#define BUG_REPOSITORY_URL "\\\\fileserver\\bugs"
#elif defined(OSX)
#define BUG_REPOSITORY_URL "/Volumes/bugs"
#elif defined(LINUX)
#define BUG_REPOSITORY_URL "\\\\fileserver\\bugs"
#else
//#error
#endif
#define REPOSITORY_VALIDATION_FILE "info.txt"

#define BUG_REPORTER_DLLNAME "bugreporter_filequeue" 
#define BUG_REPORTER_PUBLIC_DLLNAME "bugreporter_public" 

#if defined( _DEBUG )
#define PUBLIC_BUGREPORT_WAIT_TIME	3
#else
#define PUBLIC_BUGREPORT_WAIT_TIME	15
#endif

// 16Mb max zipped size
#define MAX_ZIP_SIZE	(1024 * 1024 * 16 )

extern ConVar	skill;
extern float g_fFramesPerSecond;

static ConVar bugreporter_includebsp( "bugreporter_includebsp", "1", 0, "Include .bsp for internal bug submissions." );
static ConVar bugreporter_uploadasync( "bugreporter_uploadasync", "0", FCVAR_ARCHIVE, "Upload attachments asynchronously" );


using namespace vgui;

unsigned long GetRam()
{
#ifdef WIN32
	MEMORYSTATUS stat;
	GlobalMemoryStatus( &stat );
	return (stat.dwTotalPhys / (1024 * 1024));
#elif defined(OSX)
	int mib[2] = { CTL_HW, HW_MEMSIZE };
	u_int namelen = sizeof(mib) / sizeof(mib[0]);
	uint64_t memsize;
	size_t len = sizeof(memsize);
	
	if (sysctl(mib, namelen, &memsize, &len, NULL, 0) >= 0) 
	{
		return memsize / (1024*1024);
	}
	else
		return 0;
#elif defined( LINUX )
	unsigned long Ram = 0;
	FILE *fh = fopen( "/proc/meminfo", "r" );
	if( fh )
	{
		char buf[ 256 ];
		const char szMemTotal[] = "MemTotal:";

		while( fgets( buf, sizeof( buf ), fh ) )
		{
			if ( !Q_strnicmp( buf, szMemTotal, sizeof( szMemTotal ) - 1 ) )
			{
				// Should already be in kB
				Ram = atoi( buf + sizeof( szMemTotal ) - 1 ) / 1024;
				break;
			}
		}

		fclose( fh );
	}
	return Ram;
#else
	Assert( !"Impl GetRam" );
	return 0;
#endif
}

const char *GetInternalBugReporterDLL( void )
{
	char const *pBugReportedDLL = NULL;
	if ( CommandLine()->CheckParm("-bugreporterdll", &pBugReportedDLL ) )
		return pBugReportedDLL;

	return BUG_REPORTER_DLLNAME;
}

void DisplaySystemVersion( char *osversion, int maxlen )
{
#ifdef WIN32
	osversion[ 0 ] = 0;
	OSVERSIONINFOEX osvi;
	BOOL bOsVersionInfoEx;
	
	// Try calling GetVersionEx using the OSVERSIONINFOEX structure.
	//
	// If that fails, try using the OSVERSIONINFO structure.
	
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	
	bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi);

	if( !bOsVersionInfoEx )
	{
		// If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.
		
		osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
		if (! GetVersionEx ( (OSVERSIONINFO *) &osvi) )
		{
			Q_strncpy( osversion, "Unable to get Version", maxlen );
			return;
		}
	}
	
	switch (osvi.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		
		// Test for the product.
		
		if ( osvi.dwMajorVersion <= 4 )
		{
			Q_strncat ( osversion, "NT ", maxlen, COPY_ALL_CHARACTERS );
		}
		
		if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
		{
			Q_strncat ( osversion, "2000 ", maxlen, COPY_ALL_CHARACTERS );
		}
		
		if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
		{
			Q_strncat ( osversion, "XP ", maxlen, COPY_ALL_CHARACTERS );
		}
		
		// Display version, service pack (if any), and build number.
		
		char build[256];
		Q_snprintf (build, sizeof( build ), "%s (Build %d) version %d.%d",
			osvi.szCSDVersion,
			osvi.dwBuildNumber & 0xFFFF,
			osvi.dwMajorVersion,
			osvi.dwMinorVersion );
		Q_strncat ( osversion, build, maxlen, COPY_ALL_CHARACTERS );
		break;
		
	case VER_PLATFORM_WIN32_WINDOWS:
		
		if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
		{
			Q_strncat ( osversion, "95 ", maxlen, COPY_ALL_CHARACTERS );
			if ( osvi.szCSDVersion[1] == 'C' || osvi.szCSDVersion[1] == 'B' )
			{
				Q_strncat ( osversion, "OSR2 ", maxlen, COPY_ALL_CHARACTERS );
			}
		} 
		
		if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
		{
			Q_strncat ( osversion, "98 ", maxlen, COPY_ALL_CHARACTERS );
			if ( osvi.szCSDVersion[1] == 'A' )
			{
				Q_strncat ( osversion, "SE ", maxlen, COPY_ALL_CHARACTERS );
			}
		} 
		
		if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
		{
			Q_strncat ( osversion, "Me ", maxlen, COPY_ALL_CHARACTERS );
		} 
		break;
		
	case VER_PLATFORM_WIN32s:
		
		Q_strncat ( osversion, "Win32s ", maxlen, COPY_ALL_CHARACTERS );
		break;
	}
#elif defined(OSX)
	FILE *fpVersionInfo = popen( "/usr/bin/sw_vers", "r" );
	const char *pszSearchString = "ProductVersion:\t";
	const int cchSearchString = Q_strlen( pszSearchString );
	char rgchVersionLine[1024];
		
	if ( !fpVersionInfo )
		Q_strncat ( osversion, "OSXU ", maxlen, COPY_ALL_CHARACTERS );
	else
	{
		while ( fgets( rgchVersionLine, sizeof(rgchVersionLine), fpVersionInfo ) )
		{
			if ( !Q_strnicmp( rgchVersionLine, pszSearchString, cchSearchString ) )
			{
				const char *pchVersion = rgchVersionLine + cchSearchString;
				if ( !Q_strnicmp( pchVersion, "10.", Q_strlen( "10." ) ) )
				{
					pchVersion += 3; // move past "10."
					if( *pchVersion == '4' && *(pchVersion+1) == '.' )
					{
						Q_strncat ( osversion, "OSX104 ", maxlen, COPY_ALL_CHARACTERS );
						break;
					}
					else if ( *pchVersion == '5' && *(pchVersion+1) == '.' )
					{
						Q_strncat ( osversion, "OSX105 ", maxlen, COPY_ALL_CHARACTERS );
						break;
					}
					else if ( *pchVersion == '6' && *(pchVersion+1) == '.' )
					{
						Q_strncat ( osversion, "OSX106 ", maxlen, COPY_ALL_CHARACTERS );
						break;
					}
					else if ( *pchVersion == '7' && *(pchVersion+1) == '.' )
					{
						Q_strncat ( osversion, "OSX107 ", maxlen, COPY_ALL_CHARACTERS );
						break;
					}
				}
				break;
			}
		}
		pclose( fpVersionInfo );
	}
#elif defined(LINUX)
	FILE *fpKernelVer = fopen( "/proc/version_signature", "r" );

	if ( !fpKernelVer )
	{
		Q_strncat ( osversion, "Linux ", maxlen, COPY_ALL_CHARACTERS );
	}
	else
	{
		fgets( osversion, maxlen, fpKernelVer );
		osversion[ maxlen - 1 ] = 0;

		char *szlf = Q_strrchr( osversion, '\n' );
		if( szlf )
			*szlf = '\0';

		fclose( fpKernelVer );
	}
#endif
}

static int GetNumberForMap()
{
	if ( !host_state.worldmodel )
		return 1;

	char mapname[256];
	CL_SetupMapName( modelloader->GetName( host_state.worldmodel ), mapname, sizeof( mapname ) );

	KeyValues *resfilekeys = new KeyValues( "mapnumber" );
	if ( resfilekeys->LoadFromFile( g_pFileSystem, "scripts/bugreport_mapnumber.txt", "GAME" ) )
	{
		KeyValues *entry = resfilekeys->GetFirstSubKey();

		while ( entry )
		{
			if ( !Q_stricmp( entry->GetName(), mapname ) )
			{
				return entry->GetInt() + 1;
			}

			entry = entry->GetNextKey();
		}
	}
	resfilekeys->deleteThis();

	char szNameCopy[ 128 ];

	const char *pszResult = Q_strrchr( mapname, '_' );
	if( !pszResult )
		//I don't know where the number of this map is, if there even is one.
		return 1; 

	Q_strncpy( szNameCopy, pszResult + 1, sizeof( szNameCopy ) );
	if ( !szNameCopy[0] )
		return 1;
	
//	in case we can't use tchar.h, this will do the same thing
	char *pcEndOfName = szNameCopy;
	while(*pcEndOfName != 0)
	{
		if(*pcEndOfName < '0' || *pcEndOfName > '9')
			*pcEndOfName = 0;
		pcEndOfName++;
	}
	
	//add 1 because pvcs has 0 as the first map number, not 1 (and it is not 0-based).
	return atoi(szNameCopy) + 1;		
}

//-----------------------------------------------------------------------------
// Purpose: Generic dialog for displaying animating steam progress logo
//			used when we are doing a possibly length steam op that has no progress measure (login/create user/etc)
//-----------------------------------------------------------------------------
class CBugReportUploadProgressDialog : public vgui::Frame
{
public:
	CBugReportUploadProgressDialog(vgui::Panel *parent, const char *name, const char *title, const char *message);
	~CBugReportUploadProgressDialog();

	virtual void PerformLayout();

	void SetProgress( float progress );

private:
	typedef vgui::Frame BaseClass;

	vgui::ProgressBar	*m_pProgress;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBugReportUploadProgressDialog::CBugReportUploadProgressDialog(Panel *parent, const char *name, const char *title, const char *message) : Frame(parent, name)
{
	SetSize(300, 160);
	SetSizeable(false);
	MoveToFront();
	SetTitle(title, true);

	m_pProgress = new vgui::ProgressBar( this, "ProgressBar" );

	LoadControlSettings("Resource\\BugReporterUploadProgress.res");
	SetControlString("InfoLabel", message);

	MoveToCenterOfScreen();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBugReportUploadProgressDialog::~CBugReportUploadProgressDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : percent - 
//-----------------------------------------------------------------------------
void CBugReportUploadProgressDialog::SetProgress( float progress )
{
	Assert( m_pProgress );
	m_pProgress->SetProgress( progress );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBugReportUploadProgressDialog::PerformLayout()
{
	SetMinimizeButtonVisible(false);
	SetCloseButtonVisible(false);
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Generic dialog for displaying animating steam progress logo
//			used when we are doing a possibly length steam op that has no progress measure (login/create user/etc)
//-----------------------------------------------------------------------------
class CBugReportFinishedDialog : public vgui::Frame
{
public:
	CBugReportFinishedDialog(vgui::Panel *parent, const char *name, const char *title, const char *message);

	virtual void PerformLayout();

	virtual void OnCommand( const char *command );

private:
	typedef vgui::Frame BaseClass;

	vgui::Button	*m_pOk;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBugReportFinishedDialog::CBugReportFinishedDialog(Panel *parent, const char *name, const char *title, const char *message) : Frame(parent, name)
{
	SetSize(300, 160);
	SetSizeable(false);
	MoveToFront();
	SetTitle(title, true);

	m_pOk = new vgui::Button( this, "CloseBtn", "#OK", this, "Close" );

	LoadControlSettings("Resource\\BugReporterUploadFinished.res");
	SetControlString("InfoLabel", message);

	MoveToCenterOfScreen();
}

void CBugReportFinishedDialog::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "Close" ) )
	{
		MarkForDeletion();
		OnClose();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBugReportFinishedDialog::PerformLayout()
{
	SetMinimizeButtonVisible(false);
	SetCloseButtonVisible(true);
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBugUIPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CBugUIPanel, vgui::Frame );

public:
	CBugUIPanel( bool bIsPublic, vgui::Panel *parent );
	~CBugUIPanel();

	virtual void	OnTick();

	// Command issued
	virtual void	OnCommand(const char *command);
	virtual void	Close();

	virtual void	Activate();

	virtual void	SetVisible( bool state )
	{
		bool changed = state != IsVisible();
		BaseClass::SetVisible( state );

		if ( changed && state )
		{
			m_pTitle->RequestFocus();
		}
	}

	bool			Init();
	void			Shutdown();

	virtual void	OnKeyCodePressed( KeyCode code );

	void			ParseDefaultParams( void );
	void			ParseCommands( const CCommand &args );

	inline bool		IsTakingSnapshot() const
	{	
		return m_bTakingSnapshot;
	}


protected:
	vgui::TextEntry *m_pTitle;
	vgui::TextEntry *m_pDescription;

	vgui::Button *m_pTakeShot;
	vgui::Button *m_pSaveGame;
	vgui::Button *m_pSaveBSP;
	vgui::Button *m_pSaveVMF;
	vgui::Button *m_pChooseVMFFolder;
	vgui::Button *m_pIncludeFile;
	vgui::Button *m_pClearIncludes;

	vgui::Label  *m_pScreenShotURL;
	vgui::Label	 *m_pSaveGameURL;
	vgui::Label	 *m_pBSPURL;
	vgui::Label	 *m_pVMFURL;
	vgui::Label  *m_pPosition;
	vgui::Label	 *m_pOrientation;
	vgui::Label  *m_pLevelName;
	vgui::Label  *m_pBuildNumber;

	vgui::Label		*m_pSubmitter;

	vgui::ComboBox *m_pAssignTo;
	vgui::ComboBox *m_pSeverity;
	vgui::ComboBox *m_pReportType;
	vgui::ComboBox *m_pPriority;
	vgui::ComboBox *m_pGameArea;
	vgui::ComboBox *m_pMapNumber;

	vgui::Button *m_pSubmit;
	vgui::Button *m_pCancel;
	vgui::Button *m_pClearForm;

	vgui::TextEntry	*m_pIncludedFiles;

	vgui::TextEntry		*m_pEmail;
	vgui::Label			*m_pSubmitterLabel;

	IBugReporter				*m_pBugReporter;
	CSysModule					*m_hBugReporter;

	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );
	MESSAGE_FUNC_CHARPTR( OnDirectorySelected, "DirectorySelected", dir );
	MESSAGE_FUNC( OnChooseVMFFolder, "OnChooseVMFFolder" );
	MESSAGE_FUNC_PTR( OnChooseArea, "TextChanged", panel);
protected:

	void						DetermineSubmitterName();

	void						PopulateControls();

	void						OnTakeSnapshot();
	void						OnSaveGame();
	void						OnSaveBSP();
	void						OnSaveVMF();
	void						OnSubmit();
	void						OnClearForm();
	void						OnIncludeFile();
	void						OnClearIncludedFiles();

	int							GetArea();

	bool						IsValidSubmission( bool verbose );
	bool						IsValidEmailAddress( char const *email );

	void						WipeData();

	void						GetDataFileBase( char const *suffix, char *buf, int bufsize );
	const char					*GetRepositoryURL( void );
	const char					*GetSubmissionURL( int bugid );

	bool						AddFileToZip( char const *relative );
	bool						AddBugTextToZip( char const *textfilename, char const *text, int textlen );

	void						CheckContinueQueryingSteamForCSERList();

	struct includedfile
	{
		char	name[ 256 ];
		char	fixedname[ 256 ];
	};

	bool						UploadBugSubmission( 
									char const *levelname,
									int			bugId,

									char const *savefile, 
									char const *screenshot,
									char const *bsp,
									char const *vmf,
									CUtlVector< includedfile >& includedfiles );
	bool						UploadFile( char const *local, char const *remote, bool bDeleteLocal = false );

	void						DenySound();
	void						SuccessSound( int bugid );

	bool						AutoFillToken( char const *token, bool partial );
	bool						Compare( char const *token, char const *value, bool partial );

	char const					*GetSubmitter();

	void						OnFinishBugReport();

	bool						m_bCanSubmit;
	bool						m_bLoggedIn;
	bool						m_bCanSeeRepository;
	bool						m_bValidated;
	bool						m_bUseNameForSubmitter;
	
	unsigned char				m_fAutoAddScreenshot;
	enum AutoAddScreenshot { eAutoAddScreenshot_Detect = 0, eAutoAddScreenshot_Add = 1, eAutoAddScreenshot_DontAdd = 2 };

	char						m_szScreenShotName[ 256 ];
	char						m_szSaveGameName[ 256 ];
	char						m_szBSPName[ 256 ];
	char						m_szVMFName[ 256 ];
	char						m_szLevel[ 256 ];
	CUtlVector< includedfile >	m_IncludedFiles;

	bool						m_bTakingSnapshot;
	bool						m_bHidGameUIForSnapshot;
	int							m_nSnapShotFrame;

	char						m_szVMFContentDirFullpath[ MAX_PATH ];

	vgui::DHANDLE< vgui::FileOpenDialog > m_hFileOpenDialog;
	vgui::DHANDLE< vgui::DirectorySelectDialog > m_hDirectorySelectDialog;
	// If true, then once directory for vmfs is selected, we act like the Include .vmf button got pressed, too
	bool						m_bAddVMF;

	HZIP						m_hZip;

	CBugReportUploadProgressDialog		*m_pProgressDialog;
	vgui::DHANDLE< CBugReportFinishedDialog > m_hFinishedDialog;
	bool						m_bWaitForFinish;
	float						m_flPauseTime;
	CSteamID					m_SteamID;
	netadr_t					m_cserIP;
	bool						m_bQueryingSteamForCSER;
	bool						m_bIsPublic;
	CUtlString					m_sDllName;
};

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CBugUIPanel::CBugUIPanel( bool bIsPublic, vgui::Panel *parent ) : 
	BaseClass( parent, "BugUIPanel"),
	m_bIsPublic( bIsPublic ),
	m_bAddVMF( false )
{
	m_sDllName = m_bIsPublic ? 
		BUG_REPORTER_PUBLIC_DLLNAME : 
		GetInternalBugReporterDLL();

	m_hZip = (HZIP)0;

	m_hDirectorySelectDialog = NULL;
	m_hFileOpenDialog = NULL;
	m_pBugReporter = NULL;
	m_hBugReporter = 0;
	m_bQueryingSteamForCSER = false;
	
	// Default server address (hardcoded in case not running on steam)
	char const *cserIP = "207.173.177.12:27013";

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// NOTE:  If you need to override the CSER Ip, make sure you tweak the code in
	//  CheckContinueQueryingSteamForCSERList!!!!!!!!!!
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	NET_StringToAdr( cserIP, &m_cserIP );

	m_bValidated = false;
	m_szScreenShotName[ 0 ] = 0;
	m_szSaveGameName[ 0 ] = 0;
	m_szBSPName[ 0 ] = 0;
	m_szVMFName[ 0 ] = 0;
	m_szLevel[ 0 ] = 0;
	m_szVMFContentDirFullpath[ 0 ] = 0;
	m_IncludedFiles.Purge();

	m_nSnapShotFrame = -1;
	m_bTakingSnapshot = false;
	m_bHidGameUIForSnapshot = false;

	m_bCanSubmit = false;
	m_bLoggedIn = false;
	m_bCanSeeRepository = false;

	m_pProgressDialog = NULL;
	m_flPauseTime = 0.0f;
	m_bWaitForFinish = false;
	m_bUseNameForSubmitter = false;

	m_fAutoAddScreenshot = eAutoAddScreenshot_Detect;

	SetTitle("Bug Reporter", true);

	m_pTitle = new vgui::TextEntry( this, "BugTitle" );
	m_pTitle->SetMaximumCharCount( 60 );  // Titles can be 80 chars, but we put the mapname in front, so limit to 60
	m_pDescription = new vgui::TextEntry( this, "BugDescription" );
	m_pDescription->SetMultiline( true );
	m_pDescription->SetCatchEnterKey( true );
	m_pDescription->SetVerticalScrollbar( true );
	
	m_pEmail = new vgui::TextEntry( this, "BugEmail" );;
	m_pEmail->SetMaximumCharCount( 80 );

	m_pSubmitterLabel = new vgui::Label( this, "BugSubitterLabelPublic", "" );

	m_pScreenShotURL = new vgui::Label( this, "BugScreenShotURL", "" );
	m_pSaveGameURL = new vgui::Label( this, "BugSaveGameURL", "" );
	m_pBSPURL = new vgui::Label( this, "BugBSPURL", "" );
	m_pVMFURL = new vgui::Label( this, "BugVMFURL", "" );
	m_pIncludedFiles = new vgui::TextEntry( this, "BugIncludedFiles" );
	m_pIncludedFiles->SetMultiline( true );
	m_pIncludedFiles->SetVerticalScrollbar( true );
	m_pIncludedFiles->SetEditable( false );

	m_pTakeShot = new vgui::Button( this, "BugTakeShot", "Take shot" );
	m_pSaveGame = new vgui::Button( this, "BugSaveGame", "Save game" );
	m_pSaveBSP = new vgui::Button( this, "BugSaveBSP", "Include .bsp" );
	m_pSaveVMF = new vgui::Button( this, "BugSaveVMF", "Include .vmf" );
	m_pChooseVMFFolder = new vgui::Button( this, "BugChooseVMFFolder", "Choose folder" );
	m_pIncludeFile = new vgui::Button( this, "BugIncludeFile", "Include file..." );
	m_pClearIncludes = new vgui::Button( this, "BugClearIncludedFiles", "Clear files" );

	m_pPosition = new vgui::Label( this, "BugPosition", "" );
	m_pOrientation = new vgui::Label( this, "BugOrientation", "" );
	m_pLevelName = new vgui::Label( this, "BugLevel", "" );
	m_pBuildNumber = new vgui::Label( this, "BugBuild", "" );

	m_pSubmitter = new Label(this, "BugSubmitter", "" );
	m_pAssignTo = new ComboBox(this, "BugOwner", 10, false);
	m_pSeverity = new ComboBox(this, "BugSeverity", 10, false);
	m_pReportType = new ComboBox(this, "BugReportType", 10, false);
	m_pPriority = new ComboBox(this, "BugPriority", 10, false);
	m_pGameArea = new ComboBox(this, "BugArea", 10, false);
	m_pMapNumber = new ComboBox(this, "BugMapNumber", 10, false);
	
	m_pSubmit = new vgui::Button( this, "BugSubmit", "Submit" );
	m_pCancel = new vgui::Button( this, "BugCancel", "Cancel" );
	m_pClearForm = new vgui::Button( this, "BugClearForm", "Clear Form" );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	if ( m_bIsPublic )
	{
		LoadControlSettings("Resource\\BugUIPanel_Public.res");
	}
	else 
	{
		LoadControlSettings("Resource\\BugUIPanel_Filequeue.res");
	}

	m_pChooseVMFFolder->SetCommand( new KeyValues( "OnChooseVMFFolder" ) );
	m_pChooseVMFFolder->AddActionSignalTarget( this );

	int w = GetWide();
	int h = GetTall();

	int x = ( videomode->GetModeStereoWidth() - w ) / 2;
	int y = ( videomode->GetModeStereoHeight() - h ) / 2;

	// Hidden by default
	SetVisible( false );

	SetSizeable( false );
	SetMoveable( true );

	SetPos( x, y );
}

bool CBugUIPanel::Init()
{
	Color clr( 50, 100, 255, 255 );

	Assert( !m_pBugReporter );

	m_hBugReporter = g_pFileSystem->LoadModule( m_sDllName);

	if( m_bIsPublic )
	{
		// Hack due to constructor called before phonehome->Init...
		m_hBugReporter = g_pFileSystem->LoadModule( m_sDllName );

		LoadControlSettings("Resource\\BugUIPanel_Public.res");

		int w = GetWide();
		int h = GetTall();
	
		int x = ( videomode->GetModeStereoWidth() - w ) / 2;
		int y = ( videomode->GetModeStereoHeight() - h ) / 2;


		SetPos( x, y );
	}

	if ( m_hBugReporter )
	{
		CreateInterfaceFn factory = Sys_GetFactory( m_hBugReporter );
		if ( factory )
		{
			m_pBugReporter = (IBugReporter *)factory( INTERFACEVERSION_BUGREPORTER, NULL );
			if( m_pBugReporter )
			{    
				extern CreateInterfaceFn g_AppSystemFactory;
				if ( m_pBugReporter->Init( g_AppSystemFactory ) )
				{
					m_bCanSubmit = true;
					m_bLoggedIn = true;
				}
				else
				{
					m_pBugReporter = NULL;
					ConColorMsg( clr, "m_pBugReporter->Init() failed\n" );
					return false;
				}
			}
			else
			{
				ConColorMsg( clr, "Couldn't get interface '%s' from '%s'\n", INTERFACEVERSION_BUGREPORTER, m_sDllName.String() );
				return false;
			}
		}
		else
		{
			ConColorMsg( clr, "Couldn't get factory '%s'\n", m_sDllName.String() );
			return false;
		}
	}
	else
	{
		ConColorMsg( clr, "Couldn't load '%s'\n", m_sDllName.String() );
		return false;
	}

	if ( m_bCanSubmit )
	{
		PopulateControls();
	}


	if ( m_pBugReporter && m_pBugReporter->IsPublicUI() )
	{
		m_pSaveBSP->SetVisible( false );
		m_pBSPURL->SetVisible( false );

		m_pChooseVMFFolder->SetVisible( false );
		m_pSaveVMF->SetVisible( false );
		m_pVMFURL->SetVisible( false );

		m_pIncludeFile->SetVisible( false );
		m_pClearIncludes->SetVisible( false );

		m_pAssignTo->SetVisible( false );
		m_pSeverity->SetVisible( false );
		m_pPriority->SetVisible( false );
		m_pGameArea->SetVisible( false );
		m_pMapNumber->SetVisible( false );

		m_pIncludedFiles->SetVisible( false );
		m_pSubmitter->SetVisible( false );

		if ( Steam3Client().SteamUser() )
		{
			m_pSubmitterLabel->SetText( Steam3Client().SteamUser()->GetSteamID().Render() );
		}
		else
		{
			m_pSubmitterLabel->SetText( "PublicUser" );
		}

		m_pSubmitterLabel->SetVisible( true );

		m_bQueryingSteamForCSER = true;
	}
	else
	{
		m_pEmail->SetVisible( false );
		m_pSubmitterLabel->SetVisible( true );

	}

	Q_snprintf( m_szVMFContentDirFullpath, sizeof( m_szVMFContentDirFullpath ), "%s/maps", com_gamedir );
	Q_strlower( m_szVMFContentDirFullpath );
	Q_FixSlashes( m_szVMFContentDirFullpath );

	m_pBuildNumber->SetText( va( "%d", build_number() ) );

	return false;
}

void CBugUIPanel::Shutdown()
{
	if ( m_pBugReporter )
	{
		m_pBugReporter->Shutdown();
	}

	m_pBugReporter = NULL;
	if ( m_hBugReporter )
	{
		Sys_UnloadModule( m_hBugReporter );
		m_hBugReporter = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBugUIPanel::~CBugUIPanel()
{

}

void CBugUIPanel::OnTick()
{
	BaseClass::OnTick();

	CheckContinueQueryingSteamForCSERList();

	if ( !IsVisible() )
	{
		if ( !m_bTakingSnapshot )
		{
			return;
		}

		if ( host_framecount < m_nSnapShotFrame + 2 )
			return;;

		m_bTakingSnapshot = false;
		m_nSnapShotFrame = -1;

		m_pScreenShotURL->SetText( m_szScreenShotName );

		if (m_bHidGameUIForSnapshot)
		{
			EngineVGui()->ActivateGameUI();
		}
		m_bHidGameUIForSnapshot = false;

		SetVisible( true );
		MoveToFront();
	}

	if ( !m_bCanSubmit )
	{
		if ( m_pBugReporter && !m_pBugReporter->IsPublicUI() )
		{
			if ( !m_bCanSeeRepository )
			{
				Warning( "Bug UI disabled:  Couldn't see repository\n" );
			}
			else if ( !m_bLoggedIn )
			{
				Warning( "Bug UI disabled:  Couldn't log in to PVCS Tracker\n" );
			}
			else
			{
				Assert( 0 );
			}
		}
		
		SetVisible( false );
		return;
	}

	m_pSubmit->SetEnabled( IsValidSubmission( false ) );

	if (m_flPauseTime > 0.0f )
	{
		if ( m_flPauseTime <= system()->GetFrameTime())
		{
			m_flPauseTime = 0.0f;
		
			if ( m_pProgressDialog )
			{
				m_pProgressDialog->Close();
			}
			m_pProgressDialog = NULL;

			OnFinishBugReport();

			m_bWaitForFinish = true;

			if ( !m_hFinishedDialog.Get() )
			{
				m_hFinishedDialog = new CBugReportFinishedDialog(NULL, "FinishDialog", "#Steam_FinishedBug_WorkingTitle", "#Steam_FinishedBug_Text"  );
				m_hFinishedDialog->Activate();
				vgui::input()->SetAppModalSurface(m_hFinishedDialog->GetVPanel());
			}
		}
		else
		{
			if ( m_pProgressDialog )
			{
				float percent = clamp( 1.0f - (float)( m_flPauseTime - system()->GetFrameTime() ) / (float)PUBLIC_BUGREPORT_WAIT_TIME, 0.0f, 1.0f );
				m_pProgressDialog->SetProgress( percent );
			}
		}
	}

	if ( m_bWaitForFinish && !m_hFinishedDialog.Get() )
	{
		m_bWaitForFinish = false;
		Close();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *suffix - 
//			*buf - 
//			bufsize - 
//-----------------------------------------------------------------------------
void CBugUIPanel::GetDataFileBase( char const *suffix, char *buf, int bufsize )
{
	struct tm t;
	VCRHook_LocalTime( &t );

	char who[ 128 ];
	Q_strncpy( who, suffix, sizeof( who ) );
	Q_strlower( who );

	if ( m_pBugReporter && m_pBugReporter->IsPublicUI() )
	{
		Q_snprintf( buf, bufsize, "%i_%02i_%02i", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday );
	}
	else
	{
		Q_snprintf( buf, bufsize, "%i_%02i_%02i_%s", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, who );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBugUIPanel::GetRepositoryURL( void )
{
	const char *pURL = m_pBugReporter->GetRepositoryURL();
	if ( pURL )
		return pURL;

	return BUG_REPOSITORY_URL;
}

const char *CBugUIPanel::GetSubmissionURL( int bugid )
{
	const char *pURL = m_pBugReporter->GetSubmissionURL();
	if ( pURL )
		return pURL;
	
	static char url[512];

	Q_snprintf(url, sizeof(url), "%s/%i", GetRepositoryURL(), bugid);

	return url;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBugUIPanel::OnTakeSnapshot()
{
	GetDataFileBase( GetSubmitter(), m_szScreenShotName, sizeof( m_szScreenShotName ) );
	
	m_nSnapShotFrame = host_framecount;
	m_bTakingSnapshot = true;
	
	if ( EngineVGui()->IsGameUIVisible() )
	{
		m_bHidGameUIForSnapshot = true;
		EngineVGui()->HideGameUI();
	}
	else
	{
		m_bHidGameUIForSnapshot = false;
	}

	SetVisible( false );

	// Internal reports at 100% quality .jpg
	int quality = 100;

	if ( m_pBugReporter && m_pBugReporter->IsPublicUI() )
	{
		quality = 40;
	}

	Cbuf_AddText( va( "jpeg \"%s\" %i\n", m_szScreenShotName, quality ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBugUIPanel::OnSaveGame()
{
	GetDataFileBase( GetSubmitter(), m_szSaveGameName, sizeof( m_szSaveGameName ) );

	if ( m_pBugReporter && m_pBugReporter->IsPublicUI() )
	{
		// External users send us a "minisave" file which doesn't contain data from other previously encoudntered/connected levels
		Cbuf_AddText( va( "minisave %s\n", m_szSaveGameName ) );
	}
	else
	{
		Cbuf_AddText( va( "save %s notmostrecent copymap\n", m_szSaveGameName ) );
	}

	m_pSaveGameURL->SetText( m_szSaveGameName );
}

void CBugUIPanel::OnSaveBSP()
{
	GetDataFileBase( GetSubmitter(), m_szBSPName, sizeof( m_szBSPName ) );
	m_pBSPURL->SetText( m_szBSPName );
}

void CBugUIPanel::OnSaveVMF()
{
	if ( m_pBugReporter && m_pBugReporter->IsPublicUI() )
		return;

	char level[ 256 ];
	m_pLevelName->GetText( level, sizeof( level ) );

	// See if .vmf exists in assumed location
	char localfile[ 512 ];
	Q_snprintf( localfile, sizeof( localfile ), "%s/%s.vmf", m_szVMFContentDirFullpath, level );
	if ( !g_pFileSystem->FileExists( localfile, NULL ) )
	{
		if ( !m_hDirectorySelectDialog.Get() )
		{
			m_hDirectorySelectDialog = new DirectorySelectDialog( this, "Choose .vmf folder" );
		}

		m_bAddVMF = true;
		m_hDirectorySelectDialog->SetStartDirectory( m_szVMFContentDirFullpath );
		m_hDirectorySelectDialog->DoModal();
		return;
	}

	GetDataFileBase( GetSubmitter(), m_szVMFName, sizeof( m_szVMFName ) );
	m_pVMFURL->SetText( m_szVMFName );
}

void CBugUIPanel::OnChooseVMFFolder()
{
	if ( !m_hDirectorySelectDialog.Get() )
	{
		m_hDirectorySelectDialog = new DirectorySelectDialog( this, "Choose .vmf folder" );
	}

	m_bAddVMF = false;
	m_hDirectorySelectDialog->SetStartDirectory( m_szVMFContentDirFullpath );
	m_hDirectorySelectDialog->DoModal();
}

void CBugUIPanel::OnChooseArea(vgui::Panel *panel)
{
	if (panel != m_pGameArea)
		return;

	if (!Q_strcmp(BUG_REPORTER_DLLNAME, GetInternalBugReporterDLL()))
	{
		int area_index = m_pGameArea->GetActiveItem();
		int c = m_pBugReporter->GetLevelCount(area_index);
		int item = -1;
		const char *currentLevel = cl.IsActive() ? cl.m_szLevelBaseName : "console";

		m_pMapNumber->DeleteAllItems();
		
		for ( int i = 0; i < c; i++ )
		{
			const char *level = m_pBugReporter->GetLevel(area_index, i );
			int id = m_pMapNumber->AddItem( level, NULL );
			if (!Q_strcmp(currentLevel, level))
			{
				item = id;
			}
		}

		if (item >= 0)
		{
			m_pMapNumber->ActivateItem( item );
		}
		else
		{
			m_pMapNumber->ActivateItemByRow(0);
		}
	}
}

void CBugUIPanel::OnDirectorySelected( char const *dir )
{
	Q_strncpy( m_szVMFContentDirFullpath, dir, sizeof( m_szVMFContentDirFullpath ) );
	Q_strlower( m_szVMFContentDirFullpath );
	Q_FixSlashes( m_szVMFContentDirFullpath );
	Q_StripTrailingSlash( m_szVMFContentDirFullpath );

	if ( m_hDirectorySelectDialog != 0 )
	{
		m_hDirectorySelectDialog->MarkForDeletion();
	}

	// See if .vmf exists in assumed location
	if ( m_bAddVMF )
	{
		GetDataFileBase( GetSubmitter(), m_szVMFName, sizeof( m_szVMFName ) );
		m_pVMFURL->SetText( m_szVMFName );
	}
	m_bAddVMF = false;
}

void CBugUIPanel::OnFileSelected( char const *fullpath )
{
	bool baseDirFile = false;

	if ( m_pBugReporter && m_pBugReporter->IsPublicUI() )
		return;

	if ( !fullpath || !fullpath[ 0 ] )
		return;

	char relativepath[ 512 ];
	if ( !g_pFileSystem->FullPathToRelativePath( fullpath, relativepath, sizeof( relativepath ) ) )
	{
		if ( Q_stristr( fullpath, com_basedir ) )
		{
			Q_snprintf( relativepath, sizeof( relativepath ), "..%s", fullpath + strlen(com_basedir) );	
			baseDirFile = true;
		}
		else
		{
			Msg("Only files beneath the base game directory can be included\n" );
			return;
		}
	}

	char ext[ 10 ];
	Q_ExtractFileExtension( relativepath, ext, sizeof( ext ) );

	if ( m_hFileOpenDialog != 0 )
	{
		m_hFileOpenDialog->MarkForDeletion();
	}

	includedfile inc;
	Q_strncpy( inc.name, relativepath, sizeof( inc.name ) );
	
	if ( baseDirFile )
	{
		Q_snprintf( inc.fixedname, sizeof( inc.fixedname ), "%s", inc.name+3 ); // strip the "..\"
	}
	else
	{
		Q_snprintf( inc.fixedname, sizeof( inc.fixedname ), "%s", inc.name );
	}
	Q_FixSlashes( inc.fixedname );

	m_IncludedFiles.AddToTail( inc );

	char concat[ 8192 ];
	concat[ 0 ] = 0;
	for ( int i = 0 ; i < m_IncludedFiles.Count(); ++i )
	{
		Q_strncat( concat, m_IncludedFiles[ i ].name, sizeof( concat), COPY_ALL_CHARACTERS );
		Q_strncat( concat, "\n", sizeof( concat), COPY_ALL_CHARACTERS );
	}
	m_pIncludedFiles->SetText( concat );
}

void CBugUIPanel::OnIncludeFile()
{
	if ( m_pBugReporter && m_pBugReporter->IsPublicUI() )
		return;

	if ( !m_hFileOpenDialog.Get() )
	{
		m_hFileOpenDialog = new FileOpenDialog( this, "Choose file to include", true );
		if ( m_hFileOpenDialog != 0 )
		{
			m_hFileOpenDialog->AddFilter("*.*", "All Files (*.*)", true);
		}
	}
	if ( m_hFileOpenDialog )
	{
		char startPath[ MAX_PATH ];
		Q_strncpy( startPath, com_gamedir, sizeof( startPath ) );
		Q_FixSlashes( startPath );
		m_hFileOpenDialog->SetStartDirectory( startPath );
		m_hFileOpenDialog->DoModal( false );
	}

	//GetDataFileBase( GetSubmitter(), m_szVMFName, sizeof( m_szVMFName ) );
}

void CBugUIPanel::OnClearIncludedFiles()
{
	m_IncludedFiles.Purge();
	m_pIncludedFiles->SetText( "" );
}

//-----------------------------------------------------------------------------
// Purpose: Shows the panel
//-----------------------------------------------------------------------------
void CBugUIPanel::Activate()
{
	if ( !m_bValidated )
	{
		if ( Init() )
		{
			m_bValidated = true;
			DetermineSubmitterName();
		}
	}

	if ( m_pGameArea->GetItemCount() != 0 )
	{
		int iArea = GetArea();
		if ( iArea != 0 )
			m_pGameArea->ActivateItem( iArea );
	}

	{
		int c = m_pMapNumber->GetItemCount();
		const char *currentLevel = cl.IsActive() ? cl.m_szLevelBaseName : "console";
		int item = -1;

		for ( int i = 0; i < c; i++ )
		{
			int id = m_pMapNumber->GetItemIDFromRow(i);
			char level[256];
			m_pMapNumber->GetItemText(id, level, sizeof(level));
			if (!Q_strcmp(currentLevel, level))
			{
				item = id;
			}
		}

		if (item >= 0)
		{
			m_pMapNumber->ActivateItem( item );
		}
		else
		{
			m_pMapNumber->ActivateItemByRow(0);
		}
	}

	if ( cl.IsActive() )
	{
		Vector org = MainViewOrigin();
		QAngle ang;
		VectorAngles( MainViewForward(), ang );

		IClientEntity *localPlayer = entitylist->GetClientEntity( cl.m_nPlayerSlot + 1 );
		if ( localPlayer )
		{
			org = localPlayer->GetAbsOrigin();
		}

		m_pPosition->SetText( va( "%f %f %f", org.x, org.y, org.z ) );
		m_pOrientation->SetText( va( "%f %f %f", ang.x, ang.y, ang.z ) );

		m_pLevelName->SetText( cl.m_szLevelBaseName );
		m_pSaveGame->SetEnabled( ( cl.m_nMaxClients == 1 ) ? true : false );
		m_pSaveBSP->SetEnabled( true );
		m_pSaveVMF->SetEnabled( true );
		m_pChooseVMFFolder->SetEnabled( true );
	}
	else
	{
		m_pPosition->SetText( "console" );
		m_pOrientation->SetText( "console" );
		m_pLevelName->SetText( "console" );
		m_pSaveGame->SetEnabled( false );
		m_pSaveBSP->SetEnabled( false );
		m_pSaveVMF->SetEnabled( false );
		m_pChooseVMFFolder->SetEnabled( false );
	}

	BaseClass::Activate();

	m_pTitle->RequestFocus();
	m_pTitle->SelectAllText( true );

	// Automatically make a screenshot if requested.
	if (!m_szScreenShotName[0] )
	{
		bool bAutoAddScreenshot = false;

		// For public bugreports should be explicitly requested
		if ( m_bIsPublic )
		{
			if ( m_fAutoAddScreenshot == CBugUIPanel::eAutoAddScreenshot_Add )
			{
				bAutoAddScreenshot = true;
			}
		}
		// For internal bugreports shouldn't be explicitly denied
		else
		{
			if ( m_fAutoAddScreenshot != CBugUIPanel::eAutoAddScreenshot_DontAdd )
			{
				bAutoAddScreenshot = true;
			}
		}

		if ( bAutoAddScreenshot )
		{
			OnTakeSnapshot();
		}
	}
}

void CBugUIPanel::WipeData()
{
	m_fAutoAddScreenshot = eAutoAddScreenshot_Detect;

	m_szScreenShotName[ 0 ] = 0;
	m_pScreenShotURL->SetText( "Screenshot file" );
	m_szSaveGameName[ 0 ] = 0;
	m_pSaveGameURL->SetText( "Save game file" );
	m_szBSPName[ 0 ] = 0;
	m_pBSPURL->SetText( ".bsp file" );
	m_szVMFName[ 0 ] = 0;
	m_pVMFURL->SetText( ".vmf file" );
	m_IncludedFiles.Purge();
	m_pIncludedFiles->SetText( "" );

//	m_pTitle->SetText( "" );
	m_pDescription->SetText( "" );
	m_pEmail->SetText( "" );
}

//-----------------------------------------------------------------------------
// Purpose: method to make sure the email address is acceptable to be used by steam
//-----------------------------------------------------------------------------
bool CBugUIPanel::IsValidEmailAddress( char const *email )
{
	// basic size check
	if (!email || strlen(email) < 5) 
		return false;

	// make sure all the characters in the string are valid
	{for (const char *sz = email; *sz; sz++)
	{
		if (!V_isalnum(*sz) && *sz != '.' && *sz != '-' && *sz != '@' && *sz != '_')
			return false;
	}}

	// make sure it has letters, then the '@', then letters, then '.', then letters
	const char *sz = email;
	if (!V_isalnum(*sz))
		return false;
	sz = strstr(sz, "@");
	if (!sz)
		return false;
	sz++;
	if (!V_isalnum(*sz))
		return false;
	sz = strstr(sz, ".");
	if (!sz)
		return false;
	sz++;
	if (!V_isalnum(*sz))
		return false;
	
	return true;
}

bool CBugUIPanel::IsValidSubmission( bool verbose )
{
	if ( !m_pBugReporter )
		return false;

	bool isPublic = m_pBugReporter->IsPublicUI();

	char title[ 256 ];
	char desc[ 4096 ];

	title[ 0 ] = 0;
	desc[ 0 ] = 0;

	m_pTitle->GetText( title, sizeof( title ) );
	if ( !title[ 0 ] )
	{
		if ( verbose ) 
		{
			Warning( "Bug must have a title\n" );
		}

		return false;
	}

	// Only require description in public UI
	if ( isPublic )
	{
		m_pDescription->GetText( desc, sizeof( desc ) );
		if ( !desc[ 0 ] )
		{
			if ( verbose ) 
			{
				Warning( "Bug must have a description\n" );
			}
			return false;
		}
	}
	

	if ( !isPublic && m_pSeverity->GetActiveItem() < 0 )
	{
		if ( verbose ) 
		{
			Warning( "Severity not set!\n" );
		}
		return false;
	}

	if ( !isPublic && m_pAssignTo->GetActiveItem() < 0 )
	{
		if ( verbose ) 
		{
			Warning( "Owner not set!\n" );
		}
		return false;
	}

	char owner[ 256 ];
	Q_strncpy( owner, m_pBugReporter->GetDisplayName( m_pAssignTo->GetActiveItem() ), sizeof( owner ) );
	if ( !isPublic && !Q_stricmp( owner, "<<Unassigned>>" ) )
	{
		if ( verbose ) 
		{
			Warning( "Owner not set!\n" );
		}
		return false;
	}

	if ( !isPublic && m_pPriority->GetActiveItem() < 0 )
	{
		if ( verbose ) 
		{
			Warning( "Priority not set!\n" );
		}
		return false;
	}

	if ( !isPublic && m_pReportType->GetActiveItem() < 0 )
	{
		if ( verbose ) 
		{
			Warning( "Report Type not set!\n" );
		}
		return false;
	}

	if ( !isPublic && m_pGameArea->GetActiveItem() < 0 )
	{
		if ( verbose ) 
		{
			Warning( "Area not set!\n" );
		}
		return false;
	}

	// Public must have a selection and it can't be the "<<Choose One>>"
	if ( isPublic && m_pReportType->GetActiveItem() <= 0 )
	{
		if ( verbose )
		{
			Warning( "Report type not set!\n" );
		}
		return false;
	}

	if ( isPublic )
	{
		char email[ 80 ];
		m_pEmail->GetText( email, sizeof( email ) );
		if ( email[ 0 ] != 0 &&
			!IsValidEmailAddress( email ) )
		{
			return false;
		}
	}

	return true;
}

bool CBugUIPanel::AddBugTextToZip( char const *textfilename, char const *text, int textlen )
{
	if ( !m_hZip )
	{
		// Create using OS pagefile memory...
		m_hZip = CreateZipZ( 0, MAX_ZIP_SIZE, ZIP_MEMORY );
		Assert( m_hZip );
		if ( !m_hZip )
		{
			return false;
		}
	}

	ZipAdd( m_hZip, textfilename, (void *)text, textlen, ZIP_MEMORY );

	return true;
}


bool CBugUIPanel::AddFileToZip( char const *relative )
{
	if ( !m_hZip )
	{
		// Create using OS pagefile memory...
		m_hZip = CreateZipZ( 0, MAX_ZIP_SIZE, ZIP_MEMORY );
		Assert( m_hZip );
		if ( !m_hZip )
		{
			return false;
		}
	}

	char fullpath[ 512 ];
	if ( g_pFileSystem->RelativePathToFullPath( relative, "GAME", fullpath, sizeof( fullpath ) ) )
	{
		char extension[ 32 ];
		Q_ExtractFileExtension( relative, extension, sizeof( extension ) );
		char basename[ 256 ];
		Q_FileBase( relative, basename, sizeof( basename ) );
		char outname[ 512 ];
		Q_snprintf( outname, sizeof( outname ), "%s.%s", basename, extension );

		ZipAdd( m_hZip, outname, fullpath, 0, ZIP_FILENAME );

		return true;
	}

	return false;
}

void CBugUIPanel::OnSubmit()
{
	if ( !m_bCanSubmit )
	{
		return;
	}

	if ( !IsValidSubmission( true ) )
	{
		// Play deny sound
		DenySound();
		return;
	}

	bool isPublic = m_pBugReporter->IsPublicUI();

	char title[ 80 ];
	char desc[ 4096 ];
	char severity[ 128 ];
	char area[ 128 ];
	char mapnumber[ 128 ];
	char priority[ 128 ];
	char assignedto[ 128 ];
	char level[ 128 ];
	char position[ 128 ];
	char orientation[ 128 ];
	char build[ 128 ];
	char reporttype[ 128 ];
	char email[ 80 ];

	title[ 0 ] = 0;
	desc[ 0 ] = 0;
	severity[ 0 ] = 0;
	area[ 0 ] = 0;
	mapnumber[ 0] = 0;
	priority[ 0 ] = 0;
	assignedto[ 0 ] = 0;
	level[ 0 ] = 0;
	orientation[ 0 ] = 0;
	position[ 0 ] = 0;
	build[ 0 ] = 0;
	reporttype [ 0 ] = 0;
	email[ 0 ] = 0;

	Assert( m_pBugReporter );

	// Stuff bug data files up to server
	m_pBugReporter->StartNewBugReport();


	char temp[ 80 ];
	m_pTitle->GetText( temp, sizeof( temp ) );

	if ( host_state.worldmodel )
	{
		char mapname[256];
		CL_SetupMapName( modelloader->GetName( host_state.worldmodel ), mapname, sizeof( mapname ) );

		Q_snprintf( title, sizeof( title ), "%s: %s", mapname, temp );
	}
	else
	{
		Q_snprintf( title, sizeof( title ), "%s", temp );
	}

	Msg( "title:  %s\n", title );

	m_pDescription->GetText( desc, sizeof( desc ) );

	Msg( "description:  %s\n", desc );

	m_pLevelName->GetText( level, sizeof( level ) );
	m_pPosition->GetText( position, sizeof( position ) );
	m_pOrientation->GetText( orientation, sizeof( orientation ) );
	m_pBuildNumber->GetText( build, sizeof( build ) );

	Q_strncat( build, " (Steam)", sizeof(build), COPY_ALL_CHARACTERS );

	MaterialAdapterInfo_t info;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), info );
	char driverinfo[ 2048 ];

	char const *dxlevel = "Unk";
	if ( g_pMaterialSystemHardwareConfig )
	{
		dxlevel = COM_DXLevelToString( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() ) ;
	}

	char osversion[ 256 ];
	DisplaySystemVersion( osversion, sizeof( osversion ) );

	Q_snprintf( driverinfo, sizeof( driverinfo ), 
		"OS Version:  %s\n"
		"Driver Name:  %s\n"
		"VendorId / DeviceId:  0x%x / 0x%x\n"
		"SubSystem / Rev:  0x%x / 0x%x\n"
		"DXLevel:  %s\nVid:  %i x %i\n"
		"Framerate:  %.3f\n",
		osversion,
		info.m_pDriverName,
		info.m_VendorID,
		info.m_DeviceID,
		info.m_SubSysID,
		info.m_Revision,
		dxlevel ? dxlevel : "Unk",
		videomode->GetModeWidth(), videomode->GetModeHeight(),
		g_fFramesPerSecond );

	Msg( "%s", driverinfo );

	int latency = 0;
	if ( cl.m_NetChannel )
	{
		latency = (int)( 1000.0f * cl.m_NetChannel->GetAvgLatency( FLOW_OUTGOING ) );
	}
	
	ConVarRef host_thread_mode( "host_thread_mode" );
	ConVarRef sv_alternateticks( "sv_alternateticks" );
	ConVarRef ai_strong_optimizations( "ai_strong_optimizations" );

	char misc[ 1024 ];
	Q_snprintf( misc, sizeof( misc ), "Convars:\n\tskill:  %i\n\tnet:  rate %i update %i cmd %i latency %i msec\n\thost_thread_mode:  %i\n\tsv_alternateticks:  %i\n\tai_strong_optimizations:  %i\n", 
		skill.GetInt(),
		cl_rate->GetInt(),
		(int)cl_updaterate->GetFloat(),
		(int)cl_cmdrate->GetFloat(),
		latency,
		host_thread_mode.GetInt(),
		sv_alternateticks.GetInt(),
		ai_strong_optimizations.GetInt()
	);

	if ( cl.IsActive() && g_ServerGlobalVariables.mapversion != 0 && host_state.worldmodel )
	{
		// Note, this won't work in multiplayer, oh well...
		extern CGlobalVars g_ServerGlobalVariables;
		char misc2[ 256 ];

		long mapfiletime = g_pFileSystem->GetFileTime( modelloader->GetName( host_state.worldmodel ), "GAME" );
		if ( !isPublic && mapfiletime != 0L )
		{
			char filetimebuf[ 64 ];
			g_pFileSystem->FileTimeToString( filetimebuf, sizeof( filetimebuf ), mapfiletime );

			Q_snprintf( misc2, sizeof( misc2 ), "Map version:  %i\nFile timestamp:  %s", g_ServerGlobalVariables.mapversion, filetimebuf );
			Q_strncat( misc, misc2, sizeof( misc ), COPY_ALL_CHARACTERS );
		}
		else
		{
			Q_snprintf( misc2, sizeof( misc2 ), "Map version:  %i\n", g_ServerGlobalVariables.mapversion );
			Q_strncat( misc, misc2, sizeof( misc ), COPY_ALL_CHARACTERS );
		}
	}

	if ( sv.IsActive() && serverGameClients )
	{
		char gamedlldata[ 2048 ];
		Q_memset( gamedlldata, 0, sizeof( gamedlldata ) );
		serverGameClients->GetBugReportInfo( gamedlldata, sizeof( gamedlldata ) );
		Q_strncat( misc, gamedlldata, sizeof( misc ), COPY_ALL_CHARACTERS );
	}

	{
		char misc2[ 128 ];
		Q_snprintf( misc2, sizeof( misc2 ), "gamedir:  %s\n", com_gamedir );
		Q_strncat( misc, misc2, sizeof( misc ), COPY_ALL_CHARACTERS );
		//Q_snprintf( misc2, sizeof( misc2 ), "game:  %s\n", COM_GetModDirectory() );
		//Q_strncat( misc, misc2, sizeof( misc ), COPY_ALL_CHARACTERS );
	}

	Msg( "%s", misc );

	if ( !isPublic )
	{
		m_pSeverity->GetText( severity, sizeof( severity ) );
		Msg( "severity %s\n", severity );

		m_pGameArea->GetText( area, sizeof( area ) );
		Msg( "area %s\n", area );

		m_pMapNumber->GetText( mapnumber, sizeof( mapnumber) );
		Msg( "map number %s\n", mapnumber);

		m_pPriority->GetText( priority, sizeof( priority ) );
		Msg( "priority %s\n", priority );

		m_pAssignTo->GetText( assignedto, sizeof( assignedto ) );
		Msg( "owner %s\n", assignedto );
	}

	if ( isPublic )
	{
		m_pEmail->GetText( email, sizeof( email ) );
		if ( Q_strlen( email ) > 0 )
		{
			Msg( "email %s\n", email );
		}
		else
		{
			Msg( "Not sending email address\n" );
		}

		m_pBugReporter->SetOwner( email );
		char submitter[ 80 ];
		m_pSubmitterLabel->GetText( submitter, sizeof( submitter ) );
		m_pBugReporter->SetSubmitter( submitter );
	}
	else
	{
		m_pBugReporter->SetOwner( m_pBugReporter->GetUserNameForDisplayName( assignedto ) );

		if ( m_bUseNameForSubmitter )
		{
			char submitter[ 80 ];
			m_pSubmitter->GetText( submitter, sizeof( submitter ) );
			m_pBugReporter->SetSubmitter( submitter );
		}
		else
		{
			m_pBugReporter->SetSubmitter( NULL );
		}
	}

	m_pReportType->GetText( reporttype, sizeof( reporttype ) );
	Msg( "report type %s\n", reporttype );

	Msg( "submitter %s\n", m_pBugReporter->GetUserName() );

	Msg( "level %s\n", level );
	Msg( "position %s\n", position );
	Msg( "orientation %s\n", orientation );
	Msg( "build %s\n", build );
	
	if ( m_szSaveGameName[ 0 ] )
	{
		Msg( "save file save/%s.sav\n", m_szSaveGameName );
	}
	else
	{
		Msg( "no save game\n" );
	}

	if ( m_szScreenShotName[ 0 ] )
	{
		Msg( "screenshot screenshots/%s.jpg\n", m_szScreenShotName );
	}
	else
	{
		Msg( "no screenshot\n" );
	}

	if ( !isPublic )
	{
		if ( m_szBSPName[ 0 ] )
		{
			Msg( "bsp file maps/%s.bsp\n", m_szBSPName );
		}

		if ( m_szVMFName[ 0 ] )
		{
			Msg( "vmf file maps/%s.vmf\n", m_szVMFName );
		}

		if ( m_IncludedFiles.Count() > 0 )
		{
			for ( int i = 0; i < m_IncludedFiles.Count(); ++i )
			{
				Msg( "Include:  %s\n", m_IncludedFiles[ i ].name );
			}
		}
	}


	m_pBugReporter->SetTitle( title );
	m_pBugReporter->SetDescription( desc );
	m_pBugReporter->SetLevel( level );
	m_pBugReporter->SetPosition( position );
	m_pBugReporter->SetOrientation( orientation );
	m_pBugReporter->SetBuildNumber( build );

	m_pBugReporter->SetSeverity( severity );
	m_pBugReporter->SetPriority( priority );
	m_pBugReporter->SetArea( area );
	m_pBugReporter->SetMapNumber( mapnumber );
	m_pBugReporter->SetReportType( reporttype );
	m_pBugReporter->SetDriverInfo( driverinfo );
	m_pBugReporter->SetMiscInfo( misc );

	m_pBugReporter->SetCSERAddress( m_cserIP );
	m_pBugReporter->SetExeName( "hl2.exe" );
	m_pBugReporter->SetGameDirectory( com_gamedir );

	const CPUInformation& pi = *GetCPUInformation();

	m_pBugReporter->SetRAM( GetRam() );

	double fFrequency = pi.m_Speed / 1000000.0;

	m_pBugReporter->SetCPU( (int)fFrequency );

	m_pBugReporter->SetProcessor( pi.m_szProcessorID );

	int nDxLevel = g_pMaterialSystemHardwareConfig->GetDXSupportLevel();
	int vHigh = nDxLevel / 10;
	int vLow = nDxLevel - vHigh * 10;

	m_pBugReporter->SetDXVersion( vHigh, vLow, info.m_VendorID, info.m_DeviceID );

	m_pBugReporter->SetOSVersion( osversion );

	m_pBugReporter->ResetIncludedFiles();
	m_pBugReporter->SetZipAttachmentName( "" );

	char fn[ 512 ];
	if ( m_pBugReporter->IsPublicUI() )
	{
		bool attachedSave = false;
		bool attachedScreenshot = false;

		CUtlBuffer buginfo( 0, 0, CUtlBuffer::TEXT_BUFFER );

		buginfo.Printf( "Title:  %s\n", title );
		buginfo.Printf( "Description:  %s\n\n", desc );
		buginfo.Printf( "Level:  %s\n", level );
		buginfo.Printf( "Position:  %s\n", position );
		buginfo.Printf( "Orientation:  %s\n", orientation );
		buginfo.Printf( "BuildNumber:  %s\n", build );
		buginfo.Printf( "DriverInfo:  %s\n", driverinfo );
		buginfo.Printf( "Misc:  %s\n", misc );
		buginfo.Printf( "Exe:  %s\n", "hl2.exe" );
		char gd[ 256 ];
		Q_FileBase( com_gamedir, gd, sizeof( gd ) );
		buginfo.Printf( "GameDirectory:  %s\n", gd );
		buginfo.Printf( "Ram:  %lu\n", GetRam() );
		buginfo.Printf( "CPU:  %i\n", (int)fFrequency );
		buginfo.Printf( "Processor:  %s\n", pi.m_szProcessorID );
		buginfo.Printf( "DXLevel:  %d\n", nDxLevel );
		buginfo.Printf( "OSVersion:  %s\n", osversion );
		// Terminate it
		buginfo.PutChar( 0 );


		int maxlen = buginfo.TellPut() * 2 + 1;
		char *fixed = new char [ maxlen ];
		Assert( fixed );
		if ( fixed )
		{
			Q_memset( fixed, 0, maxlen );

			char *i = (char *)buginfo.Base();
			char *o = fixed;
			while ( *i && ( o - fixed ) < maxlen - 1 )
			{
				if ( *i == '\n' )
				{
					*o++ = '\r';
				}
				*o++ = *i++;
			}
			*o = '\0';

			AddBugTextToZip( "info.txt", fixed, Q_strlen( fixed ) + 1 );

			delete[] fixed;
		}
		else
		{
			Sys_Error( "Unable to allocate %i bytes for bug description\n", maxlen );
		}

		// Only attach .sav files in single player
		if ( ( cl.m_nMaxClients == 1 ) && m_szSaveGameName[ 0 ] )
		{
			Q_snprintf( fn, sizeof( fn ), "save/%s.sav", m_szSaveGameName );
			Q_FixSlashes( fn );
			attachedSave = AddFileToZip( fn );
		}
		if ( m_szScreenShotName[ 0 ] )
		{
			Q_snprintf( fn, sizeof( fn ), "screenshots/%s.jpg", m_szScreenShotName );
			Q_FixSlashes( fn );
			attachedScreenshot = AddFileToZip( fn );
		}

		// End users can only send save games and screenshots to valve
		// Don't bother uploading any attachment if it's just the info.txt file, either...
		if ( m_hZip && ( attachedSave || attachedScreenshot ) )
		{
			Assert( m_hZip );
			void *mem;
			unsigned long len;

			ZipGetMemory( m_hZip, &mem, &len );
			if ( mem != NULL 
				 && len > 0 )
			{
				// Store .zip file
				FileHandle_t fh = g_pFileSystem->Open( "bug.zip", "wb" );
				if ( FILESYSTEM_INVALID_HANDLE != fh )
				{
					g_pFileSystem->Write( mem, len, fh );
					g_pFileSystem->Close( fh );

					m_pBugReporter->SetZipAttachmentName( "bug.zip" );
				}
			}
		}

		if ( m_hZip )
		{
			CloseZip( m_hZip );
			m_hZip = (HZIP)0;
		}

		uint64 un64SteamID = m_SteamID.ConvertToUint64();
		m_pBugReporter->SetSteamUserID( &un64SteamID, sizeof( un64SteamID ) );
	}
	else
	{
		// Notify other players that we've submitted a bug
		if ( cl.IsActive() && cl.m_nMaxClients > 1 )
		{
			char buf[256];
			Q_snprintf( buf, sizeof(buf), "say \"Bug Submitted: \'%s\'\"", title );
			Cbuf_AddText( buf );
		}

		if ( m_szSaveGameName[ 0 ] )
		{
			Q_snprintf( fn, sizeof( fn ), "%s/BugId/%s.sav", GetRepositoryURL(), m_szSaveGameName );
			Q_FixSlashes( fn );
			m_pBugReporter->SetSaveGame( fn );
		}
		if ( m_szScreenShotName[ 0 ] )
		{
			Q_snprintf( fn, sizeof( fn ), "%s/BugId/%s.jpg", GetRepositoryURL(), m_szScreenShotName );
			Q_FixSlashes( fn );
			m_pBugReporter->SetScreenShot( fn );
		}
		if ( m_szBSPName[ 0 ] )
		{
			Q_snprintf( fn, sizeof( fn ), "%s/BugId/%s.bsp", GetRepositoryURL(), m_szBSPName );
			Q_FixSlashes( fn );
			m_pBugReporter->SetBSPName( fn );
		}
		if ( m_szVMFName[ 0 ] )
		{
			Q_snprintf( fn, sizeof( fn ), "%s/BugId/%s.vmf", GetRepositoryURL(), m_szVMFName );
			Q_FixSlashes( fn );
			m_pBugReporter->SetVMFName( fn );
		}


		if ( m_IncludedFiles.Count() > 0 )
		{
			for ( int i = 0; i < m_IncludedFiles.Count(); ++i )
			{
				Q_snprintf( fn, sizeof( fn ), "%s/BugId/%s", GetRepositoryURL(), m_IncludedFiles[ i ].fixedname );
				Q_FixSlashes( fn );
				m_pBugReporter->AddIncludedFile( fn );
			}
		}
	}

	Q_strncpy( m_szLevel, level, sizeof( m_szLevel ) );

	if ( m_pBugReporter->IsPublicUI() )
	{
		m_pProgressDialog = new CBugReportUploadProgressDialog(NULL, "ProgressDialog", "#Steam_SubmittingBug_WorkingTitle", "#Steam_SubmittingBug_WorkingText"  );
		m_pProgressDialog->Activate();
		vgui::input()->SetAppModalSurface(m_pProgressDialog->GetVPanel());
		m_flPauseTime = (float)system()->GetFrameTime() + PUBLIC_BUGREPORT_WAIT_TIME;
	}
	else
	{
		OnFinishBugReport();
	}
}

void CBugUIPanel::OnFinishBugReport()
{
	int bugId = -1;

	bool success = m_pBugReporter->CommitBugReport( bugId );
	if ( success )
	{
		// The public UI handles uploading on it's own...
		if ( !m_pBugReporter->IsPublicUI() )
		{
			if ( !UploadBugSubmission( m_szLevel, bugId, m_szSaveGameName, m_szScreenShotName, m_szBSPName, m_szVMFName, m_IncludedFiles ) )
			{
				Warning( "Unable to upload saved game and screenshot to bug repository!\n" );
				success = false;
			}
		}
	}
	else
	{
		Warning( "Unable to post bug report to database\n" );
	}

	if ( !success )
	{
		// Play deny sound
		DenySound();
		m_bWaitForFinish = false;
	}
	else
	{
		// Close
		WipeData();
		SuccessSound( bugId );

		if ( !m_pBugReporter->IsPublicUI() )
		{
			Close();
		}
	}
}

void NonFileSystem_CreatePath (const char *path)
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

#ifdef LINUX
#define COPYFILE_ALL 0
#define BSIZE 65535
int copyfile( const char *local, const char *remote, void *ignored, int ignoredFlags )
{
	ssize_t bytes;
	int fps, fpd;
	char buffer[BSIZE];

	if ( (fps = open( local , O_RDONLY ) ) == -1 ) 
		return -1;
	
	if ( (fpd = open( remote, O_WRONLY | O_CREAT | O_TRUNC, 0644 ) ) == -1 )
		return -1;

	while((bytes = read(fps, buffer, BSIZE)) > 0)
		write(fpd, buffer, bytes);

	close(fpd);
	close(fps);
	return 0;
}
#endif

bool CBugUIPanel::UploadFile( char const *local, char const *remote, bool bDeleteLocal )
{
	Msg( "Uploading %s to %s\n", local, remote );
	FileHandle_t hLocal = g_pFileSystem->Open( local, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE == hLocal )
	{
		Warning( "CBugUIPanel::UploadFile:  Unable to open local path '%s'\n", local );
		return false;
	}

	int nLocalFileSize = g_pFileSystem->Size( hLocal );
	if ( nLocalFileSize <= 0 )
	{
		Warning( "CBugUIPanel::UploadFile:  Local file has 0 size '%s'\n", local );
		g_pFileSystem->Close( hLocal );
		return false;
	}
	NonFileSystem_CreatePath( remote );

	bool bResult;
	if ( !g_pFileSystem->IsSteam() )
	{
		g_pFileSystem->Close( hLocal );
#ifdef WIN32
		bResult = CopyFile( local, remote, false ) ? true : false;
#elif POSIX
		bResult = (0 == copyfile( local, remote, NULL, COPYFILE_ALL )); 
#else
#error
#endif
	}
	else
	{

		FILE *r = fopen( va( "%s", remote ), "wb" );
		if ( !r )
		{
			Warning( "CBugUIPanel::UploadFile:  Unable to open remote path '%s'\n", remote );
			g_pFileSystem->Close( hLocal );
			return false;
		}

		int nCopyBufferSize = 2 * 1024 * 1024;

		byte *pCopyBuf = new byte[ nCopyBufferSize ];
		Assert( pCopyBuf );
		if ( !pCopyBuf )
		{
			Warning( "CBugUIPanel::UploadFile:  Unable to allocate copy buffer of %d bytes\n", nCopyBufferSize );
			fclose( r );
			g_pFileSystem->Close( hLocal );
			return false;
		}

		int nRemainingBytes = nLocalFileSize;
		while ( nRemainingBytes > 0 )
		{
			int nBytesToCopy = MIN( nRemainingBytes, nCopyBufferSize );
			g_pFileSystem->Read( pCopyBuf, nBytesToCopy, hLocal );
			fwrite( pCopyBuf, nBytesToCopy, 1, r );
			nRemainingBytes -= nBytesToCopy;
		}

		fclose( r );
		g_pFileSystem->Close( hLocal );

		delete[] pCopyBuf;

		bResult = true;
	}

	if ( !bResult )
	{
		Warning( "Failed to upload %s, error %d\n", local, GetLastError() );
	}
	else if ( bDeleteLocal )
	{
		unlink( local );
	}
	return bResult;
}

CCallQueue g_UploadQueue;

bool CBugUIPanel::UploadBugSubmission( char const *levelname, int bugId, char const *savefile, char const *screenshot, char const *bsp, char const *vmf,
	CUtlVector< includedfile >& files )
{
	bool bret = true;
	bool bAsync = bugreporter_uploadasync.GetBool();

	char localfile[ 512 ];
	char remotefile[ 512 ];

	if ( savefile && savefile[ 0 ] )
	{
		Q_snprintf( localfile, sizeof( localfile ), "%s/save/%s.sav", com_gamedir, savefile );
		Q_snprintf( remotefile, sizeof( remotefile ), "%s/%s.sav", GetSubmissionURL(bugId), savefile );
		Q_FixSlashes( localfile );
		Q_FixSlashes( remotefile );

		g_UploadQueue.QueueCall( this, &CBugUIPanel::UploadFile, CUtlEnvelope<const char *>(localfile), CUtlEnvelope<const char *>(remotefile), false );
	}

	if ( screenshot && screenshot[ 0 ] )
	{
		Q_snprintf( localfile, sizeof( localfile ), "%s/screenshots/%s.jpg", com_gamedir, screenshot );
		Q_snprintf( remotefile, sizeof( remotefile ), "%s/%s.jpg", GetSubmissionURL(bugId), screenshot );
		Q_FixSlashes( localfile );
		Q_FixSlashes( remotefile );
		
		g_UploadQueue.QueueCall( this, &CBugUIPanel::UploadFile, CUtlEnvelope<const char *>(localfile), CUtlEnvelope<const char *>(remotefile), false );
	}

	if ( bsp && bsp[ 0 ] )
	{
		Q_snprintf( localfile, sizeof( localfile ), "maps/%s.bsp", levelname );
		char *pszMapPath;
		FileHandle_t hBsp = g_pFileSystem->OpenEx( localfile, "rb", 0, 0, &pszMapPath );
		if ( !hBsp )
		{
			Q_snprintf( localfile, sizeof( localfile ), "%s/maps/%s.bsp", com_gamedir, levelname );
		}
		else
		{
			V_strncpy( localfile, pszMapPath, sizeof( localfile ) );
			delete pszMapPath;
			g_pFileSystem->Close( hBsp );
		}
		Q_snprintf( remotefile, sizeof( remotefile ), "%s/%s.bsp", GetSubmissionURL(bugId), bsp );
		Q_FixSlashes( localfile );
		Q_FixSlashes( remotefile );
		
		g_UploadQueue.QueueCall( this, &CBugUIPanel::UploadFile, CUtlEnvelope<const char *>(localfile), CUtlEnvelope<const char *>(remotefile), false );
	}

	if ( vmf && vmf[ 0 ] )
	{
		Q_snprintf( localfile, sizeof( localfile ), "%s/%s.vmf", m_szVMFContentDirFullpath, levelname );
		if ( g_pFileSystem->FileExists( localfile, NULL ) )
		{
			Q_snprintf( remotefile, sizeof( remotefile ), "%s/%s.vmf", GetSubmissionURL(bugId), vmf );
			Q_FixSlashes( localfile );
			Q_FixSlashes( remotefile );
		
			g_UploadQueue.QueueCall( this, &CBugUIPanel::UploadFile, CUtlEnvelope<const char *>(localfile), CUtlEnvelope<const char *>(remotefile), false );
		}
		else
		{
			Msg( "Unable to locate .vmf file %s\n", localfile );
		}
	}

	if ( files.Count() > 0 )
	{
		bAsync = false;
		int c = files.Count();
		for ( int i = 0 ; i < c; ++i )
		{
			Q_snprintf( localfile, sizeof( localfile ), "%s/%s", com_gamedir, files[ i ].name );
			Q_snprintf( remotefile, sizeof( remotefile ), "%s/%s", GetSubmissionURL(bugId), files[ i ].fixedname );
			Q_FixSlashes( localfile );
			Q_FixSlashes( remotefile );
			
			g_UploadQueue.QueueCall( this, &CBugUIPanel::UploadFile, CUtlEnvelope<const char *>(localfile), CUtlEnvelope<const char *>(remotefile), false );
		}
	}

	if ( !bAsync )
	{
		g_UploadQueue.CallQueued();
	}
	else
	{
		ThreadExecuteSolo( "BugUpload", &g_UploadQueue, &CCallQueue::CallQueued );
	}

	return bret;
}

void CBugUIPanel::Close()
{
	WipeData();
	BaseClass::Close();
}

void CBugUIPanel::OnCommand( char const *command )
{
	if ( !Q_strcasecmp( command, "submit" ) )
	{
		OnSubmit();
	}
	else if ( !Q_strcasecmp( command, "cancel" ) )
	{
		Close();
		WipeData();
	}
	else if ( !Q_strcasecmp( command, "snapshot" ) )
	{
		OnTakeSnapshot();
	}
	else if ( !Q_strcasecmp( command, "savegame" ) )
	{
		OnSaveGame();
		
		//Adrian: We always want the BSP you used when saving the game.
		//But only if you're not the public bug reporter!
		if ( bugreporter_includebsp.GetBool() &&
			 m_pBugReporter->IsPublicUI() == false ) 
		{
			OnSaveBSP();
		}
	}
	else if ( !Q_strcasecmp( command, "savebsp" ) )
	{
		OnSaveBSP();
	}
	else if ( !Q_strcasecmp( command, "savevmf" ) )
	{
		OnSaveVMF();
	}
	else if ( !Q_strcasecmp( command, "clearform" ) )
	{
		OnClearForm();
	}
	else if ( !Q_strcasecmp( command, "addfile" ) )
	{
		OnIncludeFile();
	}
	else if ( !Q_strcasecmp( command, "clearfiles" ) )
	{
		OnClearIncludedFiles();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CBugUIPanel::OnClearForm()
{
	WipeData();

	m_pTitle->SetText( "" );
	m_pDescription->SetText( "" );

	m_pAssignTo->ActivateItem( 0 );
	m_pSeverity->ActivateItem( 0 );
	m_pReportType->ActivateItem( 0 );
	m_pPriority->ActivateItem( 2 );
	m_pGameArea->ActivateItem( 0 );
	m_pMapNumber->ActivateItem( 0 );
}

void CBugUIPanel::DetermineSubmitterName()
{
	if ( !m_pBugReporter )
		return;

	if ( m_pBugReporter->IsPublicUI() )
	{
		m_pSubmitter->SetText( "PublicUser" );
		m_bCanSeeRepository = true;
		m_bCanSubmit = true;
	}
	else
	{
		Color clr( 100, 200, 255, 255 );

		const char *pUserName = m_pBugReporter->GetUserName();
		const char *pUserDisplayName = m_pBugReporter->GetUserName_Display();

		if ( pUserName && pUserName[0] && pUserDisplayName && pUserDisplayName[0] )
		{
			ConColorMsg( clr, "Username '%s' -- '%s'\n", pUserName, pUserDisplayName );
		}
		else if ( cl.IsActive() )
		{
			m_bUseNameForSubmitter = true;
			pUserDisplayName = cl_name.GetString();
			ConColorMsg( clr, "Using '%s' as bug submission name.\n", pUserDisplayName );
		}
		else
		{
			ConColorMsg( clr, "Failed to determine bug submission name.\n" );
		}

		// See if we can see the bug repository right now
		char fn[ 512 ];
		Q_snprintf( fn, sizeof( fn ), "%s/%s", GetRepositoryURL(), REPOSITORY_VALIDATION_FILE );
		Q_FixSlashes( fn );

		FILE *fp = fopen( fn, "rb" );
		if ( fp )
		{
			ConColorMsg( clr, "Bug Repository '%s'\n", GetRepositoryURL() );
			fclose( fp );

			m_bCanSeeRepository = true;
		}
		else
		{
			Warning( "Unable to see '%s', check permissions and network connectivity\n", fn );
			m_bCanSubmit = false;
		}

		m_pSubmitter->SetText( pUserDisplayName );
	}
}

void CBugUIPanel::PopulateControls()
{
	if ( !m_pBugReporter )
		return;

	m_pAssignTo->DeleteAllItems();
	int i;
	int c = m_pBugReporter->GetDisplayNameCount();
	int defitem = 0;
	for ( i = 0; i < c; i++ )
	{
		char const  *name = m_pBugReporter->GetDisplayName( i );
		if (!V_strcasecmp(name, "Triage"))
			defitem = i;
		m_pAssignTo->AddItem(name , NULL );
	}
	m_pAssignTo->ActivateItem( defitem );
	
	defitem = 0;
	m_pSeverity->DeleteAllItems();
	c = m_pBugReporter->GetSeverityCount();
	for ( i = 0; i < c; i++ )
	{
		char const  *severity = m_pBugReporter->GetSeverity( i );
		if (!V_strcasecmp(severity, "Zero"))
			defitem = i;
		m_pSeverity->AddItem( severity, NULL );
	}
	m_pSeverity->ActivateItem( defitem );

	m_pReportType->DeleteAllItems();
	c = m_pBugReporter->GetReportTypeCount();
	for ( i = 0; i < c; i++ )
	{
		m_pReportType->AddItem( m_pBugReporter->GetReportType( i ), NULL );
	}
	m_pReportType->ActivateItem( 0 );

	m_pPriority->DeleteAllItems();
	c = m_pBugReporter->GetPriorityCount();
	for ( i = 0; i < c; i++ )
	{
		m_pPriority->AddItem( m_pBugReporter->GetPriority( i ), NULL );
	}
	m_pPriority->ActivateItem( 2 );

	m_pGameArea->DeleteAllItems();
	c = m_pBugReporter->GetAreaCount();
	for ( i = 0; i < c; i++ )
	{
		m_pGameArea->AddItem( m_pBugReporter->GetArea( i ), NULL );
	}

	int area_index = GetArea();
	m_pGameArea->ActivateItem( area_index );
}

char const *CBugUIPanel::GetSubmitter()
{
	if ( !m_bCanSubmit )
		return "";
	return m_pBugReporter->GetUserName();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBugUIPanel::DenySound()
{
	Cbuf_AddText( va( "play %s\n", DENY_SOUND ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBugUIPanel::SuccessSound( int bugId )
{
	Color clr( 50, 255, 100, 255 );

	if ( m_pBugReporter && m_pBugReporter->IsPublicUI() )
	{
		ConColorMsg( clr, "Bug submission succeeded\n" );
	}
	else
	{
		ConColorMsg( clr, "Bug submission succeeded for bug (%i)\n", bugId );
	}
	Cbuf_AddText( va( "play %s\n", SUCCEED_SOUND ) );
}

void CBugUIPanel::OnKeyCodePressed(KeyCode code)
{
	if ( code == KEY_ESCAPE || 
		 GetBaseButtonCode( code ) == KEY_XBUTTON_B )
	{
		Close();
		WipeData();
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}


// Load game-specific bug reporter defaults as params
void CBugUIPanel::ParseDefaultParams( void )
{
	const char *szDefaults = "scripts/bugreporter_defaults.txt";

	FileHandle_t hLocal = g_pFileSystem->Open( szDefaults, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE == hLocal )
	{
		return;
	}

	// load file into a null-terminated buffer
	int fileSize = g_pFileSystem->Size(hLocal);
	char *buffer = (char*)MemAllocScratch(fileSize + 1);

	Assert(buffer);

	g_pFileSystem->Read(buffer, fileSize, hLocal); // read into local buffer
	buffer[fileSize] = 0; // null terminate file as EOF
	g_pFileSystem->Close( hLocal );	// close file after reading

	char token[64];
	const char *pfile = COM_ParseFile(buffer, token, sizeof( token ) );

	while ( pfile )
	{
		bool success = AutoFillToken( token, false );
		if ( !success )
		{
			// Try partials
			success = AutoFillToken( token, true );
			if ( !success )
			{
				Msg( "Unable to determine where to set default bug parameter '%s', ignoring...\n", token );
			}
		}

		pfile = COM_ParseFile(pfile, token, sizeof( token ) );
	}
}

void CBugUIPanel::ParseCommands( const CCommand &args )
{
	if ( !m_bCanSubmit )
		return;

	for ( int i = 1; i < args.ArgC(); i++ )
	{
		bool success = AutoFillToken( args[ i ], false );
		if ( !success )
		{
			// Try partials
			success = AutoFillToken( args[ i ], true );
			if ( !success )
			{
				Msg( "Unable to determine where to set bug parameter '%s', ignoring...\n", args[ i ] );
			}
		}
	}
}

bool CBugUIPanel::Compare( char const *value, char const *token, bool partial )
{
	if ( !partial )
	{
		if ( !Q_stricmp( value, token ) )
			return true;
	}
	else
	{
		if ( Q_stristr( value, token ) )
		{
			return true;
		}
	}

	return false;
}


bool CBugUIPanel::AutoFillToken( char const *token, bool partial )
{
	// Search for token in all dropdown lists and fill it in if we find it
	if ( !m_pBugReporter )
		return true;

	int i;
	int c;
	
	c = m_pBugReporter->GetDisplayNameCount();
	for ( i = 0; i < c; i++ )
	{
		if ( Compare( m_pBugReporter->GetDisplayName( i ), token, partial ) )
		{
			m_pAssignTo->ActivateItem( i );
			return true;
		}
	}
	
	c = m_pBugReporter->GetSeverityCount();
	for ( i = 0; i < c; i++ )
	{
		if ( Compare( m_pBugReporter->GetSeverity( i ), token, partial ) )
		{
			m_pSeverity->ActivateItem( i );
			return true;
		}
	}

	c = m_pBugReporter->GetReportTypeCount();
	for ( i = 0; i < c; i++ )
	{
		if ( Compare( m_pBugReporter->GetReportType( i ), token, partial ) )
		{
			m_pReportType->ActivateItem( i );
			return true;
		}
	}

	c = m_pBugReporter->GetPriorityCount();
	for ( i = 0; i < c; i++ )
	{
		if ( Compare( m_pBugReporter->GetPriority( i ), token, partial ) )
		{
			m_pPriority->ActivateItem( i );
			return true;
		}
	}

	c = m_pBugReporter->GetAreaCount();
	for ( i = 0; i < c; i++ )
	{
		if ( Compare( m_pBugReporter->GetArea( i ), token, partial ) )
		{
			m_pGameArea->ActivateItem( i );
			return true;
		}
	}

	if ( !Q_stricmp( token, "screenshot" ) )
	{
		m_fAutoAddScreenshot = eAutoAddScreenshot_Add;
		return true;
	}

	if ( !Q_stricmp( token, "noscreenshot" ) )
	{
		m_fAutoAddScreenshot = eAutoAddScreenshot_DontAdd;
		return true;
	}

	return false;
}

void CBugUIPanel::CheckContinueQueryingSteamForCSERList()
{
	if ( !m_bQueryingSteamForCSER || !Steam3Client().SteamUtils() )
	{
		return;
	}
	
	uint32 unIP;
	uint16 usPort;
	Steam3Client().SteamUtils()->GetCSERIPPort( &unIP, &usPort );
	if ( unIP )
	{
		m_cserIP.SetIPAndPort( unIP, usPort );
		m_bQueryingSteamForCSER = false;
	}
}

static CBugUIPanel *g_pBugUI = NULL;

class CEngineBugReporter : public IEngineBugReporter
{
public:
	virtual void		Init( void );
	virtual void		Shutdown( void );

	virtual void		InstallBugReportingUI( vgui::Panel *parent, IEngineBugReporter::BR_TYPE type );
	virtual bool		ShouldPause() const;

	virtual bool		IsVisible() const; //< true iff the bug panel is active and on screen right now

	void				Restart( IEngineBugReporter::BR_TYPE type );
private:
	PHandle				m_ParentPanel;
};

static CEngineBugReporter g_BugReporter;
IEngineBugReporter *bugreporter = &g_BugReporter;

CON_COMMAND( _bugreporter_restart, "Restarts bug reporter .dll" )
{
	if ( args.ArgC() <= 1 )
	{
		Msg( "__bugreporter_restart <internal | external | autoselect>\n" );
		return;
	}

	IEngineBugReporter::BR_TYPE type = IEngineBugReporter::BR_PUBLIC;

	if ( !Q_stricmp( args.Arg( 1 ), "internal" ) )
	{
		type = IEngineBugReporter::BR_INTERNAL;
	}
	else if ( !Q_stricmp( args.Arg( 1 ), "autoselect" ) )
	{
		type = IEngineBugReporter::BR_AUTOSELECT;
	}

	g_BugReporter.Restart( type );
}

void CEngineBugReporter::Init( void )
{
	m_ParentPanel = 0;
}

void CEngineBugReporter::Shutdown( void )
{
	if ( g_pBugUI )
	{
		g_pBugUI->Shutdown();
		g_pBugUI = NULL;
	}
}

void CEngineBugReporter::InstallBugReportingUI( vgui::Panel *parent, IEngineBugReporter::BR_TYPE type )
{
	if ( g_pBugUI )
		return;

	bool bIsPublic = true;
	
	char fn[ 512 ];

	Q_snprintf( fn, sizeof( fn ), "%s%s", GetInternalBugReporterDLL(), DLL_EXT_STRING );

	bool bCanUseInternal = g_pFileSystem->FileExists( fn, "EXECUTABLE_PATH" );

	switch ( type )
	{
	case IEngineBugReporter::BR_PUBLIC:
		// Nothing
		break;
	default:
	case IEngineBugReporter::BR_AUTOSELECT:
		{
			// check
			bIsPublic = phonehome->IsExternalBuild() ? true : false;
			if ( bCanUseInternal )
			{
				// if command line param specifies internal, use that
				if ( CommandLine()->FindParm( "-internalbuild" ) )
				{
					bIsPublic = false;
				}
#if !defined( _X360 )
				// otherwise, if Steam is running and connected to beta, autoselect the internal bug db
				else if ( k_EUniverseBeta == GetSteamUniverse() )
				{
					bIsPublic = false;
				}
#endif
			}
		}
		break;

	case IEngineBugReporter::BR_INTERNAL:
		{
			if ( bCanUseInternal )
			{
				bIsPublic = false;
			}
		}
		break;
	}

	g_pBugUI = new CBugUIPanel( bIsPublic, parent );
	Assert( g_pBugUI );

	m_ParentPanel = parent;
}

void CEngineBugReporter::Restart( IEngineBugReporter::BR_TYPE type )
{
	Shutdown();
	Msg( "Changing to bugreporter(%s)\n", 
		( type == IEngineBugReporter::BR_AUTOSELECT ) ? 
			( "autoselect" ) : 
			( ( type == IEngineBugReporter::BR_PUBLIC ) ? 
				"public" : 
				"valve" ) );
	InstallBugReportingUI( m_ParentPanel, type );
}

bool CEngineBugReporter::ShouldPause() const
{
	return g_pBugUI && ( g_pBugUI->IsVisible() || g_pBugUI->IsTakingSnapshot() ) && (cl.m_nMaxClients == 1);
}


bool CEngineBugReporter::IsVisible() const
{
	return g_pBugUI && g_pBugUI->IsVisible();
}

CON_COMMAND_F( bug, "Show/hide the bug reporting UI.", FCVAR_DONTRECORD )
{
	if ( !g_pBugUI )
		return;

	bool bWasVisible = g_pBugUI->IsVisible();

	if ( bWasVisible )
	{
		// hide
		g_pBugUI->Close();		
	}
			
	// make sure the gameUI is open so the bugreporter is visible
	EngineVGui()->ActivateGameUI();

	g_pBugUI->Activate();


	g_pBugUI->ParseDefaultParams();
	if (args.ArgC() > 1 )
	{
		g_pBugUI->ParseCommands( args );
	}
}


int CBugUIPanel::GetArea()
{
	char mapname[256] = "";
	int iNewTitleLength = 80;

	if ( host_state.worldmodel )
	{		
		CL_SetupMapName( modelloader->GetName( host_state.worldmodel ), mapname, sizeof( mapname ) );
		iNewTitleLength = (80 - (strlen( mapname )+2)); 		
	}
	m_pTitle->SetMaximumCharCount( iNewTitleLength );

	char *gamedir = com_gamedir;
	gamedir = Q_strrchr( gamedir, CORRECT_PATH_SEPARATOR ) + 1;

	for ( int i = 0; i < m_pBugReporter->GetAreaMapCount(); i++ )
	{
		char szAreaMap[MAX_PATH];
		V_strcpy_safe( szAreaMap, m_pBugReporter->GetAreaMap( i ) );
		char *pszAreaDir = Q_strrchr( szAreaMap, '@' );
		char *pszAreaPrefix = Q_strrchr( szAreaMap, '%' );
		int iDirLength = 0;
		if ( pszAreaDir && pszAreaPrefix )
		{
			iDirLength = pszAreaPrefix - pszAreaDir - 1;
			pszAreaDir++;
			pszAreaPrefix++;
		}
		else if ( pszAreaDir && !pszAreaPrefix )
		{
			pszAreaDir++;
			iDirLength = Q_strlen( szAreaMap ) - (pszAreaDir - szAreaMap);			
		}
		else 
		{
			return 0;
		}

		char szDirectory[MAX_PATH];
		Q_memmove( szDirectory, pszAreaDir, iDirLength );
		szDirectory[iDirLength] = 0;

		if ( pszAreaDir && pszAreaPrefix )
		{
			if ( !Q_strcmp( szDirectory, gamedir) 
				&& Q_strstr( mapname, pszAreaPrefix ) )
			{
				return i+1;
			}
		}
		else if ( pszAreaDir && !pszAreaPrefix )
		{
			if ( !Q_strcmp( szDirectory, gamedir ) )
			{
				return i+1;
			}
		}		
	}
	return 0;
}
