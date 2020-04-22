//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "ModWizard_Intro.h"
#include "ModWizard_TemplateOptions.h"
#include "ModWizard_CopyFiles.h"
#include "ModWizard_Finished.h"
#include "CreateModWizard.h"
#include "configs.h"
#include "filesystem_tools.h"
#include "sdklauncher_main.h"
#include "SDKLauncherDialog.h"
#include <vgui_controls/WizardPanel.h>
#include <vgui/ivgui.h>
#include "SourceAppInfo.h"

using namespace vgui;

extern bool IsGameSubscribed( int nSteamAppId );
extern char g_engineDir[50];

class CTempDirectory
{
public:
	char m_FullName[MAX_PATH];
	char m_FullOutName[MAX_PATH];
};


CModWizardSubPanel_CopyFiles::CModWizardSubPanel_CopyFiles( Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
	m_pLabel = new Label( this, "MessageLabel", "" );
	m_pFinishedLabel = new Label( this, "FinishedLabel", "" );
	m_pProgressBar = new ProgressBar( this, "CopyProgressBar" );
	m_pProgressBar->SetProgress( 0 );

	LoadControlSettings( "ModWizardSubPanel_CopyFiles.res");
}

void CModWizardSubPanel_CopyFiles::OnDisplayAsNext()
{
	GetWizardPanel()->SetTitle( "#ModWizard_CopyFiles_Title", true );

	m_iCurCopyFile = -1;

	// All buttons are disabled until we're done.
	GetWizardPanel()->SetCancelButtonEnabled( false );
	GetWizardPanel()->SetFinishButtonEnabled( false );
	GetWizardPanel()->SetPrevButtonEnabled( false );
	GetWizardPanel()->SetNextButtonEnabled( false );

	ivgui()->AddTickSignal( GetVPanel() );
}

WizardSubPanel *CModWizardSubPanel_CopyFiles::GetNextSubPanel()
{
	return dynamic_cast<WizardSubPanel *>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_Finished"));
}

void CModWizardSubPanel_CopyFiles::GetReady( const char *pOutputDirName, const char *pOutputModGamedirName, const char *modName )
{
	Q_strncpy( m_OutputDirName, pOutputDirName, sizeof( m_OutputDirName ) );
	Q_strncpy( m_OutModGamedirName, pOutputModGamedirName, sizeof( m_OutModGamedirName ) );
	Q_strncpy( m_ModName, modName, sizeof( m_ModName ) );
}


// Creates the directory structure and builds a list of files to copy.
bool CModWizardSubPanel_CopyFiles::BuildCopyFiles_R( const char *pSourceDir, const char *pMask, const char *pOutputDirName )
{
	char mask[512];
	Q_snprintf( mask, sizeof( mask ), "%s%c%s", pSourceDir, CORRECT_PATH_SEPARATOR, pMask );

	// Steam only allows 5 Find handles open at one time, and
	// we actually will hit the limit if we recurse into directories while looping through the files.
	CUtlVector<CTempDirectory> directories;

	FileFindHandle_t findHandle;
	const char *pFilename = g_pFullFileSystem->FindFirstEx( mask, 0, &findHandle );
	while ( pFilename )
	{
		// Skip the "." and ".." directories.
		if ( pFilename[0] != '.' )
		{
			char fullName[MAX_PATH];
			Q_snprintf( fullName, sizeof( fullName ), "%s%c%s", pSourceDir, CORRECT_PATH_SEPARATOR, pFilename );

			char fullOutName[MAX_PATH];
			Q_snprintf( fullOutName, sizeof( fullOutName ), "%s%c%s", pOutputDirName, CORRECT_PATH_SEPARATOR, pFilename );
			
			// We were doing this for Linux but disabled it.
			//Q_strlower( fullOutName );

			// Recurse into other directories.
			if ( g_pFullFileSystem->FindIsDirectory( findHandle ) )
			{
				CTempDirectory *pBlah = &directories[directories.AddToTail()];
				Q_strncpy( pBlah->m_FullName, fullName, sizeof( pBlah->m_FullName ) );
				Q_strncpy( pBlah->m_FullOutName, fullOutName, sizeof( pBlah->m_FullOutName ) );
			}
			else
			{
				// Don't copy .dsp files.
				const char *pIgnoreExtension = ".dsp";

				char ext[512];
				Q_StrRight( pFilename, strlen(pIgnoreExtension), ext, sizeof( ext ) );
				if ( Q_stricmp( ext, pIgnoreExtension ) != 0 )
				{
					CFileCopyInfo info( fullName, fullOutName );
					m_FileCopyInfos.AddToTail( info );
				}
			}
		}

		pFilename = g_pFullFileSystem->FindNext( findHandle );
	}
	
	g_pFullFileSystem->FindClose( findHandle );


	// See the definition of directories for why we do this.
	for ( int i=0; i < directories.Count(); i++ )
	{
		// Make sure the directory exists.
		CTempDirectory *pDir = &directories[i];
		if ( !CreateFullDirectory( pDir->m_FullOutName ) )
		{
			VGUIMessageBox( this, "Error", "Can't create directory: %s", pDir->m_FullOutName );
			return false;
		}

		if ( !BuildCopyFiles_R( pDir->m_FullName, pMask, pDir->m_FullOutName ) )
			return false;
	}

	return true;
}



bool CModWizardSubPanel_CopyFiles::BuildCopyFilesForMappings( char **pMappings, int nMappings )
{
	for ( int iDir=0; iDir < nMappings; iDir+=3 )
	{
		if ( !BuildCopyFiles_R( pMappings[iDir+0], pMappings[iDir+1], pMappings[iDir+2] ) )
		{
			vgui::ivgui()->RemoveTickSignal( GetVPanel() );
			GetWizardPanel()->SetCancelButtonEnabled( true );
			return false;
		}
	}
	return true;
}


