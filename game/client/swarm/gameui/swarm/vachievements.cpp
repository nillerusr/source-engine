//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "EngineInterface.h"
#include "VGenericPanelList.h"
#include "IAchievementMgr.h"
#include "KeyValues.h"
#include "VFooterPanel.h"
#include "fmtstr.h"

#include "vgui/IBorder.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Divider.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ProgressBar.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/TextImage.h"

#include "FileSystem.h"
#include "cdll_util.h"
#include "vgui/ISurface.h"
#include "VAchievements.h"
//#include "asw_achievements.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// TODO: this is a temporary merge workaround until achievment interfaces get merged properly
// REMOVE THIS BLOCK AFTER ACHIEVEMENTS GET MERGED
//#define IAchievementMgr IAchievementMgr_TempMerge_CrashCrash
//#define achievementmgr ( ( IAchievementMgr_TempMerge_CrashCrash * ) NULL )
// -- END TODO
//#define achievementmgr ASWAchievementManager()

using namespace vgui;
using namespace BaseModUI;

KeyValues *g_pPreloadedAchievementListItemLayout = NULL;

#pragma warning( disable : 4800 ) // warning C4800: 'uint64' : forcing value to bool 'true' or 'false' (performance warning)

AchievementListItem::AchievementListItem( IAchievement *pAchievement ) : BaseClass( NULL, "AchievementListItem" )
{
	SetProportional( true );

	m_LblName = new Label( this, "LblName", "" );
	m_LblProgress = new Label( this, "LblProgress", "#asw_achievement_progress" );
	m_DivTitleDivider = new Divider( this, "DivTitleDivider" );
	m_ImgAchievementIcon = new ImagePanel( this, "ImgAchievementIcon" );
	m_LblHowTo = new Label( this, "LblHowTo", "" );
	m_PrgProgress = new ContinuousProgressBar(this, "PrgProgress" );
	m_PrgProgress->SetImage( "progressbar", PROGRESS_TEXTURE_FG );
	m_PrgProgress->SetImage( "progressbar_bg", PROGRESS_TEXTURE_BG );
	m_LblCurrProgress = new Label( this, "LblCurrProgress", "0" );
	m_LblGamerscore = new Label( this, "LblGamerScore", "" );

	m_pAchievement = NULL;

	m_bShowingDetails = false;

	SetAchievement( pAchievement );
	SetAchievementName( ACHIEVEMENT_LOCALIZED_NAME( pAchievement ) );
	SetAchievementHowTo( ACHIEVEMENT_LOCALIZED_DESC( pAchievement ) );
	SetAchievementIcon( pAchievement->GetIconPath() );
	SetAchievementGoal( pAchievement->GetGoal() );
	SetGamerScore( pAchievement->GetPointValue() );
	SetAchievementProgress( pAchievement->GetGoal() );	
}

//=============================================================================
void AchievementListItem::SetAchievement( IAchievement *pAchievement )
{
	if ( !pAchievement )
		return;

	m_pAchievement = pAchievement;

	InvalidateLayout();
}

//=============================================================================
void AchievementListItem::SetAchievementName( const wchar_t* name )
{
	m_LblName->SetText(name);
}

//=============================================================================
void AchievementListItem::SetAchievementHowTo( const wchar_t* howTo )
{
	m_LblHowTo->SetText(howTo);
}

//=============================================================================
void AchievementListItem::SetAchievementIcon(const char* iconName)
{
	m_ImgAchievementIcon->SetShouldScaleImage(true);
	m_ImgAchievementIcon->SetImage( iconName ? iconName : "vgui/white" );
	m_ImgAchievementIcon->SetVisible(true);
}

//=============================================================================
void AchievementListItem::SetAchievementProgress(int progress)
{
	m_AchievementProgress = progress;

	float fProgress = static_cast<float>(progress) / static_cast<float>(m_AchievementGoal);

	char buffer[64];
	Q_snprintf(buffer, 63, "%d / %d", m_AchievementProgress, m_AchievementGoal);
	m_LblCurrProgress->SetText(buffer);

	m_PrgProgress->SetProgress(fProgress);

	if(fProgress < 1.0f)
	{
		fProgress *= 100.0f;

		char buffer[8];
		Q_snprintf(buffer, 7, "%2.0f%%", fProgress);
	}

	// For achievements that don't have multiple steps do not display progress bar or progress label
	m_PrgProgress->SetVisible( m_AchievementGoal > 1 );
	m_LblCurrProgress->SetVisible( m_AchievementGoal > 1 );

	InvalidateLayout();
}

