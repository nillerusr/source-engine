//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "cbase.h"

#include "modelimagepanel.h"
#include "tf_badge_panel.h"

DECLARE_BUILD_FACTORY( CTFBadgePanel );

CTFBadgePanel::CTFBadgePanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{
	m_pBadgePanel = new CModelImagePanel( this, "BadgePanel" );
	m_nPrevLevel = 0;
}


void CTFBadgePanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pBadgePanel->LoadControlSettings( "resource/ui/BadgePanel.res" );
}


void CTFBadgePanel::SetupBadge( const IProgressionDesc* pProgress, const LevelInfo_t& levelInfo )
{
	if ( !pProgress )
		return;

	pProgress->SetupBadgePanel( m_pBadgePanel, levelInfo );

	if ( m_nPrevLevel != levelInfo.m_nLevelNum )
	{
		m_nPrevLevel = levelInfo.m_nLevelNum;
		m_pBadgePanel->InvalidateImage();
	}
}


void CTFBadgePanel::SetupBadge( const IProgressionDesc* pProgress, const CSteamID& steamID )
{
	if ( pProgress && steamID.IsValid() )
	{
		SetupBadge( pProgress, pProgress->YieldingGetLevelForSteamID( steamID ) );
	}
}
