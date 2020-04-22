//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The application object.
//
//===========================================================================//

#include "stdafx.h"
#include <io.h>
#include <stdlib.h>
#include <direct.h>
#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)
#include "BuildNum.h"
#include "EditGameConfigs.h"
#include "Splash.h"
#include "Options.h"
#include "custommessages.h"
#include "MainFrm.h"
#include "MessageWnd.h"
#include "ChildFrm.h"
#include "MapDoc.h"
#include "Manifest.h"
#include "MapView3D.h"
#include "MapView2D.h"
#include "PakDoc.h"
#include "PakViewDirec.h"
#include "PakFrame.h"
#include "Prefabs.h"
#include "GlobalFunctions.h"
#include "Shell.h"
#include "ShellMessageWnd.h"
#include "Options.h"
#include "TextureSystem.h"
#include "ToolManager.h"
#include "Hammer.h"
#include "StudioModel.h"
#include "ibsplighting.h"
#include "statusbarids.h"
#include "tier0/icommandline.h"
#include "soundsystem.h"
#include "IHammer.h"
#include "op_entity.h"
#include "tier0/dbg.h"
#include "tier0/minidump.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "istudiorender.h"
#include "filesystem.h"
#include "engine_launcher_api.h"
#include "filesystem_init.h"
#include "utlmap.h"
#include "progdlg.h"
#include "MapWorld.h"
#include "HammerVGui.h"
#include "vgui_controls/Controls.h"
#include "lpreview_thread.h"
#include "inputsystem/iinputsystem.h"
#include "datacache/idatacache.h"
#include "steam/steam_api.h"
#include "p4lib/ip4.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//
//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//


// dvs: hack
extern LPCTSTR GetErrorString(void);
extern void MakePrefabLibrary(LPCTSTR pszName);
void EditorUtil_ConvertPath(CString &str, bool bSave);	

static bool bMakeLib = false;

static float fSequenceVersion = 0.2f;
static char *pszSequenceHdr = "Worldcraft Command Sequences\r\n\x1a";


CHammer theApp;
COptions Options;

CShell g_Shell;
CShellMessageWnd g_ShellMessageWnd;
CMessageWnd *g_pwndMessage = NULL;

// IPC structures for lighting preview thread
CMessageQueue<MessageToLPreview> g_HammerToLPreviewMsgQueue;
CMessageQueue<MessageFromLPreview> g_LPreviewToHammerMsgQueue;
ThreadHandle_t g_LPreviewThread;
CSteamAPIContext g_SteamAPIContext;
CSteamAPIContext *steamapicontext = &g_SteamAPIContext;


bool	CHammer::m_bIsNewDocumentVisible = true;


//-----------------------------------------------------------------------------
// Expose singleton
//-----------------------------------------------------------------------------
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CHammer, IHammer, INTERFACEVERSION_HAMMER, theApp);


//-----------------------------------------------------------------------------
// global interfaces
//-----------------------------------------------------------------------------
IBaseFileSystem *g_pFileSystem;
IEngineAPI *g_pEngineAPI;
CreateInterfaceFn g_Factory;

bool g_bHDR = true;

bool IsRunningInEngine()
{
	return g_pEngineAPI != NULL;
}

struct MinidumpWrapperHelper_t
{
	int (*m_pfn)(void *pParam);
	void *m_pParam;
	int m_iRetVal;
};

static void MinidumpWrapperHelper( void *arg )
{
	MinidumpWrapperHelper_t *info = (MinidumpWrapperHelper_t *)arg;
	info->m_iRetVal = info->m_pfn( info->m_pParam );
}

static int WrapFunctionWithMinidumpHandler( int (*pfn)(void *pParam), void *pParam )
{
	int nRetVal;

	if ( !Plat_IsInDebugSession() && !CommandLine()->FindParm( "-nominidumps") )
	{
		MinidumpWrapperHelper_t info;
		info.m_pfn = pfn;
		info.m_pParam = pParam;
		info.m_iRetVal = 0;
		CatchAndWriteMiniDumpForVoidPtrFn( MinidumpWrapperHelper, &info, true );
		nRetVal = info.m_iRetVal;
	}
	else
	{
		nRetVal = pfn( pParam );
	}

	return nRetVal;
}


//-----------------------------------------------------------------------------
// Purpose: Outputs a formatted debug string.
// Input  : fmt - format specifier.
//			... - arguments to format.
//-----------------------------------------------------------------------------
void DBG(const char *fmt, ...)
{
    char ach[128];
    va_list va;

    va_start(va, fmt);
    vsprintf(ach, fmt, va);
    va_end(va);
    OutputDebugString(ach);
}


void Msg(int type, const char *fmt, ...)
{
	if ( !g_pwndMessage )
		return;

	va_list vl;
	char szBuf[512];

 	va_start(vl, fmt);
	int len = _vsnprintf(szBuf, 512, fmt, vl);
	va_end(vl);

	if ((type == mwError) || (type == mwWarning))
	{
		g_pwndMessage->ShowMessageWindow();
	}

	char temp = 0;
	char *pMsg = szBuf;
	do
	{
		if (len >= MESSAGE_WND_MESSAGE_LENGTH)
		{
			temp = pMsg[MESSAGE_WND_MESSAGE_LENGTH-1];
			pMsg[MESSAGE_WND_MESSAGE_LENGTH-1] = '\0';
		}

		g_pwndMessage->AddMsg((MWMSGTYPE)type, pMsg);

		if (len >= MESSAGE_WND_MESSAGE_LENGTH)
		{
			pMsg[MESSAGE_WND_MESSAGE_LENGTH-1] = temp;
			pMsg += MESSAGE_WND_MESSAGE_LENGTH-1;
		}

		len -= MESSAGE_WND_MESSAGE_LENGTH-1;

	} while (len > 0);
}


//-----------------------------------------------------------------------------
// Purpose: this routine calls the default doc template's OpenDocumentFile() but
//			with the ability to override the visible flag
// Input  : lpszPathName - the document to open
//			bMakeVisible - ignored
// Output : returns the opened document if successful
//-----------------------------------------------------------------------------
CDocument *CHammerDocTemplate::OpenDocumentFile( LPCTSTR lpszPathName, BOOL bMakeVisible )
{
	CDocument *pDoc = __super::OpenDocumentFile( lpszPathName, CHammer::IsNewDocumentVisible() );

	return pDoc;
}


