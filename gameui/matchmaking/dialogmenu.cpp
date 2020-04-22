//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Multi-purpose menu for matchmaking dialogs, navigable with the xbox controller.
//
//=============================================================================//

#include "engine/imatchmaking.h"
#include "GameUI_Interface.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "KeyValues.h"
#include "dialogmenu.h"
#include "BasePanel.h"
#include "vgui_controls/ImagePanel.h"
#include "iachievementmgr.h"			// for iachievement abstract class in CAchievementItem
#include "achievementsdialog.h"			// for helper functions used by both pc and xbox achievements

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------
// Base class representing a generic menu item. Supports two text labels,
// where the first label is the "action" text and the second is an optional
// description of the action.
//-----------------------------------------------------------------------
CMenuItem::CMenuItem( CDialogMenu *pParent, const char *pTitle, const char *pDescription ) 
	: BaseClass( pParent, "MenuItem" )
{
	// Quiet "parent not sized yet" spew
	SetSize( 10, 10 );

	m_pParent = pParent;

	m_bEnabled = true;
	m_nDisabledAlpha = 30;

	m_pTitle = new vgui::Label( this, "MenuItemText", pTitle );
	m_pDescription = NULL;
	if ( pDescription )
	{
		m_pDescription = new vgui::Label( this, "MenuItemDesc", pDescription );
	}
}

CMenuItem::~CMenuItem()
{
	delete m_pTitle;
	delete m_pDescription;
}

//-----------------------------------------------------------------------
// Update colors according to enabled/disabled state
//-----------------------------------------------------------------------
void CMenuItem::PerformLayout()
{
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------
// Setup margins and calculate the total menu item size
//-----------------------------------------------------------------------
void CMenuItem::ApplySettings( KeyValues *pSettings )
{
	BaseClass::ApplySettings( pSettings );

	m_nBottomMargin = pSettings->GetInt( "bottommargin", 0 );
	m_nRightMargin	= pSettings->GetInt( "rightmargin", 0 );

	int x, y;
	m_pTitle->GetPos( x, y );
	m_pTitle->SizeToContents();

	int bgTall = y + m_pTitle->GetTall() + m_nBottomMargin;
	int textWide = m_pTitle->GetWide();

	if ( m_pDescription )
	{
		m_pDescription->SizeToContents();
		m_pDescription->GetPos( x, y );
		bgTall = y + m_pDescription->GetTall() + m_nBottomMargin;
		textWide = max( textWide, m_pDescription->GetWide() );
	}

	int bgWide = x + textWide + m_nRightMargin;

	SetSize( bgWide, bgTall );
}

//-----------------------------------------------------------------------
// Setup colors and fonts
//-----------------------------------------------------------------------
void CMenuItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundType( 2 );

	m_BgColor			= pScheme->GetColor( "MatchmakingMenuItemBackground", Color( 46, 43, 42, 255 ) );
	m_BgColorActive		= pScheme->GetColor( "MatchmakingMenuItemBackgroundActive", Color( 150, 71, 0, 255 ) );

	m_pTitle->SetFgColor( pScheme->GetColor( "MatchmakingMenuItemTitleColor", Color( 0, 0, 0, 255 ) ) );

	if ( m_pDescription )
	{
		m_pDescription->SetFgColor( pScheme->GetColor( "MatchmakingMenuItemDescriptionColor", Color( 0, 0, 0, 255 ) ) );
	}

		KeyValues *pKeys = BasePanel()->GetConsoleControlSettings()->FindKey( "MenuItem.res" );
		ApplySettings( pKeys );
}

//-----------------------------------------------------------------------
// Set an item as having input focus
//-----------------------------------------------------------------------
void CMenuItem::SetFocus( const bool bActive )
{
	if ( bActive )
	{
		SetBgColor( m_BgColorActive );
	}
	else
	{
		SetBgColor( m_BgColor );
	}
}

//-----------------------------------------------------------------------
// Set an item as having input focus
//-----------------------------------------------------------------------
void CMenuItem::SetEnabled( bool bEnabled )
{
	if ( bEnabled )
	{
		SetAlpha( 255 );
	}
	else
	{
		SetAlpha( m_nDisabledAlpha );
	}
	m_bEnabled = bEnabled;
}

//-----------------------------------------------------------------------
// Set a column as having focus
//-----------------------------------------------------------------------
void CMenuItem::SetActiveColumn( int col )
{
	// do nothing
}

//-----------------------------------------------------------------------
// Set an item as having input focus
//-----------------------------------------------------------------------
bool CMenuItem::IsEnabled()
{
	return m_bEnabled;
}

//-----------------------------------------------------------------------
// Perform any special actions when an item is "clicked"
//-----------------------------------------------------------------------
void CMenuItem::OnClick()
{
	// do nothing - derived classes implement this
}


//-----------------------------------------------------------------------
// CCommandItem
//
// Menu item that issues a command when clicked.
//-----------------------------------------------------------------------
CCommandItem::CCommandItem( CDialogMenu *pParent, const char *pTitleLabel, const char *pDescLabel, const char *pCommand ) 
	: BaseClass( pParent, pTitleLabel, pDescLabel )
{
	Q_strncpy( m_szCommand, pCommand, MAX_COMMAND_LEN );
}

CCommandItem::~CCommandItem()
{
	// do nothing
}

void CCommandItem::OnClick()
{
	GetParent()->OnCommand( m_szCommand );

	vgui::surface()->PlaySound( "UI/buttonclick.wav" );
}

void CCommandItem::SetFocus(const bool bActive )
{
	BaseClass::SetFocus( bActive );

	if ( bActive == true && m_bHasFocus == false )
	{
		vgui::surface()->PlaySound( "UI/buttonclickrelease.wav" );
	}

	m_bHasFocus = bActive;
}


//-----------------------------------------------------------------------
// CPlayerItem
//
// Menu item to display a player in the lobby.
//-----------------------------------------------------------------------
CPlayerItem::CPlayerItem( CDialogMenu *pParent, const char *pTitleLabel, int64 nId, byte bVoice, bool bReady ) 
: BaseClass( pParent, pTitleLabel, NULL, "ShowGamerCard" )
{
	m_pVoiceIcon = new vgui::Label( this, "voiceicon", "" );
	m_pReadyIcon = new vgui::Label( this, "readyicon", "" );

	m_nId			= nId;
	m_bVoice		= bVoice;
	m_bReady		= bReady;
}