//=============================================================================
void AchievementListItem::SetAchievementGoal( int goal )
{
	m_AchievementGoal = goal;

	// reset the achievement progress to refresh the labels
	SetAchievementProgress( m_AchievementProgress );
}

//=============================================================================
void AchievementListItem::SetGamerScore(int score)
{
	m_GamerScore = score;

	wchar_t buffer[10];
	wchar_t num[5];

	V_snwprintf( num, sizeof(num), L"%d", m_GamerScore );
	g_pVGuiLocalize->ConstructString( buffer, sizeof( buffer ), g_pVGuiLocalize->FindSafe( "#L4D360UI_Gamerscore_Progress" ), 1, num );

	m_LblGamerscore->SetText( buffer );
	m_LblGamerscore->SetVisible( IsX360() && (score>0) );
}

//=============================================================================
int AchievementListItem::GetGoal() const
{
	return m_AchievementGoal;
}

//=============================================================================
int AchievementListItem::GetProgress() const
{
	return m_AchievementProgress;
}

//=============================================================================
bool AchievementListItem::GetCompleted() const
{
	return m_AchievementGoal == m_AchievementProgress;
}

//=============================================================================
int AchievementListItem::GetGamerScore() const
{
	return m_GamerScore;
}

//=============================================================================
void AchievementListItem::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

#ifndef _X360
	if ( !m_pAchievement )
		return;

	if ( !g_pPreloadedAchievementListItemLayout )
	{
		const char *pszResource = "Resource/UI/BaseModUI/AchievementListItem.res";
		g_pPreloadedAchievementListItemLayout = new KeyValues( pszResource );
		g_pPreloadedAchievementListItemLayout->LoadFromFile(g_pFullFileSystem, pszResource);
	}

	LoadControlSettings( "", NULL, g_pPreloadedAchievementListItemLayout );

	m_iOriginalTall = GetTall();

	KeyValues *pListItem = g_pPreloadedAchievementListItemLayout->FindKey( "AchievementListItem" );
	if ( pListItem )
	{
		SetBgColor( pListItem->GetColor( "BackgroundColor" ) );
	}

	SetAchievementName( ACHIEVEMENT_LOCALIZED_NAME( m_pAchievement ) );
	SetAchievementHowTo( ACHIEVEMENT_LOCALIZED_DESC( m_pAchievement ) );
	SetAchievementIcon( m_pAchievement->GetIconPath() );
	SetAchievementGoal( m_pAchievement->GetGoal() );
	SetGamerScore( m_pAchievement->GetPointValue() );
	SetAchievementProgress( m_pAchievement->IsAchieved() ? m_pAchievement->GetGoal() : m_pAchievement->GetCount() );

	// TODO:
	/*
	int iNumComponents = m_pAchievement->GetNumComponents();
	if ( iNumComponents > 0 )
	{
		KeyValues *pLeftTemplate = g_pPreloadedAchievementListItemLayout->FindKey( "ComponentTemplateLeft", true );
		KeyValues *pRightTemplate = g_pPreloadedAchievementListItemLayout->FindKey( "ComponentTemplateRight", true );

		KeyValues *pLeftCheckmarkTemplate = g_pPreloadedAchievementListItemLayout->FindKey( "CheckmarkTemplateLeft", true );
		KeyValues *pRightCheckmarkTemplate = g_pPreloadedAchievementListItemLayout->FindKey( "CheckmarkTemplateRight", true );

		uint64 iComponentBits = m_pAchievement->GetComponentBits();

		int iNumRows = 0;

		for ( int i = 0, iNumComponents = m_pAchievement->GetNumComponents(); i < iNumComponents; i++ )
		{
			vgui::Label *pLabel = vgui::SETUP_PANEL( new vgui::Label( this, NULL, "" ) );
			if ( !pLabel )
				continue;

			pLabel->ApplySettings( (i%2) == 0 ? pLeftTemplate : pRightTemplate );
			pLabel->SetName( CFmtStr( "comp%d", i ) );
			pLabel->SetText( g_pVGuiLocalize->FindSafe( m_pAchievement->GetComponentDisplayString(i) ) );
			pLabel->SetVisible( false );	//start out hidden

			pLabel->SetEnabled( ( iComponentBits & ((uint64) 1 << i) ) );

			if ( iComponentBits & ((uint64) 1 << i) )
			{
				// show a checkmark next to this component
				vgui::ImagePanel *pCheckmark = vgui::SETUP_PANEL( new vgui::ImagePanel( this, NULL ) );
				if ( pCheckmark )
				{
					pCheckmark->ApplySettings( (i%2) == 0 ? pLeftCheckmarkTemplate : pRightCheckmarkTemplate );
					pCheckmark->SetName( CFmtStr( "check%d", i ) );
					pCheckmark->SetVisible( false );					
				}
			}

			if ( (i%2) == 0 )
			{
				iNumRows++;
			}
		}

		if ( iNumRows > 0 )
		{
			// we have some details, make ourselves tall enough to show the details button
			SetTall( m_iOriginalTall + m_flDetailsExtraHeight );
		}

		vgui::Button *pDetailsButton = dynamic_cast< vgui::Button * >( FindChildByName( "BtnDetails" ) );
		if ( pDetailsButton )
		{
			pDetailsButton->SetVisible( true );
		}
		vgui::Label *pDetailsLabel = dynamic_cast< vgui::Label * >( FindChildByName( "LblDetails" ) );
		if ( pDetailsLabel )
		{
			pDetailsLabel ->SetVisible( true );
			SetControlString( "LblDetails", "#GameUI_ShowDetails" );
		}
	}
	*/
