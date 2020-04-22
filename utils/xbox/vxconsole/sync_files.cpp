//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SYNC_FILES.CPP
//
//	Sync Files Routines.
//=====================================================================================//
#include "vxconsole.h"

char		g_syncfiles_localPath[MAX_PATH];
char		g_syncfiles_targetPath[MAX_PATH];
fileNode_t	*g_syncfiles_targetFiles;
int			g_syncfiles_numTargetFiles;
CProgress	*g_syncFiles_progress;
BOOL		g_syncFiles_noWrite;
BOOL		g_syncFiles_force;
BOOL		g_syncFiles_verbose;

struct dvdimage_t
{
	char		szString[MAX_PATH];
	CUtlString	installPath;
	CUtlString	versionDetailString;
};
CUtlVector< dvdimage_t> g_install_dvdImages;
int			g_install_Selection;
bool		g_install_bForceSync;
bool		g_install_bCleanTarget;

//-----------------------------------------------------------------------------
//	SyncFilesDlg_Validate
//
//-----------------------------------------------------------------------------
bool SyncFilesDlg_Validate()
{
	if ( !g_connectedToXBox )
	{
		Sys_MessageBox( "Sync Error", "Cannot sync until connected to XBox." );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//	SyncFilesDlg_Setup
//
//-----------------------------------------------------------------------------
void SyncFilesDlg_Setup( HWND hWnd )
{
	SetDlgItemText( hWnd, IDC_SYNCFILES_LOCALPATH, g_syncfiles_localPath ); 
	SetDlgItemText( hWnd, IDC_SYNCFILES_TARGETPATH, g_syncfiles_targetPath );

	CheckDlgButton( hWnd, IDC_SYNCFILES_FORCESYNC, g_syncFiles_force ? BST_CHECKED : BST_UNCHECKED );
	CheckDlgButton( hWnd, IDC_SYNCFILES_NOWRITE, g_syncFiles_noWrite ? BST_CHECKED : BST_UNCHECKED );
	CheckDlgButton( hWnd, IDC_SYNCFILES_VERBOSE, g_syncFiles_verbose ? BST_CHECKED : BST_UNCHECKED );
}

//-----------------------------------------------------------------------------
//	SyncFilesDlg_GetChanges
//
//-----------------------------------------------------------------------------
bool SyncFilesDlg_GetChanges( HWND hwnd )
{
	char	localPath[MAX_PATH];
	char	targetPath[MAX_PATH];
	
	localPath[0]  = '\0';
	targetPath[0] = '\0';

	GetDlgItemText( hwnd, IDC_SYNCFILES_LOCALPATH, localPath, MAX_PATH ); 
	GetDlgItemText( hwnd, IDC_SYNCFILES_TARGETPATH, targetPath, MAX_PATH ); 
	
	strcpy( g_syncfiles_localPath, localPath );
	Sys_NormalizePath( g_syncfiles_localPath, true );

	strcpy( g_syncfiles_targetPath, targetPath );
	Sys_NormalizePath( g_syncfiles_targetPath, true );

	g_syncFiles_force = IsDlgButtonChecked( hwnd, IDC_SYNCFILES_FORCESYNC ) != 0;
	g_syncFiles_noWrite = IsDlgButtonChecked( hwnd, IDC_SYNCFILES_NOWRITE ) != 0;
	g_syncFiles_verbose = IsDlgButtonChecked( hwnd, IDC_SYNCFILES_VERBOSE ) != 0;

	// success
	return ( true );
}

//-----------------------------------------------------------------------------
//	SyncFilesDlg_Proc
//
//-----------------------------------------------------------------------------
BOOL CALLBACK SyncFilesDlg_Proc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{ 
	switch ( message ) 
	{ 
		case WM_INITDIALOG:
			SyncFilesDlg_Setup( hwnd );
			return ( TRUE );

		case WM_COMMAND: 
            switch ( LOWORD( wParam ) ) 
            {
				case IDC_OK: 
					if ( !SyncFilesDlg_GetChanges( hwnd ) )
						break;
					EndDialog( hwnd, wParam );
					return ( TRUE ); 

				case IDCANCEL:
				case IDC_CANCEL: 
					EndDialog( hwnd, wParam );
					return ( TRUE ); 
			}
			break; 
	}
	return ( FALSE ); 
} 

//-----------------------------------------------------------------------------
//	SyncFiles_FreeTargetList
//
//-----------------------------------------------------------------------------
void SyncFiles_FreeTargetList()
{
	g_syncfiles_numTargetFiles = 0;

	FreeTargetFileList( g_syncfiles_targetFiles );
	g_syncfiles_targetFiles = NULL;
}

//-----------------------------------------------------------------------------
//	SyncFiles_DoUpdates
//
//-----------------------------------------------------------------------------
bool SyncFiles_DoUpdates()
{
	WIN32_FILE_ATTRIBUTE_DATA	localAttributes;
	fileNode_t					*nodePtr;
	char						sourceFilename[MAX_PATH];
	char						statusBuff1[MAX_PATH];
	char						statusBuff2[MAX_PATH];
	int							targetPathLen;
	char						*ptr;
	int							progress;
	int							fileSyncMode;
	int							numSynced;
	int							retVal;

	g_syncFiles_progress->SetStatus( "Syncing Target Files...", "", "" );
	g_syncFiles_progress->SetMeter( 0, g_syncfiles_numTargetFiles );

	fileSyncMode = FSYNC_ANDEXISTSONTARGET;
	if ( g_syncFiles_force )
		fileSyncMode |= FSYNC_ALWAYS;
	else
		fileSyncMode |= FSYNC_IFNEWER;

	targetPathLen = strlen( g_syncfiles_targetPath );

	numSynced = 0;
	progress = 0;
	nodePtr = g_syncfiles_targetFiles;
	while ( nodePtr )
	{
		ptr = nodePtr->filename+targetPathLen;
		if ( *ptr == '\\' )
			ptr++;

		// replace with source path head
		strcpy( sourceFilename, g_syncfiles_localPath );
		Sys_AddFileSeperator( sourceFilename, sizeof( sourceFilename ) );
		strcat( sourceFilename, ptr );

		float sourceFileSize = 0;
		if ( GetFileAttributesEx( sourceFilename, GetFileExInfoStandard, &localAttributes ) )
		{
			sourceFileSize = (float)localAttributes.nFileSizeLow / (1024.0f * 1024.0f);
		}

		_snprintf( statusBuff1, sizeof( statusBuff1 ), "Local File: %s (%.2f MB)", sourceFilename, sourceFileSize );
		statusBuff1[sizeof( statusBuff1 ) - 1] = '\0';
		_snprintf( statusBuff2, sizeof( statusBuff2 ), "Target File: %s", nodePtr->filename );
		statusBuff1[sizeof( statusBuff2 ) - 1] = '\0';
		g_syncFiles_progress->SetStatus( NULL, statusBuff1, statusBuff2 );

		retVal = FileSyncEx( sourceFilename, nodePtr->filename, fileSyncMode, g_syncFiles_verbose != FALSE, g_syncFiles_noWrite != FALSE);
		if ( retVal == 1 )
		{
			ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Sync: %s -> %s\n", sourceFilename, nodePtr->filename );
			numSynced++;
		}

		progress++;
		g_syncFiles_progress->SetMeter( progress, -1 );
		if ( g_syncFiles_progress->IsCancel() )
		{
			return false;
		}

		nodePtr = nodePtr->nextPtr;
	}

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Synced %d/%d Files.\n", numSynced, g_syncfiles_numTargetFiles );

	return true;
}

//-----------------------------------------------------------------------------
//	SyncFiles_GetTargetList
//
//-----------------------------------------------------------------------------
bool SyncFiles_GetTargetList( char* pTargetPath )
{
	g_syncFiles_progress->SetStatus( "Getting Target Files...", "", "" );
	g_syncFiles_progress->SetMeter( 0, 100 );

	if ( !GetTargetFileList_r( pTargetPath, true, FA_NORMAL|FA_READONLY, 0, &g_syncfiles_targetFiles ) )
	{
		ConsoleWindowPrintf( RGB( 255,0,0 ), "Bad Target Path '%s'\n", pTargetPath );
		return false;
	}

	fileNode_t*	pFileNode;
	for ( pFileNode = g_syncfiles_targetFiles; pFileNode; pFileNode = pFileNode->nextPtr )
	{
		g_syncfiles_numTargetFiles++;
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
//	SyncFilesDlg_SaveConfig
// 
//-----------------------------------------------------------------------------
void SyncFilesDlg_SaveConfig()
{
	Sys_SetRegistryString( "syncfiles_localPath", g_syncfiles_localPath );
	Sys_SetRegistryString( "syncfiles_targetPath", g_syncfiles_targetPath );
	Sys_SetRegistryInteger( "syncfiles_force", g_syncFiles_force );
	Sys_SetRegistryInteger( "syncfiles_noWrite", g_syncFiles_noWrite );
	Sys_SetRegistryInteger( "syncfiles_verbose", g_syncFiles_verbose );
}

//-----------------------------------------------------------------------------
//	SyncFilesDlg_Open
//
//-----------------------------------------------------------------------------
void SyncFilesDlg_Open( void )
{
	int		result;
	bool	bError;
	bool	bValid;

	result = DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_SYNCFILES ), g_hDlgMain, ( DLGPROC )SyncFilesDlg_Proc );
	if ( LOWORD( result ) != IDC_OK )
		return;

	SyncFilesDlg_SaveConfig();

	if ( !SyncFilesDlg_Validate() )
		return;

	g_syncFiles_progress = new CProgress;
	g_syncFiles_progress->Open( "Sync Files...", true, true );

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "\nSyncing Files From '%s' to '%s'\n", g_syncfiles_localPath, g_syncfiles_targetPath );
	
	bError = true;

	// get a file listing from target
	bValid = SyncFiles_GetTargetList( g_syncfiles_targetPath );
	if ( !bValid )
		goto cleanUp;

	// mark all files needing update
	bValid = SyncFiles_DoUpdates();
	if ( !bValid )
		goto cleanUp;

	bError = false;

cleanUp:
	delete g_syncFiles_progress;
	g_syncFiles_progress = NULL;

	if ( bError )
		ConsoleWindowPrintf( XBX_CLR_RED, "Aborted.\n" );
	else
		ConsoleWindowPrintf( XBX_CLR_DEFAULT, "Finished.\n" );

	SyncFiles_FreeTargetList();
}

//-----------------------------------------------------------------------------
//	SyncFilesDlg_LoadConfig	
// 
//-----------------------------------------------------------------------------
void SyncFilesDlg_LoadConfig()
{
	// get our config
	Sys_GetRegistryString( "syncfiles_localPath", g_syncfiles_localPath, g_localPath, sizeof( g_syncfiles_localPath ) );
	Sys_GetRegistryString( "syncfiles_targetPath", g_syncfiles_targetPath, g_targetPath, sizeof( g_syncfiles_targetPath ) );
	Sys_GetRegistryInteger( "syncfiles_force", false, g_syncFiles_force );
	Sys_GetRegistryInteger( "syncfiles_noWrite", false, g_syncFiles_noWrite );
	Sys_GetRegistryInteger( "syncfiles_verbose", false, g_syncFiles_verbose );
}

//-----------------------------------------------------------------------------
//	SyncFilesDlg_Init
// 
//-----------------------------------------------------------------------------
bool SyncFilesDlg_Init()
{
	SyncFilesDlg_LoadConfig();
	return true;
}

//-----------------------------------------------------------------------------
//	InstallDlg_InstallImage
// 
//-----------------------------------------------------------------------------
void InstallDlg_InstallImage( const char *pInstallPath, bool bForce, bool bCleanTarget )
{
	int				errCode;
	char			*pToken;
	char			arg1[MAXTOKEN];
	char			arg2[MAXTOKEN];
	char			sourceFilename[MAX_PATH];
	char			sourcePath[MAX_PATH];
	char			targetFilename[MAX_PATH];
	char			filename[MAX_PATH];
	WIN32_FIND_DATA	findData;
	HANDLE			h;

	if ( !pInstallPath[0] )
	{
		return;
	}

	ConsoleWindowPrintf( XBX_CLR_DEFAULT, "\nSyncing From Install Depot: %s\n", pInstallPath );

	// get the install script
	char szScriptPath[MAX_PATH];
	V_ComposeFileName( pInstallPath, "../dvd_install.txt", szScriptPath, sizeof( szScriptPath ) );
	if ( !Sys_Exists( szScriptPath ) )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "Failed to open: %s\n", szScriptPath );
		return;
	}

	// sanity check
	// DVD image auto-build failure prevents the presence of the default.xex
	char szTempFilename[MAX_PATH];
	V_ComposeFileName( pInstallPath, "default.xex", szTempFilename, sizeof( szTempFilename ) );
	if ( !Sys_Exists( szTempFilename ) )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "Cancelled Installation! DVD Image Auto-Build Process Failed - Missing default.xex!\n" );
		return;
	}

	// get the version detail
	char szVersionPath[MAX_PATH];
	char *pVersionDetailString = NULL;
	V_ComposeFileName( pInstallPath, "version.txt", szVersionPath, sizeof( szVersionPath ) );
	if ( Sys_LoadFile( szVersionPath, (void**)&pVersionDetailString ) <= 0 )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "Cancelled Installation! DVD Image Auto-Build Process Failed! - Missing version.txt\n" );
		return;		
	}

	Sys_LoadScriptFile( szScriptPath );

	CProgress* pProgress = new CProgress;
	pProgress->Open( "Installing DVD Image...", true, false );

	int numFailed = 0;
	int numSkipped = 0;
	int numUpdated = 0;
	int version = 0;

	bool bFailed = true;
	bool bSyntaxError = false;
	bool bCancelled = false;
	while ( 1 )
	{
		pToken = Sys_GetToken( true );
		if ( !pToken || !pToken[0] )
			break;

		if ( pProgress->IsCancel() )
		{
			bCancelled = true;
			break;
		}

		if ( !stricmp( pToken, "$version" ) )
		{
			// version <num> <targetroot>
			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
			{
				bSyntaxError = true;
				break;
			}
			version = atoi( pToken );
			if ( version < 0 )
			{
				version = 0;
			}

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
			{
				bSyntaxError = true;
				break;
			}
			Sys_StripQuotesFromToken( pToken );
			V_FixSlashes( pToken );
			if ( pToken[0] == CORRECT_PATH_SEPARATOR )
			{
				// remove initial slash
				memcpy( pToken, pToken+1, strlen( pToken ) );
			}	

			char szRootPath[MAX_PATH];
			V_ComposeFileName( g_targetPath, pToken, szRootPath, sizeof( szRootPath ) );

			if ( bCleanTarget )
			{
				// delete any exisiting files at target
				DM_FILE_ATTRIBUTES	fileAttributes;
				HRESULT hr = DmGetFileAttributes( szRootPath, &fileAttributes );
				if ( hr == XBDM_NOERR )
				{
					// delete all files at valid target
					V_ComposeFileName( szRootPath, "*.*", szTempFilename, sizeof( szTempFilename ) );
					char *pArgs[3];
					pArgs[0] = "*del";
					pArgs[1] = szTempFilename;
					pArgs[2] = "/s";
					if ( !lc_del( 3, pArgs ) )
					{
						ConsoleWindowPrintf( XBX_CLR_RED, "Failed To Delete Files At '%s'.\n", szRootPath );
						break;
					}
				}
			}

			char szTargetVersionPath[MAX_PATH];
			V_ComposeFileName( szRootPath, "version.txt", szTargetVersionPath, sizeof( szTargetVersionPath ) );

			if ( !bForce )
			{
				int fileSize;
				char *pTargetVersionDetailString;
				if ( !LoadTargetFile( szTargetVersionPath, &fileSize, (void**)&pTargetVersionDetailString ) )
				{
					// expected version does not exist
					// force full install
					bForce = true;
				}
				else if ( strcmp( pVersionDetailString, pTargetVersionDetailString ) != 0 )
				{
					// different versions
					bForce = true;
				}
				Sys_Free( pTargetVersionDetailString );
			}

			if ( bForce && !FileSyncEx( szVersionPath, szTargetVersionPath, FSYNC_ALWAYS, true, false ) )
			{
				numFailed++;
				break;
			}
		}
		else if ( !stricmp( pToken, "$print" ) )
		{
			// print <string>
			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
			{
				bSyntaxError = true;
				break;
			}
			Sys_StripQuotesFromToken( pToken );
			ConsoleWindowPrintf( XBX_CLR_DEFAULT, "%s\n", pToken );
		}
		else if ( !stricmp( pToken, "$copy" ) )
		{
			// copy <source> <target>
			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
			{
				bSyntaxError = true;
				break;
			}
			Sys_StripQuotesFromToken( pToken );
			V_FixSlashes( pToken );
			if ( !V_strnicmp( pToken, "DVD\\", 4 ) )
			{
				// skip past DVD, used as a placeholder
				pToken += 4;
			}
			V_ComposeFileName( pInstallPath, pToken, arg1, sizeof( arg1 ) );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
			{
				bSyntaxError = true;
				break;
			}
			Sys_StripQuotesFromToken( pToken );
			V_FixSlashes( pToken );
			if ( pToken[0] == CORRECT_PATH_SEPARATOR )
			{
				// remove initial slash
				memcpy( pToken, pToken+1, strlen( pToken ) );
			}	
			V_ComposeFileName( g_targetPath, pToken, arg2, sizeof( arg2 ) );

			Sys_StripFilename( arg1, sourcePath, sizeof( sourcePath ) );

			h = FindFirstFile( arg1, &findData );
			if ( h != INVALID_HANDLE_VALUE )
			{
				do
				{
					if ( pProgress->IsCancel() )
					{
						bCancelled = true;
						break;
					}

					if ( !stricmp( findData.cFileName, "." ) || !stricmp( findData.cFileName, ".." ) )
					{
						continue;
					}
					if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
					{
						continue;
					}

					strcpy( sourceFilename, sourcePath );
					Sys_AddFileSeperator( sourceFilename, sizeof( sourceFilename ) );
					strcat( sourceFilename, findData.cFileName );
					Sys_NormalizePath( sourceFilename, false );

					Sys_StripPath( arg2, filename, sizeof( filename ) );
					if ( filename[0] )
					{
						// target filename is specified
						strcpy( targetFilename, arg2 );
					}
					else
					{
						// target filename is path
						strcpy( targetFilename, arg2 );
						Sys_AddFileSeperator( targetFilename, sizeof( targetFilename ) );
						strcat( targetFilename, findData.cFileName );
						Sys_NormalizePath( targetFilename, false );
					}

					ConsoleWindowPrintf( XBX_CLR_DEFAULT, "\nCopying: %s -> %s\n", sourceFilename, targetFilename );

					WIN32_FILE_ATTRIBUTE_DATA localAttributes;
					float sourceFileSize = 0;
					if ( GetFileAttributesEx( sourceFilename, GetFileExInfoStandard, &localAttributes ) )
					{
						sourceFileSize = (float)localAttributes.nFileSizeLow / (1024.0f * 1024.0f);
					}

					char statusBuff1[MAX_PATH];
					V_snprintf( statusBuff1, sizeof( statusBuff1 ), "Copying (%.2f MB) ... Please Wait", sourceFileSize );
					pProgress->SetStatus( statusBuff1, sourceFilename, targetFilename );

					int fileSyncMode = bForce ? FSYNC_ALWAYS : FSYNC_IFNEWER;
					errCode = FileSyncEx( sourceFilename, targetFilename, fileSyncMode, true, false );
					if ( errCode < 0 )
					{
						ConsoleWindowPrintf( XBX_CLR_RED, "Sync Failure!\n" );
						numFailed++;
					}
					else if ( errCode == 0 )
					{
						ConsoleWindowPrintf( XBX_CLR_BLUE, "Sync Skipped!\n" );
						numSkipped++;
					}
					else if ( errCode == 1 )
					{
						ConsoleWindowPrintf( XBX_CLR_GREEN, "Sync Completed!\n" );
						numUpdated++;
					}
				}
				while ( FindNextFile( h, &findData ) );

				FindClose( h );
			}
		}
		else if ( !stricmp( pToken, "$end" ) )
		{
			bFailed = false;
			break;
		}
		else
		{
			ConsoleWindowPrintf( XBX_CLR_RED, "Unknown token: '%s' in '%s'\n", pToken, szScriptPath );
			bSyntaxError = true;
			break;
		}
	}

	if ( bSyntaxError )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "Syntax Error in '%s'.\n", szScriptPath );
	}

	if ( bCancelled )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "Cancelled Installation!\n" );
	}
	else if ( bFailed )
	{
		ConsoleWindowPrintf( XBX_CLR_RED, "Failed Installation!\n" );
	}
	else
	{
		char *pCRLF = V_stristr( pVersionDetailString, "\r\n" );
		if ( pCRLF )
		{
			*pCRLF = '\0';
		}

		// spew stats
		ConsoleWindowPrintf( XBX_CLR_BLACK, "\n" );
		ConsoleWindowPrintf( XBX_CLR_BLACK, "Installation Completed.\n" );
		ConsoleWindowPrintf( XBX_CLR_BLACK, "-----------------------\n" );
		ConsoleWindowPrintf( XBX_CLR_BLACK, "Version: %s\n", pVersionDetailString ? pVersionDetailString : "???" );

		if ( numFailed )
		{
			ConsoleWindowPrintf( XBX_CLR_RED, "%d Failures.\n", numFailed );
		}		
		ConsoleWindowPrintf( XBX_CLR_BLACK, "%d Files In Sync.\n", numSkipped );
		ConsoleWindowPrintf( XBX_CLR_BLACK, "%d Files Updated.\n", numUpdated );
	}

	delete pProgress;
	Sys_Free( pVersionDetailString );
	Sys_FreeScriptFile();
}

