//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Display a list of achievements for the current game
//
//=============================================================================//

#include "cbase.h"
#include "achievements_page.h"
#include "vgui_controls/Button.h"
#include "vgui/ILocalize.h"
#include "ixboxsystem.h"
#include "iachievementmgr.h"
#include "filesystem.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/CheckButton.h"
#include "fmtstr.h"
#include "c_cs_playerresource.h"
#include "stat_card.h"
#include <vgui/IInput.h>

#include "../../../public/steam/steam_api.h"
#include "achievementmgr.h"
#include "../../../../public/vgui/IScheme.h"
#include "../vgui_controls/ScrollBar.h"
#include "achievements_cs.h"

extern CAchievementMgr g_AchievementMgrCS;

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

KeyValues *g_pPreloadedCSAchievementPageItemLayout = NULL;
KeyValues *g_pPreloadedCSAchievementPageGroupLayout = NULL;

// Shared helper functions so xbox and pc can share as much code as possible while coming from different bases.

//-----------------------------------------------------------------------------
// Purpose: Sets the parameter pIconPanel to display the specified achievement's icon file.
//-----------------------------------------------------------------------------
bool CSLoadAchievementIconForPage( vgui::ImagePanel* pIconPanel, CCSBaseAchievement* pAchievement, const char *pszExt /*= NULL*/ )
{
    char imagePath[_MAX_PATH];
    Q_strncpy( imagePath, "achievements\\", sizeof(imagePath) );
    Q_strncat( imagePath, pAchievement->GetName(), sizeof(imagePath), COPY_ALL_CHARACTERS );
    if ( pszExt )
    {
        Q_strncat( imagePath, pszExt, sizeof(imagePath), COPY_ALL_CHARACTERS );
    }
    Q_strncat( imagePath, ".vtf", sizeof(imagePath), COPY_ALL_CHARACTERS );

    char checkFile[_MAX_PATH];
    Q_snprintf( checkFile, sizeof(checkFile), "materials\\vgui\\%s", imagePath );
    if ( !g_pFullFileSystem->FileExists( checkFile ) )
    {
        Q_snprintf( imagePath, sizeof(imagePath), "hud\\icon_locked.vtf" );
    }

    pIconPanel->SetShouldScaleImage( true );
    pIconPanel->SetImage( imagePath );
    pIconPanel->SetVisible( true );

    return pIconPanel->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the parameter pIconPanel to display the specified achievement's icon file.
//-----------------------------------------------------------------------------
bool CSLoadIconForPage( vgui::ImagePanel* pIconPanel, const char* pFilename, const char *pszExt /*= NULL*/ )
{
    char imagePath[_MAX_PATH];
    Q_strncpy( imagePath, "achievements\\", sizeof(imagePath) );
    Q_strncat( imagePath, pFilename, sizeof(imagePath), COPY_ALL_CHARACTERS );
    if ( pszExt )
    {
        Q_strncat( imagePath, pszExt, sizeof(imagePath), COPY_ALL_CHARACTERS );
    }
    Q_strncat( imagePath, ".vtf", sizeof(imagePath), COPY_ALL_CHARACTERS );

    char checkFile[_MAX_PATH];
    Q_snprintf( checkFile, sizeof(checkFile), "materials\\vgui\\%s", imagePath );
    if ( !g_pFullFileSystem->FileExists( checkFile ) )
    {
        Q_snprintf( imagePath, sizeof(imagePath), "hud\\icon_locked.vtf" );
    }

    pIconPanel->SetShouldScaleImage( true );
    pIconPanel->SetImage( imagePath );
    pIconPanel->SetVisible( true );

    return pIconPanel->IsVisible();
}

//-----------------------------------------------------------------------------
// The bias is to ensure the percentage bar gets plenty orange before it reaches the text,
// as the white-on-grey is hard to read.
//-----------------------------------------------------------------------------
Color CSLerpColorsForPage ( Color cStart, Color cEnd, float flPercent )
{
    float r = (float)((float)(cStart.r()) + (float)(cEnd.r() - cStart.r()) * Bias( flPercent, 0.75 ) );
    float g = (float)((float)(cStart.g()) + (float)(cEnd.g() - cStart.g()) * Bias( flPercent, 0.75 ) );
    float b = (float)((float)(cStart.b()) + (float)(cEnd.b() - cStart.b()) * Bias( flPercent, 0.75 ) );
    float a = (float)((float)(cStart.a()) + (float)(cEnd.a() - cStart.a()) * Bias( flPercent, 0.75 ) );
    return Color( r, g, b, a );
}

//-----------------------------------------------------------------------------
// Purpose: Shares common percentage bar calculations/color settings between xbox and pc.
//			Not really intended for robustness or reuse across many panels.
// Input  : pFrame - assumed to have certain child panels (see below)
//			*pAchievement - source achievement to poll for progress. Non progress achievements will not show a percentage bar.
//-----------------------------------------------------------------------------
void CSUpdateProgressBarForPage( vgui::EditablePanel* pPanel, CCSBaseAchievement* pAchievement, Color clrProgressBar )
{
    ///*
    if ( pAchievement->GetGoal() > 1 )
    {
        bool bShowProgress = true;

        // if this achievement gets saved with game and we're not in a level and have not achieved it, then we do not have any state 
        // for this achievement, don't show progress
        if ( ( pAchievement->GetFlags() & ACH_SAVE_WITH_GAME ) && /*!GameUI().IsInLevel() &&*/ !pAchievement->IsAchieved() )
        {
            bShowProgress = false;
        }

        float flCompletion = 0.0f;

        // Once achieved, we can't rely on count. If they've completed the achievement just set to 100%.
        int iCount = pAchievement->GetCount();
        if ( pAchievement->IsAchieved() )
        {
            flCompletion = 1.0f;
            iCount = pAchievement->GetGoal();
        }
        else if ( bShowProgress )
        {
            flCompletion = ( ((float)pAchievement->GetCount()) / ((float)pAchievement->GetGoal()) );
            // In rare cases count can exceed goal and not be achieved (switch local storage on X360, take saved game from different user on PC).
            // These will self-correct with continued play, but if we're in that state don't show more than 100% achieved.
            flCompletion = MIN( flCompletion, 1.0 );
        }

        char szPercentageText[ 256 ] = "";
        if  ( bShowProgress )
        {
            Q_snprintf( szPercentageText, 256, "%d/%d", iCount, pAchievement->GetGoal() );			
        }	

        pPanel->SetControlString( "PercentageText", szPercentageText );
        pPanel->SetControlVisible( "PercentageText", true );
        pPanel->SetControlVisible( "CompletionText", true );

        vgui::ImagePanel *pPercentageBar	= (vgui::ImagePanel*)pPanel->FindChildByName( "PercentageBar" );
        vgui::ImagePanel *pPercentageBarBkg = (vgui::ImagePanel*)pPanel->FindChildByName( "PercentageBarBackground" );

        if ( pPercentageBar && pPercentageBarBkg )
        {
            pPercentageBar->SetFillColor( clrProgressBar );
            pPercentageBar->SetWide( pPercentageBarBkg->GetWide() * flCompletion );

            pPanel->SetControlVisible( "PercentageBarBackground", IsX360() ? bShowProgress : true );
            pPanel->SetControlVisible( "PercentageBar", true );
        }
    }
    //*/
}

// TODO: revisit this once other games are rebuilt using the updated IAchievement interface
bool CSGameSupportsAchievementTrackerForPage()
{
    const char *pGame = Q_UnqualifiedFileName( engine->GetGameDirectory() );
    return ( !Q_stricmp( pGame, "cstrike" ) );
}

//////////////////////////////////////////////////////////////////////////
// PC Implementation
//////////////////////////////////////////////////////////////////////////



int AchivementSortPredicate( CCSBaseAchievement* const* pLeft, CCSBaseAchievement* const* pRight )
{
	if ( (*pLeft)->IsAchieved() && !(*pRight)->IsAchieved() )
		return -1;

	if ( !(*pLeft)->IsAchieved() && (*pRight)->IsAchieved() )
		return 1;

	if ( (*pLeft)->GetAchievementID() < (*pRight)->GetAchievementID() )
		return -1;

	if ( (*pLeft)->GetAchievementID() > (*pRight)->GetAchievementID() )
		return 1;

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: creates child panels, passes down name to pick up any settings from res files.
//-----------------------------------------------------------------------------
CAchievementsPage::CAchievementsPage(vgui::Panel *parent, const char *name) : BaseClass(parent, "CSAchievementsDialog")
{
	m_iFixedWidth = 900; // Give this an initial value in order to set a proper size
    SetBounds(0, 0, 900, 780);
    SetMinimumSize( 256, 780 );

	m_pStatCard = new StatCard(this, "ignored");

    m_pAchievementsList = new vgui::PanelListPanel( this, "listpanel_achievements" );
    m_pAchievementsList->SetFirstColumnWidth( 0 );

    m_pGroupsList = new vgui::PanelListPanel( this, "listpanel_groups" );
    m_pGroupsList->SetFirstColumnWidth( 0 );

    m_pListBG = new vgui::ImagePanel( this, "listpanel_background" );

    m_pPercentageBarBackground = SETUP_PANEL( new ImagePanel( this, "PercentageBarBackground" ) );
    m_pPercentageBar = SETUP_PANEL( new ImagePanel( this, "PercentageBar" ) );    

	ListenForGameEvent( "player_stats_updated" );
	ListenForGameEvent( "achievement_earned_local" );

    // int that holds the highest number achievement id we've found
    int iHighestAchievementIDSeen = -1;
    int iNextGroupBoundary = 1000;

    Q_memset( m_AchievementGroups, 0, sizeof(m_AchievementGroups) );
    m_iNumAchievementGroups = 0;

    // Base groups
    int iCount = g_AchievementMgrCS.GetAchievementCount();
    for ( int i = 0; i < iCount; ++i )
    {
        CCSBaseAchievement* pAchievement = dynamic_cast<CCSBaseAchievement*>(g_AchievementMgrCS.GetAchievementByIndex( i ));

        if ( !pAchievement )
            continue;

        int iAchievementID = pAchievement->GetAchievementID();

        if ( iAchievementID > iHighestAchievementIDSeen )
        {
            // if it's crossed the next group boundary, create a new group
            if ( iAchievementID >= iNextGroupBoundary )
            {
                int iNewGroupBoundary = iAchievementID;
                CreateNewAchievementGroup( iNewGroupBoundary, iNewGroupBoundary+99 );

                iNextGroupBoundary = iNewGroupBoundary + 100;
            }

            iHighestAchievementIDSeen = iAchievementID;
        }
    }

	LoadControlSettings("resource/ui/CSAchievementsDialog.res");
	UpdateTotalProgressDisplay();
    CreateOrUpdateComboItems( true );

    // Default display shows the first achievement group
    UpdateAchievementList(1001, 1100);

	m_bStatsDirty = true;
	m_bAchievementsDirty = true;
}

CAchievementsPage::~CAchievementsPage()
{
    g_AchievementMgrCS.SaveGlobalStateIfDirty( false );		// check for saving here to store achievements we want pinned to HUD

    m_pAchievementsList->DeleteAllItems();
    delete m_pAchievementsList;
    delete m_pPercentageBarBackground;
    delete m_pPercentageBar;
}

void CAchievementsPage::CreateNewAchievementGroup( int iMinRange, int iMaxRange )
{
    m_AchievementGroups[m_iNumAchievementGroups].m_iMinRange = iMinRange;
    m_AchievementGroups[m_iNumAchievementGroups].m_iMaxRange = iMaxRange;
    m_iNumAchievementGroups++;
}

//----------------------------------------------------------
// Get the width we're going to lock at
//----------------------------------------------------------
void CAchievementsPage::ApplySettings( KeyValues *pResourceData )
{
    m_iFixedWidth = pResourceData->GetInt( "wide", 512 );

    BaseClass::ApplySettings( pResourceData );
}

//----------------------------------------------------------
// Preserve our width to the one in the .res file
//----------------------------------------------------------
void CAchievementsPage::OnSizeChanged(int newWide, int newTall)
{
    // Lock the width, but allow height scaling
    if ( newWide != m_iFixedWidth )
    {
        SetSize( m_iFixedWidth, newTall );
        return;
    }

    BaseClass::OnSizeChanged(newWide, newTall);
}

//----------------------------------------------------------
// Re-populate the achievement list with the selected group
//----------------------------------------------------------
void CAchievementsPage::UpdateAchievementList(CAchievementsPageGroupPanel* groupPanel)
{
    if (!groupPanel)
        return;

    UpdateAchievementList( groupPanel->GetFirstAchievementID(), groupPanel->GetLastAchievementID() );

    vgui::IScheme *pGroupScheme = scheme()->GetIScheme( GetScheme() );

    // Update active status for button display
    for (int i = 0; i < m_pGroupsList->GetItemCount(); i++)
    {
        CAchievementsPageGroupPanel *pPanel = (CAchievementsPageGroupPanel*)m_pGroupsList->GetItemPanel(i);
        if ( pPanel )
        {
            if ( pPanel != groupPanel )
            {
                pPanel->SetGroupActive( false );
            }
            else
            {
                pPanel->SetGroupActive( true );
            }

            pPanel->UpdateAchievementInfo( pGroupScheme );
        }
    }
}

void CAchievementsPage::UpdateTotalProgressDisplay()
{
	// Set up total completion percentage bar
	float flCompletion = 0.0f;

	int iCount = g_AchievementMgrCS.GetAchievementCount();
	int nUnlocked = 0;

	if ( iCount > 0 )
	{
		for ( int i = 0; i < iCount; ++i )
		{
			CCSBaseAchievement* pAchievement = dynamic_cast<CCSBaseAchievement*>(g_AchievementMgrCS.GetAchievementByIndex( i ));

			if ( pAchievement && pAchievement->IsAchieved() )
				++nUnlocked;
		}

		flCompletion = (((float)nUnlocked) / ((float)g_AchievementMgrCS.GetAchievementCount()));
	}

	char szPercentageText[64];
	V_sprintf_safe( szPercentageText, "%d / %d",
		nUnlocked, g_AchievementMgrCS.GetAchievementCount() );

	SetControlString( "PercentageText", szPercentageText );
	SetControlVisible( "PercentageText", true );
	SetControlVisible( "CompletionText", true );

	vgui::IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

	Color clrHighlight = pScheme->GetColor( "NewGame.SelectionColor", Color(255, 255, 255, 255) );
	Color clrWhite(255, 255, 255, 255);

	Color cProgressBar = Color( static_cast<float>( clrHighlight.r() ) * ( 1.0f - flCompletion ) + static_cast<float>( clrWhite.r() ) * flCompletion,
		static_cast<float>( clrHighlight.g() ) * ( 1.0f - flCompletion ) + static_cast<float>( clrWhite.g() ) * flCompletion,
		static_cast<float>( clrHighlight.b() ) * ( 1.0f - flCompletion ) + static_cast<float>( clrWhite.b() ) * flCompletion,
		static_cast<float>( clrHighlight.a() ) * ( 1.0f - flCompletion ) + static_cast<float>( clrWhite.a() ) * flCompletion );

	m_pPercentageBar->SetFgColor( cProgressBar );
	m_pPercentageBar->SetWide( m_pPercentageBarBackground->GetWide() * flCompletion );

	SetControlVisible( "PercentageBarBackground", true );
	SetControlVisible( "PercentageBar", true );
}

//----------------------------------------------------------
// Re-populate the achievement list with the selected group
//----------------------------------------------------------
void CAchievementsPage::UpdateAchievementList(int minID, int maxID)
{
    int iMinRange = minID;
    int iMaxRange = maxID;

	int iCount = g_AchievementMgrCS.GetAchievementCount();

	CUtlVector<CCSBaseAchievement*> sortedAchivementList;
	sortedAchivementList.EnsureCapacity(iCount);

    for ( int i = 0; i < iCount; ++i )
    {		
        CCSBaseAchievement* pAchievement = dynamic_cast<CCSBaseAchievement*>(g_AchievementMgrCS.GetAchievementByIndex(i));

        if ( !pAchievement )
            continue;

        int iAchievementID = pAchievement->GetAchievementID();

        if ( iAchievementID < iMinRange || iAchievementID > iMaxRange )
            continue;

        // don't show hidden achievements if not achieved
        if ( pAchievement->ShouldHideUntilAchieved() && !pAchievement->IsAchieved() )
            continue;

		sortedAchivementList.AddToTail(pAchievement);
	}

	sortedAchivementList.Sort(AchivementSortPredicate);

	m_pAchievementsList->DeleteAllItems();

	FOR_EACH_VEC(sortedAchivementList, i)
	{
		CCSBaseAchievement* pAchievement = sortedAchivementList[i];

		CAchievementsPageItemPanel *pAchievementItemPanel = new CAchievementsPageItemPanel( m_pAchievementsList, "AchievementDialogItemPanel");
		pAchievementItemPanel->SetAchievementInfo(pAchievement);

		// force all our new panel to have the correct internal layout and size so that our parent container can layout properly
		pAchievementItemPanel->InvalidateLayout(true, true);

		m_pAchievementsList->AddItem( NULL, pAchievementItemPanel );
	}

	m_pAchievementsList->MoveScrollBarToTop();
}

//-----------------------------------------------------------------------------
// Purpose: Loads settings from achievementsdialog.res in hl2/resource/ui/
//			Sets up progress bar displaying total achievement unlocking progress by the user.
//-----------------------------------------------------------------------------
void CAchievementsPage::ApplySchemeSettings( vgui::IScheme *pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

	m_pGroupsList->SetBgColor(Color(86,86,86,255));

	SetBgColor(Color(86,86,86,255));

	// Set text color for percentage
	Panel *pPanel;
	pPanel = FindChildByName("PercentageText");
	if (pPanel)
	{
		pPanel->SetFgColor(Color(157, 194, 80, 255));
	}

	// Set text color for achievement earned label
	pPanel = FindChildByName("AchievementsEarnedLabel");
	if (pPanel)
	{
		pPanel->SetFgColor(Color(157, 194, 80, 255));
	}
}

//-----------------------------------------------------------------------------
// Purpose: Each sub-panel gets its data updated
//-----------------------------------------------------------------------------
void CAchievementsPage::UpdateAchievementDialogInfo( void )
{
    // Hide the group list scrollbar
    if (m_pGroupsList->GetScrollbar())
    {
        m_pGroupsList->GetScrollbar()->SetWide(0);
    }

    int iCount = m_pAchievementsList->GetItemCount();
    vgui::IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

    int i;
    for ( i = 0; i < iCount; i++ )
    {
        CAchievementsPageItemPanel *pPanel = (CAchievementsPageItemPanel*)m_pAchievementsList->GetItemPanel(i);
        if ( pPanel )
        {
            pPanel->UpdateAchievementInfo( pScheme );
        }
    }

    // Update all group panels
    int iGroupCount = m_pGroupsList->GetItemCount();
    for ( i = 0; i < iGroupCount; i++ )
    {
        CAchievementsPageGroupPanel *pPanel = (CAchievementsPageGroupPanel*)m_pGroupsList->GetItemPanel(i);
        if ( pPanel )
        {
            pPanel->UpdateAchievementInfo( pScheme );

            if ( pPanel->IsGroupActive() )
            {
                UpdateAchievementList( pPanel );
            }
        }
    }

    // update the groups and overall progress bar
    CreateOrUpdateComboItems( false );	// update them with new achieved counts	

	UpdateTotalProgressDisplay();

	m_pStatCard->UpdateInfo();

	m_bAchievementsDirty = false;
	m_bStatsDirty = false;
}

void CAchievementsPage::CreateOrUpdateComboItems( bool bCreate )
{
    // Build up achievement group names
    for ( int i=0;i<m_iNumAchievementGroups;i++ )
    {
        char buf[128];

		int achievementRangeStart = (m_AchievementGroups[i].m_iMinRange / 1000) * 1000;
        Q_snprintf( buf, sizeof(buf), "#Achievement_Group_%d", achievementRangeStart );

        wchar_t *wzGroupName = g_pVGuiLocalize->Find( buf );

        if ( !wzGroupName )
        {
            wzGroupName = L"Need Title ( %s1 of %s2 )";
        }

        wchar_t wzGroupTitle[128];

        if ( wzGroupName )
        {
            // Determine number of achievements in the group which have been awarded
            int numAwarded = 0;
            int numTested = 0;
            for (int j = m_AchievementGroups[i].m_iMinRange; j < m_AchievementGroups[i].m_iMaxRange; j++)
            {
                IAchievement* pCur = g_AchievementMgrCS.GetAchievementByID( j );

                if ( !pCur )
                    continue;

                numTested++;

                if ( pCur->IsAchieved() )
                {
                    numAwarded++;
                }
            }

            wchar_t wzNumUnlocked[8];
            V_snwprintf( wzNumUnlocked, ARRAYSIZE( wzNumUnlocked ), L"%d", numAwarded );

            wchar_t wzNumAchievements[8];
            V_snwprintf( wzNumAchievements, ARRAYSIZE( wzNumAchievements ), L"%d", numTested );

            g_pVGuiLocalize->ConstructString( wzGroupTitle, sizeof( wzGroupTitle ), wzGroupName, 0 );
        }

        KeyValues *pKV = new KeyValues( "grp" );
        pKV->SetInt( "minrange", m_AchievementGroups[i].m_iMinRange );
        pKV->SetInt( "maxrange", m_AchievementGroups[i].m_iMaxRange );

        if ( bCreate )
        {
            // Create an achievement group instance
            CAchievementsPageGroupPanel *achievementGroupPanel = new CAchievementsPageGroupPanel( m_pGroupsList, this, "AchievementDialogGroupPanel", i );
            achievementGroupPanel->SetGroupInfo( wzGroupTitle, m_AchievementGroups[i].m_iMinRange, m_AchievementGroups[i].m_iMaxRange );

            if (i == 0)
            {
                achievementGroupPanel->SetGroupActive(true);
            }
            else
            {
                achievementGroupPanel->SetGroupActive(false);
            }

            m_pGroupsList->AddItem( NULL, achievementGroupPanel );
        }
    }

	m_pStatCard->UpdateInfo();
}

//-----------------------------------------------------------------------------
// Purpose: creates child panels, passes down name to pick up any settings from res files.
//-----------------------------------------------------------------------------
CAchievementsPageItemPanel::CAchievementsPageItemPanel( vgui::PanelListPanel *parent, const char* name) : BaseClass( parent, name )
{    
    m_pParent = parent;
    m_pSchemeSettings = NULL;

    m_pAchievementIcon = SETUP_PANEL(new vgui::ImagePanel( this, "AchievementIcon" ));
    m_pAchievementNameLabel = new vgui::Label( this, "AchievementName", "name" );
    m_pAchievementDescLabel = new vgui::Label( this, "AchievementDesc", "desc" );
    m_pPercentageBar = SETUP_PANEL( new ImagePanel( this, "PercentageBar" ) );
    m_pPercentageText = new vgui::Label( this, "PercentageText", "" );
    m_pAwardDate = new vgui::Label( this, "AwardDate", "date" );
    m_pShowOnHUDButton = new vgui::CheckButton( this, "ShowOnHudToggle", "" );
    m_pShowOnHUDButton->SetMouseInputEnabled( true );
    m_pShowOnHUDButton->SetEnabled( true );
    m_pShowOnHUDButton->SetCheckButtonCheckable( true );
    m_pShowOnHUDButton->AddActionSignalTarget( this );

    m_pHiddenHUDToggleButton = new CHiddenHUDToggleButton( this, "HiddenHUDToggle", "" );
    m_pHiddenHUDToggleButton->SetPaintBorderEnabled( false );


    SetMouseInputEnabled( true );
    parent->SetMouseInputEnabled( true );
}

CAchievementsPageItemPanel::~CAchievementsPageItemPanel()
{
    delete m_pAchievementIcon;
    delete m_pAchievementNameLabel;
    delete m_pAchievementDescLabel;
    delete m_pPercentageBar;
    delete m_pPercentageText;
    delete m_pAwardDate;
    delete m_pShowOnHUDButton;
    delete m_pHiddenHUDToggleButton;
}

void CAchievementsPageItemPanel::ToggleShowOnHUDButton()
{
    if (m_pShowOnHUDButton)
    {
        m_pShowOnHUDButton->SetSelected( !m_pShowOnHUDButton->IsSelected() );
    }
}

//-----------------------------------------------------------------------------
// Purpose: Updates displayed achievement data. In applyschemesettings, and when gameui activates.
//-----------------------------------------------------------------------------
void CAchievementsPageItemPanel::UpdateAchievementInfo( vgui::IScheme* pScheme )
{
    if ( m_pSourceAchievement && m_pSchemeSettings )
    {
        //=============================================================================
        // HPE_BEGIN:
        // [dwenger] Get achievement name and description text from the localized file
        //=============================================================================

        // Set name, description and unlocked state text
        m_pAchievementNameLabel->SetText( ACHIEVEMENT_LOCALIZED_NAME( m_pSourceAchievement ) );
        m_pAchievementDescLabel->SetText( ACHIEVEMENT_LOCALIZED_DESC( m_pSourceAchievement ) );

        //=============================================================================
        // HPE_END
        //=============================================================================

        // Setup icon
        // get the vtfFilename from the path.

        // Display percentage completion for progressive achievements
        // Set up total completion percentage bar. Goal > 1 means its a progress achievement.
        CSUpdateProgressBarForPage( this, m_pSourceAchievement, m_clrProgressBar );

        if ( m_pSourceAchievement->IsAchieved() )
        {
            CSLoadAchievementIconForPage( m_pAchievementIcon, m_pSourceAchievement );

            SetBgColor( pScheme->GetColor( "AchievementsLightGrey", Color(255, 0, 0, 255) ) );

            m_pAchievementNameLabel->SetFgColor( pScheme->GetColor( "SteamLightGreen", Color(255, 255, 255, 255) ) );

            Color fgColor = pScheme->GetColor( "Label.TextBrightColor", Color(255, 255, 255, 255) );
            m_pAchievementDescLabel->SetFgColor( fgColor );
            m_pPercentageText->SetFgColor( fgColor );
            m_pShowOnHUDButton->SetVisible( false );
            m_pShowOnHUDButton->SetSelected( false );
            m_pHiddenHUDToggleButton->SetVisible( false );
            m_pAwardDate->SetVisible( true );
            m_pAwardDate->SetFgColor( pScheme->GetColor( "SteamLightGreen", Color(255, 255, 255, 255) ) );

            // Assign the award date text
            int year, month, day, hour, minute, second;
            if ( m_pSourceAchievement->GetAwardTime(year, month, day, hour, minute, second) )
			{
				char dateBuffer[32] = "";
				Q_snprintf( dateBuffer, 32, "%4d-%02d-%02d", year, month, day );
				m_pAwardDate->SetText( dateBuffer );
			}
			else
				m_pAwardDate->SetText( "" );
        }
        else
        {
            CSLoadAchievementIconForPage( m_pAchievementIcon, m_pSourceAchievement, "_bw" );

            SetBgColor( pScheme->GetColor( "AchievementsDarkGrey", Color(255, 0, 0, 255) ) );

            Color fgColor = pScheme->GetColor( "AchievementsInactiveFG", Color(255, 255, 255, 255) );
            m_pAchievementNameLabel->SetFgColor( fgColor );
            m_pAchievementDescLabel->SetFgColor( fgColor );
            m_pPercentageText->SetFgColor( fgColor );

            if ( CSGameSupportsAchievementTrackerForPage() )
            {
                m_pShowOnHUDButton->SetVisible( !m_pSourceAchievement->ShouldHideUntilAchieved() );
                m_pShowOnHUDButton->SetSelected( m_pSourceAchievement->ShouldShowOnHUD() );

                m_pHiddenHUDToggleButton->SetVisible( !m_pSourceAchievement->ShouldHideUntilAchieved() );
            }
            else
            {
                m_pShowOnHUDButton->SetVisible( false );
                m_pHiddenHUDToggleButton->SetVisible( false );
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Makes a local copy of a pointer to the achievement entity stored on the client.
//-----------------------------------------------------------------------------
void CAchievementsPageItemPanel::SetAchievementInfo( CCSBaseAchievement* pAchievement )
{
    if ( !pAchievement )
    {
        Assert( 0 );
        return;
    }

    m_pSourceAchievement = pAchievement;
    m_iSourceAchievementIndex = pAchievement->GetAchievementID();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAchievementsPageItemPanel::PreloadResourceFile( void )
{
    const char *controlResourceName = "resource/ui/AchievementItem.res";

    g_pPreloadedCSAchievementPageItemLayout = new KeyValues(controlResourceName);
    g_pPreloadedCSAchievementPageItemLayout->LoadFromFile(g_pFullFileSystem, controlResourceName);

/*
	// precache all achievement icons
	int iCount = g_AchievementMgrCS.GetAchievementCount();
	for ( int i = 0; i < iCount; ++i )
	{
		CCSBaseAchievement* pAchievement = dynamic_cast<CCSBaseAchievement*>(g_AchievementMgrCS.GetAchievementByIndex( i ));
		char imagePath[_MAX_PATH];

		Q_strncpy( imagePath, "achievements\\", sizeof(imagePath) );
		Q_strncat( imagePath, pAchievement->GetName(), sizeof(imagePath), COPY_ALL_CHARACTERS );
		Q_strncat( imagePath, "_bw", sizeof(imagePath), COPY_ALL_CHARACTERS );
		Q_strncat( imagePath, ".vtf", sizeof(imagePath), COPY_ALL_CHARACTERS );

		scheme()->GetImage(imagePath, true);

		Q_strncpy( imagePath, "achievements\\", sizeof(imagePath) );
		Q_strncat( imagePath, pAchievement->GetName(), sizeof(imagePath), COPY_ALL_CHARACTERS );
		Q_strncat( imagePath, ".vtf", sizeof(imagePath), COPY_ALL_CHARACTERS );

		scheme()->GetImage(imagePath, true);
	}
*/
}

//-----------------------------------------------------------------------------
// Purpose: Loads settings from hl2/resource/ui/achievementitem.res
//			Sets display info for this achievement item.
//-----------------------------------------------------------------------------
void CAchievementsPageItemPanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    if ( !g_pPreloadedCSAchievementPageItemLayout )
    {
        PreloadResourceFile();
    }

    LoadControlSettings( "", NULL, g_pPreloadedCSAchievementPageItemLayout );

    m_pSchemeSettings = pScheme;

    if ( !m_pSourceAchievement )
    {
        return;
    }

    BaseClass::ApplySchemeSettings( pScheme );

    // m_pSchemeSettings must be set for this.
    UpdateAchievementInfo( pScheme );
}

void CAchievementsPageItemPanel::OnCheckButtonChecked(Panel *panel)
{
    if ( CSGameSupportsAchievementTrackerForPage() && panel == m_pShowOnHUDButton && m_pSourceAchievement )
    {
        m_pSourceAchievement->SetShowOnHUD( m_pShowOnHUDButton->IsSelected() );
    }
}


//-----------------------------------------------------------------------------
// Purpose: creates child panels, passes down name to pick up any settings from res files.
//-----------------------------------------------------------------------------
CAchievementsPageGroupPanel::CAchievementsPageGroupPanel( vgui::PanelListPanel *parent, CAchievementsPage *owner, const char* name, int iListItemID ) : BaseClass( parent, name )
{
    m_pParent = parent;
    m_pOwner = owner;
    m_pSchemeSettings = NULL;

    m_pGroupIcon = SETUP_PANEL(new vgui::ImagePanel( this, "GroupIcon" ));
    m_pAchievementGroupLabel = new vgui::Label( this, "GroupName", "name" );
    m_pPercentageText = new vgui::Label( this, "GroupPercentageText", "1/1" );
    m_pPercentageBar = SETUP_PANEL( new ImagePanel( this, "GroupPercentageBar" ) );
    m_pGroupButton = new CGroupButton( this, "GroupButton", "" );
    m_pGroupButton->SetPos( 0, 0 );
    m_pGroupButton->SetZPos( 20 );
    m_pGroupButton->SetWide( 256 );
    m_pGroupButton->SetTall( 64 );
    SetMouseInputEnabled( true );
    parent->SetMouseInputEnabled( true );

    m_bActiveButton = false;
}

CAchievementsPageGroupPanel::~CAchievementsPageGroupPanel()
{
    delete m_pAchievementGroupLabel;
    delete m_pPercentageBar;
    delete m_pPercentageText;
    delete m_pGroupIcon;
}

//-----------------------------------------------------------------------------
// Purpose: Loads settings from hl2/resource/ui/achievementitem.res
//			Sets display info for this achievement item.
//-----------------------------------------------------------------------------
void CAchievementsPageGroupPanel::ApplySchemeSettings( vgui::IScheme* pScheme )
{
    if ( !g_pPreloadedCSAchievementPageGroupLayout )
    {
        PreloadResourceFile();
    }

    LoadControlSettings( "", NULL, g_pPreloadedCSAchievementPageGroupLayout );

    m_pSchemeSettings = pScheme;

    BaseClass::ApplySchemeSettings( pScheme );

    // m_pSchemeSettings must be set for this.
    UpdateAchievementInfo( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: Updates displayed achievement data. In ApplySchemeSettings(), and
//          when gameui activates.
//-----------------------------------------------------------------------------
void CAchievementsPageGroupPanel::UpdateAchievementInfo( vgui::IScheme* pScheme )
{
    if ( m_pSchemeSettings )
    {
        int numAwarded = 0;
        int numTested = 0;

        char buf[128];
        int achievementRangeStart = (m_iFirstAchievementID / 1000) * 1000;
        Q_snprintf( buf, sizeof(buf), "#Achievement_Group_%d", achievementRangeStart );

        wchar_t *wzGroupName = g_pVGuiLocalize->Find( buf );

        if ( !wzGroupName )
        {
            wzGroupName = L"Need Title ( %s1 of %s2 )";
        }

        wchar_t wzGroupTitle[128];

        if ( wzGroupName )
        {
            // Determine number of achievements in the group which have been awarded
            for (int i = m_iFirstAchievementID; i < m_iLastAchievementID; i++)
            {
                IAchievement* pCur = g_AchievementMgrCS.GetAchievementByID( i );

                if ( !pCur )
                    continue;

                numTested++;

                if ( pCur->IsAchieved() )
                {
                    numAwarded++;
                }
            }

            wchar_t wzNumUnlocked[8];
            V_snwprintf( wzNumUnlocked, ARRAYSIZE( wzNumUnlocked ), L"%d", numAwarded );

            wchar_t wzNumAchievements[8];
            V_snwprintf( wzNumAchievements,ARRAYSIZE( wzNumAchievements ), L"%d", numTested );

            g_pVGuiLocalize->ConstructString( wzGroupTitle, sizeof( wzGroupTitle ), wzGroupName, 2, wzNumUnlocked, wzNumAchievements );
        }

        // Set group name text
        m_pAchievementGroupLabel->SetText( wzGroupTitle );//m_pGroupName );
        m_pAchievementGroupLabel->SetFgColor(Color(157, 194, 80, 255));

        char*   buff[32];
        Q_snprintf( (char*)buff, 32, "%d / %d", numAwarded, numTested );
        m_pPercentageText->SetText( (const char*)buff );
        m_pPercentageText->SetFgColor(Color(157, 194, 80, 255));

        if ( !m_bActiveButton )
        {
            CSLoadIconForPage( m_pGroupIcon, "achievement-btn-up" );
        }
        else
        {
            CSLoadIconForPage( m_pGroupIcon, "achievement-btn-select" );
        }

        // Update the percentage complete bar
        vgui::ImagePanel *pPercentageBar	= (vgui::ImagePanel*)FindChildByName( "GroupPercentageBar" );
        vgui::ImagePanel *pPercentageBarBkg = (vgui::ImagePanel*)FindChildByName( "GroupPercentageBarBackground" );

        if ( pPercentageBar && pPercentageBarBkg )
        {
            float flCompletion = (float)numAwarded / (float)numTested;
            pPercentageBar->SetFillColor( Color(157, 194, 80, 255) );
            pPercentageBar->SetWide( pPercentageBarBkg->GetWide() * flCompletion );

            SetControlVisible( "GroupPercentageBarBackground", true );
            SetControlVisible( "GroupPercentageBar", true );
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAchievementsPageGroupPanel::PreloadResourceFile( void )
{
    const char *controlResourceName = "resource/ui/AchievementGroup.res";

    g_pPreloadedCSAchievementPageGroupLayout = new KeyValues(controlResourceName);
    g_pPreloadedCSAchievementPageGroupLayout->LoadFromFile(g_pFullFileSystem, controlResourceName);


}

//-----------------------------------------------------------------------------
// Purpose: Assigns a name and achievement id bounds for an achievement group.
//-----------------------------------------------------------------------------
void CAchievementsPageGroupPanel::SetGroupInfo( const wchar_t* name, int firstAchievementID, int lastAchievementID )
{
    // Store away the group name
    short   _textLen = (short)wcslen(name) + 1;
    m_pGroupName = new wchar_t[_textLen];
    Q_memcpy( m_pGroupName, name, _textLen * sizeof(wchar_t) );

    // Store off the start & end achievement IDs
    m_iFirstAchievementID = firstAchievementID;
    m_iLastAchievementID = lastAchievementID;
}

CGroupButton::CGroupButton( vgui::Panel *pParent, const char *pName, const char *pText ) : 
BaseClass( pParent, pName, pText )
{
}

//-----------------------------------------------------------------------------
// Purpose: Handle the case where the user presses an achievement group button.
//-----------------------------------------------------------------------------
void CGroupButton::DoClick( void )
{
    // Process when a group button is hit
    CAchievementsPageGroupPanel*    pParent = static_cast<CAchievementsPageGroupPanel*>(GetParent());

    if (pParent)
    {
        CAchievementsPage*  pAchievementsPage = static_cast<CAchievementsPage*>(pParent->GetOwner());

        if (pAchievementsPage)
        {
            // Update the list of group achievements
            pAchievementsPage->UpdateAchievementList( pParent );
        }
    }
}

void CAchievementsPage::OnPageShow()
{
	m_pGroupsList->GetScrollbar()->SetWide(0);
}

void CAchievementsPage::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();

	if ( 0 == Q_strcmp( type, "achievement_earned_local" ) )
		m_bAchievementsDirty = true;

	if ( 0 == Q_strcmp( type, "player_stats_updated" ) )
		m_bStatsDirty = true;
}

void CAchievementsPage::OnThink()
{
	vgui::IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

	if ( m_bAchievementsDirty )
	{
		UpdateAchievementDialogInfo();
	}
	else if ( m_bStatsDirty )
	{
		// Update progress for currently displayed achievements
		int itemId = m_pAchievementsList->FirstItem();

		while (itemId != m_pAchievementsList->InvalidItemID() )
		{
			CAchievementsPageItemPanel *pAchievementItem = dynamic_cast<CAchievementsPageItemPanel*>(m_pAchievementsList->GetItemPanel(itemId));
			pAchievementItem->UpdateAchievementInfo(pScheme);

			itemId = m_pAchievementsList->NextItem(itemId);
		}
		m_bStatsDirty = false;
	}
}

CHiddenHUDToggleButton::CHiddenHUDToggleButton( vgui::Panel *pParent, const char *pName, const char *pText ) : 
BaseClass( pParent, pName, pText )
{
}

//-----------------------------------------------------------------------------
// Purpose: Handle the case where the user shift-clicks on an un-awarded achievement.
//-----------------------------------------------------------------------------
void CHiddenHUDToggleButton::DoClick( void )
{
    if ( input()->IsKeyDown(KEY_LSHIFT) || input()->IsKeyDown(KEY_RSHIFT) ) 
    {
        // Process when a group button is hit
        CAchievementsPageItemPanel*    pParent = static_cast<CAchievementsPageItemPanel*>(GetParent());

        if (pParent)
        {
            pParent->ToggleShowOnHUDButton();
        }
    }
}
