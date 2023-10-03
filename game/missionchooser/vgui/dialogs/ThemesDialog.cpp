#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PanelListPanel.h>
#include "vgui/missionchooser_tgaimagepanel.h"
#include <KeyValues.h>

#include "ThemesDialog.h"
#include "ThemeEditDialog.h"
#include "TileSource/LevelTheme.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CThemesDialog *g_pThemesDialog = NULL;

CThemesDialog::CThemesDialog( Panel *parent, const char *name, bool bGlobal ) : BaseClass( parent, name )
{
	if ( bGlobal )
	{
		Assert( !g_pThemesDialog );
		g_pThemesDialog = this;
	}

	m_pThemePanelList = new vgui::PanelListPanel(this, "ThemePanelListPanel");
	m_pThemePanelList->SetFirstColumnWidth(0);

	SetSize(384, 420);
	SetMinimumSize(200, 50);

	SetMinimizeButtonVisible( false );

	m_pCurrentThemeLabel = new vgui::Label(this, "CurrentThemePanel", "None");
		
	LoadControlSettings( "tilegen/ThemesDialog.res", "GAME" );

	SetDeleteSelfOnClose( true );

	SetSizeable( false );
	MoveToCenterOfScreen();
	PopulateThemeList();
}

CThemesDialog::~CThemesDialog( void )
{
	if ( g_pThemesDialog == this )
	{
		g_pThemesDialog = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles dialog commands
//-----------------------------------------------------------------------------
void CThemesDialog::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "Edit" ) == 0 )
	{
		if (CLevelTheme::s_pCurrentTheme)
		{
			// Launch the theme editing window
			CThemeEditDialog *pDialog = new CThemeEditDialog( this, "ThemeEditDialog", CLevelTheme::s_pCurrentTheme, false );

			pDialog->AddActionSignalTarget( this );
			pDialog->DoModal();
			PopulateThemeList();
		}
	}
	else if ( Q_stricmp( command, "New" ) == 0 )
	{
		CLevelTheme *pTheme = new CLevelTheme("", "", false);
		if (pTheme)
		{
			CLevelTheme::SetCurrentTheme(pTheme);
			if (CLevelTheme::s_pCurrentTheme)
			{
				// Launch the theme editing window
				CThemeEditDialog *pDialog = new CThemeEditDialog( this, "ThemeEditDialog", CLevelTheme::s_pCurrentTheme, true );

				pDialog->AddActionSignalTarget( this );
				pDialog->DoModal();
			}
		}
	}
	BaseClass::OnCommand( command );
}

void CThemesDialog::PopulateThemeList()
{	
	m_pThemePanelList->RemoveAll();

	for (int i=0;i<CLevelTheme::s_LevelThemes.Count();i++)
	{
		CThemeDetails *pEntry = new CThemeDetails(this, "ThemeDetails", this);
		pEntry->SetTheme(CLevelTheme::s_LevelThemes[i]);
		pEntry->m_iDesiredWidth = GetWide() - 55;
		pEntry->InvalidateLayout(true);

		m_pThemePanelList->AddItem(NULL, pEntry);
	}

	if (CLevelTheme::s_pCurrentTheme)
	{
		m_pCurrentThemeLabel->SetText(CLevelTheme::s_pCurrentTheme->m_szName);
	}
	else
	{
		m_pCurrentThemeLabel->SetText("None");
	}
}

void CThemesDialog::OnThemeChanged( KeyValues *params )
{
	PopulateThemeList();
	PostActionSignal(new KeyValues("UpdateCurrentTheme"));	// make the TileGenDialog update with the newly selected theme
}

void CThemesDialog::OnThemeDetailsClicked(vgui::Panel* pPanel)
{
	if (!pPanel)
		return;

	CThemeDetails *pThemeDetails = dynamic_cast<CThemeDetails*>(pPanel);
	if (!pThemeDetails)
	{
		return;
	}
	
	ThemeClicked( pThemeDetails );
}

void CThemesDialog::ThemeClicked( CThemeDetails *pThemeDetails )
{
	CLevelTheme::SetCurrentTheme( pThemeDetails->m_pTheme );
	m_pCurrentThemeLabel->SetText(CLevelTheme::s_pCurrentTheme->m_szName);

	if (m_pThemePanelList)
	{
		int iPanels = m_pThemePanelList->GetItemCount();
		for (int i=0;i<iPanels;i++)
		{
			vgui::Panel* pPanel = m_pThemePanelList->GetItemPanel(i);
			if (pPanel)
			{
				pPanel->InvalidateLayout();
				pPanel->OnThink();
			}
		}
	}
	OnThemeChanged(NULL);
}

