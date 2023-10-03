#include "cbase.h"
#include "experience_stat_line.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/isurface.h"
#include "StatsBar.h"
#include "asw_gamerules.h"
#include "c_asw_player.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "asw_medals_shared.h"
#include "BriefingTooltip.h"
#include "asw_achievements.h"
#include "c_asw_debrief_stats.h"
#include "asw_briefing.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

ExperienceStatLine::ExperienceStatLine( Panel *parent, const char *name, CASW_Earned_XP_t XPType ) : vgui::EditablePanel( parent, name )
{
	m_XPType = XPType;
	m_pStatNum = new vgui::Label( this, "StatNum", "" );
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pCounter = new vgui::Label( this, "Counter", "" );
	m_pIcon = new vgui::ImagePanel( this, "Icon" );
	m_pIcon->SetShouldScaleImage( true );
	m_pStatsBar = new StatsBar( this, "StatsBar" );
	m_pStatsBar->UseExternalCounter( m_pCounter );
	m_pStatsBar->SetShowMaxOnCounter( true );
	m_pStatsBar->m_flBorder = 0.0f;
	
	switch( XPType )
	{
		case ASW_XP_MISSION: m_pTitle->SetText( "#asw_xp_mission" ); break;
		case ASW_XP_KILLS: m_pTitle->SetText( "#asw_xp_kills" ); break;
		case ASW_XP_TIME:
		{
			int nParTime = GetDebriefStats()->GetSpeedrunTime() + 5.0f * 60.0f;
			int nMinutes = nParTime / 60;
			int nSeconds = nParTime - nMinutes * 60;

			wchar_t minutes[ 5 ];
			V_snwprintf( minutes, sizeof( minutes ), L"%d", nMinutes );
			wchar_t seconds[ 3 ];
			V_snwprintf( seconds, sizeof( seconds ), L"%02d", nSeconds );

			wchar_t wszTitle[ 128 ];
			g_pVGuiLocalize->ConstructString( wszTitle, sizeof( wszTitle ), g_pVGuiLocalize->Find( "#asw_xp_time" ), 2, minutes, seconds );

			m_pTitle->SetText( wszTitle );
			break;
		}
		case ASW_XP_FRIENDLY_FIRE: m_pTitle->SetText( "#asw_xp_friendly_fire" ); break;
		case ASW_XP_DAMAGE_TAKEN: m_pTitle->SetText( "#asw_xp_damage_taken" ); break;
		case ASW_XP_HEALING: m_pTitle->SetText( "#asw_xp_healing" ); break;
		case ASW_XP_HACKING: m_pTitle->SetText( "#asw_xp_hacking" ); break;
	}
}

ExperienceStatLine::~ExperienceStatLine()
{

}

void ExperienceStatLine::PerformLayout()
{
	BaseClass::PerformLayout();
}

void ExperienceStatLine::OnThink()
{	
}


void ExperienceStatLine::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/ExperienceStatLine.res" );

	// no longer used / icons don't exist anymore
	/*
	switch( m_XPType )
	{
		case ASW_XP_MISSION: m_pIcon->SetImage( "swarm/Briefing/statshots" ); break;
		case ASW_XP_KILLS: m_pIcon->SetImage( "swarm/Briefing/statkilled" ); break;
		case ASW_XP_TIME: m_pIcon->SetImage( "swarm/Briefing/stattime" ); break;
		case ASW_XP_FRIENDLY_FIRE: m_pIcon->SetImage( "swarm/Briefing/statfriendly" ); break;
		case ASW_XP_DAMAGE_TAKEN: m_pIcon->SetImage( "swarm/Briefing/statdamage" ); break;
		case ASW_XP_HEALING: m_pIcon->SetImage( "swarm/Briefing/statheal" ); break;
		case ASW_XP_HACKING: m_pIcon->SetImage( "swarm/Briefing/stathack" ); break;
	}
	*/

	UpdateVisibility( ToASW_Player( m_hPlayer.Get() ) );
}

