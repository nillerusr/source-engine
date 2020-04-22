//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// JobWatchDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ServiceInstallDlg.h"
#include "tier1/strtools.h"


#define DEFAULT_INSTALL_LOCATION		"C:\\Program Files\\Valve\\vmpi_service"

#define HLKM_WINDOWS_RUN_KEY		"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define VMPI_SERVICE_VALUE_NAME		"VMPI Service"
#define VMPI_SERVICE_UI_VALUE_NAME	"VMPI Service UI"

// These are the files required for installation.
char *g_pInstallFiles[] =
{
	"vmpi_service.exe",
	"vmpi_service_ui.exe",
	"WaitAndRestart.exe",
	"vmpi_service_install.exe",
	"tier0.dll",
	"vmpi_transfer.exe",
	"filesystem_stdio.dll",
	"vstdlib.dll"
};


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


HWND g_hMessageControl = NULL;
HKEY g_hVMPIKey = NULL;	// hklm/software/valve/vmpi.
bool g_bNoOutput = false;
bool g_bDontTouchUI = false;
bool g_bReinstalling = false;

FILE *g_fpLog = NULL;


char* FindArg( int argc, char **argv, const char *pArgName, char *pDefaultValue="" )
{
	for ( int i=0; i < argc; i++ )
	{
		if ( stricmp( argv[i], pArgName ) == 0 )
		{
			if ( (i+1) >= argc )
				return pDefaultValue;
			else
				return argv[i+1];
		}
	}
	return NULL;
}

void CloseLog()
{
	if ( g_fpLog )
	{
		fflush( g_fpLog );
		fclose( g_fpLog );
		flushall();
		g_fpLog = NULL;
	}
}

void OpenLog()
{
	CloseLog();
	g_fpLog = fopen( "c:\\vmpi_service_install.log", "wt" );
}


void AddToLog( const char *pMsg )
{
	if ( g_fpLog )
	{
		fprintf( g_fpLog, "%s", pMsg );
	}
}


SpewRetval_t MySpewOutputFunc( SpewType_t spewType, const tchar *pMsg )
{
	AddToLog( pMsg );
	
	if ( spewType == SPEW_MESSAGE || spewType == SPEW_WARNING )
	{
		// Format the message and send it to the control.
		CUtlVector<char> msg;
		msg.SetSize( V_strlen( pMsg )*2 + 1 );

		char *pOut = msg.Base();
		const char *pIn = pMsg;
		while ( *pIn )
		{
			if ( *pIn == '\n' )
			{
				*pOut++ = '\r';
				*pOut++ = '\n';
			}
			else
			{
				*pOut++ = *pIn;
			}
			++pIn;
		}
		*pOut = 0;

		int nLen = (int)SendMessage( g_hMessageControl, EM_GETLIMITTEXT, 0, 0 );
		SendMessage( g_hMessageControl, EM_SETSEL, nLen, nLen );
		SendMessage( g_hMessageControl, EM_REPLACESEL, FALSE, (LPARAM)msg.Base() );
	}
	
	// Show a message box for warnings and errors.
	if ( spewType == SPEW_ERROR || spewType == SPEW_WARNING )
	{
		if ( !g_bNoOutput )
			AfxMessageBox( pMsg, MB_OK );
	}
	
	if ( spewType == SPEW_ERROR	)
	{
		CloseLog();
		TerminateProcess( GetCurrentProcess(), 2 );
	}

	return SPEW_CONTINUE;
}


