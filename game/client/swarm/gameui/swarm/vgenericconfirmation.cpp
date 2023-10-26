//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VGenericConfirmation.h"

#include "vgui_controls/Label.h"
#include "vgui/ISurface.h"
#include "nb_button.h"
#include "cdll_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

int GenericConfirmation::sm_currentUsageId = 0;

GenericConfirmation::Data_t::Data_t() : 
	pWindowTitle( NULL ),
	pMessageText( NULL ),
	pMessageTextW( NULL ),
	bOkButtonEnabled( false ),
	pfnOkCallback( NULL ),
	bCancelButtonEnabled( false ),
	pfnCancelCallback( NULL ),
	bCheckBoxEnabled( false ),
	pCheckBoxLabelText( NULL ),
	pCheckBoxCvarName( NULL )
{
}

//=============================================================================
GenericConfirmation::GenericConfirmation( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, true, false ),
	m_pLblMessage( 0 ),
	m_pLblCheckBox( 0 ),
	m_data(),
	m_usageId( 0 ),
	m_pCheckBox( 0 )
{
	SetProportional( true );

	m_pLblOkButton = new vgui::Label( this, "LblOkButton", "#GameUI_Icons_A_3DBUTTON" );
	m_pLblOkText = new vgui::Label( this, "LblOkText", "#L4D360UI_Ok" );
	m_pLblCancelButton = new vgui::Label( this, "LblCancelButton", "#GameUI_Icons_B_3DBUTTON" );
	m_pLblCancelText = new vgui::Label( this, "LblCancelText", "#L4D360UI_Cancel" );
	m_pPnlLowerGarnish = new vgui::Panel( this, "PnlLowerGarnish" );

	m_pBtnOK = new CNB_Button( this, "BtnOK", "", this, "OK" );
	m_pBtnCancel = new CNB_Button( this, "BtnCancel", "", this, "cancel" );

	SetTitle( "", false );
	SetDeleteSelfOnClose( true );
	SetLowerGarnishEnabled( false );
	SetMoveable( false );
}

//=============================================================================
GenericConfirmation::~GenericConfirmation()
{
	delete m_pLblMessage;
	delete m_pLblOkButton;
	delete m_pLblOkText;
	delete m_pLblCancelButton;
	delete m_pLblCancelText;
	delete m_pPnlLowerGarnish;
}

//=============================================================================
void GenericConfirmation::OnCommand(const char *command)
{
	if ( Q_stricmp( command, "OK" ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else if ( Q_stricmp( command, "cancel" ) == 0 )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
}

//=============================================================================
void GenericConfirmation::OnKeyCodePressed( KeyCode keycode )
{
	int userId = GetJoystickForCode( keycode );
	vgui::KeyCode code = GetBaseButtonCode( keycode );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( userId );

	switch ( code )
	{
	case KEY_XBUTTON_A:
		if ( m_OkButtonEnabled )
		{
			if ( m_pCheckBox )
			{
				m_pCheckBox->ApplyChanges();
			}

			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );
			if ( !NavigateBack() )
			{
				Close();
			}

			if ( m_data.pfnOkCallback != 0 )
			{
				m_data.pfnOkCallback();
			}
		}
		break;

	case KEY_XBUTTON_B:
		if ( m_CancelButtonEnabled )
		{
			if ( m_pCheckBox )
			{
				m_pCheckBox->ApplyChanges();
			}

			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_BACK );
			if ( !NavigateBack() )
			{
				Close();
			}

			if ( m_data.pfnCancelCallback != 0 )
			{
				m_data.pfnCancelCallback();
			}
		}
		break;
	default:
		BaseClass::OnKeyCodePressed(keycode);
		break;
	}
}

#ifndef _X360
void GenericConfirmation::OnKeyCodeTyped( vgui::KeyCode code )
{
	// For PC, this maps space bar and enter to OK and esc to cancel
	switch ( code )
	{
	case KEY_SPACE:
	case KEY_ENTER:
		return OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );

	case KEY_ESCAPE:
		return OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}

	BaseClass::OnKeyTyped( code );
}
#endif

void GenericConfirmation::OnOpen( )
{
	BaseClass::OnOpen();

	m_bNeedsMoveToFront = true;
}

void ExpandButtonWidthIfNecessary( vgui::Button *pButton )
{
	int originalWide, originalTall;
	pButton->GetSize( originalWide, originalTall );

	pButton->SizeToContents();

	// restore tall
	pButton->SetTall( originalTall );

	const int iMinButtonWidth = MAX( originalWide, scheme()->GetProportionalScaledValue( 45 ) );

	// if we're smaller than original wide, restore wide
	if ( pButton->GetWide() < iMinButtonWidth )
	{
		pButton->SetWide( originalWide );
	}
}

