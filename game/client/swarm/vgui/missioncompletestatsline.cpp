#include "cbase.h"
#include "MissionCompleteStatsLine.h"
#include "vgui_controls/AnimationController.h"
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_game_resource.h"
#include "c_playerresource.h"
#include "vgui_controls/label.h"
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include "MedalArea.h"
#include "StatsBar.h"
#include "c_asw_debrief_stats.h"
#include "BriefingTooltip.h"
#include "asw_gamerules.h"
#include "controller_focus.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

MissionCompleteStatsLine::MissionCompleteStatsLine(Panel *parent, const char *name) : vgui::Panel(parent, name)
{	
	for (int i=0;i<ASW_NUM_STATS_BARS;i++)
	{
		m_pStats[i] = new StatsBar(this, "StatsBar");
		m_pBarIcons[i] = new vgui::ImagePanel(this, "BarIcon");
		m_pBarIcons[i]->SetShouldScaleImage(true);
	}

	// not used and the icons no longer exist
	//m_pBarIcons[0]->SetImage("swarm/Briefing/statkilled");
	//m_pBarIcons[1]->SetImage("swarm/Briefing/stataccuracy");
	//m_pBarIcons[2]->SetImage("swarm/Briefing/statfriendly");
	//m_pBarIcons[3]->SetImage("swarm/Briefing/statdamage");
	//m_pBarIcons[4]->SetImage("swarm/Briefing/statshots2");
	//m_pBarIcons[5]->SetImage("swarm/Briefing/statshots2");	// changed later when we know our marine
	//m_pBarIcons[6]->SetImage("swarm/Briefing/statskillpoints");

	// make the FF and damage taken bars be red
	//m_pStats[2]->SetColors(Color(255,255,255,255), Color(255,0,0,255), Color(64,0,0,255));
	//m_pStats[3]->SetColors(Color(255,255,255,255), Color(255,0,0,255), Color(64,0,0,255));	

	//m_pStats[2]->SetColors(Color(255,255,255,255), Color(33,71,96,255), Color(32,37,48,255));
	//m_pStats[3]->SetColors(Color(255,255,255,255), Color(33,71,96,255), Color(32,37,48,255));
	
	m_pNameLabel = new vgui::Label(this, "Name", "");
	m_pNameLabel->SetContentAlignment(vgui::Label::a_northwest);
	m_pWoundedLabel = new vgui::Label(this, "Wounded", "#asw_wounded");
	m_pWoundedLabel->SetContentAlignment(vgui::Label::a_west);
	m_pMedalArea = new MedalArea(this, "MedalArea", 18);
	m_pMedalArea->m_bRightAlign = true;
	m_pMedalArea->HideMedals();
	MedalArea::s_fLastMedalSound = 0;

	m_iMarineIndex = -1;
	m_szCurrentName[0] = '\0';
	iCurrentShots =-1;
	iCurrentDamage = -1;
	iCurrentKills = -1;
	m_bInitStats = false;
}

MissionCompleteStatsLine::~MissionCompleteStatsLine()
{

}

void MissionCompleteStatsLine::PerformLayout()
{
	int width = ScreenWidth();
	int height = ScreenHeight();

	int w = width * 0.85f;
	int h = height * 0.05f;
	//SetSize(w, h * 0.9f);
	SetSize(w, h * 2);
	int name_width = w * 0.15f;
	int name_offset = w * 0.03f;	
	int padding = w * 0.005f;
	int bar_icon_size = h * 0.75f;
	int stat_width = w * 0.15f + padding + bar_icon_size;
	
	m_pNameLabel->SetBounds(name_offset, 0, name_width - name_offset, h);
	
	for (int i=0;i<ASW_NUM_STATS_BARS;i++)
	{
		int bar_left = name_width + i * stat_width + padding + bar_icon_size;
		int bar_top = h * 0.125f;
		if (i>=4)
		{
			bar_top += h * 0.75f + padding;
			bar_left = name_width + (i-4) * stat_width + padding + bar_icon_size;
		}
		m_pStats[i]->SetBounds(bar_left, bar_top, stat_width * 0.9f - (padding + bar_icon_size), h * 0.75f);
		m_pBarIcons[i]->SetBounds(bar_left - (bar_icon_size + padding), bar_top, bar_icon_size, bar_icon_size);
	}	
	int wounded_left = name_width + 4 * stat_width + padding;
	m_pWoundedLabel->SetBounds(wounded_left, 0, name_width - name_offset, bar_icon_size);

	int iMedalW = 32.0f * ScreenWidth() / 1024.0f;
	int iMedalH = 32.0f * ScreenWidth() / 1024.0f;
	m_pMedalArea->SetPos(GetWide() - ((18 * iMedalW) + name_offset), h + (h - iMedalH) * 0.5f);	
}