void ScanDirectory( const char *pDirName, CUtlVector<CString> &subDirs, CUtlVector<CString> &files )
{
	subDirs.Purge();
	files.Purge();

	char strPattern[MAX_PATH];
	V_ComposeFileName( pDirName, "*.*", strPattern, sizeof( strPattern ) );

	WIN32_FIND_DATA fileInfo; // File information
	HANDLE hFile = ::FindFirstFile( strPattern, &fileInfo );
	if ( hFile == INVALID_HANDLE_VALUE )
		return;
		
	do
	{
		if ( fileInfo.cFileName[0] == '.' )
			continue;

		if ( fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			subDirs.AddToTail( fileInfo.cFileName );
		else
			files.AddToTail( fileInfo.cFileName );			
	} while( ::FindNextFile(hFile, &fileInfo) );

	::FindClose( hFile );
}


int DeleteDirectory( const char *pRootDir, bool bDeleteSubdirectories, char errorFile[MAX_PATH] )
{
	errorFile[0] = 0;

	CUtlVector<CString> subDirs, files;
	ScanDirectory( pRootDir, subDirs, files );

	// First nuke any subdirectories.
	if ( bDeleteSubdirectories && !g_bReinstalling )
	{
		for ( int i=0; i < subDirs.Count(); i++ )
		{	
			char fullName[MAX_PATH];
			V_ComposeFileName( pRootDir, subDirs[i], fullName, sizeof( fullName ) );
			
			// Delete subdirectory
			int iRC = DeleteDirectory( fullName, bDeleteSubdirectories, errorFile );
			if ( iRC )
				return iRC;
		}
	}
	
	for ( int i=0; i < files.Count(); i++ )
	{
		char fullName[MAX_PATH];
		V_ComposeFileName( pRootDir, files[i], fullName, sizeof( fullName ) );

		// Set file attributes
		if ( !SetFileAttributes( fullName, FILE_ATTRIBUTE_NORMAL ) )
			return GetLastError();

		// Delete file
		if ( !DeleteFile( fullName ) )
		{
			V_strncpy( errorFile, fullName, MAX_PATH );
			return GetLastError();
		}
	}

	if( !g_bReinstalling )
	{
		// Set directory attributes
		if ( !SetFileAttributes( pRootDir, FILE_ATTRIBUTE_NORMAL ) )
			return GetLastError();

		// Delete directory
		if ( !RemoveDirectory( pRootDir ) )
			return GetLastError();
	}

	return 0;
}


bool CreateDirectory_R( const char *pDirName )
{
	char chPrevDir[MAX_PATH];
	V_strncpy( chPrevDir, pDirName, sizeof( chPrevDir ) );
	if ( V_StripLastDir( chPrevDir, sizeof( chPrevDir ) ) )
	{
		if ( V_stricmp( chPrevDir, ".\\" ) != 0 && V_stricmp( chPrevDir, "./" ) != 0 )
			if ( !CreateDirectory_R( chPrevDir ) )
				return false;
	}

	if ( _access( pDirName, 0 ) == 0 )
		return true;
			
	return CreateDirectory( pDirName, NULL ) || GetLastError() == ERROR_ALREADY_EXISTS;
}


bool SetupStartMenuSubFolderName( const char *pSubFolderName, char *pOut, int outLen )
{
	LPITEMIDLIST pidl;

	// Get a pointer to an item ID list that represents the path of a special folder
	HRESULT hr = SHGetSpecialFolderLocation(NULL, CSIDL_COMMON_PROGRAMS, &pidl);
	if ( hr != S_OK )
		return false;

	// Convert the item ID list's binary representation into a file system path
	char szPath[_MAX_PATH];
	BOOL f = SHGetPathFromIDList(pidl, szPath);

	// Free the LPITEMIDLIST they gave us.
	LPMALLOC pMalloc;
	hr = SHGetMalloc(&pMalloc);
	pMalloc->Free(pidl);
	pMalloc->Release();

	if ( f )
	{
		V_ComposeFileName( szPath, pSubFolderName, pOut, outLen );
		return true;
	}
	else
	{
		return false;
	}
}

bool CreateStartMenuLink( const char *pSubFolderName, const char *pLinkName, const char *pLinkTarget, const char *pArguments )
{
	char fullFolderName[MAX_PATH];
	if ( !SetupStartMenuSubFolderName( pSubFolderName, fullFolderName, sizeof( fullFolderName ) ) )
		return false;
	
	// Create the folder if necessary.
	if ( !CreateDirectory_R( fullFolderName ) )
	{
		Msg( "CreateStartMenuLink failed - can't create directory %s.\n", fullFolderName );
		return false;
	}
	
	IShellLink* psl = NULL; 

	// Get a pointer to the IShellLink interface. 
	bool bRet = false;
	CoInitialize( NULL );
	HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, reinterpret_cast<void**>(&psl)); 
	if (SUCCEEDED(hres)) 
	{ 
		psl->SetPath( pLinkTarget );	// Set the path to the shortcut target
		if ( pArguments )
			psl->SetArguments( pArguments );

		// Query IShellLink for the IPersistFile interface for saving 
		//the shortcut in persistent storage. 
		IPersistFile* ppf = NULL; 
		hres = psl->QueryInterface( IID_IPersistFile, reinterpret_cast<void**>(&ppf) );
		if (SUCCEEDED(hres)) 
		{ 
			// Setup the filename for the link.
			char linkFilename[MAX_PATH];
			V_ComposeFileName( fullFolderName, pLinkName, linkFilename, sizeof( linkFilename ) );
			V_strncat( linkFilename, ".lnk", sizeof( linkFilename ) );

			// Ensure that the string is ANSI. 
			WCHAR wsz[MAX_PATH]; 
			MultiByteToWideChar(CP_ACP, 0, linkFilename, -1, wsz, MAX_PATH); 

			// Save the link by calling IPersistFile::Save. 
			hres = ppf->Save( wsz, TRUE );
			ppf->Release(); 
			bRet = true;
		} 
		
		psl->Release(); 
	}
	CoUninitialize();
	return bRet;
}



