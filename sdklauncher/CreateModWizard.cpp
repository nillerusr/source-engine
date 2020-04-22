//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <windows.h>
#include "CreateModWizard.h"
#include "sdklauncher_main.h"
#include "filesystem_tools.h"
#include "sdklauncherdialog.h"
#include "configs.h"
#include <vgui_controls/WizardSubPanel.h>
#include <vgui_controls/DirectorySelectDialog.h>
#include <vgui/ivgui.h>
#include <vgui/iinput.h>
#include <ctype.h>
#include <io.h>
#include <direct.h>
#include "ModWizard_Intro.h"
#include "ModWizard_GetModInfo.h"
#include "ModWizard_CopyFiles.h"
#include "ModWizard_Finished.h"
#include "ModWizard_TemplateOptions.h"

extern char g_engineDir[50];
using namespace vgui;


// Set to true when the mod wizard completes successfully.
bool g_bModWizardFinished = false;


bool CreateFullDirectory( const char *pDirName )
{
	CUtlVector<char*> dirs;
	const char *separators[2] = {"\\", "/"};
	Q_SplitString2( pDirName, separators, ARRAYSIZE( separators ), dirs );

	if ( dirs.Count() == 0 )
		return false;

	char dirName[512];
	Q_strncpy( dirName, dirs[0], sizeof( dirName ) );
	for ( int i=1; i < dirs.Count(); i++ )
	{
		Q_AppendSlash( dirName, sizeof( dirName ) );
		Q_strncat( dirName, dirs[i], sizeof( dirName ), COPY_ALL_CHARACTERS );
		if ( _access( dirName, 0 ) != 0 )
			if ( _mkdir( dirName ) != 0 )
				return false;
	}
	dirs.PurgeAndDeleteElements();
	return true;
}


bool CreateSubdirectory( const char *pDirName, const char *pSubdirName )
{
	char str[512];
	Q_strncpy( str, pDirName, sizeof( str ) );
	Q_AppendSlash( str, sizeof( str ) );
	Q_strncat( str, pSubdirName, sizeof( str ), COPY_ALL_CHARACTERS );
	return CreateFullDirectory( str );
}


void RunCreateModWizard( bool bRunFromCommandLine )
{
	CCreateModWizard *pWizard = new CCreateModWizard( g_pMainFrame, "CreateModWizard", NULL, bRunFromCommandLine );
	pWizard->Run();
}


bool DoCopyFile( const char *pInputFilename, const char *pOutputFilename )
{
	return CopyWithReplacements( pInputFilename, NULL, 0, "%s", pOutputFilename );
}


void SetModWizardStatusCode( unsigned long inputCode )
{
	DWORD code = inputCode;
	HKEY hKey;
	// Changed to HKEY_CURRENT_USER from HKEY_LOCAL_MACHINE
	if ( RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Valve\\SourceSDK", 0, 0, 0, KEY_ALL_ACCESS, NULL, &hKey, NULL ) == ERROR_SUCCESS )
	{
		RegSetValueEx( hKey, "ModWizard_StatusCode", 0, REG_DWORD, (unsigned char*)&code, sizeof( code ) );
		RegCloseKey( hKey );
	}
}


void NoteModWizardFinished()
{
	g_bModWizardFinished = true;
}


// --------------------------------------------------------------------------------------------------------------------- //
// CFinalStatusWindow
// --------------------------------------------------------------------------------------------------------------------- //
class CFinalStatusWindow : public vgui::Frame
{
public:
	typedef vgui::Frame BaseClass;

	CFinalStatusWindow( vgui::Panel *parent, const char *pName, const char *pOutputDirName, const char *pOutputModGamedirName ) 
		: BaseClass( parent, pName )
	{
		m_pLabel = new vgui::Label( this, "MessageLabel", "" );
		
		LoadControlSettings( "FinalStatusWindow.res" );

		char msg[512];
		Q_snprintf( msg, sizeof( msg ), "Files copied successfully!\n\n"
			"- The source code is in '%ssrc'.\n"
			"- You can run the base mod by running '%srunmod.bat'.\n"
			"- You can run the HL2 mod by running '%srunhl2.bat'.\n"
			"- There is also a new item in your game list."
			, pOutputDirName, pOutputDirName, pOutputDirName );
		m_pLabel->SetText( msg );
	}

private:
	vgui::Label *m_pLabel;
};


// --------------------------------------------------------------------------------------------------------------------- //
// CreateModWizard implementation.
// --------------------------------------------------------------------------------------------------------------------- //

CCreateModWizard::CCreateModWizard( vgui::Panel *parent, const char *name, KeyValues *pKeyValues, bool bRunFromCommandLine )
	: BaseClass( parent, name )
{
	m_bRunFromCommandLine = bRunFromCommandLine;
	m_pKeyValues = pKeyValues;
	SetBounds(0, 0, 480, 360);

	WizardSubPanel *subPanel = new CModWizardSubPanel_Intro( this, "CModWizardSubPanel_Intro" );
	subPanel->SetVisible( false );

	subPanel = new CModWizardSubPanel_GetModInfo( this, "CModWizardSubPanel_GetModInfo" );
	subPanel->SetVisible( false );

	subPanel = new CModWizardSubPanel_TemplateOptions( this, "CModWizardSubPanel_TemplateOptions" );
	subPanel->SetVisible( false );
	

	// Tell the config manager which games to put in the config by default
	if ( !V_strcmp( g_engineDir, "orangebox" ) )
	{
		subPanel = new CModWizardSubPanel_CopyFiles_Source2009( this, "CModWizardSubPanel_CopyFiles" );
	}
	else if ( !V_strcmp( g_engineDir, "source2007" ) )
	{
		subPanel = new CModWizardSubPanel_CopyFiles_Source2007( this, "CModWizardSubPanel_CopyFiles" );
	}
	else
	{
		subPanel = new CModWizardSubPanel_CopyFiles_Source2006( this, "CModWizardSubPanel_CopyFiles" );		
	}

	subPanel->SetVisible( false );

	subPanel = new CModWizardSubPanel_Finished( this, "CModWizardSubPanel_Finished" );
	subPanel->SetVisible( false );
}

CCreateModWizard::~CCreateModWizard()
{
	if ( m_bRunFromCommandLine )
	{
		g_bAppQuit = true;
		
		if ( g_bModWizardFinished )
			SetModWizardStatusCode( 2 );
		else
			SetModWizardStatusCode( 3 );
	}
}


void CCreateModWizard::Run()
{
	// Set the CompletionCode in the registry to say that we've started.
	g_bModWizardFinished = false;
	SetModWizardStatusCode( 1 );

	CModWizardSubPanel_Intro *pIntro = (CModWizardSubPanel_Intro*)FindChildByName( "CModWizardSubPanel_Intro" );
	if ( !pIntro )
		Error( "Missing CModWizardSubPanel_Intro panel." );

	if ( g_bAutoHL2Mod )
	{
		pIntro->m_pModHL2Button->SetSelected( true );
		BaseClass::Run( dynamic_cast<WizardSubPanel *>( FindChildByName( "CModWizardSubPanel_GetModInfo" ) ) );
	}
	else
	{
		BaseClass::Run( dynamic_cast<WizardSubPanel *>(pIntro) );
	}

	MoveToCenterOfScreen();
	Activate();

	vgui::input()->SetAppModalSurface( GetVPanel() );
}