CPlayerItem::~CPlayerItem()
{
	delete m_pVoiceIcon;
	delete m_pReadyIcon;
}

void CPlayerItem::PerformLayout()
{
	BaseClass::PerformLayout();

	const char *pVoice = "";

	if ( m_bVoice == 2 )
	{
		pVoice = "#TF_Icon_Voice";
	}
	else if ( m_bVoice == 1 )
	{
		pVoice = "#TF_Icon_Voice_Idle";
	}

	m_pVoiceIcon->SetText( pVoice );
	m_pReadyIcon->SetText( m_bReady ? "#TF_Icon_Ready" : "#TF_Icon_NotReady" );

	int x, y;
	m_pReadyIcon->GetPos( x, y );
	m_pReadyIcon->SetPos( GetWide() - m_pReadyIcon->GetWide() - m_nRightMargin, y );
}

void CPlayerItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	KeyValues *pKeys = BasePanel()->GetConsoleControlSettings()->FindKey( "PlayerItem.res" );
	ApplySettings( pKeys );
}

void CPlayerItem::OnClick()
{
	BaseClass::OnClick();
}

//-----------------------------------------------------------------------
// CBrowserItem
//
// Menu item used to display session search results.
//-----------------------------------------------------------------------
CBrowserItem::CBrowserItem( CDialogMenu *pParent, const char *pHost, const char *pPlayers, const char *pScenario, const char *pPing ) 
	: BaseClass( pParent, pHost, NULL, "SelectSession" )
{
	m_pPlayers = new vgui::Label( this, "players", pPlayers );
	m_pScenario = new vgui::Label( this, "scenario", pScenario );
	m_pPing = new vgui::Label( this, "ping", pPing );
}

CBrowserItem::~CBrowserItem()
{
	delete m_pPlayers;
	delete m_pScenario;
	delete m_pPing;
}

void CBrowserItem::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, wide, tall;
	m_pPing->GetBounds( x, y, wide, tall );

	m_pScenario->SizeToContents();
	int sx, sy;
	m_pScenario->GetPos( sx, sy );
	m_pScenario->SetPos( x - m_pScenario->GetWide() - m_nRightMargin, sy );

	SetSize( x + wide, GetTall() );
}

void CBrowserItem::ApplySettings( KeyValues *pSettings )
{
	BaseClass::ApplySettings( pSettings );
}

void CBrowserItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	Color fgcolor = pScheme->GetColor( "MatchmakingMenuItemDescriptionColor", Color( 64, 64, 64, 255 ) );
	m_pPlayers->SetFgColor( fgcolor );
	m_pScenario->SetFgColor( fgcolor );

	m_pPing->SetContentAlignment( vgui::Label::a_center );

	KeyValues *pKeys = BasePanel()->GetConsoleControlSettings()->FindKey( "BrowserItem.res" );
	ApplySettings( pKeys );

	SetFocus( false );
}

//-----------------------------------------------------------------------
// COptionsItem
//
// Menu item used to present a list of options for the player to select
// from, such as "choose a map" or "number of rounds".
//-----------------------------------------------------------------------
COptionsItem::COptionsItem( CDialogMenu *pParent, const char *pLabel ) 
	: BaseClass( pParent, pLabel, NULL )
{
	m_nActiveOption = m_Options.InvalidIndex();
	m_nOptionsXPos = 0;
	m_nMaxOptionWidth = 0;

	m_szOptionsFont[0] = '\0';
	m_hOptionsFont = vgui::INVALID_FONT;

	m_pLeftArrow = new vgui::Label( this, "LeftArrow", "" );
	m_pRightArrow = new vgui::Label( this, "RightArrow", "" );
}

COptionsItem::~COptionsItem()
{
	m_OptionLabels.PurgeAndDeleteElements();

	delete m_pLeftArrow;
	delete m_pRightArrow;
}

void COptionsItem::PerformLayout()
{
	BaseClass::PerformLayout();

	int optionWide = max( m_nOptionsMinWide, GetWide() - m_nOptionsXPos - m_pRightArrow->GetWide() - m_nOptionsLeftMargin );
	int optionTall = GetTall();

	for ( int i = 0; i < m_OptionLabels.Count(); ++i )
	{
		vgui::Label *pOption = m_OptionLabels[i];

		pOption->SetBounds( m_nOptionsXPos, 0, optionWide, optionTall );
	}

	int lx, ly;
	m_pLeftArrow->GetPos( lx, ly );
	m_pLeftArrow->SetPos( m_nOptionsXPos - m_nArrowGap - m_pLeftArrow->GetWide(), ly );

	int rx, ry;
	m_pRightArrow->GetPos( rx, ry );
	m_pRightArrow->SetPos( m_nOptionsXPos + optionWide + m_nArrowGap, ry );

	m_pLeftArrow->SetAlpha( 255 );
	m_pRightArrow->SetAlpha( 255 );

	if ( m_nActiveOption == 0 )
	{
		m_pLeftArrow->SetAlpha( 32 );
	}
	else if ( m_nActiveOption == m_OptionLabels.Count() - 1 )
	{
		m_pRightArrow->SetAlpha( 32 );
	}
}

void COptionsItem::ApplySettings( KeyValues *pSettings )
{
	BaseClass::ApplySettings( pSettings );

	m_nOptionsXPos			= pSettings->GetInt( "optionsxpos", 0 );
	m_nOptionsMinWide		= pSettings->GetInt( "optionsminwide", 0 );
	m_nOptionsLeftMargin	= pSettings->GetInt( "optionsleftmargin", 0 );
	m_nArrowGap				= pSettings->GetInt( "arrowgap", 0 );

	Q_strncpy( m_szOptionsFont, pSettings->GetString( "optionsfont", "Default" ), sizeof( m_szOptionsFont ) );
}

void COptionsItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( false );

	m_pTitle->SetFgColor( pScheme->GetColor( "MatchmakingMenuItemTitleColor", Color( 200, 184, 151, 255 ) ) );

	KeyValues *pKeys = BasePanel()->GetConsoleControlSettings()->FindKey( "OptionsItem.res" );
	ApplySettings( pKeys );

	m_hOptionsFont = pScheme->GetFont( m_szOptionsFont );
}

