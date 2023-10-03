#include "cbase.h"
#include "MissionStatsPanel.h"
#include "MissionCompleteStatsLine.h"
#include "vgui_controls/AnimationController.h"
#include "RestartMissionButton.h"
#include "c_asw_player.h"
#include "asw_gamerules.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "vgui/isurface.h"
#include "BriefingTooltip.h"
#include "c_playerresource.h"
#include "c_asw_debrief_stats.h"
#include "StatsBar.h"
#include "controller_focus.h"
#include "asw_hud_minimap.h"
#include <vgui/ILocalize.h>
#include "vgui_controls\PanelListPanel.h"
#include "vgui_controls\ScrollBar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

ConVar asw_stats_skip_marines_test("asw_stats_skip_marines_test", "0", FCVAR_CHEAT, "Tests removing marines 1 and 3 from the stats screen");

MissionStatsPanel::MissionStatsPanel(vgui::Panel *parent, const char *name) : vgui::Panel(parent, name)
{
	m_pTitle = new vgui::Label(this, "MissionCompleteTitle", "#asw_mission_complete");
	m_pMissionLabel = new vgui::Label(this, "MissionLabel", "");
	m_pDifficultyLabel = new vgui::Label(this, "DifficultyLabel", "");	
	m_pTitle->SetAlpha(0);
	m_pMissionLabel->SetAlpha(0);
	m_pDifficultyLabel->SetAlpha(0);
	m_bSetAlpha = false;

	for (int i=0;i<MAX_STAT_LINES;i++)
	{
		m_pStatLines[i] = NULL;
	}
	for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
	{
		m_pPlayerLine[i] = NULL;
	}
	m_pStatLineContainer = new vgui::Panel(this, "StatLineContainer");

	m_pStatLineList = new vgui::PanelListPanel( this, "statlinelist" );
	m_pStatLineList->AllowMouseWheel(true);
	m_pStatLineList->SetMouseInputEnabled(true);
	m_pStatLineList->SetShowScrollBar(true);	
	m_pStatLineList->SetFirstColumnWidth(0);
	m_pStatLineList->AddItem(NULL, m_pStatLineContainer);
	m_pStatLineList->SetVerticalBufferPixels( 0 );
	m_pStatLineList->SetPaintBackgroundEnabled(false);

	m_pTimeTakenBar = new StatsBar(this, "TimeTakenBar");
	m_pTimeTakenBar->m_bDisplayTime = true;
	m_pTimeTakenIcon = new vgui::ImagePanel(this, "TimeTakenIcon");
	m_pTimeTakenIcon->SetShouldScaleImage(true);
		
	m_pTotalKillsBar = new StatsBar(this, "TotalKillsBar");
	m_pTotalKillsIcon = new vgui::ImagePanel(this, "TotalKillsIcon");
	m_pTotalKillsIcon->SetShouldScaleImage(true);

	m_pTotalKillsIcon->SetImage("swarm/Briefing/statkilled");
	m_pTimeTakenIcon->SetImage("swarm/Briefing/stattime");

	m_pUnlockedLabel = new vgui::Label(this, "UnlockedLabel", " ");
	m_pUnlockedLabel->SetContentAlignment(vgui::Label::a_north);
	m_pUnlockedLabel->SetAlpha(0);

	m_pBestKillsLabel = new vgui::Label(this, "UnlockedLabel", " ");
	m_pBestKillsLabel->SetContentAlignment(vgui::Label::a_northeast);
	m_pBestKillsLabel->SetAlpha(0);

	m_pBestTimeLabel = new vgui::Label(this, "UnlockedLabel", " ");
	m_pBestTimeLabel->SetContentAlignment(vgui::Label::a_northeast);
	m_pBestTimeLabel->SetAlpha(0);
}

