//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================


// Valve includes
#include "itemtest/itemtest_controls.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui/ISystem.h"
#include "vgui_controls/MessageBox.h"
#include "tier1/fmtstr.h"


// Last include
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CFinalSubPanel::CFinalSubPanel( vgui::Panel *pParent, const char *pszName, const char *pszNextName )
: BaseClass( pParent, pszName, pszNextName )
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return;

	{
		m_pHLMVButton = new vgui::Button( this, "hlmvButton", "#hlmvButton", this, "Hlmv" );
		m_pHLMVLabel = new vgui::Label( this, "hlmvLabel", "#hlmvLabel" );

		CFmtStrMax sCmd;
		if ( GetHlmvCmd( sCmd ) )
		{
			m_pHLMVLabel->SetText( sCmd.Access() );
		}

		m_pPanelListPanel->AddItem( m_pHLMVButton, m_pHLMVLabel );
		m_pHLMVButton->AddActionSignalTarget( this );
	}

	{
		m_pExploreMaterialContentButton = new vgui::Button( this, "exploreMaterialContentButton", "#exploreMaterialContentButton", this, "ExploreMaterialContent" );
		m_pExploreMaterialContentLabel = new vgui::Label( this, "exploreMaterialContentLabel", "#exploreMaterialContentLabel" );

		m_pPanelListPanel->AddItem( m_pExploreMaterialContentButton, m_pExploreMaterialContentLabel );
	}

	{
		m_pExploreModelContentButton = new vgui::Button( this, "exploreModelContentButton", "#exploreModelContentButton", this, "ExploreModelContent" );
		m_pExploreModelContentLabel = new vgui::Label( this, "exploreModelContentLabel", "#exploreModelContentLabel" );

		m_pPanelListPanel->AddItem( m_pExploreModelContentButton, m_pExploreModelContentLabel );
	}

	{
		m_pExploreMaterialGameButton = new vgui::Button( this, "exploreMaterialGameButton", "#exploreMaterialGameButton", this, "ExploreMaterialGame" );
		m_pExploreMaterialGameLabel = new vgui::Label( this, "exploreMaterialGameLabel", "#exploreMaterialGameLabel" );

		m_pPanelListPanel->AddItem( m_pExploreMaterialGameButton, m_pExploreMaterialGameLabel );
	}

	{
		m_pExploreModelGameButton = new vgui::Button( this, "exploreModelGameButton", "#exploreModelGameButton", this, "ExploreModelGame" );
		m_pExploreModelGameLabel = new vgui::Label( this, "exploreModelGameLabel", "#exploreModelGameLabel" );

		m_pPanelListPanel->AddItem( m_pExploreModelGameButton, m_pExploreModelGameLabel );
	}

	UpdateStatus();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFinalSubPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pPanelListPanel->SetFirstColumnWidth( 256 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFinalSubPanel::UpdateGUI()
{
	UpdateStatus();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFinalSubPanel::OnCommand( const char *command )
{
	if ( !V_strcmp( command, "Gather" ) )
	{
		OnGather();
	}
	else if ( !V_strcmp( command, "StudioMDL" ) )
	{
		OnStudioMDL();
	}
	else if ( !V_strcmp( command, "Zip" ) )
	{
		OnZip();
	}
	else if ( !V_strcmp( command, "Hlmv" ) )
	{
		OnHlmv();
	}
	else if ( !V_strcmp( command, "ExploreMaterialContent" ) )
	{
		OnExplore( true, true );
	}
	else if ( !V_strcmp( command, "ExploreModelContent" ) )
	{
		OnExplore( false, true );
	}
	else if ( !V_strcmp( command, "ExploreMaterialGame" ) )
	{
		OnExplore( true, false );
	}
	else if ( !V_strcmp( command, "ExploreModelGame" ) )
	{
		OnExplore( false, false );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CFinalSubPanel::UpdateStatus()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return false;

	SetStatus( true, "", NULL, true );

	CAsset &asset = pItemUploadWizard->Asset();

	{
		char szBuf[ MAX_PATH ];

		CUtlString sDir;

		asset.GetAbsoluteDir( sDir, "materialsrc", true );
		V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sDir.Get() );
		m_pExploreMaterialContentLabel->SetText( szBuf );

		asset.GetAbsoluteDir( sDir, "materials", false );
		V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sDir.Get() );
		m_pExploreMaterialGameLabel->SetText( szBuf );

		asset.GetAbsoluteDir( sDir, NULL, true );
		V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sDir.Get() );
		m_pExploreModelContentLabel->SetText( szBuf );

		asset.GetAbsoluteDir( sDir, NULL, false );
		V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sDir.Get() );
		m_pExploreModelGameLabel->SetText( szBuf );

		CFmtStrMax sCmd;
		if ( GetHlmvCmd( sCmd ) )
		{
			m_pHLMVLabel->SetText( sCmd.Access() );
		}
	}

	return BaseClass::UpdateStatus();

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFinalSubPanel::OnGather()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
	{
//		m_pGatherLabel->SetText( "Gather Failed" );
		return;
	}

//	m_pGatherLabel->SetText( "OnGather" );

	m_pExploreMaterialContentButton->SetEnabled( true );
	m_pExploreModelContentButton->SetEnabled( true );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFinalSubPanel::OnStudioMDL()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
	{
//		m_pStudioMDLLabel->SetText( "StudioMDL Failed" );
		return;
	}

	OnGather();