void MissionCompleteStatsLine::OnThink()
{	
	UpdateLabels();
	// check tooltips
	if (!g_hBriefingTooltip.Get())
		return;

	const char *szName = "";
	const char *szDescription = "";
	for (int i=0;i<ASW_NUM_STATS_BARS;i++)
	{
		vgui::Panel *pPanel = NULL;
		if (m_pStats[i]->IsCursorOver() && m_pStats[i]->IsFullyVisible())
			pPanel = m_pStats[i];
		
		if (pPanel)
		{
			switch (i)
			{
			case 0: szName = "#asw_stats_kills"; szDescription = "#asw_stats_kills_desc"; break;
			case 1: szName = "#asw_stats_accuracy"; szDescription = "#asw_stats_accuracy_desc"; break;
			case 2: szName = "#asw_stats_ff"; szDescription = "#asw_stats_ff_desc"; break;
			case 3: szName = "#asw_stats_damage"; szDescription = "#asw_stats_damage_desc"; break;			
			case 6: szName = "#asw_stats_skillpoints"; szDescription = "#asw_stats_skillpoints_desc"; break;
			case 4: default:  szName = "#asw_stats_shots"; szDescription = "#asw_stats_shots_desc"; break;				
			}
			if (i == 5)	// class specific
			{
				szName = ""; szDescription = "";
				if (m_iMarineIndex >= 0 && m_iMarineIndex < ASW_MAX_MARINE_RESOURCES
					&& ASWGameResource())
				{
					C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(m_iMarineIndex);
					if (pMR && pMR->GetProfile())
					{
						if (pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_MEDIC)
						{
							szName= "#asw_stats_healed"; szDescription = "#asw_stats_healed_desc";
						}
						else if (pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_SPECIAL_WEAPONS)
						{
							szName= "#asw_stats_burned"; szDescription = "#asw_stats_burned_desc";
						}
						else if (pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_TECH)
						{
							szName = "#asw_stats_fasth"; szDescription = "#asw_stats_fasth";
						}
					}
				}
			}
			if (g_hBriefingTooltip->GetTooltipPanel() != pPanel)
			{	
				int tx, ty, w, h;
				tx = ty = 0;
				pPanel->LocalToScreen(tx, ty);
				pPanel->GetSize(w, h);
				tx += w * 0.5f;
				ty -= h * 0.01f;
				
				g_hBriefingTooltip->SetTooltip(pPanel, szName, szDescription,
					tx, ty);
			}
			return;
		}
	}
}

void MissionCompleteStatsLine::UpdateLabels()
{
	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	if (m_iMarineIndex <0 || m_iMarineIndex>=pGameResource->GetMaxMarineResources())
	{
		m_pNameLabel->SetText("");
		m_pMedalArea->SetMarineResource(NULL);
		Q_snprintf(m_szCurrentName, sizeof(m_szCurrentName), "");
		return;	
	}
	
	C_ASW_Marine_Resource* pMR = pGameResource->GetMarineResource(m_iMarineIndex);
	if (m_pMedalArea)
		m_pMedalArea->SetMarineResource(pMR);
	if (pMR)
	{
		if ( pMR->GetProfile() && Q_strcmp( pMR->GetProfile()->m_ShortName, m_szCurrentName ) )
		{
			m_pNameLabel->SetText(pMR->GetProfile()->m_ShortName);
			Q_snprintf(m_szCurrentName, sizeof(m_szCurrentName), "%s", pMR->GetProfile()->m_ShortName);
		}
		SetBgColor(m_pBGColor);
	}
	else
	{
		m_pNameLabel->SetText("");
	}

	// todo: reset stats bars
}