bool CModWizardSubPanel_CopyFiles_Source2006::BuildCopyFilesForMod_HL2()
{
	bool retVal = false;
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s%s", m_OutputDirName, "src" );

	char outputUIDir[MAX_PATH], outputResourceDir[MAX_PATH], outputScriptsDir[MAX_PATH];
	Q_snprintf( outputUIDir, sizeof( outputUIDir ), "%sResource%cUI", m_OutModGamedirName, CORRECT_PATH_SEPARATOR );
	Q_snprintf( outputResourceDir, sizeof( outputResourceDir ), "%sresource", m_OutModGamedirName );
	Q_snprintf( outputScriptsDir, sizeof( outputScriptsDir ), "%sscripts", m_OutModGamedirName );

	// Also, create the mapsrc/modelsrc/materialsrc directories now because they may not be there yet.
	CreateSubdirectory( m_OutputDirName, "modelsrc" );
	CreateSubdirectory( m_OutputDirName, "materialsrc" );
	CreateSubdirectory( m_OutputDirName, "mapsrc" );

	// These go in c:\mymod
	char *sources_mappings[] =
	{
		"src_mod\\ep1", "*.*", outputSrcDirName
	};

	char outputGamedirNameNoSlash[MAX_PATH];
	Q_strncpy( outputGamedirNameNoSlash, m_OutModGamedirName, sizeof( outputGamedirNameNoSlash ) );
	if ( strlen( outputGamedirNameNoSlash ) > 0 )
		outputGamedirNameNoSlash[strlen(outputGamedirNameNoSlash)-1] = 0;
	
	// These go in c:\steam\steamapps\sourcemods\modname
	char *content_mappings[] = 
	{ 
		"game_content\\half-life 2\\hl2","lights.rad",	outputGamedirNameNoSlash,
		"game_content\\half-life 2\\hl2","detail.vbsp",	outputGamedirNameNoSlash,
		"hl2\\scripts",		"*.*",						outputScriptsDir,
		"hl2\\resource\\ui","*.*",						outputUIDir,
		"hl2\\resource",	"clientscheme.res",			outputResourceDir,
		"hl2\\resource",	"combinepanelscheme.res",	outputResourceDir,
		"hl2\\resource",	"gameevents.res",			outputResourceDir,
		"hl2\\resource",	"gamemenu.res",				outputResourceDir,
		"hl2\\resource",	"hl2_*.txt",				outputResourceDir,
		"hl2\\resource",	"serverevents.res",			outputResourceDir,
		"hl2\\resource",	"sourcescheme.res",			outputResourceDir,
	};

	// Copy gameinfo.txt
	const char *replacements[] = 
	{
		"%modname%", m_ModName
	};
	
	retVal = CopyWithReplacements("CreateModFiles\\hl2\\gameinfo_ep1.txt", replacements, ARRAYSIZE( replacements ), "%s%s", m_OutModGamedirName, "gameinfo.txt" );
	
	if ( retVal &&
	     BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) ) &&
		 BuildCopyFilesForMappings( content_mappings, ARRAYSIZE( content_mappings ) ) )
	{
		retVal = true;
	}
	else
	{
		retVal = false;
	}
	
	return retVal;
}


bool CModWizardSubPanel_CopyFiles_Source2009::BuildCopyFilesForMod_HL2()
{
	bool retVal = false;
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s%s", m_OutputDirName, "src" );

	char outputUIDir[MAX_PATH], outputResourceDir[MAX_PATH], outputScriptsDir[MAX_PATH], outputCFGDir[MAX_PATH];
	Q_snprintf( outputUIDir, sizeof( outputUIDir ), "%sResource%cUI", m_OutModGamedirName, CORRECT_PATH_SEPARATOR );
	Q_snprintf( outputResourceDir, sizeof( outputResourceDir ), "%sresource", m_OutModGamedirName );
	Q_snprintf( outputScriptsDir, sizeof( outputScriptsDir ), "%sscripts", m_OutModGamedirName );
	Q_snprintf( outputCFGDir, sizeof( outputCFGDir ), "%scfg", m_OutModGamedirName );

	// Also, create the mapsrc/modelsrc/materialsrc directories now because they may not be there yet.
	CreateSubdirectory( m_OutputDirName, "modelsrc" );
	CreateSubdirectory( m_OutputDirName, "materialsrc" );
	CreateSubdirectory( m_OutputDirName, "mapsrc" );

	// These go in c:\mymod
	char *sources_mappings[] =
	{
		"src_mod\\source2009", "*.*", outputSrcDirName
	};

	char outputGamedirNameNoSlash[MAX_PATH];
	Q_strncpy( outputGamedirNameNoSlash, m_OutModGamedirName, sizeof( outputGamedirNameNoSlash ) );
	if ( strlen( outputGamedirNameNoSlash ) > 0 )
		outputGamedirNameNoSlash[strlen(outputGamedirNameNoSlash)-1] = 0;

	// These go in c:\steam\steamapps\sourcemods\modname
	char *content_mappings[] = 
	{ 
		"game_content\\half-life 2\\hl2","lights.rad",										outputGamedirNameNoSlash,
		"game_content\\half-life 2\\hl2","detail.vbsp",										outputGamedirNameNoSlash,
		"CreateModFiles\\source2009\\singleplayer\\scripts",		"*.*",						outputScriptsDir,
		"CreateModFiles\\source2009\\singleplayer\\cfg",			"*.*",						outputCFGDir,
		"CreateModFiles\\source2009\\singleplayer\\resource",	"clientscheme.res",			outputResourceDir,
		"CreateModFiles\\source2009\\singleplayer\\resource",	"HL2EP2.ttf",				outputResourceDir,
	};

	// Copy gameinfo.txt
	const char *replacements[] = 
	{
		"%modname%", m_ModName
	};

	retVal = CopyWithReplacements("CreateModFiles\\source2009\\gameinfo_sp.txt", replacements, ARRAYSIZE( replacements ), "%s%s", m_OutModGamedirName, "gameinfo.txt" );

	if ( retVal &&
		BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) ) &&
		BuildCopyFilesForMappings( content_mappings, ARRAYSIZE( content_mappings ) ) )
	{
		retVal = true;
	}
	else
	{
		retVal = false;
	}

	return retVal;
}