void ExperienceStatLine::UpdateVisibility( C_ASW_Player *pPlayer )
{
	if ( !pPlayer )
	{
		pPlayer = ToASW_Player( m_hPlayer.Get() );
	}

	if ( !pPlayer || !ASWGameResource() )
		return;

	C_ASW_Marine_Resource *pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	if ( !pMR )
		return;

	bool bMedic = pMR->GetProfile() && pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_MEDIC;
	bool bTech = pMR->GetProfile() && pMR->GetProfile()->GetMarineClass() == MARINE_CLASS_TECH;

	if ( !Briefing()->IsOfflineGame() )
	{
		if ( ( m_XPType == ASW_XP_HEALING && !bMedic ) || ( m_XPType == ASW_XP_HACKING && !bTech ) )
		{
			SetVisible( false );
			return;
		}
	}

	if ( pPlayer->GetStatNumXP( ASW_XP_MISSION ) < 100 )
	{
		// Don't do these if we failed
		if ( m_XPType == ASW_XP_DAMAGE_TAKEN || m_XPType == ASW_XP_FRIENDLY_FIRE || m_XPType == ASW_XP_TIME )
		{
			SetVisible( false );
			return;
		}
	}

	SetVisible( true );
}

void ExperienceStatLine::InitFor( C_ASW_Player *pPlayer )
{
	m_hPlayer = pPlayer;
	if ( !pPlayer )
		return;

	float flRate = 100.0f / 2.0f;
	float flMissionRate = 1000.0f / 2.0f;
	switch( m_XPType )
	{
		case ASW_XP_MISSION:
		{
			wchar_t num[ 8 ];
			V_snwprintf( num, sizeof( num ), L"%d%%", pPlayer->GetStatNumXP( ASW_XP_MISSION ) );
			m_pStatNum->SetText( num );

			m_pStatsBar->Init( 0, pPlayer->GetEarnedXP( ASW_XP_MISSION ), flMissionRate, true, false );
			m_pStatsBar->AddMinMax( 0, 1000 );
			break;
		}

		case ASW_XP_KILLS:
		{
			wchar_t num[ 8 ];
			if ( pPlayer->GetStatNumXP( ASW_XP_KILLS ) > 100 )
			{
				V_snwprintf( num, sizeof( num ), L"100+" );
			}
			else
			{
				V_snwprintf( num, sizeof( num ), L"%d", pPlayer->GetStatNumXP( ASW_XP_KILLS ) );
			}
			m_pStatNum->SetText( num );

			m_pStatsBar->Init( 0, pPlayer->GetEarnedXP( ASW_XP_KILLS ), flRate, true, false );
			m_pStatsBar->AddMinMax( 0, 100 );
			break;
		}

		case ASW_XP_TIME:
		{
			int nTime = pPlayer->GetStatNumXP( ASW_XP_TIME );
			wchar_t num[ 8 ];

			int nMinutes = nTime / 60;
			int nSeconds = nTime - nMinutes * 60;

			V_snwprintf( num, sizeof( num ), L"%d:%02d", nMinutes, nSeconds );

			m_pStatNum->SetText( num );

			m_pStatsBar->Init( 0, pPlayer->GetEarnedXP( ASW_XP_TIME ), flRate, true, false );
			m_pStatsBar->AddMinMax( 0, 100 );
			break;
		}

		case ASW_XP_FRIENDLY_FIRE:
		{
			wchar_t num[ 8 ];
			if ( pPlayer->GetStatNumXP( ASW_XP_FRIENDLY_FIRE ) > 100 )
			{
				V_snwprintf( num, sizeof( num ), L"100+" );
			}
			else
			{
				V_snwprintf( num, sizeof( num ), L"%d", pPlayer->GetStatNumXP( ASW_XP_FRIENDLY_FIRE ) );
			}
			m_pStatNum->SetText( num );

			m_pStatsBar->Init( 0, pPlayer->GetEarnedXP( ASW_XP_FRIENDLY_FIRE ), flRate, true, false );
			m_pStatsBar->AddMinMax( 0, 100 );
			break;
		}

		case ASW_XP_DAMAGE_TAKEN:
		{
			wchar_t num[ 8 ];
			if ( pPlayer->GetStatNumXP( ASW_XP_DAMAGE_TAKEN ) > 100 )
			{
				V_snwprintf( num, sizeof( num ), L"100+" );
			}
			else
			{
				V_snwprintf( num, sizeof( num ), L"%d", pPlayer->GetStatNumXP( ASW_XP_DAMAGE_TAKEN ) );
			}
			m_pStatNum->SetText( num );

			m_pStatsBar->Init( 0, pPlayer->GetEarnedXP( ASW_XP_DAMAGE_TAKEN ), flRate, true, false );
			m_pStatsBar->AddMinMax( 0, 100 );
			break;
		}

		case ASW_XP_HEALING:
		{
			wchar_t num[ 8 ];
			if ( pPlayer->GetStatNumXP( ASW_XP_HEALING ) > 500 )
			{
				V_snwprintf( num, sizeof( num ), L"500+" );
			}
			else
			{
				V_snwprintf( num, sizeof( num ), L"%d", pPlayer->GetStatNumXP( ASW_XP_HEALING ) );
			}
			m_pStatNum->SetText( num );

			m_pStatsBar->Init( 0, pPlayer->GetEarnedXP( ASW_XP_HEALING ), flRate, true, false );
			m_pStatsBar->AddMinMax( 0, 50 );
			break;
		}

		case ASW_XP_HACKING:
		{
			wchar_t num[ 8 ];
			if ( pPlayer->GetStatNumXP( ASW_XP_HACKING ) > 2 )
			{
				V_snwprintf( num, sizeof( num ), L"2+" );
			}
			else
			{
				V_snwprintf( num, sizeof( num ), L"%d", pPlayer->GetStatNumXP( ASW_XP_HACKING ) );
			}
			m_pStatNum->SetText( num );

			m_pStatsBar->Init( 0, pPlayer->GetEarnedXP( ASW_XP_HACKING ), flRate, true, false );
			m_pStatsBar->AddMinMax( 0, 50 );
			break;
		}
	}
	
	m_pStatsBar->SetStartCountingTime( gpGlobals->curtime + 7.0f + 0.5f * (int) m_XPType );

	UpdateVisibility( pPlayer );
}