char* GetLastErrorString()
{
	static char err[2048];
	
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

	strncpy( err, (char*)lpMsgBuf, sizeof( err ) );
	LocalFree( lpMsgBuf );

	err[ sizeof( err ) - 1 ] = 0;

	return err;
}


bool LaunchApp( char *pCommandLine, const char *pBaseDir )
{
	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );

	return CreateProcess( NULL, pCommandLine, NULL,	NULL, FALSE, 0,	NULL, pBaseDir, &si, &pi ) != 0;
}		


bool StartVMPIServiceUI( const char *pInstallLocation )
{
	if ( g_bDontTouchUI )
	{
		Msg( "StartVMPIServiceUI: Ignoring due to -DontTouchUI.\n" );
		return true;
	}
	
	char cmdLine[MAX_PATH];
	V_ComposeFileName( pInstallLocation, "vmpi_service_ui.exe", cmdLine, sizeof( cmdLine ) );
	return LaunchApp( cmdLine, pInstallLocation );
}


bool StartVMPIService( SC_HANDLE hSCManager )
{
	bool bRet = true;

	// First, get rid of an old service with the same name.
	SC_HANDLE hMyService = OpenService( hSCManager, VMPI_SERVICE_NAME_INTERNAL, SERVICE_START );
	if ( hMyService )
	{
		if ( StartService( hMyService, NULL, NULL ) )
		{
			Msg( "Started!\n" );
		}
		else
		{
			Error( "Can't start the service.\n" );
			bRet = false;
		}
	}
	else
	{
		Error( "Can't open service: %s\n", VMPI_SERVICE_NAME_INTERNAL );
		bRet = false;
	}

	CloseServiceHandle( hMyService );
	return bRet;
}


