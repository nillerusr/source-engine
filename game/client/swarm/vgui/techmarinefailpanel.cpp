#include "cbase.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "TechMarineFailPanel.h"
#include "iclientmode.h"
#include "vgui/ISurface.h"
#include "c_asw_player.h"
#include "c_asw_game_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

int TechMarineFailPanel::s_iTechPanels = 0;
TechMarineFailPanel *TechMarineFailPanel::s_pTechPanel = NULL;

TechMarineFailPanel::TechMarineFailPanel(vgui::Panel *parent, const char *name) :
	vgui::Panel(parent, name)
{
	//Msg("TechMarineFailPanel panel created\n");
	m_pBackground = new vgui::Panel( this, "Background" );
	m_pFailedMessage = new vgui::Label(this, "FailedMessage", "#asw_mission_failed");
	m_pReasonMessage = new vgui::Label(this, "ReasonMessage", "#asw_no_live_tech");
	m_pIcon = new vgui::ImagePanel(this, "Icon");
	m_pIcon->SetShouldScaleImage(true);
	m_pIcon->SetImage("swarm/EquipIcons/Locked");
	m_bLeader = false;
	m_hFont = vgui::INVALID_FONT;

	s_iTechPanels++;
	s_pTechPanel = this;
}

TechMarineFailPanel::~TechMarineFailPanel()
{
	//Msg("TechMarineFailPanel panel destroyed\n");
	s_iTechPanels--;
	s_pTechPanel = NULL;
}

// static
void TechMarineFailPanel::ShowTechFailPanel()
{
	//Msg("TechMarineFailPanel::ShowTechFailPanel num tech panels = %d\n", s_iTechPanels);
	if (s_iTechPanels <= 0)
	{
		vgui::Panel *pPanel = new TechMarineFailPanel(GetClientMode()->GetViewport(), "TechFail");
		vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
		pPanel->SetScheme(scheme);
	}
}

void TechMarineFailPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "Default", IsProportional() );
	m_pFailedMessage->SetFgColor(pScheme->GetColor("Red", Color(255,0,0,255)));
	m_pFailedMessage->SetFont(m_hFont);
	m_pReasonMessage->SetFgColor(pScheme->GetColor("White", Color(255,255,255,255)));
	m_pReasonMessage->SetFont(m_hFont);	

	m_pBackground->SetPaintBackgroundType( 2 );
	m_pBackground->SetBgColor( Color( 35, 41, 57, 210 ) );
}

void TechMarineFailPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	
	int w, t;
	w = ScreenWidth() * 0.4f;	

	int line_height = vgui::surface()->GetFontTall( m_hFont ) * 1.1f;
	t = line_height * 4;
	SetSize(w, t);
	SetPos(ScreenWidth() * 0.4f, ScreenHeight() * 0.3f);

	int icon_size = line_height * 2;
	int padding = ScreenWidth() * 0.01f;

	m_pIcon->SetBounds(padding,padding,icon_size, icon_size);

	m_pFailedMessage->SetPos(icon_size + padding * 2, padding );
	m_pFailedMessage->SizeToContents();
	m_pReasonMessage->SetPos(icon_size + padding * 2, padding + line_height );
	m_pReasonMessage->SizeToContents();

	m_pBackground->SetSize( icon_size * 3 + padding * 3 + MAX( m_pFailedMessage->GetWide(), m_pReasonMessage->GetWide() ), GetTall() );
}

void TechMarineFailPanel::OnThink()
{
	BaseClass::OnThink();
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer || !ASWGameResource())
		return;

	m_pReasonMessage->SetText("#asw_no_live_tech");
}