//	m_pStudioMDLLabel->SetText( "OnStudioMDL" );
}


//-----------------------------------------------------------------------------
//
// The idea is that there could be three steps...
// 1. Gather data from user specified sources into properly named & organized content, make VMT's & QC
// 2. Run StudioMDL on the gathered content
// 3. Zip the results
//
// Right now all three steps are combined
//-----------------------------------------------------------------------------
void CFinalSubPanel::OnZip()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );

	CAsset &asset = pItemUploadWizard->Asset();
	const bool bOk = asset.Compile();

	vgui::MessageBox *pMsgDialog = NULL;
	
	if ( bOk )
	{
		pMsgDialog = new vgui::MessageBox( "Compile Ok", "Compile Ok", this );
	}
	else
	{
		pMsgDialog = new vgui::MessageBox( "Compile Failed", "Compile Failed", this );
	}

	pMsgDialog->SetOKButtonVisible( true );
	pMsgDialog->SetOKButtonText( "Quit ItemTest" );
	pMsgDialog->SetCommand( new KeyValues( "QuitApp" ) );

	pMsgDialog->SetCancelButtonVisible( true );
	pMsgDialog->SetCancelButtonText( "Dismiss And Continue" );

	pMsgDialog->AddActionSignalTarget( this );

	pMsgDialog->DoModal();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFinalSubPanel::OnHlmv()
{
	CUtlString sBinDir;
	if ( !CItemUpload::GetBinDirectory( sBinDir ) )
	{
		m_pHLMVLabel->SetText( "Cannot determine bin directory" );
		return;
	}

	char szBinLaunchDir[ MAX_PATH ];
	V_ExtractFilePath( sBinDir.String(), szBinLaunchDir, ARRAYSIZE( szBinLaunchDir ) );

	CFmtStrMax sCmd;
	if ( !GetHlmvCmd( sCmd ) )
		return;

	if ( CItemUpload::RunCommandLine( sCmd.Access(), szBinLaunchDir, false ) )
	{
		m_pHLMVLabel->SetText( sCmd.Access() );
	}
	else
	{
		m_pHLMVLabel->SetText( "Hlmv launch failed" );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFinalSubPanel::OnExplore( bool bMaterial, bool bContent )
{
	vgui::Label *pLabel = NULL;
	if ( bMaterial )
	{
		if ( bContent )
		{
			pLabel = m_pExploreMaterialContentLabel;
		}
		else
		{
			pLabel = m_pExploreMaterialGameLabel;
		}
	}
	else
	{
		if ( bContent )
		{
			pLabel = m_pExploreModelContentLabel;
		}
		else
		{
			pLabel = m_pExploreModelGameLabel;
		}
	}

	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
	{
		pLabel->SetText( "Explore game failed" );
		return;
	}

	CAsset &asset = pItemUploadWizard->Asset();
	CUtlString sDir;
	const char *pszPrefix = bMaterial ? ( bContent ? "materialsrc" : "materials" ) : NULL;
	asset.GetAbsoluteDir( sDir, pszPrefix, bContent );

	char szBuf[ MAX_PATH ];
	V_FixupPathName( szBuf, ARRAYSIZE( szBuf ), sDir.Get() );
	vgui::system()->ShellExecute( "open", szBuf );

	pLabel->SetText( szBuf );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFinalSubPanel::OnQuitApp()
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
		return;

	pItemUploadWizard->Close();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CFinalSubPanel::GetHlmvCmd( CFmtStrMax &sHlmvCmd )
{
	CItemUploadWizard *pItemUploadWizard = dynamic_cast< CItemUploadWizard * >( GetWizardPanel() );
	if ( !pItemUploadWizard )
	{
		m_pHLMVLabel->SetText( "Hlmv Failed" );
		return false;
	}

	CUtlString sBinDir;
	if ( !CItemUpload::GetBinDirectory( sBinDir ) )
	{
		m_pHLMVLabel->SetText( "Cannot determine bin directory" );
		return false;
	}

	CAsset &asset = pItemUploadWizard->Asset();
	CSmartPtr< CTargetMDL > pTargetMDL = asset.GetTargetMDL();
	if ( !pTargetMDL.IsValid() )
	{
		m_pHLMVLabel->SetText( "No TargetMDL" );
		return false;
	}

	CUtlString sMdlPath;
	if ( !pTargetMDL->GetOutputPath( sMdlPath ) )
	{
		m_pHLMVLabel->SetText( "Cannot determine path to MDL" );
		return false;
	}


	if ( CItemUpload::GetDevMode() )
	{
		sHlmvCmd.sprintf( "\"%s\\hlmv.exe\" \"%s\"", sBinDir.Get(), sMdlPath.Get() );
	}
	else
	{
		CUtlString sVProjectDir;
		if ( CItemUpload::GetVProjectDir( sVProjectDir ) )
		{
			sHlmvCmd.sprintf( "\"%s\\hlmv.exe\" -allowdebug -game \"%s\" -nop4 \"%s\"", sBinDir.Get(), sVProjectDir.Get(), sMdlPath.Get() );
		}
		else
		{
			m_pHLMVLabel->SetText( "Cannot determine VPROJECT (gamedir)" );
			return false;
		}
	}

	return true;
}