#else	
	SetBgColor(pScheme->GetColor( "Button.BgColor", Color( 32, 32, 32, 255 ) ) );
#endif
}

//=============================================================================
void AchievementListItem::NavigateTo()
{
	BaseClass::NavigateTo();
}


//=============================================================================
void AchievementListItem::PerformLayout( void )
{
	BaseClass::PerformLayout();

#ifndef _X360
	// TODO:
	/*
	uint64 iComponentBits = m_pAchievement->GetComponentBits();

	for ( int i=0;i<m_pAchievement->GetNumComponents(); i++ )
	{
		char buf[120];
		Q_snprintf( buf, sizeof(buf), "comp%d", i );

		vgui::Label *pLabel = dynamic_cast< vgui::Label * > ( FindChildByName( buf ) );

		if ( !pLabel )
			continue;

		int iRow = i / 2;

		int x, y;
		pLabel->GetPos( x, y );

		y += iRow * pLabel->GetTall();

		pLabel->SetPos( x, y );

		if ( iComponentBits & ((uint64)1<<i) )
		{
			// move its associated checkmark next to it
			Q_snprintf( buf, sizeof(buf), "check%d", i );
			vgui::ImagePanel *pCheckmark = dynamic_cast< vgui::ImagePanel * > ( FindChildByName( buf ) );
			if ( pCheckmark )
			{
				int checkX, checkY;
				pCheckmark->GetPos( checkX, checkY );

				checkY += iRow * pLabel->GetTall();

				pCheckmark->SetPos( checkX, checkY );
			}
		}
	}
	*/
#endif
}

