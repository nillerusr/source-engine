#include "cbase.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/TextImage.h>
#include "FontTestPanel.h"
#include "iclientmode.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


FontTestPanel::FontTestPanel(vgui::Panel *parent, const char *name) :
	vgui::Panel(parent, name)
{	
	for (int i=0;i<ASW_FONT_TEST_LABELS;i++)
	{
		m_pLabel[i] = new vgui::Label(this, "FontLabel", "Alien Swarm: 12345 MEOW");
		m_pLabel[i]->SetContentAlignment(vgui::Label::a_northwest);
	}
	SetMouseInputEnabled(true);
	m_iPage = 0;
}

FontTestPanel::~FontTestPanel()
{

}

void FontTestPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled(true);
	SetPaintBackgroundType(0);
	SetBgColor(Color(0,0,0,255));

	for (int i=0;i<ASW_FONT_TEST_LABELS;i++)
	{
		m_pLabel[i]->SetFgColor(pScheme->GetColor("White", Color(255,255,255,255)));
	}
	int k=0;
	SetLabelFont(pScheme, k++, "Default", "Default 123456 NO", false);
	SetLabelFont(pScheme, k++, "Default", "Default proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "DefaultUnderline", "DefaultUnderline 123456 NO", false);
	SetLabelFont(pScheme, k++, "DefaultUnderline", "DefaultUnderline proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "DefaultSmall", "DefaultSmall 123456 NO", false);
	SetLabelFont(pScheme, k++, "DefaultSmall", "DefaultSmall proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "DefaultMedium", "DefaultMedium 123456 NO", false);
	SetLabelFont(pScheme, k++, "DefaultMedium", "DefaultMedium proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "DefaultVerySmall", "DefaultVerySmall 123456 NO", false);
	SetLabelFont(pScheme, k++, "DefaultVerySmall", "DefaultVerySmall proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "DefaultLarge", "DefaultLarge 123456 NO", false);
	SetLabelFont(pScheme, k++, "DefaultLarge", "DefaultLarge proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "DefaultExtraLarge", "DefaultExtraLarge 123456 NO", false);
	SetLabelFont(pScheme, k++, "DefaultExtraLarge", "DefaultExtraLarge proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "DefaultLarge", "DefaultLarge 123456 NO", false);
	SetLabelFont(pScheme, k++, "DefaultLarge", "DefaultLarge proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "Sansman8", "Sansman8 123456 NO", false);
	SetLabelFont(pScheme, k++, "Sansman8", "Sansman8 proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "Sansman10", "Sansman10 123456 NO", false);
	SetLabelFont(pScheme, k++, "Sansman10", "Sansman10 proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "Sansman12", "Sansman12 123456 NO", false);
	SetLabelFont(pScheme, k++, "Sansman12", "Sansman12 proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "Sansman14", "Sansman14 123456 NO", false);
	SetLabelFont(pScheme, k++, "Sansman14", "Sansman14 proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "Sansman16", "Sansman16 123456 NO", false);
	SetLabelFont(pScheme, k++, "Sansman16", "Sansman16 proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "Sansman18", "Sansman18 123456 NO", false);
	SetLabelFont(pScheme, k++, "Sansman18", "Sansman18 proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "Sansman20", "Sansman20 123456 NO", false);
	SetLabelFont(pScheme, k++, "Sansman20", "Sansman20 proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "CleanHUD", "CleanHUD 123456 NO", false);
	SetLabelFont(pScheme, k++, "CleanHUD", "CleanHUD proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "Courier", "Courier 123456 NO", false);
	SetLabelFont(pScheme, k++, "Courier", "Courier proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "SmallCourier", "SmallCourier 123456 NO", false);
	SetLabelFont(pScheme, k++, "SmallCourier", "SmallCourier proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "Verdana", "Verdana 123456 NO", false);
	SetLabelFont(pScheme, k++, "Verdana", "Verdana proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "VerdanaSmall", "VerdanaSmall 123456 NO", false);
	SetLabelFont(pScheme, k++, "VerdanaSmall", "VerdanaSmall proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "CampaignTitle", "CampaignTitle 123456 NO", false);
	SetLabelFont(pScheme, k++, "CampaignTitle", "CampaignTitle proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "MainMenu", "MainMenu 123456 NO", false);
	SetLabelFont(pScheme, k++, "MainMenu", "MainMenu proportional 123456 YES", true);
	SetLabelFont(pScheme, k++, "MissionChooserFont", "MissionChooserFont 123456 NO", false);
	SetLabelFont(pScheme, k++, "MissionChooserFont", "MissionChooserFont proportional 123456 YES", true);
}

void FontTestPanel::SetLabelFont(vgui::IScheme *pScheme, int i, const char* font, const char* text, bool bProportional)
{
	char buffer[256];
	Q_snprintf(buffer, sizeof(buffer), "%s %s %dx%d", font, bProportional ? "Proportional" : "", ScreenWidth(), ScreenHeight());
	m_pLabel[i]->SetText(buffer);
	m_pLabel[i]->SetFont(pScheme->GetFont(font, bProportional));
}

void FontTestPanel::OnMouseReleased(vgui::MouseCode code)
{
	if (code == MOUSE_LEFT)
	{
		m_iPage = 1 - m_iPage;
	}
	else
	{
		MarkForDeletion();
		SetVisible(false);
	}
}

void FontTestPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int iStart = 0;
	if (m_iPage == 1)
	{
		iStart = ASW_FONT_TEST_LABELS * 0.5f;
		for (int i=0;i<ASW_FONT_TEST_LABELS;i++)
		{
			m_pLabel[i]->SetVisible(i >= iStart);
		}
	}
	else
	{
		for (int i=0;i<ASW_FONT_TEST_LABELS;i++)
		{
			m_pLabel[i]->SetVisible(true);
		}
	}
	
	SetBounds(0, 0, ScreenWidth(), ScreenHeight());
	int ypos = 0;
	int padding = 4;
	for (int i=iStart;i<ASW_FONT_TEST_LABELS;i+=2)
	{
		int x, y;
		m_pLabel[i]->GetTextImage()->SetDrawWidth(ScreenWidth() * 0.5f);
		m_pLabel[i]->InvalidateLayout(true);
		m_pLabel[i]->GetTextImage()->GetContentSize(x, y);
		m_pLabel[i]->SetBounds(0, ypos, ScreenWidth() * 0.5f, y);

		m_pLabel[i+1]->GetTextImage()->SetDrawWidth(ScreenWidth() * 0.5f);
		m_pLabel[i+1]->InvalidateLayout(true);
		m_pLabel[i+1]->GetTextImage()->GetContentSize(x, y);
		m_pLabel[i+1]->SetBounds(ScreenWidth() * 0.5f, ypos, ScreenWidth() * 0.5f, y);

		ypos += y + padding;
	}
}

void FontTestPanel::OnThink()
{
	BaseClass::OnThink();
	InvalidateLayout();
}

void ShowFontTest()
{
	vgui::Panel *pPanel = GetClientMode()->GetViewport()->FindChildByName("FontTest", true);
	if (pPanel)
	{
		pPanel->MarkForDeletion();
		pPanel->SetVisible(false);
		return;
	}

	FontTestPanel* pFontTest = new FontTestPanel(GetClientMode()->GetViewport(), "FontTest");
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	pFontTest->SetScheme(scheme);
}
static ConCommand asw_fonts("asw_fonts", ShowFontTest, "Shows all AS:I fonts", FCVAR_CHEAT);