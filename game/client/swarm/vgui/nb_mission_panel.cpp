#include "cbase.h"
#include "nb_mission_panel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Button.h"
#include <vgui/ILocalize.h>
#include "ObjectivePanel.h"
#include "ObjectiveListBox.h"
#include "ObjectiveDetailsPanel.h"
#include "ObjectiveMap.h"
#include "ObjectiveIcons.h"
#include "asw_hud_minimap.h"
#include "asw_gamerules.h"
#include "c_asw_campaign_save.h"
#include "nb_header_footer.h"
#include "nb_button.h"
#include "gameui/swarm/vdropdownmenu.h"
#include "c_asw_player.h"
#include "c_asw_game_resource.h"
#include "asw_input.h"
#include "nb_island.h"
#include "gameui/swarm/basemodpanel.h"
#include "gameui/swarm/VFooterPanel.h"

using namespace vgui;

#define MAP_SIZE 0.66666f


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNB_Mission_Panel::CNB_Mission_Panel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pObjectiveListBoxIsland = new CNB_Island( this, "ObjectiveListBoxIsland" );
	m_pObjectiveListBoxIsland->m_pTitle->SetText( "#nb_objectives" );
	m_pObjectiveMapIsland = new CNB_Island( this, "ObjectiveMapIsland" );
	m_pObjectiveMapIsland->m_pTitle->SetText( "#nb_overview_map" );
	m_pObjectiveDetailsIsland = new CNB_Island( this, "ObjectiveDetailsIsland" );
	m_pObjectiveDetailsIsland->m_pTitle->SetText( "#nb_objective_details" );
	m_pDifficultyIsland = new CNB_Island( this, "DifficultyIsland" );
	m_pDifficultyIsland->m_pTitle->SetText( "#nb_mission_options" );
	m_pDifficultyDescription = new vgui::Label( this, "DifficultyDescription", "" );
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pRetriesLabel = new vgui::Label( this, "RetriesLabel", "" );
	// == MANAGED_MEMBER_CREATION_END ==
	m_pBackButton = new CNB_Button( this, "BackButton", "", this, "BackButton" );
	m_drpDifficulty = new BaseModUI::DropDownMenu( this, "DrpDifficulty" );
	m_drpFriendlyFire = new BaseModUI::DropDownMenu( this, "DrpFriendlyFire" );
	m_drpOnslaught = new BaseModUI::DropDownMenu( this, "DrpOnslaught" );
	m_drpFixedSkillPoints = new BaseModUI::DropDownMenu( this, "DrpFixedSkillPoints" );

	m_pHeaderFooter->SetTitle( "#nb_mission_details" );

	m_pObjectiveList = new ObjectiveListBox(this, "objectivelistbox");
	m_pObjectiveDetails = new ObjectiveDetailsPanel(this, "objectivedetails");
	m_pObjectiveMap = new ObjectiveMap(this, "objectivemap");

	m_pObjectiveMap->SetDetailsPanel(m_pObjectiveDetails);
	m_pObjectiveList->SetDetailsPanel(m_pObjectiveDetails);
	m_pObjectiveList->SetMapPanel(m_pObjectiveMap);

	m_bSelectedFirstObjective = false;

	m_iLastSkillLevel = -1;
	m_iLastFixedSkillPoints = -1;
	m_iLastHardcoreFF = -1;
	m_iLastOnslaught = -1;
}

CNB_Mission_Panel::~CNB_Mission_Panel()
{

}

void CNB_Mission_Panel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_mission_panel.res" );

	if ( ASWGameRules()->GetGameState() == ASW_GS_INGAME )
	{
		m_pObjectiveMap->m_pMapImage->SetAlpha( 90 );
		m_pHeaderFooter->SetBackgroundStyle( NB_BACKGROUND_TRANSPARENT_BLUE );
		m_pHeaderFooter->SetMovieEnabled( false );
// 		m_pObjectiveDetails->SetVisible( false );
// 		m_pObjectiveMap->SetWide( m_pObjectiveMap->GetWide() * 1.5f );
// 		m_pObjectiveMap->SetTall( m_pObjectiveMap->GetTall() * 1.5f );
		//m_drpDifficulty->SetVisible( false );
		m_drpFixedSkillPoints->SetVisible( false );
	}
}

