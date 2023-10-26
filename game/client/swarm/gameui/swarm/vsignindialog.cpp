//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VSignInDialog.h"
#include "VAttractScreen.h"
#include "tier1/KeyValues.h"

#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

#define PLAYER_DEBUG_NAME "WWWWWWWWWWWWWWW"

//=============================================================================
SignInDialog::SignInDialog( Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, true, false )
{
	SetProportional( true );

	m_flTimeAutoClose = Plat_FloatTime() + 60.0f;

	SetDeleteSelfOnClose( true );
}

//=============================================================================
SignInDialog::~SignInDialog()
{
}

void SignInDialog::SetSignInTitle( const wchar_t *title )
{
	vgui::Label * lblTitle = dynamic_cast< vgui::Label * > ( FindChildByName( "LblTitle" ) );
	if ( lblTitle )
	{
		lblTitle->SetText( title );
	}
}

void SignInDialog::SetSignInTitle( const char* title )
{
	vgui::Label * lblTitle = dynamic_cast< vgui::Label * > ( FindChildByName( "LblTitle" ) );
	if ( lblTitle )
	{
		lblTitle->SetText( title );
	}
}

//=============================================================================
void SignInDialog::OnCommand( const char *command )
{
	int iUser = BaseModUI::CBaseModPanel::GetSingleton().GetLastActiveUserId();

	if( ! Q_strcmp( command, "Play" ) )
	{
		if ( iUser != (int) XBX_GetPrimaryUserId() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}

		NavigateBack( 1 );
	}
	else if( ! Q_strcmp( command, "PlaySplitscreen" ) )
	{
		NavigateBack( 2 );
	}
	else if( ! Q_strcmp( command, "PlayAsGuest" ) )
	{
		if ( iUser != (int) XBX_GetPrimaryUserId() )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_INVALID );
			return;
		}

		if ( CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) ) )
		{
			BaseClass::NavigateBack();
			Close();
			attractScreen->StartGameWithTemporaryProfile_Stage1();
		}
	}
	else if( ! Q_strcmp( command, "CancelSignIn" ) )
	{
		NavigateBack( 0 );
	}
}

void SignInDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		NavigateBack( 0 );
		break;
	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

void SignInDialog::PaintBackground()
{
	BaseClass::DrawGenericBackground();
}

//=============================================================================
void SignInDialog::LoadLayout()
{
	BaseClass::LoadLayout();

#ifdef _X360
	vgui::Panel * panel = FindChildByName( "BtnSignin" );

	if( panel )
	{
		panel->NavigateTo();
	}
#endif // _X360
}

//=============================================================================
void SignInDialog::ApplySettings( KeyValues * inResourceData )
{
	BaseClass::ApplySettings( inResourceData );
}

void SignInDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

#ifdef _X360
	char chGamerName[256];

	XUSER_SIGNIN_INFO xsi;
	if ( XBX_GetPrimaryUserId() >= 0 &&
		 XUserGetSigninState( XBX_GetPrimaryUserId() ) != eXUserSigninState_NotSignedIn &&
		 ERROR_SUCCESS == XUserGetName( XBX_GetPrimaryUserId(), chGamerName, sizeof( chGamerName ) ) &&
		 ERROR_SUCCESS == XUserGetSigninInfo( XBX_GetPrimaryUserId(), XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) &&
		 !(xsi.dwInfoFlags & XUSER_INFO_FLAG_GUEST) )	// need to catch promoted-guest-accounts
	{
		static ConVarRef cl_names_debug( "cl_names_debug" );
		if ( cl_names_debug.GetInt() )
		{
			strcpy( chGamerName, PLAYER_DEBUG_NAME );
		}

		const unsigned messageLen = 256;
		wchar_t* wStringTableEntry = g_pVGuiLocalize->Find( "#L4D360UI_SignIn_Title" );

		wchar_t wWelcomeMsg[ messageLen ];
		wchar_t wGamerTag[ messageLen ];
		g_pVGuiLocalize->ConvertANSIToUnicode( chGamerName, wGamerTag, sizeof( wGamerTag ) );

		g_pVGuiLocalize->ConstructString( wWelcomeMsg, sizeof( wWelcomeMsg ), wStringTableEntry,
			1, wGamerTag );

		SetSignInTitle( wWelcomeMsg );
		m_bSignedIn = true;
	}
	else
#endif
	{
		SetSignInTitle( "#L4D360UI_SignIn_TitleNo" );
		m_bSignedIn = false;
	}
}

void SignInDialog::OnThink()
{
	// As soon as UI opens pretend like cancel was selected
	if ( CUIGameData::Get()->IsXUIOpen() )
	{
		NavigateBack( 0 );
		return;
	}

	if ( Plat_FloatTime() > m_flTimeAutoClose )
	{
		NavigateBack( 0 );
		return;
	}
	
	if ( vgui::Label *pLbl = dynamic_cast< vgui::Label* >( FindChildByName( "LblMustBeSignedIn" ) ) )
	{
		pLbl->SetVisible( !m_bSignedIn );
	}

	BaseClass::OnThink();
}

//=============================================================================
vgui::Panel* SignInDialog::NavigateBack( int numSlotsRequested )
{
	CAttractScreen* attractScreen = static_cast< CAttractScreen* >( CBaseModPanel::GetSingleton().GetWindow( WT_ATTRACTSCREEN ) );
	if( attractScreen )
	{
#if defined(_X360)
		// Todo: add an option to play without signing in!

		// Attract screen preserved the state of who activated the
		// sign-in dialog, so we can reset user ids freely here.

		// When the attact screen opens, clear the primary userId and
		// reset all the controllers as enabled
		// This is here because CAttractScreen::OnOpen can be called when navigating away
		// from it as well
		XBX_SetPrimaryUserId( XBX_INVALID_USER_ID );
		XBX_ResetUserIdSlots();
#endif //defined(_X360)

		switch ( numSlotsRequested )
		{
		default:
		case 0:
			attractScreen->ShowPressStart();
			break;
		case 1:
			PostMessage( attractScreen, new KeyValues( "StartWaitingForBlade1" ) );
			break;
		case 2:
			PostMessage( attractScreen, new KeyValues( "StartWaitingForBlade2" ) );
			break;
		}
	}

	vgui::Panel *pReturn = BaseClass::NavigateBack();
	Close();
	return pReturn;
}