bool CModWizardSubPanel_CopyFiles_Source2007::BuildCopyFilesForMod_HL2()
{
	bool retVal = false;
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s%s", m_OutputDirName, "src" );

	char outputUIDir[MAX_PATH], outputResourceDir[MAX_PATH], outputScriptsDir[MAX_PATH], outputCFGDir[MAX_PATH];
	Q_snprintf( outputUIDir, sizeof( outputUIDir ), "%sResource%cUI", m_OutModGamedirName, CORRECT_PATH_SEPARATOR );
	Q_snprintf( outputResourceDir, sizeof( outputResourceDir ), "%sresource", m_OutModGamedirName );
	Q_snprintf( outputScriptsDir, sizeof( outputScriptsDir ), "%sscripts", m_OutModGamedirName );
	Q_snprintf( outputCFGDir, sizeof( outputCFGDir ), "%scfg", m_OutModGamedirName );

	// Also, create the mapsrc/modelsrc/materialsrc directories now because they may not be there yet.
	CreateSubdirectory( m_OutputDirName, "modelsrc" );
	CreateSubdirectory( m_OutputDirName, "materialsrc" );
	CreateSubdirectory( m_OutputDirName, "mapsrc" );

	// These go in c:\mymod
	char *sources_mappings[] =
	{
		"src_mod\\orangebox", "*.*", outputSrcDirName
	};

	char outputGamedirNameNoSlash[MAX_PATH];
	Q_strncpy( outputGamedirNameNoSlash, m_OutModGamedirName, sizeof( outputGamedirNameNoSlash ) );
	if ( strlen( outputGamedirNameNoSlash ) > 0 )
		outputGamedirNameNoSlash[strlen(outputGamedirNameNoSlash)-1] = 0;

	// These go in c:\steam\steamapps\sourcemods\modname
	char *content_mappings[] = 
	{ 
		"game_content\\half-life 2\\hl2","lights.rad",										outputGamedirNameNoSlash,
		"game_content\\half-life 2\\hl2","detail.vbsp",										outputGamedirNameNoSlash,
		"CreateModFiles\\orangebox\\singleplayer\\scripts",		"*.*",						outputScriptsDir,
		"CreateModFiles\\orangebox\\singleplayer\\cfg",			"*.*",						outputCFGDir,
		"CreateModFiles\\orangebox\\singleplayer\\resource",	"clientscheme.res",			outputResourceDir,
		"CreateModFiles\\orangebox\\singleplayer\\resource",	"HL2EP2.ttf",				outputResourceDir,
	};

	// Copy gameinfo.txt
	const char *replacements[] = 
	{
		"%modname%", m_ModName
	};

	retVal = CopyWithReplacements("CreateModFiles\\orangebox\\gameinfo_sp.txt", replacements, ARRAYSIZE( replacements ), "%s%s", m_OutModGamedirName, "gameinfo.txt" );

	if ( retVal &&
		BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) ) &&
		BuildCopyFilesForMappings( content_mappings, ARRAYSIZE( content_mappings ) ) )
	{
		retVal = true;
	}
	else
	{
		retVal = false;
	}

	return retVal;
}


bool CModWizardSubPanel_CopyFiles_Source2006::BuildCopyFilesForMod_FromScratch()
{
	bool retVal = false;
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s%s", m_OutputDirName, "src" );

	// These go into c:\mymod
	char *sources_mappings[] =
	{
		"src_mod\\ep1", "*.*", outputSrcDirName,
		"sampleapp_sources", "*.*", m_OutputDirName
	};

	// Also, create the mapsrc/modelsrc/materialsrc directories now because they may not be there yet.
	CreateSubdirectory( m_OutputDirName, "modelsrc" );
	CreateSubdirectory( m_OutputDirName, "materialsrc" );
	CreateSubdirectory( m_OutputDirName, "mapsrc" );
		
	char *content_mappings[] =
	{
		"sampleapp",			"*.*",				m_OutModGamedirName,
	};

	// Copy gameinfo.txt
	const char *replacements[] = 
	{
		"%modname%", m_ModName
	};
	
	retVal = CopyWithReplacements( "CreateModFiles\\base\\gameinfo_ep1.txt", replacements, ARRAYSIZE( replacements ), "%s%s", m_OutModGamedirName, "gameinfo.txt" );
	
	if ( retVal &&
	     BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) ) &&
		 BuildCopyFilesForMappings( content_mappings, ARRAYSIZE( content_mappings ) ) )
	{
		retVal = true;
	}
	else
	{
		retVal = false;
	}
	
	return retVal;
}


bool CModWizardSubPanel_CopyFiles_Source2009::BuildCopyFilesForMod_FromScratch()
{
	bool retVal = false;
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s%s", m_OutputDirName, "src" );

	// These go into c:\mymod
	char *sources_mappings[] =
	{
		"src_mod\\source2009", "*.*", outputSrcDirName,
		"sampleapp_sources", "*.*", m_OutputDirName
	};

	// Also, create the mapsrc/modelsrc/materialsrc directories now because they may not be there yet.
	CreateSubdirectory( m_OutputDirName, "modelsrc" );
	CreateSubdirectory( m_OutputDirName, "materialsrc" );
	CreateSubdirectory( m_OutputDirName, "mapsrc" );

	char *content_mappings[] =
	{
		"CreateModFiles\\source2009\\template",			"*.*",				m_OutModGamedirName,
	};

	// Copy gameinfo.txt
	const char *replacements[] = 
	{
		"%modname%", m_ModName
	};

	retVal = CopyWithReplacements( "CreateModFiles\\source2009\\gameinfo_template.txt", replacements, ARRAYSIZE( replacements ), "%s%s", m_OutModGamedirName, "gameinfo.txt" );

	if ( retVal &&
		BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) ) &&
		BuildCopyFilesForMappings( content_mappings, ARRAYSIZE( content_mappings ) ) )
	{
		retVal = true;
	}
	else
	{
		retVal = false;
	}


	return retVal;
}



bool CModWizardSubPanel_CopyFiles_Source2007::BuildCopyFilesForMod_FromScratch()
{
	bool retVal = false;
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s%s", m_OutputDirName, "src" );

	// These go into c:\mymod
	char *sources_mappings[] =
	{
		"src_mod\\orangebox", "*.*", outputSrcDirName,
		"sampleapp_sources", "*.*", m_OutputDirName
	};

	// Also, create the mapsrc/modelsrc/materialsrc directories now because they may not be there yet.
	CreateSubdirectory( m_OutputDirName, "modelsrc" );
	CreateSubdirectory( m_OutputDirName, "materialsrc" );
	CreateSubdirectory( m_OutputDirName, "mapsrc" );

	char *content_mappings[] =
	{
		"CreateModFiles\\orangebox\\template",			"*.*",				m_OutModGamedirName,
	};

	// Copy gameinfo.txt
	const char *replacements[] = 
	{
		"%modname%", m_ModName
	};

	retVal = CopyWithReplacements( "CreateModFiles\\orangebox\\gameinfo_template.txt", replacements, ARRAYSIZE( replacements ), "%s%s", m_OutModGamedirName, "gameinfo.txt" );

	if ( retVal &&
		BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) ) &&
		BuildCopyFilesForMappings( content_mappings, ARRAYSIZE( content_mappings ) ) )
	{
		retVal = true;
	}
	else
	{
		retVal = false;
	}


	return retVal;
}


