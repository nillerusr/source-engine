#include "cbase.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include <vgui/ischeme.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/ImagePanel.h>
#include "convar.h"
#include "asw_difficulty_chooser.h"
#include <vgui/isurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

vgui::DHANDLE<CASW_Difficulty_Chooser> g_hDifficultyFrame;

CASW_Difficulty_Chooser::CASW_Difficulty_Chooser( vgui::Panel *pParent, const char *pElementName,
															const char *szLaunchCommand) :
	Frame(pParent, pElementName)
{
	Q_snprintf(m_szLaunchCommand, sizeof(m_szLaunchCommand), "%s", szLaunchCommand);

	m_pCancelButton = new vgui::Button(this, "CancelButton", "#asw_chooser_close", this, "Cancel");

	for (int i=0;i<ASW_NUM_DIFFICULTIES;i++)
	{
		m_pDifficulty[i] = new CASW_Difficulty_Entry(this, "DifficultyEntry", i+1);
		m_pDifficulty[i]->SetVisible(true);
	}

	g_hDifficultyFrame = this;
	InvalidateLayout();
}

CASW_Difficulty_Chooser::~CASW_Difficulty_Chooser()
{

}

void CASW_Difficulty_Chooser::PerformLayout()
{
	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );	
	
	int wide = sw * 0.5f;
	int tall = sh * 0.5f;

	SetSize(wide, tall);
	SetPos((sw * 0.5f) - (wide * 0.5f), (sh * 0.5f) - (tall * 0.5f));

	BaseClass::PerformLayout();
	
	int x, y, cwide, ctall;
	GetClientArea(x, y, cwide, ctall);

	float fScale = float(sh) / 768.0f;
	int iFooterSize = 32 * fScale;	

	int iEntryHeight = (ctall - iFooterSize * 2) / ASW_NUM_DIFFICULTIES;

	m_pCancelButton->SetTextInset(6.0f * fScale, 0);
	for (int i=0;i<ASW_NUM_DIFFICULTIES;i++)
	{
		if (!m_pDifficulty[i])
			continue;
		int inset = 0;
		m_pDifficulty[i]->SetBounds(x + inset, y + (iEntryHeight*i) + (iFooterSize * 0.5f), cwide - (inset * 2), iEntryHeight);
		m_pDifficulty[i]->InvalidateLayout();		
	}

	m_pCancelButton->GetTextImage()->ResizeImageToContent();
	m_pCancelButton->SizeToContents();
	int cancel_wide = m_pCancelButton->GetWide();
	m_pCancelButton->SetSize(cancel_wide, iFooterSize - 5 * fScale);
	m_pCancelButton->SetPos(GetWide() - (cancel_wide + 5 * fScale),
							GetTall() - iFooterSize);
}

void CASW_Difficulty_Chooser::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);	
	vgui::HFont defaultfont = pScheme->GetFont( "DefaultButton", IsProportional() );
	m_pCancelButton->SetFont(defaultfont);
	SetPaintBackgroundType(2);
}

void CASW_Difficulty_Chooser::OnCommand(const char* command)
{
	if (!stricmp(command, "Cancel"))
	{
		SetVisible(false);
		MarkForDeletion();
	}
}

void CASW_Difficulty_Chooser::DifficultyEntryClicked(int iDifficulty)
{
	if (iDifficulty < 1 || iDifficulty > ASW_NUM_DIFFICULTIES)
		return;

	char szCommand[1024];
	Q_snprintf(szCommand, sizeof(szCommand), "disconnect\nwait\nwait\nwait\nasw_skill %d\n%s", iDifficulty, m_szLaunchCommand);
	SetVisible(false);
	MarkForDeletion();
	engine->ClientCmd(szCommand);
}

void CASW_Difficulty_Chooser::OnThink()
{
	BaseClass::OnThink();
}

// ==============================================================================

