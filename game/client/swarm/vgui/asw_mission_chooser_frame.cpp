#include "cbase.h"
#include "vgui/ivgui.h"
#include <vgui/vgui.h>
#include <vgui/ischeme.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/PropertySheet.h>
#include "convar.h"
#include "asw_mission_chooser_list.h"
#include "asw_mission_chooser_frame.h"
#include "asw_difficulty_chooser.h"
#include "ServerOptionsPanel.h"
#include <vgui/isurface.h>
#include <vgui/IInput.h>
#include "vgui_controls/AnimationController.h"
#include "ienginevgui.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "clientmode_asw.h"
#include "c_asw_voting_mission_chooser_source.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_Mission_Chooser_Frame::CASW_Mission_Chooser_Frame( vgui::Panel *pParent, const char *pElementName,
															int iHostType, int iChooserType,
															IASW_Mission_Chooser_Source *pMissionSource) :
	Frame(pParent, pElementName)
{
	SetProportional( false );
	m_pMissionSource = pMissionSource;

	// create a propertysheet
	m_pSheet = new vgui::PropertySheet(this, "MissionChooserSheet");	
	m_pSheet->SetProportional( false );
	m_pSheet->SetVisible(true);
		
	// create our 3 lists and put them in as pages in the property sheet
	m_pChooserList = new CASW_Mission_Chooser_List(m_pSheet, "CampaignList", iChooserType, iHostType, pMissionSource);
	//m_pCampaignList = new CASW_Mission_Chooser_List(m_pSheet, "CampaignList", ASW_CHOOSER_CAMPAIGN, iHostType);
	//m_pSavedCampaignList = new CASW_Mission_Chooser_List(m_pSheet, "SavedCampaignList", ASW_CHOOSER_SAVED_CAMPAIGN, iHostType);
	//m_pSingleMissionList = new CASW_Mission_Chooser_List(m_pSheet, "MissionList", ASW_CHOOSER_SINGLE_MISSION, iHostType);

	if (iChooserType == ASW_CHOOSER_CAMPAIGN)
		m_pSheet->AddPage(m_pChooserList, "#asw_start_campaign");
	else if (iChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
		m_pSheet->AddPage(m_pChooserList, "#asw_load_campaign");
	else if (iChooserType == ASW_CHOOSER_SINGLE_MISSION)
		m_pSheet->AddPage(m_pChooserList, "#asw_single_mission");

	if (iHostType == ASW_HOST_TYPE_CREATESERVER)
	{
		m_pOptionsPanel = new ServerOptionsPanel(this, "OptionsPanel");	
		m_pSheet->AddPage(m_pOptionsPanel, "#asw_server_options");
		m_pChooserList->m_pServerOptions = m_pOptionsPanel;
	}
	else	// no need for server options if they're just playing singleplayer
	{
		m_pOptionsPanel = NULL;
	}

	m_bMadeModal = false;
	m_bAvoidTranslucency = false;
	Msg("CASW_Mission_Chooser_Frame\n");
}

CASW_Mission_Chooser_Frame::~CASW_Mission_Chooser_Frame()
{
	Msg("~CASW_Mission_Chooser_Frame\n");
}

void CASW_Mission_Chooser_Frame::PerformLayout()
{
	SetPos(100,10);
	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );
	SetSize(sw - 200, sh - 20);

	BaseClass::PerformLayout();	

	int x, y, wide, tall;
	GetClientArea(x, y, wide, tall);
	m_pSheet->SetBounds(x, y, wide, tall);

	m_pSheet->SetVisible(true);
	m_pSheet->InvalidateLayout(true);
	//m_pSheet->SetTabWidth(GetWide()/3.0f);

	float top_edge = 30;
	m_pChooserList->SetPos(0, top_edge);

	m_pChooserList->SetSize(wide, GetTall() - (60));
	m_pChooserList->InvalidateLayout(true);

	if (m_pOptionsPanel)
		m_pOptionsPanel->SetBounds(x, top_edge, wide - x * 2, GetTall() - top_edge);
}

