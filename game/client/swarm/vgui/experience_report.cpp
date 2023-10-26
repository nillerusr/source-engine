#include "cbase.h"
#include "experience_report.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/Label.h"
#include "StatsBar.h"
#include "experience_bar.h"
#include "experience_stat_line.h"
#include "c_asw_player.h"
#include "c_playerresource.h"
#include "asw_gamerules.h"
#include "weapon_unlock_panel.h"
#include "SkillAnimPanel.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "missioncompletepanel.h"
#include "igameevents.h"
#include "asw_medal_store.h"
#include "nb_island.h"
#include "clientmode_asw.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern float g_flXPDifficultyScale[4];

CExperienceReport::CExperienceReport( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_pXPBreakdownBackground = new CNB_Island( this, "XPBreakdownBackground" );
	m_pXPBreakdownBackground->m_pTitle->SetText( "#asw_xp_total" );

	for ( int i = 0; i < ASW_EXPERIENCE_REPORT_MAX_PLAYERS; i++ )
	{
		if ( i <= 0 )
		{
			m_pExperienceBar[ i ] = new ExperienceBar( this, VarArgs( "ExperienceBar%d", i ) );
		}
		else
		{
			m_pExperienceBar[ i ] = new ExperienceBarSmall( this, VarArgs( "ExperienceBar%d", i ) );
		}
	}
	for ( int i = 0; i < ASW_EXPERIENCE_REPORT_STAT_LINES; i++ )
	{
		m_pStatLine[ i ] = new ExperienceStatLine( this, VarArgs( "ExperienceStatLine%d", i ), (CASW_Earned_XP_t) i );
	}

	m_pEarnedXPTitle = new vgui::Label( this, "EarnedXPTitle", "" );
	m_pEarnedXPNumber = new vgui::Label( this, "EarnedXPNumber", "" );

	m_pXPDifficultyScaleTitle = new vgui::Label( this, "XPDifficultyScaleTitle", "" );
	m_pXPDifficultyScaleNumber = new vgui::Label( this, "XPDifficultyScaleNumber", "" );
	
	m_pSkillAnim = new SkillAnimPanel(this, "SkillAnim");

	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==	
	m_pMedalsTitle = new vgui::Label( this, "MedalsTitle", "#asw_medals_title" );
	m_pWeaponUnlockPanel = new WeaponUnlockPanel( this, "WeaponUnlockPanel" );
	// == MANAGED_MEMBER_CREATION_END ==

	m_pCheatsUsedLabel = new vgui::Label( this, "CheatsUsedLabel", "#asw_cheated" );
	m_pCheatsUsedLabel->SetVisible( ASWGameRules() && ASWGameRules()->m_bCheated.Get() );

	m_pUnofficialMapLabel = new vgui::Label( this, "UnofficialMapLabel", "#asw_unofficial_map" );
	m_pUnofficialMapLabel->SetVisible( false ); //GetClientModeASW() && !GetClientModeASW()->IsOfficialMap() );

	m_iPlayerLevel = 0;
	m_pszWeaponUnlockClass = NULL;
	m_flOldBarMin = -1;
	m_bOldCapped = false;
	m_flUnlockSequenceStart = 0.0f;

	m_bDoneAnimating = false;
	m_bPendingUnlockSequence = false;

	m_nPendingSuggestDifficulty = 0;
	m_flSuggestDifficultyStart = 0.0f;

	m_szMedalString[0] = 0;
}

CExperienceReport::~CExperienceReport()
{

}

void CExperienceReport::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	LoadControlSettings( "resource/UI/ExperienceReport.res" );

	BaseClass::ApplySchemeSettings(pScheme);

	for ( int i = 1; i < ASW_EXPERIENCE_REPORT_STAT_LINES; i++ )
	{
		m_pStatLine[ i ]->UpdateVisibility();
	}
}