void COptionsItem::SetFocus( const bool bActive )
{
	if ( bActive )
	{
		for ( int i = 0; i < m_OptionLabels.Count(); ++i )
		{
			m_OptionLabels[i]->SetBgColor( m_BgColorActive );
		}
	}
	else
	{
		for ( int i = 0; i < m_OptionLabels.Count(); ++i )
		{
			m_OptionLabels[i]->SetBgColor( m_BgColor );
		}
	}
}

void COptionsItem::AddOption( const char *pLabelText, const sessionProperty_t& option )
{
	// Add a new option to this item's list of options
	m_Options.AddToTail( option );

	int idx = m_OptionLabels.AddToTail( new vgui::Label( this, "Option Value", pLabelText ) );
	vgui::Label *pOption = m_OptionLabels[idx];

	// Check for a format string
	if ( Q_stristr( pLabelText, "Fmt" ) )
	{
		wchar_t wszString[64];
		wchar_t wzNumber[8];
		wchar_t *wzFmt = g_pVGuiLocalize->Find( pLabelText );
		g_pVGuiLocalize->ConvertANSIToUnicode( option.szValue, wzNumber, sizeof( wzNumber ) );
		g_pVGuiLocalize->ConstructString( wszString, sizeof( wszString ), wzFmt, 1, wzNumber );
		pOption->SetText( wszString );
	}

	SETUP_PANEL( pOption );
	pOption->SetPaintBackgroundType( 2 );

	pOption->SetFont( m_hOptionsFont );
	pOption->SetBgColor( Color( 46, 43, 42, 255 ) );
	pOption->SetFgColor( m_pTitle ? m_pTitle->GetFgColor() : Color( 200, 184, 151, 255 ) );
	pOption->SetTextInset( m_nOptionsLeftMargin, 0 );
	pOption->SetContentAlignment( vgui::Label::a_southwest );
	pOption->SizeToContents();

	int wide = max( m_nOptionsMinWide, pOption->GetWide() );
	pOption->SetBounds( m_nOptionsXPos, 0, wide, GetTall() );
	m_nMaxOptionWidth = max( wide, m_nMaxOptionWidth );

	SetWide( m_nOptionsXPos + m_nMaxOptionWidth + m_nOptionsLeftMargin * 2 + m_nArrowGap * 2 + m_pRightArrow->GetWide() );
}

//-----------------------------------------------------------------------
// Return the session property associated with the current active option
//-----------------------------------------------------------------------
const sessionProperty_t &COptionsItem::GetActiveOption()
{
	return m_Options[m_nActiveOption];
}

//-----------------------------------------------------------------------
// Return the index of the current active option
//-----------------------------------------------------------------------
int COptionsItem::GetActiveOptionIndex()
{
	return m_nActiveOption;
}

//-----------------------------------------------------------------------
// Sets which option currently has focus
//-----------------------------------------------------------------------
void COptionsItem::SetOptionFocus( unsigned int idx )
{
	unsigned int itemCt = (unsigned int)m_OptionLabels.Count();
	if ( idx > itemCt )
		return;

	m_nActiveOption = idx;

	for ( unsigned int i = 0; i < itemCt; ++i )
	{
		vgui::Label *pLabel = m_OptionLabels[i];

		const bool bVisible = ( i == idx );
		pLabel->SetVisible( bVisible );
	}

	InvalidateLayout();
}

//-----------------------------------------------------------------------
// Move focus to the next option - does not wrap
//-----------------------------------------------------------------------
void COptionsItem::SetOptionFocusNext()
{
	if ( m_nActiveOption + 1 < m_OptionLabels.Count() )
	{
		SetOptionFocus( m_nActiveOption + 1 );
	}
	else
	{
		vgui::surface()->PlaySound( "player/suit_denydevice.wav" );
	}
}

//-----------------------------------------------------------------------
// Move focus to the previous option - does not wrap
//-----------------------------------------------------------------------
void COptionsItem::SetOptionFocusPrev()
{
	if ( m_nActiveOption > 0 )
	{
		SetOptionFocus( m_nActiveOption - 1 );
	}
	else
	{
		vgui::surface()->PlaySound( "player/suit_denydevice.wav" );
	}
}


//-----------------------------------------------------------------------
// CAchievementItem
//
// Menu item used to present an achievement - including image, title,
// description, points and unlock date. Clicking the item opens another
// dialog with additional information about the achievement.
//-----------------------------------------------------------------------
CAchievementItem::CAchievementItem( CDialogMenu *pParent, const wchar_t *pName, const wchar_t *pDesc, uint points, bool bUnlocked, IAchievement* pSourceAchievement ) 
	: BaseClass( pParent, "", "" )
{
	// Title and description were returned as results of a system query,
	// and are therefore already localized.
	m_pTitle->SetText( pName );

	if ( IsX360() )
	{
		wchar_t buf[120];

		// Get the screen size
		int wide, tall;
		vgui::surface()->GetScreenSize(wide, tall);

		unsigned int iWrapLen;

		if ( tall <= 480 )
		{
			iWrapLen = 50;
		}
		else
		{
			iWrapLen = 65;
		}

		// let's do some wrapping on this label
		wcsncpy( buf, pDesc, sizeof(buf) / sizeof( wchar_t ) );

		if ( wcslen(buf) > iWrapLen )
		{
			int iPos = iWrapLen;

			while ( iPos > 0 && buf[iPos] != L' ' )
			{
				iPos--;
			}

			if ( iPos > 0 && buf[iPos] == L' ' )
			{
				buf[iPos] = L'\n';
			}				
		}

		m_pDescription->SetText( buf );
	}
	else
	{
		m_pDescription->SetText( pDesc );
	}

	m_pSourceAchievement = pSourceAchievement;

	m_pPercentageBarBackground = SETUP_PANEL( new vgui::ImagePanel( this, "PercentageBarBackground" ) );
	m_pPercentageBar = SETUP_PANEL( new vgui::ImagePanel( this, "PercentageBar" ) );
	m_pPercentageText = SETUP_PANEL( new vgui::Label( this, "PercentageText", "" ) );

	// Set the status icons
	m_pLockedIcon = SETUP_PANEL( new vgui::ImagePanel( this, "lockedicon" ) );
	m_pUnlockedIcon = SETUP_PANEL( new vgui::ImagePanel( this, "unlockedicon" ) );

	// Gamerscore number
	if ( IsX360() )
	{
		wchar_t *wzFormat = g_pVGuiLocalize->Find( "#GameUI_Achievement_Points" );	// "%s1G"
		wchar_t wzPoints[10];
		V_snwprintf( wzPoints, ARRAYSIZE( wzPoints ), L"%d", points );
		wchar_t wzPointsLayout[10];
		g_pVGuiLocalize->ConstructString( wzPointsLayout, sizeof( wzPointsLayout ), wzFormat, 1, wzPoints );
		m_pPoints = new vgui::Label( this, "Points", wzPointsLayout );
	}

	// Achievement image
	m_pImage = new vgui::ImagePanel( this, "icon" );
}

