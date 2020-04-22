//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Display a list of achievements for the current game
//
//=============================================================================//

#include "achievementsdialog.h"
#include "vgui_controls/Button.h"
#include "vgui/ILocalize.h"
#include "ixboxsystem.h"
#include "iachievementmgr.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "MouseMessageForwardingPanel.h"
#include "filesystem.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ScrollBar.h"
#include "BasePanel.h"
#include "fmtstr.h"
#include "UtlSortVector.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

KeyValues *g_pPreloadedAchievementItemLayout = NULL;

#define NUM_COMBO_BOX_LINES_DEFAULT	10
#define NUM_COMBO_BOX_LINES_MAX		16

// Shared helper functions so xbox and pc can share as much code as possible while coming from different bases.

//-----------------------------------------------------------------------------
// Purpose: Sets the parameter pIconPanel to display the specified achievement's icon file.
//-----------------------------------------------------------------------------
bool LoadAchievementIcon( vgui::ImagePanel* pIconPanel, IAchievement *pAchievement, const char *pszExt /*= NULL*/ )
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
// The bias is to ensure the percentage bar gets plenty orange before it reaches the text,
// as the white-on-grey is hard to read.
//-----------------------------------------------------------------------------
Color LerpColors ( Color cStart, Color cEnd, float flPercent )
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
void UpdateProgressBar( vgui::EditablePanel* pPanel, IAchievement *pAchievement, Color clrProgressBar )
{
	if ( pAchievement->GetGoal() > 1 )
	{
		bool bShowProgress = true;

		// if this achievement gets saved with game and we're not in a level and have not achieved it, then we do not have any state 
		// for this achievement, don't show progress
		if ( ( pAchievement->GetFlags() & ACH_SAVE_WITH_GAME ) && !GameUI().IsInLevel() && !pAchievement->IsAchieved() )
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
			flCompletion = min( flCompletion, 1.f );
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
}

// TODO: revisit this once other games are rebuilt using the updated IAchievement interface
bool GameSupportsAchievementTracker()
{
	const char *pGame = Q_UnqualifiedFileName( engine->GetGameDirectory() );
	if ( ( Q_stricmp( pGame, "tf" ) == 0 ) || ( Q_stricmp( pGame, "tf_beta" ) == 0 ) )
		return true;

	return false;
}


//----------------------------------------------------------
// CAchievementsDialog_XBox
//----------------------------------------------------------

CAchievementsDialog_XBox::CAchievementsDialog_XBox( vgui::Panel *pParent ) : BaseClass( pParent, "AchievementsDialog" )
{
	m_nTotalAchievements = 0;
	m_nUnlocked = 0;
	m_iSelection = 0;
	m_iScroll = 0;
	m_iNumItems = 0;
	m_bCenterOnScreen = true;

	m_pProgressBg		= new vgui::Panel( this, "ProgressBg" );
	m_pProgressBar		= new vgui::Panel( this, "ProgressBar" );
 	m_pProgressPercent	= new vgui::Label( this, "ProgressPercent", "" );
 	m_pNumbering		= new vgui::Label( this, "Numbering", "" ); 
	m_pUpArrow			= new vgui::Label( this, "UpArrow", "" );
	m_pDownArrow		= new vgui::Label( this, "DownArrow", "" );

	SetDeleteSelfOnClose(true);
	SetSizeable( false );

	m_pFooter = new CFooterPanel( pParent, "AchievementsFooter" );
} 

CAchievementsDialog_XBox::~CAchievementsDialog_XBox()
{
	delete m_pProgressBg;
	delete m_pProgressBar;
	delete m_pProgressPercent;
	delete m_pNumbering;
	delete m_pUpArrow;
	delete m_pDownArrow;

	delete m_pFooter;
}

//----------------------------------------------------------
// Position the dialogs elements
//----------------------------------------------------------
void CAchievementsDialog_XBox::PerformLayout( void )
{
	BaseClass::PerformLayout();

	// Avoid division by zero if the achievements haven't been downloaded yet
	if ( m_nTotalAchievements )
	{
		int x, y, wide, tall;
		m_pProgressBg->GetBounds( x, y, wide, tall );
		int iBarX = GetWide() - wide - m_nBorderWidth;
		int iBarWide = wide * m_nUnlocked / m_nTotalAchievements;
		m_pProgressBg->SetBounds( iBarX, y, wide, tall );
		m_pProgressBar->SetBounds( iBarX, y, iBarWide, tall );

		wchar_t wszProgressText[64];
#ifdef WIN32
		V_snwprintf( wszProgressText, ARRAYSIZE(wszProgressText), L"%d%% %s", (m_nUnlocked * 100) / m_nTotalAchievements, g_pVGuiLocalize->Find( "#GameUI_Achievement_Unlocked" ) );
#else
		V_snwprintf( wszProgressText, ARRAYSIZE(wszProgressText), L"%d%% %S", (m_nUnlocked * 100) / m_nTotalAchievements, g_pVGuiLocalize->Find( "#GameUI_Achievement_Unlocked" ) );		
#endif
		m_pProgressPercent->SetText( wszProgressText );
		m_pProgressPercent->SizeToContents();
		m_pProgressPercent->SetPos( GetWide() - m_pProgressPercent->GetWide() - m_nBorderWidth, y + tall + 3 );

		int menux, menuy;
		m_Menu.GetPos( menux, menuy );

		// Do a perform layout on the menu so we get the correct height now
		m_Menu.InvalidateLayout( true, false );

		int iBottomOfTable = menuy + m_Menu.GetTall() + 3;

		m_pNumbering->SizeToContents();
		m_pNumbering->SetPos( m_nBorderWidth, iBottomOfTable );

		m_pUpArrow->GetPos( x, y );
		m_pUpArrow->SetPos( x, iBottomOfTable );
		m_pDownArrow->GetPos( x, y );
		m_pDownArrow->SetPos( x, iBottomOfTable );

		wchar_t wszNumbering[64];
		wchar_t *wzNumberingFmt = g_pVGuiLocalize->Find( "#GameUI_Achievement_Menu_Range" );
		wchar_t wzActiveItem[8];
		wchar_t wzTotal[8];
		V_snwprintf( wzActiveItem, ARRAYSIZE( wzActiveItem ), L"%d", m_Menu.GetActiveItemIndex()+1 );
		V_snwprintf( wzTotal, ARRAYSIZE( wzTotal ), L"%d", m_nTotalAchievements );
		g_pVGuiLocalize->ConstructString( wszNumbering, sizeof( wszNumbering ), wzNumberingFmt, 2, wzActiveItem, wzTotal );
		m_pNumbering->SetText( wszNumbering );
		m_pNumbering->SetWide( GetWide() );
	}

	if ( m_bCenterOnScreen )
	{
		MoveToCenterOfScreen();
	}

	// Set the footer
	m_pFooter->ClearButtons();

	if ( m_pFooterInfo && ( m_pFooterInfo->GetInt( "hide_regular_footer", 0 ) > 0 ) == false )
	{
		for ( KeyValues *pButton = m_pFooterInfo->GetFirstSubKey(); pButton != NULL; pButton = pButton->GetNextKey() )
		{
			if ( !Q_stricmp( pButton->GetName(), "button" ) )
			{
				// Add a button to the footer
				const char *pText = pButton->GetString( "text", "NULL" );
				const char *pIcon = pButton->GetString( "icon", "NULL" );
				m_pFooter->AddNewButtonLabel( pText, pIcon );
			}
		}
	}
}

//----------------------------------------------------------
// Query the system interface for this game's list of achievements
//----------------------------------------------------------
void CAchievementsDialog_XBox::ApplySettings( KeyValues *pResourceData )
{
	BaseClass::ApplySettings( pResourceData );

	m_bCenterOnScreen = pResourceData->GetInt( "center", 1 ) == 1;

	if ( !achievementmgr )
	{
		AssertOnce( 0 && "No achievement manager interface in GameUI.dll" );
		return;
	}

	achievementmgr->EnsureGlobalStateLoaded();

	int iAllAchievements = achievementmgr->GetAchievementCount();
	
	for ( int i = 0; i < iAllAchievements; ++i )
	{		
		IAchievement* pCurAchievement = (IAchievement*)achievementmgr->GetAchievementByIndex( i );
		Assert ( pCurAchievement );

		if ( pCurAchievement->IsAchieved() )
			++m_nUnlocked;

		// don't show hidden achievements if not achieved
		if ( pCurAchievement->ShouldHideUntilAchieved() && !pCurAchievement->IsAchieved() )
			continue;

		CAchievementItem *pItem = m_Menu.AddAchievementItem(  ACHIEVEMENT_LOCALIZED_NAME( pCurAchievement ), 
									ACHIEVEMENT_LOCALIZED_DESC( pCurAchievement ), 
									pCurAchievement->GetPointValue(),
									pCurAchievement->IsAchieved(),
									pCurAchievement
									);

		SETUP_PANEL( pItem );
	}

	m_nTotalAchievements = m_Menu.GetItemCount();
}

//----------------------------------------------------------
// Add the progress bar and update the dialog layout
//----------------------------------------------------------
void CAchievementsDialog_XBox::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pProgressBg->SetBgColor( Color( 200, 184, 151, 255 ) );
	m_pProgressBar->SetBgColor( Color( 179, 82, 22, 255 ) );
 	m_pProgressPercent->SetFgColor( pScheme->GetColor( "MatchmakingMenuItemDescriptionColor", Color( 64, 64, 64, 255 ) ) );
 	m_pNumbering->SetFgColor( pScheme->GetColor( "MatchmakingMenuItemDescriptionColor", Color( 64, 64, 64, 255 ) ) );
}