//=============================================================================
void AchievementListItem::OnCommand( const char *command )
{
#ifndef _X360
	if ( !Q_strcmp( command, "toggle_details" ) )
	{
		m_bShowingDetails = !m_bShowingDetails;

		// TODO:
		/*
		uint64 iComponentBits = m_pAchievement->GetComponentBits();

		int iNumRows = 0;

		for ( int i=0;i<m_pAchievement->GetNumComponents(); i++ )
		{
			char buf[120];
			Q_snprintf( buf, sizeof(buf), "comp%d", i );

			vgui::Label *pLabel = dynamic_cast< vgui::Label * > ( FindChildByName( buf ) );

			if ( !pLabel )
				continue;

			pLabel->SetVisible( m_bShowingDetails );

			if ( (i%2) == 0 )
			{
				iNumRows++;
			}

			if ( iComponentBits & ((uint64)1<<i) )
			{
				// move its associated checkmark next to it
				Q_snprintf( buf, sizeof(buf), "check%d", i );

				SetControlVisible( buf, m_bShowingDetails );
			}
		}

		if ( m_bShowingDetails )
		{
			SetTall( m_iOriginalTall + m_flDetailsExtraHeight + m_flDetailsRowHeight * iNumRows );

			SetControlString( "BtnDetails", "-" );
			SetControlString( "LblDetails", "#GameUI_HideDetails" );
		}
		else
		{
			SetTall( m_iOriginalTall + m_flDetailsExtraHeight );

			SetControlString( "BtnDetails", "+" );
			SetControlString( "LblDetails", "#GameUI_ShowDetails" );
		}
		*/

		InvalidateLayout();
	}
#endif
}

//=============================================================================
void AchievementListItem::OnSizeChanged( int newWide, int newTall )
{
	BaseClass::OnSizeChanged( newWide, newTall );

	PostActionSignal( new KeyValues( "ChildResized" ) );
}

//=============================================================================
void AchievementListItem::Paint(void)
{
	BaseClass::Paint();
}

//=============================================================================
AchievementListItemLabel::AchievementListItemLabel(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetProportional( true );

	m_LblCategory = new Label( this, "LblCategory", "#L4D360UI_Unattained" );

	LoadControlSettings("Resource/UI/BaseModUI/AchievementListItemLabel.res");
}

//=============================================================================
AchievementListItemLabel::~AchievementListItemLabel()
{
	delete m_LblCategory;
}

//=============================================================================
void AchievementListItemLabel::SetCategory( const wchar_t* category )
{
	m_LblCategory->SetText( category );
}

class BaseModUI::AchievementGenericPanelList : public GenericPanelList
{
	DECLARE_CLASS_SIMPLE( AchievementGenericPanelList, GenericPanelList );

public:
	AchievementGenericPanelList::AchievementGenericPanelList( vgui::Panel *parent, const char *panelName, ITEM_SELECTION_MODE selectionMode, int iControllingSlot ) :
	    BaseClass( parent, panelName, selectionMode ),
		m_iControllingUserSlot( iControllingSlot )
	{
	}

protected:
	void OnKeyCodePressed(KeyCode code)
	{
		if ( m_iControllingUserSlot != GetJoystickForCode( code ) )
			return;

		BaseClass::OnKeyCodePressed(code);
	}
	int m_iControllingUserSlot;
};

//=============================================================================
//
//=============================================================================
Achievements::Achievements(Panel *parent, const char *panelName):
BaseClass(parent, panelName, false, true)
{
	GameUI().PreventEngineHideGameUI();

	// Determine the slot and controller of the player who opened the dialog
	m_iStartingUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();

	memset( m_wAchievementsTitle, 0, sizeof( m_wAchievementsTitle ) );
	if ( IsX360() )
	{
		// Set the name of the dialog, adding the handle of the initiating user
		const wchar_t *pwcTemplate = g_pVGuiLocalize->Find("#L4D360UI_My_Achievements_User");
		int iActiveController = XBX_GetUserId( m_iStartingUserSlot );

		const char *pszPlayerName = BaseModUI::CUIGameData::Get()->GetLocalPlayerName( iActiveController );

		wchar_t wGamerTag[32];
		g_pVGuiLocalize->ConvertANSIToUnicode( pszPlayerName, wGamerTag, sizeof( wGamerTag ) );
		g_pVGuiLocalize->ConstructString( m_wAchievementsTitle, sizeof( m_wAchievementsTitle ), pwcTemplate, 1, wGamerTag );
	}

	SetDeleteSelfOnClose(true);
	SetProportional( true );

	m_LblComplete = new Label(this, "LblComplete", ""); 
	m_LblGamerscore = new Label(this, "LblGamerscore", ""); 
	m_GplAchievements = new AchievementGenericPanelList( this, "GplAchievements", GenericPanelList::ISM_ELEVATOR, m_iStartingUserSlot );
	m_GplAchievements->ShowScrollProgress( true );
	m_GplAchievements->SetScrollBarVisible( IsPC() );
	m_GplAchievements->SetBgColor( Color( 0, 0, 0, 0 ) );

	if ( IsX360() )
	{
		m_GplAwards = new AchievementGenericPanelList( this, "GplAwards", GenericPanelList::ISM_ELEVATOR, m_iStartingUserSlot );
		m_GplAwards->ShowScrollProgress( true );
		m_GplAwards->SetScrollBarVisible( false );
		m_GplAwards->SetBgColor( Color( 0, 0, 0, 0 ) );
	}
	else
	{
		m_GplAwards = NULL;
	}

	m_pProgressBar = new ContinuousProgressBar( this, "ProTotalProgress" );
	m_pProgressBar->SetImage( "progressbar", PROGRESS_TEXTURE_FG );
	m_pProgressBar->SetImage( "progressbar_bg", PROGRESS_TEXTURE_BG );

	SetUpperGarnishEnabled(true);
	SetLowerGarnishEnabled( true );
	SetOkButtonEnabled( false );

	m_ActiveControl = m_GplAchievements;

	LoadControlSettings("Resource/UI/BaseModUI/Achievements.res");

	UpdateFooter();

	m_bShowingAssets = false;
	m_iAwardCompleteCount = 0;
	m_iAchCompleteCount = 0;
}