CAchievementItem::~CAchievementItem()
{
	delete m_pImage;
	delete m_pPoints;
	delete m_pLockedIcon;
	delete m_pUnlockedIcon;
	delete m_pPercentageBarBackground;
	delete m_pPercentageBar;
	delete m_pPercentageText;
}

void CAchievementItem::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y;

 	m_pPoints->SizeToContents();
	m_pPoints->GetPos( x, y );
 	x = GetWide() - m_pPoints->GetWide() - m_nRightMargin;
 	m_pPoints->SetPos( x, y );

}

void CAchievementItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	KeyValues*pKeys = BasePanel()->GetConsoleControlSettings()->FindKey( "AchievementItem.res" );	
	ApplySettings( pKeys );

	m_pImage->SetBgColor( Color( 32, 32, 32, 255 ) );
	m_pImage->SetFgColor( Color( 32, 32, 32, 255 ) );
	m_pImage->SetPaintBackgroundEnabled( true );

	m_pPoints->SetFgColor( pScheme->GetColor( "MatchmakingMenuItemDescriptionColor", Color( 64, 64, 64, 255 ) ) );

	// Set icon image
	LoadAchievementIcon( m_pImage, m_pSourceAchievement );

	// Percentage completion bar (for progressive achievements)
	UpdateProgressBar( this, m_pSourceAchievement, m_clrProgressBar );

	if ( m_pSourceAchievement && m_pSourceAchievement->IsAchieved() )
	{
		m_pLockedIcon->SetVisible( false );
		m_pUnlockedIcon->SetVisible ( true );
		m_pImage->SetVisible( true );
	}
	else
	{
		m_pLockedIcon->SetVisible( true );
		m_pUnlockedIcon->SetVisible( false );
		m_pImage->SetVisible( false );
	}
}

//-----------------------------------------------------------------------
// CSectionedItem
//
// Menu item used to display some number of data entries, which are arranged
// into columns.  Supports scrolling through columns horizontally with the 
// ability to "lock" columns so they don't scroll
//-----------------------------------------------------------------------
CSectionedItem::CSectionedItem( CDialogMenu *pParent, const char **ppEntries, int ct  ) 
	: BaseClass( pParent, "", NULL, "SelectSession" )
{
	m_bHeader = false;
	for ( int i = 0; i < ct; ++i )
	{
		AddSection( ppEntries[i], m_pParent->GetColumnAlignment( i ) );
	}
}

CSectionedItem::~CSectionedItem()
{
	ClearSections();
}

void CSectionedItem::ClearSections()
{
	for ( int i = 0; i < m_Sections.Count(); ++i )
	{
		section_s &sec = m_Sections[i];
		delete sec.pLabel;
	}
}

void CSectionedItem::PerformLayout()
{
	BaseClass::PerformLayout();

	int tall = GetTall();
	for ( int i = 0; i < m_Sections.Count(); ++i )
	{
		vgui::Label *pLabel = m_Sections[i].pLabel;
		if ( !m_bHeader )
		{
			pLabel->SetFont( m_pParent->GetColumnFont(i) );
			pLabel->SetFgColor( m_pParent->GetColumnColor(i) );
		}
		pLabel->SetBounds( m_pParent->GetColumnXPos(i), 0, m_pParent->GetColumnWide(i), tall );
		pLabel->SetTextInset( 10, m_bHeader ? 5 : m_pParent->GetColumnYPos(i) ); // only use ypos for the y-inset if we're not a header
	}
}

void CSectionedItem::ApplySettings( KeyValues *pResourceData )
{
	BaseClass::ApplySettings( pResourceData );
}

void CSectionedItem::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	KeyValues *pKeys = BasePanel()->GetConsoleControlSettings()->FindKey( "SectionedItem.res" );
	ApplySettings( pKeys );

	int iLast = m_Sections.Count() -1;
	SetWide( m_pParent->GetColumnXPos(iLast) + m_pParent->GetColumnWide(iLast) );
}

void CSectionedItem::AddSection( const char *pText, int align )
{
	section_s sec;
	sec.pLabel = new vgui::Label( this, "Section", pText );
	SETUP_PANEL( sec.pLabel );
	sec.pLabel->SetContentAlignment( (vgui::Label::Alignment)align );
	sec.pLabel->SetTextInset( 10, 0 );
	sec.pLabel->SetBgColor( Color( 209, 112, 52, 128 ) );
	m_Sections.AddToTail( sec );
}

void CSectionedItem::SetActiveColumn( int col )
{
	for ( int i = 0; i < m_Sections.Count(); ++i )
	{
		m_Sections[i].pLabel->SetPaintBackgroundEnabled( i == col );
	}
}

//--------------------------------------------------------------------------------------
// Generic menu for Xbox 360 matchmaking dialogs. Contains a list of CMenuItems arranged
// vertically. The user can navigate the list using the controller and click on any
// item. A clicked item may send a command to the dialog and the dialog responds accordingly.
//--------------------------------------------------------------------------------------
CDialogMenu::CDialogMenu() : BaseClass( NULL, "DialogMenu" )
{
	// Quiet "parent not sized yet" spew
	SetSize( 100, 100 );

	m_pParent			= NULL;
	m_pHeader			= NULL;
	m_bUseFilter		= false;
	m_bHasHeader		= false;
	m_nItemSpacing		= 0;
	m_nMinWide			= 0;
	m_nActive			= -1;
	m_nActiveColumn		= -1;
	m_nBaseRowIdx		= 0;
	m_nBaseColumnIdx	= 0;
	m_iUnlocked			= 0;
	m_nMaxVisibleItems	= 1000;	// arbitrarily large	
	m_nMaxVisibleColumns = 1000;// arbitrarily large
}

