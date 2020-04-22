//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <vgui/isystem.h>
#include "min_footprint_files.h"
#include "filesystem_tools.h"
#include "KeyValues.h"
#include "sdklauncher_main.h"
#include "ModConfigsHelper.h"
#include "vgui_controls/Frame.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui/iinput.h>
#include <vgui/ivgui.h>
#include "SourceAppInfo.h"

extern void OpenLocalizedURL( const char *lpszLocalName );
extern FSReturnCode_t LoadGameInfoFile( const char *pDirectoryName, 
										KeyValues *&pMainFile, 
										KeyValues *&pFileSystemInfo, 
										KeyValues *&pSearchPaths );
using namespace vgui;

#define SHOW_MIGRATION_MARKER             "show_migration_marker.txt"
#define SHOW_DEPRECATED_APP_ID_MARKER     "show_deprecatedappid_marker.txt"

#define MIN_FOOTPRINT_VERSION_FILENAME	  "min_footprint_version.txt"
#define MIN_FOOTPRINT_FILES_FILENAME	  "min_footprint_file_list.txt"


#define WRITABLE_PREFIX		"[make_writable]"
#define ALWAYS_COPY_PREFIX	"[always_copy]"


class CTempDirName
{
public:
	char m_SrcDirName[MAX_PATH];
	char m_DestDirName[MAX_PATH];
};


#define MF_MAKE_WRITABLE	0x0001
#define MF_ALWAYS_COPY		0x0002


class CMinFootprintFilename
{
public:
	char m_SrcFilename[MAX_PATH];
	char m_DestFilename[MAX_PATH];
	int m_Flags;	// Combination of MF_ flags.
};


void SafeCopy( const char *pSrcFilename, const char *pDestFilename )
{
	FileHandle_t fpSrc = g_pFullFileSystem->Open( pSrcFilename, "rb" );
	if ( fpSrc )
	{
		FILE *fpDest = fopen( pDestFilename, "wb" );
		if ( fpDest )
		{
			while ( 1 )
			{
				char tempData[4096];
				int nBytesRead = g_pFullFileSystem->Read( tempData, sizeof( tempData ), fpSrc );
				if ( nBytesRead )
					fwrite( tempData, 1, nBytesRead, fpDest );

				if ( nBytesRead < sizeof( tempData ) )
					break;
			}

			fclose( fpDest );
		}
		else
		{
			Warning( "SafeCopy: can't open %s for writing.", pDestFilename );
		}


		g_pFullFileSystem->Close( fpSrc );
	}
	else
	{
		Warning( "SafeCopy: can't open %s for reading.", pSrcFilename );
	}
}

class CMinFootprintFilesPanel : public Frame
{
public:
	typedef Frame BaseClass;

	CMinFootprintFilesPanel( Panel *parent, const char *panelName )
		: BaseClass( parent, panelName )
	{
		m_pProgressBar = new ProgressBar( this, "CopyProgressBar" );
		m_pProgressBar->SetProgress( 0 );

		LoadControlSettings( "MinFootprintFilesPanel.res");
		SetSizeable( false );
		SetMenuButtonVisible( false );
		SetMenuButtonResponsive( false );
		SetCloseButtonVisible( false );
		
		// Take the focus.
		m_PrevAppFocusPanel = input()->GetAppModalSurface();
		input()->SetAppModalSurface( GetVPanel() );
	}

	~CMinFootprintFilesPanel()
	{
		input()->SetAppModalSurface( m_PrevAppFocusPanel );
	}

	void StartDumpingFiles()
	{
		m_pProgressBar->SetProgress( 0 );
		m_iCurCopyFile = 0;
		ivgui()->AddTickSignal( GetVPanel() );

		MoveToCenterOfScreen();
		Activate();
	}

	void OnTick()
	{
		int nCopied = 0;
		while ( nCopied < 10 )
		{
			if ( m_iCurCopyFile >= m_Filenames.Count() )
			{
				// Done! Write the output version and disappear.
				OnFinished();
				return;
			}

			CMinFootprintFilename *pFilename = &m_Filenames[m_iCurCopyFile++];

			// If the dest file doesn't exist or it's read-only, copy our file over it.
			if ( (pFilename->m_Flags & MF_ALWAYS_COPY) || (_access( pFilename->m_DestFilename, 2 ) != 0) )
			{
				// Get rid of the old version of the file.
				_chmod( pFilename->m_DestFilename, _S_IWRITE | _S_IREAD ); // Read-only.
				_unlink( pFilename->m_DestFilename );

				// Copy the new version in.
				SafeCopy( pFilename->m_SrcFilename, pFilename->m_DestFilename );
				
				// Set its access permissions.
				if ( pFilename->m_Flags & MF_MAKE_WRITABLE )
					_chmod( pFilename->m_DestFilename, _S_IREAD | _S_IWRITE );
				else
					_chmod( pFilename->m_DestFilename, _S_IREAD ); // Read-only.
			}

			++nCopied;
		}

		m_pProgressBar->SetProgress( (float)m_iCurCopyFile / m_Filenames.Count() );
	}