void ExperienceStatLine::SetVisible(bool state)
{
	BaseClass::SetVisible( state );
}

// ===================================================

MedalStatLine::MedalStatLine( Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_pTitle = new vgui::Label( this, "Title", "" );
	m_pDescription = new vgui::Label( this, "Description", "" );
	m_pCounter = new vgui::Label( this, "Counter", "" );
	m_pIcon = new vgui::ImagePanel( this, "Icon" );
	m_pIcon->SetShouldScaleImage( true );

	m_nAchievementIndex = -1;
	m_nMedalIndex = -1;

	m_szMedalName[0] = '\0';
	m_szMedalDescription[0] = '\0';
	m_bShowTooltip = true;
	m_nXP = 0;
}

MedalStatLine::~MedalStatLine()
{

}

void MedalStatLine::PerformLayout()
{
	BaseClass::PerformLayout();
}

BriefingTooltip* MedalStatLine::GetTooltip()
{
	if ( m_hTooltip.Get() )
		return m_hTooltip.Get();

	return g_hBriefingTooltip.Get();
}

void MedalStatLine::OnThink()
{	
	if ( ( m_nMedalIndex != -1 || m_nAchievementIndex != -1 ) && m_bShowTooltip && IsCursorOver()
		&& GetTooltip() && GetTooltip()->GetTooltipPanel()!= this
		&& IsFullyVisible())
	{	
		int tx, ty, w, h;
		tx = ty = 0;
		LocalToScreen(tx, ty);
		GetSize(w, h);
		tx += w * 0.5f;
		ty -= h * 0.01f;
		GetTooltip()->SetTooltip(this, m_szMedalName, m_szMedalDescription,
			tx, ty);
	}
}


