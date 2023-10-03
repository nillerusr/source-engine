#include "cbase.h"
#include <vgui_controls/Label.h>
#include "vgui_controls/AnimationController.h"
#include <vgui_controls/ImagePanel.h>
#include "statsbar.h"
#include "experience_bar.h"
#include "c_asw_player.h"
#include <vgui/ILocalize.h>
#include "skillanimpanel.h"
#include "clientmode_asw.h"
#include "asw_gamerules.h"
#include <vgui/IVGui.h>
#include "vgui_avatarimage.h"
#include "asw_player_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar asw_xp_screen_debug( "asw_xp_screen_debug", "0", FCVAR_CHEAT, "If enabled, XP screen will show dummy player slots" );

ExperienceBar::ExperienceBar(vgui::Panel *parent, const char *name) :
	vgui::EditablePanel( parent, name )
{
	m_flOldBarMin = -1.0f;
	m_bOldCapped = false;
	m_iPlayerLevel = 0;
	m_nOldPlayerXP = -1;

	m_pPlayerNameLabel = new vgui::Label( this, "PlayerNameLabel", "" );
	m_pPlayerLevelLabel = new vgui::Label( this, "PlayerLevelLabel", "" );
	m_pExperienceCounter = new vgui::Label( this, "ExperienceCounter", "" );
	m_pLevelUpLabel = new vgui::Label( this, "LevelUpLabel", "#asw_level_up" );
	m_pPromotionIcon = new vgui::ImagePanel( this, "PromotionIcon" );

	m_pAvatarBackground = new vgui::Panel( this, "AvatarBackground" );
	m_pAvatarImage = new CAvatarImagePanel( this, "AvatarImage" );	

	m_pExperienceBar = new StatsBar( this, "ExperienceBar" );
	m_pExperienceBar->UseExternalCounter( m_pExperienceCounter );
	m_pExperienceBar->SetShowMaxOnCounter( true );
	m_pExperienceBar->SetColors( Color( 255, 255, 255, 0 ), Color( 93,148,192,255 ), Color( 255, 255, 255, 255 ), Color( 17,37,57,255 ), Color( 35, 77, 111, 255 ) );
	//m_pExperienceBar->m_bShowCumulativeTotal = true;
	m_nLastPromotion = -1;
	UpdateMinMaxes( 0 );

	m_pExperienceBar->m_flBorder = 1.5f;

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}

void ExperienceBar::UpdateMinMaxes( int nPromotion )
{
	if ( m_nLastPromotion == nPromotion )
		return;

	m_nLastPromotion = nPromotion;
	m_pExperienceBar->ClearMinMax();
	m_pExperienceBar->AddMinMax( 0, g_iLevelExperience[ 0 ] * g_flPromotionXPScale[ m_nLastPromotion ] );
	for ( int i = 0; i < ASW_NUM_EXPERIENCE_LEVELS - 1; i++ )
	{
		m_pExperienceBar->AddMinMax( g_iLevelExperience[ i ] * g_flPromotionXPScale[ m_nLastPromotion ] , g_iLevelExperience[ i + 1 ] * g_flPromotionXPScale[ m_nLastPromotion ] );
	}
}

void ExperienceBar::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/ExperienceBar.res" );
	
	m_pLevelUpLabel->SetVisible( false );
}

void ExperienceBar::PerformLayout()
{
	BaseClass::PerformLayout();
	
	if ( m_pAvatarImage )
	{
		int wide, tall;
		m_pAvatarImage->GetSize( wide, tall );
		if ( ((CAvatarImage*)m_pAvatarImage->GetImage()) )
		{
			((CAvatarImage*)m_pAvatarImage->GetImage())->SetAvatarSize( wide, tall );
			((CAvatarImage*)m_pAvatarImage->GetImage())->SetPos( -AVATAR_INDENT_X, -AVATAR_INDENT_Y );
		}
	}
}

void ExperienceBar::OnTick()
{
	BaseClass::OnTick();

	SetVisible( m_hPlayer.Get() || asw_xp_screen_debug.GetBool() );

	if ( m_hPlayer.Get() )
	{
		int nPromotion = m_hPlayer->GetPromotion();

		if ( nPromotion <= 0 || nPromotion > ASW_PROMOTION_CAP )
		{
			m_pPromotionIcon->SetVisible( false );
		}
		else
		{
			m_pPromotionIcon->SetVisible( true );
			m_pPromotionIcon->SetImage( VarArgs( "briefing/promotion_%d", nPromotion ) );
		}
	}

	if ( m_hPlayer.Get() )
	{
		if ( ASWGameRules()->GetGameState() <= ASW_GS_BRIEFING )
		{
			int nXP = m_hPlayer->GetExperience();
			if ( nXP != m_nOldPlayerXP )
			{
				m_pExperienceBar->Init( nXP, nXP, 1.0, true, false );
				Msg( "Experience bar in briefing set xp to %d\n", nXP );
				m_nOldPlayerXP = nXP;

				m_iPlayerLevel = m_hPlayer->GetLevel();
				UpdateLevelLabel();
			}

			m_pPlayerNameLabel->SetText( m_hPlayer->GetPlayerName() );
		}
		else
		{
			float flBarMin = m_pExperienceBar->GetBarMin();
			bool bCapped = ( (int) m_pExperienceBar->m_fCurrent ) >= ASW_XP_CAP * g_flPromotionXPScale[ m_hPlayer->GetPromotion() ];

			if ( m_flOldBarMin == -1 )
			{
				m_bOldCapped = bCapped;
			}

			if ( m_flOldBarMin != -1 && ( m_flOldBarMin != flBarMin || m_bOldCapped != bCapped ) )		// bar min has changed - player has levelled up!
			{
				m_iPlayerLevel = LevelFromXP( m_pExperienceBar->m_fCurrent, m_hPlayer->GetPromotion() );
				UpdateLevelLabel();

				m_pLevelUpLabel->SetVisible( true );
				SkillAnimPanel *pSkillAnim = dynamic_cast<SkillAnimPanel*>(GetClientMode()->GetViewport()->FindChildByName("SkillAnimPanel", true));
				if ( pSkillAnim )
				{
					pSkillAnim->AddParticlesAroundPanel( m_pPlayerLevelLabel );
				}
			}
			
			m_flOldBarMin = flBarMin;
			m_bOldCapped = bCapped;
		}
	}
}