bool CModWizardSubPanel_CopyFiles_Source2006::BuildCopyFilesForMod_HL2MP()
{
	bool retVal = false;
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s%s", m_OutputDirName, "src" );

	char outputUIDir[MAX_PATH], outputResourceDir[MAX_PATH], outputScriptsDir[MAX_PATH];
	Q_snprintf( outputUIDir, sizeof( outputUIDir ), "%sResource%cUI", m_OutModGamedirName, CORRECT_PATH_SEPARATOR );
	Q_snprintf( outputResourceDir, sizeof( outputResourceDir ), "%sresource", m_OutModGamedirName );
	Q_snprintf( outputScriptsDir, sizeof( outputScriptsDir ), "%sscripts", m_OutModGamedirName );

	// Also, create the mapsrc/modelsrc/materialsrc directories now because they may not be there yet.
	CreateSubdirectory( m_OutputDirName, "modelsrc" );
	CreateSubdirectory( m_OutputDirName, "materialsrc" );
	CreateSubdirectory( m_OutputDirName, "mapsrc" );

	// These go in c:\mymod
	char *sources_mappings[] =
	{
		"src_mod\\ep1", "*.*", outputSrcDirName
	};

	char outputGamedirNameNoSlash[MAX_PATH];
	Q_strncpy( outputGamedirNameNoSlash, m_OutModGamedirName, sizeof( outputGamedirNameNoSlash ) );
	if ( strlen( outputGamedirNameNoSlash ) > 0 )
		outputGamedirNameNoSlash[strlen(outputGamedirNameNoSlash)-1] = 0;

	// These go in c:\steam\steamapps\sourcemods\modname
	char *content_mappings[] = 
	{ 
		"game_content\\half-life 2 deathmatch\\hl2mp",	"lights.rad",	outputGamedirNameNoSlash,
		"game_content\\half-life 2 deathmatch\\hl2mp",	"detail.vbsp",	outputGamedirNameNoSlash,
		"CreateModFiles\\hl2mp\\scripts",				"*.*",			outputScriptsDir,
		"CreateModFiles\\hl2mp\\resource",				"*.*",			outputResourceDir,
		"CreateModFiles\\hl2mp\\resource\\ui",			"*.*",			outputUIDir
	};

	// Copy gameinfo.txt
	const char *replacements[] = 
	{
		"%modname%", m_ModName
	};

	retVal = CopyWithReplacements( "CreateModFiles\\hl2mp\\gameinfo_ep1.txt", replacements, ARRAYSIZE( replacements ), "%s%s", m_OutModGamedirName, "gameinfo.txt" );

	if ( retVal && 
		BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) ) &&
		BuildCopyFilesForMappings( content_mappings, ARRAYSIZE( content_mappings ) ) )
	{
		retVal = true;
	}
	else
	{
		retVal = false;
	}

	return retVal;
}


bool CModWizardSubPanel_CopyFiles_Source2009::BuildCopyFilesForMod_HL2MP()
{
	bool retVal = false;
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s%s", m_OutputDirName, "src" );

	char outputUIDir[MAX_PATH], outputResourceDir[MAX_PATH], outputScriptsDir[MAX_PATH];
	Q_snprintf( outputUIDir, sizeof( outputUIDir ), "%sResource%cUI", m_OutModGamedirName, CORRECT_PATH_SEPARATOR );
	Q_snprintf( outputResourceDir, sizeof( outputResourceDir ), "%sresource", m_OutModGamedirName );
	Q_snprintf( outputScriptsDir, sizeof( outputScriptsDir ), "%sscripts", m_OutModGamedirName );

	// Also, create the mapsrc/modelsrc/materialsrc directories now because they may not be there yet.
	CreateSubdirectory( m_OutputDirName, "modelsrc" );
	CreateSubdirectory( m_OutputDirName, "materialsrc" );
	CreateSubdirectory( m_OutputDirName, "mapsrc" );

	// These go in c:\mymod
	char *sources_mappings[] =
	{
		"src_mod\\source2009", "*.*", outputSrcDirName
	};

	char outputGamedirNameNoSlash[MAX_PATH];
	Q_strncpy( outputGamedirNameNoSlash, m_OutModGamedirName, sizeof( outputGamedirNameNoSlash ) );
	if ( strlen( outputGamedirNameNoSlash ) > 0 )
		outputGamedirNameNoSlash[strlen(outputGamedirNameNoSlash)-1] = 0;

	// These go in c:\steam\steamapps\sourcemods\modname
	char *content_mappings[] = 
	{ 
		"game_content\\half-life 2 deathmatch\\hl2mp",					"lights.rad",	outputGamedirNameNoSlash,
		"game_content\\half-life 2 deathmatch\\hl2mp",					"detail.vbsp",	outputGamedirNameNoSlash,
		"CreateModFiles\\source2009\\multiplayer",						"*.*",			m_OutModGamedirName,
	};

	// Copy gameinfo.txt
	const char *replacements[] = 
	{
		"%modname%", m_ModName
	};

	retVal = CopyWithReplacements( "CreateModFiles\\source2009\\gameinfo_mp.txt", replacements, ARRAYSIZE( replacements ), "%s%s", m_OutModGamedirName, "gameinfo.txt" );

	if ( retVal && 
		BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) ) &&
		BuildCopyFilesForMappings( content_mappings, ARRAYSIZE( content_mappings ) ) )
	{
		retVal = true;
	}
	else
	{
		retVal = false;
	}

	return retVal;
}