//=============================================================================
void GenericConfirmation::LoadLayout()
{
	BaseClass::LoadLayout();

	int screenWidth, screenHeight;
	CBaseModPanel::GetSingleton().GetSize( screenWidth, screenHeight );

	int dialogWidth = ScreenWidth(); //vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 100 );
	int dialogHeight = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 66 );

	// need a border gap to inset all controls
	int borderGap = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 8 );

	int checkBoxGap = 0;

	// first solve the size of the parent window
	int titleWide = 0;
	int titleTall = 0;
	vgui::Label *pLblTitle = dynamic_cast< vgui::Label* >( FindChildByName( "LblTitle" ) );
	if ( pLblTitle )
	{
		// account for size of the title and a gap
		pLblTitle->GetContentSize( titleWide, titleTall );
		int shim = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), IsPC() ? 90 : 15 );
		if ( dialogWidth < titleWide + shim )
		{
			dialogWidth = titleWide + shim;
		}
	}

	if ( m_pLblMessage )
	{
		// account for the size of the message and a gap
		int msgWide, msgTall;
		m_pLblMessage->GetContentSize( msgWide, msgTall );

		if ( msgWide > screenWidth - 100 )
		{
			m_pLblMessage->SetWrap( true );
			m_pLblMessage->SetWide( screenWidth - 100 );
			m_pLblMessage->SetTextInset( 0, 0 );
			m_pLblMessage->GetContentSize( msgWide, msgTall );
		}

		int shimX = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), IsPC() ? 80 : 20 );
		int shimY = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 50 );
		if ( dialogWidth < msgWide + shimX )
		{
			dialogWidth = msgWide + shimX;
		}
		if ( dialogHeight < msgTall + shimY )
		{
			dialogHeight = msgTall + shimY;
		}

		if ( m_pCheckBox )
		{
			int boxWide, boxTall;
			m_pCheckBox->GetContentSize( boxWide, boxTall );
			checkBoxGap = boxTall * 2;
			dialogHeight += checkBoxGap;
		}
	}


	// In the Xbox, OK/Cancel xbox buttons use the same font and are the same size, use the OK button
	int buttonWide = 0;
	int buttonTall = 0;
	if ( IsX360() )
	{
		m_pLblOkButton->GetContentSize( buttonWide, buttonTall );
	}
	// On the PC, the buttons will be the same size, use the OK button
	vgui::Button *pOkButton = NULL;
	vgui::Button *pCancelButton = NULL;
	if ( IsPC() )
	{
		pOkButton = dynamic_cast< vgui::Button* >( FindChildByName( "BtnOk" ) );
		pCancelButton = dynamic_cast< vgui::Button* >( FindChildByName( "BtnCancel" ) );
		pOkButton->GetSize( buttonWide, buttonTall );
	}

	if ( m_data.bOkButtonEnabled || m_data.bCancelButtonEnabled )
	{
		// account for the buttons
		dialogHeight += vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), buttonTall );
		dialogHeight += borderGap;
	}

	// don't get smaller than a minimum width
	int minWidth = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 235 );
	if ( dialogWidth < minWidth )
		dialogWidth = minWidth;

	// now have final dialog dimensions, center the dialog
	SetPos( ( screenWidth - dialogWidth ) / 2, ( screenHeight - dialogHeight ) / 2 );
	SetSize( dialogWidth, dialogHeight );
	//SetPos( screenWidth, ( screenHeight - dialogHeight ) / 2 );
	//SetSize( screenWidth, dialogHeight );

	if ( pLblTitle )
	{
		// horizontally center and vertically inset the title
		pLblTitle->SetPos( ( dialogWidth - titleWide ) / 2, borderGap );
		pLblTitle->SetSize( titleWide, titleTall );
	}

	if ( m_pLblMessage )
	{
		// center the message
		int msgWide, msgTall;
		m_pLblMessage->GetContentSize( msgWide, msgTall );
		m_pLblMessage->SetPos( ( dialogWidth - msgWide ) / 2, ( dialogHeight - checkBoxGap - msgTall ) / 2 );
		m_pLblMessage->SetSize( msgWide, msgTall );

		if ( m_pCheckBox )
		{
			int boxWide, boxTall;
			m_pCheckBox->GetContentSize( boxWide, boxTall );
			m_pCheckBox->SetVisible( true );

			int boxX = ( dialogWidth - boxWide ) / 2;
			int boxY = ( dialogHeight - checkBoxGap - msgTall ) / 2 + msgTall + checkBoxGap / 2;

			if ( m_pLblCheckBox )
			{
				int lblCheckBoxWide, lblCheckBoxTall;
				m_pLblCheckBox->GetContentSize( lblCheckBoxWide, lblCheckBoxTall );
				boxX = ( dialogWidth - ( boxWide + lblCheckBoxWide ) ) / 2;

				m_pLblCheckBox->SetVisible( true );
				m_pLblCheckBox->SetPos( boxX + boxWide, boxY );
				m_pLblCheckBox->SetSize( lblCheckBoxWide, lblCheckBoxTall );
			}
			
			m_pCheckBox->SetPos( boxX, boxY );
		}

	}

	if ( IsX360() )
	{
		// the OK/Cancel button icons and text are for 360 only
		// they just refer to the A/B buttons on controller
		m_pLblCancelButton->SetVisible( m_data.bCancelButtonEnabled );
		m_pLblCancelText->SetVisible( m_data.bCancelButtonEnabled );
		m_pLblOkButton->SetVisible( m_data.bOkButtonEnabled );
		m_pLblOkText->SetVisible( m_data.bOkButtonEnabled );
	
		if ( m_data.bCancelButtonEnabled || m_data.bOkButtonEnabled )
		{
			// The xbox has to assemble a graphic icon and text to the right of it.
			// Calculate the positions for proper centering. The scripts can't do this.
			// The buttons are the same size, but the text size varies, thus the calculations.
			// Before I got to it, it was hardcoded, and it looked noob (crap) aligned in every case except for 1.
			// The buttons are aligned to the left and right bottom edges
			int buttonY = dialogHeight - borderGap - buttonTall;
			int textY = buttonY + buttonTall/2;

			// when only one button is enabled, center that button
			vgui::Label *pLblButton = NULL;
			vgui::Label *pLblText = NULL;
			bool bSingleButton = false;
			if (  m_data.bCancelButtonEnabled && !m_data.bOkButtonEnabled )
			{
				// cancel is centered
				bSingleButton = true;
				pLblButton = m_pLblCancelButton;
				pLblText = m_pLblCancelText;
			}
			else if ( !m_data.bCancelButtonEnabled && m_data.bOkButtonEnabled )
			{
				// OK is centered
				bSingleButton = true;
				pLblButton = m_pLblOkButton;
				pLblText = m_pLblOkText;
			}
			
			if ( bSingleButton )
			{
				// center the button and its text
				// only the cancel button should be visible, center CANCEL
				int textWide, textTall;
				pLblText->GetContentSize( textWide, textTall );
				int totalWidth = 5*buttonWide/4 + textWide;
				int buttonX = ( dialogWidth - totalWidth ) / 2;
				pLblButton->SetPos( buttonX, buttonY );
				pLblButton->SetSize( buttonWide, buttonTall );
				int textX = buttonX + 5*buttonWide/4;
				// back up half the text height
				textY -= textTall/2;
				pLblText->SetPos( textX, textY );
			}
			else
			{
				// Both buttons enabled
				// Align the CANCEL button to the bottom left edge
				int buttonX = buttonWide;
				m_pLblCancelButton->SetPos( buttonX, buttonY );
				m_pLblCancelButton->SetSize( buttonWide, buttonTall );

				// advance past the button and a gap
				int textX = buttonX + 5*buttonWide/4;
				int textWide, textTall;
				m_pLblCancelText->GetContentSize( textWide, textTall );
				// back up half the text height
				textY -= textTall/2;
				m_pLblCancelText->SetPos( textX, textY );

				// Align the OK text to the bottom right edge
				m_pLblOkText->GetContentSize( textWide, textTall );
				textX = dialogWidth - buttonWide - textWide;
				// back up by the text width
				m_pLblOkText->SetPos( textX, textY );
				// back up by the button width and a gap
				m_pLblOkButton->SetPos( textX - 5*buttonWide/4, buttonY );
				m_pLblOkButton->SetSize( buttonWide, buttonTall );
			}
		}
	}
	else
	{
		m_pLblCancelButton->SetVisible( false );
		m_pLblCancelText->SetVisible( false );
		m_pLblOkButton->SetVisible( false );
		m_pLblOkText->SetVisible( false );
	}

	if ( IsPC() )
	{
		if ( pOkButton )
		{
			pOkButton->SetVisible( m_data.bOkButtonEnabled );
			ExpandButtonWidthIfNecessary( pOkButton );
		}
		if ( pCancelButton )
		{
			pCancelButton->SetVisible( m_data.bCancelButtonEnabled );
			ExpandButtonWidthIfNecessary( pCancelButton );
		}

		if ( m_data.bCancelButtonEnabled || m_data.bOkButtonEnabled )
		{
			// when only one button is enabled, center that button
			vgui::Button *pButton = NULL;
			bool bSingleButton = false;
			if ( ( m_data.bCancelButtonEnabled && !m_data.bOkButtonEnabled ) )
			{
				// cancel is centered
				bSingleButton = true;
				pButton = pCancelButton;
			}
			else if ( !m_data.bCancelButtonEnabled && m_data.bOkButtonEnabled )
			{
				// OK is centered
				bSingleButton = true;
				pButton = pOkButton;
			}

			if ( bSingleButton )
			{
				// center the button
				pButton->SetPos( ( dialogWidth - pButton->GetWide() )/2, dialogHeight - borderGap - buttonTall );
			}
			else
			{
				//if( dialogWidth <= minWidth )
				{
					// center left the OK
					pOkButton->SetPos( dialogWidth/2 - buttonWide - borderGap/2, dialogHeight - borderGap - buttonTall );
					// center right the CANCEL
					pCancelButton->SetPos( dialogWidth/2 + borderGap/2, dialogHeight - borderGap - buttonTall );
				}
// 				else
// 				{
// 					// right align both the buttons
// 					int x = dialogWidth - borderGap - vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 40 );
// 
// 					x -= pCancelButton->GetWide();
// 					pCancelButton->SetPos( x, dialogHeight - borderGap - buttonTall );
// 
// 					x -= borderGap;
// 					x -= pOkButton->GetWide();
// 					pOkButton->SetPos(x, dialogHeight - borderGap - buttonTall );
// 				}
			}
		}
	}
}

