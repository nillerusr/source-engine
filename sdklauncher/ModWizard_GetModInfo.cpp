//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "ModWizard_GetModInfo.h"
#include <vgui_controls/DirectorySelectDialog.h>
#include <vgui/IInput.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/WizardPanel.h>
#include "sdklauncher_main.h"
#include <ctype.h>
#include "CreateModWizard.h"
#include "ModWizard_CopyFiles.h"


using namespace vgui;


extern char g_engineDir[50];

// ---------------------------------------------------------------------------------------- //
// Directory select dialog that preserves the modalness of the previous dialog.
// ---------------------------------------------------------------------------------------- //

class CModalPreserveDirectorySelectDialog : public DirectorySelectDialog
{
public:
	typedef vgui::DirectorySelectDialog BaseClass;

	CModalPreserveDirectorySelectDialog(vgui::Panel *pParent, const char *title)
		: BaseClass( pParent, title )
	{
		m_PrevAppFocusPanel = vgui::input()->GetAppModalSurface();
		vgui::input()->SetAppModalSurface( GetVPanel() );
	}

	~CModalPreserveDirectorySelectDialog()
	{
		vgui::input()->SetAppModalSurface( m_PrevAppFocusPanel );
	}


public:
	vgui::VPANEL m_PrevAppFocusPanel;
};
		


CModWizardSubPanel_GetModInfo::CModWizardSubPanel_GetModInfo( Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
	m_pModPath = new vgui::TextEntry( this, "ModPath" );
	m_pModName = new vgui::TextEntry( this, "ModName" );
	new vgui::Button( this, "SearchButton", "", this, "SearchButton" );

	m_pModNameInfoLabel = new Label( this, "ModNameInfoLabel", "" );

	LoadControlSettings( "ModWizardSubPanel_GetModInfo.res");

	if ( g_bModWizard_CmdLineFields )
	{
		m_pModPath->SetText( g_ModWizard_CmdLine_ModDir );
		m_pModPath->SetEditable( false );
		m_pModPath->SetEnabled( false );

		m_pModName->SetText( g_ModWizard_CmdLine_ModName );
		m_pModName->SetEditable( false );
		m_pModName->SetEnabled( false );
	}
}								  

WizardSubPanel *CModWizardSubPanel_GetModInfo::GetNextSubPanel()
{
	// In scratch/template, go to the template options panel - orange box only!
	if ( m_ModType == ModType_FromScratch && !V_strcmp( g_engineDir, "source2007" ) )
		return dynamic_cast<WizardSubPanel *>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_TemplateOptions"));
	else
		return dynamic_cast<WizardSubPanel *>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_CopyFiles"));
}

void CModWizardSubPanel_GetModInfo::PerformLayout()
{
	BaseClass::PerformLayout();
	GetWizardPanel()->SetFinishButtonEnabled(false);
}

void CModWizardSubPanel_GetModInfo::OnDisplayAsNext()
{
	GetWizardPanel()->SetTitle( "#ModWizard_GetModInfo_Title", true );
}