void CASW_Mission_Chooser_Frame::OnThink()
{
	BaseClass::OnThink();
	if (!m_bMadeModal)
	{
		//vgui::input()->SetAppModalSurface(GetVPanel());
		m_bMadeModal = true;
	}
}

void CASW_Mission_Chooser_Frame::OnClose()
{
	BaseClass::OnClose();
	SetVisible(false);	// have to do this, as the fading transparency causes sorting issues in the briefing
}

void CASW_Mission_Chooser_Frame::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// ASWTODO - solve translucency problem when showing this panel over a frame
	//if (m_bAvoidTranslucency)
		//m_flTransitionEffectTime = 0;
	//Msg("CASW_Mission_Chooser_Frame::ApplySchemeSettings m_bAvoidTranslucency=%d m_flTransitionEffectTime=%f\n", m_bAvoidTranslucency, m_flTransitionEffectTime);
}

void CASW_Mission_Chooser_Frame::RemoveTranslucency()
{
	Msg("CASW_Mission_Chooser_Frame::RemoveTranslucency\n");
	m_bAvoidTranslucency = true;
	// ASWTODO - solve translucency problem when showing this panel over a frame
	//m_flTransitionEffectTime = 0;
	SetBgColor(Color(65, 74, 96, 255));
	SetAlpha(255.0f);
	// ASWTODO - solve translucency problem when showing this panel over a frame
	//vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 255.0f, 0.0f, m_flTransitionEffectTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

vgui::DHANDLE<CASW_Mission_Chooser_Frame> g_hChooserFrame;
void LaunchMissionChooser(int iHostType, int iChooserType)
{
	if (g_hChooserFrame!=NULL)
	{
		CASW_Mission_Chooser_Frame *pFrame = g_hChooserFrame;
		pFrame->SetVisible(false);
		pFrame->MarkForDeletion();
		g_hChooserFrame = NULL;
	}
	// close any difficulty panels we have up
	if (g_hDifficultyFrame != NULL)
	{
		CASW_Difficulty_Chooser *pFrame = g_hDifficultyFrame;
		pFrame->SetVisible(false);
		pFrame->MarkForDeletion();
		g_hDifficultyFrame = NULL;
	}

	IASW_Mission_Chooser_Source *pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
	if (!pSource)
		return;

	if (iChooserType == ASW_CHOOSER_SAVED_CAMPAIGN)
	{
		// refresh these each time we bring up the panel, since new saves get created during play
		pSource->RefreshSavedCampaigns();
	}

	Msg("LaunchMissionChooser\n");
	// ASWTODO - check this works even though parent is null
	CASW_Mission_Chooser_Frame *pFrame = new CASW_Mission_Chooser_Frame( NULL, "MissionChooserFrame", iHostType, iChooserType, pSource );
	vgui::VPANEL rootpanel = enginevgui->GetPanel( PANEL_GAMEUIDLL );
	pFrame->SetParent( rootpanel );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(0, "resource/SwarmFrameScheme.res", "SwarmFrameScheme");
	pFrame->SetScheme(scheme);

	if (iHostType == ASW_HOST_TYPE_SINGLEPLAYER)
		pFrame->SetTitle("#asw_menu_singleplayer", true );
	else if (iHostType == ASW_HOST_TYPE_CREATESERVER)
		pFrame->SetTitle("#asw_menu_create_server", true );
	else
	{
		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "Host type: %d chooser: %d", iHostType, iChooserType);
		pFrame->SetTitle(buffer, true );
	}
	//pFrame->LoadControlSettings("Resource/UI/ScoreBoard.res");

	pFrame->SetTitleBarVisible(true);
	pFrame->SetMoveable(false);
	pFrame->SetSizeable(false);
	pFrame->SetMenuButtonVisible(false);
	pFrame->SetMaximizeButtonVisible(false);
	pFrame->SetMinimizeToSysTrayButtonVisible(false);
	pFrame->SetMinimizeButtonVisible(false);
	pFrame->SetCloseButtonVisible(true);	

	pFrame->RequestFocus();
	pFrame->SetVisible(true);
	pFrame->SetEnabled(true);

	pFrame->InvalidateLayout(true, true);

	///g_hChooserFrame = pFrame;
}

