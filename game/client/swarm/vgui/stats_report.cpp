#include "cbase.h"
#include "stats_report.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/ImagePanel.h"
#include <vgui_controls/AnimationController.h>
#include "objectivemap.h"
#include "stat_graph.h"
#include "stat_graph_player.h"
#include "debrieftextpage.h"
#include "asw_gamerules.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "c_playerresource.h"
#include "vgui_avatarimage.h"
#include "asw_briefing.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


StatsReport::StatsReport( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_pObjectiveMap = new ObjectiveMap( this, "ObjectiveMap" );
	
	for ( int i = 0; i < ASW_STATS_REPORT_CATEGORIES; i++ )
	{
		const char *pButtonName = VarArgs( "CategoryButton%d", i );
		m_pCategoryButtons[ i ] = new vgui::Button( this, pButtonName, "", this, pButtonName );
	}

	for ( int i = 0; i < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		m_pPlayerNames[ i ] = new vgui::Label( this, VarArgs( "PlayerName%d", i ), "" );
		m_pReadyCheckImages[ i ] = new vgui::ImagePanel( this, VarArgs( "ReadyCheck%d", i ) );
		m_pReadyCheckImages[ i ]->SetShouldScaleImage( true );
		m_pAvatarImages[ i ] = new CAvatarImagePanel( this, VarArgs( "AvatarImage%d", i ) );
	}

	m_pPlayerNamesPosition = new vgui::Panel( this, "PlayerNamesPosition" );

	m_pStatGraphPlayer = new StatGraphPlayer( this, "StatGraphPlayer" );

	m_pDebrief = new DebriefTextPage( this, "Debrief" );

	m_rgbaStatsReportPlayerColors[ 0 ] = Color( 225, 60, 60, 255 );
	m_rgbaStatsReportPlayerColors[ 1 ] = Color( 200, 200, 60, 255 );
	m_rgbaStatsReportPlayerColors[ 2 ] = Color( 60, 225, 60, 255 );
	m_rgbaStatsReportPlayerColors[ 3 ] = Color( 30, 90, 225, 255 );
}

StatsReport::~StatsReport()
{

}

void StatsReport::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	LoadControlSettings( "resource/UI/StatsReport.res" );

	m_pObjectiveMap->m_pMapImage->SetAlpha( 90 );

	BaseClass::ApplySchemeSettings(pScheme);

	SetPlayerNames();
}