//-----------------------------------------------------------------------------
//	InstallDlg_GetChanges
//
//-----------------------------------------------------------------------------
bool InstallDlg_GetChanges( HWND hWnd )
{
	g_install_bForceSync = IsDlgButtonChecked( hWnd, IDC_INSTALL_FORCESYNC ) != 0;
	g_install_bCleanTarget = IsDlgButtonChecked( hWnd, IDC_INSTALL_CLEANTARGET ) != 0;

	g_install_Selection = -1;
	for ( int i = 0; i < g_install_dvdImages.Count(); i++ )
	{
		int state = ListView_GetItemState( GetDlgItem( hWnd, IDC_INSTALL_LIST ), i, LVIS_FOCUSED|LVIS_SELECTED );
		if ( state == ( LVIS_FOCUSED|LVIS_SELECTED ) )
		{
			g_install_Selection = i;
			break;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
//	SortDVDImages
//
//-----------------------------------------------------------------------------
int SortDVDImages( const dvdimage_t *pA, const dvdimage_t *pB )
{
	char szStringA[256];
	char szStringB[256];

	V_strncpy( szStringA, pA->szString, sizeof( szStringA ) );
	V_strncpy( szStringB, pB->szString, sizeof( szStringB ) );

	// sort staging first
	char *pCommentA = V_stristr( szStringA, " (" );
	char *pCommentB = V_stristr( szStringB, " (" );
	if ( pCommentA )
	{
		*pCommentA = '\0';
	}
	if ( pCommentB )
	{
		*pCommentB = '\0';
	}
	
	return stricmp( szStringB, szStringA );
}

//-----------------------------------------------------------------------------
//	InstallDlg_Populate
//
//-----------------------------------------------------------------------------
void InstallDlg_Populate( HWND hWnd )
{
	HWND hWndListView = GetDlgItem( hWnd, IDC_INSTALL_LIST );

	ListView_DeleteAllItems( hWndListView );
	g_install_dvdImages.Purge();

	// get list of DVD images
	char szPath[MAX_PATH];
	V_ComposeFileName( g_installPath, "DVD_*", szPath, sizeof( szPath ) );
	WIN32_FIND_DATA	findData;	
	HANDLE h = FindFirstFile( szPath, &findData );
	if ( h != INVALID_HANDLE_VALUE )
	{
		do
		{
			if ( !stricmp( findData.cFileName, "." ) || !stricmp( findData.cFileName, ".." ) )
			{
				continue;
			}
			if ( !( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
			{
				// skip files
				continue;
			}

			char szInstallPath[MAX_PATH];
			V_strncpy( szInstallPath, g_installPath, sizeof( szInstallPath ) );
			V_AppendSlash( szInstallPath, sizeof( szInstallPath ) );
			V_strncat( szInstallPath, findData.cFileName, sizeof( szInstallPath ) );

			// qualify dvd dirs that are images
			char szBooterPath[MAX_PATH];
			V_ComposeFileName( szInstallPath, "default.xex", szBooterPath, sizeof( szBooterPath ) );
			if ( !Sys_Exists( szBooterPath ) )
			{
				continue;
			}
			char szVersionPath[MAX_PATH];
			V_ComposeFileName( szInstallPath, "version.txt", szVersionPath, sizeof( szVersionPath ) );
			char *pVersionDetailString = NULL;
			if ( Sys_LoadFile( szVersionPath, (void**)&pVersionDetailString ) == -1 )
			{
				continue;
			}

			char *pCRLF = V_stristr( pVersionDetailString, "\r\n" );
			if ( pCRLF )
			{
				*pCRLF = '\0';
			}

			dvdimage_t image;

			int year = 0;
			int month = 0;
			int day = 0;
			int hour = 0;
			int minute = 0;
			char timeOfDay[256];
			sscanf( findData.cFileName, "DVD_%d_%d_%d_%d_%d_%s", &year, &month, &day, &hour, &minute, timeOfDay );

			const char *pInfoString = timeOfDay;
			if ( !V_strnicmp( timeOfDay, "PM", 2 ) )
			{
				if ( hour != 12 )
				{
					// 24 hour time for correct sorting
					hour += 12;
				}
				pInfoString += 2;
			}
			else if ( !V_strnicmp( timeOfDay, "AM", 2 ) )
			{
				if ( hour == 12 )
				{
					// 24 hour time for correct sorting
					hour = 0;
				}
				pInfoString += 2;
			}

			char szCommentBuff[128];
			if ( pInfoString[0] == '_' )
			{
				// optional info after AM/PM
				pInfoString++;
				V_snprintf( szCommentBuff, sizeof( szCommentBuff ), " (%s)", pInfoString );
			}
			else
			{
				szCommentBuff[0] = '\0';
			}
			
			V_snprintf( image.szString, sizeof( image.szString ), "%2.2d/%2.2d/%4.4d %2.2d:%2.2d%s", month, day, year, hour, minute, szCommentBuff );

			image.installPath = szInstallPath;
			image.versionDetailString = pVersionDetailString;
			g_install_dvdImages.AddToTail( image );
			Sys_Free( pVersionDetailString );
		}
		while ( FindNextFile( h, &findData ) );
		FindClose( h );
	}

	// current image will be at head
	g_install_dvdImages.Sort( SortDVDImages );

	for ( int i = 0; i < g_install_dvdImages.Count(); i++ )
	{
		// setup and insert at end of list
		LVITEM	lvi;
		memset( &lvi, 0, sizeof( lvi ) );
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.state = 0; 
		lvi.stateMask = 0;
		lvi.pszText = LPSTR_TEXTCALLBACK;  
		lvi.lParam = (LPARAM)i;

		ListView_InsertItem( hWndListView, &lvi );
	}

	if ( g_install_dvdImages.Count() )
	{
		ListView_SetItemState( hWndListView, 0, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED );
	}
}

//-----------------------------------------------------------------------------
//	InstallDlg_Setup
//
//-----------------------------------------------------------------------------
void InstallDlg_Setup( HWND hWnd )
{
	g_install_Selection = -1;
	g_install_bForceSync = false;
	g_install_bCleanTarget = false;

	HWND hWndListView = GetDlgItem( hWnd, IDC_INSTALL_LIST );

	// initialize columns
	LVCOLUMN lvc; 
	memset( &lvc, 0, sizeof( lvc ) );
	lvc.mask = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM; 
	lvc.iSubItem = 0;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 200;
	lvc.pszText = ( LPSTR )"Date Built:";
	ListView_InsertColumn( hWndListView, 0, &lvc );
	lvc.iSubItem = 0;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = 500;
	lvc.pszText = ( LPSTR )"Perforce Changelist:";
	ListView_InsertColumn( hWndListView, 1, &lvc );

	ListView_SetBkColor( hWndListView, g_backgroundColor );
	ListView_SetTextBkColor( hWndListView, g_backgroundColor );

	DWORD style = LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES;
	ListView_SetExtendedListViewStyleEx( hWndListView, style, style );

	InstallDlg_Populate( hWnd );
}

//-----------------------------------------------------------------------------
//	InstallDlg_Proc
//
//-----------------------------------------------------------------------------
BOOL CALLBACK InstallDlg_Proc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{ 
	switch ( message ) 
	{ 
	case WM_INITDIALOG:
		InstallDlg_Setup( hWnd );
		return ( TRUE );

	case WM_NOTIFY:
		switch ( ( ( LPNMHDR )lParam )->code )
		{
		case LVN_GETDISPINFO:
			NMLVDISPINFO* plvdi;
			plvdi = (NMLVDISPINFO*)lParam;
			int item = (int)plvdi->item.lParam;
			switch ( plvdi->item.iSubItem )
			{
			case 0:
				plvdi->item.pszText = g_install_dvdImages[item].szString;
				return TRUE;
			case 1:
				plvdi->item.pszText = (LPSTR)g_install_dvdImages[item].versionDetailString.String();
				return TRUE;
			}
		}
		break;

	case WM_COMMAND: 
		switch ( LOWORD( wParam ) ) 
		{
		case IDC_OK: 
			InstallDlg_GetChanges( hWnd );
			EndDialog( hWnd, wParam );
			return ( TRUE ); 

		case IDC_INSTALL_REFRESH:
			InstallDlg_Populate( hWnd );
			return TRUE;

		case IDCANCEL:
		case IDC_CANCEL: 
			EndDialog( hWnd, wParam );
			return ( TRUE ); 
		}
		break; 
	}
	return ( FALSE ); 
} 

//-----------------------------------------------------------------------------
//	InstallDlg_Open
//
//-----------------------------------------------------------------------------
void InstallDlg_Open( void )
{
	int result = DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_INSTALL ), g_hDlgMain, ( DLGPROC )InstallDlg_Proc );
	if ( LOWORD( result ) == IDC_OK && g_install_Selection != -1 )
	{
		InstallDlg_InstallImage( 
			g_install_dvdImages[g_install_Selection].installPath.String(),
			g_install_bForceSync,
			g_install_bCleanTarget );
	}

	g_install_dvdImages.Purge();
}