bool StopRunningApp()
{
	if ( g_bDontTouchUI )
	{
		Msg( "StopRunningApp: -DontTouchUI was specified, so exiting before stopping the app.\n" );
		return true;
	}
	
	// Send the 
	ISocket *pSocket = CreateIPSocket();
	if ( pSocket )
	{
		if ( pSocket->BindToAny( 0 ) )
		{
			CUtlVector<char> protocolVersions;
			protocolVersions.AddToTail( VMPI_PROTOCOL_VERSION );
			if ( VMPI_PROTOCOL_VERSION == 5 )
				protocolVersions.AddToTail( 4 );	// We want this installer to kill the previous services too.
			
			for ( int iProtocolVersion=0; iProtocolVersion < protocolVersions.Count(); iProtocolVersion++ )
			{
				char cPacket[4] =
				{
					protocolVersions[iProtocolVersion],
					VMPI_PASSWORD_OVERRIDE,	// (force it to accept this message).
					0,
					VMPI_STOP_SERVICE
				};
				
				CIPAddr addr( 127, 0, 0, 1, 0 );
				
				for ( int iPort=VMPI_SERVICE_PORT; iPort <= VMPI_LAST_SERVICE_PORT; iPort++ )
				{
					addr.port = iPort;
					pSocket->SendTo( &addr, cPacket, sizeof( cPacket ) );
				}
			}
			
			// Give it a sec to get the message and shutdown in case we're restarting.
			Sleep( 2000 );
			
			
			// This is the overkill method. If it didn't shutdown gracefully, kill it.
			HMODULE hInst = LoadLibrary( "psapi.dll" );
			if ( hInst )
			{
				typedef BOOL (WINAPI *EnumProcessesFn)(DWORD *lpidProcess, DWORD cb, DWORD *cbNeeded);
				typedef BOOL (WINAPI *EnumProcessModulesFn)(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded );
				typedef DWORD (WINAPI *GetModuleBaseNameFn)( HANDLE hProcess, HMODULE hModule, LPTSTR lpBaseName, DWORD nSize );
				
				EnumProcessesFn EnumProcesses = (EnumProcessesFn)GetProcAddress( hInst, "EnumProcesses" );
				EnumProcessModulesFn EnumProcessModules = (EnumProcessModulesFn)GetProcAddress( hInst, "EnumProcessModules" );
				GetModuleBaseNameFn GetModuleBaseName = (GetModuleBaseNameFn)GetProcAddress( hInst, "GetModuleBaseNameA" );
				if ( EnumProcessModules && EnumProcesses )
				{				
					// Now just to make sure, kill the processes we're interested in.
					DWORD procIDs[1024];
					DWORD nBytes;
					if ( EnumProcesses( procIDs, sizeof( procIDs ), &nBytes ) )
					{
						DWORD nProcs = nBytes / sizeof( procIDs[0] );
						for ( DWORD i=0; i < nProcs; i++ )
						{
							HANDLE hProc = OpenProcess( PROCESS_ALL_ACCESS, FALSE, procIDs[i] );
							if ( hProc )
							{
								HMODULE hModules[1024];
								if ( EnumProcessModules( hProc, hModules, sizeof( hModules ), &nBytes ) )
								{
									DWORD nModules = nBytes / sizeof( hModules[0] );
									for ( DWORD iModule=0; iModule < nModules; iModule++ )
									{
										char filename[512];
										if ( GetModuleBaseName( hProc, hModules[iModule], filename, sizeof( filename ) ) )
										{
											if ( Q_stristr( filename, "vmpi_service.exe" ) || Q_stristr( filename, "vmpi_service_ui.exe" ) )
											{
												TerminateProcess( hProc, 1 );
												CloseHandle( hProc );
												hProc = NULL;
												break;
											}
										}
									}
								}

								CloseHandle( hProc );
							}
						}
					}
				}

				FreeLibrary( hInst );
			}
		}

		pSocket->Release();
	}

	return true;
}


bool StopOrDeleteService( SC_HANDLE hSCManager, bool bDelete )
{
	bool bRet = true;

	// First, get rid of an old service with the same name.
	SC_HANDLE hOldService = OpenService( hSCManager, VMPI_SERVICE_NAME_INTERNAL, SERVICE_STOP | DELETE );
	if ( hOldService )
	{
		// Stop the service.
		Msg( "Found the service already running.\n" );
		Msg( "Stopping service...\n" );
		SERVICE_STATUS status;
		ControlService( hOldService, SERVICE_CONTROL_STOP, &status );
		
		if ( bDelete )
		{
			Msg( "Deleting service...\n" );
			bool bExitedNicely = false;
			DWORD startTime = GetTickCount();
			while ( 1 )
			{
				BOOL bRet = DeleteService( hOldService );
				if ( !bRet || bRet == ERROR_SERVICE_MARKED_FOR_DELETE )
				{
					Msg( "Deleted old service.\n" );
					bExitedNicely = true;
					break;
				}
				
				// Wait for the service to stop for 8 seconds.
				if ( GetTickCount() - startTime > 8000 )
					break;
			}

			if ( !bExitedNicely )
			{
				Error( "Couldn't delete the old '%s' service! Error: %s.\n", VMPI_SERVICE_NAME, GetLastErrorString() );
				bRet = false;
			}
		}

		CloseServiceHandle( hOldService );
	}

	return bRet;
}