void MissionCompleteStatsLine::InitFrom(C_ASW_Debrief_Stats *pDebriefStats)
{
	if (!pDebriefStats)
		return;

	if (m_iMarineIndex < 0 || m_iMarineIndex >= ASW_MAX_MARINE_RESOURCES)
		return;

	if (!ASWGameRules() || !ASWGameResource())
		return;

	C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(m_iMarineIndex);
	if (!pMR)
		return;

	m_bInitStats = true;

	// find marine with highest kills, highest acc, lowest FF and lowest damage taken
	int iHighestKills = pDebriefStats->GetHighestKills();
	float fHighestAccuracy = pDebriefStats->GetHighestAccuracy();
	int iHighestFF = pDebriefStats->GetHighestFriendlyFire();
	int iHighestDamage = pDebriefStats->GetHighestDamageTaken();
	int iHighestShotsFired = pDebriefStats->GetHighestShotsFired();

	float fDelay = 1.0f;	// roughly how many seconds we want it to take for the bars to fill
	float fKillRate = float(iHighestKills) / fDelay;
	float fAccRate = fHighestAccuracy / fDelay;
	float fFFRate = float(iHighestFF) / fDelay;
	float fDamageRate = float(iHighestDamage) / fDelay;
	float fShotsFiredRate = float(iHighestShotsFired) / fDelay;

	// kills
	m_pStats[0]->Init(0, pDebriefStats->GetKills(m_iMarineIndex), fKillRate, true, false);
	m_pStats[0]->AddMinMax( 0, iHighestKills );
	m_pStats[1]->Init(0, pDebriefStats->GetAccuracy(m_iMarineIndex), fAccRate, true, true);
	m_pStats[1]->AddMinMax( 0, fHighestAccuracy );
	m_pStats[2]->Init(0, pDebriefStats->GetFriendlyFire(m_iMarineIndex), fFFRate, true, false);
	m_pStats[2]->AddMinMax( 0, iHighestFF );
	m_pStats[3]->Init(0, pDebriefStats->GetDamageTaken(m_iMarineIndex), fDamageRate, true, false);
	m_pStats[3]->AddMinMax( 0, iHighestDamage );
	m_pStats[4]->Init(0, pDebriefStats->GetShotsFired(m_iMarineIndex), fShotsFiredRate, true, false);
	m_pStats[4]->AddMinMax( 0, iHighestShotsFired );

	// wounded
	m_pWoundedLabel->SetFgColor(Color(255,0,0,255));
	
	if (pMR->GetHealthPercent() <= 0)
	{
		m_pWoundedLabel->SetText("#asw_kia");
		m_pWoundedLabel->SetVisible(true);
	}
	else if (pDebriefStats->IsWounded(m_iMarineIndex))
	{
		m_pWoundedLabel->SetText("#asw_wounded");
		m_pWoundedLabel->SetVisible(true);
	}
	else
	{
		m_pWoundedLabel->SetVisible(false);
	}
	
	// update our class specific bar
	if (pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_SPECIAL_WEAPONS)
	{
		m_pStats[5]->SetVisible(false);
		m_pBarIcons[5]->SetVisible(false);
	}
	else
	{
		m_pStats[5]->SetVisible(true);
		m_pBarIcons[5]->SetVisible(true);

		if (pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_NCO)
		{
			int iHighest = pDebriefStats->GetHighestAliensBurned();
			float fRate = float(iHighest) / fDelay;
			m_pStats[5]->Init(0, pDebriefStats->GetAliensBurned(m_iMarineIndex), fRate, true, false);
			m_pStats[5]->AddMinMax( 0, iHighest );
			m_pBarIcons[5]->SetImage("swarm/Briefing/statburned");
		}
		else if (pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_MEDIC)
		{
			int iHighest = pDebriefStats->GetHighestHealthHealed();
			float fRate = float(iHighest) / fDelay;
			Msg( "Medic healed %d highest %d\n", pDebriefStats->GetHealthHealed(m_iMarineIndex), iHighest );
			m_pStats[5]->Init(0, pDebriefStats->GetHealthHealed(m_iMarineIndex), fRate, true, false);
			m_pStats[5]->AddMinMax( 0, iHighest );
			m_pBarIcons[5]->SetImage("swarm/Briefing/statheal");
		}
		else if (pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_TECH)
		{
			int iHighest = pDebriefStats->GetHighestFastHacks();
			float fRate = float(iHighest) / fDelay;
			m_pStats[5]->Init(0, pDebriefStats->GetFastHacks(m_iMarineIndex), fRate, true, false);
			m_pStats[5]->AddMinMax( 0, iHighest );
			m_pBarIcons[5]->SetImage("swarm/Briefing/stathack");
		}
	}

	if (ASWGameRules() && ASWGameRules()->GetMissionSuccess() && ASWGameRules()->IsCampaignGame())
	{
		m_pStats[6]->SetVisible(true);
		m_pBarIcons[6]->SetVisible(true);

		int iHighestSkillPointsAwarded = pDebriefStats->GetHighestSkillPointsAwarded();
		float fDelay = 1.0f;	// roughly how many seconds we want it to take for the bars to fill
		float fSkillPointsRate = float(iHighestSkillPointsAwarded) / fDelay;
		m_pStats[6]->Init(0, pDebriefStats->GetSkillPointsAwarded(m_iMarineIndex), fSkillPointsRate, true, false);
		m_pStats[6]->AddMinMax( 0, iHighestSkillPointsAwarded );
	}
	else
	{
		m_pStats[6]->SetVisible(false);
		m_pBarIcons[6]->SetVisible(false);
	}
}