void MedalStatLine::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/MedalStatLine.res" );

	if ( m_nMedalIndex != -1 )
	{
		//int nMedalImage = m_nMedalIndex;
		bool bCheckSingleplayerTooltip = false;
// 		if ( nMedalImage >= 44 && nMedalImage <= 51 )	// speedrun
// 		{
// 			nMedalImage = 44;
// 			bCheckSingleplayerTooltip = true;
// 		}

		if ( ASWGameRules() )
		{
			int nAchievementIndex = GetAchievementIndexForMedal( m_nMedalIndex );
			if ( nAchievementIndex != -1 )
			{
				CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
				if ( pAchievementMgr )
				{
					CBaseAchievement *pAch = pAchievementMgr->GetAchievementByID( nAchievementIndex, 0 );
					if ( pAch )
					{
						char buffer[128];
						Q_snprintf( buffer, sizeof(buffer), "achievements/%s", pAch->GetName() );
						m_pIcon->SetImage(buffer);
					}
				}
			}
		}

		//m_pIcon->SetImage( VarArgs( "swarm/medals/medal%d", nMedalImage ) );
		Q_snprintf( m_szMedalName, sizeof( m_szMedalName ), "#asw_medal%d", m_nMedalIndex );
		m_pTitle->SetText( m_szMedalName );

		if ( bCheckSingleplayerTooltip && ASWGameResource() && ASWGameResource()->IsOfflineGame() )
		{
			Q_snprintf( m_szMedalDescription, sizeof( m_szMedalDescription ), "#asw_medaltt%dsp", m_nMedalIndex );
		}
		else
		{
			Q_snprintf( m_szMedalDescription, sizeof( m_szMedalDescription ), "#asw_medaltt%d", m_nMedalIndex );
		}
		m_pDescription->SetText( m_szMedalDescription );
		m_pCounter->SetText( VarArgs( "+%d", GetXPForMedal( m_nMedalIndex ) ) );
		m_nXP = GetXPForMedal( m_nMedalIndex );
	}
	else if ( m_nAchievementIndex != -1 )
	{
		CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
		if ( !pAchievementMgr )
			return;

		CBaseAchievement *pAchievement = pAchievementMgr->GetAchievementByID ( m_nAchievementIndex, 0 );
		if ( pAchievement)
		{
			m_pIcon->SetImage( pAchievement->GetIconPath() );
			Q_snprintf( m_szMedalName, sizeof( m_szMedalName ), "#%s_NAME", pAchievement->GetName() );
			m_pTitle->SetText( m_szMedalName );

			Q_snprintf( m_szMedalDescription, sizeof( m_szMedalDescription ), "#%s_DESC", pAchievement->GetName() );
			m_pDescription->SetText( m_szMedalDescription );
			m_pCounter->SetText( VarArgs( "+%d", GetXPForMedal( -m_nAchievementIndex ) ) );
			m_nXP = GetXPForMedal( -m_nAchievementIndex );
		}
	}
}

void MedalStatLine::SetMedalIndex( int nMedalIndex )
{
	if ( m_nMedalIndex != nMedalIndex )
	{
		m_nMedalIndex = nMedalIndex;
		if ( nMedalIndex != -1 )
		{
			m_nAchievementIndex = -1;
		}
		InvalidateLayout( false, true );
	}
}

void MedalStatLine::SetAchievementIndex( int nAchievementIndex )
{
	if ( m_nAchievementIndex != nAchievementIndex )
	{
		m_nAchievementIndex = nAchievementIndex;
		if ( m_nAchievementIndex != -1 )
		{
			m_nMedalIndex = -1;
		}
		InvalidateLayout( false, true );
	}
}