bool GetExistingInstallationLocation( CString &strInstallLocation )
{
	char buf[1024];
	DWORD bufSize = sizeof( buf );
	DWORD dwType;
	if ( RegQueryValueEx( g_hVMPIKey, SERVICE_INSTALL_LOCATION_KEY, NULL, &dwType, (LPBYTE)buf, &bufSize ) == ERROR_SUCCESS )
	{
		if ( dwType == REG_SZ )
		{
			strInstallLocation = buf;
			return true;
		}
	}
	
	return false;
}

void RemoveRegistryKeys()
{
	// Delete the run values (that tells it to run the app when the user logs in).
	HKEY hKey = NULL;
	RegCreateKey( HKEY_LOCAL_MACHINE, HLKM_WINDOWS_RUN_KEY, &hKey );
	RegDeleteValue( hKey, VMPI_SERVICE_VALUE_NAME );
	RegDeleteValue( hKey, VMPI_SERVICE_UI_VALUE_NAME );

	// Get rid of the "InstallLocation" value.
	RegDeleteValue( g_hVMPIKey, SERVICE_INSTALL_LOCATION_KEY );
}


bool IsAnInstallFile( const char *pName )
{
	for ( int i=0; i < ARRAYSIZE( g_pInstallFiles ); i++ )
	{
		if ( V_stricmp( g_pInstallFiles[i], pName ) == 0 )
			return true;
	}
	return false;
}


bool AnyNonInstallFilesInDirectory( const char *strInstallLocation )
{
	char searchStr[MAX_PATH];
	V_ComposeFileName( strInstallLocation, "*.*", searchStr, sizeof( searchStr ) );

	_finddata_t data;
	long handle = _findfirst( searchStr, &data );
	if ( handle != -1 )
	{
		do
		{
			if ( data.name[0] == '.' || (data.attrib & _A_SUBDIR) != 0 )
				continue;
				
			if ( !IsAnInstallFile( data.name ) )
				return true;
			
		} while( _findnext( handle, &data ) == 0 );
	
		_findclose( handle );
	}
	return false;
}


/////////////////////////////////////////////////////////////////////////////
// CServiceInstallDlg dialog


CServiceInstallDlg::CServiceInstallDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CServiceInstallDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CServiceInstallDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


CServiceInstallDlg::~CServiceInstallDlg()
{
}


void CServiceInstallDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CServiceInstallDlg)
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CServiceInstallDlg, CDialog)
	//{{AFX_MSG_MAP(CServiceInstallDlg)
	ON_BN_CLICKED(IDC_CANCEL_BUTTON, OnCancel)
	ON_BN_CLICKED(IDC_INSTALL_BUTTON, OnInstall)
	ON_BN_CLICKED(IDC_UNINSTALL_BUTTON2, OnUninstall)
	ON_BN_CLICKED(IDC_START_EXISTING_BUTTON, OnStartExisting)
	ON_BN_CLICKED(IDC_STOP_EXISTING_BUTTON, OnStopExisting)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CServiceInstallDlg message handlers

const char* FindArg( const char *pArgName, const char *pDefault="" )
{
	for ( int i=1; i < __argc; i++ )
	{
		if ( Q_stricmp( pArgName, __argv[i] ) == 0 )
		{
			if ( (i+1) < __argc )
				return __argv[i+1];
			else
				return pDefault;
		}
	}
	return NULL;
}


