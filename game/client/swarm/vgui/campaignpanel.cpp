#include "cbase.h"
#include "CampaignPanel.h"
#include "MissionCompleteStatsLine.h"
#include "vgui_controls/AnimationController.h"
#include "RestartMissionButton.h"
#include "c_asw_player.h"
#include "c_asw_game_resource.h"
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
#include "CampaignMapLocation.h"
#include <vgui_controls/TextImage.h>
#include <vgui/ILocalize.h>
#include "CampaignMapSearchLights.h"
#include "controller_focus.h"
#include "clientmode_asw.h"
#include "soundenvelope.h"
#include "c_playerresource.h"
#include <vgui_controls/ProgressBar.h>
#include "nb_header_footer.h"
#include "nb_button.h"
#include "nb_commander_list.h"
#include "nb_campaign_mission_details.h"
#include "asw_briefing.h"
#include "gameui/swarm/uigamedata.h"
#include "nb_vote_panel.h"
#include <vgui/IVgui.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

extern ConVar asw_client_build_maps;

CampaignPanel::CampaignPanel(Panel *parent, const char *name) : vgui::EditablePanel(parent, name)
{	
	m_bCurrentAnimating = false;
	m_bVoted = false;
	m_iHighlightedMission = -1;
	m_bShowGalaxy = false;
	m_bSetAlpha = false;
	m_iLocationOver = -1;
	m_bAddedLines = false;

	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pHeaderFooter->SetTitleStyle( NB_TITLE_BRIGHT );

	m_pBackDrop = new vgui::ImagePanel(this, "CampaignBackdrop");
	m_pMapBorder = new vgui::Panel(this, "CampaignMapBorder");
	m_pGalacticMap = new vgui::ImagePanel(this, "CampaignGalacticMap");	
	m_pGalaxyLines = new vgui::ImagePanel(m_pGalacticMap, "GalaxyLines");
	m_pGalaxyEdges = new MapEdgesBox(this, "GalaxyEdges", Color(64,142,192,128));
	m_pGalaxyEdges->SetAlpha(0);	
	m_pGalaxyEdges->SetMouseInputEnabled(false);
	m_pSurfaceMap = new vgui::ImagePanel(this, "CampaignSurfaceMap");
	m_pSurfaceMapLayer[0]  = new vgui::ImagePanel(this, "CampaignSurfaceMapL0");
	m_pSurfaceMapLayer[1]  = new vgui::ImagePanel(this, "CampaignSurfaceMapL1");
	m_pSurfaceMapLayer[2]  = new vgui::ImagePanel(this, "CampaignSurfaceMapL2");
	//m_pSurfaceMap->SetMouseInputEnabled(false);
	m_pSurfaceMapLayer[0]->SetMouseInputEnabled(false);
	m_pSurfaceMapLayer[1]->SetMouseInputEnabled(false);
	m_pSurfaceMapLayer[2]->SetMouseInputEnabled(false);	
	m_pBracket = new ObjectiveMapMarkPanel(this, "CampaignBracket");
	m_pBracket->SetMouseInputEnabled(false);

	// now just setting to a small temp material because this isn't shown
	m_pBackDrop->SetImage("swarm/Briefing/ShadedButton");

	m_pGalacticMap->SetImage("swarm/Campaign/Jacob_GalacticMap");		
	m_pGalaxyLines->SetImage("swarm/Campaign/GalaxyLines");
	m_pSurfaceMap->SetImage("swarm/Campaign/JacobCampaignMap");
	m_pSurfaceMapLayer[0]->SetImage("swarm/Campaign/JacobCampaignMap");
	m_pSurfaceMapLayer[1]->SetImage("swarm/Campaign/JacobCampaignMap");
	m_pSurfaceMapLayer[2]->SetImage("swarm/Campaign/JacobCampaignMap");

	m_pBackDrop->SetShouldScaleImage(true);
	m_pGalacticMap->SetShouldScaleImage(true);
	m_pGalaxyLines->SetShouldScaleImage(true);
	m_pSurfaceMap->SetShouldScaleImage(true);
	m_pSurfaceMapLayer[0]->SetShouldScaleImage(true);
	m_pSurfaceMapLayer[1]->SetShouldScaleImage(true);
	m_pSurfaceMapLayer[2]->SetShouldScaleImage(true);

	m_pBackDrop->SetDrawColor(Color(66, 73, 99, 255));
	m_pBackDrop->SetAlpha(0);
	m_pGalacticMap->SetAlpha(0);
	m_pGalaxyLines->SetAlpha(0);
	m_pSurfaceMap->SetAlpha(0);
	m_pSurfaceMapLayer[0]->SetAlpha(0);
	m_pSurfaceMapLayer[1]->SetAlpha(0);
	m_pSurfaceMapLayer[2]->SetAlpha(0);
	m_pBracket->SetAlpha(0);
	m_bSetTitle = false;	

	m_pMapLabels[0] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_core_systems" );
	m_pMapLabels[1] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_syntek_megacorporation" );
	m_pMapLabels[2] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_trust_media_hub" );
	m_pMapLabels[3] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_humanist_protectorate" );
	m_pMapLabels[4] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_iaf_academy" );
	m_pMapLabels[5] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_paradise_supplies" );
	m_pMapLabels[6] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_enigma_research_group" );
	m_pMapLabels[7] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_united_industries" );
	m_pMapLabels[8] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_chosen" );
	m_pMapLabels[9] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_frontier_pharmaceuticals" );
	m_pMapLabels[10] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_penitentiary_cluster" );
	m_pMapLabels[11] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_void" );
	m_pMapLabels[12] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_infested_systems" );
	m_pMapLabels[13] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_hatal_9" );
	m_pMapLabels[14] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_anhaven_ruins" );
	m_pMapLabels[15] = new vgui::Label(m_pGalacticMap, "GalacticMapLabel", "#asw_galactic_map_jacobs_rest" );

	m_pMapEdges = new MapEdgesBox(this, "MapEdges", Color(64,142,192,128));
	m_pMapEdges->SetAlpha(0);	
	m_pMapEdges->SetMouseInputEnabled(false);
	m_pLights = new CampaignMapSearchLights(m_pSurfaceMap, "SearchLights");
	m_pLights->SetMouseInputEnabled(false);

	m_pCurrentLocationImage = new vgui::ImagePanel(m_pSurfaceMap, "CampaignCurrentLoc");
	m_pCurrentLocationImage->SetShouldScaleImage(true);
	m_pCurrentLocationImage->SetImage("swarm/Campaign/RedArrows");	
	m_pCurrentLocationImage->SetAlpha(0);	
	m_pCurrentLocationImage->SetZPos( 32 );
	

	m_pTimeLeftLabel = new vgui::Label(this, "TimeLeft", "");
	m_pTimeLeftLabel->SetAlpha(0);

	m_pMouseOverGlowLabel = new vgui::Label(m_pSurfaceMap, "MouseOverGlowLabel", " ");
	m_pMouseOverLabel = new vgui::Label(m_pSurfaceMap, "MouseOverLabel", " ");
	m_pMouseOverGlowLabel->SetVisible(false);
	m_pMouseOverLabel->SetVisible(false);
	m_pMouseOverLabel->SetContentAlignment(vgui::Label::a_north);
	m_pMouseOverGlowLabel->SetContentAlignment(vgui::Label::a_north);
	m_pMouseOverLabel->SetMouseInputEnabled(false);
	m_pMouseOverGlowLabel->SetMouseInputEnabled(false);

	m_pLeaderLabel = new vgui::Label( this, "LeaderLabel", "" );
	m_pLaunchButton = new CNB_Button( this, "LaunchButton", "", this, "LaunchButton" );
	m_pFriendsButton = new CNB_Button( this, "FriendsButton", "", this, "FriendsButton" );
	m_pCommanderList = new CNB_Commander_List( this, "CommanderList" );
	m_pMissionDetails = new CNB_Campaign_Mission_Details( this, "Campaign_Mission_Details" );

	char buffer[32];
	for (int i=0;i<ASW_MAX_MISSIONS_PER_CAMPAIGN;i++)
	{
		m_pLocationPanel[i] = new CampaignMapLocation(m_pSurfaceMap, "CampaignLocationDot", this, i);
		m_pLocationPanel[i]->SetVisible(false);
		Q_snprintf(buffer, sizeof(buffer), "Location %d", i);
		
		//m_pLocationPanel[i]->AddActionSignalTarget( this );
		//m_pLocationPanel[i]->SetCommand( buffer );
	}
	m_pButtonBorder = NULL;
	//m_pChatEcho = new ChatEchoPanel(this, "ChatEchoPanel");	

	m_iLastLabelBlipSound = -1;
	m_iStage = CMP_NONE;
	m_fNextStageTime = gpGlobals->curtime + 0.5f;

	m_pSoftLine.Purge();

	m_pProgressBar[0] = new vgui::ProgressBar(this, "ProgressBar");
	m_pProgressBar[0]->SetProgress(0.0f);	
	m_pProgressLabel[0] = new vgui::Label( this, "ProgressLabel", "Host" );
	for ( int i=1;i<ASW_MAX_READY_PLAYERS;i++ )
	{
		m_pProgressBar[i] = new vgui::ProgressBar(this, "ProgressBar");
		m_pProgressLabel[i] = new vgui::Label(this, "ProgressLabel", "Player" );
	}

	m_pVotePanel = new CNB_Vote_Panel( this, "VotePanel" );
	m_pVotePanel->m_VotePanelType = VPT_CAMPAIGN_MAP;
	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

CampaignPanel::~CampaignPanel()
{
}

// sets the starting positions of the elements in this panel, based on the supplied screen res
void CampaignPanel::PerformLayout()
{
	int width = ScreenWidth();
	int height = ScreenHeight();

	SetPos(0,0);	
	SetSize(width + 1, width + 1);

	m_pBracket->SetBracketScale(width / 1024.0f);
	
	if (m_pTimeLeftLabel)
	{		
		m_pTimeLeftLabel->SetPos(width * 0.71f, height * 0.88f);
		m_pTimeLeftLabel->SetSize(width * 0.3, height * 0.02f);
	}	

	int x,y,w,t;
	if (m_iStage < CMP_ZOOMING_SURFACE_MAP)	// position the map tiny if we haven't zoomed in yet
		GetConstellationBracket(x,y,w,t);
	else
		GetMapBounds(x,y,w,t);
	m_pSurfaceMap->SetBounds(x,y,w,t);
	m_pSurfaceMapLayer[0]->SetBounds(x,y,w,t);
	m_pSurfaceMapLayer[1]->SetBounds(x,y,w,t);
	m_pSurfaceMapLayer[2]->SetBounds(x,y,w,t);
	int bracket_inset = w * 0.012f;
	m_pMapEdges->SetBounds(x + bracket_inset, y + bracket_inset, w - (bracket_inset*2), t - (bracket_inset * 2));

	GetMapBounds(x,y,w,t);
	m_pGalacticMap->SetBounds(x,y,w,t);	
	m_pGalaxyLines->SetBounds(0, 0, w, t);
	bracket_inset = w * 0.012f;
	m_pGalaxyEdges->SetBounds(x + bracket_inset, y + bracket_inset, w - (bracket_inset*2), t - (bracket_inset * 2));
	float fFrameSize = 0.09f;
	m_pMapBorder->SetBounds(x-fFrameSize, y-fFrameSize, w + fFrameSize * 2, t + fFrameSize * 2);

	m_pBracket->SetBounds(x,y,w,t);

	if (m_pBackDrop)
	{
		m_pBackDrop->SetPos(0,0);	
		m_pBackDrop->SetSize(width + 1, width + 1);
	}

	// position the labels
	for (int i=0;i<ASW_NUM_CAMPAIGN_LABELS;i++)
	{
		m_pMapLabels[i]->SizeToContents();
		m_pMapLabels[i]->InvalidateLayout(true);
	}
	SetLabelPos(0, (310.0f / 1024.0f) * w, (689.0f / 1024.0f) * t);
	SetLabelPos(1, (393.0f / 1024.0f) * w, (459.0f / 1024.0f) * t);
	SetLabelPos(2, (372.0f / 1024.0f) * w, (806.0f / 1024.0f) * t);
	SetLabelPos(3, (61.0f / 1024.0f) * w, (573.0f / 1024.0f) * t);
	SetLabelPos(4, (167.0f / 1024.0f) * w, (792.0f / 1024.0f) * t);
	SetLabelPos(5, (515.0f / 1024.0f) * w, (729.0f / 1024.0f) * t);
	SetLabelPos(6, (196.0f / 1024.0f) * w, (878.0f / 1024.0f) * t);
	SetLabelPos(7, (158.0f / 1024.0f) * w, (361.0f / 1024.0f) * t);
	SetLabelPos(8, (35.0f / 1024.0f) * w, (704.0f / 1024.0f) * t);
	SetLabelPos(9, (480.0f / 1024.0f) * w, (910.0f / 1024.0f) * t);
	SetLabelPos(10, (717.0f / 1024.0f) * w, (614.0f / 1024.0f) * t);
	SetLabelPos(11, (897.0f / 1024.0f) * w, (702.0f / 1024.0f) * t);
	SetLabelPos(12, (751.0f / 1024.0f) * w, (272.0f / 1024.0f) * t);
	SetLabelPos(13, (799.0f / 1024.0f) * w, (667.0f / 1024.0f) * t);
	SetLabelPos(14, (807.0f / 1024.0f) * w, (536.0f / 1024.0f) * t);
	SetLabelPos(15, (597.0f / 1024.0f) * w, (304.0f / 1024.0f) * t);

// 	if (m_pChatEcho)
// 	{
// 		m_pChatEcho->SetPos(ScreenWidth() * 0.025f, height * 0.87f);
// 		m_pChatEcho->InvalidateLayout(true);
// 	}

	PositionSoftLines();
	PositionLocationDots();

	int border = height * 0.0125;
	int font_tall = height * 0.04f;
	int progress_window_top = height * 0.4f;
	float cursor_y = font_tall + progress_window_top;
	int progress_bar_height = height * 0.1f - ( (border * 2) + font_tall );
	for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
	{
		if ( !m_pProgressBar[i] )
			continue;

		m_pProgressBar[i]->SetBounds(width * 0.075f + border, cursor_y, width * 0.35f - (border * 2), progress_bar_height );

		if ( m_pProgressLabel[i] )
		{
			m_pProgressLabel[i]->SetContentAlignment(vgui::Label::a_east);
			m_pProgressLabel[i]->SetBounds(width * 0.075f + border, cursor_y, width * 0.5f - (border * 2), progress_bar_height );
		}

		cursor_y += progress_bar_height + border;
	}
}

// positions a map label, making sure it doesn't go outside the map bounds
void CampaignPanel::SetLabelPos(int iLabelIndex, int x, int y)
{
	int lx, ly;
	m_pMapLabels[iLabelIndex]->GetTextImage()->GetContentSize(lx, ly);

	int mx, my, mw, mt;
	GetMapBounds(mx, my, mw, mt);

	int padding = 28.0f * (ScreenHeight() / 768.0f);

	//Msg("SetLabelPos %d x %d y %d mx %d my %d mw %d mt %d lx %d ly %d", iLabelIndex, x, y, mx, my, mw, mt, lx, ly);

	if (x < padding)
		x = padding;

	if (y < padding)
		y = padding;

	if (x + lx > mw - padding)
		x = mw - (padding + lx);

	if (y + ly > mt - padding)
		y = mt - (padding + ly);

	m_pMapLabels[iLabelIndex]->SetPos(x, y);
}

// advance the campaign screen to the next stage in its animation when the time is hit
void CampaignPanel::OnThink()
{	
	if ( !Briefing() )
		return;	

	m_pFriendsButton->SetVisible( ! ( ASWGameResource() && ASWGameResource()->IsOfflineGame() ) );
	
	const char *pszLeaderName = Briefing()->GetLeaderName();
	if ( pszLeaderName )
	{
		m_pLeaderLabel->SetVisible( ! ( ASWGameResource() && ASWGameResource()->IsOfflineGame() ) );

		wchar_t wszPlayerName[32];
		g_pVGuiLocalize->ConvertANSIToUnicode( pszLeaderName, wszPlayerName, sizeof(wszPlayerName));

		wchar_t wszBuffer[128];
		g_pVGuiLocalize->ConstructString( wszBuffer, sizeof(wszBuffer), g_pVGuiLocalize->Find( "#nb_leader" ), 1, wszPlayerName );

		m_pLeaderLabel->SetText( wszBuffer );
	}
	else
	{
		m_pLeaderLabel->SetVisible( false );
	}

	C_ASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	C_ASW_Campaign_Save *pSave = pGameResource->GetCampaignSave();
	if (pSave && pSave->m_iNumMissionsComplete > 0)
		m_bShowGalaxy = false;
	if (m_fNextStageTime!=0 && gpGlobals->curtime > m_fNextStageTime)
	{
		SetStage(m_iStage + 1);
	}


	if (!ASWGameRules())
		return;

	UpdateMapGenerationStatus();
	
	if (pSave)
	{
		CASW_Campaign_Info *pCampaign = ASWGameRules()->GetCampaignInfo();
		if (pCampaign)
		{
			if (!m_bSetTitle)
			{
				m_bSetTitle = true;

				wchar_t campaignbuffer[128];
				g_pVGuiLocalize->ConvertANSIToUnicode(STRING( pCampaign->m_CampaignName ), campaignbuffer, sizeof( campaignbuffer ));

				wchar_t wbuffer[256];		
				g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
					g_pVGuiLocalize->Find("#nb_campaign_title"), 1,
					campaignbuffer );
				m_pHeaderFooter->SetTitle( wbuffer );

				m_pSurfaceMap->SetImage(STRING(pCampaign->m_CampaignTextureName));
				m_pSurfaceMapLayer[0]->SetImage(STRING(pCampaign->m_CampaignTextureLayer1));
				m_pSurfaceMapLayer[1]->SetImage(STRING(pCampaign->m_CampaignTextureLayer2));
				m_pSurfaceMapLayer[2]->SetImage(STRING(pCampaign->m_CampaignTextureLayer3));

				m_pBackDrop->SetImage(STRING(pCampaign->m_CampaignTextureName));

				if (!ASWGameRules() || !ASWGameRules()->IsIntroMap())
				{
					vgui::GetAnimationController()->RunAnimationCommand(m_pBackDrop, "alpha", 128, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
				}
			}
		}
	}
	// check for animating the current location
	if (m_iStage >= CMP_FADE_LOCATIONS_IN)
	{
		int x, y;
		m_pCurrentLocationImage->GetPos(x, y);		// location of red arrows
		
		C_ASW_Campaign_Save *pSave = pGameResource->GetCampaignSave();
		if (pSave)
		{
			// check if there's a timer on
			if (pSave->m_fVoteEndTime != 0)
			{
				int iTimeLeft = pSave->m_fVoteEndTime - gpGlobals->curtime;
				if (iTimeLeft > 0)
				{
					char buffer[8];
					Q_snprintf(buffer, sizeof(buffer), "%d", iTimeLeft);

					wchar_t wnumber[8];
					g_pVGuiLocalize->ConvertANSIToUnicode(buffer, wnumber, sizeof( wnumber ));

					wchar_t wbuffer[96];		
					g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
						g_pVGuiLocalize->Find("#asw_mission_launch_in"), 1,
							wnumber);
					m_pTimeLeftLabel->SetText(wbuffer);
				}
				else
					m_pTimeLeftLabel->SetText("");
			}
			CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission = GetCampaignInfo()->GetMission(pSave->m_iCurrentPosition);
			if (pMission)
			{
				int iMapX, iMapY, iMapW, iMapT;
				GetMapBounds(iMapX, iMapY, iMapW, iMapT);
				
				float dot_w = iMapW * 0.1f;
				float pos_x = (pMission->m_iLocationX / 1024.0f) * iMapW;
				float pos_y = (pMission->m_iLocationY / 1024.0f) * iMapT;
				pos_x -= (dot_w * 0.5f);	// make sure the dots are centered
				pos_y -= (dot_w * 0.5f);

				// if we're animating, check our destination is the same, if not, unflag animating so a new animation dest gets set in
				if (m_bCurrentAnimating)
				{
					if (m_CurrentAnimatingToX != int(pos_x)
						|| m_CurrentAnimatingToY != int(pos_y))
					{
						m_bCurrentAnimating = false;
					}
				}
				
				if (!m_bCurrentAnimating)
				{
					m_CurrentAnimatingToX = int(pos_x);		// x/y our current location on the map as specified by the save
					m_CurrentAnimatingToY = int(pos_y);

					if (m_CurrentAnimatingToX != x || m_CurrentAnimatingToY != y)	// if red arrows and current save loc don't match up, it's time to do some animating
					{
						m_bCurrentAnimating = true;						
						vgui::GetAnimationController()->RunAnimationCommand(m_pCurrentLocationImage, "xpos", m_CurrentAnimatingToX, 0, 0.8f, vgui::AnimationController::INTERPOLATOR_LINEAR);
						vgui::GetAnimationController()->RunAnimationCommand(m_pCurrentLocationImage, "ypos", m_CurrentAnimatingToY, 0, 0.8f, vgui::AnimationController::INTERPOLATOR_LINEAR);

						CLocalPlayerFilter filter;
						C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.AreaBrackets", 0, 0 );
					}
				}
			}
		}
	}
	if (m_iStage < CMP_WAITING && m_bShowGalaxy)
	{
		// check for galaxy labels needing to make a sound
		for (int i=m_iLastLabelBlipSound+1;i<ASW_NUM_CAMPAIGN_LABELS;i++)
		{
			if (m_pMapLabels[i]->IsVisible() && m_pMapLabels[i]->GetAlpha() > 1)
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Teletype3", 0, 0 );				
				m_iLastLabelBlipSound = i;
			}
		}		
	}
	// check for finishing an animation
	if (m_bCurrentAnimating)
	{
		int x, y;
		m_pCurrentLocationImage->GetPos(x, y);
		if (m_CurrentAnimatingToX == x && m_CurrentAnimatingToY == y)
		{
			m_bCurrentAnimating = false;			
		}
	}
	if (m_iStage >= CMP_FADE_LOCATIONS_IN)
	{
		bool bOver = false;
		for (int i=0;i<ASW_MAX_MISSIONS_PER_CAMPAIGN;i++)
		{
			if (m_pLocationPanel[i] && m_pLocationPanel[i]->IsVisible() && m_pLocationPanel[i]->IsCursorOver())
			{
				CASW_Campaign_Info *pCampaign = ASWGameRules()->GetCampaignInfo();
				if (!pCampaign)
					return;

				CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission = pCampaign->GetMission(i);
				if (!pMission)
					return;

				CASW_Campaign_Save *pSave = ASWGameRules()->GetCampaignSave();
				if (!pSave)
					return;

				// if mission is already completed, just show it as a grey label
				bool bComplete = (pSave->m_MissionComplete[i] != 0) || i <= 0;

				if (!bComplete)
					m_pMouseOverGlowLabel->SetVisible(true);
				else
					m_pMouseOverGlowLabel->SetVisible(false);
				m_pMouseOverLabel->SetVisible(true);
				char buffer[128];
				Q_snprintf(buffer,sizeof(buffer), " %s ", pMission->m_LocationDescription);
				m_pMouseOverLabel->SetText(buffer);	
				m_pMouseOverGlowLabel->SetText(buffer);
				m_pMouseOverLabel->InvalidateLayout(true);
				m_pMouseOverLabel->GetTextImage()->ResizeImageToContent();
				m_pMouseOverLabel->SizeToContents();
				m_pMouseOverLabel->InvalidateLayout(true);
				int lw, lt;
				m_pMouseOverLabel->GetSize(lw, lt);
				m_pMouseOverGlowLabel->SetSize(lw, lt);
				m_pMouseOverGlowLabel->InvalidateLayout(true);
				m_pMouseOverLabel->GetTextImage()->GetContentSize(lw, lt);

				// set their colours here, since these commands are apparently ignored in applyschemesettings..
				m_pMouseOverGlowLabel->SetFgColor(Color(66,142,192,255));
				if (bComplete)
					m_pMouseOverLabel->SetFgColor(Color(128,128,128,255));
				else
					m_pMouseOverLabel->SetFgColor(Color(255,255,255,255));
				
				int x, y, w, t;
				m_pLocationPanel[i]->GetBounds(x,y,w,t);
				x += w*0.5f;
				y += t*1.1f;

				// clamp
				int labelx = x - lw * 0.5f;
				int labely = y;
				int mapx, mapy, mapw, mapt;				
				GetMapBounds(mapx, mapy, mapw, mapt);
				int padding = 10 * (float(mapw) / 1024.0f);
				if (labelx > mapw - padding)
					labelx = mapw - padding;
				if (labely > mapt - padding)
					labely = mapt - padding;
				if (labelx < padding)
					labelx = padding;
				if (labely < padding)
					labely = padding;
				m_pMouseOverLabel->SetPos(labelx, labely);
				m_pMouseOverGlowLabel->SetPos(labelx, labely);
				bOver = true;	
				if (m_iLocationOver != i)
				{
					m_iLocationOver = i;
					CLocalPlayerFilter filter;
					C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.TooltipBox" );
				}
				//LocationOver(i);
				break;
			}
		}
		if (!bOver)
		{
			m_pMouseOverGlowLabel->SetVisible(false);
			m_pMouseOverLabel->SetVisible(false);
			m_iLocationOver = -1;
		}
	}

	UpdateLocationLabels();

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	m_pLaunchButton->SetVisible( pPlayer && ASWGameResource() && pPlayer == ASWGameResource()->GetLeader() );
}

