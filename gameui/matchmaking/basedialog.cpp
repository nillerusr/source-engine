//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: All matchmaking dialogs inherit from this
//
//=============================================================================//

#include "vgui_controls/Label.h"
#include "GameUI_Interface.h"
#include "KeyValues.h"
#include "basedialog.h"
#include "BasePanel.h"
#include "matchmakingbasepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------
// CBaseDialog
//---------------------------------------------------------
CBaseDialog::CBaseDialog( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{
	SetTitleBarVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( false );

	m_pParent = pParent;
	m_Menu.SetParent( this );

	m_pTitle = new vgui::Label( this, "DialogTitle", "" );
	m_pFooterInfo = NULL;
	
	m_nBorderWidth = 0;
	m_nButtonGap = -1;
}

CBaseDialog::~CBaseDialog()
{
	delete m_pTitle;

	if ( m_pFooterInfo )
	{
		m_pFooterInfo->deleteThis();
	}
}

//---------------------------------------------------------
// Purpose: Activate the dialog
//---------------------------------------------------------
void CBaseDialog::Activate( void )
{
	BaseClass::Activate();

	InvalidateLayout( false, false );
}

//---------------------------------------------------------
// Purpose: Set the title and menu positions
//---------------------------------------------------------
void CBaseDialog::PerformLayout( void )
{
	BaseClass::PerformLayout();

	m_pTitle->SizeToContents();

	int menux, menuy;
	m_Menu.GetPos( menux, menuy );

	int autoWide = m_Menu.GetWide() + m_nBorderWidth * 2;
	int autoTall = menuy + m_Menu.GetTall() + m_nBorderWidth;
	autoWide = max( autoWide, GetWide() );
	autoTall = max( autoTall, GetTall() );

	SetSize( autoWide, autoTall );

	if ( m_pFooterInfo && m_pParent )
	{
		CMatchmakingBasePanel *pBasePanel = dynamic_cast< CMatchmakingBasePanel* >( m_pParent );
		if ( pBasePanel )
		{
			// the base panel is our parent
			pBasePanel->SetFooterButtons( this, m_pFooterInfo, m_nButtonGap );
		}
	}

	if ( m_Menu.GetActiveItemIndex() == -1 )
	{
		m_Menu.SetFocus( 0 );
	}
}

//---------------------------------------------------------
// Purpose: Setup sizes and positions
//---------------------------------------------------------
void CBaseDialog::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_nBorderWidth	= inResourceData->GetInt( "borderwidth", 0 );

	KeyValues *pFooter = inResourceData->FindKey( "Footer" );
	if ( pFooter )
	{
		m_pFooterInfo = pFooter->MakeCopy();
	}

	m_nButtonGap = inResourceData->GetInt( "footer_buttongap", -1 );
}

//---------------------------------------------------------
// Purpose: Setup colors and fonts
//---------------------------------------------------------
void CBaseDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundType( 2 );

	m_pTitle->SetFgColor( pScheme->GetColor( "MatchmakingDialogTitleColor", Color( 200, 184, 151, 255 ) ) );

	char szResourceName[MAX_PATH];
	Q_snprintf( szResourceName, sizeof( szResourceName ), "%s.res", GetName() );
	KeyValues *pKeys = BasePanel()->GetConsoleControlSettings()->FindKey( szResourceName );
	LoadControlSettings( "NULL", NULL, pKeys );
}

//-----------------------------------------------------------------
// Purpose: Set the resource file to load this dialog's settings
//-----------------------------------------------------------------
void CBaseDialog::OnClose()
{
	// Hide the rather ugly fade out
	SetAlpha( 0 );
	BaseClass::OnClose();
}

//-----------------------------------------------------------------
// Purpose: Change properties of a menu item
//-----------------------------------------------------------------
void CBaseDialog::OverrideMenuItem( KeyValues *pKeys )
{
	// Do nothing
}

//-----------------------------------------------------------------
// Purpose: Swap the order of two menu items
//-----------------------------------------------------------------
void CBaseDialog::SwapMenuItems( int iOne, int iTwo )
{
	// Do nothing
}

