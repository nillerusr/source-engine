#include "cbase.h"

#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/TextImage.h"
#include "WrappedLabel.h"
#include "c_asw_game_resource.h"
#include "ObjectiveListBox.h"
#include "ObjectiveMap.h"
#include "c_asw_player.h"
#include "c_asw_objective.h"
#include "vgui_controls/AnimationController.h"
#include "ObjectiveTitlePanel.h"
#include "ObjectiveDetailsPanel.h"
#include "ObjectiveIcons.h"
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ObjectiveListBox::ObjectiveListBox(Panel *parent, const char *name) : Panel(parent, name)
{	
	m_iNumTitlePanels = 0;
	m_pSelectedTitle = NULL;
	m_pDetailsPanel = NULL;
	m_pObjectiveIcons = NULL;
}
	
ObjectiveListBox::~ObjectiveListBox()
{

}

void ObjectiveListBox::PerformLayout()
{
	BaseClass::PerformLayout();
}

void ObjectiveListBox::OnThink()
{
	BaseClass::OnThink();

	//  create and position all necessary objectivetitlepanels
	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	float panel_y = ScreenHeight() * 0.014f;
	int m_iNumRealTitlePanels = 0;
	int iCurrent = 0;
	for (int i=0;i<12;i++)
	{
		C_ASW_Objective* pObjective = pGameResource->GetObjective(i);
		if (pObjective && !pObjective->IsObjectiveHidden())
		{
			if (!pObjective->IsObjectiveDummy())
				m_iNumRealTitlePanels++;
			// do we need to create a title panel for this slot?
			if (m_iNumTitlePanels < iCurrent+1)
			{
				m_pTitlePanel[m_iNumTitlePanels] = new ObjectiveTitlePanel( this, "objectivetitlepanel" );
				m_pTitlePanel[m_iNumTitlePanels]->SetIndex(m_iNumRealTitlePanels);
				m_pTitlePanel[m_iNumTitlePanels]->InvalidateLayout();
				m_iNumTitlePanels++;
			}
			if (!m_pTitlePanel[iCurrent])
				continue;
			// make the panel be filled with the required info
			m_pTitlePanel[iCurrent]->SetObjective(pObjective);
			// position the panel
			m_pTitlePanel[iCurrent]->SetPos( YRES( 3 ), panel_y);
			panel_y += m_pTitlePanel[iCurrent]->GetTall() + ScreenHeight() * 0.005f;
			iCurrent++;
		}
	}
}


void ObjectiveListBox::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	//SetBgColor(Color(30,66,89,164));
	SetBgColor(Color(0,0,0,0));
}

// for first showing of the objective screen
bool ObjectiveListBox::ClickFirstTitle()
{
	if ( ASWGameRules() && ASWGameRules()->GetGameState() >= ASW_GS_INGAME )
	{
		// click the first incomplete objective
		for ( int i = 0; i < m_iNumTitlePanels; i++ )
		{
			if ( !m_pTitlePanel[i] )
				continue;
			C_ASW_Objective* pObjective = m_pTitlePanel[i]->GetObjective();
			if ( !pObjective )
				continue;

			if ( !pObjective->IsObjectiveDummy() && !pObjective->IsObjectiveComplete() )
			{
				ClickedTitle( m_pTitlePanel[i] );
				return true;
			}
		}
	}

	if (m_iNumTitlePanels > 0 && m_pTitlePanel[0])
	{
		ClickedTitle(m_pTitlePanel[0]);
		return true;
	}
	return false;
}

void ObjectiveListBox::ClickedTitle(ObjectiveTitlePanel* pTitle)
{
	if (m_pSelectedTitle)
		m_pSelectedTitle->SetSelected(false);
	//if (m_pSelectedTitle!=pTitle)
	{
		pTitle->SetSelected(true);
		m_pSelectedTitle = pTitle;
	}
	//else
		//m_pSelectedTitle = NULL;

	if (m_pSelectedTitle)
	{
		m_pDetailsPanel->SetObjective(m_pSelectedTitle->GetObjective());
		if (m_pMapPanel)
			m_pMapPanel->SetObjective(m_pSelectedTitle->GetObjective());
		if (m_pObjectiveIcons)
			m_pObjectiveIcons->SetObjective(m_pSelectedTitle->GetObjective());
	}
	else
	{
		m_pDetailsPanel->SetObjective(NULL);
		if (m_pMapPanel)
			m_pMapPanel->SetObjective(NULL);
		if (m_pObjectiveIcons)
			m_pObjectiveIcons->SetObjective(NULL);
	}
}