//-----------------------------------------------------------------------------
// Purpose: Scrolling up and down with up/down, exit to main menu with back button
//-----------------------------------------------------------------------------
void CAchievementsDialog_XBox::OnKeyCodePressed( vgui::KeyCode code )
{
	// Handle close here, CBasePanel parent doesn't support "DialogClosing" command
	if ( GetBaseButtonCode( code ) == KEY_XBUTTON_B )
	{
		OnCommand( "AchievementsDialogClosing" );
		SetDeleteSelfOnClose( true );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: catch key repeats
//-----------------------------------------------------------------------------
void CAchievementsDialog_XBox::HandleKeyRepeated( vgui::KeyCode code )
{
	BaseClass::HandleKeyRepeated( code );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Fade main menu back in when this dialog closes.
//-----------------------------------------------------------------------------
void CAchievementsDialog_XBox::OnClose()
{
	BasePanel()->RunCloseAnimation( "CloseAchievementsDialog_OpenMainMenu" );
	BaseClass::OnClose();
}

//////////////////////////////////////////////////////////////////////////
// PC Implementation
//////////////////////////////////////////////////////////////////////////



//-----------------------------------------------------------------------------
// Purpose: creates child panels, passes down name to pick up any settings from res files.
//-----------------------------------------------------------------------------
CAchievementsDialog::CAchievementsDialog(vgui::Panel *parent) : BaseClass(parent, "AchievementsDialog")
{
	SetDeleteSelfOnClose(true);
	SetBounds(0, 0, 512, 384);
	SetMinimumSize( 256, 300 );
	SetSizeable( true );

	m_nOldScrollItem = -1;
	m_nScrollItem = -1;

	m_nUnlocked = 0;
	m_nTotalAchievements = 0;

	m_pAchievementsList = new vgui::PanelListPanel( this, "listpanel_achievements" );
	m_pAchievementsList->SetFirstColumnWidth( 0 );

	m_pListBG = new vgui::ImagePanel( this, "listpanel_background" );

	m_pPercentageBarBackground = SETUP_PANEL( new ImagePanel( this, "PercentageBarBackground" ) );
	m_pPercentageBar = SETUP_PANEL( new ImagePanel( this, "PercentageBar" ) );
	
	m_pSelectionHighlight = new ImagePanel( m_pAchievementsList->FindChildByName( "PanelListEmbedded" ), "SelectionHighlight" );
	m_pSelectionHighlight->SetVisible( false );
	m_pSelectionHighlight->SetZPos( 100 );

	m_pAchievementPackCombo = new ComboBox(this, "achievement_pack_combo", NUM_COMBO_BOX_LINES_DEFAULT, false);
	m_pHideAchievedCheck = new vgui::CheckButton( this, "HideAchieved", "" );

	// int that holds the highest number achievement id we've found
	int iHighestAchievementIDSeen = -1;
	int iNextGroupBoundary = 1000;

	Q_memset( m_AchievementGroups, 0, sizeof(m_AchievementGroups) );
	m_iNumAchievementGroups = 0;

	// Base groups
//	CreateNewAchievementGroup( 0, 16000 );
	CreateNewAchievementGroup( 0, 999 );

	Assert ( achievementmgr );
	if ( achievementmgr )
	{
		int iCount = achievementmgr->GetAchievementCount();
		for ( int i = 0; i < iCount; ++i )
		{		
			IAchievement* pCur = achievementmgr->GetAchievementByIndex( i );

			if ( !pCur )
				continue;

			int iAchievementID = pCur->GetAchievementID();

			if ( iAchievementID > iHighestAchievementIDSeen )
			{
				// if its crossed the next group boundary, create a new group
				if ( iAchievementID >= iNextGroupBoundary )
				{
					int iNewGroupBoundary = 100 * ( (int)( (float)iAchievementID / 100 ) );
					CreateNewAchievementGroup( iNewGroupBoundary, iNewGroupBoundary+99 );

					iNextGroupBoundary = iNewGroupBoundary + 100;
				}

				iHighestAchievementIDSeen = iAchievementID;
			}

			// don't show hidden achievements if not achieved
			if ( pCur->ShouldHideUntilAchieved() && !pCur->IsAchieved() )
				continue;

			m_nTotalAchievements++;
			
			if ( m_pHideAchievedCheck->IsSelected() && pCur->IsAchieved() )
				continue;

			bool bAchieved = pCur->IsAchieved();

			if ( bAchieved )
			{
				++m_nUnlocked;
			}

			for ( int j=0;j<m_iNumAchievementGroups;j++ )
			{
				if ( iAchievementID >= m_AchievementGroups[j].m_iMinRange &&
					iAchievementID <= m_AchievementGroups[j].m_iMaxRange )
				{
					if ( bAchieved )
					{
						m_AchievementGroups[j].m_iNumUnlocked++;
					}

					m_AchievementGroups[j].m_iNumAchievements++;
				}
			}
		}
	}

	CreateOrUpdateComboItems( true );

	m_pAchievementPackCombo->ActivateItemByRow( 0 );
}

CAchievementsDialog::~CAchievementsDialog()
{
	if ( achievementmgr )
	{
		achievementmgr->SaveGlobalStateIfDirty( false );		// check for saving here to store achievements we want pinned to HUD
	}
	
	m_pAchievementsList->DeleteAllItems();
	delete m_pAchievementsList;
	delete m_pPercentageBarBackground;
	delete m_pPercentageBar;
}

void CAchievementsDialog::OnCheckButtonChecked(Panel *panel)
{
	if ( panel == m_pHideAchievedCheck )
	{
		UpdateAchievementList();
	}
}

void CAchievementsDialog::CreateNewAchievementGroup( int iMinRange, int iMaxRange )
{
	if ( m_iNumAchievementGroups < MAX_ACHIEVEMENT_GROUPS )
	{
		m_AchievementGroups[m_iNumAchievementGroups].m_iMinRange = iMinRange;
		m_AchievementGroups[m_iNumAchievementGroups].m_iMaxRange = iMaxRange;
		m_iNumAchievementGroups++;
	}
	else
	{
		AssertMsg( false, "CAchievementsDialog: Too many achievement groups" );
	}
}

//----------------------------------------------------------
// Get the width we're going to lock at
//----------------------------------------------------------
void CAchievementsDialog::ApplySettings( KeyValues *pResourceData )
{
	m_iFixedWidth = pResourceData->GetInt( "wide", 512 );

	BaseClass::ApplySettings( pResourceData );
}

//----------------------------------------------------------
// Preserve our width to the one in the .res file
//----------------------------------------------------------
void CAchievementsDialog::OnSizeChanged(int newWide, int newTall)
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
// New group was selected in the dropdown, recalc what achievements to show
//----------------------------------------------------------
void CAchievementsDialog::OnTextChanged(KeyValues *data)
{
	Panel *pPanel = (Panel *)data->GetPtr( "panel", NULL );

	// first check which control had its text changed!
	if ( pPanel == m_pAchievementPackCombo )
	{
		UpdateAchievementList();
	}
}

class CAchievementsLess
{
public:
	bool Less( const IAchievement* src1, const IAchievement* src2, void *pCtx )
	{
		IAchievement* _src1 = const_cast<IAchievement*>(src1);
		IAchievement* _src2 = const_cast<IAchievement*>(src2);

		if ( _src1->IsAchieved() && !_src2->IsAchieved() )
			return false;
		if ( !_src1->IsAchieved() && _src2->IsAchieved() )
			return true;

		const char* name1 = _src1->GetName();
		const char* name2 = _src2->GetName();
		if ( wcscoll( ACHIEVEMENT_LOCALIZED_NAME_FROM_STR( name1 ), ACHIEVEMENT_LOCALIZED_NAME_FROM_STR( name2 ) ) < 0 )
			return true;
		return false;
	}
};

//----------------------------------------------------------
// Re-populate the achievement list with the selected group
//----------------------------------------------------------
void CAchievementsDialog::UpdateAchievementList()
{	
	m_pAchievementsList->DeleteAllItems();

	KeyValues *pData = m_pAchievementPackCombo->GetActiveItemUserData();

	if ( pData )
	{
		int iMinRange = pData->GetInt( "minrange" );
		int iMaxRange = pData->GetInt( "maxrange" );

		if ( achievementmgr )
		{
			CUtlSortVector<IAchievement*, CAchievementsLess> achievements;

			int iCount = achievementmgr->GetAchievementCount();
			for ( int i = 0; i < iCount; ++i )
			{		
				IAchievement* pCur = achievementmgr->GetAchievementByIndex( i );

				if ( !pCur )
					continue;

				int iAchievementID = pCur->GetAchievementID();

				if ( iAchievementID < iMinRange || iAchievementID > iMaxRange )
					continue;

				// don't show hidden achievements if not achieved
				if ( pCur->ShouldHideUntilAchieved() && !pCur->IsAchieved() )
					continue;

				if ( m_pHideAchievedCheck->IsSelected() && pCur->IsAchieved() )
					continue;

				// if we don't have a localized name, don't add it to the list
				if ( pCur->GetName() == NULL || ACHIEVEMENT_LOCALIZED_NAME_FROM_STR( pCur->GetName() ) == NULL )
					continue;

				achievements.Insert( pCur );
			}

			for ( int i=0; i<achievements.Count(); i++ )
			{
				CAchievementDialogItemPanel *achievementItemPanel = new CAchievementDialogItemPanel( m_pAchievementsList, "AchievementDialogItemPanel", i );
				achievementItemPanel->SetAchievementInfo( achievements[i] );
				m_pAchievementsList->AddItem( NULL, achievementItemPanel );
			}
		}
	}

	if ( m_pAchievementsList->GetItemCount() > 0 )
	{
		m_nOldScrollItem = -1;
		m_nScrollItem = -1;
		m_pSelectionHighlight->SetVisible( false );
		m_pAchievementsList->ScrollToItem( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Loads settings from achievementsdialog.res in hl2/resource/ui/
//			Sets up progress bar displaying total achievement unlocking progress by the user.
//-----------------------------------------------------------------------------
void CAchievementsDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	LoadControlSettings("resource/ui/AchievementsDialog.res");

	if ( !achievementmgr )
	{
		AssertOnce( 0 && "No achievement manager interface in GameUI.dll" );
		return;
	}

	// Set up total completion percentage bar
	float flCompletion = 0.0f;
	if ( achievementmgr->GetAchievementCount() > 0 )
	{
		flCompletion = (((float)m_nUnlocked) / ((float)m_nTotalAchievements));
	}

	char szPercentageText[64];
	V_sprintf_safe( szPercentageText, "%d / %d ( %d%% )",
		m_nUnlocked, m_nTotalAchievements, (int)( flCompletion * 100.0f ) );

	SetControlString( "PercentageText", szPercentageText );
	SetControlVisible( "PercentageText", true );
	SetControlVisible( "CompletionText", true );

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

	m_pSelectionHighlight->SetFillColor( Color( clrHighlight.r(), clrHighlight.g(), clrHighlight.b(), 32 ) );

	if ( m_iNumAchievementGroups <= 2 )
	{
		// we have no achievement packs. Hide the combo and bump the achievement list up a bit
		m_pAchievementPackCombo->SetVisible( false );

		// do some work to preserve the pincorner and resizing

		int comboX, comboY;
		m_pAchievementPackCombo->GetPos( comboX, comboY );

		int x, y, w, h;
		m_pAchievementsList->GetBounds( x, y, w, h );

		PinCorner_e corner = m_pAchievementsList->GetPinCorner();
		int pinX, pinY;
		m_pAchievementsList->GetPinOffset( pinX, pinY );

		int resizeOffsetX, resizeOffsetY;
		m_pAchievementsList->GetResizeOffset( resizeOffsetX, resizeOffsetY );

		m_pAchievementsList->SetAutoResize( corner, AUTORESIZE_DOWN, pinX, comboY, resizeOffsetX, resizeOffsetY );

		m_pAchievementsList->SetBounds( x, comboY, w, h + ( y - comboY ) );

		m_pListBG->SetAutoResize( corner, AUTORESIZE_DOWN, pinX, comboY, resizeOffsetX, resizeOffsetY );
		m_pListBG->SetBounds( x, comboY, w, h + ( y - comboY ) );
	}

	MoveToCenterOfScreen();
}

void CAchievementsDialog::ScrollToItem( int nDirection )
{
	if ( m_pAchievementsList->GetItemCount() > 0 )
	{
		m_nScrollItem = clamp( m_nScrollItem + nDirection, 0, m_pAchievementsList->GetItemCount() - 1 );

		if ( m_nOldScrollItem != m_nScrollItem )
		{
			m_nOldScrollItem = m_nScrollItem;
			m_pAchievementsList->ScrollToItem( m_nScrollItem );

			Panel *pItem = m_pAchievementsList->GetItemPanel( m_nScrollItem );
			if ( pItem )
			{
				int nX, nY, nWide, nTall;
				pItem->GetBounds( nX, nY, nWide, nTall );

				m_pSelectionHighlight->SetVisible( true );
				m_pSelectionHighlight->SetBounds( nX, nY, nWide, nTall );
			}
		}
	}
}

void CAchievementsDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	// Handle close here, CBasePanel parent doesn't support "DialogClosing" command
	ButtonCode_t nButtonCode = GetBaseButtonCode( code );

	if ( nButtonCode == KEY_XBUTTON_B || nButtonCode == STEAMCONTROLLER_B )
	{
		OnCommand( "Close" );
	}
	else if ( nButtonCode == KEY_XBUTTON_X || nButtonCode == STEAMCONTROLLER_X )
	{
		if ( m_pHideAchievedCheck && m_pHideAchievedCheck->IsVisible() )
		{
			m_pHideAchievedCheck->SetSelected( !m_pHideAchievedCheck->IsSelected() );
		}
	}
	else if ( nButtonCode == KEY_XBUTTON_A || nButtonCode == STEAMCONTROLLER_A )
	{
		if ( GameSupportsAchievementTracker() && m_nScrollItem >= 0 && m_nScrollItem < m_pAchievementsList->GetItemCount() )
		{
			CAchievementDialogItemPanel *pItem = dynamic_cast< CAchievementDialogItemPanel* >( m_pAchievementsList->GetItemPanel( m_nScrollItem) );
			if ( pItem && pItem->IsVisible() )
			{
				pItem->ToggleShowOnHUD();
			}
		}
	}
	else if ( nButtonCode == KEY_XBUTTON_UP || 
			  nButtonCode == KEY_XSTICK1_UP ||
			  nButtonCode == KEY_XSTICK2_UP ||
			  nButtonCode == STEAMCONTROLLER_DPAD_UP ||
			  nButtonCode == KEY_UP )
	{
		ScrollToItem( -1 );
	}
	else if ( nButtonCode == KEY_XBUTTON_DOWN || 
			  nButtonCode == KEY_XSTICK1_DOWN ||
			  nButtonCode == KEY_XSTICK2_DOWN || 
			  nButtonCode == STEAMCONTROLLER_DPAD_DOWN ||
			  nButtonCode == KEY_DOWN )
	{
		ScrollToItem( 1 );
	}
	else if ( nButtonCode == KEY_XBUTTON_LEFT || 
			  nButtonCode == KEY_XSTICK1_LEFT ||
			  nButtonCode == KEY_XSTICK2_LEFT || 
			  nButtonCode == STEAMCONTROLLER_DPAD_LEFT ||
			  nButtonCode == KEY_LEFT )
	{
		m_pAchievementPackCombo->ActivateItemByRow( m_pAchievementPackCombo->GetActiveItem() - 1 );
	}
	else if ( nButtonCode == KEY_XBUTTON_RIGHT || 
			  nButtonCode == KEY_XSTICK1_RIGHT ||
			  nButtonCode == KEY_XSTICK2_RIGHT || 
			  nButtonCode == STEAMCONTROLLER_DPAD_RIGHT ||
			  nButtonCode == KEY_RIGHT )
	{
		m_pAchievementPackCombo->ActivateItemByRow( m_pAchievementPackCombo->GetActiveItem() + 1 );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Each sub-panel gets its data updated
//-----------------------------------------------------------------------------
void CAchievementsDialog::UpdateAchievementDialogInfo( void )
{
	int iCount = m_pAchievementsList->GetItemCount();
	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );

	for ( int i = 0; i < iCount; i++ )
	{
		CAchievementDialogItemPanel *pPanel = (CAchievementDialogItemPanel*)m_pAchievementsList->GetItemPanel(i);
		if ( pPanel )
		{
			pPanel->UpdateAchievementInfo( pScheme );
		}
	}

	// update the groups and overall progress bar
	if ( achievementmgr )
	{
		int i;

		// reset group completed counts
		for ( i=0;i<m_iNumAchievementGroups;i++ )
		{
			m_AchievementGroups[i].m_iNumUnlocked = 0;
		}

		int iAchievementCount = achievementmgr->GetAchievementCount();

		// update counts for each achieved achievement
		for ( i=0;i<iAchievementCount;i++ )
		{
			IAchievement* pCurAchievement = achievementmgr->GetAchievementByIndex( i );

			if ( !pCurAchievement || !pCurAchievement->IsAchieved() )
				continue;
			
			int iAchievementID = pCurAchievement->GetAchievementID();

			for ( int j=0;j<m_iNumAchievementGroups;j++ )
			{
				if ( iAchievementID >= m_AchievementGroups[j].m_iMinRange &&
					 iAchievementID <= m_AchievementGroups[j].m_iMaxRange )
				{
					m_AchievementGroups[j].m_iNumUnlocked++;
				}
			}
		}

		// update the global progress bar label
		char szPercentageText[64];
		float flCompletion = (((float)m_AchievementGroups[0].m_iNumUnlocked) / ((float)iAchievementCount));
		V_sprintf_safe( szPercentageText, "%d / %d ( %d%% )",
			m_AchievementGroups[0].m_iNumUnlocked, m_nTotalAchievements, (int)( flCompletion * 100.0f ) );

		// and the progress bar
		m_pPercentageBar->SetWide( m_pPercentageBarBackground->GetWide() * flCompletion );

		SetControlString( "PercentageText", szPercentageText );
	}

	CreateOrUpdateComboItems( false );	// update them with new achieved counts	

	m_pAchievementPackCombo->ActivateItemByRow( m_pAchievementPackCombo->GetActiveItem() );
}

void CAchievementsDialog::CreateOrUpdateComboItems( bool bCreate )
{
	for ( int i=0;i<m_iNumAchievementGroups;i++ )
	{
		char buf[128];

		Q_snprintf( buf, sizeof(buf), "#Achievement_Group_%d", m_AchievementGroups[i].m_iMinRange );

		const wchar_t *wzGroupName = g_pVGuiLocalize->Find( buf );

		if ( !wzGroupName )
		{
			wzGroupName = L"Need Title ( %s1 of %s2 )";
		}

		wchar_t wzGroupTitle[128];

		if ( wzGroupName )
		{
			wchar_t wzNumUnlocked[8];
			V_snwprintf( wzNumUnlocked, ARRAYSIZE( wzNumUnlocked ), L"%d", m_AchievementGroups[i].m_iNumUnlocked );

			wchar_t wzNumAchievements[8];
			V_snwprintf( wzNumAchievements, ARRAYSIZE( wzNumAchievements ), L"%d", m_AchievementGroups[i].m_iNumAchievements );

			g_pVGuiLocalize->ConstructString( wzGroupTitle, sizeof( wzGroupTitle ), wzGroupName, 2, wzNumUnlocked, wzNumAchievements );
		}

		KeyValues *pKV = new KeyValues( "grp" );
		pKV->SetInt( "minrange", m_AchievementGroups[i].m_iMinRange );
		pKV->SetInt( "maxrange", m_AchievementGroups[i].m_iMaxRange );

		if ( bCreate )
			m_AchievementGroups[i].m_iDropDownGroupID = m_pAchievementPackCombo->AddItem( wzGroupTitle, pKV );
		else
			m_pAchievementPackCombo->UpdateItem( m_AchievementGroups[i].m_iDropDownGroupID, wzGroupTitle, pKV );
	}

	if ( bCreate && ( m_iNumAchievementGroups > NUM_COMBO_BOX_LINES_DEFAULT ) )
	{
		if ( m_pAchievementPackCombo )
		{
			m_pAchievementPackCombo->SetNumberOfEditLines( ( m_iNumAchievementGroups <= NUM_COMBO_BOX_LINES_MAX ) ? m_iNumAchievementGroups : NUM_COMBO_BOX_LINES_MAX );
		}
	}
}

void CAchievementsDialog::OnCommand( const char *command )
{
	if ( !Q_strcasecmp( command, "ongameuiactivated" ) )
	{
		UpdateAchievementDialogInfo();
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: creates child panels, passes down name to pick up any settings from res files.
//-----------------------------------------------------------------------------
CAchievementDialogItemPanel::CAchievementDialogItemPanel( vgui::PanelListPanel *parent, const char* name, int iListItemID ) : BaseClass( parent, name )
{
	m_iListItemID = iListItemID;
	m_pParent = parent;
	m_pSchemeSettings = NULL;

	SetMouseInputEnabled( true );
	parent->SetMouseInputEnabled( true );

	//CMouseMessageForwardingPanel *panel = new CMouseMessageForwardingPanel(this, NULL);
	//panel->SetZPos(2);
}

CAchievementDialogItemPanel::~CAchievementDialogItemPanel()
{
	/*
	delete m_pAchievementIcon;
	delete m_pAchievementNameLabel;
	delete m_pAchievementDescLabel;
	delete m_pPercentageBar;
	delete m_pPercentageText;
	delete m_pShowOnHUDCheck;
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Updates displayed achievement data. In applyschemesettings, and when gameui activates.
//-----------------------------------------------------------------------------
void CAchievementDialogItemPanel::UpdateAchievementInfo( IScheme* pScheme )
{
	if ( m_pSourceAchievement && m_pSchemeSettings )
	{
		// Set name, description and unlocked state text
		m_pAchievementNameLabel->SetText( ACHIEVEMENT_LOCALIZED_NAME( m_pSourceAchievement ) );
		m_pAchievementDescLabel->SetText( ACHIEVEMENT_LOCALIZED_DESC( m_pSourceAchievement ) );

		// Setup icon
		// get the vtfFilename from the path.

		// Display percentage completion for progressive achievements
		// Set up total completion percentage bar. Goal > 1 means its a progress achievement.
		UpdateProgressBar( this, m_pSourceAchievement, m_clrProgressBar );

		if ( m_pSourceAchievement->IsAchieved() )
		{
			LoadAchievementIcon( m_pAchievementIcon, m_pSourceAchievement );

			SetBgColor( pScheme->GetColor( "AchievementsLightGrey", Color(255, 0, 0, 255) ) );

			m_pAchievementNameLabel->SetFgColor( pScheme->GetColor( "SteamLightGreen", Color(255, 255, 255, 255) ) );

			Color fgColor = pScheme->GetColor( "Label.TextBrightColor", Color(255, 255, 255, 255) );
			m_pAchievementDescLabel->SetFgColor( fgColor );
			m_pPercentageText->SetFgColor( fgColor );
			m_pShowOnHUDCheck->SetVisible( false );
			m_pShowOnHUDCheck->SetSelected( false );
		}
		else
		{
			LoadAchievementIcon( m_pAchievementIcon, m_pSourceAchievement, "_bw" );

			SetBgColor( pScheme->GetColor( "AchievementsDarkGrey", Color(255, 0, 0, 255) ) );

			Color fgColor = pScheme->GetColor( "AchievementsInactiveFG", Color(255, 255, 255, 255) );
			m_pAchievementNameLabel->SetFgColor( fgColor );
			m_pAchievementDescLabel->SetFgColor( fgColor );
			m_pPercentageText->SetFgColor( fgColor );
			if ( GameSupportsAchievementTracker() )
			{
				m_pShowOnHUDCheck->SetVisible( !m_pSourceAchievement->ShouldHideUntilAchieved() );	// m_pSourceAchievement->GetGoal() > 1 && 
				m_pShowOnHUDCheck->SetSelected( m_pSourceAchievement->ShouldShowOnHUD() );
			}
			else
			{
				m_pShowOnHUDCheck->SetVisible( false );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Makes a local copy of a pointer to the achievement entity stored on the client.
//-----------------------------------------------------------------------------
void CAchievementDialogItemPanel::SetAchievementInfo( IAchievement* pAchievement )
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
void CAchievementDialogItemPanel::PreloadResourceFile( void )
{
	const char *controlResourceName = "resource/ui/AchievementItem.res";

	g_pPreloadedAchievementItemLayout = new KeyValues(controlResourceName);
	g_pPreloadedAchievementItemLayout->LoadFromFile(g_pFullFileSystem, controlResourceName);
}

//-----------------------------------------------------------------------------
// Purpose: Loads settings from hl2/resource/ui/achievementitem.res
//			Sets display info for this achievement item.
//-----------------------------------------------------------------------------
void CAchievementDialogItemPanel::ApplySchemeSettings( IScheme* pScheme )
{
	if ( !g_pPreloadedAchievementItemLayout )
	{
		PreloadResourceFile();
	}

	LoadControlSettings( "", NULL, g_pPreloadedAchievementItemLayout );

	m_pSchemeSettings = pScheme;
	
	if ( !m_pSourceAchievement )
	{
		return;
	}

	m_pAchievementIcon		= SETUP_PANEL(dynamic_cast<vgui::ImagePanel*>(FindChildByName("AchievementIcon")));
	m_pAchievementNameLabel = dynamic_cast<vgui::Label*>(FindChildByName("AchievementName"));
	m_pAchievementDescLabel = dynamic_cast<vgui::Label*>(FindChildByName("AchievementDesc"));
	m_pPercentageBar		= SETUP_PANEL(dynamic_cast<vgui::ImagePanel*>(FindChildByName("PercentageBar")));
	m_pPercentageText		= dynamic_cast<vgui::Label*>(FindChildByName("PercentageText"));
	m_pShowOnHUDCheck		= dynamic_cast<vgui::CheckButton*>(FindChildByName("ShowOnHUD"));
	m_pShowOnHUDCheck->SetMouseInputEnabled( true );
	m_pShowOnHUDCheck->SetEnabled( true );
	m_pShowOnHUDCheck->SetCheckButtonCheckable( true );
	m_pShowOnHUDCheck->AddActionSignalTarget( this );

	BaseClass::ApplySchemeSettings( pScheme );

	// m_pSchemeSettings must be set for this.
	UpdateAchievementInfo( pScheme );
}

void CAchievementDialogItemPanel::ToggleShowOnHUD( void )
{
	m_pShowOnHUDCheck->SetSelected( !m_pShowOnHUDCheck->IsSelected() );
}

void CAchievementDialogItemPanel::OnCheckButtonChecked(Panel *panel)
{
	if ( GameSupportsAchievementTracker() && panel == m_pShowOnHUDCheck && m_pSourceAchievement )
	{
		m_pSourceAchievement->SetShowOnHUD( m_pShowOnHUDCheck->IsSelected() );
	}
}