void CExperienceReport::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, h;
	m_pStatLine[ 0 ]->GetBounds( x, y, w, h );
	int first_line_y = y;

	int padding = YRES( 6 );
	y += h + padding;

	// align stat lines in a vertical list one after the other
	for ( int i = 1; i < ASW_EXPERIENCE_REPORT_STAT_LINES; i++ )
	{
		if ( m_pStatLine[ i ]->IsVisible() )
		{
			m_pStatLine[ i ]->SetPos( x, y );
			m_pStatLine[ i ]->SetSize( w, h );
			y += h + padding;
		}
	}

	// align medal/achievement panels
	for ( int i = 0; i < m_pMedalLines.Count(); i++ )
	{
		m_pMedalLines[i]->SetPos( x, y );
		m_pMedalLines[i]->InvalidateLayout( true );
		int mw, mh;
		m_pMedalLines[i]->GetSize( mw, mh );
		m_pMedalLines[i]->SetSize( w, mh );
		y += mh;
	}

	y += YRES( 15 );

	m_pXPDifficultyScaleTitle->SetPos( x, y );
	m_pXPDifficultyScaleNumber->SetPos( x, y );

	y += m_pXPDifficultyScaleNumber->GetTall() + padding;

	m_pEarnedXPTitle->SetPos( x, y );
	m_pEarnedXPNumber->SetPos( x, y );

	
	if ( m_pCheatsUsedLabel->IsVisible() )
	{
		y += m_pEarnedXPTitle->GetTall();
		m_pCheatsUsedLabel->SetPos( x, y );
	}

	if ( m_pUnofficialMapLabel->IsVisible() )
	{
		y += m_pEarnedXPTitle->GetTall();
		m_pUnofficialMapLabel->SetPos( x, y );
	}

	int island_border = YRES( 10 );
	int island_top = first_line_y - YRES( 38 );
	int island_tall = ( y + YRES( 15 ) + island_border ) - island_top;
	m_pXPBreakdownBackground->SetBounds( x - island_border, island_top, YRES( 205 ) + island_border * 2, island_tall );

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		int nPlayerLevel = pPlayer->GetLevel();
		m_pWeaponUnlockPanel->SetVisible( nPlayerLevel < ASW_NUM_EXPERIENCE_LEVELS );

		if ( ASWGameRules() && ASWGameRules()->m_iSkillLevel == 2 && !ASWGameRules()->IsOfflineGame() &&
			 !( ASWGameRules()->IsCampaignGame() && ASWGameRules()->CampaignMissionsLeft() <= 1 ) )
		{
			CASW_Game_Resource *pGameResource = ASWGameResource();
			if ( pGameResource->GetLeader() == pPlayer )
			{
				m_nPendingSuggestDifficulty = pGameResource->m_nDifficultySuggestion;
			}
		}
	}
}

extern ConVar asw_skill;