//=============================================================================
void GenericConfirmation::PaintBackground()
{
	BaseClass::DrawGenericBackground();

	if ( m_bNeedsMoveToFront )
	{
		vgui::ipanel()->MoveToFront( GetVPanel() );
		m_bNeedsMoveToFront = false;
	}
}

//=============================================================================
// returns the usageId, which gets incremented each time this function is called
int GenericConfirmation::SetUsageData( const Data_t & data )
{
	m_data = data;

	SetTitle( data.pWindowTitle, false );

	if ( m_pLblMessage )
	{
		m_pLblMessage->DeletePanel();
		m_pLblMessage = NULL;
	}
	
	if ( data.pMessageTextW )
		m_pLblMessage = new Label( this, "LblMessage", data.pMessageTextW );
	else
		m_pLblMessage = new Label( this, "LblMessage", data.pMessageText );

	// tell our base version so input is disabled
	m_OkButtonEnabled = data.bOkButtonEnabled;
	m_CancelButtonEnabled = data.bCancelButtonEnabled;

	if ( data.bCheckBoxEnabled && data.pCheckBoxCvarName )
	{
		m_pCheckBox = new CvarToggleCheckButton<CGameUIConVarRef>( 
			this, 
			"CheckButton", 
			"",
			data.pCheckBoxCvarName,
			true );

		if ( m_pLblCheckBox )
		{
			m_pLblCheckBox->DeletePanel();
			m_pLblCheckBox = NULL;
		}

		m_pLblCheckBox = new Label( this, "LblCheckButton", data.pCheckBoxLabelText );
	}

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] GenericConfirmation::SetWindowTitle : %s\n", data.pWindowTitle!=NULL ? data.pWindowTitle : "<NULL>" );
		Msg( "[GAMEUI] GenericConfirmation::SetMessageText : %s\n", data.pMessageText!=NULL ? data.pMessageText : "<NULL>" );
	}

	// the window may need to be resized.
	LoadLayout();

	return m_usageId = ++sm_currentUsageId;
}


//=============================================================================
void GenericConfirmation::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	LoadLayout();

	SetPaintBackgroundEnabled( true );

	//
	// Override title and msg font
	//
	m_pLblOkButton->SetFont( pScheme->GetFont( "GameUIButtonsMini", true ) );
	m_pLblCancelButton->SetFont( pScheme->GetFont( "GameUIButtonsMini", true ) );

	m_hTitleFont = pScheme->GetFont( "MainBold", true );
	m_hMessageFont = pScheme->GetFont( "Default", true );

	if ( m_LblTitle )
	{
		m_LblTitle->SetFont( m_hTitleFont );
	}

	if ( m_pLblMessage )
	{
		m_pLblMessage->SetFont( m_hMessageFont );
	}

	if ( m_pLblCheckBox )
	{
		m_pLblCheckBox->SetFont( m_hMessageFont );
	}
}