bool CModWizardSubPanel_GetModInfo::OnNextButton()
{
	char modPath[512];	
	m_pModPath->GetText( modPath, sizeof( modPath ) );
	Q_AppendSlash( modPath, sizeof( modPath ) );

	char modName[512];
	m_pModName->GetText( modName, sizeof( modName ) );

	
	// Validate the path they passed in.
	if ( modPath[0] == 0 )
	{
		VGUIMessageBox( GetWizardPanel(), "Error", "#PleaseEnterModPath" );
		return false;
	}
	else if ( !Q_IsAbsolutePath( modPath ) )
	{
		VGUIMessageBox( this, "Error", "Please enter a full path (like C:\\MyMod) to install the mod." );
		return false;
	}

	// Validate the mod name.
	char outputModGamedirName[MAX_PATH];

	if ( m_ModType == ModType_SourceCodeOnly )
	{
		outputModGamedirName[0] = 0;
		modName[0] = 0;
	}
	else
	{
		if ( modName[0] == 0 )
		{
			VGUIMessageBox( this, "Error", "#PleaseEnterModName" );
			m_pModName->RequestFocus();
			return false;
		}
		int modNameLen = strlen( modName );
		for ( int i=0; i < modNameLen; i++ )
		{
			if ( !isalnum( modName[i] ) && modName[i] != '-' && modName[i] != '_' && modName[i] != ' ' )
			{
				VGUIMessageBox( this, "Error", "#ModNameInvalidCharacters" );
				return false;
			}
		}

		//Tony; after the other check for invalid characters, make the actual outputModGamedirName lowercase with spaces stripped out.
		char strippedModName[512]; //this is what gets thrown on below!
		//Tony; reuse modNameLen when I copy it to the new string.
		//I could have sworn there was an easier way to do this, but it completely escapes me.
		int i,j;
		modNameLen = strlen( modName );
		for (i=0, j=0;i < modNameLen; i++ )
		{
			 if (modName[i] == ' ') continue;
			 strippedModName[j++] = modName[i];
		}
		strippedModName[j] = '\0'; //Null terminate. 

//		Q_strncpy( strippedModName, ModName, sizeof( strippedModName ) );
		Q_strlower( strippedModName ); //Tony; convert it to lower case


		
		// Build the name of the content directory (c:\steam\steamapps\sourcemods\modname).
		Q_strncpy( outputModGamedirName, GetSDKLauncherBaseDirectory(), sizeof( outputModGamedirName ) );		// steamapps\username\sourcesdk

		Q_StripLastDir( outputModGamedirName, sizeof( outputModGamedirName ) );									// steamapps\username
		Q_StripLastDir( outputModGamedirName, sizeof( outputModGamedirName ) );									// steamapps
		Q_AppendSlash( outputModGamedirName, sizeof( outputModGamedirName ) );									
		Q_strncat( outputModGamedirName, "SourceMods", sizeof( outputModGamedirName ), COPY_ALL_CHARACTERS );	// steamapps\sourcemods
		Q_AppendSlash( outputModGamedirName, sizeof( outputModGamedirName ) );									
		Q_strncat( outputModGamedirName, strippedModName, sizeof( outputModGamedirName ), COPY_ALL_CHARACTERS );		// steamapps\sourcemods\modname
		Q_AppendSlash( outputModGamedirName, sizeof( outputModGamedirName ) );

		if ( !CreateFullDirectory( outputModGamedirName ) ||
			 !CreateSubdirectory( outputModGamedirName, "resource" ) ||
			 !CreateSubdirectory( outputModGamedirName, "resource\\ui" ) ||
			 !CreateSubdirectory( outputModGamedirName, "maps" ) ||
			 !CreateSubdirectory( outputModGamedirName, "cfg" ) ||
			 !CreateSubdirectory( outputModGamedirName, "scripts" )
			 )
		{
			VGUIMessageBox( this, "Error", "Unable to create directory '%s' or one if its subdirectories.", modPath );
			return false;
		}
	}

	
	// Setup all the data for the copy panel.
	CModWizardSubPanel_CopyFiles *pCopyPanel = dynamic_cast<CModWizardSubPanel_CopyFiles *>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_CopyFiles"));
	if ( !pCopyPanel )
	{
		VGUIMessageBox( this, "Error", "Can't find copy panel!" );
		return false;			
	}
	
	pCopyPanel->GetReady( modPath, outputModGamedirName, modName );
	return true;
}

void CModWizardSubPanel_GetModInfo::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "SearchButton" ) == 0 )
	{
		CModalPreserveDirectorySelectDialog *pDlg = vgui::SETUP_PANEL( new CModalPreserveDirectorySelectDialog( this, "#SelectInstallDirectory" ) );
		pDlg->SetStartDirectory( "C:\\" );
		char szPath[MAX_PATH];
		m_pModPath->GetText( szPath, sizeof(szPath) );
		pDlg->ExpandTreeToPath( szPath );
		pDlg->SetDefaultCreateDirectoryName( "MyMod" );
		pDlg->MoveToCenterOfScreen();
		pDlg->DoModal();
		pDlg->AddActionSignalTarget( this );
	}

	BaseClass::OnCommand( command );
}

void CModWizardSubPanel_GetModInfo::OnChooseDirectory( const char *dir )
{
	m_pModPath->SetText( dir );
}