void StatsReport::PerformLayout()
{
	BaseClass::PerformLayout();

	int padding = YRES( 6 );
	int paddingSmall = YRES( 2 );

	int nCategoryButtonX, nCategoryButtonY, nCategoryButtonW, nCategoryButtonH;
	nCategoryButtonY = m_pCategoryButtons[ 0 ]->GetTall();
	m_pCategoryButtons[ 0 ]->SizeToContents();
	m_pCategoryButtons[ 0 ]->SetTall( nCategoryButtonY );
	m_pCategoryButtons[ 0 ]->GetBounds( nCategoryButtonX, nCategoryButtonY, nCategoryButtonW, nCategoryButtonH );

	nCategoryButtonX += nCategoryButtonW + padding;

	// align stat lines in a vertical list one after the other
	for ( int i = 1; i < ASW_STATS_REPORT_CATEGORIES; i++ )
	{
		m_pCategoryButtons[ i ]->SetPos( nCategoryButtonX, nCategoryButtonY );
		m_pCategoryButtons[ i ]->SizeToContents();
		m_pCategoryButtons[ i ]->SetTall( nCategoryButtonH );
		nCategoryButtonW = m_pCategoryButtons[ i ]->GetWide();

		nCategoryButtonX += nCategoryButtonW + padding;
	}

	// Check box
	int nPlayerBoundsW, nPlayerBoundsH;
	int nReadyCheckImageX, nReadyCheckImageY, nReadyCheckImageYOffset, nReadyCheckImageW, nReadyCheckImageH;
	m_pPlayerNamesPosition->GetBounds( nReadyCheckImageX, nReadyCheckImageY, nPlayerBoundsW, nPlayerBoundsH );
	m_fPlayerNamePosY[ 0 ] = nReadyCheckImageY;
	nReadyCheckImageW = m_pReadyCheckImages[ 0 ]->GetTall();
	nReadyCheckImageH = m_pReadyCheckImages[ 0 ]->GetTall();

	nReadyCheckImageYOffset = nPlayerBoundsH / 2 - nReadyCheckImageH / 2;

	for ( int i = 0; i < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		m_pReadyCheckImages[ i ]->SetPos( nReadyCheckImageX, nReadyCheckImageY + nReadyCheckImageYOffset );
		m_pReadyCheckImages[ i ]->SetSize( nReadyCheckImageW, nReadyCheckImageH );
		m_fPlayerNamePosY[ i ] = nReadyCheckImageY;

		nReadyCheckImageY += nPlayerBoundsH + paddingSmall;
	}

	nPlayerBoundsW -= nReadyCheckImageW + paddingSmall;

	// Avatar
	int nAvatarImageX, nAvatarImageYOffset, nAvatarImageW, nAvatarImageH;
	nAvatarImageX = nReadyCheckImageX + nReadyCheckImageW + padding;
	nAvatarImageW = m_pAvatarImages[ 0 ]->GetWide();
	nAvatarImageH = m_pAvatarImages[ 0 ]->GetTall();

	nAvatarImageYOffset = nPlayerBoundsH / 2 - nAvatarImageH / 2;

	for ( int i = 0; i < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		m_pAvatarImages[ i ]->SetPos( nAvatarImageX, m_fPlayerNamePosY[ i ] + nAvatarImageYOffset );
		m_pAvatarImages[ i ]->SetSize( nAvatarImageW, nAvatarImageH );

		CAvatarImage *pImage = static_cast< CAvatarImage* >( m_pAvatarImages[ i ]->GetImage() );
		if ( pImage )
		{
			pImage->SetAvatarSize( nAvatarImageW, nAvatarImageH );
		}
	}

	nPlayerBoundsW -= nAvatarImageW + paddingSmall;

	// Player name
	int nPlayerNameX;
	nPlayerNameX = nAvatarImageX + nAvatarImageW + paddingSmall;

	for ( int i = 0; i < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		m_pPlayerNames[ i ]->SetPos( nPlayerNameX, m_fPlayerNamePosY[ i ] );
		m_pPlayerNames[ i ]->SetSize( nPlayerBoundsW, nPlayerBoundsH );
	}

	SetStatCategory( 1 );
}

void StatsReport::OnThink()
{
	int nMarine = 0;

	m_pObjectiveMap->ClearBlips();

	C_ASW_Game_Resource *pGameResource = ASWGameResource();

	for ( int i = 0; i < pGameResource->GetMaxMarineResources() && nMarine < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
		if ( pMR )
		{
			Vector vPos;
			vPos.x = pMR->m_TimelinePosX.GetValueAtInterp( m_pStatGraphPlayer->m_fTimeInterp );
			vPos.y = pMR->m_TimelinePosY.GetValueAtInterp( m_pStatGraphPlayer->m_fTimeInterp );
			vPos.z = 0.0f;

			bool bDead = ( pMR->m_TimelineHealth.GetValueAtInterp( m_pStatGraphPlayer->m_fTimeInterp ) <= 0.0f );
			
			m_pObjectiveMap->AddBlip( MapBlip_t( vPos, bDead ? Color( 255, 255, 255, 255 ) : m_rgbaStatsReportPlayerColors[ nMarine ], bDead ? MAP_BLIP_TEXTURE_DEATH : MAP_BLIP_TEXTURE_NORMAL ) );

			if ( m_pReadyCheckImages[ nMarine ]->IsVisible() )
			{
				C_ASW_Player *pPlayer = pMR->GetCommander();
				if ( pPlayer )
				{
					if ( !pMR->IsInhabited() || ASWGameResource()->IsPlayerReady( pPlayer ) )
					{
						m_pReadyCheckImages[ i ]->SetImage( "swarm/HUD/TickBoxTicked" );
					}
					else if ( pPlayer == ASWGameResource()->GetLeader() )
					{
						m_pReadyCheckImages[ i ]->SetImage( "swarm/PlayerList/LeaderIcon" );
					}
					else
					{
						m_pReadyCheckImages[ i ]->SetImage( "swarm/HUD/TickBoxEmpty" );
					}
				}
			}

			nMarine++;
		}
	}

	for ( int i = 0; i < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		
	}
}