CDialogMenu::~CDialogMenu()
{
	m_MenuItems.PurgeAndDeleteElements();
	delete m_pHeader;
}

void CDialogMenu::SetParent( CBaseDialog *pParent )
{
	BaseClass::SetParent( pParent );
	m_pParent = pParent;
}

//--------------------------------------------------------------------------------------
// Set a filter to use when reading in menu item keyvalues
//--------------------------------------------------------------------------------------
void CDialogMenu::SetFilter( const char *pFilter )
{
	if ( pFilter )
	{
		Q_strncpy( m_szFilter, pFilter, sizeof( m_szFilter ) );
		m_bUseFilter = true;
	}
	else
	{
		m_bUseFilter = false;
	}
}

//--------------------------------------------------------------------------------------
// Add a new menu item to the item array
//--------------------------------------------------------------------------------------
CMenuItem *CDialogMenu::AddItemInternal( CMenuItem *pItem )
{
	int idx = m_MenuItems.AddToTail( pItem );

 	SETUP_PANEL( pItem );

	return m_MenuItems[idx];
}

//--------------------------------------------------------------------------------------
// Add a new menu item of some type that derives from CMenuItem
//--------------------------------------------------------------------------------------
CCommandItem *CDialogMenu::AddCommandItem( const char *pTitleLabel, const char *pDescLabel, const char *pCommand )
{
	return (CCommandItem*)AddItemInternal( new CCommandItem( this, pTitleLabel, pDescLabel, pCommand ) );
}

CBrowserItem *CDialogMenu::AddBrowserItem( const char *pHost, const char *pPlayers, const char *pScenario, const char *pPing )
{
	// Results are added to the menu at runtime, so the layout needs to be updated after each addition.
	CBrowserItem *pItem = (CBrowserItem*)AddItemInternal( new CBrowserItem( this, pHost, pPlayers, pScenario, pPing ) );
	PerformLayout();
	return pItem;
}

COptionsItem *CDialogMenu::AddOptionsItem( const char *pLabel )
{
	return (COptionsItem*)AddItemInternal( new COptionsItem( this, pLabel ) );
}

CAchievementItem *CDialogMenu::AddAchievementItem( const wchar_t *pName, const wchar_t *pDesc, uint points, bool bUnlocked, IAchievement* pSourceAchievement  )
{
	return (CAchievementItem*)AddItemInternal( new CAchievementItem( this, pName, pDesc, points, bUnlocked, pSourceAchievement ) );
}

CSectionedItem *CDialogMenu::AddSectionedItem( const char **ppEntries, int ct )
{
	CSectionedItem *pItem = (CSectionedItem*)AddItemInternal( new CSectionedItem( this, ppEntries, ct ) );
	PerformLayout();
	return pItem;
}

CPlayerItem *CDialogMenu::AddPlayerItem( const char *pTitleLabel, int64 nId, byte bVoice, bool bReady )
{
	// Players are added to the lobby at runtime, so the layout needs to be updated after each addition.
	CPlayerItem *pItem = (CPlayerItem*)AddItemInternal( new CPlayerItem( this, pTitleLabel, nId, bVoice, bReady ) );
	PerformLayout();
	return pItem;
}

void CDialogMenu::RemovePlayerItem( int idx )
{
	delete m_MenuItems[idx];
	m_MenuItems.Remove( idx );
	PerformLayout();
}

void CDialogMenu::ClearItems()
{
	m_MenuItems.PurgeAndDeleteElements();
	InvalidateLayout();
}

//--------------------------------------------------------------------------------------
// Set the size an position of all the menu items
//--------------------------------------------------------------------------------------
void CDialogMenu::PerformLayout()
{
	BaseClass::PerformLayout();

	// Position the menu items and set their width
	int yPos = 0;
	int wide = GetWide();

	if ( m_bHasHeader )
	{
		yPos = 40;
		m_pHeader->SetPos( 0, 0 );
		m_pHeader->SetWide( wide );
		m_pHeader->PerformLayout();
	}

	for ( int i = 0; i < m_MenuItems.Count(); ++i )
	{
		CMenuItem *pItem = m_MenuItems[i];

		pItem->SetPos( 0, yPos );
		pItem->SetWide( wide );
		pItem->SetActiveColumn( m_nActiveColumn );
		pItem->PerformLayout();

		if ( i < m_nBaseRowIdx || i > m_nBaseRowIdx + m_nMaxVisibleItems - 1 )
		{
			pItem->SetVisible( false );
		}
		else
		{
			pItem->SetVisible( true );
			yPos += pItem->GetTall() + m_nItemSpacing;
		}
	}
	
	// Reset the focus to update background colors of all menu items
	SetFocus( m_nActive );


	SetTall( yPos );
}

