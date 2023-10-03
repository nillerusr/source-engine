#ifndef _INCLUDED_CAMPAIGN_PANEL_H
#define _INCLUDED_CAMPAIGN_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include "asw_shareddefs.h"
#include "asw_campaign_info.h"

namespace vgui
{
	class WrappedLabel;
	class ImagePanel;
	class IBorder;
	class ProgressBar;
};

class ObjectiveMapMarkPanel;
class CASW_Campaign_Info;
class C_ASW_Campaign_Save;
class CampaignMapLocation;
//class ChatEchoPanel;
class SoftLine;
class MapEdgesBox;
class PlayersWaitingPanel;
class CampaignMapSearchLights;
class CNB_Header_Footer;
class CNB_Button;
class CNB_Commander_List;
class CNB_Campaign_Mission_Details;
class CNB_Vote_Panel;

#define ASW_NUM_CAMPAIGN_LABELS 16
#define ASW_CAMPAIGN_PANEL_PLAYERS 6

// this panel shows the campaign map and allows the players to decide which mission to try next

class CampaignPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CampaignPanel, vgui::EditablePanel );
public:
	CampaignPanel(vgui::Panel *parent, const char *name);
	virtual ~CampaignPanel();

	virtual void OnThink();
	virtual void Paint();
	virtual void PerformLayout();
	void ApplySchemeSettings( vgui::IScheme *scheme );	
	void OnCommand(const char *command);
	void OnTick();

	void SetStage(int i);
	void SetHighlightedMission(int iMission);	
	void LocationClicked(int iMission);
	void LocationOver(int iMission);
	bool IsMissionVisible(int i);

	void UpdateLocationLabels();
	void FadeOutLocationLabels();

	void GetMapBounds(int &x, int &y, int &w, int &t);
	void GetConstellationBracket(int &x, int &y, int &w, int &t);

	// get the coords of the various bracketing areas	
	void GetSurfaceBracket(int &x, int &y, int &w, int &t);

	void AddSoftLines();	
	void PositionSoftLines();
	void PositionLocationDots();

	CASW_Campaign_Info* GetCampaignInfo();
	C_ASW_Campaign_Save* GetCampaignSave();

	CNB_Header_Footer* m_pHeaderFooter;

	vgui::Label *m_pLeaderLabel;
	CNB_Button *m_pLaunchButton;
	CNB_Button *m_pFriendsButton;
	CNB_Commander_List *m_pCommanderList;
	CNB_Campaign_Mission_Details *m_pMissionDetails;

	vgui::ImagePanel* m_pBackDrop;
	vgui::ImagePanel* m_pGalacticMap;		
	vgui::ImagePanel* m_pGalaxyLines;
	vgui::ImagePanel* m_pSurfaceMap;
	vgui::ImagePanel* m_pSurfaceMapLayer[3];
	vgui::Label* m_pMouseOverLabel;
	vgui::Label* m_pMouseOverGlowLabel;

	vgui::Panel* m_pMapBorder;

	vgui::Label* m_pTimeLeftLabel;
	CUtlVector<SoftLine*> m_pSoftLine;

	void SetLabelPos(int iLabelIndex, int x, int y);
	vgui::Label* m_pMapLabels[ASW_NUM_CAMPAIGN_LABELS];
	CampaignMapSearchLights* m_pLights;

	CampaignMapLocation* m_pLocationPanel[ASW_MAX_MISSIONS_PER_CAMPAIGN];
	vgui::ImagePanel* m_pCurrentLocationImage;

	MapEdgesBox* m_pMapEdges;
	MapEdgesBox* m_pGalaxyEdges;

	vgui::HFont m_LargeFont;
	vgui::IBorder *m_pButtonBorder;

	ObjectiveMapMarkPanel* m_pBracket;
	CNB_Vote_Panel *m_pVotePanel;

	// the different stages of the campaign map animation
	enum
	{
		CMP_NONE,
		CMP_FADING_IN,
		CMP_LINES_SOUND,
		CMP_BRACKETING_CONSTELLATION,
		CMP_ZOOMING_SURFACE_MAP,
		CMP_FADE_LOCATIONS_IN,
		CMP_WAITING		
	};

	// temp unused stages
	enum
	{					
		CMP_BRACKETING_PLANET,
	};

	bool m_bVoted;
	bool m_bSetTitle;
	int m_iStage;
	bool m_bCurrentAnimating;
	int m_CurrentAnimatingToX;
	int m_CurrentAnimatingToY;
	float m_fNextStageTime;
	int m_iHighlightedMission;
	bool m_bShowGalaxy;
	int m_iLocationOver;

	int m_iLastLabelBlipSound;

	bool m_bSetAlpha;
	bool m_bAddedLines;

	// map generation progress bar
	void UpdateMapGenerationStatus();
	vgui::Label *m_pStatusLabel;
	vgui::ProgressBar *m_pProgressBar[ASW_MAX_READY_PLAYERS];
	vgui::Label *m_pProgressLabel[ASW_MAX_READY_PLAYERS];
};



#endif // _INCLUDED_CAMPAIGN_PANEL_H