void CNB_Mission_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	m_pObjectiveList->InvalidateLayout( true );
	m_pObjectiveDetails->InvalidateLayout( true );
	m_pObjectiveMap->InvalidateLayout( true );

	int border = YRES( 2 );
	int title_size = YRES( 30 );
	int x, y, w, t;
	m_pObjectiveList->GetBounds( x, y, w, t );
	m_pObjectiveListBoxIsland->SetBounds( x - border, y - border - title_size, w + border * 2, t + border * 2 + title_size );

	m_pObjectiveMap->GetBounds( x, y, w, t );
	m_pObjectiveMapIsland->SetBounds( x - border, y - border - title_size, w + border * 2, t + border * 2 + title_size );

	m_pObjectiveDetails->GetBounds( x, y, w, t );
	m_pObjectiveDetailsIsland->SetBounds( x - border, y - border - title_size, w + border * 2, t + border * 2 + title_size );
}

void CNB_Mission_Panel::OnThink()
{
	BaseClass::OnThink();

	if (!m_bSelectedFirstObjective && GetAlpha()>=200)
	{
		if ( m_pObjectiveList->ClickFirstTitle() )
		{
			m_bSelectedFirstObjective = true;
			m_pObjectiveDetails->InvalidateLayout( true, true );
		}
	}

	CASWHudMinimap *pMap = GET_HUDELEMENT( CASWHudMinimap );
	if ( pMap )
	{
		wchar_t wszMissionTitle[ 128 ];
		if ( pMap->m_szMissionTitle[0] == '#' )
		{
			_snwprintf( wszMissionTitle, sizeof( wszMissionTitle ), L"%s", g_pVGuiLocalize->FindSafe( pMap->m_szMissionTitle ) );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pMap->m_szMissionTitle, wszMissionTitle, sizeof( wszMissionTitle ) );
		}
		wchar_t wszTitleText[ 255 ];
		g_pVGuiLocalize->ConstructString( wszTitleText, sizeof( wszTitleText ), g_pVGuiLocalize->Find( "#nb_mission_details" ), 1, wszMissionTitle );

		m_pHeaderFooter->SetTitle( wszTitleText );
	}
	if (ASWGameRules() && ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetCampaignSave())
	{
		int iRetries = ASWGameRules()->GetCampaignSave()->GetRetries();
		if (iRetries > 0)
		{
			m_pRetriesLabel->SetVisible(true);
			char buffer[24];
			Q_snprintf(buffer, sizeof(buffer), "Retries: %d", iRetries);
			m_pRetriesLabel->SetText(buffer);
		}
		else
			m_pRetriesLabel->SetVisible(false);
	}
	else
	{	
		m_pRetriesLabel->SetVisible(false);
	}

	if ( !ASWGameRules() )
		return;

	// disable mission settings flyouts if not leader or the mission has started
	int iLeaderIndex = ASWGameResource() ? ASWGameResource()->GetLeaderEntIndex() : -1;
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	bool bLeader = ( pPlayer && (pPlayer->entindex() == iLeaderIndex ) );

	m_drpDifficulty->SetEnabled( ASWGameRules()->GetGameState() == ASW_GS_BRIEFING && bLeader );
	m_drpFriendlyFire->SetEnabled( ASWGameRules()->GetGameState() == ASW_GS_BRIEFING && bLeader );
	m_drpOnslaught->SetEnabled( ASWGameRules()->GetGameState() == ASW_GS_BRIEFING && bLeader );
	m_drpFixedSkillPoints->SetEnabled( false ); //ASWGameRules()->GetGameState() == ASW_GS_BRIEFING && bLeader );

	if (m_iLastSkillLevel != ASWGameRules()->GetSkillLevel())
	{
		m_iLastSkillLevel = ASWGameRules()->GetSkillLevel();
		if (m_iLastSkillLevel == 4)
		{
			m_drpDifficulty->SetCurrentSelection("#L4D360UI_Difficulty_insane");
			//m_pDifficultyDescription->SetText( "#asw_difficulty_chooser_insaned" );
		}
		else if (m_iLastSkillLevel == 3)
		{
			m_drpDifficulty->SetCurrentSelection("#L4D360UI_Difficulty_hard");
			//m_pDifficultyDescription->SetText( "#asw_difficulty_chooser_hardd" );
		}
		else if (m_iLastSkillLevel == 1)
		{
			m_drpDifficulty->SetCurrentSelection("#L4D360UI_Difficulty_easy");
			//m_pDifficultyDescription->SetText( "#asw_difficulty_chooser_easyd" );
		}
		else if (m_iLastSkillLevel == 5)
		{
			m_drpDifficulty->SetCurrentSelection("#L4D360UI_Difficulty_imba");
			//m_pDifficultyDescription->SetText( "#asw_difficulty_chooser_imbad" );
		}
		else 
		{
			m_drpDifficulty->SetCurrentSelection("#L4D360UI_Difficulty_normal");
			//m_pDifficultyDescription->SetText( "#asw_difficulty_chooser_normald" );
		}
	}
	extern ConVar asw_sentry_friendly_fire_scale;
	extern ConVar asw_marine_ff_absorption;
	int nHardcoreFF = ( asw_sentry_friendly_fire_scale.GetFloat() != 0.0f || asw_marine_ff_absorption.GetInt() != 1 ) ? 1 : 0;
	if ( m_iLastHardcoreFF != nHardcoreFF )
	{
		m_iLastHardcoreFF = nHardcoreFF;
		if ( nHardcoreFF == 1 )
		{
			m_drpFriendlyFire->SetCurrentSelection( "#L4D360UI_HardcoreFF" );
		}
		else
		{
			m_drpFriendlyFire->SetCurrentSelection( "#L4D360UI_RegularFF" );
		}
	}
	extern ConVar asw_horde_override;
	extern ConVar asw_wanderer_override;
	int nOnslaught = ( asw_horde_override.GetBool() || asw_wanderer_override.GetBool() ) ? 1 : 0;
	if ( m_iLastOnslaught != nOnslaught )
	{
		m_iLastOnslaught = nOnslaught;
		if ( nOnslaught == 1 )
		{
			m_drpOnslaught->SetCurrentSelection( "#L4D360UI_OnslaughtEnabled" );
		}
		else
		{
			m_drpOnslaught->SetCurrentSelection( "#L4D360UI_OnslaughtDisabled" );
		}
	}

	BaseModUI::CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		m_pDifficultyDescription->SetText( footer->GetHelpText() );
	}

	// only show insane in multiplayer
	m_drpDifficulty->SetFlyoutItemEnabled( "BtnImpossible", gpGlobals->maxClients > 1 );
	m_drpDifficulty->SetFlyoutItemEnabled( "BtnImba", gpGlobals->maxClients > 1 );

	if ( ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetCampaignSave() && ASWGameRules()->GetGameState() != ASW_GS_INGAME )
	{
		int iFixedSkillPoints = ASWGameRules()->GetCampaignSave()->UsingFixedSkillPoints() ? 1 : 0;
		if ( iFixedSkillPoints != m_iLastFixedSkillPoints )
		{
			if ( iFixedSkillPoints == 1 )
			{
				m_drpFixedSkillPoints->SetCurrentSelection( "#nb_skill_points_fixed" );
				m_drpFixedSkillPoints->SetVisible( false );
			}
			else
			{
				m_drpFixedSkillPoints->SetCurrentSelection( "#nb_skill_points_custom" );
				m_drpFixedSkillPoints->SetVisible( true );
			}
			m_iLastFixedSkillPoints = iFixedSkillPoints;
		}
	}
	else
	{
		m_drpFixedSkillPoints->SetVisible( false );
	}
	
}