//--------------------------------------------------------------------------------------
// Parse the res file for menu items to build out the dialog menu.
//--------------------------------------------------------------------------------------
void CDialogMenu::ApplySettings( KeyValues *pResourceData )
{
	BaseClass::ApplySettings( pResourceData );

	m_nItemSpacing		 = pResourceData->GetInt( "itemspacing", 2 );
	m_nMinWide			 = pResourceData->GetInt( "minwide", 0 );
	m_nActiveColumn		 = pResourceData->GetInt( "activecolumn", -1 );
	m_nMaxVisibleItems	 = pResourceData->GetInt( "maxvisibleitems", 1000 ); // arbitrarily large
	m_nMaxVisibleColumns = pResourceData->GetInt( "maxvisiblecolumns", 1000 ); // arbitrarily large

	KeyValues *pColumnData = pResourceData->FindKey( "Columns" );
	if ( pColumnData )
	{
		int xPos = 0;
		int idx = 0;
		const char *ppHeader[MAX_COLUMNS];
		for ( KeyValues *pColumn = pColumnData->GetFirstSubKey(); pColumn != NULL; pColumn = pColumn->GetNextKey() )
		{
			if ( !Q_stricmp( pColumn->GetName(), "Column" ) )
			{
				columninfo_s col;
				col.bSortDown	= true;
				col.xpos		= pColumn->GetInt( "xpos", xPos );
				col.ypos		= pColumn->GetInt( "ypos", 0 );
				col.wide		= pColumn->GetInt( "wide", 0 );
				col.align		= pColumn->GetInt( "align", 3 ); // west by default
				col.bLocked		= pColumn->GetInt( "locked", 0 );
				col.hFont		= m_pScheme->GetFont( pColumn->GetString( "font", "default" ) );
				col.color		= m_pScheme->GetColor( pColumn->GetString( "fgcolor" ), Color( 0, 0, 0, 255 ) );
				
				ppHeader[idx++] = pColumn->GetString( "header", "" );

				xPos = col.xpos + col.wide;
				m_Columns.AddToTail( col );

				if ( col.bLocked )
				{
					m_nBaseColumnIdx = idx;
					m_iUnlocked = idx;
				}
			}
		}
		m_bHasHeader = true;
		m_pHeader = new CSectionedItem( this, ppHeader, idx );
		m_pHeader->m_bHeader = true;
		SETUP_PANEL( m_pHeader );

		m_pHeader->SetPaintBackgroundEnabled( false );
		vgui::HFont headerFont = m_pScheme->GetFont( pColumnData->GetString( "headerfont", "default" ) );
		Color headerColor = m_pScheme->GetColor( pColumnData->GetString( "headerfgcolor" ), Color( 0, 0, 0, 255 ) );
		for ( int i = 0; i < idx; ++i )
		{
			vgui::Label *pLabel = m_pHeader->m_Sections[i].pLabel;
			pLabel->SetFont( headerFont );
			pLabel->SetFgColor( headerColor );
			pLabel->SetPaintBackgroundEnabled( false );
		}
	}

	for ( KeyValues *pMenuData = pResourceData->GetFirstSubKey(); pMenuData != NULL; pMenuData = pMenuData->GetNextKey() )
	{
		// See if we should skip over this block
		if ( m_bUseFilter )
		{
			if ( pMenuData->GetInt( m_szFilter, 0 ) == 0 )
				continue;
		}

		// Give our parent a chance to change the properties of this item
		m_pParent->OverrideMenuItem( pMenuData );

		if ( !Q_stricmp( pMenuData->GetName(), "CommandItem" ) )
		{
			// New Command Item
			const char *label		= pMenuData->GetString( "label", "<unknown>" );
			const char *description = pMenuData->GetString( "description", NULL );
			const char *command		= pMenuData->GetString( "command", "<unknown>" );

			AddCommandItem( label, description, command );
		}
		else if ( !Q_stricmp( pMenuData->GetName(), "OptionsItem" ) )
		{
			// New Options Item
			COptionsItem *pItem = AddOptionsItem( pMenuData->GetString( "label", "<unknown>" ) );
 
			// ID and ValueType and the same for all option values
			const char *pID			= pMenuData->GetString( "id", "NULL" );
			const char *pValueType	= pMenuData->GetString( "valuetype", NULL );

			// Add all the options
			for ( KeyValues *pValue = pMenuData->GetFirstSubKey(); pValue != NULL; pValue = pValue->GetNextKey() )
			{
				if ( !Q_stricmp( pValue->GetName(), "Option" ) )
				{
					sessionProperty_t prop;
					prop.nType = SESSION_CONTEXT;
					Q_strncpy( prop.szID, pID, sizeof( prop.szID ) );
					Q_strncpy( prop.szValue, pValue->GetString( "value", "NULL" ), sizeof( prop.szValue ) );

					if ( pValueType )
					{
						// Only session properties have a type
						prop.nType = SESSION_PROPERTY;
						Q_strncpy( prop.szValueType, pValueType, sizeof( prop.szValueType ) );
					}

					const char *pLabel = pValue->GetString( "label", "<unknown>" );
					pItem->AddOption( pLabel, prop );
				}
			}

			// Add range items after the specified items
			if ( pMenuData->GetInt( "userange" ) )
			{
				// Options are an implicit range of integers
				int nStart		= pMenuData->GetInt( "rangelow" );
				int nEnd		= pMenuData->GetInt( "rangehigh" );
				int nInterval	= pMenuData->GetInt( "interval", 1 );

				// Prevent total destruction from a bad resource file
				if ( nEnd < nStart )
				{
					nEnd = nStart;
				}

				for ( int i = nStart; i <= nEnd; i += nInterval )
				{
					sessionProperty_t prop;
					prop.nType = SESSION_PROPERTY;
					Q_strncpy( prop.szID, pID, sizeof( prop.szID ) );
					Q_strncpy( prop.szValueType, pValueType, sizeof( prop.szValueType ) );
					Q_snprintf( prop.szValue, sizeof(prop.szValue), "%d", i ); 

					pItem->AddOption( prop.szValue, prop );
				}
			}
  
			// Set the default active option
  			int active = pMenuData->GetInt( "activeoption", 0 );
  			pItem->SetOptionFocus( active );

			// Notify our parent that each option has been set to its current setting
			KeyValues *kv = new KeyValues( "MenuItemChanged", "item", GetItemCount() - 1 );
			PostActionSignal( kv );
		}
	}

	// Calculate the final menu size according to the widest menu item
	int wide = m_nMinWide;
	for ( int i = 0; i < m_MenuItems.Count(); ++i )
	{
		wide = max( wide, m_MenuItems[i]->GetWide() );
	}
	SetWide( wide );
}

//--------------------------------------------------------------------------------------
// Cache off the scheme
//--------------------------------------------------------------------------------------
void CDialogMenu::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pScheme = pScheme;
}

//--------------------------------------------------------------------------------------
// Give focus (highlights) a particular menu item by index
//--------------------------------------------------------------------------------------
void CDialogMenu::SetFocus( int idx )
{
	int itemCt = (unsigned int)m_MenuItems.Count();
	if ( idx >= itemCt )
		return;

	for ( int i = 0; i < itemCt; ++i )
	{
		m_MenuItems[i]->SetFocus( i == idx );
	}
	m_nActive = idx;

	if ( m_nActive >= 0 && m_nActive < m_nBaseRowIdx )
	{
		m_nBaseRowIdx = m_nActive;
	}
	else if ( m_nActive > m_nBaseRowIdx + m_nMaxVisibleItems - 1 )
	{
		m_nBaseRowIdx = m_nActive - ( m_nMaxVisibleItems - 1 );
	}
}

