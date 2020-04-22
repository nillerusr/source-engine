//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "ModWizard_Intro.h"
#include <vgui_controls/WizardPanel.h>
#include <vgui/ILocalize.h>
#include "ModWizard_GetModInfo.h"
#include "filesystem_tools.h"
#include "SourceAppInfo.h"
#include "steam/steam_api.h"

using namespace vgui;

extern CSteamAPIContext *steamapicontext;

extern char g_engineDir[50];

//-----------------------------------------------------------------------------
// Purpose: Determines whether the specified game is subscribed to
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool IsGameSubscribed( int nSteamAppId )
{
   bool bIsSubscribed = false;

	if ( g_pFullFileSystem != NULL && g_pFullFileSystem->IsSteam() && steamapicontext->SteamApps() )
	{
		// See if specified app is installed
		bIsSubscribed = steamapicontext->SteamApps()->BIsSubscribedApp( nSteamAppId );
	}

	return bIsSubscribed;
}


CModWizardSubPanel_Intro::CModWizardSubPanel_Intro( Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
	m_pModHL2Button = new RadioButton( this, "ModHL2Button", "" );
	m_pModHL2MPButton = new RadioButton( this, "ModHL2MPButton", "" );
	m_pModFromScratchButton = new RadioButton( this, "ModFromScratchButton", "" );
	m_pSourceCodeOnlyButton = new RadioButton( this, "ModSourceCodeOnlyButton", "" );

	LoadControlSettings( "ModWizardSubPanel_Intro.res");

	// We presently don't support SDK scratch configuration using the "orangebox" codebase
	if ( !V_strcmp( g_engineDir, "orangebox" ) )
	{
		m_pModFromScratchButton->SetVisible( false );
	}

	//
	// Enable and select the appropriate mod types 
	//
	if ( !V_strcmp( g_engineDir, "orangebox" ) || !V_strcmp( g_engineDir, "source2007" ) )
	   m_pModHL2Button->SetEnabled( IsGameSubscribed( GetAppSteamAppId( k_App_HL2_EP2 ) ) );
	else
	{
		// do it without nesting.
		if ( ( IsGameSubscribed( GetAppSteamAppId( k_App_HL2 ) ) || IsGameSubscribed( GetAppSteamAppId( k_App_HL2_EP1 ) ) ) )
			m_pModHL2Button->SetEnabled( true );
		else
			m_pModHL2Button->SetEnabled( false );
	}

	m_pModHL2MPButton->SetEnabled( IsGameSubscribed( GetAppSteamAppId( k_App_HL2MP ) ) );

   // De-select all
   m_pModHL2Button->SetSelected( false );
   m_pModHL2MPButton->SetSelected( false );
   m_pModFromScratchButton->SetSelected( false );

   // Select the most common possible option
   if ( m_pModHL2Button->IsEnabled() )
   {
      m_pModHL2Button->SetSelected( true );
   }
   else if ( m_pModHL2MPButton->IsEnabled() )
   {
      m_pModHL2Button->SetSelected( true );
   }
   else if ( m_pModFromScratchButton->IsVisible() )
   {
	  m_pModFromScratchButton->SetSelected( true );
   }
}

WizardSubPanel *CModWizardSubPanel_Intro::GetNextSubPanel()
{
	return dynamic_cast<WizardSubPanel *>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_GetModInfo"));
}

void CModWizardSubPanel_Intro::PerformLayout()
{
	BaseClass::PerformLayout();

	// Update the scratch button text - in orange box, the text is different, but it's the same button.
	if ( !V_strcmp( g_engineDir, "source2007" ) )
		SetDialogVariable( "ScratchLabel", g_pVGuiLocalize->Find( "#StartFromTemplate" ) );
	else
		SetDialogVariable( "ScratchLabel", g_pVGuiLocalize->Find( "#StartFromScratch" ) );

	GetWizardPanel()->SetFinishButtonEnabled(false);
}

void CModWizardSubPanel_Intro::OnDisplayAsNext()
{
	GetWizardPanel()->SetTitle( "#ModWizard_Intro_Title", true );
}


ModType_t CModWizardSubPanel_Intro::GetModType()
{
	if ( m_pModHL2Button && m_pModHL2Button->IsSelected() )
	{
		return ModType_HL2;
	}
	else if ( m_pModHL2MPButton && m_pModHL2MPButton->IsSelected() )
	{
		return ModType_HL2_Multiplayer;
	}
	else if ( m_pSourceCodeOnlyButton && m_pSourceCodeOnlyButton->IsSelected() )
	{
		return ModType_SourceCodeOnly;
	}
	else
	{
		return ModType_FromScratch;
	}
}


bool CModWizardSubPanel_Intro::OnNextButton()
{
	// If we're only installing the source code, then we don't care about the game directory.
	bool bShowModName = true;
	if ( GetModType() == ModType_SourceCodeOnly )
		bShowModName = false;

	CModWizardSubPanel_GetModInfo *pNextPanel = dynamic_cast<CModWizardSubPanel_GetModInfo*>(GetWizardPanel()->FindChildByName("CModWizardSubPanel_GetModInfo"));
	if ( pNextPanel )
	{
		if ( pNextPanel->m_pModName )
			pNextPanel->m_pModName->SetVisible( bShowModName );
		
		if ( pNextPanel->m_pModNameInfoLabel )
			pNextPanel->m_pModNameInfoLabel->SetVisible( bShowModName );
	
		pNextPanel->m_ModType = GetModType();
	}

	return true;
}