	void OnFinished()
	{
		m_pProgressBar->SetProgress( 1 );

		//
		// Write out that we are at the latest version.
		//
		KeyValues *pVersionKV = new KeyValues( MIN_FOOTPRINT_VERSION_FILENAME );
		pVersionKV->SetInt( "Version", m_iOutputVersionNumber );
		pVersionKV->SaveToFile( g_pFullFileSystem, MIN_FOOTPRINT_VERSION_FILENAME );
		pVersionKV->deleteThis();

		// Don't tick any more.
		ivgui()->RemoveTickSignal( GetVPanel() );

		// Remove our panel.
		PostMessage(this, new KeyValues("Close"));
		
		ShowDeprecatedAppIDNotice();

		ShowContentMigrationNotice();
	}

	void ShowDeprecatedAppIDNotice()
	{
		// Try to open the file that indicates that we've already been through this check
		FileHandle_t fp = g_pFullFileSystem->Open( SHOW_DEPRECATED_APP_ID_MARKER, "rb" );

		// If the file exists then we've run the check already, don't do anything.
		if ( fp )
		{
			g_pFullFileSystem->Close( fp );
		}
		// We have not checked for outdated mods yet...
		else
		{
			// Write file that ensures we never perform this check again
			fp = g_pFullFileSystem->Open( SHOW_DEPRECATED_APP_ID_MARKER, "wb" );
			g_pFullFileSystem->Close( fp );

			//
			// Look for all of the mods under 'SourceMods', check if the SteamAppId is out of date, and warn the user if 
			// this is the case
			//
			ModConfigsHelper mch;
			const CUtlVector<char *> &modDirs = mch.getModDirsVector();
			vgui::MessageBox *alert = NULL;
			char szProblemMods[5*MAX_PATH];
			bool bProblemModExists = false;
			
			// Set up the error string
			Q_strncpy(szProblemMods, "The SteamAppId values for the following mods should be changed from 220 to 215:\r\n\r\n", 5*MAX_PATH);

			// Iterate through the mods and check which ones have outdated SteamAppIds
			for ( int i = 0; i < modDirs.Count(); i++ )
			{
				// Construct full path to mod directory
				char szDirPath[MAX_PATH];
				char szGameConfigPath[MAX_PATH];
				Q_strncpy( szDirPath, mch.getSourceModBaseDir(), MAX_PATH );
				Q_strncat( szDirPath, "\\", MAX_PATH , COPY_ALL_CHARACTERS );
				Q_strncat( szDirPath, modDirs[i], MAX_PATH , COPY_ALL_CHARACTERS );

				// Check for the existence of gameinfo.txt
				Q_strncpy( szGameConfigPath, szDirPath, MAX_PATH );
				Q_strncat( szGameConfigPath, "\\gameinfo.txt", MAX_PATH , COPY_ALL_CHARACTERS );
				FileHandle_t fpGameInfo = g_pFullFileSystem->Open( szGameConfigPath, "rb" );

				// GameInfo.txt exists so let's inspect it...
				if ( fpGameInfo )
				{
					// Close the file handle
					g_pFullFileSystem->Close( fpGameInfo );
				
					// Load up the "gameinfo.txt"
					KeyValues *pMainFile, *pFileSystemInfo, *pSearchPaths;
					LoadGameInfoFile( szDirPath, pMainFile, pFileSystemInfo, pSearchPaths );
					//!! bug? check return code of loadgameinfofile??
					if (NULL != pFileSystemInfo)
					{
						const int iAppId = pFileSystemInfo->GetInt( "SteamAppId", -1 );

						// This is the one that needs replacing add this mod to the list of suspect mods
                  if ( GetAppSteamAppId( k_App_HL2 ) == iAppId)
						{
							bProblemModExists = true;
							Q_strncat( szProblemMods, modDirs[i], MAX_PATH , COPY_ALL_CHARACTERS );
							Q_strncat( szProblemMods, "\r\n", MAX_PATH , COPY_ALL_CHARACTERS );
						}
					}
				}
			}
			
			// If necessary, pop up a message box informing the user that 
			if (bProblemModExists)
			{
				// Amend the warning message text
				Q_strncat( szProblemMods, "\r\nIf you did not author any of the above mods you can ignore this message.\r\nIf you did author any of the above mods you should change the SteamAppId\nvalue in the gameinfo.txt file for each mod listed above.\r\n", 5*MAX_PATH , COPY_ALL_CHARACTERS );
			
				// Pop up a message box 
				alert = new vgui::MessageBox( "Warning", szProblemMods );
				alert->SetOKButtonText("Close");
				alert->DoModal();
			}
		}
	}

