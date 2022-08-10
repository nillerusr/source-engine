//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "c_team.h"
#include "tf_spectatorgui.h"
#include "tf_shareddefs.h"
#include "tf_gamerules.h"
#include "tf_hud_objectivestatus.h"
#include "tf_hud_statpanel.h"
#include "iclientmode.h"
#include "c_playerresource.h"
#include "tf_hud_building_status.h"
#include "tf_hud_winpanel.h"
#include "tf_tips.h"
#include "tf_mapinfomenu.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>

using namespace vgui;

extern ConVar _cl_classmenuopen;

const char *GetMapDisplayName( const char *mapName );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFSpectatorGUI::CTFSpectatorGUI(IViewPort *pViewPort) : CSpectatorGUI(pViewPort)
{
	m_flNextTipChangeTime = 0;
	m_iTipClass = TF_CLASS_UNDEFINED;

	m_nEngBuilds_xpos = m_nEngBuilds_ypos = 0;
	m_nSpyBuilds_xpos = m_nSpyBuilds_ypos = 0;

	m_pReinforcementsLabel = new Label( this, "ReinforcementsLabel", "" );
	m_pClassOrTeamLabel = new Label( this, "ClassOrTeamLabel", "" );
	m_pSwitchCamModeKeyLabel = new Label( this, "SwitchCamModeKeyLabel", "" );
	m_pCycleTargetFwdKeyLabel = new Label( this, "CycleTargetFwdKeyLabel", "" );
	m_pCycleTargetRevKeyLabel = new Label( this, "CycleTargetRevKeyLabel", "" );
	m_pMapLabel = new Label( this, "MapLabel", "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFSpectatorGUI::Reset( void )
{
	BaseClass::Reset();
}

//-----------------------------------------------------------------------------
// Purpose: makes the GUI fill the screen
//-----------------------------------------------------------------------------
void CTFSpectatorGUI::PerformLayout( void )
{
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFSpectatorGUI::NeedsUpdate( void )
{
	if ( !C_BasePlayer::GetLocalPlayer() )
		return false;

	if( IsVisible() )
		return true;

	return BaseClass::NeedsUpdate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFSpectatorGUI::Update()
{
	BaseClass::Update();

	UpdateReinforcements();
	UpdateKeyLabels();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFSpectatorGUI::UpdateReinforcements( void )
{
	if( !m_pReinforcementsLabel )
		return;

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pPlayer || pPlayer->IsHLTV() ||
		( pPlayer->GetTeamNumber() != TF_TEAM_RED && pPlayer->GetTeamNumber() != TF_TEAM_BLUE ) ||
		( pPlayer->m_Shared.GetState() != TF_STATE_OBSERVER && pPlayer->m_Shared.GetState() != TF_STATE_DYING ) ||
		pPlayer->GetObserverMode() == OBS_MODE_FREEZECAM )
	{
		m_pReinforcementsLabel->SetVisible( false );
		return;
	}

	wchar_t wLabel[128];
	
	if ( TFGameRules()->InStalemate() )
	{
		g_pVGuiLocalize->ConstructString( wLabel, sizeof( wLabel ), g_pVGuiLocalize->Find( "#game_respawntime_stalemate" ), 0 );
	}
	else if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN )
	{
		// a team has won the round
		g_pVGuiLocalize->ConstructString( wLabel, sizeof( wLabel ), g_pVGuiLocalize->Find( "#game_respawntime_next_round" ), 0 );
	}
	else
	{
		float flNextRespawn = TFGameRules()->GetNextRespawnWave( pPlayer->GetTeamNumber(), pPlayer );
		if ( !flNextRespawn )
		{
			m_pReinforcementsLabel->SetVisible( false );
			return;
		}

		int iRespawnWait = (flNextRespawn - gpGlobals->curtime);
		if ( iRespawnWait <= 0 )
		{
			g_pVGuiLocalize->ConstructString( wLabel, sizeof( wLabel ), g_pVGuiLocalize->Find("#game_respawntime_now" ), 0 );
		}
		else if ( iRespawnWait <= 1.0 )
		{
			g_pVGuiLocalize->ConstructString( wLabel, sizeof( wLabel ), g_pVGuiLocalize->Find("#game_respawntime_in_sec" ), 0 );
		}
		else
		{
			char szSecs[6];
			Q_snprintf( szSecs, sizeof(szSecs), "%d", iRespawnWait );
			wchar_t wSecs[4];
			g_pVGuiLocalize->ConvertANSIToUnicode(szSecs, wSecs, sizeof(wSecs));
			g_pVGuiLocalize->ConstructString( wLabel, sizeof( wLabel ), g_pVGuiLocalize->Find("#game_respawntime_in_secs" ), 1, wSecs );
		}
	}

	m_pReinforcementsLabel->SetVisible( true );
	m_pReinforcementsLabel->SetText( wLabel, true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFSpectatorGUI::UpdateKeyLabels( void )
{
	// get the desired player class
	int iClass = TF_CLASS_UNDEFINED;
	bool bIsHLTV = engine->IsHLTV();

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( pPlayer )
	{
		iClass = pPlayer->m_Shared.GetDesiredPlayerClassIndex();
	}

	// if it's time to change the tip, or the player has changed desired class, update the tip
	if ( ( gpGlobals->curtime >= m_flNextTipChangeTime ) || ( iClass != m_iTipClass ) )
	{
		if ( bIsHLTV )
		{
			const wchar_t *wzTip = g_pVGuiLocalize->Find( "#Tip_HLTV" );

			if ( wzTip )
			{
				SetDialogVariable( "tip", wzTip );
			}
		}
		else
		{
			wchar_t wzTipLabel[512]=L"";
			const wchar_t *wzTip = g_TFTips.GetNextClassTip( iClass );
			Assert( wzTip && wzTip[0] );
			g_pVGuiLocalize->ConstructString( wzTipLabel, sizeof( wzTipLabel ), g_pVGuiLocalize->Find( "#Tip_Fmt" ), 1, wzTip );
			SetDialogVariable( "tip", wzTipLabel );
		}
		
		m_flNextTipChangeTime = gpGlobals->curtime + 10.0f;
		m_iTipClass = iClass;
	}

	if ( m_pClassOrTeamLabel )
	{
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pPlayer )
		{
			static wchar_t wzFinal[512] = L"";
			const wchar_t *wzTemp = NULL;

			if ( bIsHLTV )
			{
				wzTemp = g_pVGuiLocalize->Find( "#TF_Spectator_AutoDirector" );
			}
			else if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
			{
				wzTemp = g_pVGuiLocalize->Find( "#TF_Spectator_ChangeTeam" );
			}
			else
			{
				wzTemp = g_pVGuiLocalize->Find( "#TF_Spectator_ChangeClass" );
			}

			if ( wzTemp )
			{
				UTIL_ReplaceKeyBindings( wzTemp, 0, wzFinal, sizeof( wzFinal ) );
				m_pClassOrTeamLabel->SetText( wzFinal );
			}
		}
	}

	if ( m_pSwitchCamModeKeyLabel )
	{
		if ( ( pPlayer && pPlayer->GetTeamNumber() > TEAM_SPECTATOR ) && ( ( mp_forcecamera.GetInt() == OBS_ALLOW_NONE ) || mp_fadetoblack.GetBool() ) )
		{
			if ( m_pSwitchCamModeKeyLabel->IsVisible() )
			{
				m_pSwitchCamModeKeyLabel->SetVisible( false );

				Label *pLabel = dynamic_cast<Label *>( FindChildByName( "SwitchCamModeLabel" ) );
				if ( pLabel )
				{
					pLabel->SetVisible( false );
				}
			}
		}
		else
		{
			if ( !m_pSwitchCamModeKeyLabel->IsVisible() )
			{
				m_pSwitchCamModeKeyLabel->SetVisible( true );

				Label *pLabel = dynamic_cast<Label *>( FindChildByName( "SwitchCamModeLabel" ) );
				if ( pLabel )
				{
					pLabel->SetVisible( true );
				}
			}

			wchar_t wLabel[256] = L"";
			const wchar_t *wzTemp = g_pVGuiLocalize->Find( "#TF_Spectator_SwitchCamModeKey" );
			UTIL_ReplaceKeyBindings( wzTemp, 0, wLabel, sizeof( wLabel ) );
			m_pSwitchCamModeKeyLabel->SetText( wLabel );
		}
	}

	if ( m_pCycleTargetFwdKeyLabel )
	{
		if ( ( pPlayer && pPlayer->GetTeamNumber() > TEAM_SPECTATOR ) && ( mp_fadetoblack.GetBool() || ( mp_forcecamera.GetInt() == OBS_ALLOW_NONE ) ) )
		{
			if ( m_pCycleTargetFwdKeyLabel->IsVisible() )
			{
				m_pCycleTargetFwdKeyLabel->SetVisible( false );

				Label *pLabel = dynamic_cast<Label *>( FindChildByName( "CycleTargetFwdLabel" ) );
				if ( pLabel )
				{
					pLabel->SetVisible( false );
				}
			}
		}
		else
		{
			if ( !m_pCycleTargetFwdKeyLabel->IsVisible() )
			{
				m_pCycleTargetFwdKeyLabel->SetVisible( true );

				Label *pLabel = dynamic_cast<Label *>( FindChildByName( "CycleTargetFwdLabel" ) );
				if ( pLabel )
				{
					pLabel->SetVisible( true );
				}
			}

			wchar_t wLabel[256] = L"";
			const wchar_t *wzTemp = g_pVGuiLocalize->Find( "#TF_Spectator_CycleTargetFwdKey" );
			UTIL_ReplaceKeyBindings( wzTemp, 0, wLabel, sizeof( wLabel ) );
			m_pCycleTargetFwdKeyLabel->SetText( wLabel );
		}
	}

	if ( m_pCycleTargetRevKeyLabel )
	{
		if ( ( pPlayer && pPlayer->GetTeamNumber() > TEAM_SPECTATOR ) && ( mp_fadetoblack.GetBool() || ( mp_forcecamera.GetInt() == OBS_ALLOW_NONE ) ) )
		{
			if ( m_pCycleTargetRevKeyLabel->IsVisible() )
			{
				m_pCycleTargetRevKeyLabel->SetVisible( false );

				Label *pLabel = dynamic_cast<Label *>( FindChildByName( "CycleTargetRevLabel" ) );
				if ( pLabel )
				{
					pLabel->SetVisible( false );
				}
			}
		}
		else
		{
			if ( !m_pCycleTargetRevKeyLabel->IsVisible() )
			{
				m_pCycleTargetRevKeyLabel->SetVisible( true );

				Label *pLabel = dynamic_cast<Label *>( FindChildByName( "CycleTargetRevLabel" ) );
				if ( pLabel )
				{
					pLabel->SetVisible( true );
				}
			}

			wchar_t wLabel[256] = L"";
			const wchar_t *wzTemp = g_pVGuiLocalize->Find( "#TF_Spectator_CycleTargetRevKey" );
			UTIL_ReplaceKeyBindings( wzTemp, 0, wLabel, sizeof( wLabel ) );
			m_pCycleTargetRevKeyLabel->SetText( wLabel );
		}
	}

	if ( m_pMapLabel )
	{
		wchar_t wMapName[16];
		wchar_t wLabel[256];
		char szMapName[16];

		char tempname[128];
		Q_FileBase( engine->GetLevelName(), tempname, sizeof( tempname ) );
		Q_strlower( tempname );

		if ( IsX360() )
		{
			char *pExt = Q_stristr( tempname, ".360" );
			if ( pExt )
			{
				*pExt = '\0';
			}
		}

		Q_strncpy( szMapName, GetMapDisplayName( tempname ), sizeof( szMapName ) );

		g_pVGuiLocalize->ConvertANSIToUnicode( szMapName, wMapName, sizeof(wMapName));
		g_pVGuiLocalize->ConstructString( wLabel, sizeof( wLabel ), g_pVGuiLocalize->Find( "#Spec_Map" ), 1, wMapName );

		m_pMapLabel->SetText( wLabel ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows/hides the buy menu
//-----------------------------------------------------------------------------
void CTFSpectatorGUI::ShowPanel(bool bShow)
{
	if ( bShow != IsVisible() )
	{
		CTFHudObjectiveStatus *pStatus = GET_HUDELEMENT( CTFHudObjectiveStatus );
		CHudBuildingStatusContainer_Engineer *pEngBuilds = GET_NAMED_HUDELEMENT( CHudBuildingStatusContainer_Engineer, BuildingStatus_Engineer );
		CHudBuildingStatusContainer_Spy *pSpyBuilds = GET_NAMED_HUDELEMENT( CHudBuildingStatusContainer_Spy, BuildingStatus_Spy );

		if ( bShow )
		{
			int xPos = 0, yPos = 0;

			if ( pStatus )
			{
				pStatus->SetParent( this );
				pStatus->SetProportional( true );
			}

			if ( pEngBuilds )
			{
				pEngBuilds->GetPos( xPos, yPos );
				m_nEngBuilds_xpos = xPos;
				m_nEngBuilds_ypos = yPos;
				pEngBuilds->SetPos( xPos, GetTopBarHeight() );
			}

			if ( pSpyBuilds )
			{
				pSpyBuilds->GetPos( xPos, yPos );
				m_nSpyBuilds_xpos = xPos;
				m_nSpyBuilds_ypos = yPos;
				pSpyBuilds->SetPos( xPos, GetTopBarHeight() );
			}	

			m_flNextTipChangeTime = 0;	// force a new tip immediately

			InvalidateLayout();
		}
		else
		{
			if ( pStatus )
			{
				pStatus->SetParent( g_pClientMode->GetViewport() );
			}

			if ( pEngBuilds )
			{
				pEngBuilds->SetPos( m_nEngBuilds_xpos, m_nEngBuilds_ypos );
			}

			if ( pSpyBuilds )
			{
				pSpyBuilds->SetPos( m_nSpyBuilds_xpos, m_nSpyBuilds_ypos );
			}
		}

		UpdateKeyLabels();
	}

	BaseClass::ShowPanel( bShow );
}