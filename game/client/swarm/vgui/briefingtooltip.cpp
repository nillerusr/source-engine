#include "cbase.h"
#include "BriefingTooltip.h"
#include "controller_focus.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

vgui::DHANDLE<BriefingTooltip> g_hBriefingTooltip;

BriefingTooltip::BriefingTooltip(Panel *parent, const char *panelName) :
	Panel(parent, panelName)
{
	m_pTooltipPanel = NULL;
	m_pMainLabel = new Label(this, "BriefingTooltipMainLabel", " ");
	m_pMainLabel->SetContentAlignment(vgui::Label::a_center);	
	m_pMainLabel = new Label(this, "BriefingTooltipMainLabel", " ");
	m_pMainLabel->SetContentAlignment(vgui::Label::a_center);
	m_pSubLabel = new Label(this, "BriefingTooltipSubLabel", " ");
	m_pSubLabel->SetContentAlignment(vgui::Label::a_center);
	m_iTooltipX = m_iTooltipY = 0;
	m_bTooltipsEnabled = true;
	m_bAlignTopLeft = false;
}

BriefingTooltip::~BriefingTooltip()
{
	if (g_hBriefingTooltip.Get() == this)
		g_hBriefingTooltip = NULL;
}

void BriefingTooltip::OnThink()
{
	BaseClass::OnThink();
	if (m_pTooltipPanel)
	{
		if (!m_pTooltipPanel->IsCursorOver() || !m_bTooltipsEnabled 
			|| !m_pTooltipPanel->IsFullyVisible()
			)
		{
			SetVisible(false);
			m_pTooltipPanel = NULL;
		}
		else
		{			
			PerformLayout();
		}
	}
}


void BriefingTooltip::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	SetZPos(200);
	m_MainFont = pScheme->GetFont( "Default", IsProportional() );	
	if (ScreenHeight() <= 600)
		m_SubFont = pScheme->GetFont( "VerdanaSmall", IsProportional() );
	else
		m_SubFont = pScheme->GetFont( "DefaultSmall", IsProportional() );
	SetBgColor(pScheme->GetColor("Black", Color(0,0,0,255)));
	SetBorder(pScheme->GetBorder("ASWBriefingButtonBorder"));
	SetPaintBorderEnabled(true);
	SetPaintBackgroundType(2);

	m_pMainLabel->SetFgColor(pScheme->GetColor("White", Color(255,255,255,255)));
	m_pMainLabel->SetContentAlignment(vgui::Label::a_center);
	m_pMainLabel->SetZPos(201);
	m_pMainLabel->SetFont(m_MainFont);	
	m_pMainLabel->SetMouseInputEnabled(false);
	m_pSubLabel->SetFgColor(pScheme->GetColor("LightBlue", Color(128,128,128,255)));
	m_pSubLabel->SetContentAlignment(vgui::Label::a_center);
	m_pSubLabel->SetZPos(201);
	m_pSubLabel->SetFont(m_SubFont);
	m_pSubLabel->SetMouseInputEnabled(false);
	SetMouseInputEnabled(false);

}

void BriefingTooltip::SetTooltip(vgui::Panel* pPanel, const char* szMainText, const char* szSubText,
								 int iTooltipX, int iTooltipY, bool bAlignTopLeft)
{
	if (!pPanel || !m_bTooltipsEnabled)
		return;

	if (m_pTooltipPanel != pPanel)
	{
		//CLocalPlayerFilter filter;
		//C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.TooltipBox" );
	}

	MoveToFront();
	m_pTooltipPanel = pPanel;
	m_pMainLabel->SetText(szMainText);	
	m_pSubLabel->SetText(szSubText);
	m_pMainLabel->InvalidateLayout(true);
	m_pSubLabel->InvalidateLayout(true);
	m_iTooltipX = iTooltipX;
	m_iTooltipY = iTooltipY;
	m_bAlignTopLeft = bAlignTopLeft;
	//LayoutTooltip();	
	SetVisible(true);
	InvalidateLayout(true);
}

void BriefingTooltip::SetTooltip(vgui::Panel* pPanel, const wchar_t* szMainText, const wchar_t* szSubText,
								 int iTooltipX, int iTooltipY, bool bAlignTopLeft)
{
	if (!pPanel || !m_bTooltipsEnabled)
		return;

	if (m_pTooltipPanel != pPanel)
	{
		//CLocalPlayerFilter filter;
		//C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.TooltipBox" );
	}

	MoveToFront();
	m_pTooltipPanel = pPanel;
	m_pMainLabel->SetText(szMainText);	
	m_pSubLabel->SetText(szSubText);
	m_pMainLabel->InvalidateLayout(true);
	m_pSubLabel->InvalidateLayout(true);
	m_iTooltipX = iTooltipX;
	m_iTooltipY = iTooltipY;
	m_bAlignTopLeft = bAlignTopLeft;
	//LayoutTooltip();	
	SetVisible(true);
	InvalidateLayout(true);
}

void BriefingTooltip::PerformLayout()
{
	BaseClass::PerformLayout();
	m_pMainLabel->SetContentAlignment(vgui::Label::a_center);
	m_pSubLabel->SetContentAlignment(vgui::Label::a_center);
	m_pMainLabel->SizeToContents();
	m_pSubLabel->SizeToContents();
	int mainwide, maintall, subwide, subtall;
	int border = ScreenWidth() * 0.01;
	m_pMainLabel->GetContentSize(mainwide, maintall);
	m_pSubLabel->GetContentSize(subwide, subtall);
	m_pMainLabel->SetPos(border, border);
	m_pSubLabel->SetPos(border, maintall + border);
	int wide = (mainwide > subwide) ? mainwide : subwide;
	int tall = maintall+subtall+(border*2);
	wide += border * 2;
	SetSize(wide, tall);
	int x = m_iTooltipX - wide * 0.5f;	
	int y = m_iTooltipY - tall;
	if (m_bAlignTopLeft)
	{
		x = m_iTooltipX;
		y = m_iTooltipY;
	}
	if (x < border)
		x = border;
	if (y < border)
		y = border;
	if (x + wide > ScreenWidth() - border)
		x = ScreenWidth() - wide - border;
	SetPos(x, y);
}