//=============================================================================
Achievements::~Achievements()
{
	GameUI().AllowEngineHideGameUI();
}

//=============================================================================
void Achievements::Activate()
{
	BaseClass::Activate();

	// Populate the achievements list.
	m_GplAchievements->RemoveAllPanelItems();

	if ( !achievementmgr )
		return;

	m_iAwardCompleteCount = 0;
	m_iAchCompleteCount = 0;
	int incompleteCount= 0;
	int gamerScore = 0;

	//
	// Add the "Achieved" ones
	//
	for(int i = 0; i < achievementmgr->GetAchievementCount(); i++)
	{
		IAchievement* achievement = achievementmgr->GetAchievementByDisplayOrder( i, m_iStartingUserSlot );

		if ( achievement && achievement->IsAchieved() )
		{
			AchievementListItem *panelItem = new AchievementListItem( achievement );
			if ( panelItem )
			{
				m_GplAchievements->AddPanelItem( panelItem, true );
			}

			gamerScore += achievement->GetPointValue();
			++m_iAchCompleteCount;
		}
	}

	//
	// Add the "Unattained" ones
	//
	for(int i = 0; i < achievementmgr->GetAchievementCount(); i++)
	{
		IAchievement* achievement = achievementmgr->GetAchievementByDisplayOrder( i, m_iStartingUserSlot );

		if ( achievement && !achievement->IsAchieved() )
		{
			AchievementListItem *panelItem = new AchievementListItem( achievement );
			if ( panelItem )
			{
				m_GplAchievements->AddPanelItem( panelItem, true );
			}

			++incompleteCount;
		}
	} 

	// Populate the awards list.
	int awardIncompleteCount= 0;
	if ( IsX360() )
	{
		m_GplAwards->RemoveAllPanelItems();

		for ( int i = 0; i < achievementmgr->GetAchievementCount(); i++ )
		{
			IAchievement* achievement = achievementmgr->GetAchievementByIndex( i, m_iStartingUserSlot );
			if ( achievement && achievement->IsAchieved() )
			{
				AchievementListItem *panelItem = new AchievementListItem( achievement );
				if ( panelItem )
				{
					m_GplAchievements->AddPanelItem( panelItem, true );
				}

				gamerScore += achievement->GetPointValue();
				++m_iAwardCompleteCount;
			}
		}

		for( int i = 0; i < achievementmgr->GetAchievementCount(); i++ )
		{
			IAchievement* achievement = achievementmgr->GetAchievementByIndex( i, m_iStartingUserSlot );
			if ( achievement && !achievement->IsAchieved() )
			{
				AchievementListItem *panelItem = new AchievementListItem( achievement );
				if ( panelItem )
				{
					m_GplAchievements->AddPanelItem( panelItem, true );
				}

				++awardIncompleteCount;
			}
		} 
	}
	if ( m_GplAwards )
	{
		m_GplAwards->SetVisible( false );
	}

	//
	// Update achievement and gamerscore progress
	//
	wchar_t localizedGamerscoreProgress[128]; 
	char buffer[64];
	wchar_t wNumAchieved[64];
	wchar_t wTotalAchievements[64];
	wchar_t wGamerscore[64];

	// Construct achievement progress string
	itoa( achievementmgr->GetAchievementCount(), buffer, 10 );
	V_UTF8ToUnicode( buffer, wTotalAchievements, sizeof( wNumAchieved ) );
	itoa( m_iAchCompleteCount, buffer, 10 );
	V_UTF8ToUnicode( buffer, wNumAchieved, sizeof( wTotalAchievements ) );
	g_pVGuiLocalize->ConstructString( m_wAchievementsProgress, sizeof( m_wAchievementsProgress ), g_pVGuiLocalize->Find( "#L4D360UI_Achievement_Progress" ), 2, wNumAchieved, wTotalAchievements );
	m_LblComplete->SetText( m_wAchievementsProgress );

	// Construct gamerscore progress string
	itoa( gamerScore, buffer, 10 );
	V_UTF8ToUnicode( buffer, wGamerscore, sizeof( wGamerscore ) );
	g_pVGuiLocalize->ConstructString( localizedGamerscoreProgress, sizeof( localizedGamerscoreProgress ), g_pVGuiLocalize->Find( "#L4D360UI_Gamerscore_Progress" ), 2, wGamerscore );
	m_LblGamerscore->SetText( localizedGamerscoreProgress );

	// Focus on the first item in the list
	m_GplAchievements->NavigateTo();
	m_GplAchievements->SelectPanelItem( 0 );

	// Set the progress bar
	m_flTotalProgress = static_cast<float>(m_iAchCompleteCount) / static_cast<float>(achievementmgr->GetAchievementCount());

	UpdateFooter();

	ToggleDisplayType( m_bShowingAssets );
}