void CampaignPanel::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	LoadControlSettings( "resource/ui/campaign_panel.res" );

	if (!m_bSetAlpha)
	{
		SetAlpha(0);
		m_pBackDrop->SetAlpha(0);
		m_pGalacticMap->SetAlpha(0);
		m_pGalaxyLines->SetAlpha(0);
		m_pGalaxyEdges->SetAlpha(0);
		m_pSurfaceMap->SetAlpha(0);
		m_pMapEdges->SetAlpha(0);
		m_pSurfaceMapLayer[0]->SetAlpha(0);
		m_pSurfaceMapLayer[1]->SetAlpha(0);
		m_pSurfaceMapLayer[2]->SetAlpha(0);
		m_pBracket->SetAlpha(0);
		m_pCurrentLocationImage->SetAlpha(0);	
	}
	SetBgColor(Color(0,0,0,0));	

	if (ASWGameRules() && ASWGameRules()->IsIntroMap())
	{
		SetPaintBackgroundEnabled(false);
	}
	else
	{
		SetPaintBackgroundEnabled(true);
		SetPaintBackgroundType(0);
		SetBgColor(Color(0,0,0,255));
	}
	m_pMapBorder->SetPaintBackgroundEnabled(true);
	m_pMapBorder->SetBgColor(Color(0,0,0,128));

	m_LargeFont = scheme->GetFont( "DefaultLarge", IsProportional() );
	m_pTimeLeftLabel->SetFgColor(scheme->GetColor("LightBlue", Color(128,128,128,255)));
	m_pMouseOverLabel->SetFont( scheme->GetFont( "Default", IsProportional() ) );
	m_pMouseOverGlowLabel->SetFont( scheme->GetFont( "DefaultBlur", IsProportional() ) );
	m_pMouseOverGlowLabel->SetFgColor(scheme->GetColor("LightBlue", Color(128,128,128,255)));
	m_pMouseOverLabel->SetFgColor(scheme->GetColor("White", Color(255,255,255,255)));
	for (int i=0;i<ASW_NUM_CAMPAIGN_LABELS;i++)
	{
		if (!m_bSetAlpha)
			m_pMapLabels[i]->SetAlpha(0);
		m_pMapLabels[i]->SetBgColor(Color(0,0,0,255));
		m_pMapLabels[i]->SetFgColor(scheme->GetColor("White", Color(255,255,255,255)));
		m_pMapLabels[i]->SetContentAlignment(vgui::Label::a_center);
		m_pMapLabels[i]->SetFont( scheme->GetFont( "DefaultSmall", IsProportional() ) );
		m_pMapLabels[i]->SetMouseInputEnabled(false);
		m_pMapLabels[i]->SetBorder(scheme->GetBorder("ASWMapLabelBorder"));
		m_pMapLabels[i]->SetPaintBorderEnabled(true);
	}
	for (int i=0;i<ASW_MAX_MISSIONS_PER_CAMPAIGN;i++)
	{
		if (!m_bSetAlpha)
			m_pLocationPanel[i]->SetAlpha(0);
		//m_pMapLabels[i]->SetFgColor(scheme->GetColor("White", Color(255,255,255,255)));
		//m_pLocationPanel[i]->SetBgColor(Color(0,0,0,255));
		m_pLocationPanel[i]->SetMouseInputEnabled(true);
		//m_pLocationPanel[i]->SetShouldScaleImage(true);
	}
	m_pButtonBorder = scheme->GetBorder("ASWBriefingButtonBorder");	

	m_bSetAlpha = true;
}