//-----------------------------------------------------------------------------
// Purpose: this function will attempt an orderly shutdown of all maps.  It will attempt to
//			close only documents that have no references, hopefully freeing up additional documents
// Input  : bEndSession - ignored
//-----------------------------------------------------------------------------
void CHammerDocTemplate::CloseAllDocuments( BOOL bEndSession )
{
	bool	bFound = true;

	// rough loop to always remove the first map doc that has no references, then start over, try again.
	// if we still have maps with references ( that's bad ), we'll exit out of this loop and just do
	// the default shutdown to force them all to close.
	while( bFound )
	{
		bFound = false;

		POSITION pos = GetFirstDocPosition();
		while( pos != NULL )
		{
			CDocument *pDoc = GetNextDoc( pos );
			CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

			if ( pMapDoc && pMapDoc->GetReferenceCount() == 0 )
			{
				pDoc->OnCloseDocument();
				bFound = true;
				break;
			}
		}
	}

#if 0
	POSITION pos = GetFirstDocPosition();
	while( pos != NULL )
	{
		CDocument *pDoc = GetNextDoc( pos );
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		if ( pMapDoc )
		{
			pMapDoc->ForceNoReference();
		}
	}

	__super::CloseAllDocuments( bEndSession );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: This function will allow hammer to control the initial visibility of an opening document
// Input  : pFrame - the new document's frame
//			pDoc - the new document
//			bMakeVisible - ignored as a parameter
//-----------------------------------------------------------------------------
void CHammerDocTemplate::InitialUpdateFrame( CFrameWnd* pFrame, CDocument* pDoc, BOOL bMakeVisible )
{
	bMakeVisible = CHammer::IsNewDocumentVisible();

	__super::InitialUpdateFrame( pFrame, pDoc, bMakeVisible );

	if ( bMakeVisible )
	{
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		if ( pMapDoc )
		{
			pMapDoc->SetInitialUpdate();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: this function will let all other maps know that an instanced map has been updated ( usually for volume size )
// Input  : pInstanceMapDoc - the document that has been updated
//-----------------------------------------------------------------------------
void CHammerDocTemplate::UpdateInstanceMap( CMapDoc *pInstanceMapDoc )
{
	POSITION pos = GetFirstDocPosition();
	while( pos != NULL )
	{
		CDocument *pDoc = GetNextDoc( pos );
		CMapDoc *pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

		if ( pMapDoc && pMapDoc != pInstanceMapDoc )
		{
			pMapDoc->UpdateInstanceMap( pInstanceMapDoc );
		}
	}
}


class CHammerCmdLine : public CCommandLineInfo
{
	public:

		CHammerCmdLine(void)
		{
			m_bShowLogo = true;
			m_bGame = false;
			m_bConfigDir = false;
		}

		void ParseParam(LPCTSTR lpszParam, BOOL bFlag, BOOL bLast)
		{
			if ((!m_bGame) && (bFlag && !stricmp(lpszParam, "game")))
			{
				m_bGame = true;	
			}
			else if (m_bGame)
			{
				if (!bFlag)
				{
					m_strGame = lpszParam;
				}

				m_bGame = false;
			}
			else if (bFlag && !strcmpi(lpszParam, "nologo"))
			{
				m_bShowLogo = false;
			}
			else if (bFlag && !strcmpi(lpszParam, "makelib"))
			{
				bMakeLib = TRUE;
			}
			else if (!bFlag && bMakeLib)
			{
				MakePrefabLibrary(lpszParam);
			}
			else if ((!m_bConfigDir) && (bFlag && !stricmp(lpszParam, "configdir")))
			{
				m_bConfigDir = true;	
			}
			else if (m_bConfigDir)
			{
				if ( !bFlag )
				{
					Options.configs.m_strConfigDir = lpszParam;
				}
				m_bConfigDir = false;
			}
			else
			{
				CCommandLineInfo::ParseParam(lpszParam, bFlag, bLast);
			}
		}

	
	bool m_bShowLogo;
	bool m_bGame;			// Used to find and parse the "-game blah" parameter pair.
	bool m_bConfigDir;		// Used to find and parse the "-configdir blah" parameter pair.
	CString m_strGame;		// The name of the game to use for this session, ie "hl2" or "cstrike". This should match the mod dir, not the config name.
};


BEGIN_MESSAGE_MAP(CHammer, CWinApp)
	//{{AFX_MSG_MAP(CHammer)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes member variables and creates a scratch
//			buffer for use when loading WAD files.
//-----------------------------------------------------------------------------
CHammer::CHammer(void)
{
	m_bActiveApp = true;
	m_SuppressVideoAllocation = false;
	m_bForceRenderNextFrame = false;
	m_bClosing = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor. Frees scratch buffer used when loading WAD files.
//			Deletes all command sequences used when compiling maps.
//-----------------------------------------------------------------------------
CHammer::~CHammer(void)
{
}


//-----------------------------------------------------------------------------
// Inherited from IAppSystem
//-----------------------------------------------------------------------------
bool CHammer::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

//	bool bCVarOk = ConnectStudioRenderCVars( factory );
	g_pFileSystem = ( IBaseFileSystem * )factory( BASEFILESYSTEM_INTERFACE_VERSION, NULL );
	g_pStudioRender = ( IStudioRender * )factory( STUDIO_RENDER_INTERFACE_VERSION, NULL );
	g_pEngineAPI = ( IEngineAPI * )factory( VENGINE_LAUNCHER_API_VERSION, NULL );
	g_pMDLCache = (IMDLCache*)factory( MDLCACHE_INTERFACE_VERSION, NULL );
	p4 = ( IP4 * )factory( P4_INTERFACE_VERSION, NULL );
    g_Factory = factory;

	if ( !g_pMDLCache || !g_pFileSystem || !g_pFullFileSystem || !materials || !g_pMaterialSystemHardwareConfig || !g_pStudioRender )
		return false;

	// ensure we're in the same directory as the .EXE
	char *p;
	GetModuleFileName(NULL, m_szAppDir, MAX_PATH);
	p = strrchr(m_szAppDir, '\\');
	if(p)
	{
		// chop off \wc.exe
		p[0] = 0;
	}

	if ( IsRunningInEngine() )
	{
		strcat( m_szAppDir, "\\bin" );
	}
	
	// Create the message window object for capturing errors and warnings.
	// This does NOT create the window itself. That happens later in CMainFrame::Create.
	g_pwndMessage = CMessageWnd::CreateMessageWndObject();

	// Default location for GameConfig.txt is the same directory as Hammer.exe but this may be overridden on the command line
	char szGameConfigDir[MAX_PATH];
	APP()->GetDirectory( DIR_PROGRAM, szGameConfigDir );
	Options.configs.m_strConfigDir = szGameConfigDir;
	CHammerCmdLine cmdInfo;
	ParseCommandLine(cmdInfo);
	
	// Set up SteamApp() interface (for checking app ownership)
	SteamAPI_InitSafe();
	SteamAPI_SetTryCatchCallbacks( false ); // We don't use exceptions, so tell steam not to use try/catch in callback handlers
	g_SteamAPIContext.Init();

	// Load the options
	// NOTE: Have to do this now, because we need it before Inits() are called 
	// NOTE: SetRegistryKey will cause hammer to look into the registry for its values
	SetRegistryKey("Valve");
	Options.Init();
	return true;
}


void CHammer::Disconnect()
{
	g_pStudioRender = NULL;
	g_pFileSystem = NULL;
	g_pEngineAPI = NULL;
	g_pMDLCache = NULL;
	BaseClass::Disconnect();
}

void *CHammer::QueryInterface( const char *pInterfaceName )
{
	// We also implement the IMatSystemSurface interface
	if (!Q_strncmp(	pInterfaceName, INTERFACEVERSION_HAMMER, Q_strlen(INTERFACEVERSION_HAMMER) + 1))
		return (IHammer*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Methods related to message pumping
//-----------------------------------------------------------------------------
bool CHammer::HammerPreTranslateMessage(MSG * pMsg)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// Copy this into the current message, needed for MFC
#if _MSC_VER >= 1300
	_AFX_THREAD_STATE* pState = AfxGetThreadState();
	pState->m_msgCur = *pMsg;
#else
	m_msgCur = *pMsg;
#endif

	return (/*pMsg->message == WM_KICKIDLE ||*/ PreTranslateMessage(pMsg) != FALSE);
}

bool CHammer::HammerIsIdleMessage(MSG * pMsg)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return IsIdleMessage(pMsg) != FALSE;
}

// return TRUE if more idle processing
bool CHammer::HammerOnIdle(long count)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return OnIdle(count) != FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a backslash to the end of a string if there isn't one already.
// Input  : psz - String to add the backslash to.
//-----------------------------------------------------------------------------
static void EnsureTrailingBackslash(char *psz)
{
	if ((psz[0] != '\0') && (psz[strlen(psz) - 1] != '\\'))
	{
		strcat(psz, "\\");
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tweaks our data members to enable us to import old Hammer settings
//			from the registry.
//-----------------------------------------------------------------------------
static const char *s_pszOldAppName = NULL;
void CHammer::BeginImportWCSettings(void)
{
	s_pszOldAppName = m_pszAppName;
	m_pszAppName = "Worldcraft";
	SetRegistryKey("Valve");
}


//-----------------------------------------------------------------------------
// Purpose: Tweaks our data members to enable us to import old Valve Hammer Editor
//			settings from the registry.
//-----------------------------------------------------------------------------
void CHammer::BeginImportVHESettings(void)
{
	s_pszOldAppName = m_pszAppName;
	m_pszAppName = "Valve Hammer Editor";
	SetRegistryKey("Valve");
}


//-----------------------------------------------------------------------------
// Purpose: Restores our tweaked data members to their original state.
//-----------------------------------------------------------------------------
void CHammer::EndImportSettings(void)
{
	m_pszAppName = s_pszOldAppName;
	SetRegistryKey("Valve");
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves various important directories.
// Input  : dir - Enumerated directory to retrieve.
//			p - Pointer to buffer that receives the full path to the directory.
//-----------------------------------------------------------------------------
void CHammer::GetDirectory(DirIndex_t dir, char *p)
{
	switch (dir)
	{
		case DIR_PROGRAM:
		{
			strcpy(p, m_szAppDir);
			EnsureTrailingBackslash(p);
			break;
		}

		case DIR_PREFABS:
		{
			strcpy(p, m_szAppDir);
			EnsureTrailingBackslash(p);
			strcat(p, "Prefabs");

			//
			// Make sure the prefabs directory exists.
			//
			if ((_access( p, 0 )) == -1)
			{
				CreateDirectory(p, NULL);
			}
			break;
		}

		//
		// Get the game directory with a trailing backslash. This is
		// where the base game's resources are, such as "C:\Half-Life\valve\".
		//
		case DIR_GAME_EXE:
		{
			strcpy(p, g_pGameConfig->m_szGameExeDir);
			EnsureTrailingBackslash(p);
			break;
		}

		//
		// Get the mod directory with a trailing backslash. This is where
		// the mod's resources are, such as "C:\Half-Life\tfc\".
		//
		case DIR_MOD:
		{
			strcpy(p, g_pGameConfig->m_szModDir);
			EnsureTrailingBackslash(p);
			break;
		}

		//
		// Get the materials directory with a trailing backslash. This is where
		// the mod's materials are, such as "C:\Half-Life\tfc\materials".
		//
		case DIR_MATERIALS:
		{
			strcpy(p, g_pGameConfig->m_szModDir);
			EnsureTrailingBackslash(p);
			Q_strcat(p, "materials\\", MAX_PATH);
			break;
		}

		case DIR_AUTOSAVE:
		{			
            strcpy( p, m_szAutosaveDir );
			EnsureTrailingBackslash(p);			
			break;
		}
	}
}

void CHammer::SetDirectory(DirIndex_t dir, const char *p)
{
	switch(dir)
	{
		case DIR_AUTOSAVE:
		{
			strcpy( m_szAutosaveDir, p );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns a color from the application configuration storage.
//-----------------------------------------------------------------------------
COLORREF CHammer::GetProfileColor(const char *pszSection, const char *pszKey, int r, int g, int b)
{
	int newR, newG, newB;
	
	CString strDefault;
	CString strReturn;
	char szBuff[128];
	sprintf(szBuff, "%i %i %i", r, g, b);

	strDefault = szBuff;

	strReturn = GetProfileString(pszSection, pszKey, strDefault);

	if (strReturn.IsEmpty())
		return 0;

	// Parse out colors.
	char *pStart;
	char *pCurrent;
	pStart = szBuff;
	pCurrent = pStart;
	
	strcpy( szBuff, (char *)(LPCSTR)strReturn );

	while (*pCurrent && *pCurrent != ' ')
		pCurrent++;

	*pCurrent++ = 0;
	newR = atoi(pStart);

	pStart = pCurrent;
	while (*pCurrent && *pCurrent != ' ')
		pCurrent++;

	*pCurrent++ = 0;
	newG = atoi(pStart);

	pStart = pCurrent;
	while (*pCurrent)
		pCurrent++;

	*pCurrent++ = 0;
	newB = atoi(pStart);

	return COLORREF(RGB(newR, newG, newB));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszURL - 
//-----------------------------------------------------------------------------
void CHammer::OpenURL(const char *pszURL, HWND hwnd)
{
	if (HINSTANCE(32) > ::ShellExecute(hwnd, "open", pszURL, NULL, NULL, 0))
	{
		AfxMessageBox("The website couldn't be opened.");
	}
}


//-----------------------------------------------------------------------------
// Purpose: Opens a URL in the default web browser by string ID.
//-----------------------------------------------------------------------------
void CHammer::OpenURL(UINT nID, HWND hwnd)
{
	CString str;
	str.LoadString(nID);
	OpenURL(str, hwnd);
}


//-----------------------------------------------------------------------------
// Purpose: Launches the help system for the specified help topic.
// Input  : pszTopic - Topic to open.
//-----------------------------------------------------------------------------
void CHammer::Help(const char *pszTopic)
{
	//
	// Get the directory that the help file should be in (our program directory).
	//
	/*char szHelpDir[MAX_PATH];
	GetDirectory(DIR_PROGRAM, szHelpDir);

	//
	// Find the application that is associated with compiled HTML files.
	//
	char szHelpExe[MAX_PATH];
	HINSTANCE hResult = FindExecutable("wc.chm", szHelpDir, szHelpExe);
	if (hResult > (HINSTANCE)32)
	{
		//
		// Build the full topic with which to launch the help application.
		//
		char szParam[2 * MAX_PATH];
		strcpy(szParam, szHelpDir);
		strcat(szParam, "wc.chm");
		if (pszTopic != NULL)
		{
			strcat(szParam, "::/");
			strcat(szParam, pszTopic);
		}

		//
		// Launch the help application for the given topic.
		//
		hResult = ShellExecute(NULL, "open", szHelpExe, szParam, szHelpDir, SW_SHOW);
	}

	if (hResult <= (HINSTANCE)32)
	{
		char szError[MAX_PATH];
		sprintf(szError, "The help system could not be launched. The the following error was returned:\n%s (0x%X)", GetErrorString(), hResult);
		AfxMessageBox(szError);
	}
	*/
}


static SpewRetval_t HammerDbgOutput( SpewType_t spewType, char const *pMsg )
{
	// FIXME: The messages we're getting from the material system
	// are ones that we really don't care much about.
	// I'm disabling this for now, we need to decide about what to do with this

	switch( spewType )
	{
	case SPEW_ERROR:
		MessageBox( NULL, (LPCTSTR)pMsg, "Fatal Error", MB_OK | MB_ICONINFORMATION );
#ifdef _DEBUG
		return SPEW_DEBUGGER;
#else
		TerminateProcess( GetCurrentProcess(), 1 );
		return SPEW_ABORT;
#endif

	default:
		OutputDebugString( pMsg );
		return (spewType == SPEW_ASSERT) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static HANDLE dwChangeHandle = NULL;
void UpdatePrefabs_Init()
{
 
	// Watch the prefabs tree for file or directory creation
	// and deletion. 
	if (dwChangeHandle == NULL)
	{
		char szPrefabDir[MAX_PATH];
		APP()->GetDirectory(DIR_PREFABS, szPrefabDir);

		dwChangeHandle = FindFirstChangeNotification( 
			szPrefabDir,													// directory to watch 
			TRUE,															// watch the subtree 
			FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME);	// watch file and dir name changes 
 
		if (dwChangeHandle == INVALID_HANDLE_VALUE) 
		{
			ExitProcess(GetLastError()); 
		}
	}
}	 


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UpdatePrefabs()
{
 	// Wait for notification.
 	DWORD dwWaitStatus = WaitForSingleObject(dwChangeHandle, 0);

	if (dwWaitStatus == WAIT_OBJECT_0)
	{
		// A file was created or deleted in the prefabs tree. 
		// Refresh the prefabs and restart the change notification.
		CPrefabLibrary::FreeAllLibraries();
		CPrefabLibrary::LoadAllLibraries();
		GetMainWnd()->m_ObjectBar.UpdateListForTool(ToolManager()->GetActiveToolID());

		if (FindNextChangeNotification(dwChangeHandle) == FALSE)
		{
			ExitProcess(GetLastError()); 
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UpdatePrefabs_Shutdown()
{
	FindCloseChangeNotification(dwChangeHandle);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
BOOL CHammer::InitInstance()
{
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Prompt the user to select a game configuration.
//-----------------------------------------------------------------------------
CGameConfig *CHammer::PromptForGameConfig()
{
	CEditGameConfigs dlg(TRUE, GetMainWnd());
	if (dlg.DoModal() != IDOK)
	{
		return NULL;
	}

	return dlg.GetSelectedGame();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHammer::InitSessionGameConfig(const char *szGame)
{
	CGameConfig *pConfig = NULL;
	bool bManualChoice = false;

	if ( CommandLine()->FindParm( "-chooseconfig" ) )
	{
		pConfig = PromptForGameConfig();
		bManualChoice = true;
	}

	if (!bManualChoice)
	{
		if (szGame && szGame[0] != '\0')
		{
			// They passed in -game on the command line, use that.
			pConfig = Options.configs.FindConfigForGame(szGame);
			if (!pConfig)
			{
				Msg(mwError, "Invalid game \"%s\" specified on the command-line, ignoring.", szGame);
			}
		}
		else
		{
			// No -game on the command line, try using VPROJECT.
			const char *pszGameDir = getenv("vproject");
			if ( pszGameDir )
			{
				pConfig = Options.configs.FindConfigForGame(pszGameDir);
				if (!pConfig)
				{
					Msg(mwError, "Invalid game \"%s\" found in VPROJECT environment variable, ignoring.", pszGameDir);
				}
			}
		}
	}

	if (pConfig == NULL)
	{
		// Nothing useful was passed in or found in VPROJECT.

		// If there's only one config, use that.
		if (Options.configs.GetGameConfigCount() == 1)
		{
			pConfig = Options.configs.GetGameConfig(0);
		}
		else
		{
			// Otherwise, prompt for a config to use.
			pConfig = PromptForGameConfig();
		}
	}

	if (pConfig)
	{
		CGameConfig::SetActiveGame(pConfig);
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Check for 16-bit color or higher.
//-----------------------------------------------------------------------------
bool CHammer::Check16BitColor()
{
	// Check for 15-bit color or higher.
	HDC hDC = ::CreateCompatibleDC(NULL);
	if (hDC)
	{
		int bpp = GetDeviceCaps(hDC, BITSPIXEL);
		if (bpp < 15)
		{
			AfxMessageBox("Your screen must be in 16-bit color or higher to run Hammer.");
			return false;
		}
		::DeleteDC(hDC);
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if Hammer is in the process of shutting down.
//-----------------------------------------------------------------------------
InitReturnVal_t CHammer::Init()
{
	return (InitReturnVal_t)WrapFunctionWithMinidumpHandler( &CHammer::StaticHammerInternalInit, this );
}


int CHammer::StaticHammerInternalInit( void *pParam )
{
	return (int)((CHammer*)pParam)->HammerInternalInit();
}


InitReturnVal_t CHammer::HammerInternalInit()
{
	SpewOutputFunc( HammerDbgOutput );
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	if ( !Check16BitColor() )
		return INIT_FAILED;


	//
	// Create a custom window class for this application so that engine's
	// FindWindow will find us.
	//
	WNDCLASS wndcls;
	memset(&wndcls, 0, sizeof(WNDCLASS));
    wndcls.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wndcls.lpfnWndProc   = AfxWndProc;
    wndcls.hInstance     = AfxGetInstanceHandle();
    wndcls.hIcon         = LoadIcon(IDR_MAINFRAME);
    wndcls.hCursor       = LoadCursor( IDC_ARROW );
    wndcls.hbrBackground = (HBRUSH)0; //  (COLOR_WINDOW + 1);
    wndcls.lpszMenuName  = "IDR_MAINFRAME";
	wndcls.cbWndExtra    = 0;
	 
	// HL Shell class name
    wndcls.lpszClassName = "VALVEWORLDCRAFT";

    // Register it, exit if it fails    
	if(!AfxRegisterClass(&wndcls))
	{
		AfxMessageBox("Could not register Hammer's main window class");
		return INIT_FAILED;
	}

	srand(time(NULL));

	WriteProfileString("General", "Directory", m_szAppDir);

	//
	// Create a window to receive shell commands from the engine, and attach it
	// to our shell processor.
	//
	g_ShellMessageWnd.Create();
	g_ShellMessageWnd.SetShell(&g_Shell);

	if (bMakeLib)
		return INIT_FAILED;	// made library .. don't want to enter program

	CHammerCmdLine cmdInfo;
	ParseCommandLine(cmdInfo);

	//
	// Create and optionally display the splash screen.
	//
	CSplashWnd::EnableSplashScreen(cmdInfo.m_bShowLogo);

	LoadSequences();	// load cmd sequences - different from options because
						//  users might want to share (darn registry)

	// other init:
	randomize();

	/*
#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
	*/

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.
	pMapDocTemplate = new CHammerDocTemplate(
		IDR_MAPDOC,
		RUNTIME_CLASS(CMapDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CMapView2D));
	AddDocTemplate(pMapDocTemplate);

	pManifestDocTemplate = new CHammerDocTemplate(
		IDR_MANIFESTDOC,
		RUNTIME_CLASS(CManifest),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CMapView2D));
	HINSTANCE hInst = AfxFindResourceHandle( MAKEINTRESOURCE( IDR_MAPDOC ), RT_MENU );
	pManifestDocTemplate->m_hMenuShared = ::LoadMenu( hInst, MAKEINTRESOURCE( IDR_MAPDOC ) );
	hInst = AfxFindResourceHandle( MAKEINTRESOURCE( IDR_MAPDOC ), RT_ACCELERATOR );
	pManifestDocTemplate->m_hAccelTable = ::LoadAccelerators( hInst, MAKEINTRESOURCE( IDR_MAPDOC ) );
	AddDocTemplate(pManifestDocTemplate);


	// register shell file types
	RegisterShellFileTypes();

	//
	// Initialize the rich edit control so we can use it in the entity help dialog.
	//
	AfxInitRichEdit();

	//
	// Create main MDI Frame window. Must be done AFTER registering the multidoc template!
	//
	CMainFrame *pMainFrame = new CMainFrame;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return INIT_FAILED;

	m_pMainWnd = pMainFrame;

	CSplashWnd::ShowSplashScreen(pMainFrame);


	// try to init VGUI
	HammerVGui()->Init( m_pMainWnd->GetSafeHwnd() );

	// The main window has been initialized, so show and update it.
	//
	m_nCmdShow = SW_SHOWMAXIMIZED;
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

	// Now that we've initialized the file system, we can parse this config's gameinfo.txt for the additional settings there.
	g_pGameConfig->ParseGameInfo();

	materials->ModInit();

	//
	// Initialize the texture manager and load all textures.
	//
	if (!g_Textures.Initialize(m_pMainWnd->m_hWnd))
	{
		Msg(mwError, "Failed to initialize texture system.");
	}
	else
	{
		//
		// Initialize studio model rendering (must happen after g_Textures.Initialize since
		// g_Textures.Initialize kickstarts the material system and sets up g_MaterialSystemClientFactory)
		//
		StudioModel::Initialize();
		g_Textures.LoadAllGraphicsFiles();
		g_Textures.SetActiveConfig(g_pGameConfig);
	}
	
	// Watch for changes to models.
	InitStudioFileChangeWatcher();

	LoadFileSystemDialogModule();

	// Load detail object descriptions.
	char	szGameDir[_MAX_PATH];
	APP()->GetDirectory(DIR_MOD, szGameDir);
	DetailObjects::LoadEmitDetailObjectDictionary( szGameDir );
	
	// Initialize the sound system
	g_Sounds.Initialize();

	UpdatePrefabs_Init();

	// Indicate that we are ready to use.
	m_pMainWnd->FlashWindow(TRUE);

	// Parse command line for standard shell commands, DDE, file open
	if ( !IsRunningInEngine() )
	{
		if ( Q_stristr( cmdInfo.m_strFileName, ".vmf" ) )
		{
			// we don't want to make a new file (default behavior if no file
			//  is specified on the commandline.)

			// Dispatch commands specified on the command line
			if (!ProcessShellCommand(cmdInfo))
				return INIT_FAILED;
		}
	}

	if ( Options.general.bClosedCorrectly == FALSE )
	{
		CString strLastGoodSave = APP()->GetProfileString("General", "Last Good Save", "");
        
		if ( strLastGoodSave.GetLength() != 0 )
		{
			char msg[1024];
			V_snprintf( msg, sizeof( msg ), "Hammer did not shut down correctly the last time it was used.\nWould you like to load the last saved file?\n(%s)", (const char*)strLastGoodSave );
			if ( AfxMessageBox( msg, MB_YESNO ) == IDYES )
			{
				LoadLastGoodSave();
			}
		}
	}

#ifdef VPROF_HAMMER
	g_VProfCurrentProfile.Start();
#endif
	
	CSplashWnd::HideSplashScreen();

	// create the lighting preview thread
	g_LPreviewThread = CreateSimpleThread( LightingPreviewThreadFN, 0 );

	return INIT_OK;
}

int CHammer::MainLoop()
{
	return WrapFunctionWithMinidumpHandler( StaticInternalMainLoop, this );
}


int CHammer::StaticInternalMainLoop( void *pParam )
{
	return ((CHammer*)pParam)->InternalMainLoop();
}


int CHammer::InternalMainLoop()
{	
	MSG msg;

	g_pDataCache->SetSize( 128 * 1024 * 1024 );

	// For tracking the idle time state
	bool bIdle = true;
	long lIdleCount = 0;

	// We've got our own message pump here
	g_pInputSystem->EnableMessagePump( false );

	// Acquire and dispatch messages until a WM_QUIT message is received.
	for (;;)
	{
		RunFrame();

		if ( bIdle && !HammerOnIdle(lIdleCount++) )
		{
			bIdle = false;
		}

		//
		// Pump messages until the message queue is empty.
		//
		while (::PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE))
		{
			if ( msg.message == WM_QUIT )
				return 1;

			if ( !HammerPreTranslateMessage(&msg) )
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}

			// Reset idle state after pumping idle message.
			if ( HammerIsIdleMessage(&msg) )
			{
				bIdle = true;
				lIdleCount = 0;
			}
		}
	}

	Assert(0);  // not reachable
}

//-----------------------------------------------------------------------------
// Shuts down hammer
//-----------------------------------------------------------------------------
void CHammer::Shutdown()
{
	if ( g_LPreviewThread )
	{
		MessageToLPreview StopMsg( LPREVIEW_MSG_EXIT );
		g_HammerToLPreviewMsgQueue.QueueMessage( StopMsg );
		ThreadJoin( g_LPreviewThread );
		g_LPreviewThread = 0;
	}

#ifdef VPROF_HAMMER
	g_VProfCurrentProfile.Stop();
#endif

	// PrintBudgetGroupTimes_Recursive( g_VProfCurrentProfile.GetRoot() );

	HammerVGui()->Shutdown();

	UnloadFileSystemDialogModule();

	// Delete the command sequences.
	int nSequenceCount = m_CmdSequences.GetSize();
	for (int i = 0; i < nSequenceCount; i++)
	{
		CCommandSequence *pSeq = m_CmdSequences[i];
		if ( pSeq != NULL )
		{
			delete pSeq;
			m_CmdSequences[i] = NULL;
		}
	}

	g_Textures.ShutDown();

	// Shutdown the sound system
	g_Sounds.ShutDown();

	materials->ModShutdown();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Methods used by the engine
//-----------------------------------------------------------------------------
const char *CHammer::GetDefaultMod()
{
	return g_pGameConfig->GetMod();
}

const char *CHammer::GetDefaultGame()
{
	return g_pGameConfig->GetGame();
}

const char *CHammer::GetDefaultModFullPath()
{
	return g_pGameConfig->m_szModDir;
}

	
//-----------------------------------------------------------------------------
// Pops up the options dialog
//-----------------------------------------------------------------------------
RequestRetval_t CHammer::RequestNewConfig()
{
	if ( !Options.RunConfigurationDialog() )
		return REQUEST_QUIT;

	return REQUEST_OK;
}

	
//-----------------------------------------------------------------------------
// Purpose: Returns true if Hammer is in the process of shutting down.
//-----------------------------------------------------------------------------
bool CHammer::IsClosing()
{
	return m_bClosing;
}


//-----------------------------------------------------------------------------
// Purpose: Signals the beginning of app shutdown. Should be called before
//			rendering views.
//-----------------------------------------------------------------------------
void CHammer::BeginClosing()
{
	m_bClosing = true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHammer::ExitInstance()
{
	g_ShellMessageWnd.DestroyWindow();

	UpdatePrefabs_Shutdown();

	if ( GetSpewOutputFunc() == HammerDbgOutput )
	{
		SpewOutputFunc( NULL );
	}

	SaveStdProfileSettings();

	return CWinApp::ExitInstance();
}


//-----------------------------------------------------------------------------
// Purpose: this function sets the global flag indicating if new documents should
//			be visible.
// Input  : bIsVisible - flag to indicate visibility status.
//-----------------------------------------------------------------------------
void CHammer::SetIsNewDocumentVisible( bool bIsVisible )
{
	CHammer::m_bIsNewDocumentVisible = bIsVisible;
}


//-----------------------------------------------------------------------------
// Purpose: this functionr eturns the global flag indicating if new documents should
//			be visible.
//-----------------------------------------------------------------------------
bool CHammer::IsNewDocumentVisible( void )
{
	return CHammer::m_bIsNewDocumentVisible;
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	CStatic	m_cRedHerring;
	CButton	m_Order;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	afx_msg void OnOrder();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	DDX_Control(pDX, IDC_REDHERRING, m_cRedHerring);
	DDX_Control(pDX, IDC_ORDER, m_Order);
	//}}AFX_DATA_MAP
}

#include <process.h>


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAboutDlg::OnOrder() 
{
	char szBuf[MAX_PATH];
	GetWindowsDirectory(szBuf, MAX_PATH);
	strcat(szBuf, "\\notepad.exe");
	_spawnl(_P_NOWAIT, szBuf, szBuf, "order.txt", NULL);
}


#define DEMO_VERSION	0

// 1, 4, 8, 17, 0, 0 // Encodes "Valve"

#if DEMO_VERSION

char gVersion[] = {                        
#if DEMO_VERSION==1
	7, 38, 68, 32, 4, 77, 12, 1, 0 // Encodes "PC Gamer Demo"
#elif DEMO_VERSION==2
	7, 38, 68, 32, 4, 77, 12, 0, 0 // Encodes "PC Games Demo"
#elif DEMO_VERSION==3
	20, 10, 9, 23, 16, 84, 12, 1, 0, 38, 65, 25, 6, 1, 11, 119, 50, 11, 21, 9, 68, 0 // Encodes "Computer Gaming World Demo"
#elif DEMO_VERSION==4
	25, 0, 28, 19, 72, 103, 12, 29, 69, 19, 65, 0, 6, 0, 2, 0		// Encodes "Next-Generation Demo"
#elif DEMO_VERSION==5
	20, 10, 9, 23, 16, 84, 12, 1, 0, 38, 65, 25, 10, 79, 41, 57, 17, 1, 21, 17, 65, 0, 29, 77, 4, 78, 0, 0 // Encodes "Computer Game Entertainment"
#elif DEMO_VERSION==6
	20, 10, 9, 23, 16, 84, 12, 1, 0, 0, 78, 16, 79, 33, 9, 35, 69, 52, 11, 4, 89, 12, 1, 0 // Encodes "Computer and Net Player"
#elif DEMO_VERSION==7
	50, 72, 52, 43, 36, 121, 0 // Encodes "e-PLAY"
#elif DEMO_VERSION==8
	4, 17, 22, 6, 17, 69, 14, 10, 0, 49, 76, 1, 28, 0 // Encodes "Strategy Plus"
#elif DEMO_VERSION==9
	7, 38, 68, 42, 4, 71, 8, 9, 73, 15, 69, 0 // Encodes "PC Magazine"
#elif DEMO_VERSION==10
	5, 10, 8, 11, 12, 78, 14, 83, 115, 21, 79, 26, 10, 0 // Encodes "Rolling Stone"
#elif DEMO_VERSION==11
	16, 4, 9, 2, 22, 80, 6, 7, 0 // Encodes "Gamespot"
#endif
};

static char gKey[] = "Wedge is a tool";	// Decrypt key

// XOR a string with a key
void Encode( char *pstring, char *pkey, int strLen )
{
	int i, len;
	len = strlen( pkey );
	for ( i = 0; i < strLen; i++ )
		pstring[i] ^= pkey[ i % len ];
}
#endif // DEMO_VERSION


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CAboutDlg::OnInitDialog(void)
{
	CDialog::OnInitDialog();

	m_Order.SetRedraw(FALSE);

#if DEMO_VERSION
	static BOOL bFirst = TRUE;
	if(bFirst)
	{
		Encode(gVersion, gKey, sizeof(gVersion)-1);
		bFirst = FALSE;
	}
	CString str;
	str.Format("%s Demo", gVersion);
	m_cRedHerring.SetWindowText(str);
#endif // DEMO_VERSION

	//
	// Display the build number.
	//
	CWnd *pWnd = GetDlgItem(IDC_BUILD_NUMBER);
	if (pWnd != NULL)
	{
		char szTemp1[MAX_PATH];
		char szTemp2[MAX_PATH];
		int nBuild = build_number();
		pWnd->GetWindowText(szTemp1, sizeof(szTemp1));
		sprintf(szTemp2, szTemp1, nBuild);
		pWnd->SetWindowText(szTemp2);
	}

	//
	// For SDK builds, append "SDK" to the version number.
	//
#ifdef SDK_BUILD
	char szTemp[MAX_PATH];
	GetWindowText(szTemp, sizeof(szTemp));
	strcat(szTemp, " SDK");
	SetWindowText(szTemp);
#endif // SDK_BUILD

	return TRUE;
}


BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
	ON_BN_CLICKED(IDC_ORDER, OnOrder)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHammer::OnAppAbout(void)
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();

#ifdef VPROF_HAMMER
	g_VProfCurrentProfile.OutputReport();
	g_VProfCurrentProfile.Reset();
	g_pMemAlloc->DumpStats();
#endif

}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHammer::OnFileNew(void)
{
	pMapDocTemplate->OpenDocumentFile(NULL);
	if(Options.general.bLoadwinpos && Options.general.bIndependentwin)
	{
		::GetMainWnd()->LoadWindowStates();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHammer::OnFileOpen(void)
{
	static char szInitialDir[MAX_PATH] = "";
	if (szInitialDir[0] == '\0')
	{
		strcpy(szInitialDir, g_pGameConfig->szMapDir);
	}

	// TODO: need to prevent (or handle) opening VMF files when using old map file formats
	CFileDialog dlg(TRUE, NULL, NULL, OFN_LONGNAMES | OFN_HIDEREADONLY | OFN_NOCHANGEDIR, "Valve Map Files (*.vmf;*.vmm)|*.vmf;*.vmm|Valve Map Files Autosave (*.vmf_autosave)|*.vmf_autosave|Worldcraft RMFs (*.rmf)|*.rmf|Worldcraft Maps (*.map)|*.map||");
	dlg.m_ofn.lpstrInitialDir = szInitialDir;
	int iRvl = dlg.DoModal();

	if (iRvl == IDCANCEL)
	{
		return;
	}

	//
	// Get the directory they browsed to for next time.
	//
	CString str = dlg.GetPathName();
	int nSlash = str.ReverseFind('\\');
	if (nSlash != -1)
	{
		strcpy(szInitialDir, str.Left(nSlash));
	}

	if (str.Find('.') == -1)
	{
		switch (dlg.m_ofn.nFilterIndex)
		{
			case 1:
			{
				str += ".vmf";
				break;
			}
			
			case 2:
			{
				str += ".vmf_autosave";
				break;
			}

			case 3:
			{
				str += ".rmf";
				break;
			}

			case 4:
			{
				str += ".map";
				break;
			}
		}
	}

	OpenDocumentFile(str);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lpszFileName - 
// Output : CDocument*
//-----------------------------------------------------------------------------
CDocument* CHammer::OpenDocumentFile(LPCTSTR lpszFileName) 
{
	if(GetFileAttributes(lpszFileName) == 0xFFFFFFFF)
	{
		CString		Message;

		Message = "The file " + CString( lpszFileName ) + " does not exist.";
		AfxMessageBox( Message );

		return NULL;
	}

	CDocument	*pDoc = m_pDocManager->OpenDocumentFile( lpszFileName );
	CMapDoc		*pMapDoc = dynamic_cast< CMapDoc * >( pDoc );

	if ( pMapDoc )
	{
		CMapDoc::SetActiveMapDoc( pMapDoc );

	}
	if( pDoc && Options.general.bLoadwinpos && Options.general.bIndependentwin)
	{
		::GetMainWnd()->LoadWindowStates();
	}

	if ( pMapDoc && !CHammer::IsNewDocumentVisible() )
	{
		pMapDoc->ShowWindow( false );
	}
	else
	{
		pMapDoc->ShowWindow( true );
	}

	if ( pDoc && ((CMapDoc *)pDoc)->IsAutosave() )
	{			
		char szRenameMessage[MAX_PATH+MAX_PATH+256];
		CString newMapPath = *((CMapDoc *)pDoc)->AutosavedFrom();

		sprintf( szRenameMessage, "This map was loaded from an autosave file.\nWould you like to rename it from \"%s\" to \"%s\"?\nNOTE: This will not save the file with the new name; it will only rename it.", lpszFileName, (const char*)newMapPath );

		if ( AfxMessageBox( szRenameMessage, MB_YESNO ) == IDYES )
		{			
			((CMapDoc *)pDoc)->SetPathName( newMapPath );		
		}			
	}

	return pDoc;
}


//-----------------------------------------------------------------------------
// Returns true if this is a key message that is not a special dialog navigation message.
//-----------------------------------------------------------------------------
inline bool IsKeyStrokeMessage( MSG *pMsg )
{
	if ( ( pMsg->message != WM_KEYDOWN ) && ( pMsg->message != WM_CHAR ) )
		return false;

	// Check for special dialog navigation characters -- they don't count
	if ( ( pMsg->wParam == VK_ESCAPE ) || ( pMsg->wParam == VK_RETURN ) || ( pMsg->wParam == VK_TAB ) )
		return false;

	if ( ( pMsg->wParam == VK_UP ) || ( pMsg->wParam == VK_DOWN ) || ( pMsg->wParam == VK_LEFT ) || ( pMsg->wParam == VK_RIGHT ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CHammer::PreTranslateMessage(MSG* pMsg)
{
	// CG: The following lines were added by the Splash Screen component.
	if (CSplashWnd::PreTranslateAppMessage(pMsg))
		return TRUE;

	// This is for raw input, these shouldn't be translated so skip that here.
	if ( pMsg->message == WM_INPUT )
		return TRUE;

	// Suppress the accelerator table for edit controls so that users can type
	// uppercase characters without invoking Hammer tools.	
	if ( IsKeyStrokeMessage( pMsg ) )
	{
		char className[80];
		::GetClassNameA( pMsg->hwnd, className, sizeof( className ) );

		// The classname of dialog window in the VGUI model browser and particle browser is AfxWnd100sd in Debug and AfxWnd100s in Release
		if ( !V_stricmp( className, "edit" ) || V_stristr( className, "AfxWnd" ) )
		{
			// Typing in an edit control. Don't pretranslate, just translate/dispatch.
			return FALSE;
		}
	}

	return CWinApp::PreTranslateMessage(pMsg);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHammer::LoadSequences(void)
{
	char szRootDir[MAX_PATH];
	char szFullPath[MAX_PATH];
	APP()->GetDirectory(DIR_PROGRAM, szRootDir);
	Q_MakeAbsolutePath( szFullPath, MAX_PATH, "CmdSeq.wc", szRootDir ); 
	std::ifstream file(szFullPath, std::ios::in | std::ios::binary);
	
	if(!file.is_open())
		return;	// none to load

	// skip past header & version
	float fThisVersion;

	file.seekg(strlen(pszSequenceHdr));
	file.read((char*)&fThisVersion, sizeof fThisVersion);

	// read number of sequences
	DWORD dwSize;
	int nSeq;

	file.read((char*)&dwSize, sizeof dwSize);
	nSeq = dwSize;

	for(int i = 0; i < nSeq; i++)
	{
		CCommandSequence *pSeq = new CCommandSequence;
		file.read(pSeq->m_szName, 128);

		// read commands in sequence
		file.read((char*)&dwSize, sizeof dwSize);
		int nCmd = dwSize;
		CCOMMAND cmd;
		for(int iCmd = 0; iCmd < nCmd; iCmd++)
		{
			if(fThisVersion < 0.2f)
			{
				file.read((char*)&cmd, sizeof(CCOMMAND)-1);
				cmd.bNoWait = FALSE;
			}
			else
			{
				file.read((char*)&cmd, sizeof(CCOMMAND));
			}
			pSeq->m_Commands.Add(cmd);
		}

		m_CmdSequences.Add(pSeq);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHammer::SaveSequences(void)
{
	char szRootDir[MAX_PATH];
	char szFullPath[MAX_PATH];
	APP()->GetDirectory(DIR_PROGRAM, szRootDir);
	Q_MakeAbsolutePath( szFullPath, MAX_PATH, "CmdSeq.wc", szRootDir ); 
	std::ofstream file( szFullPath, std::ios::out | std::ios::binary );

	// write header
	file.write(pszSequenceHdr, Q_strlen(pszSequenceHdr));
	// write out version
	file.write((char*)&fSequenceVersion, sizeof(float));

	// write out each sequence..
	int i, nSeq = m_CmdSequences.GetSize();
	DWORD dwSize = nSeq;
	file.write((char*)&dwSize, sizeof dwSize);
	for(i = 0; i < nSeq; i++)
	{
		CCommandSequence *pSeq = m_CmdSequences[i];

		// write name of sequence
		file.write(pSeq->m_szName, 128);
		// write number of commands
		int nCmd = pSeq->m_Commands.GetSize();
		dwSize = nCmd;
		file.write((char*)&dwSize, sizeof dwSize);
		// write commands .. 
		for(int iCmd = 0; iCmd < nCmd; iCmd++)
		{
			CCOMMAND &cmd = pSeq->m_Commands[iCmd];
			file.write((char*)&cmd, sizeof cmd);
		}
	}
}


void CHammer::SetForceRenderNextFrame()
{
	m_bForceRenderNextFrame = true;
}


bool CHammer::GetForceRenderNextFrame()
{
	return m_bForceRenderNextFrame;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDoc - 
//-----------------------------------------------------------------------------
void CHammer::UpdateLighting(CMapDoc *pDoc)
{
	static int lastPercent = -20000;
	int curPercent = -10000;
	
	IBSPLighting *pLighting = pDoc->GetBSPLighting();
	if ( pLighting )
	{
		// Update 5x / second.
		static DWORD lastTime = 0;

		DWORD curTime = GetTickCount();
		if ( curTime - lastTime < 200 )
		{
			curPercent = lastPercent; // no change
		}
		else
		{
			curPercent = (int)( pLighting->GetPercentComplete() * 10000.0f );
			lastTime = curTime;
		}

		// Redraw the views when new lightmaps are ready.
		if ( pLighting->CheckForNewLightmaps() )
		{
			SetForceRenderNextFrame();
			pDoc->UpdateAllViews( MAPVIEW_UPDATE_ONLY_3D );
		}
	}

	// Update the status text.
	if ( curPercent == -10000 )
	{
		SetStatusText( SBI_LIGHTPROGRESS, "<->" );
	}
	else if( curPercent != lastPercent )
	{
		char str[256];
		sprintf( str, "%.2f%%", curPercent / 100.0f );
		SetStatusText( SBI_LIGHTPROGRESS, str );
	}

	lastPercent = curPercent;	
}


//-----------------------------------------------------------------------------
// Purpose: Performs idle processing. Runs the frame and does MFC idle processing.
// Input  : lCount - The number of times OnIdle has been called in succession,
//				indicating the relative length of time the app has been idle without
//				user input.
// Output : Returns TRUE if there is more idle processing to do, FALSE if not.
//-----------------------------------------------------------------------------
BOOL CHammer::OnIdle(LONG lCount)
{
	UpdatePrefabs();

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (pDoc)
	{
		UpdateLighting(pDoc);
	}

	g_Textures.UpdateFileChangeWatchers();
	UpdateStudioFileChangeWatcher();
	return(CWinApp::OnIdle(lCount));
}


//-----------------------------------------------------------------------------
// Purpose: Renders the realtime views.
//-----------------------------------------------------------------------------
void CHammer::RunFrame(void)
{

	// Note: since hammer may well not even have a 3D window visible
	// at any given time, we have to call into the material system to
	// make it deal with device lost. Usually this happens during SwapBuffers,
	// but we may well not call SwapBuffers at any given moment.
	materials->HandleDeviceLost();

	if (!IsActiveApp())
	{
		Sleep(50);
	}

#ifdef VPROF_HAMMER
	g_VProfCurrentProfile.MarkFrame();
#endif

	HammerVGui()->Simulate();

	if ( CMapDoc::GetActiveMapDoc() && !IsClosing() || m_bForceRenderNextFrame )
		HandleLightingPreview();

	// never render without document or when closing down
	// usually only render when active, but not compiling a map unless forced
	if ( CMapDoc::GetActiveMapDoc() && !IsClosing() &&
		 ( ( !IsRunningCommands() && IsActiveApp() ) || m_bForceRenderNextFrame ) &&
		 CMapDoc::GetActiveMapDoc()->HasInitialUpdate() )
		 
	{
		// get the time
		CMapDoc::GetActiveMapDoc()->UpdateCurrentTime();

		// run any animation
		CMapDoc::GetActiveMapDoc()->UpdateAnimation();

		// redraw the 3d views
		CMapDoc::GetActiveMapDoc()->RenderAllViews();
	}

	// No matter what, we want to keep caching in materials...
	if ( IsActiveApp() )
	{
		g_Textures.LazyLoadTextures();
	}

	m_bForceRenderNextFrame = false;


}


//-----------------------------------------------------------------------------
// Purpose: Overloaded Run so that we can control the frameratefor realtime
//			rendering in the 3D view.
// Output : As MFC CWinApp::Run.
//-----------------------------------------------------------------------------
int CHammer::Run(void)
{
	Assert(0);
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the editor is the active app, false if not.
//-----------------------------------------------------------------------------
bool CHammer::IsActiveApp(void)
{
	return m_bActiveApp;
}


//-----------------------------------------------------------------------------
// Purpose: Called from CMainFrame::OnSysCommand, this informs the app when it
//			is minimized and unminimized. This allows us to stop rendering the
//			3D view when we are minimized.
// Input  : bMinimized - TRUE when minmized, FALSE otherwise.
//-----------------------------------------------------------------------------
void CHammer::OnActivateApp(bool bActive)
{
//	static int nCount = 0;
//	if (bActive)
//		DBG("ON %d\n", nCount);
//	else
//		DBG("OFF %d\n", nCount);
//	nCount++;
	m_bActiveApp = bActive;


}

//-----------------------------------------------------------------------------
// Purpose: Called from the shell to relinquish our video memory in favor of the
//			engine. This is called by the engine when it starts up.
//-----------------------------------------------------------------------------
void CHammer::ReleaseVideoMemory()
{
   POSITION pos = GetFirstDocTemplatePosition();

   while (pos)
   {
      CDocTemplate* pTemplate = (CDocTemplate*)GetNextDocTemplate(pos);
      POSITION pos2 = pTemplate->GetFirstDocPosition();
      while (pos2)
      {
         CDocument * pDocument;
         if ((pDocument=pTemplate->GetNextDoc(pos2)) != NULL)
		 {
			 static_cast<CMapDoc*>(pDocument)->ReleaseVideoMemory();
		 }
      }
   }
} 

void CHammer::SuppressVideoAllocation( bool bSuppress )
{
	m_SuppressVideoAllocation = bSuppress;
} 

bool CHammer::CanAllocateVideo() const
{
	return !m_SuppressVideoAllocation;
}

//-------------------------------------------------------------------------------
// Purpose: Runs through the autosave directory and fills the autosave map.
//			Also sets the total amount of space used by the directory.
// Input  : pFileMap - CUtlMap that will hold the list of files in the dir
//			pdwTotalDirSize - pointer to the DWORD that will hold directory size
//			pstrMapTitle - the name of the current map to be saved
// Output : returns an int containing the next number to use for the autosave
//-------------------------------------------------------------------------------
int CHammer::GetNextAutosaveNumber( CUtlMap<FILETIME, WIN32_FIND_DATA, int> *pFileMap, DWORD *pdwTotalDirSize, const CString *pstrMapTitle ) const
{
	FILETIME oldestAutosaveTime;
	oldestAutosaveTime.dwHighDateTime = 0;
	oldestAutosaveTime.dwLowDateTime = 0; 

	char szRootDir[MAX_PATH];
	APP()->GetDirectory(DIR_AUTOSAVE, szRootDir);
	CString strAutosaveDirectory( szRootDir );
   
	int nNumberActualAutosaves = 0;
	int nCurrentAutosaveNumber = 1;
	int nOldestAutosaveNumber = 1;
	int nExpectedNextAutosaveNumber = 1;
	int nLastHole = 0;
	int nMaxAutosavesPerMap = Options.general.iMaxAutosavesPerMap; 

	WIN32_FIND_DATA fileData;
	HANDLE hFile;
	DWORD dwTotalAutosaveDirectorySize = 0;
			
	hFile = FindFirstFile( strAutosaveDirectory + "*.vmf_autosave", &fileData );

    if ( hFile != INVALID_HANDLE_VALUE )
	{
		//go through and for each file check to see if it is an autosave for this map; also keep track of total file size
		//for directory.
		while( GetLastError() != ERROR_NO_MORE_FILES && hFile != INVALID_HANDLE_VALUE )
		{				
			(*pFileMap).Insert( fileData.ftLastAccessTime, fileData );
		
			DWORD dwFileSize = fileData.nFileSizeLow;
			dwTotalAutosaveDirectorySize += dwFileSize;
			FILETIME fileAccessTime = fileData.ftLastAccessTime;

			CString currentFilename( fileData.cFileName );

			//every autosave file ends in three digits; this code separates the name from the digits
			CString strMapName = currentFilename.Left( currentFilename.GetLength() - 17 );
			CString strCurrentNumber = currentFilename.Mid( currentFilename.GetLength() - 16, 3 );	
			int nMapNumber = atoi( (char *)strCurrentNumber.GetBuffer() );
			
			if ( strMapName.CompareNoCase( (*pstrMapTitle) ) == 0 )
			{
				//keep track of real number of autosaves with map name; deals with instance where older maps get deleted
				//and create sequence holes in autosave map names.
				nNumberActualAutosaves++; 

				if ( oldestAutosaveTime.dwLowDateTime == 0 )
				{
					//the first file is automatically the oldest
					oldestAutosaveTime = fileAccessTime;
				}			
                
				if ( nMapNumber != nExpectedNextAutosaveNumber )
				{					
					//the current map number is different than what was expected
					//there is a hole in the sequence
					nLastHole = nMapNumber;										
				}

				nExpectedNextAutosaveNumber = nMapNumber + 1;
				if ( nExpectedNextAutosaveNumber > 999 )
				{
					nExpectedNextAutosaveNumber = 1;
				}
				if ( CompareFileTime( &fileAccessTime, &oldestAutosaveTime ) == -1 ) 
				{
					//this file is older than previous oldest file
					oldestAutosaveTime = fileAccessTime;
					nOldestAutosaveNumber = nMapNumber;					
				}
			}	
			FindNextFile(hFile, &fileData);
		}
		FindClose(hFile);
	}		

    if ( nNumberActualAutosaves < nMaxAutosavesPerMap ) 
	{
		//there are less autosaves than wanted for the map; use the larger
		//of the next expected or the last found hole as the number.
		nCurrentAutosaveNumber = max( nExpectedNextAutosaveNumber, nLastHole );        
	}
	else 
	{
		//there are no holes, use the oldest.
		nCurrentAutosaveNumber = nOldestAutosaveNumber;
	}	

	*pdwTotalDirSize = dwTotalAutosaveDirectorySize;

	return nCurrentAutosaveNumber;
}


static bool LessFunc( const FILETIME &lhs, const FILETIME &rhs )
{ 
	return CompareFileTime(&lhs, &rhs) < 0;	
}


//-----------------------------------------------------------------------------
// Purpose: This is called when the autosave timer goes off.  It checks to 
//			make sure the document has changed and handles deletion of old
//			files when the total directory size is too big
//-----------------------------------------------------------------------------

void CHammer::Autosave( void )
{
	if ( !Options.general.bEnableAutosave )
	{
		return;
	}
	
	if ( VerifyAutosaveDirectory() != true )
	{     
		Options.general.bEnableAutosave = false;
		return;		
	}    	

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();

	//value from options is in megs
	DWORD dwMaxAutosaveSpace = Options.general.iMaxAutosaveSpace * 1024 * 1024; 
	
	CUtlMap<FILETIME, WIN32_FIND_DATA, int> autosaveFiles;

	autosaveFiles.SetLessFunc( LessFunc );

	if ( pDoc && pDoc->NeedsAutosave() )
	{
		char szRootDir[MAX_PATH];
		APP()->GetDirectory(DIR_AUTOSAVE, szRootDir);
		CString strAutosaveDirectory( szRootDir );

		//expand the path if $SteamUserDir etc are used for SDK users
		EditorUtil_ConvertPath(strAutosaveDirectory, true);
		
		CString strExtension  = ".vmf";
		//this will hold the name of the map w/o leading directory info or file extension
		CString strMapTitle; 
		//full path of map file
		CString strMapFilename = pDoc->GetPathName(); 
	
		DWORD dwTotalAutosaveDirectorySize = 0;
		int nCurrentAutosaveNumber = 0;

		// the map hasn't been saved before and doesn't have a filename; using default: 'autosave'
		if ( strMapFilename.IsEmpty() ) 
		{
			strMapTitle = "autosave";
		}
		// the map already has a filename 
		else 
		{
			int nFilenameBeginOffset = strMapFilename.ReverseFind( '\\' ) + 1;
			int nFilenameEndOffset = strMapFilename.Find( '.' );
			//get the filename of the map, between the leading '\' and the '.'
			strMapTitle = strMapFilename.Mid( nFilenameBeginOffset, nFilenameEndOffset - nFilenameBeginOffset );			
		}
       
		nCurrentAutosaveNumber = GetNextAutosaveNumber( &autosaveFiles, &dwTotalAutosaveDirectorySize, &strMapTitle );

		//creating the proper suffix for the autosave file
		char szNumberChars[4];
        CString strAutosaveString = itoa( nCurrentAutosaveNumber, szNumberChars, 10 );
		CString strAutosaveNumber = "000";
		strAutosaveNumber += strAutosaveString;
		strAutosaveNumber = strAutosaveNumber.Right( 3 );
		strAutosaveNumber = "_" + strAutosaveNumber;
   
		CString strSaveName = strAutosaveDirectory + strMapTitle + strAutosaveNumber + strExtension + "_autosave";

		pDoc->SaveVMF( (char *)strSaveName.GetBuffer(), SAVEFLAGS_AUTOSAVE );
		//don't autosave again unless they make changes
		pDoc->SetAutosaveFlag( FALSE ); 

		//if there is too much space used for autosaves, delete the oldest file until the size is acceptable
		while( dwTotalAutosaveDirectorySize > dwMaxAutosaveSpace ) 
		{	
			int nFirstElementIndex = autosaveFiles.FirstInorder();
			if ( !autosaveFiles.IsValidIndex( nFirstElementIndex ) )
			{
				Assert( false );
				break;
			}

			WIN32_FIND_DATA fileData = autosaveFiles.Element( nFirstElementIndex );
			DWORD dwOldestFileSize =  fileData.nFileSizeLow;
			char filename[MAX_PATH];
			strcpy( filename, fileData.cFileName );
			DeleteFile( strAutosaveDirectory + filename );
			dwTotalAutosaveDirectorySize -= dwOldestFileSize;
			autosaveFiles.RemoveAt( nFirstElementIndex );			
		}
		
		autosaveFiles.RemoveAll();

		
	}
}

//-----------------------------------------------------------------------------
// Purpose: Verifies that the autosave directory exists and attempts to create it if 
//			it doesn't.  Also returns various failure errors.  
//			This function is now called at two different times: immediately after a new
//			directory is entered in the options screen and during every autosave call.
//			If called with a directory, the input directory is checked for correctness.
//			Otherwise, the system directory DIR_AUTOSAVE is checked
//-----------------------------------------------------------------------------
bool CHammer::VerifyAutosaveDirectory( char *szAutosaveDirectory ) const
{	
	HANDLE hDir;
	HANDLE hTestFile;

	char szRootDir[MAX_PATH];
	if ( szAutosaveDirectory )
	{
		strcpy( szRootDir, szAutosaveDirectory );
		EnsureTrailingBackslash( szRootDir );
	}
	else
	{
		APP()->GetDirectory(DIR_AUTOSAVE, szRootDir);
	}

	if ( szRootDir[0] == 0 )
	{
		AfxMessageBox( "No autosave directory has been selected.\nThe autosave feature will be disabled until a directory is entered.", MB_OK );
		return false;
	}
	CString strAutosaveDirectory( szRootDir );	
	{
		EditorUtil_ConvertPath(strAutosaveDirectory, true);
		if ( ( strAutosaveDirectory[1] != ':' ) || ( strAutosaveDirectory[2] != '\\' ) )
		{
			AfxMessageBox( "The current autosave directory does not have an absolute path.\nThe autosave feature will be disabled until a new directory is entered.", MB_OK );
			return false;
		}
	}

	hDir = CreateFile (
		strAutosaveDirectory,
		GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL
		);

	if ( hDir == INVALID_HANDLE_VALUE )
	{

		bool bDirResult = CreateDirectory( strAutosaveDirectory, NULL );
		if ( !bDirResult )
		{
			AfxMessageBox( "The current autosave directory does not exist and could not be created.  \nThe autosave feature will be disabled until a new directory is entered.", MB_OK );
			return false;
		}
	}    
	else
	{
		CloseHandle( hDir );

		hTestFile = CreateFile( strAutosaveDirectory + "test.txt", 
			GENERIC_READ,
			FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
			NULL,
			CREATE_NEW,
			FILE_FLAG_BACKUP_SEMANTICS,
			NULL
			);
		
		if ( hTestFile == INVALID_HANDLE_VALUE )
		{
			 if ( GetLastError() == ERROR_ACCESS_DENIED )
			 {
				 AfxMessageBox( "The autosave directory is marked as read only.  Please remove the read only attribute or select a new directory in Tools->Options->General.\nThe autosave feature will be disabled.", MB_OK );
				 return false;
			 }
			 else
			 {
				 AfxMessageBox( "There is a problem with the autosave directory.  Please select a new directory in Tools->Options->General.\nThe autosave feature will be disabled.", MB_OK );
				 return false;
			 }

			 
		}

		CloseHandle( hTestFile );
		DeleteFile( strAutosaveDirectory + "test.txt" );	
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called when Hammer starts after a crash.  It loads the newest 
//			autosave file after Hammer has initialized.
//-----------------------------------------------------------------------------
void CHammer::LoadLastGoodSave( void )
{	
	CString strLastGoodSave = APP()->GetProfileString("General", "Last Good Save", "");
	char szMapDir[MAX_PATH];
	strcpy(szMapDir, g_pGameConfig->szMapDir);
	CDocument *pCurrentDoc;

	if ( !strLastGoodSave.IsEmpty() )
	{		
		pCurrentDoc = APP()->OpenDocumentFile( strLastGoodSave );

		if ( !pCurrentDoc )
		{
			AfxMessageBox( "There was an error loading the last saved file.", MB_OK );
			return;
		}
		
		char szAutoSaveDir[MAX_PATH];	
		APP()->GetDirectory(DIR_AUTOSAVE, szAutoSaveDir);	

		if ( !((CMapDoc *)pCurrentDoc)->IsAutosave() && Q_stristr( pCurrentDoc->GetPathName(), szAutoSaveDir ) )
		{
			//This handles the case where someone recovers from a crash and tries to load an autosave file that doesn't have the new autosave chunk in it
			//It assumes the file should go into the gameConfig map directory
			char szRenameMessage[MAX_PATH+MAX_PATH+256];
			char szLastSaveCopy[MAX_PATH];
			Q_strcpy( szLastSaveCopy, strLastGoodSave );		
			char *pszFileName = Q_strrchr( strLastGoodSave, '\\') + 1;
			char *pszFileNameEnd = Q_strrchr( strLastGoodSave, '_');
			if ( !pszFileNameEnd )
			{
				pszFileNameEnd = Q_strrchr( strLastGoodSave, '.');
			}
			strcpy( pszFileNameEnd, ".vmf" );
			CString newMapPath( szMapDir );
			newMapPath.Append( "\\" );
			newMapPath.Append( pszFileName );
			sprintf( szRenameMessage, "The last saved map was found in the autosave directory.\nWould you like to rename it from \"%s\" to \"%s\"?\nNOTE: This will not save the file with the new name; it will only rename it.", szLastSaveCopy, (const char*)newMapPath );

			if ( AfxMessageBox( szRenameMessage, MB_YESNO ) == IDYES )
			{			
				pCurrentDoc->SetPathName( newMapPath );		
			}		
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called from the General Options dialog when the autosave timer or
//			directory values have changed.  
//-----------------------------------------------------------------------------
void CHammer::ResetAutosaveTimer()
{
	Options.general.bEnableAutosave = true;

	CMainFrame *pMainWnd = ::GetMainWnd();
	if ( pMainWnd )
	{
		pMainWnd->ResetAutosaveTimer();
	}
}