void StatsReport::OnCommand( const char *command )
{
	if ( StringHasPrefix( command, "CategoryButton" ) )
	{
		int nCategory = command[ Q_strlen( "CategoryButton" ) ] - '0';
		SetStatCategory( nCategory );
	}

	BaseClass::OnCommand(command);
}


// fills in all the bars and labels with the current players and their XP values
void StatsReport::SetStatCategory( int nCategory )
{
	for ( int i = 0; i < ASW_STATS_REPORT_CATEGORIES; ++i )
	{
		m_pCategoryButtons[ i ]->SetDefaultColor( Color( 100, 100, 100, 255 ), Color( 35, 41, 57, 90 ) );
	}

	m_pCategoryButtons[ nCategory ]->SetDefaultColor( Color( 255, 255, 255, 255 ), Color( 35, 41, 57, 192 ) );

	int nRankOrder[ ASW_STATS_REPORT_MAX_PLAYERS ];
	float fBestValues[ ASW_STATS_REPORT_MAX_PLAYERS ];

	for ( int i = 0; i < ASW_STATS_REPORT_MAX_PLAYERS; ++i )
	{
		nRankOrder[ i ] = i;
	}

	//float fMinValue = FLT_MAX;
	float fMaxValue = -FLT_MAX;

	int nMarine = 0;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();

	for ( int i = 0; i < pGameResource->GetMaxMarineResources() && nMarine < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
		if ( pMR )
		{
			switch ( nCategory )
			{
			case 0:
				m_pStatGraphPlayer->m_pStatGraphs[ nMarine ]->SetTimeline( &pMR->m_TimelineFriendlyFire );
				fMaxValue = MAX( fMaxValue, 50.0f );
				break;

			case 1:
				m_pStatGraphPlayer->m_pStatGraphs[ nMarine ]->SetTimeline( &pMR->m_TimelineKillsTotal );
				break;

			case 2:
				m_pStatGraphPlayer->m_pStatGraphs[ nMarine ]->SetTimeline( &pMR->m_TimelineHealth );
				break;

			case 3:
				m_pStatGraphPlayer->m_pStatGraphs[ nMarine ]->SetTimeline( &pMR->m_TimelineAmmo );
				break;
			}

			fBestValues[ nMarine ] = m_pStatGraphPlayer->m_pStatGraphs[ nMarine ]->GetFinalValue();

			//fMinValue = MIN( fMinValue, m_pStatGraphPlayer->m_pStatGraphs[ nMarine ]->GetTroughValue() );
			fMaxValue = MAX( fMaxValue, m_pStatGraphPlayer->m_pStatGraphs[ nMarine ]->GetCrestValue() );

			nMarine++;
		}
	}
	
	// Sort the names based on who did the best in this category
	for ( int i = 0; i < ASW_STATS_REPORT_MAX_PLAYERS - 1; ++i )
	{
		for ( int j = 0; j < ASW_STATS_REPORT_MAX_PLAYERS - 1 - i; ++j )
		{
			if ( fBestValues[ j ] < fBestValues[ j + 1 ] )
			{
				float fTemp = fBestValues[ j ];
				fBestValues[ j ] = fBestValues[ j + 1 ];
				fBestValues[ j + 1 ] = fTemp;

				int nTemp = nRankOrder[ j ];
				nRankOrder[ j ] = nRankOrder[ j + 1 ];
				nRankOrder[ j + 1 ] = nTemp;
			}
		}
	}

	// Physically position all the marine names in their ranked order
	for ( int i = 0; i < ASW_STATS_REPORT_MAX_PLAYERS; ++i )
	{
		m_pStatGraphPlayer->m_pStatGraphs[ i ]->SetMinMaxValues( 0.0f, fMaxValue );
		vgui::GetAnimationController()->RunAnimationCommand( m_pPlayerNames[ nRankOrder[ i ] ], "ypos", m_fPlayerNamePosY[ i ], 0, 0.25f, vgui::AnimationController::INTERPOLATOR_LINEAR );
		vgui::GetAnimationController()->RunAnimationCommand( m_pAvatarImages[ nRankOrder[ i ] ], "ypos", m_fPlayerNamePosY[ i ], 0, 0.25f, vgui::AnimationController::INTERPOLATOR_LINEAR );
		vgui::GetAnimationController()->RunAnimationCommand( m_pReadyCheckImages[ nRankOrder[ i ] ], "ypos", m_fPlayerNamePosY[ i ], 0, 0.25f, vgui::AnimationController::INTERPOLATOR_LINEAR );
	}

	m_pStatGraphPlayer->InvalidateLayout( true );
}