void Achievements::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_BBUTTON | FB_YBUTTON, FF_ACHIEVEMENTS, false );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Done" );
		footer->SetButtonText( FB_YBUTTON, m_bShowingAssets ? "#L4D360UI_ShowAchievements" : "#L4D360UI_ShowUnlockableAwards" );
	}
}

//=============================================================================
void Achievements::OnCommand(const char *command)
{
	if( V_strcmp( command, "Back" ) == 0 )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		m_bShowingAssets = false;
	}
	else
	{
		BaseClass::OnCommand( command );
	}	
}

//=============================================================================
void Achievements::OnKeyCodePressed(KeyCode code)
{
	if ( m_iStartingUserSlot != GetJoystickForCode( code ) )
		return;

//	int iUserSlot = GetJoystickForCode( code );
//	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_Y:
		ToggleDisplayType( !m_bShowingAssets );
		break;
	}

	BaseClass::OnKeyCodePressed(code);
}

//=============================================================================
void Achievements::ToggleDisplayType( bool bDisplayType )
{
	if ( IsPC() )
		return;

	m_bShowingAssets = bDisplayType;

	if ( m_bShowingAssets )
	{
		m_GplAwards->SetVisible( true );
		m_GplAchievements->SetVisible( false );
		m_LblGamerscore->SetVisible( false );

		m_GplAwards->NavigateTo();
		m_GplAwards->SelectPanelItem( 0 );

		m_ActiveControl = m_GplAwards;
	}
	else
	{
		m_GplAwards->SetVisible( false );
		m_GplAchievements->SetVisible( true );
		m_LblGamerscore->SetVisible( true );

		m_GplAchievements->NavigateTo();
		m_GplAchievements->SelectPanelItem( 0 );

		m_ActiveControl = m_GplAchievements;
	}

	m_ActiveControl->SetBgColor( Color( 0, 0, 0, 0 ) );

	char buffer[64];
	wchar_t wNumAchieved[64];
	wchar_t wTotalAchievements[64];

	itoa( achievementmgr->GetAchievementCount(), buffer, 10 );
	V_UTF8ToUnicode( buffer, wTotalAchievements, sizeof( wNumAchieved ) );
	itoa( m_bShowingAssets ? m_iAwardCompleteCount : m_iAchCompleteCount, buffer, 10 );
	V_UTF8ToUnicode( buffer, wNumAchieved, sizeof( wTotalAchievements ) );
	g_pVGuiLocalize->ConstructString( m_wAchievementsProgress, sizeof( m_wAchievementsProgress ), g_pVGuiLocalize->Find( "#L4D360UI_Achievement_Progress" ), 2, wNumAchieved, wTotalAchievements );
	m_LblComplete->SetText( m_wAchievementsProgress );
	m_flTotalProgress = static_cast<float>(m_bShowingAssets ? m_iAwardCompleteCount : m_iAchCompleteCount) / static_cast<float>(achievementmgr->GetAchievementCount());
	m_pProgressBar->SetProgress( m_flTotalProgress );

	UpdateFooter();
}

