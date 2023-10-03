#include "cbase.h"

#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/ScrollBar.h"
#include "WrappedLabel.h"
#include "c_asw_game_resource.h"
#include "ObjectiveDetailsPanel.h"
#include "c_asw_player.h"
#include "c_asw_objective.h"
#include "vgui_controls/AnimationController.h"
#include "ObjectiveTitlePanel.h"
#include <vgui/ISurface.h>
#include <vgui_controls/PanelListPanel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define TEXT_BORDER_X 0.01f
#define TEXT_BORDER_Y 0.012f

#define DESCRIPTION_WIDE 175
#define DETAILS_TALL 240

ObjectiveDetailsPanel::ObjectiveDetailsPanel(Panel *parent, const char *name) : Panel(parent, name)
{	
	m_pObjective = NULL;
	m_pQueuedObjective = NULL;

	m_pDescriptionContainer = new vgui::Panel(this, "DescriptionContainer");

	// create the text boxes for our various description lines
	for (int i=0;i<4;i++)
	{		
		m_pDescription[i] = new vgui::WrappedLabel(m_pDescriptionContainer, "ObjectiveDetailsPanelLabel", "");
		m_pDescription[i]->SetContentAlignment(vgui::Label::a_northwest);
		m_pDescription[i]->SetProportional( true );
		m_pDescription[i]->SetMouseInputEnabled(false);
	}
	m_iTotalTextTall = 0;	

	m_pListPanel = new vgui::PanelListPanel( this, "objectivedetailslist" );
	m_pListPanel->AllowMouseWheel(false);
	m_pListPanel->SetShowScrollBar(true);	
	m_pListPanel->SetProportional(false);
	m_pListPanel->SetFirstColumnWidth(0);
	m_pListPanel->AddItem(NULL, m_pDescriptionContainer);
	m_pListPanel->SetVerticalBufferPixels( 0 );
	m_pListPanel->SetPaintBackgroundEnabled(false);
	m_pListPanel->GetScrollBar()->UseImages( "scroll_up", "scroll_down", "scroll_line", "scroll_box" );
	m_pDescriptionContainer->SetProportional( true );
}
	
ObjectiveDetailsPanel::~ObjectiveDetailsPanel()
{

}

// a single panel showing a marine
void ObjectiveDetailsPanel::PerformLayout()
{
	int tall = YRES( DETAILS_TALL );

	int nPanelWide = YRES( DESCRIPTION_WIDE );
	SetSize( nPanelWide, tall );	

	m_pListPanel->GetScrollBar()->SetWide( YRES( 15 ) );

	m_pDescriptionContainer->SetBounds(0, 0, GetTextWidth(),
											MAX( m_iTotalTextTall, tall ) );
	m_pListPanel->InvalidateLayout(true);
	m_pListPanel->SetBounds(0, 0, nPanelWide, tall - YRES( 5 ) );	
}

int ObjectiveDetailsPanel::GetTextWidth()
{
	int nPanelWide = YRES( DESCRIPTION_WIDE );
	int text_border_x = ScreenWidth() * TEXT_BORDER_X;
	return (nPanelWide - m_pListPanel->GetScrollBar()->GetWide()) - text_border_x * 2;
}

void ObjectiveDetailsPanel::OnThink()
{
	BaseClass::OnThink();

	if (GetAlpha() <= 0)
	{
		if (m_pQueuedObjective != NULL)
		{
			m_pObjective = m_pQueuedObjective;
			UpdateText();
			m_pQueuedObjective = NULL;
			// fade the objective text back in
			vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 255.0f, 0, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
		else
		{
			m_pObjective = NULL;
		}
	}

	// calculate the height of the text
	float panel_y = ScreenHeight() * TEXT_BORDER_Y;
	char buffer[256];
	for (int i=0;i<4;i++)
	{
		m_pDescription[i]->GetText(buffer, 256);
		if (buffer[0]!=NULL)
		{
			m_pDescription[i]->SetWide(GetTextWidth());
			//m_pDescription[i]->SizeToContents();
			int w, h;
			m_pDescription[i]->GetTextImage()->SetDrawWidth(GetTextWidth());
			m_pDescription[i]->GetTextImage()->GetContentSize(w, h);
			m_pDescription[i]->SetTall(h);
			m_pDescription[i]->SetPos(ScreenWidth() * TEXT_BORDER_X, panel_y);
			//Msg("onthink: desc[%d] is %d high\n", i, h);
			panel_y += h + ScreenHeight() * 0.015f;
		}
	}
	int text_tall = panel_y + (ScreenHeight() * TEXT_BORDER_Y);
	if ( text_tall != m_iTotalTextTall )
	{
		m_iTotalTextTall = text_tall;
		
		InvalidateLayout(true, true);
	}

	//m_pListPanel->SetShowScrollBar(m_iTotalTextTall > m_pListPanel->GetTall());
}

void ObjectiveDetailsPanel::SetObjective(C_ASW_Objective* pObjective)
{
	if (pObjective == m_pObjective)
		return;

	m_pQueuedObjective = pObjective;
	vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 0, 0, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

void ObjectiveDetailsPanel::UpdateText()
{		
	// calculate the height of the text
	float panel_y = ScreenHeight() * TEXT_BORDER_Y;
	for (int i=0;i<4;i++)
	{		
		m_pDescription[i]->SetText(m_pObjective->GetDescription(i));
		if (m_pObjective->GetDescription(i)[0] != NULL)
		{
			// set our text to the various descriptions in our m_pObjective
			m_pDescription[i]->SetWide(GetTextWidth());
			int w, h;
			m_pDescription[i]->GetTextImage()->SetDrawWidth(GetTextWidth());
			m_pDescription[i]->GetTextImage()->GetContentSize(w, h);
			m_pDescription[i]->SetTall(h);
			m_pDescription[i]->SetPos(ScreenWidth() * TEXT_BORDER_X, panel_y);
			panel_y += h + ScreenHeight() * 0.015f;
		}
	}

	int text_tall = panel_y + (ScreenHeight() * TEXT_BORDER_Y);
	if ( text_tall != m_iTotalTextTall )
	{
		m_iTotalTextTall = text_tall;

		InvalidateLayout(true, true);
	}
}


void ObjectiveDetailsPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	//SetBgColor(Color(30,66,89,164));
	SetPaintBackgroundEnabled( false );
	for (int i=0;i<4;i++)
	{
		if (m_pDescription[i])
		{
			m_pDescription[i]->SetFgColor(pScheme->GetColor("LightBlue", Color(255,255,255,255)));
		}
	}	
}
