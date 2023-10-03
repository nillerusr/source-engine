#ifndef _INCLUDED_IASW_MISSION_CHOOSER_LIST_H
#define _INCLUDED_IASW_MISSION_CHOOSER_LIST_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"
#include "missionchooser/iasw_mission_chooser_source.h"
#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/PropertyPage.h>

class CASW_Mission_Chooser_Entry;
class ServerOptionsPanel;

class CASW_Mission_Chooser_List : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( CASW_Mission_Chooser_List, vgui::Panel );
public:
	CASW_Mission_Chooser_List( vgui::Panel *pParent, const char *pElementName, int iChooserType, int iHostType,  IASW_Mission_Chooser_Source *pMissionSource );
	virtual ~CASW_Mission_Chooser_List();

	virtual void PerformLayout();
	virtual void OnThink();
	virtual void CASW_Mission_Chooser_List::ApplySchemeSettings(vgui::IScheme *pScheme);
	void UpdateDetails();
	virtual void OnCommand(const char* command);
	void OnEntryClicked(CASW_Mission_Chooser_Entry *pClicked);
	void OnSaveDeleted();
	const char * GenerateNewSaveGameName();
	void CloseSelf();
	void UpdateNumPages();
	void ShowDifficultyChooser(const char *szCommand);
	void ChangeToShowingMissionsWithinCampaign( int nCampaignIndex );

	vgui::Button *m_pCancelButton;
	vgui::Button *m_pNextPageButton;
	vgui::Button *m_pPrevPageButton;
	vgui::Label *m_pPageLabel;
	vgui::Label *m_pTitleLabel;
	vgui::CheckButton *m_pShowAllCheck;
	CASW_Mission_Chooser_Entry* m_pEntry[ASW_SAVES_PER_PAGE]; // this array needs to be large enough for either ASW_SAVES_PER_SCREEN (for when showing save games), or ASW_MISSIONS_PER_SCREEN (for when showing missions) or ASW_CAMPAIGNS_PER_SCREEN (for when showing campaigns)

	int m_ChooserType;
	int m_HostType;	
	int m_iPage;
	int m_iNumSlots;
	int m_iMaxPages;
	int m_nCampaignIndex;
	IASW_Mission_Chooser_Source *m_pMissionSource;

	ServerOptionsPanel *m_pServerOptions;

	MESSAGE_FUNC( OnButtonChecked, "CheckButtonChecked" );
};

// chooser types - what we're going to launch
enum
{	
	ASW_CHOOSER_CAMPAIGN,
	ASW_CHOOSER_SAVED_CAMPAIGN,
	ASW_CHOOSER_SINGLE_MISSION,
};

// host types - how we're going to launch it
enum
{
	ASW_HOST_TYPE_SINGLEPLAYER,
	ASW_HOST_TYPE_CREATESERVER,
	ASW_HOST_TYPE_CALLVOTE,
};

#endif // _INCLUDED_IASW_MISSION_CHOOSER_LIST_H