bool CModWizardSubPanel_CopyFiles_Source2007::BuildCopyFilesForMod_HL2MP()
{
	bool retVal = false;
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s%s", m_OutputDirName, "src" );

	char outputUIDir[MAX_PATH], outputResourceDir[MAX_PATH], outputScriptsDir[MAX_PATH];
	Q_snprintf( outputUIDir, sizeof( outputUIDir ), "%sResource%cUI", m_OutModGamedirName, CORRECT_PATH_SEPARATOR );
	Q_snprintf( outputResourceDir, sizeof( outputResourceDir ), "%sresource", m_OutModGamedirName );
	Q_snprintf( outputScriptsDir, sizeof( outputScriptsDir ), "%sscripts", m_OutModGamedirName );

	// Also, create the mapsrc/modelsrc/materialsrc directories now because they may not be there yet.
	CreateSubdirectory( m_OutputDirName, "modelsrc" );
	CreateSubdirectory( m_OutputDirName, "materialsrc" );
	CreateSubdirectory( m_OutputDirName, "mapsrc" );

	// These go in c:\mymod
	char *sources_mappings[] =
	{
		"src_mod\\orangebox", "*.*", outputSrcDirName
	};

	char outputGamedirNameNoSlash[MAX_PATH];
	Q_strncpy( outputGamedirNameNoSlash, m_OutModGamedirName, sizeof( outputGamedirNameNoSlash ) );
	if ( strlen( outputGamedirNameNoSlash ) > 0 )
		outputGamedirNameNoSlash[strlen(outputGamedirNameNoSlash)-1] = 0;
	
	// These go in c:\steam\steamapps\sourcemods\modname
	char *content_mappings[] = 
	{ 
		"game_content\\half-life 2 deathmatch\\hl2mp",					"lights.rad",	outputGamedirNameNoSlash,
		"game_content\\half-life 2 deathmatch\\hl2mp",					"detail.vbsp",	outputGamedirNameNoSlash,
		"CreateModFiles\\orangebox\\multiplayer",						"*.*",			m_OutModGamedirName,
	};

	// Copy gameinfo.txt
	const char *replacements[] = 
	{
		"%modname%", m_ModName
	};
	
	retVal = CopyWithReplacements( "CreateModFiles\\orangebox\\gameinfo_mp.txt", replacements, ARRAYSIZE( replacements ), "%s%s", m_OutModGamedirName, "gameinfo.txt" );
	
	if ( retVal && 
	     BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) ) &&
		 BuildCopyFilesForMappings( content_mappings, ARRAYSIZE( content_mappings ) ) )
	{
		retVal = true;
	}
	else
	{
		retVal = false;
	}
	
	return retVal;
}


bool CModWizardSubPanel_CopyFiles_Source2006::BuildCopyFilesForMod_SourceCodeOnly()
{
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s", m_OutputDirName );
	int len = strlen( outputSrcDirName );
	if ( len > 0 && PATHSEPARATOR( outputSrcDirName[len-1] ) )
		outputSrcDirName[len-1] = 0;

	char *sources_mappings[] =
	{
		"src_mod\\ep1", "*.*", outputSrcDirName
	};

	return BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) );
}


bool CModWizardSubPanel_CopyFiles_Source2009::BuildCopyFilesForMod_SourceCodeOnly()
{
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s", m_OutputDirName );
	int len = strlen( outputSrcDirName );
	if ( len > 0 && PATHSEPARATOR( outputSrcDirName[len-1] ) )
		outputSrcDirName[len-1] = 0;

	char *sources_mappings[] =
	{
		"src_mod\\source2009", "*.*", outputSrcDirName
	};

	return BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) );
}


bool CModWizardSubPanel_CopyFiles_Source2007::BuildCopyFilesForMod_SourceCodeOnly()
{
	char outputSrcDirName[MAX_PATH];
	Q_snprintf( outputSrcDirName, sizeof( outputSrcDirName ), "%s", m_OutputDirName );
	int len = strlen( outputSrcDirName );
	if ( len > 0 && PATHSEPARATOR( outputSrcDirName[len-1] ) )
		outputSrcDirName[len-1] = 0;

	char *sources_mappings[] =
	{
		"src_mod\\orangebox", "*.*", outputSrcDirName
	};

	return BuildCopyFilesForMappings( sources_mappings, ARRAYSIZE( sources_mappings ) );
}