void CNB_Mission_Panel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "BackButton" ) )
	{
		InGameMissionPanelFrame *pParent = dynamic_cast<InGameMissionPanelFrame*>( GetParent() );
		if ( pParent )
		{
			pParent->MarkForDeletion();
		}
		else
		{
			MarkForDeletion();
		}
		return;
	}
	else if ( !Q_stricmp( command, "fixed_skill_points" ) )
	{
		engine->ClientCmd( "cl_fixedskills 1" );
		return;
	}
	else if ( !Q_stricmp( command, "custom_skill_points" ) )
	{
		engine->ClientCmd( "cl_fixedskills 0" );
		return;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_Difficulty_easy" ) )
	{
		engine->ClientCmd( "cl_skill 1" );
		return;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_Difficulty_normal" ) )
	{
		engine->ClientCmd( "cl_skill 2" );
		return;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_Difficulty_hard" ) )
	{
		engine->ClientCmd( "cl_skill 3" );
		return;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_Difficulty_insane" ) )
	{
		engine->ClientCmd( "cl_skill 4" );
		return;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_Difficulty_imba" ) )
	{
		engine->ClientCmd( "cl_skill 5" );
		return;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_RegularFF" ) )
	{
		engine->ClientCmd( "cl_hardcore_ff 0" );
		return;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_HardcoreFF" ) )
	{
		engine->ClientCmd( "cl_hardcore_ff 1" );
		return;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_OnslaughtDisabled" ) )
	{
		engine->ClientCmd( "cl_onslaught 0" );
		return;
	}
	else if ( !Q_stricmp( command, "#L4D360UI_OnslaughtEnabled" ) )
	{
		engine->ClientCmd( "cl_onslaught 1" );
		return;
	}
	BaseClass::OnCommand( command );
}


// ================================== Frame container ==================================================

InGameMissionPanelFrame::InGameMissionPanelFrame(Panel *parent, const char *panelName, bool showTaskbarIcon) :
vgui::Frame(parent, panelName, showTaskbarIcon)
{
	SetMoveable(false);
	SetSizeable(false);
	SetMenuButtonVisible(false);
	SetMaximizeButtonVisible(false);
	SetMinimizeToSysTrayButtonVisible(false);
	SetCloseButtonVisible(true);
	SetTitleBarVisible(false);
	m_iAnimated = 0;

	ASWInput()->SetCameraFixed( true );
}

InGameMissionPanelFrame::~InGameMissionPanelFrame()
{
	ASWInput()->SetCameraFixed( false );
}

void InGameMissionPanelFrame::PerformLayout()
{
	BaseClass::PerformLayout();

	int border = 0;
	int x = border;
	int y = border;
	int w = ScreenWidth() - border * 2;
	int h = ScreenHeight() - border * 2;


// 	if ( m_iAnimated < 1 )
// 	{
// 	Msg("Starting animation m_iAnimated=%d\n", m_iAnimated);
// 	int cx, cy, cw, ct;
// 	vgui::Panel *pContainer = GetClientMode()->GetViewport()->FindChildByName("MapBackdrop", true);
// 	pContainer->GetBounds( cx, cy, cw, ct);
// 	SetBounds( cx, cy, cw, ct );
// 
// 	float fDuration = 0.5f;
// 	GetAnimationController()->RunAnimationCommand(this, "xpos", x, 0, fDuration, AnimationController::INTERPOLATOR_LINEAR);
// 	GetAnimationController()->RunAnimationCommand(this, "ypos", y, 0, fDuration, AnimationController::INTERPOLATOR_LINEAR);
// 	GetAnimationController()->RunAnimationCommand(this, "wide", w, 0, fDuration, AnimationController::INTERPOLATOR_LINEAR);
// 	GetAnimationController()->RunAnimationCommand(this, "tall", h, 0, fDuration, AnimationController::INTERPOLATOR_LINEAR);
// 	m_iAnimated++;
// 	}

	SetBounds( x, y, w, h );
}

void InGameMissionPanelFrame::ApplySchemeSettings(vgui::IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled(false);
	SetBgColor(Color(0,0,0,64));
}