//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "achievement_stats_summary.h"
#include "achievements_page.h"
#include "lifetime_stats_page.h"
#include "match_stats_page.h"
#include "stats_summary.h"

#include <stdio.h>

using namespace vgui;

#include <vgui/ILocalize.h>
#include "vgui/ISurface.h"

#include "filesystem.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


const int cDialogWidth = 900;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CAchievementAndStatsSummary::CAchievementAndStatsSummary(vgui::Panel *parent) : BaseClass(parent, "AchievementAndStatsSummary")
{
    SetDeleteSelfOnClose(false);
    //SetBounds(0, 0, 640, 384);
    SetBounds(0, 0, 900, 780);
    SetMinimumSize( 640, 780 );
    SetSizeable( false );

    SetTitle("#GameUI_CreateAchievementsAndStats", true);
    SetOKButtonText("#GameUI_Close");
    SetCancelButtonVisible(false);

	m_pStatsSummary = new CStatsSummary( this, "StatsSummary" );
    m_pAchievementsPage = new CAchievementsPage(this, "AchievementsPage");
    m_pLifetimeStatsPage = new CLifetimeStatsPage(this, "StatsPage");
	m_pMatchStatsPage = new CMatchStatsPage(this, "MatchStatsPage");

	AddPage(m_pStatsSummary, "#GameUI_Stats_Summary");
	AddPage(m_pAchievementsPage, "#GameUI_Achievements_Tab");    
	AddPage(m_pMatchStatsPage, "#GameUI_MatchStats");
	AddPage(m_pLifetimeStatsPage, "#GameUI_LifetimeStats");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CAchievementAndStatsSummary::~CAchievementAndStatsSummary()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAchievementAndStatsSummary::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	// [smessick] Close the achievements dialog for a low resolution screen.
	if ( screenWide < cAchievementsDialogMinWidth )
	{
		OnOK( true );
		Close();
	}
}

//-----------------------------------------------------------------------------
// Purpose: runs the server when the OK button is pressed
//-----------------------------------------------------------------------------
bool CAchievementAndStatsSummary::OnOK(bool applyOnly)
{
    BaseClass::OnOK(applyOnly);

    return true;
}

//----------------------------------------------------------
// Purpose: Preserve our width to the one in the .res file
//----------------------------------------------------------
void CAchievementAndStatsSummary::OnSizeChanged(int newWide, int newTall)
{
    // Lock the width, but allow height scaling
    if ( newWide != cDialogWidth )
    {
        SetSize( cDialogWidth, newTall );
        return;
    }

    BaseClass::OnSizeChanged(newWide, newTall);
}

//----------------------------------------------------------
// Purpose: Processes when summary dialog is activated.
//----------------------------------------------------------
void CAchievementAndStatsSummary::Activate()
{
	m_pStatsSummary->MakeReadyForUse();
	m_pStatsSummary->UpdateStatsData();
    m_pAchievementsPage->UpdateAchievementDialogInfo();
    m_pLifetimeStatsPage->UpdateStatsData();
	m_pMatchStatsPage->UpdateStatsData();

    BaseClass::Activate();
}