BOOL CServiceInstallDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	HICON hIcon = LoadIcon( AfxGetInstanceHandle(), MAKEINTRESOURCE( IDR_MAINFRAME ) );
	SetIcon( hIcon, true );

	OpenLog();

	// Setup the registry key for the install location.
	if ( RegCreateKey( HKEY_LOCAL_MACHINE, VMPI_SERVICE_KEY, &g_hVMPIKey ) != ERROR_SUCCESS )
	{
		Error( "Can't open registry key: %s.", VMPI_SERVICE_KEY );
		return FALSE;
	}

	VerifyInstallFiles();

	g_hMessageControl = ::GetDlgItem( GetSafeHwnd(), IDC_TEXTOUTPUT );
	SpewOutputFunc( MySpewOutputFunc );

	// Init the service manager.	
	m_hSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	if ( !m_hSCManager )
	{
		Error( "OpenSCManager failed (%s)!\n", GetLastErrorString() );
		return FALSE;
	}

									
	// See if there is a previous installation.
	CString strInstallLocation;
	if ( GetExistingInstallationLocation( strInstallLocation ) )
		SetDlgItemText( IDC_INSTALL_LOCATION, strInstallLocation );
	else
		SetDlgItemText( IDC_INSTALL_LOCATION, DEFAULT_INSTALL_LOCATION );

	// Now, if they passed in a command line option .
	if ( FindArg( __argc, __argv, "-install_quiet" ) )
	{
		g_bReinstalling = true;
		g_bNoOutput = true;
		g_bDontTouchUI = (FindArg( __argc, __argv, "-DontTouchUI", NULL ) != 0);
		OnInstall();
		EndDialog( 0 );
	}
	else if ( FindArg( __argc, __argv, "-uninstall_quiet" ) )
	{
		g_bNoOutput = true;
		DoUninstall( false );
		EndDialog( 0 );
	}
	else if ( FindArg( __argc, __argv, "-start" ) )
	{
		OnStartExisting();
	}
	
	else if ( FindArg( __argc, __argv, "-stop" ) )
	{
		OnStopExisting();
	}
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void CServiceInstallDlg::OnCancel(void)
{
	EndDialog( 1 );
}

// This function registers the service with the service manager.
bool InstallService( SC_HANDLE hSCManager, const char *pBaseDir )
{
	char filename[512], uiFilename[512];
	V_ComposeFileName( pBaseDir, "vmpi_service.exe", filename, sizeof( filename ) );
	V_ComposeFileName( pBaseDir, "vmpi_service_ui.exe", uiFilename, sizeof( uiFilename ) );


	// Try a to reinstall the service for up to 5 seconds.
	Msg( "Creating new service...\n" );

	SC_HANDLE hMyService = NULL;
	DWORD startTime = GetTickCount();
	while ( GetTickCount() - startTime < 5000 )
	{
		// Now reinstall it.
		hMyService = CreateService(
			hSCManager,				// Service Control Manager database.
			VMPI_SERVICE_NAME_INTERNAL,		// Service name.
			VMPI_SERVICE_NAME,		// Display name.
			SERVICE_ALL_ACCESS,
			SERVICE_WIN32_OWN_PROCESS,
			SERVICE_AUTO_START,		// Start automatically on system bootup.
			SERVICE_ERROR_NORMAL,
			filename,				// Executable to register for the service.
			NULL,					// no load ordering group 
			NULL,					// no tag identifier 
			NULL,					// no dependencies 
			NULL,					// account
			NULL					// password 
			);
	
		if ( hMyService )
			break;
		else
			Sleep( 300 );
	}

	if ( !hMyService )
	{
		Warning( "CreateService failed (%s)!\n", GetLastErrorString() );
		CloseServiceHandle( hSCManager );
		return false;
	}


	// Now setup the UI executable to run when their system starts.
	HKEY hUIKey = NULL;
	RegCreateKey( HKEY_LOCAL_MACHINE, HLKM_WINDOWS_RUN_KEY, &hUIKey );
	if ( !hUIKey || RegSetValueEx( hUIKey, VMPI_SERVICE_UI_VALUE_NAME, 0, REG_SZ, (unsigned char*)uiFilename, strlen( uiFilename) + 1 ) != ERROR_SUCCESS )
	{
		Warning( "Can't install registry key for %s\n", uiFilename );
		return false;
	}

	CloseServiceHandle( hMyService );
	return true;
}