// moves the campaign screen to the specified stage, triggering the necessary animations
//  note: this function pretty much assumes you're coming from the previous stage
void CampaignPanel::SetStage(int i)
{
	int iMapX, iMapY, iMapW, iMapT;
	int bracketx, brackety, bracketw, brackett;
	float fFadeTime = 0.1f;
	float fMediumFadeTime = 0.5f;
	float fBracketTime = 0.4f;
	float fInitialMapLabelDelay = 2.5f;
	float fMapLabelInterval = 0.05f;
	GetMapBounds(iMapX, iMapY, iMapW, iMapT);

	m_iStage = i;
	
	switch (i)
	{
	case CMP_FADING_IN:		// fade all the starting elements in		
		{
			//Msg("CMP_FADING_IN\n");
			vgui::GetAnimationController()->RunAnimationCommand(this, "alpha", 255, 0, fMediumFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);			
						
			vgui::GetAnimationController()->RunAnimationCommand(m_pTimeLeftLabel, "alpha", 255, 0, fMediumFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pMapBorder, "alpha", 255, 0, fMediumFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pCommanderList, "alpha", 255, 0, fMediumFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);			

			if (m_bShowGalaxy)
			{
				vgui::GetAnimationController()->RunAnimationCommand(m_pGalacticMap, "alpha", 255, 0, fMediumFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
				vgui::GetAnimationController()->RunAnimationCommand(m_pGalaxyLines, "alpha", 128, 1.0f, fMediumFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);				
				
				vgui::GetAnimationController()->RunAnimationCommand(m_pGalaxyEdges, "alpha", 128, 0, fMediumFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
				
				// fade in all the galactic map labels
				for (int i=0;i<ASW_NUM_CAMPAIGN_LABELS;i++)
				{
					float fLabelDelay = fInitialMapLabelDelay + (i*fMapLabelInterval);
					vgui::GetAnimationController()->RunAnimationCommand(m_pMapLabels[i], "alpha", 255, fLabelDelay, fMediumFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
										
					m_pMapLabels[i]->SizeToContents();
					m_pMapLabels[i]->SetSize(m_pMapLabels[i]->GetWide() + 6, m_pMapLabels[i]->GetTall() + 2);
				}
				// update right side messages
				vgui::GetAnimationController()->RunAnimationCommand(m_pMissionDetails, "alpha", 255, 0, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);				
				
				m_fNextStageTime = gpGlobals->curtime + 1.0f;
				
			}
			else
			{
				vgui::GetAnimationController()->RunAnimationCommand(m_pMissionDetails, "alpha", 255, 0, 0.3f, vgui::AnimationController::INTERPOLATOR_LINEAR);
				
				m_fNextStageTime = gpGlobals->curtime + 0.1f;

				GetSurfaceBracket(bracketx, brackety, bracketw, brackett);
				m_pSurfaceMap->SetBounds(bracketx, brackety, bracketw, brackett);			
				
				vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMap, "alpha", 255, 0, fFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
				m_pSurfaceMap->SetBounds(iMapX, iMapY, iMapW, iMapT);
				for (int k=0;k<3;k++)
				{
					m_pSurfaceMapLayer[k]->SetBounds(iMapX, iMapY, iMapW, iMapT);
					vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMapLayer[k], "alpha", 255, 0, fFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);					
				}
				int bracket_inset = iMapW * 0.012f;			
				vgui::GetAnimationController()->RunAnimationCommand(m_pMapEdges, "alpha", 128, 0, fFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
				m_pMapEdges->SetBounds(iMapX+ bracket_inset, iMapY+ bracket_inset, iMapW- (bracket_inset*2), iMapT- (bracket_inset*2));
				m_pBracket->SetAlpha(0);

				SetStage(CMP_FADE_LOCATIONS_IN);	// skip ahead				
			}
		}
		break;
	case CMP_LINES_SOUND:
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.MissionBoxes", 0, 0 );

			m_fNextStageTime = gpGlobals->curtime + fMediumFadeTime * 2 + 0.1f + 4.0f;
		}
		break;
	case CMP_BRACKETING_CONSTELLATION:		
		{
			//Msg("CMP_BRACKETING_CONSTELLATION\n");
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "alpha", 255, 0, fFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			float fBracketTime = 0.4f;
			GetConstellationBracket(bracketx, brackety, bracketw, brackett);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "xpos", bracketx, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "ypos", brackety, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "wide", bracketw, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "tall", brackett, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.AreaBrackets", 0, 0 );
			m_fNextStageTime = gpGlobals->curtime + fBracketTime * 1.5f;
		}
		break;
	case CMP_ZOOMING_SURFACE_MAP:
		{
			//Msg("CMP_ZOOMING_SURFACE_MAP\n");
			
			GetSurfaceBracket(bracketx, brackety, bracketw, brackett);
			m_pSurfaceMap->SetBounds(bracketx, brackety, bracketw, brackett);			
			
			vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMap, "alpha", 255, 0, fFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMap, "xpos", iMapX, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMap, "ypos", iMapY, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMap, "wide", iMapW, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMap, "tall", iMapT, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);

			for (int k=0;k<3;k++)
			{
				m_pSurfaceMapLayer[k]->SetBounds(bracketx, brackety, bracketw, brackett);
				vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMapLayer[k], "alpha", 255, 0, fFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
				vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMapLayer[k], "xpos", iMapX, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
				vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMapLayer[k], "ypos", iMapY, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
				vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMapLayer[k], "wide", iMapW, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
				vgui::GetAnimationController()->RunAnimationCommand(m_pSurfaceMapLayer[k], "tall", iMapT, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			}
			int bracket_inset = iMapW * 0.012f;			
			vgui::GetAnimationController()->RunAnimationCommand(m_pMapEdges, "alpha", 128, 0, fFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pMapEdges, "xpos", iMapX+ bracket_inset, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pMapEdges, "ypos", iMapY+ bracket_inset, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pMapEdges, "wide", iMapW- (bracket_inset*2), 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pMapEdges, "tall", iMapT- (bracket_inset*2), 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);

			// make the brackets expand with the map too		
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "alpha", 255, 0, fFadeTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "xpos", iMapX, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "ypos", iMapY, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "wide", iMapW, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "tall", iMapT, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			vgui::GetAnimationController()->RunAnimationCommand(m_pBracket, "alpha", 0, 0, fBracketTime, vgui::AnimationController::INTERPOLATOR_LINEAR);
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.ObjectiveSlide", 0, 0 );

			m_fNextStageTime = gpGlobals->curtime + fBracketTime * 1.5f;			
		}
		break;
	case CMP_FADE_LOCATIONS_IN:
		{
			//Msg("CMP_FADE_LOCATIONS_IN\n");			
			if (!GetCampaignInfo() || !GetCampaignSave())	// if we don't have the campaign info from the server yet, then wait a bit and try again
			{
				m_iStage--;
				m_fNextStageTime = gpGlobals->curtime + 1.0f;
			}
			else
			{	
				float dot_w = iMapW * 0.05f;

				AddSoftLines();
				
				for (int i=0;i<GetCampaignInfo()->GetNumMissions();i++)
				{
					CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission = GetCampaignInfo()->GetMission(i);
					if (pMission && IsMissionVisible(i))
					{
						float pos_x = (pMission->m_iLocationX / 1024.0f) * iMapW;
						float pos_y = (pMission->m_iLocationY / 1024.0f) * iMapT;

						pos_x -= (dot_w * 0.5f);	// make sure the dots are centered
						pos_y -= (dot_w * 0.5f);

						if (i == GetCampaignSave()->m_iCurrentPosition)
						{
							m_CurrentAnimatingToX = pos_x;
							m_CurrentAnimatingToY = pos_y;

							if (GetControllerFocus() && GetControllerFocus()->IsControllerMode())
							{
								GetControllerFocus()->SetFocusPanel(m_pLocationPanel[i], false);
							}
						}

						m_pLocationPanel[i]->SetVisible(true);
						vgui::GetAnimationController()->RunAnimationCommand(m_pLocationPanel[i], "alpha", 255, 0, fFadeTime * 3, vgui::AnimationController::INTERPOLATOR_LINEAR);
					}
				}
				PositionLocationDots();
				C_ASW_Campaign_Save *pSave = ASWGameRules()->GetCampaignSave();
				if (pSave)
				{
					m_iHighlightedMission = pSave->m_iCurrentPosition;
				}							
				vgui::GetAnimationController()->RunAnimationCommand(m_pCurrentLocationImage, "alpha", 255, 0.2f, fFadeTime * 3, vgui::AnimationController::INTERPOLATOR_LINEAR);
				m_pCurrentLocationImage->SetShouldScaleImage(true);												
			
				m_fNextStageTime = gpGlobals->curtime + fFadeTime * 5.0f;
			}
		}
		break;
	case CMP_WAITING:
	default:
		{
			m_fNextStageTime = gpGlobals->curtime + 30;
		}
		break;
	}
}

void CampaignPanel::GetMapBounds(int &x, int &y, int &w, int &t)
{
	x = ScreenWidth() * 0.5f - YRES( 288 );
	y = YRES( 50 );
	w = YRES( 336 );
	t = w;
}

void CampaignPanel::GetSurfaceBracket(int &x, int &y, int &w, int &t)
{
	w = t = GetWide() * 0.02f;
	if (!ASWGameRules() || !ASWGameRules()->GetCampaignInfo())
	{
		x = y = GetWide() * 0.5f;
		return;
	}
	int mx ,my;
	int iMapX, iMapY, iMapW, iMapT;
	GetMapBounds(iMapX, iMapY, iMapW, iMapT);
	ASWGameRules()->GetCampaignInfo()->GetGalaxyPos(mx, my);
	x = (mx / 1024.0f) * iMapW + iMapX;
	y = (my / 1024.0f) * iMapT + iMapY;	
}

CASW_Campaign_Info* CampaignPanel::GetCampaignInfo()
{
	if (!ASWGameRules())
		return NULL;

	return ASWGameRules()->GetCampaignInfo();
}

C_ASW_Campaign_Save* CampaignPanel::GetCampaignSave()
{
	if (!ASWGameRules())
		return NULL;

	if (!ASWGameResource())
		return NULL;

	return ASWGameResource()->GetCampaignSave();
}

void CampaignPanel::AddSoftLines()
{
	if (!GetCampaignInfo())
		return;

	m_pSoftLine.Purge();
	
	for (int i=0;i<GetCampaignInfo()->GetNumMissions();i++)
	{
		for (int k=0;k<GetCampaignInfo()->GetNumMissions();k++)
		{
			if (!IsMissionVisible(i) || !IsMissionVisible(k))
				continue;
			if (GetCampaignInfo()->AreMissionsLinked(i, k))
			{
				SoftLine *pLine = new SoftLine(m_pSurfaceMap, "SoftLine", Color(0,0,0,255));
				m_pSoftLine.AddToTail(pLine);
				pLine->SetAlpha(0);
				vgui::GetAnimationController()->RunAnimationCommand(pLine, "alpha", 255, 0, 0.1f, vgui::AnimationController::INTERPOLATOR_LINEAR);
				m_pMouseOverLabel->MoveToFront();
				m_pMouseOverGlowLabel->MoveToFront();
			}
		}
	}

	m_bAddedLines = true;
	PositionSoftLines();
}

void CampaignPanel::PositionSoftLines()
{
	if (!m_bAddedLines)
		return;

	if (!GetCampaignInfo())
		return;
	// draw soft lines between each of the places that are connected
	int iMapX, iMapY, iMapW, iMapT;
	GetMapBounds(iMapX, iMapY, iMapW, iMapT);
	//Msg("Map bounds are %d,%d %d,%d\n", iMapX, iMapY, iMapW, iMapT);
	int iLineNum = 0;
	for (int i=0;i<GetCampaignInfo()->GetNumMissions();i++)
	{
		for (int k=0;k<GetCampaignInfo()->GetNumMissions();k++)
		{
			if (!IsMissionVisible(i) || !IsMissionVisible(k))
				continue;
			if (GetCampaignInfo()->AreMissionsLinked(i, k))
			{
				CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission1 = GetCampaignInfo()->GetMission(i);
				CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission2 = GetCampaignInfo()->GetMission(k);
				if (pMission1 && pMission2)
				{					
					float pos_x = (pMission1->m_iLocationX / 1024.0f) * iMapW;
					float pos_y = (pMission1->m_iLocationY / 1024.0f) * iMapT;	

					float pos_x2 = (pMission2->m_iLocationX / 1024.0f) * iMapW;
					float pos_y2 = (pMission2->m_iLocationY / 1024.0f) * iMapT;

					bool bSwappedX = false;
					if (pos_x > pos_x2)
					{
						int t = pos_x;
						pos_x = pos_x2;
						pos_x2 = t;
						bSwappedX = true;
					}
					bool bSwappedY = false;
					if (pos_y > pos_y2)
					{
						int t = pos_y;
						pos_y = pos_y2;
						pos_y2 = t;
						bSwappedY = true;
					}
					
					SoftLine *pLine = m_pSoftLine[iLineNum];
					if (!pLine)
						return;

					pLine->SetBounds(pos_x, pos_y, pos_x2 - pos_x, pos_y2 - pos_y);
					if ((bSwappedX && !bSwappedY)
						|| (bSwappedY && !bSwappedX))
					{
						pLine->SetCornerType(1);
					}
					iLineNum++;
				}				
			}
		}
	}
}

void CampaignPanel::PositionLocationDots()
{
	int iMapX, iMapY, iMapW, iMapT;
	GetMapBounds(iMapX, iMapY, iMapW, iMapT);
	float dot_w = iMapW * 0.05f;
	float current_location_x = 0;
	float current_location_y = 0;	
	
	if (!GetCampaignInfo())
		return;

	for (int i=0;i<GetCampaignInfo()->GetNumMissions();i++)
	{
		CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission = GetCampaignInfo()->GetMission(i);
		if (pMission && IsMissionVisible(i))
		{
			float pos_x = (pMission->m_iLocationX / 1024.0f) * iMapW;
			float pos_y = (pMission->m_iLocationY / 1024.0f) * iMapT;

			pos_x -= (dot_w * 0.5f);	// make sure the dots are centered
			pos_y -= (dot_w * 0.5f);

			m_pLocationPanel[i]->SetVisible(true);
			m_pLocationPanel[i]->SetPos(pos_x, pos_y);
			m_pLocationPanel[i]->ResizeTo(dot_w, dot_w);
			m_pLocationPanel[i]->SetMouseInputEnabled(true);
			m_pLocationPanel[i]->MoveToFront();	

			if (GetCampaignSave() && i == GetCampaignSave()->m_iCurrentPosition)
			{
				current_location_x = pos_x;
				current_location_y = pos_y;
			}
		}
	}

	m_pCurrentLocationImage->SetSize(dot_w*2, dot_w*2);
	m_pCurrentLocationImage->SetPos(current_location_x, current_location_y);
}

void CampaignPanel::Paint()
{
	BaseClass::Paint();
}

void CampaignPanel::OnCommand(const char *command)
{	
	if ( !Q_stricmp( command, "LaunchButton" ) )
	{		
		engine->ClientCmd( "cl_forcelaunch" );
	}
	else if ( !Q_stricmp( command, "FriendsButton" ) )
	{
#ifndef _X360 
		if ( BaseModUI::CUIGameData::Get() )
		{
			BaseModUI::CUIGameData::Get()->ExecuteOverlayCommand( "LobbyInvite" );
		}
#endif
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

void CampaignPanel::LocationOver(int iMission)
{
	SetHighlightedMission(iMission);
	FadeOutLocationLabels();
	UpdateLocationLabels();	// todo: make this get called after the fadeout has happened
}

void CampaignPanel::LocationClicked(int iMission)
{	
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer || !ASWGameResource() )
		return;
	
	// if you're not leader, you can't select the next mission
	if ( ASWGameResource()->GetLeader() != pPlayer )
		return;

	C_ASW_Campaign_Save *pSave = ASWGameRules()->GetCampaignSave();
	if (!pSave)	
		return;

	// don't allow vote for this mission if this mission was already completed
	if (pSave->m_MissionComplete[iMission] != 0 || iMission <= 0)
		return;

	// make sure the mission they want to vote for is reachable
	if (!pSave->IsMissionLinkedToACompleteMission(iMission, ASWGameRules()->GetCampaignInfo()))
		return;	

	pPlayer->NextCampaignMission(iMission);

	// skip the sound if we're a spectator
	if (GetClientModeASW() && GetClientModeASW()->m_bSpectator)
	{
		CASW_Game_Resource *pGameResource = ASWGameResource();
		if (pGameResource && pGameResource->IsPlayerReady(pPlayer))
			return;
	}
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWInterface.Button3", 0, 0 );
}

void CampaignPanel::SetHighlightedMission(int iMission)
{
	if (m_iHighlightedMission != -1)
	{
		m_pLocationPanel[m_iHighlightedMission]->SetSelected(false);
	}
	m_iHighlightedMission = iMission;
	m_pLocationPanel[m_iHighlightedMission]->SetSelected(true);
}

// updates the text on the right to the current location and fades it in
void CampaignPanel::UpdateLocationLabels()
{
	CASW_Campaign_Info *pCampaign = ASWGameRules()->GetCampaignInfo();
	if ( !pCampaign || !ASWGameResource() )
		return;

	int nNextMission = ASWGameResource()->GetNextCampaignMissionIndex();

	CASW_Campaign_Info::CASW_Campaign_Mission_t* pMission = pCampaign->GetMission(nNextMission);
	if (!pMission)
		return;

	C_ASW_Campaign_Save *pSave = ASWGameRules()->GetCampaignSave();
	if (!pSave)	
		return;

	m_pMissionDetails->SetCurrentMission( pMission );
	m_pMissionDetails->SetAlpha(255);
}

void CampaignPanel::FadeOutLocationLabels()
{
	m_pMissionDetails->SetAlpha(0);	
}

void CampaignPanel::GetConstellationBracket(int &x, int &y, int &w, int &t)
{
	GetSurfaceBracket(x, y, w, t);
}

bool CampaignPanel::IsMissionVisible(int i)
{
	if (!ASWGameRules() || !ASWGameRules()->GetCampaignSave())
		return false;
	if (i<0 || i>=ASW_MAX_MISSIONS_PER_CAMPAIGN)
		return false;

	if (i==0)	// drop zone always visible
		return true;

	CASW_Campaign_Info *pInfo = ASWGameRules()->GetCampaignInfo();
	if ( !pInfo )
		return false;

	CASW_Campaign_Info::CASW_Campaign_Mission_t *pMission = pInfo->GetMission( i );
	if ( !pMission )
		return false;

	if ( pMission->m_bAlwaysVisible )
		return true;

	C_ASW_Campaign_Save* pSave = ASWGameRules()->GetCampaignSave();
	if (pSave->m_MissionComplete[i])
		return true;
	
	return pSave->IsMissionLinkedToACompleteMission(i, ASWGameRules()->GetCampaignInfo());
}

void CampaignPanel::UpdateMapGenerationStatus()
{
	if ( !ASWGameResource() )
		return;

	if ( Q_strlen( ASWGameResource()->m_szMapGenerationStatus ) <= 0 )
	{
		for ( int i=0;i<ASW_MAX_READY_PLAYERS;i++ )
		{
			if ( m_pProgressBar[i] )
			{
				m_pProgressBar[i]->SetVisible( false );
			}
			if ( m_pProgressLabel[i] )
			{
				m_pProgressLabel[i]->SetVisible( false );
			}
		}
	}
	else
	{		
		m_pProgressBar[0]->SetVisible( true );
		m_pProgressBar[0]->SetProgress( ASWGameResource()->m_fMapGenerationProgress );
		m_pProgressLabel[0]->SetVisible( true );

		if ( asw_client_build_maps.GetBool() )
		{
			for ( int i = 1; i<= gpGlobals->maxClients; i++)
			{
				C_ASW_Player *pPlayer = dynamic_cast<C_ASW_Player*>( UTIL_PlayerByIndex( i ) );
				bool bListenServerPlayer = false; //!engine->IsDedicatedServer() && ( i==1 ); // TODO
				if ( pPlayer && g_PR && g_PR->IsConnected(i) && !bListenServerPlayer )
				{
					if ( m_pProgressBar[i] )
					{
						m_pProgressBar[i]->SetVisible( true );
						m_pProgressBar[i]->SetProgress( pPlayer->m_fMapGenerationProgress );
					}

					if ( m_pProgressLabel[i] )
					{
						m_pProgressLabel[i]->SetVisible( true );
						m_pProgressLabel[i]->SetText( g_PR->GetPlayerName( i ) );
					}
				}
				else
				{
					if ( m_pProgressBar[i] )
					{
						m_pProgressBar[i]->SetVisible( false );
					}
					if ( m_pProgressLabel[i] )
					{
						m_pProgressLabel[i]->SetVisible( false );
					}
				}
			}
		}
		else
		{
			for ( int i = 1; i < ASW_MAX_READY_PLAYERS; i++)
			{
				if ( m_pProgressBar[i] )
				{
					m_pProgressBar[i]->SetVisible( false );
				}
				if ( m_pProgressLabel[i] )
				{
					m_pProgressLabel[i]->SetVisible( false );
				}
			}
		}
	}	
}

void CampaignPanel::OnTick()
{
	BaseClass::OnTick();

	if ( ASWGameRules() && ASWGameRules()->GetCurrentVoteType() != ASW_VOTE_NONE )
	{
		m_pCommanderList->SetVisible( false );
		return;
	}
	m_pCommanderList->SetVisible( true );
}