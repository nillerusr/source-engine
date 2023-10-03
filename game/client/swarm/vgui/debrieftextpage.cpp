#include "cbase.h"
#include "DebriefTextPage.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/TextImage.h"
#include "ImageButton.h"
#include "asw_gamerules.h"
#include "WrappedLabel.h"
#include "c_asw_debrief_stats.h"
#include "nb_island.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DebriefTextPage::DebriefTextPage(vgui::Panel *parent, const char *name) : vgui::Panel(parent, name)
{	
	m_bMissionSuccess = ( ASWGameRules() ? ASWGameRules()->GetMissionSuccess() : false );

	m_pBackground = new CNB_Island( this, "BackgroundIsland" );
	m_pBackground->m_pTitle->SetText( m_bMissionSuccess ? "#asw_debrief_complete" : "#asw_debrief_failed" );

	for (int i=0;i<ASW_NUM_DEBRIEF_PARAS;i++)
	{
		m_pPara[i] = new vgui::WrappedLabel(this, "DebriefLabel", "");
		m_pPara[i]->SetZPos( 5 );
	}	

	m_hDebriefStats = NULL;
	m_bStringUpdated = false;
}

void DebriefTextPage::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled( false );

	for (int i=0;i<ASW_NUM_DEBRIEF_PARAS;i++)
	{
		m_pPara[i]->SetContentAlignment(vgui::Label::a_northwest);
		m_pPara[i]->SetFont( pScheme->GetFont( "Default", IsProportional() ) );
		m_pPara[i]->SetFgColor(pScheme->GetColor("LightBlue", Color(255,255,255,255)));	
	}
}

void DebriefTextPage::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pBackground->SetBounds( 0, 0, GetWide(), GetTall() );

	LayoutStrings();	
}

void DebriefTextPage::OnThink()
{
	if (!m_hDebriefStats.Get())
	{
		unsigned int c = ClientEntityList().GetHighestEntityIndex();
		C_ASW_Debrief_Stats *pTemp = NULL;
		for ( unsigned int i = 0; i <= c; i++ )
		{
			C_BaseEntity *e = ClientEntityList().GetBaseEntity( i );
			if ( !e )
				continue;
			
			pTemp = dynamic_cast<C_ASW_Debrief_Stats*>(e);
			if (pTemp)
			{				
				break;
			}
		}
		if (pTemp)
		{
			m_hDebriefStats = pTemp;
			m_bStringUpdated = false;
		}
	}
	else if ( !m_bStringUpdated )
	{
		UpdateStrings();
		m_bStringUpdated = true;
	}
}

void DebriefTextPage::UpdateStrings()
{
	if ( !m_hDebriefStats.Get() )
		return;

	if ( !m_bMissionSuccess )
	{
		m_pPara[0]->SetText( ASWGameRules() ? ASWGameRules()->GetFailAdviceText() : "#asw_default_debrief" );
		m_pPara[1]->SetText("");
		m_pPara[2]->SetText("");
	}
	else
	{
		if ( Q_strlen(m_hDebriefStats->m_DebriefText1.Get()) <= 0)	// just show some default debrief text if the mapper didn't specify any
		{
			m_pPara[0]->SetText("#asw_default_debrief");
			m_pPara[1]->SetText("");
			m_pPara[2]->SetText("");
		}
		else
		{
			m_pPara[0]->SetText(m_hDebriefStats->m_DebriefText1.Get());
			m_pPara[1]->SetText(m_hDebriefStats->m_DebriefText2.Get());
			m_pPara[2]->SetText(m_hDebriefStats->m_DebriefText3.Get());
		}
	}

	LayoutStrings();
}

void DebriefTextPage::LayoutStrings()
{
	int padding = 0.02f * ScreenHeight();
	int left_edge = padding;
	int ypos = padding;
	int text_wide = GetWide() - padding * 2;

	int w, t;
	ypos += YRES( 25 );

	for (int i=0;i<ASW_NUM_DEBRIEF_PARAS;i++)
	{
		m_pPara[i]->SetBounds(left_edge, ypos, text_wide, ScreenHeight());
		m_pPara[i]->InvalidateLayout(true);
		m_pPara[i]->GetTextImage()->GetContentSize(w, t);

		ypos += t + ScreenHeight() * 0.02f;
	}
}