void StatsReport::SetPlayerNames( void )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	int nMarine = 0;

	C_ASW_Game_Resource *pGameResource = ASWGameResource();

	for ( int i = 0; i < pGameResource->GetMaxMarineResources() && nMarine < ASW_STATS_REPORT_MAX_PLAYERS; i++ )
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource( i );
		if ( pMR )
		{
			C_ASW_Player *pCommander = pMR->GetCommander();

			Color color = m_rgbaStatsReportPlayerColors[ nMarine ];

			if ( pPlayer != pCommander )
			{
				color[ 3 ] = 128;
			}

			m_pStatGraphPlayer->m_pStatGraphs[ nMarine ]->SetLineColor( color );
			m_pPlayerNames[ nMarine ]->SetFgColor( color );

			wchar_t wszMarineName[ 32 ];
			pMR->GetDisplayName( wszMarineName, sizeof( wszMarineName ) );

			m_pPlayerNames[ nMarine ]->SetText( wszMarineName );

			if ( gpGlobals->maxClients == 1 )
			{
				// Don't need these in singleplayer
				m_pAvatarImages[ nMarine ]->SetVisible( false );
				m_pReadyCheckImages[ nMarine ]->SetVisible( false );
			}
			else
			{
#if !defined(NO_STEAM)
				CSteamID steamID;

				if ( pCommander )
				{
					player_info_t pi;
					if ( engine->GetPlayerInfo( pCommander->entindex(), &pi ) )
					{
						if ( pi.friendsID )
						{
							CSteamID steamIDForPlayer( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
							steamID = steamIDForPlayer;
						}
					}
				}

				if ( steamID.IsValid() )
				{
					m_pAvatarImages[ nMarine ]->SetAvatarBySteamID( &steamID );

					int wide, tall;
					m_pAvatarImages[ nMarine ]->GetSize( wide, tall );

					CAvatarImage *pImage = static_cast< CAvatarImage* >( m_pAvatarImages[ nMarine ]->GetImage() );
					if ( pImage )
					{
						pImage->SetAvatarSize( wide, tall );
						pImage->SetPos( -AVATAR_INDENT_X, -AVATAR_INDENT_Y );
					}
				}
#endif
			}

			nMarine++;
		}
	}

	while ( nMarine < ASW_STATS_REPORT_MAX_PLAYERS )
	{
		m_pAvatarImages[ nMarine ]->SetVisible( false );
		m_pReadyCheckImages[ nMarine ]->SetVisible( false );
		nMarine++;
	}
}