void SetupStartMenuLinks( const char *pInstallerFilename )
{
	CreateStartMenuLink( "Valve\\VMPI", "Start VMPI Service", pInstallerFilename, "-start" );
	CreateStartMenuLink( "Valve\\VMPI", "Stop VMPI Service", pInstallerFilename, "-stop" );
	CreateStartMenuLink( "Valve\\VMPI", "Uninstall VMPI", pInstallerFilename, NULL );
}


void RemoveStartMenuLinks()
{
	char fullFolderName[MAX_PATH];
	if ( !SetupStartMenuSubFolderName( "Valve\\VMPI", fullFolderName, sizeof( fullFolderName ) ) )
		return;
	
	char errorFile[MAX_PATH];
	if ( !DeleteDirectory( fullFolderName, true, errorFile ) )
	{
		Msg( "Unable to remove Start Menu items in %s.\n", fullFolderName );
	}
}


void CServiceInstallDlg::OnInstall()
{
	// Get the install location.
	Msg( "Verifying install location.\n" );
	CString strInstallLocation;
	if ( !GetDlgItemText( IDC_INSTALL_LOCATION, strInstallLocation ) )
	{
		Error( "Can't get install location." );
		return;
	}

	if ( strchr( strInstallLocation, ':' ) == NULL )
	{
		Warning( "Install location must be an absolute path (include a colon)." );
		return;
	}

	// Stop the existing service.
	if ( !DoUninstall( false ) )
		return;

	// Create the directory.
	Msg( "Creating install directory %s.\n", strInstallLocation );
	if ( !CreateDirectory_R( strInstallLocation ) )
	{
		Warning( "Unable to create directory: %s.", (const char*)strInstallLocation );
		return;
	}
	
	// Copy the files down.
	Msg( "Copying files.\n" );
	char chDir[MAX_PATH];
	GetModuleFileName( NULL, chDir, sizeof( chDir ) );
	V_StripFilename( chDir );
	for ( int i=0; i < ARRAYSIZE( g_pInstallFiles ); i++ )
	{
		char srcFilename[MAX_PATH], destFilename[MAX_PATH];
		V_ComposeFileName( chDir, g_pInstallFiles[i], srcFilename, sizeof( srcFilename ) );
		V_ComposeFileName( strInstallLocation, g_pInstallFiles[i], destFilename, sizeof( destFilename ) );
		
		if ( !CopyFile( srcFilename, destFilename, FALSE ) )
		{
			Sleep( 2000 );
			
			if ( !CopyFile( srcFilename, destFilename, FALSE ) )
			{
				Error( "CopyFile() failed.\nSrc: %s\nDest: %s\n%s", srcFilename, destFilename, GetLastErrorString() );
				return;
			}
		}
	}

	// Register the service.
	if ( !InstallService( m_hSCManager, strInstallLocation ) )
		return;

	// Write the new location to the registry.
	Msg( "Updating registry.\n" );
	if ( RegSetValueEx( g_hVMPIKey, SERVICE_INSTALL_LOCATION_KEY, 0, REG_SZ, (BYTE*)(const char*)strInstallLocation, V_strlen( strInstallLocation ) + 1 ) != ERROR_SUCCESS )
	{
		Error( "RegSetValueEx( %s, %s ) failed.", SERVICE_INSTALL_LOCATION_KEY, (const char*)strInstallLocation );
		return;
	}
	
	// Setup start menu links.
	char installerFilename[MAX_PATH];
	V_ComposeFileName( strInstallLocation, "vmpi_service_install.exe", installerFilename, sizeof( installerFilename ) );
	SetupStartMenuLinks( installerFilename );
	
	// Start the new service.
	Msg( "Starting new service.\n" );
	if ( DoStartExisting() )
	{
		Warning( "Installed successfully!" );
	}
}