//--------------------------------------------------------------------------------------
// Sort the menu items according to the selected column
//--------------------------------------------------------------------------------------
void CDialogMenu::SortMenuItems()
{
	if ( !m_bHasHeader )
		return;

	// Simple bubble sort
	char szBufferOne[32];
	char szBufferTwo[32];
	bool bSortDown = GetColumnSortType( m_nActiveColumn );
	for ( int i = 1; i <= m_MenuItems.Count(); ++i )
	{
		for ( int j = 0; j < m_MenuItems.Count() - i; ++j )
		{
			((CSectionedItem*)m_MenuItems[j])->m_Sections[m_nActiveColumn].pLabel->GetText( szBufferOne, sizeof( szBufferOne ) );
			((CSectionedItem*)m_MenuItems[j+1])->m_Sections[m_nActiveColumn].pLabel->GetText( szBufferTwo, sizeof( szBufferTwo ) );

			int diff = Q_stricmp( szBufferOne, szBufferTwo );
			bool bSwap = bSortDown ? diff > 0 : diff < 0;
			if ( bSwap )
			{
				CMenuItem *pTemp = m_MenuItems[j+1];
				m_MenuItems[j+1] = m_MenuItems[j];
				m_MenuItems[j] = pTemp;

				m_pParent->SwapMenuItems( j, j+1 );
			}
		}
	}
	InvalidateLayout();
}

//--------------------------------------------------------------------------------------
// Move item focus to the next item in the menu - supports wrapping
//--------------------------------------------------------------------------------------
void CDialogMenu::SetFocusNext()
{
	if ( m_MenuItems.Count() )
	{
		int iNewIndex = ( m_nActive + 1 ) % m_MenuItems.Count();

		int i = 0;
		bool bSet = false;
		while ( i < m_MenuItems.Count() )
		{
			if ( m_MenuItems[iNewIndex]->IsEnabled() )
			{
				SetFocus( iNewIndex );
				bSet = true;
				break;
			}

			iNewIndex = ( iNewIndex + 1 ) % m_MenuItems.Count();
			i++;
		}

		InvalidateLayout();
	}
}

//--------------------------------------------------------------------------------------
// Move item focus to the previous item in the menu - supports wrapping
//--------------------------------------------------------------------------------------
void CDialogMenu::SetFocusPrev()
{
	if ( m_MenuItems.Count() )
	{
		int iNewIndex = m_nActive - 1;
		if ( iNewIndex < 0 )
			iNewIndex = m_MenuItems.Count() - 1;

		int i = 0;
		bool bSet = false;
		while ( i < m_MenuItems.Count() )
		{
			if ( m_MenuItems[iNewIndex]->IsEnabled() )
			{
				SetFocus( iNewIndex );
				bSet = true;
				break;
			}

			if ( --iNewIndex < 0 )
				iNewIndex = m_MenuItems.Count() - 1;

			i++;
		}

		InvalidateLayout();
	}
}

//--------------------------------------------------------------------------------------
// For OptionsItems: Move focus to the next option in the menu item - does not wrap
//--------------------------------------------------------------------------------------
void CDialogMenu::SetOptionFocusNext()
{
	COptionsItem *pItem = dynamic_cast< COptionsItem* >( GetItem( m_nActive ) );
	if ( pItem )
	{
		pItem->SetOptionFocusNext();

		KeyValues *kv = new KeyValues( "MenuItemChanged", "item", m_nActive );
		PostActionSignal( kv );
	}
}

//--------------------------------------------------------------------------------------
// For OptionsItems: Move focus to the previous option in the menu item - does not wrap
//--------------------------------------------------------------------------------------
void CDialogMenu::SetOptionFocusPrev()
{
	COptionsItem *pItem = dynamic_cast< COptionsItem* >( GetItem( m_nActive ) );
	if ( pItem )
	{
		pItem->SetOptionFocusPrev();

		KeyValues *kv = new KeyValues( "MenuItemChanged", "item", m_nActive );
		PostActionSignal( kv );
	}
}

//--------------------------------------------------------------------------------------
// For OptionsItems: Update the base index for the columns
//--------------------------------------------------------------------------------------
void CDialogMenu::UpdateBaseColumnIndex()
{
	if ( m_iUnlocked + m_nActiveColumn - m_nBaseColumnIdx >= m_nMaxVisibleColumns )
	{
		m_nBaseColumnIdx = m_iUnlocked + m_nActiveColumn - m_nMaxVisibleColumns + 1;
	}
	else if ( m_nActiveColumn - m_nBaseColumnIdx < 0 )
	{
		m_nBaseColumnIdx = m_nActiveColumn;
	}
}

//--------------------------------------------------------------------------------------
// For menus with sectioned columns - move focus to the next column
//--------------------------------------------------------------------------------------
void CDialogMenu::SetColumnFocusNext()
{
	if ( m_nActiveColumn == -1 )
		return;

	if ( m_Columns.Count() )
	{
		int iNewColumn = m_nActiveColumn + 1;
		if ( iNewColumn >= m_Columns.Count() )
			return;

		m_nActiveColumn = iNewColumn;	
		UpdateBaseColumnIndex();
		InvalidateLayout();
	}
}

//--------------------------------------------------------------------------------------
// For menus with sectioned columns - move focus to the next column
//--------------------------------------------------------------------------------------
void CDialogMenu::SetColumnFocusPrev()
{
	if ( m_nActiveColumn == -1 )
		return;

	if ( m_Columns.Count() )
	{
		int iNewColumn = m_nActiveColumn - 1;
		if ( iNewColumn < 0 || m_Columns[iNewColumn].bLocked )
			return;

		m_nActiveColumn = iNewColumn;
		UpdateBaseColumnIndex();
		InvalidateLayout();
	}
}

