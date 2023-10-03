//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VPasswordEntry.h"

#include "vgui_controls/Label.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

int PasswordEntry::sm_currentUsageId = 0;

PasswordEntry::Data_t::Data_t() : 
	pWindowTitle( NULL ),
	pMessageText( NULL ),
	bOkButtonEnabled( false ),
	pfnOkCallback( NULL ),
	bCancelButtonEnabled( false ),
	pfnCancelCallback( NULL )
{
}

//=============================================================================
PasswordEntry::PasswordEntry( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, true, false ),
	m_pLblMessage( 0 ),
	m_data(),
	m_usageId( 0 )
{
	SetProportional( true );

	m_pLblOkButton = new vgui::Label( this, "LblOkButton", "#GameUI_Icons_A_3DBUTTON" );
	m_pLblOkText = new vgui::Label( this, "LblOkText", "#L4D360UI_Ok" );
	m_pLblCancelButton = new vgui::Label( this, "LblCancelButton", "#GameUI_Icons_B_3DBUTTON" );
	m_pLblCancelText = new vgui::Label( this, "LblCancelText", "#L4D360UI_Cancel" );
	m_pPnlLowerGarnish = new vgui::Panel( this, "PnlLowerGarnish" );

	m_pInputField = new vgui::TextEntry( this, "TxtPasswordEntry" );
	m_pInputField->SetTextHidden( true );

	SetTitle( "", false );
	SetDeleteSelfOnClose( true );
	SetLowerGarnishEnabled( false );
}

//=============================================================================
PasswordEntry::~PasswordEntry()
{
	delete m_pLblOkButton;
	delete m_pLblOkText;
	delete m_pLblCancelButton;
	delete m_pLblCancelText;
	delete m_pPnlLowerGarnish;
}

//=============================================================================
void PasswordEntry::OnCommand(const char *command)
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
void PasswordEntry::OnKeyCodePressed( KeyCode keycode )
{
	int userId = GetJoystickForCode( keycode );
	vgui::KeyCode code = GetBaseButtonCode( keycode );
	BaseModUI::CBaseModPanel::GetSingleton().SetLastActiveUserId( userId );

	switch ( code )
	{
	case KEY_XBUTTON_A:
		if ( m_OkButtonEnabled )
		{
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
void PasswordEntry::OnKeyCodeTyped( vgui::KeyCode code )
{
	// For PC, this maps space bar to OK and esc to cancel
	switch ( code )
	{
	case KEY_SPACE:
		return OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );

	case KEY_ESCAPE:
		return OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}

	BaseClass::OnKeyTyped( code );
}
#endif

void PasswordEntry::OnOpen( )
{
	BaseClass::OnOpen();

	m_bNeedsMoveToFront = true;
}

//=============================================================================
void PasswordEntry::LoadLayout()
{
	BaseClass::LoadLayout();

	vgui::Label *pLblTitle = dynamic_cast< vgui::Label* >( FindChildByName( "LblTitle" ) );
	ErrorIfNot( pLblTitle, ( "PasswordEntry Missing Title" ) );


	int screenWidth, screenHeight;
	CBaseModPanel::GetSingleton().GetSize( screenWidth, screenHeight );

	int dialogWidth = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 100 );

	// need a border gap to inset all controls
	int borderGap = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 8 );

	// first solve the size of the parent window
	int titleWide = 0;
	int titleTall = 0;
	int titleX = 0;
	int titleY = 0;

	if ( pLblTitle )
	{
		// account for size of the title and a gap
		pLblTitle->GetPos( titleX, titleY );
		pLblTitle->GetContentSize( titleWide, titleTall );
		int shim = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 15 );
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

		int shimX = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 20 );
		if ( dialogWidth < msgWide + shimX )
		{
			dialogWidth = msgWide + shimX;
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

	// Account for input button
	int inputTall = m_pInputField->GetTall();

	int nextY = titleY;

	if ( pLblTitle )
	{
		// horizontally center and vertically inset the title
		pLblTitle->SetPos( ( dialogWidth - titleWide ) / 2, nextY );
		pLblTitle->SetSize( titleWide, titleTall );

		nextY += ( titleTall + borderGap );
	}

	if ( m_pLblMessage )
	{
		// center the message
		int msgWide, msgTall;
		m_pLblMessage->GetContentSize( msgWide, msgTall );
		m_pLblMessage->SetPos( ( dialogWidth - msgWide ) / 2, nextY );
		m_pLblMessage->SetSize( msgWide, msgTall );

		nextY += ( msgTall + borderGap );
	}

	m_pInputField->SetPos( borderGap, nextY );
	m_pInputField->SetSize( dialogWidth - 2 * borderGap, inputTall );

	nextY += ( inputTall + borderGap );

	// Room for buttons
	int nButtonY = nextY;

	nextY += ( buttonTall + borderGap );

	int dialogHeight = nextY;

	// now have final dialog dimensions, center the dialog
	SetPos( ( screenWidth - dialogWidth ) / 2, ( screenHeight - dialogHeight ) / 2 );
	SetSize( dialogWidth, dialogHeight );
	
	m_pLblCancelButton->SetVisible( false );
	m_pLblCancelText->SetVisible( false );
	m_pLblOkButton->SetVisible( false );
	m_pLblOkText->SetVisible( false );

	if ( IsPC() )
	{
		if ( pOkButton )
		{
			pOkButton->SetVisible( m_data.bOkButtonEnabled );
		}
		if ( pCancelButton )
		{
			pCancelButton->SetVisible( m_data.bCancelButtonEnabled );
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
				pButton->SetPos( ( dialogWidth - buttonWide )/2, nButtonY );
			}
			else
			{
				// left align the cancel
				pCancelButton->SetPos( borderGap, nButtonY );
				// right align the OK
				pOkButton->SetPos( dialogWidth - borderGap - buttonWide, nButtonY );
			}
		}
	}

	
}

void PasswordEntry::PaintBackground()
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
int PasswordEntry::SetUsageData( const Data_t & data )
{
	m_data = data;

	SetTitle( data.pWindowTitle, false );

	if ( m_pLblMessage )
	{
		m_pLblMessage->DeletePanel();
		m_pLblMessage = NULL;
	}
	m_pLblMessage = new Label( this, "LblMessage", data.pMessageText );

	// tell our base version so input is disabled
	m_OkButtonEnabled = data.bOkButtonEnabled;
	m_CancelButtonEnabled = data.bCancelButtonEnabled;

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] PasswordEntry::SetWindowTitle : %s\n", data.pWindowTitle!=NULL ? data.pWindowTitle : "<NULL>" );
		Msg( "[GAMEUI] PasswordEntry::SetMessageText : %s\n", data.pMessageText!=NULL ? data.pMessageText : "<NULL>" );
	}

	m_pInputField->SetText( data.m_szCurrentPW );

	// the window may need to be resized.
	LoadLayout();

	return m_usageId = ++sm_currentUsageId;
}


//=============================================================================
void PasswordEntry::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pLblOkButton->SetFont( pScheme->GetFont( "GameUIButtons" ) );
	m_pLblCancelButton->SetFont( pScheme->GetFont( "GameUIButtons" ) );
	m_pInputField->SetFont( pScheme->GetFont( "MainBold" ) );

	SetPaintBackgroundEnabled( true );
	LoadLayout();
}

void PasswordEntry::GetPassword( char *buf, size_t bufsize )
{
	m_pInputField->GetText( buf, bufsize );
}