void CExperienceReport::OnThink()
{
	bool bShowCheatsLabel = ASWGameRules() && ASWGameRules()->m_bCheated.Get();
	if ( bShowCheatsLabel != m_pCheatsUsedLabel->IsVisible() )
	{
		m_pCheatsUsedLabel->SetVisible( bShowCheatsLabel );
		InvalidateLayout();
	}
	bool bShowUnofficialMapLabel = false; //( GetClientModeASW() && !GetClientModeASW()->IsOfficialMap() );
	if ( bShowUnofficialMapLabel != m_pUnofficialMapLabel->IsVisible() )
	{
		m_pUnofficialMapLabel->SetVisible( bShowUnofficialMapLabel );
		InvalidateLayout();
	}
	
	// monitor local player's experience bar to see when it loops
	float flBarMin = m_pExperienceBar[ 0 ]->m_pExperienceBar->GetBarMin();
	bool bCapped = false;
	
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer )
	{
		bCapped = ( (int) m_pExperienceBar[ 0 ]->m_pExperienceBar->m_fCurrent ) >= ASW_XP_CAP * g_flPromotionXPScale[ pPlayer->GetPromotion() ];
		if ( m_flOldBarMin == -1 )
		{
			m_bOldCapped = bCapped;
		}

		if ( m_flOldBarMin != -1 && ( m_flOldBarMin != flBarMin || m_bOldCapped != bCapped ) )		// bar min has changed - player has levelled up!
		{
			m_bPendingUnlockSequence = true;
			IGameEvent *event = gameeventmanager->CreateEvent( "level_up" );
			int nNewLevel = LevelFromXP( (int) m_pExperienceBar[ 0 ]->m_pExperienceBar->m_fCurrent, pPlayer->GetPromotion() );
			if ( event )
			{
				event->SetInt( "level", nNewLevel );
				gameeventmanager->FireEventClientSide( event );
			}
			
			const char *szWeaponClassUnlocked = pPlayer->GetWeaponUnlockedAtLevel( nNewLevel );
			if ( szWeaponClassUnlocked )
			{
				if ( GetMedalStore() )
				{
					GetMedalStore()->OnUnlockedEquipment( szWeaponClassUnlocked );
				}
				MissionCompletePanel *pComplete = dynamic_cast<MissionCompletePanel*>( GetParent()->GetParent()->GetParent() );
				if ( pComplete )
				{
					pComplete->OnWeaponUnlocked( szWeaponClassUnlocked );
				}
			}

			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASW_XP.LevelUp" );
		}
	}

	m_flOldBarMin = flBarMin;
	m_bOldCapped = bCapped;

	float flTotalXP = 0.0f;
	for ( int i = 0; i < ASW_EXPERIENCE_REPORT_STAT_LINES; ++i )
	{
		if ( m_pStatLine[ i ]->IsVisible() )
		{
			flTotalXP += m_pStatLine[ i ]->m_pStatsBar->m_fCurrent;
		}
	}
	for ( int i = 0; i < m_pMedalLines.Count(); i++ )
	{
		flTotalXP += m_pMedalLines[i]->m_nXP;
	}
	if ( ASWGameRules() )
	{
		switch( ASWGameRules()->GetSkillLevel() )
		{
		case 1:	m_pXPDifficultyScaleNumber->SetText( "-50%" ); flTotalXP *= g_flXPDifficultyScale[0]; break;
		default:
		case 2: m_pXPDifficultyScaleNumber->SetText( "" ); flTotalXP *= g_flXPDifficultyScale[1]; break;
		case 3: m_pXPDifficultyScaleNumber->SetText( "+20%" ); flTotalXP *= g_flXPDifficultyScale[2]; break;
		case 4: m_pXPDifficultyScaleNumber->SetText( "+40%" ); flTotalXP *= g_flXPDifficultyScale[3]; break;
		case 5: m_pXPDifficultyScaleNumber->SetText( "+50%" ); flTotalXP *= g_flXPDifficultyScale[4]; break;
		}
		bool bShowDifficultyBonus = ( ASWGameRules()->GetSkillLevel() != 2 );
		m_pXPDifficultyScaleNumber->SetVisible( bShowDifficultyBonus );
		m_pXPDifficultyScaleTitle->SetVisible( bShowDifficultyBonus );
	}

	int nEarnedXP = (int) flTotalXP;
	wchar_t number_buffer[ 16 ];
	_snwprintf( number_buffer, sizeof( number_buffer ), L"+%d", nEarnedXP );
	m_pEarnedXPNumber->SetText( number_buffer );

	// See if we're still animating
	if ( !m_bDoneAnimating )
	{
		m_bDoneAnimating = true;

		for ( int i = 0; i < ASW_EXPERIENCE_REPORT_MAX_PLAYERS; ++i )
		{
			if ( !m_pExperienceBar[ i ]->IsDoneAnimating() )
			{
				m_bDoneAnimating = false;
				break;
			}
		}
	}

	UpdateMedals();

	MissionCompletePanel *pComplete = dynamic_cast<MissionCompletePanel*>( GetParent()->GetParent()->GetParent() );
	if ( pComplete && pComplete->m_iStage >= MissionCompletePanel::MCP_STAGE_STATS )
	{
		if ( m_bDoneAnimating && m_bPendingUnlockSequence )
		{
			if ( m_flUnlockSequenceStart == 0.0f )
			{
				m_flUnlockSequenceStart = gpGlobals->curtime + 1.0f;
			}
			else if ( gpGlobals->curtime > m_flUnlockSequenceStart )
			{
				m_iPlayerLevel++;
				m_pWeaponUnlockPanel->SetDetails( m_pszWeaponUnlockClass, m_iPlayerLevel );

				
				if ( pComplete )
				{
					pComplete->ShowQueuedUnlockPanels();
				}

				m_bPendingUnlockSequence = false;
				m_flUnlockSequenceStart = 0.0f;
			}
		}
		else if ( m_bDoneAnimating && m_nPendingSuggestDifficulty != 0 )
		{
			if ( pComplete->m_hSubScreen.Get() == NULL )
			{
				if ( m_flSuggestDifficultyStart == 0.0f )
				{
					m_flSuggestDifficultyStart = gpGlobals->curtime + 1.0f;
				}
				else if ( gpGlobals->curtime > m_flSuggestDifficultyStart )
				{
					pComplete->OnSuggestDifficulty( m_nPendingSuggestDifficulty == 1 );

					m_nPendingSuggestDifficulty = 0;
					m_flSuggestDifficultyStart = 0.0f;
				}
			}
		}
	}
}