	void ShowContentMigrationNotice()
	{
		// If we've shown it already, don't do anything.
		FileHandle_t fp = g_pFullFileSystem->Open( SHOW_MIGRATION_MARKER, "rb" );
		if ( fp )
		{
			g_pFullFileSystem->Close( fp );
			return;
		}

		// Remember that we showed it.
		fp = g_pFullFileSystem->Open( SHOW_MIGRATION_MARKER, "wb" );
		g_pFullFileSystem->Close( fp );

		OpenLocalizedURL( "URL_Content_Migration_Notice" );
	}

public:
	CUtlVector<CMinFootprintFilename> m_Filenames;
	int m_iOutputVersionNumber;


private:	
	vgui::VPANEL m_PrevAppFocusPanel;
	ProgressBar *m_pProgressBar;
	int m_iCurCopyFile;
};



void AddMinFootprintFile( CUtlVector<CMinFootprintFilename> &filenames, const char *pSrcFilename, const char *pDestFilename, int flags )
{
	// Just copy this one file.
	CMinFootprintFilename *pOutputFilename = &filenames[filenames.AddToTail()];
	Q_strncpy( pOutputFilename->m_SrcFilename, pSrcFilename, sizeof( pOutputFilename->m_SrcFilename ) );
	Q_strncpy( pOutputFilename->m_DestFilename, pDestFilename, sizeof( pOutputFilename->m_DestFilename ) );
	pOutputFilename->m_Flags = flags;
}


void GetMinFootprintFiles_R( CUtlVector<CMinFootprintFilename> &filenames, const char *pSrcDirName, const char *pDestDirName )
{
	// pDirName\*.*
	char wildcard[MAX_PATH];
	Q_strncpy( wildcard, pSrcDirName, sizeof( wildcard ) );
	Q_AppendSlash( wildcard, sizeof( wildcard ) );
	Q_strncat( wildcard, "*.*", sizeof( wildcard ), COPY_ALL_CHARACTERS );

	CUtlVector<CTempDirName> subDirs;

	// Make sure the dest directory exists for when we're copying files into it.
	CreateDirectory( pDestDirName, NULL );

	// Look at all the files.
	FileFindHandle_t findHandle;
	const char *pFilename = g_pFullFileSystem->FindFirstEx( wildcard, SDKLAUNCHER_MAIN_PATH_ID, &findHandle );
	while ( pFilename )
	{
		if ( Q_stricmp( pFilename, "." ) != 0 && Q_stricmp( pFilename, ".." ) != 0 )
		{
			char fullSrcFilename[MAX_PATH], fullDestFilename[MAX_PATH];
			Q_snprintf( fullSrcFilename, sizeof( fullSrcFilename ), "%s%c%s", pSrcDirName, CORRECT_PATH_SEPARATOR, pFilename );
			Q_snprintf( fullDestFilename, sizeof( fullDestFilename ), "%s%c%s", pDestDirName, CORRECT_PATH_SEPARATOR, pFilename );

			if ( g_pFullFileSystem->FindIsDirectory( findHandle ) )
			{
				CTempDirName *pOut = &subDirs[subDirs.AddToTail()];
				Q_strncpy( pOut->m_SrcDirName, fullSrcFilename, sizeof( pOut->m_SrcDirName ) );
				Q_strncpy( pOut->m_DestDirName, fullDestFilename, sizeof( pOut->m_DestDirName ) );
			}
			else
			{
				AddMinFootprintFile( filenames, fullSrcFilename, fullDestFilename, 0 );
			}
		}
		
		pFilename = g_pFullFileSystem->FindNext( findHandle );
	}
	g_pFullFileSystem->FindClose( findHandle );
	
	// Recurse.
	for ( int i=0; i < subDirs.Count(); i++ )
	{
		GetMinFootprintFiles_R( filenames, subDirs[i].m_SrcDirName, subDirs[i].m_DestDirName );
	}
}