bool CThemesDialog::ShouldHighlight( CThemeDetails *pDetails )
{
	return pDetails && ( CLevelTheme::s_pCurrentTheme == pDetails->m_pTheme );
}

// Theme entry ------------------------------------------------------
//   an entry in the list panel above

CThemeDetails::CThemeDetails(vgui::Panel *parent, const char *name, CThemesDialog *pThemesDialog ) : vgui::Panel(parent, name)
{
	m_pThemesDialog = pThemesDialog;
	m_pTheme = NULL;
	m_pTGAImagePanel = new CMissionChooserTGAImagePanel(this, "TGAThemeImage");
	m_pTGAImagePanel->SetMouseInputEnabled(false);
	m_pTGAImagePanel->SetKeyBoardInputEnabled(false);
	m_pNameLabel = new vgui::Label(this, "ThemeName", "Theme");
	m_pNameLabel->SetMouseInputEnabled(false);
	m_pNameLabel->SetKeyBoardInputEnabled(false);
	m_pNameLabel->SetPaintBackgroundEnabled(false);
	m_pDescriptionLabel = new vgui::Label(this, "ThemeDesc", "Desc");
	m_pDescriptionLabel->SetMouseInputEnabled(false);
	m_pDescriptionLabel->SetKeyBoardInputEnabled(false);
	m_pDescriptionLabel->SetWrap(true);
	SetMouseInputEnabled(true);

	m_iDesiredHeight = 80;
	m_iDesiredWidth = 400;
	m_bCurrentTheme = false;
}

CThemeDetails::~CThemeDetails()
{
}

void CThemeDetails::SetTheme(CLevelTheme *pTheme)
{
	if ( m_pTheme == pTheme )
		return;

	m_pTheme = pTheme;
	char buffer[MAX_PATH];
	buffer[0] = 0;
	if ( m_pTheme )
	{
		Q_snprintf(buffer, sizeof(buffer), "tilegen/themes/%s.tga", m_pTheme->m_szName);
	}
	m_pTGAImagePanel->SetTGA(buffer);
	if (!pTheme)
	{
		m_pNameLabel->SetText("No theme selected");
		m_pDescriptionLabel->SetText("");
		return;
	}
	
	m_pNameLabel->SetText(pTheme->m_szName);
	m_pDescriptionLabel->SetText(pTheme->m_szDescription);
}

void CThemeDetails::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	m_pNameLabel->SetPaintBackgroundEnabled(false);
	m_pDescriptionLabel->SetPaintBackgroundEnabled(false);

	if (m_bCurrentTheme)
	{
		//m_pNameLabel->SetBgColor(Color(128,128,128, 255));
		m_pNameLabel->SetPaintBackgroundEnabled(true);
	}
	else
	{
		//SetBgColor(Color(0,0,0, 255));
		m_pNameLabel->SetPaintBackgroundEnabled(false);
	}
}

void CThemeDetails::PerformLayout()
{
	BaseClass::PerformLayout();

	if (!GetParent())
		return;

	SetSize(m_iDesiredWidth, m_iDesiredHeight);
	int w = m_iDesiredWidth;
	
	int image_wide = m_iDesiredHeight * (4.0f / 3.0f);
	m_pTGAImagePanel->SetBounds(0, 0, image_wide, m_iDesiredHeight);
	int font_tall = 24;
	int padding = 8;
	m_pNameLabel->SetBounds(image_wide + padding, 0, w - image_wide + padding, font_tall);	
	m_pDescriptionLabel->SetBounds(image_wide + padding, font_tall, w - image_wide + padding, m_iDesiredHeight - font_tall);
}

void CThemeDetails::OnMouseReleased(vgui::MouseCode code)
{
	PostActionSignal(new KeyValues("ThemeDetailsClicked"));
}

void CThemeDetails::OnThink()
{
	m_bCurrentTheme = ( m_pThemesDialog && m_pThemesDialog->ShouldHighlight( this ) );

	if (m_bCurrentTheme)
	{
		//m_pNameLabel->SetBgColor(Color(128,128,128, 255));
		m_pNameLabel->SetPaintBackgroundEnabled(true);
		Repaint();
	}
	else
	{
		//SetBgColor(Color(0,0,0, 255));
		m_pNameLabel->SetPaintBackgroundEnabled(false);
		Repaint();
	}
}