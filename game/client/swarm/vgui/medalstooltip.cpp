#include "cbase.h"
#include <vgui/isurface.h>
#include "MedalsTooltip.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

// todo: make this a child of briefingtooltip (if it even needs to do anything different?)
//  then pass it down to all medalpanels created from the medal collection panel

MedalsTooltip::MedalsTooltip(Panel *parent, const char *panelName) :
	Panel(parent, panelName)
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(0, "resource/SwarmSchemeNew.res", "SwarmSchemeNew");
	SetScheme(scheme);

	m_pTooltipPanel = NULL;
	m_pMainLabel = new Label(this, "MedalsTooltipMainLabel", " ");
	m_pMainLabel->SetContentAlignment(vgui::Label::a_center);	
	m_pMainLabel = new Label(this, "MedalsTooltipMainLabel", " ");
	m_pMainLabel->SetContentAlignment(vgui::Label::a_center);
	m_pSubLabel = new Label(this, "MedalsTooltipSubLabel", " ");
	m_pSubLabel->SetContentAlignment(vgui::Label::a_center);
	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	m_iLastWidth = sw;
	m_iTooltipX = m_iTooltipY = 0;
}

MedalsTooltip::~MedalsTooltip()
{
	
}

void MedalsTooltip::OnThink()
{
	BaseClass::OnThink();
	if (m_pTooltipPanel)
	{
		if (!m_pTooltipPanel->IsCursorOver())
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


void MedalsTooltip::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	SetZPos(200);
	m_MainFont = pScheme->GetFont( "Default", IsProportional() );

	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );

	if (sh <= 600)
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

void MedalsTooltip::SetTooltip(vgui::Panel* pPanel, const char* szMainText, const char* szSubText,
								 int iTooltipX, int iTooltipY)
{
	if (!pPanel)
		return;

	if (m_pTooltipPanel != pPanel)
	{
		//CLocalPlayerFilter filter;
		//C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.TooltipBox" );
	}

	m_pTooltipPanel = pPanel;
	m_pMainLabel->SetText(szMainText);	
	m_pSubLabel->SetText(szSubText);
	m_pMainLabel->InvalidateLayout(true);
	m_pSubLabel->InvalidateLayout(true);
	m_iTooltipX = iTooltipX;
	m_iTooltipY = iTooltipY;
	//LayoutTooltip();	
	SetVisible(true);
	InvalidateLayout(true);
}

void MedalsTooltip::PerformLayout()
{
	BaseClass::PerformLayout();
	m_pMainLabel->SetContentAlignment(vgui::Label::a_center);
	m_pSubLabel->SetContentAlignment(vgui::Label::a_center);
	m_pMainLabel->SizeToContents();
	m_pSubLabel->SizeToContents();
	int mainwide, maintall, subwide, subtall;
	int border = m_iLastWidth * 0.01;
	int screen_border = m_iLastWidth * 0.01f;
	m_pMainLabel->GetContentSize(mainwide, maintall);
	m_pSubLabel->GetContentSize(subwide, subtall);
	m_pMainLabel->SetPos(border, border);
	m_pSubLabel->SetPos(border, maintall + border);
	int wide = (mainwide > subwide) ? mainwide : subwide;
	int tall = maintall+subtall+(border*2);
	wide += border * 2;
	SetSize(wide, tall);
	border += screen_border;
	int x = m_iTooltipX - wide * 0.5f;
	int y = m_iTooltipY - tall;

	if (GetParent())
	{
		int px, py;
		px = py = 0;
		GetParent()->LocalToScreen(px, py);
		x -= px;
		y -= py;
	}

	if (x < border)
		x = border;
	if (y < border * 4)
		y = border * 4;
	if (x + wide > m_iLastWidth - border * 4)
		x = m_iLastWidth - wide - border * 4;
	SetPos(x, y);
}

void MedalsTooltip::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	ResizeFor(sw, sh);
}

void MedalsTooltip::ResizeFor(int width, int height)
{
	m_iLastWidth = width;	
	m_pMainLabel->InvalidateLayout(true);
	m_pSubLabel->InvalidateLayout(true);
	InvalidateLayout(true);
}
