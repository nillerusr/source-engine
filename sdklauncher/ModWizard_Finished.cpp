//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "ModWizard_Finished.h"
#include "CreateModWizard.h"
#include <vgui_controls/WizardPanel.h>
#include <vgui/ILocalize.h>
#include <vgui/ISystem.h>

extern void OpenLocalizedURL( const char *lpszLocalName );

using namespace vgui;

CModWizardSubPanel_Finished::CModWizardSubPanel_Finished( Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )						 
{
	m_pFinishedText = new Label( this, "FinishedText", "" );
	m_pOpenReadme = new CheckButton( this, "OpenReadme", "" );
	
	m_OutputDirName[0] = 0;

	LoadControlSettings( "ModWizardSubPanel_Finished.res");

	m_pOpenReadme->SetSelected( true );
}

void CModWizardSubPanel_Finished::GetReady( const char *pOutputDirName )
{
	wchar_t *formatStr = g_pVGuiLocalize->Find( "ModWizard_FinishedText" );
	if ( formatStr )
	{
		wchar_t tempStr[4096], labelStr[4096];

		Q_strncpy( m_OutputDirName, pOutputDirName, sizeof( m_OutputDirName ) );
		int len = strlen( m_OutputDirName );
		if ( len > 0 )
		{
			if ( m_OutputDirName[len-1] == '/' || m_OutputDirName[len-1] == '\\' )
				m_OutputDirName[len-1] = 0;
		}

		g_pVGuiLocalize->ConvertANSIToUnicode( m_OutputDirName, tempStr, sizeof( tempStr ) );
		g_pVGuiLocalize->ConstructString( labelStr, sizeof( labelStr ), formatStr, 1, tempStr );
		m_pFinishedText->SetText( labelStr );
	}
}

WizardSubPanel *CModWizardSubPanel_Finished::GetNextSubPanel()
{
	return NULL;
}

void CModWizardSubPanel_Finished::PerformLayout()
{
	BaseClass::PerformLayout();
	
	GetWizardPanel()->SetFinishButtonEnabled( true );
	GetWizardPanel()->SetPrevButtonEnabled( false );
}

void CModWizardSubPanel_Finished::OnDisplayAsNext()
{
	GetWizardPanel()->SetTitle( "#ModWizard_Finished_Title", true );
	NoteModWizardFinished();
}

bool CModWizardSubPanel_Finished::OnFinishButton()
{
	if ( m_pOpenReadme->IsSelected() )
	{
		// ShellExecute.. to the site.
		OpenLocalizedURL( "URL_Create_Mod_Finished" );
	}

	return true;
}