//--------------------------------------------------------------------------------------
// For OptionsItems: Lets the dialog find out which option is currently selected
//--------------------------------------------------------------------------------------
int	CDialogMenu::GetActiveOptionIndex( int nMenuItemIdx )
{
	int retval = -1;
	COptionsItem *pItem = dynamic_cast< COptionsItem* >( GetItem( nMenuItemIdx ) );
	if ( pItem )
	{
		retval = pItem->GetActiveOptionIndex();
	}
	return retval;
}

//-----------------------------------------------------------------------
// Return the index of the current active menu item
//-----------------------------------------------------------------------
int CDialogMenu::GetActiveItemIndex()
{
	return m_nActive;
}

//-----------------------------------------------------------------------
// Return the index of the current active menu column
//-----------------------------------------------------------------------
int CDialogMenu::GetActiveColumnIndex()
{
	return m_nActiveColumn;
}

//-----------------------------------------------------------------------
// Return the number of menu items
//-----------------------------------------------------------------------
int CDialogMenu::GetItemCount()
{
	return m_MenuItems.Count();
}

//-----------------------------------------------------------------------
// Return the number of visible menu items
//-----------------------------------------------------------------------
int CDialogMenu::GetVisibleItemCount()
{
	return min( GetItemCount(), m_nMaxVisibleItems );
}

//-----------------------------------------------------------------------
// Return the number of visible menu columns
//-----------------------------------------------------------------------
int CDialogMenu::GetVisibleColumnCount()
{
	return m_nMaxVisibleColumns;
}

//-----------------------------------------------------------------------
// Return the index of the first unlocked column
//-----------------------------------------------------------------------
int CDialogMenu::GetFirstUnlockedColumnIndex()
{
	return m_iUnlocked;
}

//-----------------------------------------------------------------------
// Return the first visible index in the menu
//-----------------------------------------------------------------------
int CDialogMenu::GetBaseRowIndex()
{
	return m_nBaseRowIdx;
}

//-----------------------------------------------------------------------
// Set the first visible index in the menu
//-----------------------------------------------------------------------
void CDialogMenu::SetBaseRowIndex( int idx )
{
	m_nBaseRowIdx = idx;
}

//-----------------------------------------------------------------------
// Return the specified menu item
//-----------------------------------------------------------------------
CMenuItem *CDialogMenu::GetItem( int idx )
{
	if ( m_MenuItems.IsValidIndex( idx ) )
	{
		return m_MenuItems[idx];
	}
	return NULL;
}

//-----------------------------------------------------------------------
// Return the specified column xpos
//-----------------------------------------------------------------------
int	CDialogMenu::GetColumnXPos( int idx )
{
	// Compensate for scrolling offsets
	columninfo_s &col = m_Columns[idx];
	
	int xpos;
	if ( col.bLocked )
	{
		xpos = m_Columns[idx].xpos;
	}
	else
	{
		int trueIdx = m_iUnlocked + idx - m_nBaseColumnIdx;
		if ( trueIdx < m_iUnlocked )
		{
			// Put it offscreen
			xpos = -100 - col.wide;
		}
		else
		{
			xpos = m_Columns[trueIdx].xpos;
		}
	}
	return xpos;
}

//-----------------------------------------------------------------------
// Return the specified column ypos
//-----------------------------------------------------------------------
int	CDialogMenu::GetColumnYPos( int idx )
{
	return m_Columns[idx].ypos;
}

//-----------------------------------------------------------------------
// Return the specified column width
//-----------------------------------------------------------------------
int	CDialogMenu::GetColumnWide( int idx )
{
	return m_Columns[idx].wide;
}

//-----------------------------------------------------------------------
// Return the specified column alignment
//-----------------------------------------------------------------------
int CDialogMenu::GetColumnAlignment( int idx )
{
	return m_Columns[idx].align;
}

//-----------------------------------------------------------------------
// Return the specified column font
//-----------------------------------------------------------------------
HFont CDialogMenu::GetColumnFont( int idx )
{
	return m_Columns[idx].hFont;
}

//-----------------------------------------------------------------------
// Return the specified column fgcolor
//-----------------------------------------------------------------------
Color CDialogMenu::GetColumnColor( int idx )
{
	return m_Columns[idx].color;
}

//-----------------------------------------------------------------------
// Return the specified column fgcolor
//-----------------------------------------------------------------------
bool CDialogMenu::GetColumnSortType( int idx )
{
	bool bSortDown = m_Columns[idx].bSortDown;
	m_Columns[idx].bSortDown = !bSortDown;
	return bSortDown;
}

//--------------------------------------------------------------------------------------
// Receive the command from a clicked menu item and forwards it to the parent dialog
//--------------------------------------------------------------------------------------
void CDialogMenu::OnCommand( const char *pCommand )
{
	GetParent()->OnCommand( pCommand );
}

//--------------------------------------------------------------------------------------
// Update the menu state according to controller input.
// Returns whether or not the keycode was handled by the menu.
//--------------------------------------------------------------------------------------
bool CDialogMenu::HandleKeyCode( vgui::KeyCode code )
{
	switch( code )
	{
	case KEY_XBUTTON_DOWN:
	case KEY_XSTICK1_DOWN:
	case STEAMCONTROLLER_DPAD_DOWN:
		SetFocusNext();
		break;

	case KEY_XBUTTON_UP:
	case KEY_XSTICK1_UP:
	case STEAMCONTROLLER_DPAD_UP:
		SetFocusPrev();
		break;

	case KEY_XBUTTON_LEFT:
	case KEY_XSTICK1_LEFT:
	case STEAMCONTROLLER_DPAD_LEFT:
		SetOptionFocusPrev();
		SetColumnFocusPrev();
		break;

	case KEY_XBUTTON_RIGHT:
	case KEY_XSTICK1_RIGHT:
	case STEAMCONTROLLER_DPAD_RIGHT:
		SetOptionFocusNext();
		SetColumnFocusNext();
		break;

	case KEY_XBUTTON_A:
	case STEAMCONTROLLER_A:
		if ( m_MenuItems.Count() && m_nActive >= 0 )
		{
			m_MenuItems[m_nActive]->OnClick();
		}
		break;

	case KEY_XBUTTON_Y:
	case STEAMCONTROLLER_Y:
		SortMenuItems();
		break;

	default:
		// Not handled
		return false;
	}

	return true;
}