void MissionCompleteStatsLine::ShowStats()
{
	m_pStats[0]->SetStartCountingTime(gpGlobals->curtime);
	m_pStats[1]->SetStartCountingTime(gpGlobals->curtime + 1);
	m_pStats[2]->SetStartCountingTime(gpGlobals->curtime + 2);
	m_pStats[3]->SetStartCountingTime(gpGlobals->curtime + 3);
	m_pStats[4]->SetStartCountingTime(gpGlobals->curtime);

	if (ASWGameRules() && ASWGameRules()->GetMissionSuccess())
		m_pStats[6]->SetStartCountingTime(gpGlobals->curtime + 3);
	
	if (m_iMarineIndex >= 0 && m_iMarineIndex < ASW_MAX_MARINE_RESOURCES
		&& ASWGameRules() && ASWGameResource())
	{
		C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(m_iMarineIndex);
		if (pMR && pMR->GetProfile() && pMR->GetProfile()->GetMarineClass() != MARINE_CLASS_SPECIAL_WEAPONS)
			m_pStats[5]->SetStartCountingTime(gpGlobals->curtime + 1);
	}

	int x, y;
	GetPos(x, y);
	float fProp = y / ScreenHeight();
	m_pMedalArea->StartShowingMedals(gpGlobals->curtime + fProp * 0.9f + 5.0f);
}

void MissionCompleteStatsLine::SetMarineIndex(int i)
{
	if (m_iMarineIndex != i)
	{
		m_iMarineIndex = i;

		UpdateLabels();

		// check if this line already started the stats showing
		if (m_bInitStats)
		{
			// it did, so we need to apply the new marine's stats
			unsigned int c = ClientEntityList().GetHighestEntityIndex();
			for ( unsigned int i = 0; i <= c; i++ )
			{
				C_BaseEntity *e = ClientEntityList().GetBaseEntity( i );
				if ( !e )
					continue;
				
				C_ASW_Debrief_Stats *pStats = dynamic_cast<C_ASW_Debrief_Stats*>(e);
				if (pStats)
				{
					InitFrom(pStats);
					for (int b=0;b<ASW_NUM_STATS_BARS;b++)
					{
						if (m_pStats[b])
						{
							m_pStats[b]->SetStartCountingTime(gpGlobals->curtime - 10);
						}
					}
					return;
				}
			}
		}
	}
}

void MissionCompleteStatsLine::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );	
	m_pBGColor = pScheme->GetColor("DarkBlueTrans", Color(0,0,0,64));	
	
	if (!m_bSetAlpha)
	{
		SetAlpha(0);
		m_bSetAlpha = true;
	}

	if (m_pNameLabel)
		m_pNameLabel->SetFgColor(pScheme->GetColor("LightBlue", Color(128,128,255,255)));
	if (m_pMedalArea)
		m_pMedalArea->SetFgColor(Color(255,255,255,255));
}


// ================== Player stats line =======================