void CModWizardSubPanel_CopyFiles::OnTick()
{
	if ( m_iCurCopyFile == -1 )
	{
		// Figure out if we're an hl2 mod or not.
		m_ModType = ModType_HL2;
		CModWizardSubPanel_Intro *pIntro = dynamic_cast<CModWizardSubPanel_Intro*>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_Intro"));
		if ( pIntro )
			m_ModType = pIntro->GetModType();


		if ( m_ModType == ModType_HL2 )
		{
			if ( !BuildCopyFilesForMod_HL2() )
				return;
		}
		else if ( m_ModType == ModType_HL2_Multiplayer )
		{
			if ( !BuildCopyFilesForMod_HL2MP() )
				return;
		}
		else if ( m_ModType == ModType_SourceCodeOnly )
		{
			if ( !BuildCopyFilesForMod_SourceCodeOnly() )
				return;
		}
		else
		{
			if ( !BuildCopyFilesForMod_FromScratch() )
				return;
		}


		// Prepare the lists of files to copy.
		m_iCurCopyFile = 0;

		// Get the output dir name sans the slash at the end.
		if ( m_ModType != ModType_SourceCodeOnly )
		{
			char outModGamedirNameNoSlash[MAX_PATH];
			Assert( m_OutModGamedirName[strlen(m_OutModGamedirName)-1] == CORRECT_PATH_SEPARATOR );
			Q_StrSlice( m_OutModGamedirName, 0, -1, outModGamedirNameNoSlash, sizeof( outModGamedirNameNoSlash ) );

			// Figure out the steam directory. Starting with gamedir, which is
			// 
			char steamdir[MAX_PATH];
			Q_strncpy( steamdir, gamedir, sizeof( steamdir ) );	// c:\valve\steam\steamapps\name\sourcesdk\launcher
			Q_StripLastDir( steamdir, sizeof( steamdir ) );		// c:\valve\steam\steamapps\name\sourcesdk
			Q_StripLastDir( steamdir, sizeof( steamdir ) );		// c:\valve\steam\steamapps\name
			Q_StripLastDir( steamdir, sizeof( steamdir ) );		// c:\valve\steam\steamapps
			Q_StripLastDir( steamdir, sizeof( steamdir ) );		// c:\valve\steam
			Q_AppendSlash( steamdir, sizeof( steamdir ) );


			// Setup the path to their hl2 game folder.
			char hl2dir[MAX_PATH];
			Q_strncpy( hl2dir, gamedir, sizeof( hl2dir ) );	// c:\valve\steam\steamapps\name\sourcesdk\launcher
			Q_StripLastDir( hl2dir, sizeof( hl2dir ) );		// c:\valve\steam\steamapps\name\sourcesdk
			Q_StripLastDir( hl2dir, sizeof( hl2dir ) );		// c:\valve\steam\steamapps\name
			Q_AppendSlash( hl2dir, sizeof( hl2dir ) );
			V_strncat( hl2dir, "source sdk base", sizeof( hl2dir ), COPY_ALL_CHARACTERS );

			// If the engine version is 'orange box' then use the new 'source sdk base 2009' to launch the mods
			if ( !V_strcmp( g_engineDir, "orangebox" ) )
			{
				V_strncat( hl2dir, " 2009", sizeof( hl2dir ), COPY_ALL_CHARACTERS );
			}
			// If the engine version isn't 'source2007' then use the new 'source sdk base 2007' to launch the mods
			else if ( !V_strcmp( g_engineDir, "source2007" ) )
			{
				V_strncat( hl2dir, " 2007", sizeof( hl2dir ), COPY_ALL_CHARACTERS );
			}

			char szAppID[10];

			// DoD only people need to run their mod via DoD
			if ( IsGameSubscribed( GetAppSteamAppId( k_App_DODS ) ) && !IsGameSubscribed( GetAppSteamAppId( k_App_HL2 ) ) && !IsGameSubscribed( GetAppSteamAppId( k_App_HL2MP ) ) )
			{
				_itoa_s(GetAppSteamAppId( k_App_DODS ), szAppID, sizeof(szAppID), 10);
			}
			else
			{
				// Otherwise use Source SDK Base
				_itoa_s(GetAppSteamAppId( k_App_SDK_BASE ), szAppID, sizeof(szAppID), 10);
			}

			// Copy the batch files.
			const char *replacements[] = 
			{
				"%steamdir%", steamdir,
				"%appid%", szAppID,
				"%hl2dir%", hl2dir,
				"%moddir%", outModGamedirNameNoSlash,
				"%bindir%", GetSDKToolsBinDirectory()
			};
			const char *filenames[] =
			{
				"run_mod.bat",		
				"run_hlmv.bat",		
				"run_studiomdl.bat",
				"run_hammer.bat"	
			};
			for ( int iFilename=0; iFilename < ARRAYSIZE( filenames ); iFilename++ )
			{
				char srcFilename[MAX_PATH];
				Q_snprintf( srcFilename, sizeof( srcFilename ), "CreateModFiles\\%s", filenames[iFilename] );
				if ( !CopyWithReplacements( 
						srcFilename, replacements, ARRAYSIZE( replacements ), 
						"%s%s", m_OutputDirName, filenames[iFilename] ) )
				{
					vgui::ivgui()->RemoveTickSignal( GetVPanel() );
					GetWizardPanel()->SetCancelButtonEnabled( true );
					return;
				}
			}
		}
	}
	else
	{
		bool bFinished = false;

		// File copying has begun. Copy N more files and update our label.
		int nCopied = 0;
		while ( nCopied < 10 )
		{
			if ( m_iCurCopyFile >= m_FileCopyInfos.Count() )
			{
				// Also, add a game configuration.
				char modGamedirNoSlash[MAX_PATH];
				Q_strncpy( modGamedirNoSlash, m_OutModGamedirName, sizeof( modGamedirNoSlash ) );
				if ( strlen( modGamedirNoSlash ) > 0 )
				{
					if ( modGamedirNoSlash[strlen(modGamedirNoSlash)-1] == '/' || modGamedirNoSlash[strlen(modGamedirNoSlash)-1] == '\\' )
						modGamedirNoSlash[strlen(modGamedirNoSlash)-1] = 0;
				}

				if ( m_ModType != ModType_SourceCodeOnly )
					AddConfig( m_ModName, modGamedirNoSlash, m_ModType );

				// Setup the next panel.
				CModWizardSubPanel_Finished *pNextPanel = dynamic_cast<CModWizardSubPanel_Finished*>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_Finished"));
				if ( !pNextPanel )
				{
					VGUIMessageBox( this, "Error", "Can't find CModWizardSubPanel_Finished panel." );
					GetWizardPanel()->SetCancelButtonEnabled( true );
					return;
				}

				pNextPanel->GetReady( m_OutputDirName );
				
				// Direct them out..
				GetWizardPanel()->SetNextButtonEnabled( true );

				m_pFinishedLabel->SetVisible( true );
				m_pProgressBar->SetProgress( 1 );
				bFinished = true;

				vgui::ivgui()->RemoveTickSignal( GetVPanel() );
				break;
			}

			// Copy another file.
			bool bRet= false;
			CFileCopyInfo *pInfo = &m_FileCopyInfos[m_iCurCopyFile++];

			if ( !HandleSpecialFileCopy( pInfo, bRet ) )
			{
				bRet = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );
			}

			if ( !bRet )
			{
				vgui::ivgui()->RemoveTickSignal( GetVPanel() );
				GetWizardPanel()->SetCancelButtonEnabled( true );
				break;
			}
			++nCopied;
		}

		// Update our label.
		if ( !bFinished )
		{
			unsigned int iNum = min( m_iCurCopyFile, m_FileCopyInfos.Count()-1 );
			if ( iNum < (unsigned int)m_FileCopyInfos.Count() )
			{
				char msg[512];
				Q_snprintf( msg, sizeof( msg ), "%s", m_FileCopyInfos[iNum].m_InFilename );
				m_pLabel->SetText( msg );

				m_pProgressBar->SetProgress( (float)iNum / m_FileCopyInfos.Count() );
			}
		}
	}
	BaseClass::OnTick();
}


bool IsVCProjFile( const char *pFilename )
{
	char ext[512];
	Q_StrRight( pFilename, 7, ext, sizeof( ext ) );
	return ( Q_stricmp( ext, ".vcproj" ) == 0 );
}	