bool CServiceInstallDlg::DoUninstall( bool bShowMessage )
{
	// Figure out where to uninstall from.
	CString strInstallLocation;
	if ( !GetDlgItemText( IDC_INSTALL_LOCATION, strInstallLocation ) )
	{
		Error( "Can't get install location." );
		return false;
	}

	if ( _access( strInstallLocation, 0 ) == 0 && !g_bNoOutput )
	{
		// Don't ask if they care if we delete all the files in that directory if the only exes in there are the install exes.
		if ( AnyNonInstallFilesInDirectory( strInstallLocation ) )
		{		
			char str[512];
			V_snprintf( str, sizeof( str ), "Warning: this will delete all files under this directory: \n%s\nContinue?", strInstallLocation );
			if ( AfxMessageBox( str, MB_YESNO ) != IDYES )
				return false;
		}
	}

	// Stop both the service and the win app.
	bool bDone = StopRunningApp() && StopOrDeleteService( m_hSCManager, true );
	if ( !bDone )
		return false;

	bool bSuccess = true;
	RemoveRegistryKeys();
	char errorFile[MAX_PATH];
	if ( !NukeDirectory( strInstallLocation, errorFile ) )
	{
		// When reinstalling, the service may not be done exiting, so give it a sec.
		Sleep( 2000 );
		if ( !NukeDirectory( strInstallLocation, errorFile ) )
		{
			if ( errorFile[0] )
				Msg( "NukeDirectory( %s ) failed.\nError on file: %s\n", strInstallLocation, errorFile );
			else
				Msg( "NukeDirectory( %s ) failed.\n", strInstallLocation );
			
			Msg( "Uninstall complete, but files are left over in %s.\n", strInstallLocation );
			
			bSuccess = false;
		}
	}

	RemoveStartMenuLinks();

	if ( bShowMessage && bSuccess )
		AfxMessageBox( "Uninstall successful." );
		
	return true;
}

void CServiceInstallDlg::OnUninstall()
{
	DoUninstall( true );
}

void CServiceInstallDlg::OnStartExisting()
{
	if ( DoStartExisting() )
		AfxMessageBox( "Started successfully." );
}

bool CServiceInstallDlg::DoStartExisting()
{
	StopRunningApp();
	StopOrDeleteService( m_hSCManager, false );
	
	CString strInstallLocation;
	if ( !GetExistingInstallationLocation( strInstallLocation ) )
	{
		Error( "The VMPI service is not installed." );
		return false;
	}
	
	if ( StartVMPIService( m_hSCManager ) )
	{
		return StartVMPIServiceUI( strInstallLocation );
	}
	else
	{
		return false;
	}
}

void CServiceInstallDlg::OnStopExisting()
{
	
	// Stop the app but don't delete it.
	bool bDone = StopRunningApp() && StopOrDeleteService( m_hSCManager, false );
	if ( bDone )
	{
		AfxMessageBox( "Service successfully stopped." );
	}
}

bool CServiceInstallDlg::NukeDirectory( const char *pDir, char errorFile[MAX_PATH] )
{
	// If the directory doesn't exist anyways, then return true..
	if ( _access( pDir, 0 ) != 0 )
		return true;

	return DeleteDirectory( pDir, true, errorFile ) == 0;
}


void CServiceInstallDlg::VerifyInstallFiles()
{
	char chDir[MAX_PATH];
	GetModuleFileName( NULL, chDir, sizeof( chDir ) );
	V_StripFilename( chDir );	 
	
	for ( int i=0; i < ARRAYSIZE( g_pInstallFiles ); i++ )
	{
		char filename[MAX_PATH];
		V_ComposeFileName( chDir, g_pInstallFiles[i], filename, sizeof( filename ) );
		
		if ( _access( filename, 0 ) != 0 )
		{
			char szErrorMessage[MAX_PATH];
			
			V_snprintf( szErrorMessage, sizeof( szErrorMessage ), "Required installation file missing: %s", filename );

			AfxMessageBox( szErrorMessage );
			return;
		}
	}
}