CASW_Difficulty_Entry::CASW_Difficulty_Entry( vgui::Panel *pParent, const char *pElementName, int iDifficulty)
	: vgui::Panel(pParent, pElementName)
{
	m_pDifficultyDescriptionLabel = new vgui::Label(this, "Label", " ");
	m_pDifficultyDescriptionLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pDifficultyDescriptionLabel->SetWrap(true);
	m_pDifficultyDescriptionLabel->InvalidateLayout();
	m_pDifficultyDescriptionLabel->SetMouseInputEnabled(false);

	m_pDifficultyLabel = new vgui::Label(this, "Label", " ");
	m_pDifficultyLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pDifficultyLabel->InvalidateLayout();
	m_pDifficultyLabel->SetMouseInputEnabled(false);

	m_pImagePanel = new vgui::ImagePanel(this, "EntryImage");	
	m_pImagePanel->SetShouldScaleImage(true);
	m_pImagePanel->SetVisible(true);
	m_pImagePanel->SetMouseInputEnabled(false);

	m_iDifficulty = iDifficulty;

	switch (m_iDifficulty)
	{
	case 1: m_pDifficultyLabel->SetText("#asw_difficulty_chooser_easy"); break;
	case 2: m_pDifficultyLabel->SetText("#asw_difficulty_chooser_normal"); break;
	case 3: m_pDifficultyLabel->SetText("#asw_difficulty_chooser_hard"); break;
	case 4: m_pDifficultyLabel->SetText("#asw_difficulty_chooser_insane"); break;
	case 5: m_pDifficultyLabel->SetText("#asw_difficulty_chooser_imba"); break;
	default: m_pDifficultyLabel->SetText("???"); break;
	}

	switch (m_iDifficulty)
	{
	case 1: m_pDifficultyDescriptionLabel->SetText("#asw_difficulty_chooser_easyd"); break;
	case 2: m_pDifficultyDescriptionLabel->SetText("#asw_difficulty_chooser_normald"); break;
	case 3: m_pDifficultyDescriptionLabel->SetText("#asw_difficulty_chooser_hardd"); break;
	case 4: m_pDifficultyDescriptionLabel->SetText("#asw_difficulty_chooser_insaned"); break;
	case 5: m_pDifficultyDescriptionLabel->SetText("#asw_difficulty_chooser_imbad"); break;
	default: m_pDifficultyDescriptionLabel->SetText("???"); break;
	}

	switch (m_iDifficulty)
	{
	case 1: m_pImagePanel->SetImage("swarm/MissionPics/DifficultyPicEasy"); break;
	case 2: m_pImagePanel->SetImage("swarm/MissionPics/DifficultyPicNormal"); break;
	case 3: m_pImagePanel->SetImage("swarm/MissionPics/DifficultyPicHard"); break;
	case 4: m_pImagePanel->SetImage("swarm/MissionPics/DifficultyPicInsane"); break;
	case 5: m_pImagePanel->SetImage("swarm/MissionPics/DifficultyPicInsane"); break;
	default: m_pImagePanel->SetImage("swarm/MissionPics/UnknownMissionPic"); break;
	}

	

	m_hDefaultFont = vgui::INVALID_FONT;
}

CASW_Difficulty_Entry::~CASW_Difficulty_Entry()
{

}

void CASW_Difficulty_Entry::PerformLayout()
{
	BaseClass::PerformLayout();

	int image_tall = GetTall() * 0.9f;
	int image_wide = image_tall * 1.3333f;
	m_pImagePanel->SetBounds(GetTall() * 0.05f, GetTall() * 0.05f, image_wide, image_tall);

	int font_tall = vgui::surface()->GetFontTall(m_hDefaultFont);
	m_pDifficultyLabel->SetBounds(image_wide * 1.2f, 0, GetWide() - (image_wide * 1.2f), font_tall);
	m_pDifficultyLabel->InvalidateLayout(true);
	m_pDifficultyLabel->GetTextImage()->RecalculateNewLinePositions();

	m_pDifficultyDescriptionLabel->SetBounds(image_wide * 1.2f, font_tall, GetWide() - (image_wide * 1.2f), GetTall() - font_tall);
	m_pDifficultyDescriptionLabel->InvalidateLayout(true);
	m_pDifficultyDescriptionLabel->GetTextImage()->RecalculateNewLinePositions();	
}

void CASW_Difficulty_Entry::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pDifficultyDescriptionLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pDifficultyDescriptionLabel->InvalidateLayout();
	vgui::HFont defaultfont = pScheme->GetFont( "MissionChooserFont", IsProportional() );	
	m_pDifficultyDescriptionLabel->SetFont(defaultfont);
	m_pDifficultyDescriptionLabel->InvalidateLayout();
	m_pDifficultyDescriptionLabel->GetTextImage()->RecalculateNewLinePositions();
	m_pDifficultyDescriptionLabel->SetFgColor(Color(160,160,160,255));

	m_pDifficultyLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pDifficultyLabel->InvalidateLayout();
	m_hDefaultFont = pScheme->GetFont( "MissionChooserFont", IsProportional() );	
	m_pDifficultyLabel->SetFont(m_hDefaultFont);
	m_pDifficultyLabel->InvalidateLayout();
	m_pDifficultyLabel->GetTextImage()->RecalculateNewLinePositions();
	m_pDifficultyLabel->SetFgColor(Color(255,255,255,255));
}

void CASW_Difficulty_Entry::OnMouseReleased(vgui::MouseCode code)
{
	if ( code == MOUSE_LEFT )
	{
		CASW_Difficulty_Chooser *pChooser = dynamic_cast<CASW_Difficulty_Chooser*>(GetParent());
		if (pChooser)
		{
			pChooser->DifficultyEntryClicked(m_iDifficulty);
		}
	}
}

void CASW_Difficulty_Entry::OnThink()
{
	bool bCursorOver = IsCursorOver();
	if (m_bMouseOver != bCursorOver)
	{
		m_bMouseOver = bCursorOver;
		if (m_bMouseOver)
		{
			SetBgColor(Color(75,84,106,255));
		}
		else
		{
			SetBgColor(Color(0,0,0,0));
		}
	}	
}