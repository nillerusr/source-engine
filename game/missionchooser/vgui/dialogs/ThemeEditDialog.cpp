#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/CheckButton.h>
#include <KeyValues.h>

#include "TileGenDialog.h"
#include "ThemeEditDialog.h"
#include "TileSource/LevelTheme.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CThemeEditDialog *g_pThemeEditDialog = NULL;

CThemeEditDialog::CThemeEditDialog( Panel *parent, const char *name, CLevelTheme *pTheme, bool bCreatingNew ) : BaseClass( parent, name )
{
	Assert( !g_pThemeEditDialog );
	g_pThemeEditDialog = this;
	m_bCreatingNew = bCreatingNew;

	m_pLevelTheme = pTheme;
	m_pThemeNameEdit = new vgui::TextEntry(this, "ThemeNameEdit");
	m_pThemeDescriptionEdit = new vgui::TextEntry(this, "ThemeDescriptionEdit");
	m_pThemeAmbientEdit = new vgui::TextEntry(this, "ThemeAmbientEdit");
	m_pVMFTweakCheck = new vgui::CheckButton( this, "VMFTweakCheck", "VMF Tweak" );

	if (!bCreatingNew)
	{
		m_pThemeNameEdit->SetText(pTheme->m_szName);
		m_pThemeDescriptionEdit->SetText(pTheme->m_szDescription);
		char buffer[128];
		Q_snprintf( buffer, sizeof( buffer ), "%d %d %d", (int) pTheme->m_vecAmbientLight.x, (int) pTheme->m_vecAmbientLight.y, (int) pTheme->m_vecAmbientLight.z );
		m_pThemeAmbientEdit->SetText( buffer );
		m_pVMFTweakCheck->SetSelected(pTheme->m_bRequiresVMFTweak);
	}

	SetSize(384, 420);
	SetMinimumSize(200, 50);

	SetMinimizeButtonVisible( false );
	SetCloseButtonVisible( false );
		
	LoadControlSettings( "tilegen/ThemeEditDialog.res", "GAME" );

	SetDeleteSelfOnClose( true );

	SetSizeable( false );
	MoveToCenterOfScreen();
}

CThemeEditDialog::~CThemeEditDialog( void )
{
	g_pThemeEditDialog = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Handles dialog commands
//-----------------------------------------------------------------------------
void CThemeEditDialog::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "Okay" ) == 0 )
	{
		// check the name is filled in
		char name[64];
		m_pThemeNameEdit->GetText(name, sizeof(name));
		if (Q_strlen(name) <= 0)
		{
			MessageBox *pMessage = new MessageBox ("Bad Theme Name", "Please enter a valid theme name", this);
			pMessage->DoModal();
			return;
		}

		m_pThemeNameEdit->GetText(m_pLevelTheme->m_szName, sizeof(m_pLevelTheme->m_szName));
		m_pThemeDescriptionEdit->GetText(m_pLevelTheme->m_szDescription, sizeof(m_pLevelTheme->m_szDescription));
		char buffer[128];
		m_pThemeAmbientEdit->GetText( buffer, sizeof( buffer ) );

		sscanf( buffer, "%f %f %f", &m_pLevelTheme->m_vecAmbientLight.x, &m_pLevelTheme->m_vecAmbientLight.y, &m_pLevelTheme->m_vecAmbientLight.z );
		// temp to avoid the 0,0,0 losing ambient light bug
		if ( m_pLevelTheme->m_vecAmbientLight == vec3_origin )
		{
			m_pLevelTheme->m_vecAmbientLight.x = 1;
			m_pLevelTheme->m_vecAmbientLight.y = 1;
			m_pLevelTheme->m_vecAmbientLight.z = 1;
		}
		m_pLevelTheme->m_bRequiresVMFTweak = m_pVMFTweakCheck->IsSelected();

		if (m_bCreatingNew)
		{
			CLevelTheme::s_LevelThemes.AddToTail(m_pLevelTheme);
		}
		if ( !m_pLevelTheme->SaveTheme(m_pLevelTheme->m_szName) )
		{
			VGUIMessageBox( this, "Save Error", "Failed to save %s.theme.  Make sure file is checked out from Perforce.", m_pLevelTheme->m_szName );
			return;
		}
		PostActionSignal(new KeyValues("ThemeChanged", "themename", m_pLevelTheme->m_szName));
		OnClose();
	}
	else if (Q_stricmp( command, "Close" ) == 0 )
	{
		if (m_bCreatingNew)
		{
			delete m_pLevelTheme;
			m_pLevelTheme = NULL;
			CLevelTheme::SetCurrentTheme(CLevelTheme::s_pPreviousTheme);
		}
	}
	BaseClass::OnCommand( command );
}