void LaunchMissionVotePanel(int iChooserType, bool bAvoidTranslucency)
{
	// close any chooser frame already open	
	if (g_hChooserFrame!=NULL)
	{
		CASW_Mission_Chooser_Frame *pFrame = g_hChooserFrame;
		pFrame->SetVisible(false);
		pFrame->MarkForDeletion();
		g_hChooserFrame = NULL;
	}

	if (iChooserType == -1)
		return;

	IASW_Mission_Chooser_Source *pSource = GetVotingMissionSource(); //missionchooser ? missionchooser->LocalMissionSource() : NULL;
	if (!pSource)
		return;

	pSource->ResetCurrentPage();

	// since we're voting, this must be multiplayer, so make sure the server hosts the game
	int iHostType = ASW_HOST_TYPE_CALLVOTE;
	//CBaseViewport
	Msg("LaunchMissionVotePanel\n");
	// ASWTODO - check this works even though parent is null
	CASW_Mission_Chooser_Frame *pFrame = new CASW_Mission_Chooser_Frame( GetClientMode()->GetViewport(), "MissionChooserFrame", iHostType, iChooserType, pSource );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(0, "resource/SwarmFrameScheme.res", "SwarmFrameScheme");
	pFrame->SetScheme(scheme);

	pFrame->SetTitle("#asw_vote_server", true);

	//pFrame->LoadControlSettings("Resource/UI/ScoreBoard.res");	
	pFrame->SetTitleBarVisible(true);
	pFrame->SetMoveable(false);
	pFrame->SetSizeable(false);
	pFrame->SetMenuButtonVisible(false);
	pFrame->SetMaximizeButtonVisible(false);
	pFrame->SetMinimizeToSysTrayButtonVisible(false);
	pFrame->SetMinimizeButtonVisible(false);
	pFrame->SetCloseButtonVisible(true);	

	pFrame->RequestFocus();
	pFrame->SetVisible(true);
	pFrame->SetEnabled(true);

	// make sure it shows up on the client panel

	if (enginevgui)
	{
		//vgui::VPANEL rootpanel = enginevgui->GetPanel( PANEL_CLIENTDLL );
		//pFrame->SetParent( rootpanel );

		/*
		// if another frame is up, make sure we have no transparency, since it's bugged and shows the level behind		
		vgui::Panel* pPanel = pFrame->GetParent();
		if (pPanel)
		{
		int iChildren = pPanel->GetChildCount();
		for (int i=0;i<iChildren;i++)
		{
		vgui::Panel *pChild = dynamic_cast<vgui::Frame*>(pPanel->GetChild(i));
		if (pChild && pChild != pFrame)
		{
		pFrame->RemoveTranslucency();
		break;
		}
		}
		}*/
	}	

	if (bAvoidTranslucency)
		pFrame->RemoveTranslucency();

	g_hChooserFrame = pFrame;
}

void CC_ASW_Main_Menu_Option( const CCommand &args )
{
	if ( args.ArgC() < 3 )
	{
		Msg("Usage: asw_main_menu_option [host type] [chooser type]");
		return;
	}
	int iHostType = atoi(args[1]);
	if (iHostType < 0 || iHostType > 2 )
		return;

	int iChooserType = atoi(args[2]);
	if (iChooserType < 0 || iChooserType > 2 )
		return;

	LaunchMissionChooser(iHostType, iChooserType);
}
static ConCommand asw_main_menu_option("asw_main_menu_option", CC_ASW_Main_Menu_Option, 0 );


void CC_ASW_Launch_Vote_Chooser( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg("Usage: asw_vote_chooser [chooser type]");
		return;
	}
	int iChooserType = atoi(args[1]);
	if ( iChooserType < -1 || iChooserType > 2 )
		return;

	// if third param is specified, we assume it's the 'notrans' one
	bool bAvoidTranslucency =(args.ArgC() >= 3);

	LaunchMissionVotePanel(iChooserType, bAvoidTranslucency);
}
static ConCommand asw_vote_chooser("asw_vote_chooser", CC_ASW_Launch_Vote_Chooser, 0 );