void DumpMinFootprintFiles( bool bForceRefresh )
{
	if ( !g_pFullFileSystem->IsSteam() )
		return;

	// What version are we at now?
	int curVersion = 0;
	if ( !bForceRefresh )
	{
		KeyValues *kv = new KeyValues( "" );
		if ( kv->LoadFromFile( g_pFullFileSystem, MIN_FOOTPRINT_VERSION_FILENAME ) )
		{
			curVersion = kv->GetInt( "Version", 0 );
		}
		kv->deleteThis();
	}


	// What is the current version?
	int latestVersion = 0;
	KeyValues *kv = new KeyValues( "" );
	
	bool bValidFile = true;
	if ( !kv->LoadFromFile( g_pFullFileSystem, MIN_FOOTPRINT_FILES_FILENAME ) )
		bValidFile = false;

	KeyValues *pFileList = NULL;
	if ( bValidFile && (pFileList = kv->FindKey( "files" )) == NULL )
		bValidFile = false;

	if ( !bValidFile )		
	{
		VGUIMessageBox( (vgui::Frame *) g_pMainFrame, "#Warning", "#CantLoadMinFootprintFiles" );
		kv->deleteThis();
		return;
	}

	latestVersion = kv->GetInt( "Version", 0 );

	bool bVersionChanged = ( curVersion != latestVersion );

	CMinFootprintFilesPanel *pDisplay = new CMinFootprintFilesPanel( (vgui::Frame *) g_pMainFrame, "MinFootprintFilesPanel" );
	pDisplay->m_iOutputVersionNumber = latestVersion;
	
	// Our version is out of date, let's copy out all the min footprint files.
	// NOTE: copy files that are set to always_copy whether the version changed or not.
	for ( KeyValues *pCur=pFileList->GetFirstSubKey(); pCur; pCur=pCur->GetNextKey() )
	{
		if ( ( Q_stricmp( pCur->GetName(), "mapping" ) == 0 ) && bVersionChanged )
		{
			const char *pSrcMapping  = pCur->GetString( "src" );
			const char *pDestMapping = pCur->GetString( "dest" );

			char destDir[MAX_PATH], destDirTemp[MAX_PATH];
			Q_snprintf( destDirTemp, sizeof( destDirTemp ), "%s%c%s", GetSDKLauncherBaseDirectory(), CORRECT_PATH_SEPARATOR, pDestMapping );
			Q_MakeAbsolutePath( destDir, sizeof( destDir ), destDirTemp );

			GetMinFootprintFiles_R( pDisplay->m_Filenames, pSrcMapping, destDir );
		}
		else if ( Q_stricmp( pCur->GetName(), "single_file" ) == 0 )
		{
			const char *pDestMapping = pCur->GetString();
			
			// If the filename is preceded by the right prefix, then make it writable.
			int flags = 0;
			if ( Q_stristr( pDestMapping, WRITABLE_PREFIX ) == pDestMapping )
			{
				flags |= MF_MAKE_WRITABLE;
				pDestMapping += strlen( WRITABLE_PREFIX );
			}

			// Check for [always_copy]
			if ( Q_stristr( pDestMapping, ALWAYS_COPY_PREFIX ) == pDestMapping )
			{
				flags |= MF_ALWAYS_COPY;
				pDestMapping += strlen( ALWAYS_COPY_PREFIX );
			}

			char destFile[MAX_PATH], destFileTemp[MAX_PATH];
			Q_snprintf( destFileTemp, sizeof( destFileTemp ), "%s%c%s", GetSDKLauncherBaseDirectory(), CORRECT_PATH_SEPARATOR, pDestMapping );
			Q_MakeAbsolutePath( destFile, sizeof( destFile ), destFileTemp );

			if ( bVersionChanged || ( flags & MF_ALWAYS_COPY ) )
			{
				AddMinFootprintFile( pDisplay->m_Filenames, pDestMapping, destFile, flags );
			}
		}
		else if ( ( Q_stricmp( pCur->GetName(), "create_directory" ) == 0 ) && bVersionChanged )
		{
			// Create an empty directory?
			char destFile[MAX_PATH], destFileTemp[MAX_PATH];
			Q_snprintf( destFileTemp, sizeof( destFileTemp ), "%s%c%s", GetSDKLauncherBaseDirectory(), CORRECT_PATH_SEPARATOR, pCur->GetString() );
			Q_MakeAbsolutePath( destFile, sizeof( destFile ), destFileTemp );

			CreateDirectory( destFile, NULL );
		}
	}

	pDisplay->StartDumpingFiles();		
}