void MissionStatsPanel::SetMissionLabels(vgui::Label *pMissionLabel, vgui::Label *pDifficultyLabel)
{
	if (ASWGameRules() && ASWGameRules()->IsTutorialMap())
	{
		pMissionLabel->SetText("#ASWMainMenu_Tutorial");
	}
	else
	{
		CASWHudMinimap *pMap = GET_HUDELEMENT( CASWHudMinimap );
		if ( pMap )
		{
			pMissionLabel->SetText(pMap->m_szMissionTitle);
		}
	}

	// find the mission difficulty
	int iDiff = 1;
	const char* pszToken = "#asw_difficulty_normal";
	bool bCampaign = false;
	bool bCarnage = false;
	bool bUber = false;
	bool bCheated = false;
	bool bHardcore = false;
	if (ASWGameRules())
	{
		iDiff = ASWGameRules()->GetSkillLevel();
		bCampaign = ASWGameRules()->IsCampaignGame() == 1;
		bCarnage = ASWGameRules()->IsCarnageMode();
		bUber = ASWGameRules()->IsUberMode();
		bHardcore = ASWGameRules()->IsHardcoreMode();
		bCheated = ASWGameRules()->m_bCheated;
	}
	if (iDiff <= 1)
		pszToken = "#asw_difficulty_easy";
	else if (iDiff == 3)
		pszToken = "#asw_difficulty_hard";
	else if (iDiff == 4)
		pszToken = "#asw_difficulty_insane";
	else if (iDiff >= 5)
		pszToken = "#asw_difficulty_imba";
	const wchar_t *pDiff = g_pVGuiLocalize->Find( pszToken );

	bool bOnslaught = CAlienSwarm::IsOnslaught();

	const wchar_t *pOnslaught = L"";
	if ( bOnslaught )
	{
		pOnslaught = g_pVGuiLocalize->Find( "#nb_onslaught_title" );
	}

	bool bHardcoreFriendlyFire = CAlienSwarm::IsHardcoreFF();
	const wchar_t *pHardcoreFF = L"";
	if ( bHardcoreFriendlyFire )
	{
		pHardcoreFF = g_pVGuiLocalize->Find( "#asw_hardcore_ff" );
	}

	const wchar_t *pCheated = L"";
	if (bCheated)	
	{
		pCheated = g_pVGuiLocalize->Find( "#asw_cheated" );
	}	

	wchar_t mission_difficulty[96];
	g_pVGuiLocalize->ConstructString( mission_difficulty, sizeof(mission_difficulty),
		g_pVGuiLocalize->Find("#asw_mission_difficulty"), 4,
			pDiff, pOnslaught, pHardcoreFF, pCheated);
	pDifficultyLabel->SetText(mission_difficulty);
}