//=============================================================================
void Achievements::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled( true );
	SetupAsDialogStyle();

	m_pProgressBar->SetProgress( m_flTotalProgress );
	if ( IsX360() )
	{
		m_pProgressBar->SetVisible( false );
	}
}

#ifdef _X360
void Achievements::NavigateTo()
{
	BaseClass::NavigateTo();
	if ( m_bShowingAssets )
	{
		m_GplAwards->NavigateTo();
	}
	else
	{
		m_GplAchievements->NavigateTo();
	}
}

void Achievements::NavigateFrom()
{
	BaseClass::NavigateFrom();

}
#endif // _X360

//=============================================================================
void Achievements::PaintBackground()
{
	if ( IsPC() )
	{
		//BaseClass::DrawDialogBackground( "#L4D360UI_My_Achievements", NULL, "#L4D360UI_My_Achievements_Desc", NULL );

		int wide, tall;
		GetSize( wide, tall );

		vgui::surface()->DrawSetColor( Color( 0, 0, 0, 192 ) );
		vgui::surface()->DrawFilledRect( 0, 0, wide, tall * 0.5f );

		vgui::surface()->DrawSetColor( Color( 0, 0, 0, 250 ) );
		vgui::surface()->DrawFilledRect( 0, tall * 0.5f, wide, tall );

		int y = YRES( 75 );
		tall = YRES( 330 );
		int iHalfWide = wide * 0.5f;

		float flAlpha = 200.0f / 255.0f;

		// fill bar background
		vgui::surface()->DrawSetColor( Color( 0, 0, 0, 255 * flAlpha ) );
		vgui::surface()->DrawFilledRect( 0, y, wide, y + tall );

		vgui::surface()->DrawSetColor( Color( 53, 86, 117, 255 * flAlpha ) );
		//vgui::surface()->DrawFilledRect( 0, YRES( 4 ), wide, tall - YRES( 4 ) );

		int nBarPosY = y + YRES( 4 );
		int nBarHeight = tall - YRES( 8 );
		vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, wide, nBarPosY + nBarHeight, 255, 0, true );
		vgui::surface()->DrawFilledRectFade( 0, nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );
/*
		nBarPosY = y + tall - YRES( 2 );
		vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, wide, nBarPosY + nBarHeight, 255, 0, true );
		vgui::surface()->DrawFilledRectFade( 0, nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );
*/
		
		// draw highlights
		nBarHeight = YRES( 2 );
		nBarPosY = y;
		vgui::surface()->DrawSetColor( Color( 97, 210, 255, 255 * flAlpha ) );
		vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, wide, nBarPosY + nBarHeight, 255, 0, true );
		vgui::surface()->DrawFilledRectFade( 0, nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );

		nBarPosY = y + tall - YRES( 2 );
		vgui::surface()->DrawFilledRectFade( iHalfWide, nBarPosY, wide, nBarPosY + nBarHeight, 255, 0, true );
		vgui::surface()->DrawFilledRectFade( 0, nBarPosY, iHalfWide, nBarPosY + nBarHeight, 0, 255, true );
	}
	else
	{
		if ( V_wcslen( m_wAchievementsTitle ) > 0 )
		{
			BaseClass::DrawDialogBackground( NULL, m_wAchievementsTitle, NULL, m_wAchievementsProgress );
		}
		else
		{
			BaseClass::DrawDialogBackground( "#L4D360UI_My_Achievements", NULL, NULL, m_wAchievementsProgress );
		}
	}
}