//-----------------------------------------------------------------
// Purpose: Send key presses to the dialog's menu
//-----------------------------------------------------------------
void CBaseDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( code == KEY_XBUTTON_START )
	{
		m_KeyRepeat.Reset();
		if ( GameUI().IsInLevel() )
		{
			m_pParent->OnCommand( "ResumeGame" );
		}
		return;
	}

	m_KeyRepeat.KeyDown( code );

	// Send down to the menu
	if ( !m_Menu.HandleKeyCode( code ) )
	{
		if ( code == KEY_XBUTTON_B )
		{
			OnCommand( "DialogClosing" );
			SetDeleteSelfOnClose( true );
		}
		else
		{
			BaseClass::OnKeyCodePressed( code );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDialog::OnKeyCodeReleased( vgui::KeyCode code )
{
	m_KeyRepeat.KeyUp( code );

	BaseClass::OnKeyCodeReleased( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDialog::HandleKeyRepeated( vgui::KeyCode code )
{
	m_Menu.HandleKeyCode( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseDialog::OnThink()
{
	vgui::KeyCode code = m_KeyRepeat.KeyRepeated();
	if ( code )
	{
		if ( HasFocus() )
		{
			HandleKeyRepeated( code );
		}
		else
		{
			// This can happen because of the slight delay after selecting a 
			// menu option and the resulting action. The selection caused the
			// key repeater to be reset, but the player can press a movement
			// key before the action occurs, leaving us with a key repeating
			// on this dialog even though it no longer has focus.
			m_KeyRepeat.Reset();
		}
	}

	BaseClass::OnThink();
}

//-----------------------------------------------------------------
// Purpose: Forward commands to the matchmaking base panel
//-----------------------------------------------------------------
void CBaseDialog::OnCommand( const char *pCommand )
{
	m_KeyRepeat.Reset();
	m_pParent->OnCommand( pCommand );
}


//---------------------------------------------------------------------
// Helper object to display the map picture and descriptive text
//---------------------------------------------------------------------
CScenarioInfoPanel::CScenarioInfoPanel( vgui::Panel *parent, const char *pName ) : BaseClass( parent, pName )
{
	m_pMapImage = new vgui::ImagePanel( this, "MapImage" );
	m_pTitle = new CPropertyLabel( this, "Title", "" );
	m_pSubtitle = new CPropertyLabel( this, "Subtitle", "" );

	m_pDescOne = new CPropertyLabel( this, "DescOne", "" );
	m_pDescTwo = new CPropertyLabel( this, "DescTwo", "" );
	m_pDescThree = new CPropertyLabel( this, "DescThree", "" );

	m_pValueTwo = new CPropertyLabel( this, "ValueTwo", "" );
	m_pValueThree = new CPropertyLabel( this, "ValueThree", "" );
}
CScenarioInfoPanel::~CScenarioInfoPanel()
{
	delete m_pMapImage;
	delete m_pTitle;
	delete m_pSubtitle;
	delete m_pDescOne;
	delete m_pDescTwo;
	delete m_pDescThree;
	delete m_pValueTwo;
	delete m_pValueThree;
}

void CScenarioInfoPanel::PerformLayout( void )
{
	BaseClass::PerformLayout();
}

void CScenarioInfoPanel::ApplySettings( KeyValues *pResourceData )
{
	BaseClass::ApplySettings( pResourceData );
}

void CScenarioInfoPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	Color fontColor = pScheme->GetColor( "MatchmakingDialogTitleColor", Color( 0, 0, 0, 255 ) );
	m_pTitle->SetFgColor( fontColor );
	m_pSubtitle->SetFgColor( fontColor );
	m_pDescOne->SetFgColor( fontColor );
	m_pDescTwo->SetFgColor( fontColor );
	m_pDescThree->SetFgColor( fontColor );
	m_pValueTwo->SetFgColor( fontColor );
	m_pValueThree->SetFgColor( fontColor );

	SetPaintBackgroundType( 2 );

	KeyValues *pKeys = BasePanel()->GetConsoleControlSettings()->FindKey( "ScenarioInfoPanel.res" );
	ApplySettings( pKeys );
}

DECLARE_BUILD_FACTORY( CScenarioInfoPanel );