bool CModWizardSubPanel_CopyFiles_Source2009::HandleReplacements_GameProjectFiles( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	// Speed up the copy process a bit, if it's not a vcproj, get out.
	if ( !IsVCProjFile( pInfo->m_InFilename ) )
		return false;

	char replaceWith[MAX_PATH] = {0};
	const char *replacements[] = 
	{
		"..\\..\\game\\template\\", replaceWith,
		"..\\..\\game\\sdksample\\", replaceWith, 
		"..\\..\\game\\hl2\\", replaceWith,
		"..\\..\\game\\hl2mp\\", replaceWith,
		"..\\game\\bin", "..\\bin"
	};

	// No 'dx8' shaders in orange box, at least any time soon, just keeping it cleaned up.
	if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\materialsystem\\stdshaders\\stdshader_dx9-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );
		bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );

		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game\\client\\client_scratch-2005.vcproj" ) == 0 ||
		Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game\\server\\server_scratch-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_FromScratch )
		{
			Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );

			bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );
		}

		return true;
	}
	// removed 'hl2' projects as they're not needed in orange box anymore.
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game\\client\\client_episodic-2005.vcproj" ) == 0 ||
		Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game\\server\\server_episodic-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2 || m_ModType == ModType_SourceCodeOnly )
		{
			Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );

			bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );
		}

		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game\\client\\client_hl2mp-2005.vcproj" ) == 0 || 
		Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game\\server\\server_hl2mp-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2_Multiplayer )
		{
			Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );

			bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );
		}

		return true;
	}

	return false;
}


bool CModWizardSubPanel_CopyFiles_Source2007::HandleReplacements_GameProjectFiles( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	// Speed up the copy process a bit, if it's not a vcproj, get out.
	if ( !IsVCProjFile( pInfo->m_InFilename ) )
		return false;

	char replaceWith[MAX_PATH] = {0};
	const char *replacements[] = 
	{
		"..\\..\\game\\template\\", replaceWith,
		"..\\..\\game\\sdksample\\", replaceWith, 
		"..\\..\\game\\hl2\\", replaceWith,
		"..\\..\\game\\hl2mp\\", replaceWith,
		"..\\game\\bin", "..\\bin"
	};

	// No 'dx8' shaders in orange box, at least any time soon, just keeping it cleaned up.
	if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\materialsystem\\stdshaders\\stdshader_dx9-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );
		bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );

		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game\\client\\client_scratch-2005.vcproj" ) == 0 ||
		      Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game\\server\\server_scratch-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_FromScratch )
		{
			Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );

			bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );
		}

		return true;
	}
	// removed 'hl2' projects as they're not needed in orange box anymore.
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game\\client\\client_episodic-2005.vcproj" ) == 0 ||
			  Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game\\server\\server_episodic-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2 || m_ModType == ModType_SourceCodeOnly )
		{
			Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );

			bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );
		}

		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game\\client\\client_hl2mp-2005.vcproj" ) == 0 || 
			  Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game\\server\\server_hl2mp-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2_Multiplayer )
		{
			Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );

			bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );
		}

		return true;
	}

	return false;
}

bool CModWizardSubPanel_CopyFiles_Source2006::HandleReplacements_GameProjectFiles( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	// Speed up the copy process a bit, if it's not a vcproj, get out.
	if ( !IsVCProjFile( pInfo->m_InFilename ) )
		return false;

	char replaceWith[MAX_PATH] = {0};
	const char *replacements[] = 
	{
		"..\\..\\game\\sdksample\\", replaceWith, 
		"..\\..\\game\\hl2\\", replaceWith,
		"..\\..\\game\\hl2mp\\", replaceWith,
		"..\\game\\bin", "..\\bin"
	};

	if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\materialsystem\\stdshaders\\stdshader_dx8-2003.vcproj" ) == 0 ||
		 Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\materialsystem\\stdshaders\\stdshader_dx8-2005.vcproj" ) == 0 || 
		 Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\materialsystem\\stdshaders\\stdshader_dx9-2003.vcproj" ) == 0 ||
		 Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\materialsystem\\stdshaders\\stdshader_dx9-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );
		bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );

		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\cl_dll\\client_scratch-2003.vcproj" ) == 0 || 
              Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\cl_dll\\client_scratch-2005.vcproj" ) == 0 ||
              Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\dlls\\server_scratch-2003.vcproj" ) == 0 ||
              Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\dlls\\server_scratch-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_FromScratch )
		{
			Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );
			
			bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );
		}

		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\cl_dll\\client_hl2-2003.vcproj" ) == 0 || 
		      Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\cl_dll\\client_hl2-2005.vcproj" ) == 0 ||
			  Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\dlls\\server_hl2-2003.vcproj" ) == 0 ||
		      Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\dlls\\server_hl2-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2 || m_ModType == ModType_SourceCodeOnly )
		{
			Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );

			bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );
		}
		
		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\cl_dll\\client_hl2mp-2003.vcproj" ) == 0 || 
		      Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\cl_dll\\client_hl2mp-2005.vcproj" ) == 0 || 
		      Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\dlls\\server_hl2mp-2003.vcproj" ) == 0 ||
			  Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\dlls\\server_hl2mp-2005.vcproj" ) == 0 )
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2_Multiplayer )
		{
			Q_snprintf( replaceWith, sizeof( replaceWith ), "%s", m_OutModGamedirName );

			bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );
		}
		
		return true;
	}

	return false;
}


bool CModWizardSubPanel_CopyFiles::HandleReplacements_GenericVCProj( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	if ( IsVCProjFile( pInfo->m_InFilename ) )
	{	
		// This code is for all the tools. Internally, Valve uses <base dir>\game\bin, and externally,
		// they're using <game dir>\bin to store tools.
		const char *vcprojReplacements[] =
		{
			"..\\game\\bin", "..\\bin"
		};

		bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, vcprojReplacements, ARRAYSIZE( vcprojReplacements ), "%s", pInfo->m_OutFilename );
		return true;
	}

	return false;
}


bool CModWizardSubPanel_CopyFiles_Source2006::HandleReplacements_Solution( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\game_scratch-2003.sln" ) == 0 ||
		 Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\game_scratch-2005.sln" ) == 0)
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_FromScratch )
			bErrorStatus = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );

		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\game_hl2-2003.sln" ) == 0 ||
		      Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\game_hl2-2005.sln" ) == 0)
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2 || m_ModType == ModType_SourceCodeOnly )
		{
			bErrorStatus = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );
		}
		
		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\game_hl2mp-2003.sln" ) == 0 ||
		      Q_stricmp( pInfo->m_InFilename, "src_mod\\ep1\\game_hl2mp-2005.sln" ) == 0)
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2_Multiplayer )
		{
			bErrorStatus = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );
		}
		
		return true;
	}

	return false;
}