MissionCompletePlayerStatsLine::MissionCompletePlayerStatsLine(Panel *parent, const char *name) : vgui::Panel(parent, name)
{	
	m_pNameLabel = new vgui::Label(this, "StatsNameLabel", "");
	m_pNameLabel->SetContentAlignment(vgui::Label::a_west);	
	m_pMedalArea = new MedalAreaPlayer(this, "PlayerMedalArea", 18);
	m_pMedalArea->m_bRightAlign = true;

	m_iPlayerIndex = -1;
	m_wszCurrentName[ 0 ] = L'\0';
}

void MissionCompletePlayerStatsLine::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetAlpha(0);
	m_pBGColor = pScheme->GetColor("DarkBlueTrans", Color(0,0,0,64));	
	
	if (m_pNameLabel)
		m_pNameLabel->SetFgColor(Color(255,255,255,255));	
	if (m_pMedalArea)
		m_pMedalArea->SetFgColor(Color(255,255,255,255));
}

void MissionCompletePlayerStatsLine::PerformLayout()
{
	int w = ScreenWidth() * 0.85f;
	int h = ScreenHeight() * 0.05f;
	//SetSize(w, h * 0.9f);
	SetSize(w, h);
	int name_width = w * 0.3f;
	int name_offset = w * 0.03f;
	//int stat_width = w * 0.2333333f;
	m_pNameLabel->SetBounds(name_offset, 0, name_width - name_offset, h);	
	int iMedalW = 32.0f * ScreenWidth() / 1024.0f;
	int iMedalH = 32.0f * ScreenWidth() / 1024.0f;
	m_pMedalArea->SetPos(GetWide() - ((18 * iMedalW) + name_offset), (h - iMedalH) * 0.5f);	
}

void MissionCompletePlayerStatsLine::OnThink()
{	
	UpdateLabels();
}

void MissionCompletePlayerStatsLine::UpdateLabels()
{
	C_ASW_Game_Resource* pGameResource = ASWGameResource();
	if (!pGameResource)
		return;

	if ( m_iPlayerIndex < 1 || m_iPlayerIndex>gpGlobals->maxClients )
	{
		m_pNameLabel->SetText("");
		if ( m_pMedalArea )
		{
			m_pMedalArea->SetProfileIndex( -1 );
		}

		m_wszCurrentName[ 0 ] = L'\0';
		return;
	}

	C_ASW_Player *pPlayer = dynamic_cast<C_ASW_Player*>( UTIL_PlayerByIndex( m_iPlayerIndex ) );
	if ( !pPlayer )
		return;

	wchar_t wszPlayerName[ 64 ];
	g_pVGuiLocalize->ConvertANSIToUnicode( g_PR->GetPlayerName( m_iPlayerIndex ), wszPlayerName, sizeof( wszPlayerName ) );

	wchar_t wszPlayerStatus[ 64 ];
	wszPlayerStatus[ 0 ] = L'\0';
	if ( m_iPlayerIndex == pGameResource->GetLeaderEntIndex() )
	{				
		V_wcsncpy( wszPlayerStatus, g_pVGuiLocalize->Find( "#asw_stats_player_status_leader" ), sizeof( wszPlayerStatus ) );
	}
	else if ( pGameResource->IsPlayerReady(m_iPlayerIndex) )
	{
		V_wcsncpy( wszPlayerStatus, g_pVGuiLocalize->Find( "#asw_stats_player_status_ready" ), sizeof( wszPlayerStatus ) );
	}

	char szPlayerXP[ 64 ];
	V_snprintf( szPlayerXP, sizeof( szPlayerXP ), "%d", pPlayer->GetExperience() );
	wchar_t wszPlayerXP[ 64 ];
	g_pVGuiLocalize->ConvertANSIToUnicode( szPlayerXP, wszPlayerXP, sizeof( wszPlayerXP ) );

	g_pVGuiLocalize->ConstructString( m_wszCurrentName, sizeof( m_wszCurrentName ), g_pVGuiLocalize->Find( "#asw_stats_player_name" ), 3, wszPlayerName, wszPlayerStatus, wszPlayerXP );
	m_pNameLabel->SetText( m_wszCurrentName );

	if ( m_pMedalArea )
	{
		m_pMedalArea->SetProfileIndex( m_iPlayerIndex - 1 );
	}

	SetBgColor( m_pBGColor );		
}

void MissionCompletePlayerStatsLine::SetPlayerIndex(int i)
{
	m_iPlayerIndex = i;
	UpdateLabels();
}