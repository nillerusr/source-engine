//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "LoadingDialog.h"
#include "EngineInterface.h"
#include "igameuifuncs.h"

#include <vgui/IInput.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISystem.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/HTML.h>
#include <vgui_controls/RichText.h>
#include "tier0/icommandline.h"

#include "GameUI_Interface.h"
#include "ModInfo.h"
#include "basepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLoadingDialog::CLoadingDialog( vgui::Panel *parent ) : Frame(parent, "LoadingDialog")
{
	SetDeleteSelfOnClose(true);

	// Use console style
	m_bConsoleStyle = GameUI().IsConsoleUI();

	if ( !m_bConsoleStyle )
	{
		SetSize( 416, 100 );
		SetTitle( "#GameUI_Loading", true );
	}

	// center the loading dialog, unless we have another dialog to show in the background
	m_bCenter = !GameUI().HasLoadingBackgroundDialog();

	m_bShowingSecondaryProgress = false;
	m_flSecondaryProgress = 0.0f;
	m_flLastSecondaryProgressUpdateTime = 0.0f;
	m_flSecondaryProgressStartTime = 0.0f;

	m_pProgress = new ProgressBar( this, "Progress" );
	m_pProgress2 = new ProgressBar( this, "Progress2" );
	m_pInfoLabel = new Label( this, "InfoLabel", "" );
	m_pCancelButton = new Button( this, "CancelButton", "#GameUI_Cancel" );
	m_pTimeRemainingLabel = new Label( this, "TimeRemainingLabel", "" );
	m_pCancelButton->SetCommand( "Cancel" );

	if ( ModInfo().IsSinglePlayerOnly() == false && m_bConsoleStyle == true )
	{
		m_pLoadingBackground = new Panel( this, "LoadingDialogBG" );
	}
	else
	{
		m_pLoadingBackground = NULL;
	}

	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( false );
	SetMoveable( false );

	if ( m_bConsoleStyle )
	{
		m_bCenter = false;
		m_pProgress->SetVisible( false );
		m_pProgress2->SetVisible( false );
		m_pInfoLabel->SetVisible( false );
		m_pCancelButton->SetVisible( false );
		m_pTimeRemainingLabel->SetVisible( false );
		m_pCancelButton->SetVisible( false );

		SetMinimumSize( 0, 0 );
		SetTitleBarVisible( false );

		m_flProgressFraction = 0;
	}
	else
	{
		m_pInfoLabel->SetBounds(20, 32, 392, 24);
		m_pProgress->SetBounds(20, 64, 300, 24); 
		m_pCancelButton->SetBounds(330, 64, 72, 24);
		m_pInfoLabel->SetTextColorState(Label::CS_DULL);
		m_pProgress2->SetVisible(false);
	}

	SetupControlSettings( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLoadingDialog::~CLoadingDialog()
{
}

void CLoadingDialog::PaintBackground()
{
	if ( !m_bConsoleStyle )
	{
		BaseClass::PaintBackground();
		return;
	}

	// draw solid progress bar with curved endcaps
	int panelWide, panelTall;
	GetSize( panelWide, panelTall );
	int barWide, barTall;
	m_pProgress->GetSize( barWide, barTall );
	int x = ( panelWide - barWide )/2;
	int y = panelTall - barTall;

	if ( m_pLoadingBackground )
	{
		vgui::HScheme scheme = vgui::scheme()->GetScheme( "ClientScheme" );
		Color color = GetSchemeColor( "TanDarker", Color(255, 255, 255, 255), vgui::scheme()->GetIScheme(scheme) );

		m_pLoadingBackground->SetFgColor( color );
		m_pLoadingBackground->SetBgColor( color );

		m_pLoadingBackground->SetPaintBackgroundEnabled( true );
	}
	
	if ( ModInfo().IsSinglePlayerOnly() )
	{
		DrawBox( x, y, barWide, barTall, Color( 0, 0, 0, 255 ), 1.0f );
	}

	DrawBox( x+2, y+2, barWide-4, barTall-4, Color( 100, 100, 100, 255 ), 1.0f );

	barWide = m_flProgressFraction * ( barWide - 4 );
	if ( barWide >= 12 )
	{
		// cannot draw a curved box smaller than 12 without artifacts
		DrawBox( x+2, y+2, barWide, barTall-4, Color( 200, 100, 0, 255 ), 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets up dialog layout
//-----------------------------------------------------------------------------
void CLoadingDialog::SetupControlSettings( bool bForceShowProgressText )
{
	m_bShowingVACInfo = false;

#if defined( BASEPANEL_LEGACY_SOURCE1 )
	if ( GameUI().IsConsoleUI() )
	{
		KeyValues *pControlSettings = BasePanel()->GetConsoleControlSettings()->FindKey( "LoadingDialogNoBanner.res" );
		LoadControlSettings( "null", NULL, pControlSettings );
		return;
	}
#endif

	if ( ModInfo().IsSinglePlayerOnly() && !bForceShowProgressText )
	{
		LoadControlSettings("Resource/LoadingDialogNoBannerSingle.res");
	}
	else if ( gameuifuncs->IsConnectedToVACSecureServer() )
	{
		LoadControlSettings("Resource/LoadingDialogVAC.res");
		m_bShowingVACInfo = true;
	}
	else
	{
		LoadControlSettings("Resource/LoadingDialogNoBanner.res");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activates the loading screen, initializing and making it visible
//-----------------------------------------------------------------------------
void CLoadingDialog::Open()
{
	if ( !m_bConsoleStyle )
	{
		SetTitle( "#GameUI_Loading", true );
	}

	HideOtherDialogs( true );
	BaseClass::Activate();

	if ( !m_bConsoleStyle )
	{
		m_pProgress->SetVisible( true );
		if ( !ModInfo().IsSinglePlayerOnly() )
		{
			m_pInfoLabel->SetVisible( true );
		}
		m_pInfoLabel->SetText("");
		
		m_pCancelButton->SetText("#GameUI_Cancel");
		m_pCancelButton->SetCommand("Cancel");
	}
}


//-----------------------------------------------------------------------------
// Purpose: error display file
//-----------------------------------------------------------------------------
void CLoadingDialog::SetupControlSettingsForErrorDisplay( const char *settingsFile )
{
	if ( m_bConsoleStyle )
	{
		return;
	}

	m_bCenter = true;
	SetTitle("#GameUI_Disconnected", true);
	m_pInfoLabel->SetText("");
	LoadControlSettings( settingsFile );
	HideOtherDialogs( true );

	BaseClass::Activate();
	
	m_pProgress->SetVisible(false);

	m_pInfoLabel->SetVisible(true);
	m_pCancelButton->SetText("#GameUI_Close");
	m_pCancelButton->SetCommand("Close");
	m_pInfoLabel->InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: shows or hides other top-level dialogs
//-----------------------------------------------------------------------------
void CLoadingDialog::HideOtherDialogs( bool bHide )
{
	if ( bHide )
	{
		if ( GameUI().HasLoadingBackgroundDialog() )
		{
			// if we have a loading background dialog, hide any other dialogs by moving the full-screen background dialog to the
			// front, then moving ourselves in front of it
			GameUI().ShowLoadingBackgroundDialog();
			vgui::ipanel()->MoveToFront( GetVPanel() );
			vgui::input()->SetAppModalSurface( GetVPanel() );
		}
		else
		{
			// if there is no loading background dialog, use VGUI paint restrictions to hide other dialogs
			vgui::surface()->RestrictPaintToSinglePanel(GetVPanel());
		}
	}
	else
	{
		if ( GameUI().HasLoadingBackgroundDialog() )
		{
			GameUI().HideLoadingBackgroundDialog();
			vgui::input()->SetAppModalSurface( NULL );
		}
		else
		{
			// remove any rendering restrictions
			vgui::surface()->RestrictPaintToSinglePanel(NULL);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Turns dialog into error display
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplayGenericError(const char *failureReason, const char *extendedReason)
{
	if ( m_bConsoleStyle )
	{
		return;
	}

	// In certain race conditions, DisplayGenericError can get called AFTER OnClose() has been called.
	// If that happens and we don't call Activate(), then it'll continue closing when we don't want it to.
	Activate(); 
	
	SetupControlSettingsForErrorDisplay("Resource/LoadingDialogError.res");

	if ( extendedReason && strlen( extendedReason ) > 0 ) 
	{
		wchar_t compositeReason[256], finalMsg[512], formatStr[256];
		if ( extendedReason[0] == '#' )
		{
			wcsncpy(compositeReason, g_pVGuiLocalize->Find(extendedReason), sizeof( compositeReason ) / sizeof( wchar_t ) );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode(extendedReason, compositeReason, sizeof( compositeReason ));
		}

		if ( failureReason[0] == '#' )
		{
			wcsncpy(formatStr, g_pVGuiLocalize->Find(failureReason), sizeof( formatStr ) / sizeof( wchar_t ) );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode(failureReason, formatStr, sizeof( formatStr ));
		}

		g_pVGuiLocalize->ConstructString(finalMsg, sizeof( finalMsg ), formatStr, 1, compositeReason);
		m_pInfoLabel->SetText(finalMsg);
	}
	else
	{
		m_pInfoLabel->SetText(failureReason);
	}
}


//-----------------------------------------------------------------------------
// Purpose: explain to the user they can't join secure servers due to a VAC ban
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplayVACBannedError()
{
	if ( m_bConsoleStyle )
	{
		return;
	}

	SetupControlSettingsForErrorDisplay("Resource/LoadingDialogErrorVACBanned.res");
	SetTitle("#VAC_ConnectionRefusedTitle", true);
}


//-----------------------------------------------------------------------------
// Purpose: explain to the user they can't connect to public servers due to 
//			not having a valid connection to Steam
//			this should only happen if they are a pirate
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplayNoSteamConnectionError()
{
	if ( m_bConsoleStyle )
	{
		return;
	}

	SetupControlSettingsForErrorDisplay("Resource/LoadingDialogErrorNoSteamConnection.res");
}


//-----------------------------------------------------------------------------
// Purpose: explain to the user they got kicked from a server due to that same account 
//			logging in from another location. This also triggers the refresh login dialog on OK 
//			being pressed.
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplayLoggedInElsewhereError()
{
	if ( m_bConsoleStyle )
	{
		return;
	}

	SetupControlSettingsForErrorDisplay("Resource/LoadingDialogErrorLoggedInElsewhere.res");
	m_pCancelButton->SetText("#GameUI_RefreshLogin_Login");
	m_pCancelButton->SetCommand("Login");
}


//-----------------------------------------------------------------------------
// Purpose: sets status info text
//-----------------------------------------------------------------------------
void CLoadingDialog::SetStatusText(const char *statusText)
{
	if ( m_bConsoleStyle )
	{
		return;
	}

	m_pInfoLabel->SetText(statusText);
}

//-----------------------------------------------------------------------------
// Purpose: returns the previous state
//-----------------------------------------------------------------------------
bool CLoadingDialog::SetShowProgressText( bool show )
{
	if ( m_bConsoleStyle )
	{
		return false;
	}

	bool bret = m_pInfoLabel->IsVisible();
	if ( bret != show )
	{
		SetupControlSettings( show );
		m_pInfoLabel->SetVisible( show );
	}
	return bret;
}

//-----------------------------------------------------------------------------
// Purpose: updates time remaining
//-----------------------------------------------------------------------------
void CLoadingDialog::OnThink()
{
	BaseClass::OnThink();

	if ( !m_bConsoleStyle && m_bShowingSecondaryProgress )
	{
		// calculate the time remaining string
		wchar_t unicode[512];
		if (m_flSecondaryProgress >= 1.0f)
		{
			m_pTimeRemainingLabel->SetText("complete");
		}
		else if (ProgressBar::ConstructTimeRemainingString(unicode, sizeof(unicode), m_flSecondaryProgressStartTime, (float)system()->GetFrameTime(), m_flSecondaryProgress, m_flLastSecondaryProgressUpdateTime, true))
		{
			m_pTimeRemainingLabel->SetText(unicode);
		}
		else
		{
			m_pTimeRemainingLabel->SetText("");
		}
	}

	SetAlpha( 255 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadingDialog::PerformLayout()
{
	if ( m_bConsoleStyle )
	{
		// place in lower center
		int screenWide, screenTall;
		surface()->GetScreenSize( screenWide, screenTall );
		int wide,tall;
		GetSize( wide, tall );
		int x = 0;
		int y = 0;

		if ( ModInfo().IsSinglePlayerOnly() )
		{
			x = ( screenWide - wide ) * 0.50f;
			y = ( screenTall - tall ) * 0.86f;
		}
		else
		{
			x = ( screenWide - ( wide * 1.30f ) );
			y = ( ( screenTall * 0.875f ) );
		}

		SetPos( x, y );
	}
	else if ( m_bCenter )
	{
		MoveToCenterOfScreen();
	}
	else
	{
		// if we're not supposed to be centered, move ourselves to the lower right hand corner of the screen
		int x, y, screenWide, screenTall;
		surface()->GetWorkspaceBounds( x, y, screenWide, screenTall );
		int wide,tall;
		GetSize( wide, tall );

		if ( IsPC() )
		{
			x = screenWide - ( wide + 10 );
			y = screenTall - ( tall + 10 );
		}
		else
		{
			// Move farther in so we're title safe
			x = screenWide - wide - (screenWide * 0.05);
			y = screenTall - tall - (screenTall * 0.05);
		}

		x -= m_iAdditionalIndentX;
		y -= m_iAdditionalIndentY;

		SetPos( x, y );
	}
	
	BaseClass::PerformLayout();
	
	vgui::ipanel()->MoveToFront( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the number of ticks has changed
//-----------------------------------------------------------------------------
bool CLoadingDialog::SetProgressPoint( float fraction )
{
	if ( m_bConsoleStyle )
	{
		if ( fraction >= 0.99f )
		{
			// show the progress artifically completed to fill in 100%
			fraction = 1.0f;
		}
		fraction = clamp( fraction, 0.0f, 1.0f );
		if ( (int)(fraction * 25) != (int)(m_flProgressFraction * 25) )
		{
			m_flProgressFraction = fraction;
			return true;
		}
		return IsX360();
	}

	if ( !m_bShowingVACInfo && gameuifuncs->IsConnectedToVACSecureServer() )
	{
		SetupControlSettings( false );
	}

	int nOldDrawnSegments = m_pProgress->GetDrawnSegmentCount();
	m_pProgress->SetProgress( fraction );
	int nNewDrawSegments = m_pProgress->GetDrawnSegmentCount();
	return (nOldDrawnSegments != nNewDrawSegments) || IsX360();
}

//-----------------------------------------------------------------------------
// Purpose: sets and shows the secondary progress bar
//-----------------------------------------------------------------------------
void CLoadingDialog::SetSecondaryProgress( float progress )
{
	if ( m_bConsoleStyle )
		return;

	// don't show the progress if we've jumped right to completion
	if (!m_bShowingSecondaryProgress && progress > 0.99f)
		return;

	// if we haven't yet shown secondary progress then reconfigure the dialog
	if (!m_bShowingSecondaryProgress)
	{
		LoadControlSettings("Resource/LoadingDialogDualProgress.res");
		m_bShowingSecondaryProgress = true;
		m_pProgress2->SetVisible(true);
		m_flSecondaryProgressStartTime = (float)system()->GetFrameTime();
	}

	// if progress has increased then update the progress counters
	if (progress > m_flSecondaryProgress)
	{
		m_pProgress2->SetProgress(progress);
		m_flSecondaryProgress = progress;
		m_flLastSecondaryProgressUpdateTime = (float)system()->GetFrameTime();
	}

	// if progress has decreased then reset progress counters
	if (progress < m_flSecondaryProgress)
	{
		m_pProgress2->SetProgress(progress);
		m_flSecondaryProgress = progress;
		m_flLastSecondaryProgressUpdateTime = (float)system()->GetFrameTime();
		m_flSecondaryProgressStartTime = (float)system()->GetFrameTime();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadingDialog::SetSecondaryProgressText(const char *statusText)
{
	if ( m_bConsoleStyle )
	{
		return;
	}

	SetControlString( "SecondaryProgressLabel", statusText );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadingDialog::OnClose()
{
	// remove any rendering restrictions
	HideOtherDialogs( false );

	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: command handler
//-----------------------------------------------------------------------------
void CLoadingDialog::OnCommand(const char *command)
{
	if ( !stricmp(command, "Cancel") )
	{
		// disconnect from the server
		engine->ClientCmd_Unrestricted("disconnect\n");

		// close
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Maps ESC to quiting loading
//-----------------------------------------------------------------------------
void CLoadingDialog::OnKeyCodePressed(KeyCode code)
{
	if ( m_bConsoleStyle )
	{
		return;
	}

	if ( code == KEY_ESCAPE )
	{
		OnCommand("Cancel");
	}
	else
	{
		BaseClass::OnKeyCodePressed(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Singleton accessor
//-----------------------------------------------------------------------------
extern vgui::DHANDLE<CLoadingDialog> g_hLoadingDialog;
CLoadingDialog *LoadingDialog()
{
	return g_hLoadingDialog.Get();
}