bool CModWizardSubPanel_CopyFiles_Source2009::HandleReplacements_Solution( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game_scratch-2005.sln" ) == 0)
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_FromScratch )
			bErrorStatus = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );

		return true;
	}
	// Episodic now, not 'hl2'!!
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game_episodic-2005.sln" ) == 0)
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2 || m_ModType == ModType_SourceCodeOnly )
		{
			bErrorStatus = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );
		}

		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game_hl2mp-2005.sln" ) == 0)
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2_Multiplayer )
		{
			bErrorStatus = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );
		}

		return true;
	}

	return false;
}


bool CModWizardSubPanel_CopyFiles_Source2007::HandleReplacements_Solution( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game_scratch-2005.sln" ) == 0)
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_FromScratch )
			bErrorStatus = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );

		return true;
	}
	// Episodic now, not 'hl2'!!
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game_episodic-2005.sln" ) == 0)
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2 || m_ModType == ModType_SourceCodeOnly )
		{
			bErrorStatus = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );
		}

		return true;
	}
	else if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game_hl2mp-2005.sln" ) == 0)
	{
		bErrorStatus = true;
		if ( m_ModType == ModType_HL2_Multiplayer )
		{
			bErrorStatus = DoCopyFile( pInfo->m_InFilename, pInfo->m_OutFilename );
		}

		return true;
	}

	return false;
}

bool CModWizardSubPanel_CopyFiles_Source2009::HandleReplacements_TemplateOptions( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	// Copy over the sdk_shareddefs.h with replacements for the defines.
	if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\source2009\\game\\shared\\sdk\\sdk_shareddefs.h" ) == 0 )
	{
		// If the panel can't be found, get out.
		CModWizardSubPanel_TemplateOptions *pTemplate = dynamic_cast<CModWizardSubPanel_TemplateOptions*>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_TemplateOptions"));
		if ( !pTemplate )
			return false;

		const char *replacements[] = 
		{
			"%TemplateOptionTeams%", pTemplate->GetOption(TPOPTION_TEAMS),
			"%TemplateOptionClasses%", pTemplate->GetOption(TPOPTION_CLASSES), 
			"%TemplateOptionStamina%", pTemplate->GetOption(TPOPTION_STAMINA),
			"%TemplateOptionSprinting%", pTemplate->GetOption(TPOPTION_SPRINTING),
			"%TemplateOptionProne%", pTemplate->GetOption(TPOPTION_PRONE),
			"%TemplateOptionShootWhileSprinting%", pTemplate->GetOption(TPOPTION_SHOOTSPRINTING),
			"%TemplateOptionShootOnLadders%", pTemplate->GetOption(TPOPTION_SHOOTLADDERS),
			"%TemplateOptionShootWhileJumping%", pTemplate->GetOption(TPOPTION_SHOOTJUMPING)
		};

		bErrorStatus = true;
		bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );

		return true;
	}

	return false;
}


bool CModWizardSubPanel_CopyFiles_Source2007::HandleReplacements_TemplateOptions( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	// Copy over the sdk_shareddefs.h with replacements for the defines.
	if ( Q_stricmp( pInfo->m_InFilename, "src_mod\\orangebox\\game\\shared\\sdk\\sdk_shareddefs.h" ) == 0 )
	{
		// If the panel can't be found, get out.
		CModWizardSubPanel_TemplateOptions *pTemplate = dynamic_cast<CModWizardSubPanel_TemplateOptions*>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_TemplateOptions"));
		if ( !pTemplate )
			return false;

		const char *replacements[] = 
		{
			"%TemplateOptionTeams%", pTemplate->GetOption(TPOPTION_TEAMS),
			"%TemplateOptionClasses%", pTemplate->GetOption(TPOPTION_CLASSES), 
			"%TemplateOptionStamina%", pTemplate->GetOption(TPOPTION_STAMINA),
			"%TemplateOptionSprinting%", pTemplate->GetOption(TPOPTION_SPRINTING),
			"%TemplateOptionProne%", pTemplate->GetOption(TPOPTION_PRONE),
			"%TemplateOptionShootWhileSprinting%", pTemplate->GetOption(TPOPTION_SHOOTSPRINTING),
			"%TemplateOptionShootOnLadders%", pTemplate->GetOption(TPOPTION_SHOOTLADDERS),
			"%TemplateOptionShootWhileJumping%", pTemplate->GetOption(TPOPTION_SHOOTJUMPING)
		};

		bErrorStatus = true;
		bErrorStatus = CopyWithReplacements( pInfo->m_InFilename, replacements, ARRAYSIZE( replacements ), "%s", pInfo->m_OutFilename );

		return true;
	}

	return false;
}

bool CModWizardSubPanel_CopyFiles::HandleSpecialFileCopy( CFileCopyInfo *pInfo, bool &bErrorStatus )
{
	if ( HandleReplacements_TemplateOptions( pInfo, bErrorStatus ) )
		return true;

	if ( HandleReplacements_GameProjectFiles( pInfo, bErrorStatus ) )
		return true;

	if ( HandleReplacements_GenericVCProj( pInfo, bErrorStatus ) )
		return true;

	if ( HandleReplacements_Solution( pInfo, bErrorStatus ) )
		return true;

	return false;
}

CModWizardSubPanel_CopyFiles_Source2006::CModWizardSubPanel_CopyFiles_Source2006( Panel *parent, const char *panelName ) : CModWizardSubPanel_CopyFiles( parent, panelName )
{
}

CModWizardSubPanel_CopyFiles_Source2007::CModWizardSubPanel_CopyFiles_Source2007( Panel *parent, const char *panelName ) : CModWizardSubPanel_CopyFiles( parent, panelName )
{
}

CModWizardSubPanel_CopyFiles_Source2009::CModWizardSubPanel_CopyFiles_Source2009( Panel *parent, const char *panelName ) : CModWizardSubPanel_CopyFiles( parent, panelName )
{
}