void MissionStatsPanel::ShowStats(bool bSuccess)
{
	unsigned int c = ClientEntityList().GetHighestEntityIndex();
	C_ASW_Debrief_Stats *pTemp = NULL;
	for ( unsigned int i = 0; i <= c; i++ )
	{
		C_BaseEntity *e = ClientEntityList().GetBaseEntity( i );
		if ( !e )
			continue;
		
		pTemp = dynamic_cast<C_ASW_Debrief_Stats*>(e);
		if (pTemp && pTemp->m_bCreated)
		{
			InitFrom(pTemp);
			break;
		}
	}
	
	if (!bSuccess)
		m_pTitle->SetText("#asw_mission_failed");
	m_pTitle->SetFgColor(Color(255,255,255,255));	
	m_pTitle->SetContentAlignment(vgui::Label::a_north);
	m_pTitle->SetBounds(0, ScreenHeight() * 0.025f, ScreenWidth(), ScreenHeight()*0.1f);
	m_pTitle->SetFont(m_LargeFont);
	m_pTitle->SetAlpha(0);
		
	SetMissionLabels(m_pMissionLabel, m_pDifficultyLabel);

	m_pMissionLabel->SetFgColor(Color(255,255,255,255));
	m_pMissionLabel->SetContentAlignment(vgui::Label::a_north);
	m_pMissionLabel->SetBounds(0, ScreenHeight() * 0.075f, ScreenWidth() * 0.5f, ScreenHeight()*0.1f);
	m_pMissionLabel->SetFont(m_DefaultFont);
	m_pMissionLabel->SetAlpha(0);	

	
	m_pDifficultyLabel->SetFgColor(Color(255,255,255,255));	
	m_pDifficultyLabel->SetContentAlignment(vgui::Label::a_north);
	m_pDifficultyLabel->SetBounds(ScreenWidth() * 0.5f, ScreenHeight() * 0.075f, ScreenWidth() * 0.5f, ScreenHeight()*0.1f);
	m_pDifficultyLabel->SetFont(m_DefaultFont);
	m_pDifficultyLabel->SetAlpha(0);
	
	float fDuration = 1.0f;
	vgui::GetAnimationController()->RunAnimationCommand(m_pTitle, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pMissionLabel, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pDifficultyLabel, "alpha", 255.0f, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	
	// fade in the stat lines				
	for (int i=0;i<MAX_STAT_LINES;i++)
	{
		if (m_pStatLines[i])
		{
			vgui::GetAnimationController()->RunAnimationCommand(m_pStatLines[i], "alpha", 255, 0.25f * i, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			m_pStatLines[i]->ShowStats();
		}
	}
	for (int i=0;i<ASW_MAX_READY_PLAYERS;i++)
	{
		if (m_pPlayerLine[i] && m_pPlayerLine[i]->m_iPlayerIndex > 0)
		{
			m_pPlayerLine[i]->SetAlpha(1);
			vgui::GetAnimationController()->RunAnimationCommand(m_pPlayerLine[i], "alpha", 255, 0.25f * i, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
		}
	}
	
	m_pTimeTakenBar->SetStartCountingTime(gpGlobals->curtime);
	m_pTotalKillsBar->SetStartCountingTime(gpGlobals->curtime + 2);

	m_pTimeTakenBar->SetAlpha(0);
	m_pTotalKillsBar->SetAlpha(0);
	m_pTotalKillsIcon->SetAlpha(0);
	m_pTimeTakenIcon->SetAlpha(0);

	vgui::GetAnimationController()->RunAnimationCommand(m_pTimeTakenBar, "alpha", 255, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pTotalKillsBar, "alpha", 255, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pTotalKillsIcon, "alpha", 255, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
	vgui::GetAnimationController()->RunAnimationCommand(m_pTimeTakenIcon, "alpha", 255, 0, fDuration, vgui::AnimationController::INTERPOLATOR_LINEAR);
}

void MissionStatsPanel::OnThink()
{
	// why do I have to do this every frame to get the colour to 'stick'?
	//if (m_pTitle)
		//m_pTitle->SetFgColor(Color(255,255,255,255));

	UpdateStatsLines();

	if (!g_hBriefingTooltip.Get())
		return;

	const char *szName = "";
	const char *szDescription = "";
	
	vgui::Panel *pPanel = NULL;
	if (m_pTimeTakenBar->IsCursorOver())
	{
		pPanel = m_pTimeTakenBar;
		szName = "#asw_stats_time"; szDescription = "#asw_stats_time_desc";
	}
	if (m_pTotalKillsBar->IsCursorOver())
	{
		pPanel  = m_pTotalKillsBar;
		szName = "#asw_stats_tkills"; szDescription = "#asw_stats_tkills_desc";
	}
	
	if (pPanel && pPanel->IsFullyVisible())
	{
		if (g_hBriefingTooltip->GetTooltipPanel() != pPanel)
		{	
			int tx, ty, w, h;
			tx = ty = 0;
			pPanel->LocalToScreen(tx, ty);
			pPanel->GetSize(w, h);
			tx += w * 0.5f;
			ty += h * 2.5f;
			
			g_hBriefingTooltip->SetTooltip(pPanel, szName, szDescription,
				tx, ty);
		}
		return;
	}
}

// creates marine/player stat lines and keeps their indices up to date
void MissionStatsPanel::UpdateStatsLines()
{	
	if ( !ASWGameResource() )
		return;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();

	// go through each player
	int iNumPlayers = 0;
	int iNumMarines = 1;
	
	// make first stat line be the headers
	if (m_pStatLines[iNumMarines] == NULL)
	{
		m_pStatLines[iNumMarines] = new MissionCompleteStatsLine(m_pStatLineContainer, "StatsLine");
		m_pStatLines[iNumMarines]->SetMarineIndex(-1);
	}

	for ( int j = 1; j <= gpGlobals->maxClients; j++ )
	{	
		if ( g_PR->IsConnected( j ) && iNumPlayers < ASW_MAX_READY_PLAYERS)
		{
			if (m_pPlayerLine[iNumPlayers] == NULL)
			{
				m_pPlayerLine[iNumPlayers] = new MissionCompletePlayerStatsLine(m_pStatLineContainer, "PlayerLine");								
			}
			m_pPlayerLine[iNumPlayers]->SetPlayerIndex(j);
			if (m_pPlayerLine[iNumPlayers]->GetAlpha() <= 0)
			{
				m_pPlayerLine[iNumPlayers]->SetAlpha(1);
				vgui::GetAnimationController()->RunAnimationCommand(m_pPlayerLine[iNumPlayers], "alpha", 255, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
			}

			// go through all the marines that this player has
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				if (asw_stats_skip_marines_test.GetBool() && (i==1 || i==3))
					continue;
				C_ASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
				if (pMR && pMR->GetCommanderIndex() == j && iNumMarines < MAX_STAT_LINES)
				{
					if (m_pStatLines[iNumMarines] == NULL)
					{
						m_pStatLines[iNumMarines] = new MissionCompleteStatsLine(m_pStatLineContainer, "StatsLine");
					}
					m_pStatLines[iNumMarines]->SetMarineIndex(i);
					if (m_pStatLines[iNumMarines]->GetAlpha() <= 0)
					{
						m_pStatLines[iNumMarines]->SetAlpha(1);
						vgui::GetAnimationController()->RunAnimationCommand(m_pStatLines[iNumMarines], "alpha", 255, 0, 0.5f, vgui::AnimationController::INTERPOLATOR_LINEAR);
					}
					m_pStatLines[iNumMarines]->InvalidateLayout(true);
					iNumMarines++;
				}
			}
			iNumPlayers++;
		}
	}
	// hide all lines we're not using
	for (int j=iNumPlayers; j<ASW_MAX_READY_PLAYERS; j++)
	{
		if (m_pPlayerLine[j])
		{
			m_pPlayerLine[j]->SetPlayerIndex(0);
			m_pPlayerLine[j]->SetAlpha(0);
		}
	}
	for (int j=iNumMarines; j<MAX_STAT_LINES;j++)
	{
		if (m_pStatLines[j])
		{
			m_pStatLines[j]->SetAlpha(0);
		}
	}
	InvalidateLayout(true);
}

void MissionStatsPanel::PerformLayout()
{
	if ( !ASWGameResource() )
		return;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();

	int width = ScreenWidth();
	int height = ScreenHeight();
	int list_ypos = 0.215f * height;		
	int player_line_height = height * 0.05f;
	int marine_line_height = height * 0.05f * 2;
	int gap = (height / 768.0f) * 4;
	int iNumPlayers = 0;
	int iNumMarines = 1;
	int line_wide = width * 0.85f;
	int line_x = (width - line_wide) * 0.5f;
	
	int list_high = ScreenHeight() * 0.55f;	
	int ypos = 0;

	for ( int j = 1; j <= gpGlobals->maxClients; j++ )
	{	
		if ( g_PR->IsConnected( j ) && iNumPlayers < ASW_MAX_READY_PLAYERS)
		{
			if (m_pPlayerLine[iNumPlayers] != NULL)
			{
				m_pPlayerLine[iNumPlayers]->SetPos(0, ypos);
				//m_pPlayerLine[iNumPlayers]->SetPlayerIndex(j);
				ypos += player_line_height + gap;
			}			

			// go through all the marines that this player has
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				C_ASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
				if (pMR && pMR->GetCommanderIndex() == j && iNumMarines < MAX_STAT_LINES)
				{
					if (m_pStatLines[iNumMarines] != NULL)
					{
						m_pStatLines[iNumMarines]->SetPos(0, ypos);
						//m_pStatLines[iNumMarines]->SetMarineIndex(i);
						ypos += marine_line_height + gap;
					}					
					iNumMarines++;
				}
			}
			iNumPlayers++;
		}
	}

	m_pStatLineContainer->SetSize(line_wide, ypos);
	m_pStatLineList->SetShowScrollBar(ypos > list_high);
	int list_x = line_x;
	if (ypos > list_high)
	{
		list_x -= m_pStatLineList->GetScrollBar()->GetWide();
	}
	m_pStatLineList->SetBounds(list_x, list_ypos, line_wide + m_pStatLineList->GetScrollBar()->GetWide() * 2, list_high);	

	int bar_high = height * 0.05f;	
	int icon_size = height * 0.0375f;
	int padding = width * 0.00425f;
	int bar_wide = (line_wide * 0.5f) - (icon_size * 2 + padding * 2);
	int bars_top = 0.115f * height;
	line_x += icon_size * 0.5f;
	m_pTimeTakenBar->SetBounds(line_x + (icon_size + padding), bars_top, bar_wide, bar_high);
	m_pTotalKillsBar->SetBounds(line_x + (icon_size * 3 + padding * 3) + bar_wide, bars_top, bar_wide, bar_high);

	float diff = bar_high - icon_size;
	m_pTimeTakenIcon->SetBounds(line_x, bars_top + diff * 0.5f, icon_size, icon_size);
	m_pTotalKillsIcon->SetBounds(line_x + bar_wide + (icon_size * 2 + padding * 2), bars_top + diff * 0.5f, icon_size, icon_size);

	m_pBestTimeLabel->SetBounds(line_x + (icon_size + padding), bars_top + bar_high + padding, bar_wide, bar_high);
	m_pBestKillsLabel->SetBounds(line_x + (icon_size * 3 + padding * 3) + bar_wide, bars_top + bar_high + padding, bar_wide, bar_high);	

	m_pTitle->SetBounds(0, ScreenHeight() * 0.025f, ScreenWidth(), ScreenHeight()*0.1f);
	m_pTitle->InvalidateLayout(true);
	m_pMissionLabel->SetBounds(0, ScreenHeight() * 0.075f, ScreenWidth() * 0.5f, ScreenHeight()*0.1f);
	m_pDifficultyLabel->SetBounds(ScreenWidth() * 0.5f, ScreenHeight() * 0.075f, ScreenWidth() * 0.5f, ScreenHeight()*0.1f);

	m_pUnlockedLabel->SetBounds(line_x, list_ypos + list_high + padding * 2, ScreenWidth() - line_x * 2, ScreenHeight() * 0.3f);
}

void MissionStatsPanel::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );	
	m_LargeFont = scheme->GetFont( "DefaultLarge", IsProportional() );
	m_DefaultFont = scheme->GetFont( "Default", IsProportional() );
	vgui::HFont SmallFont = scheme->GetFont( "DefaultSmall", IsProportional() );

	if (!m_bSetAlpha)
	{
		m_pTimeTakenBar->SetAlpha(0);
		m_pTimeTakenIcon->SetAlpha(0);	
		m_pTotalKillsBar->SetAlpha(0);
		m_pTotalKillsIcon->SetAlpha(0);
		m_bSetAlpha = true;
	}

	if (m_pTitle)
		m_pTitle->SetFgColor(Color(255,255,255,255));
	if (m_pMissionLabel)
		m_pMissionLabel->SetFgColor(scheme->GetColor("LightBlue", Color(128,128,255,255)));
	if (m_pDifficultyLabel)
		m_pDifficultyLabel->SetFgColor(scheme->GetColor("LightBlue", Color(128,128,255,255)));	
	if (m_pUnlockedLabel)
		m_pUnlockedLabel->SetFgColor(scheme->GetColor("LightBlue", Color(128,128,255,255)));		

	if (m_pBestKillsLabel)
	{
		m_pBestKillsLabel->SetFgColor(scheme->GetColor("LightBlue", Color(128,128,255,255)));
		m_pBestKillsLabel->SetFont(SmallFont);
	}
	if (m_pBestTimeLabel)
	{
		m_pBestTimeLabel->SetFgColor(scheme->GetColor("LightBlue", Color(128,128,255,255)));
		m_pBestTimeLabel->SetFont(SmallFont);
	}
}

void MissionStatsPanel::InitFrom(C_ASW_Debrief_Stats* pStats)
{
	if (!pStats || !ASWGameRules())
		return;

	m_hStats = pStats;

	for (int i=0;i<MAX_STAT_LINES;i++)
	{
		if (m_pStatLines[i])
			m_pStatLines[i]->InitFrom(pStats);
	}
	
	float fDelay = 2.0f;	// roughly how many seconds we want it to take for the bars to fill
	float fHighestTime = MAX(pStats->GetTimeTaken(), pStats->GetBestTime());
	float fHighestKills = MAX(pStats->GetTotalKills(), pStats->GetBestKills());
	float fTimeRate = fHighestTime / fDelay;
	m_pTimeTakenBar->Init(0, pStats->GetTimeTaken(), fTimeRate, true, false);
	m_pTimeTakenBar->AddMinMax( 0, fHighestTime );
	m_pTotalKillsBar->Init(0, pStats->GetTotalKills(), fTimeRate, true, false);
	m_pTotalKillsBar->AddMinMax( 0, fHighestKills );

	// if we unlocked Uber mode, we're going to display the speedrun time the player beat
	wchar_t wszUnlockMsg[128];
	bool bShowSpeedrunTime = false;
	if (pStats->m_bJustUnlockedUber)	// beat the speedrun so show the uber message
	{
		// so grab the speedrun time and format it into our message
		int iTotalSeconds = pStats->GetSpeedrunTime();
		int iMinutes = iTotalSeconds / 60;
		int iSeconds = iTotalSeconds - (iMinutes * 60);

		char timebuffer[8];
		Q_snprintf(timebuffer, sizeof(timebuffer), "%.2d:%.2d", iMinutes, iSeconds);

		wchar_t wtimebuffer[8];
		g_pVGuiLocalize->ConvertANSIToUnicode(timebuffer, wtimebuffer, sizeof( wtimebuffer ));

		g_pVGuiLocalize->ConstructString( wszUnlockMsg, sizeof(wszUnlockMsg),
			g_pVGuiLocalize->Find("#asw_unlocked_uber_format"), 1,
			wtimebuffer);

		bShowSpeedrunTime = true;
	}
	else if (pStats->m_bBeatSpeedrunTime)	// didn't unlock uber (already unlocked), but we did beat the speedrun time
	{
		// so grab the speedrun time and format it into our message
		int iTotalSeconds = pStats->GetSpeedrunTime();
		int iMinutes = iTotalSeconds / 60;
		int iSeconds = iTotalSeconds - (iMinutes * 60);

		char timebuffer[8];
		Q_snprintf(timebuffer, sizeof(timebuffer), "%.2d:%.2d", iMinutes, iSeconds);

		wchar_t wtimebuffer[8];
		g_pVGuiLocalize->ConvertANSIToUnicode(timebuffer, wtimebuffer, sizeof( wtimebuffer ));

		g_pVGuiLocalize->ConstructString( wszUnlockMsg, sizeof(wszUnlockMsg),
			g_pVGuiLocalize->Find("#asw_beat_speedrun_format"), 1,
			wtimebuffer);

		bShowSpeedrunTime = true;
	}

	// set our label to show which modes were just unlocked
	wchar_t wbuffer[255];		
	g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
		g_pVGuiLocalize->Find("#asw_unlocked_format"), 3,
		bShowSpeedrunTime ? wszUnlockMsg : L"",
		pStats->m_bJustUnlockedCarnage ? g_pVGuiLocalize->Find("#asw_unlocked_carnage") : L"",
		pStats->m_bJustUnlockedHardcore ? g_pVGuiLocalize->Find("#asw_unlocked_hardcore") : L""
		);
	m_pUnlockedLabel->SetText(wbuffer);

	if (ASWGameRules()->GetMissionSuccess())
	{
		bool bNewRecordTime = ((pStats->GetTimeTaken() < pStats->GetBestTime()) || pStats->GetBestTime() == 0);
		bool bNewRecordKills = ((pStats->GetTotalKills() > pStats->GetBestKills()) ||
									(pStats->GetBestKills() == 0 && pStats->GetTotalKills() > 0));

		// format the best time
		int iTotalSeconds = pStats->GetBestTime();
		int iMinutes = iTotalSeconds / 60;
		int iSeconds = iTotalSeconds - (iMinutes * 60);

		char timebuffer[8];
		Q_snprintf(timebuffer, sizeof(timebuffer), "%.2d:%.2d", iMinutes, iSeconds);

		wchar_t wtimebuffer[8];
			g_pVGuiLocalize->ConvertANSIToUnicode(timebuffer, wtimebuffer, sizeof( wtimebuffer ));


		if (bNewRecordTime)
		{
			g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
					g_pVGuiLocalize->Find("#asw_newbest_time_format"), 1,
					wtimebuffer);
		}
		else
		{
			g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
					g_pVGuiLocalize->Find("#asw_best_time_format"), 1,
					wtimebuffer);
		}

		m_pBestTimeLabel->SetText(wbuffer);

		// format the best kills
		Q_snprintf(timebuffer, sizeof(timebuffer), "%d", pStats->GetBestKills());
		g_pVGuiLocalize->ConvertANSIToUnicode(timebuffer, wtimebuffer, sizeof( wtimebuffer ));

		if (bNewRecordKills)
		{
			g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
					g_pVGuiLocalize->Find("#asw_newbest_kills_format"), 1,
					wtimebuffer);
		}
		else
		{
			g_pVGuiLocalize->ConstructString( wbuffer, sizeof(wbuffer),
					g_pVGuiLocalize->Find("#asw_best_kills_format"), 1,
					wtimebuffer);
		}

		m_pBestKillsLabel->SetText(wbuffer);
	}
}
