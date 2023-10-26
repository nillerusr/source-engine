#include "cbase.h"
#include "CampaignPanel.h"
#include "MissionCompleteStatsLine.h"
#include "vgui_controls/AnimationController.h"
#include "c_asw_player.h"
#include "c_asw_game_resource.h"
#include "asw_campaign_info.h"
#include "c_asw_campaign_save.h"
#include "ObjectiveMapMarkPanel.h"
#include "WrappedLabel.h"
#include "asw_gamerules.h"
#include "vgui/isurface.h"
#include "SoftLine.h"
#include "ChatEchoPanel.h"
#include "ScanLinePanel.h"
#include <vgui_controls/ImagePanel.h>
#include "MapEdgesBox.h"
#include "PlayersWaitingPanel.h"
#include <vgui_controls/TextImage.h>
#include <vgui/ILocalize.h>
#include "CampaignMapLocation.h"
#include "controller_focus.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// ============================================ CampaignMapLocation ============================================

CampaignMapLocation::CampaignMapLocation(vgui::Panel *parent, const char *name, CampaignPanel* pCampaignPanel, int iMission)
		: vgui::Panel(parent, name),
		m_iMission(iMission),
		m_pCampaignPanel(pCampaignPanel)	
{	
	m_pLocationImage = new vgui::ImagePanel(this, "CampaignLocationImage");
	m_pLocationImage->SetShouldScaleImage(true);
	m_pLocationImage->SetMouseInputEnabled(false);
	m_pLocationImage->SetImage("swarm/Campaign/PlainLocationDot");
	
	m_pBlueCircle = new vgui::ImagePanel(this, "BlueCircle");
	m_pBlueCircle->SetShouldScaleImage(true);
	m_pBlueCircle->SetMouseInputEnabled(false);
	m_pBlueCircle->SetImage("swarm/Campaign/CampaignCircle0");
	m_pBlueCircle->SetVisible(false);

	if (GetControllerFocus())
	{
		GetControllerFocus()->AddToFocusList(this, false);
	}

	m_bValidMissionChoice = false;
}

void CampaignMapLocation::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings(scheme);
	m_pBlueCircle->SetAlpha(128);
}

CampaignMapLocation::~CampaignMapLocation()
{
	 m_iNumVotes = 0;

	 if (GetControllerFocus())
	{
		GetControllerFocus()->RemoveFromFocusList(this);
	}
}

void CampaignMapLocation::ResizeTo(int width, int height)
{
	SetSize(width, height);
	if (m_pLocationImage)
	{
		m_pLocationImage->SetSize(width, height);
		m_pBlueCircle->SetBounds(0, 0, width, height);
	}
}

void CampaignMapLocation::OnMouseReleased(vgui::MouseCode code)
{
	if ( m_pCampaignPanel && code == MOUSE_LEFT )
	{
		m_pCampaignPanel->LocationClicked(m_iMission);
	}
}

void CampaignMapLocation::OnCursorEntered()
{
	if (m_pCampaignPanel && m_pCampaignPanel->m_iHighlightedMission != m_iMission)
	{		
		m_pCampaignPanel->LocationOver(m_iMission);
	}
}

bool CampaignMapLocation::ValidMissionChoice()
{
	C_ASW_Campaign_Save *pSave = ASWGameRules()->GetCampaignSave();
	if (!pSave)	
		return false;

	// don't allow vote for this mission if this mission was already completed
	if (pSave->m_MissionComplete[m_iMission] != 0 || m_iMission <= 0)
		return false;

	// make sure the mission they want to vote for is reachable
	if (!pSave->IsMissionLinkedToACompleteMission(m_iMission, ASWGameRules()->GetCampaignInfo()))
		return false;

	return true;
}

int CampaignMapLocation::FindNumVotes()
{
	C_ASW_Campaign_Save *pSave = ASWGameRules()->GetCampaignSave();
	if (!pSave)	
		return 0;

	if (m_iMission<0 || m_iMission>=ASW_MAX_MISSIONS_PER_CAMPAIGN)
		return 0;

	// don't allow vote for this mission if this mission was already completed
	return (pSave->m_NumVotes[m_iMission]);
}

void CampaignMapLocation::OnThink()
{
	bool bValid = ValidMissionChoice();
	if (m_bValidMissionChoice != bValid)
	{
		m_bValidMissionChoice = bValid;
		if (bValid)
		{
			m_pBlueCircle->SetVisible(true);
		}
		else
		{
			m_pBlueCircle->SetVisible(false);
		}
	}
	if (IsVisible())
	{
		// find out how many votes we have
		int iVotes = FindNumVotes();
		if (iVotes != m_iNumVotes)
		{
			 m_iNumVotes = iVotes;			 
			 if (iVotes == 1)
				 m_pBlueCircle->SetImage("swarm/Campaign/CampaignCircle1");
			 else if (iVotes == 2)
				 m_pBlueCircle->SetImage("swarm/Campaign/CampaignCircle2");
			 else if (iVotes == 3)
				 m_pBlueCircle->SetImage("swarm/Campaign/CampaignCircle3");
			 else if (iVotes == 4)
				 m_pBlueCircle->SetImage("swarm/Campaign/CampaignCircle4");
			 else if (iVotes == 5)
				 m_pBlueCircle->SetImage("swarm/Campaign/CampaignCircle5");
			 else
				 m_pBlueCircle->SetImage("swarm/Campaign/CampaignCircle0");
		}
	}
}

void CampaignMapLocation::SetSelected(bool bSelected)
{
	if (m_pLocationImage)
	{
		//if (!bSelected)
			//m_pLocationImage->SetImage("swarm/Campaign/PlainLocationDot");	
		//else
			//m_pLocationImage->SetImage("swarm/Campaign/DotSelected");
	}
}