// fills in all the bars and labels with the current players and their XP values
void CExperienceReport::Init()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	// local player
	m_iPlayerLevel = pPlayer->GetLevel();
	m_pszWeaponUnlockClass = pPlayer->GetNextWeaponClassUnlocked();

	pPlayer->AwardExperience();
	m_pExperienceBar[ 0 ]->InitFor( pPlayer );

	for ( int i = 0; i < ASW_EXPERIENCE_REPORT_STAT_LINES; i++ )
	{
		m_pStatLine[ i ]->InitFor( pPlayer );
	}

	int iBar = 1;

	// other players
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		if ( !g_PR->IsConnected( i ) )
			continue;

		C_ASW_Player *pOtherPlayer = static_cast<C_ASW_Player*>( UTIL_PlayerByIndex( i ) );
		if ( pOtherPlayer == pPlayer )
			continue;

		pOtherPlayer->AwardExperience();
		m_pExperienceBar[ iBar ]->InitFor( pOtherPlayer );
		iBar++;

		if ( iBar >= ASW_EXPERIENCE_REPORT_MAX_PLAYERS )
			break;
	}

	for ( int i = iBar; i < ASW_EXPERIENCE_REPORT_MAX_PLAYERS; i++ )
	{
		m_pExperienceBar[ i ]->InitFor( NULL );
	}

	m_pWeaponUnlockPanel->SetDetails( m_pszWeaponUnlockClass, m_iPlayerLevel );
}

void CExperienceReport::UpdateMedals()
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	C_ASW_Marine_Resource* pMR = ASWGameResource()->GetFirstMarineResourceForPlayer( pPlayer );
	if ( !pMR )
	{
		for ( int i = 0; i < m_pMedalLines.Count(); i++ )
		{
			m_pMedalLines[i]->SetVisible( false );
		}
		return;
	}
	
	const char *pMedals = pMR->m_MedalsAwarded;
	int iMedalNum = 0;

	// check to see if the medals awarded to our marine have changed
	if ( Q_strcmp( m_szMedalString, pMedals ) )
	{		
		Q_strncpy( m_szMedalString, pMedals, sizeof(m_szMedalString) );

		// break up the medal string into medal numbers
		const char	*p = m_szMedalString;
		char		token[128];

		p = nexttoken( token, p, ' ' );
		while ( Q_strlen( token ) > 0 )  
		{
			int iMedalIndex = atoi(token);

			if (m_pMedalLines.Count() <= iMedalNum)
			{
				// make a new medal
				m_pMedalLines.AddToTail( new MedalStatLine( this, "MedalStatLine" ) );
			}

			if (m_pMedalLines.Count() > iMedalNum)
			{
				if ( iMedalIndex > 0 )
				{
					m_pMedalLines[iMedalNum]->SetMedalIndex( iMedalIndex );
				}
				else
				{
					// negative medal numbers are achievements
					m_pMedalLines[iMedalNum]->SetAchievementIndex( -iMedalIndex );
				}
			}

			iMedalNum++;

			if (p)
				p = nexttoken( token, p, ' ' );
			else
				token[0] = '\0';
		}
		InvalidateLayout();
	}
}