void ExperienceBar::InitFor( C_ASW_Player *pPlayer )
{
	m_hPlayer = pPlayer;
	if ( !pPlayer )
	{		
		if ( !asw_xp_screen_debug.GetBool() )
		{
			SetVisible( false );
		}
		else
		{
			m_pPlayerNameLabel->SetText( "Player" );
			m_pPlayerLevelLabel->SetText( "Level 5" );
			m_pExperienceBar->Init( 1200, 1500, 1500.0f / 4.0f, true, false );
			m_pExperienceBar->SetStartCountingTime( gpGlobals->curtime + 15.0f );
		}
		return;
	}

	SetVisible( true );
	
	m_pPlayerNameLabel->SetText( pPlayer->GetPlayerName() );

#if !defined(NO_STEAM)
	CSteamID steamID = pPlayer->GetSteamID();
	if ( steamID.IsValid() )
	{
		if ( steamID.ConvertToUint64() != m_lastSteamID.ConvertToUint64() )
		{
			m_pAvatarImage->SetAvatarBySteamID( &steamID );

			int wide, tall;
			m_pAvatarImage->GetSize( wide, tall );
			((CAvatarImage*)m_pAvatarImage->GetImage())->SetAvatarSize( wide, tall );
			((CAvatarImage*)m_pAvatarImage->GetImage())->SetPos( -AVATAR_INDENT_X, -AVATAR_INDENT_Y );
		}
		m_lastSteamID = steamID;
	}
#endif

	UpdateMinMaxes( pPlayer->GetPromotion() );
	if ( ASWGameRules()->GetGameState() <= ASW_GS_BRIEFING )
	{
		m_iPlayerLevel = pPlayer->GetLevel();
		UpdateLevelLabel();	

		m_pExperienceBar->Init( pPlayer->GetExperience(), pPlayer->GetExperience(), 1.0f, true, false );
		Msg( "init xp bar to %d / %d.  pPlayer->GetLevel() = %d\n", pPlayer->GetExperience(), pPlayer->GetExperience(), pPlayer->GetLevel() );
	}
	else
	{
		m_iPlayerLevel = pPlayer->GetLevelBeforeDebrief();
		UpdateLevelLabel();
		

		int iEarnedXP = pPlayer->GetEarnedXP( ASW_XP_TOTAL );
		int nGoalXP = pPlayer->GetExperienceBeforeDebrief() + iEarnedXP;
		nGoalXP = MIN( nGoalXP, ASW_XP_CAP * g_flPromotionXPScale[ pPlayer->GetPromotion() ] );
		float flRate = (float) iEarnedXP / 3.0f;	// take 4 seconds to increase XP.
		if ( iEarnedXP < 150 )		// if XP is really low, count it up in 1 second
		{
			flRate = (float) iEarnedXP;
		}
		m_pExperienceBar->Init( pPlayer->GetExperienceBeforeDebrief(), nGoalXP, flRate, true, false );
		Msg( "init xp bar to %d / %d.  pPlayer->GetLevelBeforeDebrief() = %d\n", pPlayer->GetExperienceBeforeDebrief(), nGoalXP, pPlayer->GetLevelBeforeDebrief() );
		
		m_pExperienceBar->SetStartCountingTime( gpGlobals->curtime + 11.0f );
	}
}

void ExperienceBar::UpdateLevelLabel()
{
	wchar_t szLevelNum[16]=L"";
	_snwprintf( szLevelNum, ARRAYSIZE( szLevelNum ), L"%i", m_iPlayerLevel + 1 );  // levels start at 0 in code, but show from 1 in the UI

	wchar_t wzLevelLabel[64];
	g_pVGuiLocalize->ConstructString( wzLevelLabel, sizeof( wzLevelLabel ), g_pVGuiLocalize->Find( "#asw_experience_level" ), 1, szLevelNum );
	m_pPlayerLevelLabel->SetText( wzLevelLabel );
}

bool ExperienceBar::IsDoneAnimating()
{
	if ( !m_hPlayer.Get() )
		return true;

	if ( !IsVisible() )
		return true;

	return m_pExperienceBar->IsDoneAnimating();
}

// Small version ======================

ExperienceBarSmall::ExperienceBarSmall(vgui::Panel *parent, const char *name) :
	BaseClass( parent, name )
{
	m_pExperienceBar->m_flBorder = 0.0f;
}

void ExperienceBarSmall::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/ExperienceBarSmall.res" );

	m_pLevelUpLabel->SetVisible( false );
}