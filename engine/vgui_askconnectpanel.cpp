//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "vgui_baseui_interface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/AnimationController.h"
#include "vgui/ILocalize.h"
#include "mathlib/mathlib.h"
#include "inputsystem/ButtonCode.h"
#include "vgui_askconnectpanel.h"
#include "keys.h"
#include "cl_pluginhelpers.h"


using namespace vgui;


class CAskConnectPanel : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CAskConnectPanel, vgui::EditablePanel );

public:
	CAskConnectPanel( VPANEL parent );
	~CAskConnectPanel();

	void GetHostName( char *pOut, int maxOutBytes );
	void SetHostName( const char *pHostName );
	void StartSlideAnimation( float flDuration );
	void UpdateCurrentPosition();
	void Hide();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void ApplySettings(KeyValues *inResourceData);
	virtual void OnTick();

public:
	static CAskConnectPanel *s_pAskConnectPanel;

private:
	char m_HostName[256];
	Color m_bgColor;
	int m_OriginalWidth;					// Don't get less than this wide.
	
	double m_flAnimationEndTime;				// -1 if not playing

	Label *m_pInfoLabel;
	
	Label *m_pHostNameLabel;
	int m_HostNameLabelRightSidePadding;	// Grow the whole panel to make sure there's this much padding on the right of the hostname label.
	
	Label *m_pAcceptLabel;
	AnimationController *m_pAnimationController;
};

CAskConnectPanel *CAskConnectPanel::s_pAskConnectPanel = NULL;


CAskConnectPanel::CAskConnectPanel( VPANEL parent ) 
	: BaseClass( NULL, "AskConnectPanel" ), m_bgColor( 0, 0, 0, 192 )
{
	SetParent( parent );
	Assert( s_pAskConnectPanel == NULL );
	s_pAskConnectPanel = this;
	m_flAnimationEndTime = -1;

	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetVisible( false );

	m_pHostNameLabel = new Label( this, "HostNameLabel", "" );
	m_pAcceptLabel = new Label( this, "AcceptLabel", "" );
	m_pInfoLabel = new Label( this, "InfoLabel", "" );
	
	m_HostName[0] = 0;
	vgui::ivgui()->AddTickSignal( GetVPanel() );
	SetAutoDelete( true );

	m_pAnimationController = new vgui::AnimationController( NULL );
	m_pAnimationController->SetParent( parent );
	m_pAnimationController->SetScriptFile( parent, "scripts/plugin_animations.txt" );
	m_pAnimationController->SetProportional( false );

	LoadControlSettings( "resource\\askconnectpanel.res" );
	InvalidateLayout( true );
	
	m_OriginalWidth = GetWide();
	int x, y, wide, tall;
	m_pHostNameLabel->GetBounds( x, y, wide, tall );
	m_HostNameLabelRightSidePadding = GetWide() - (x+wide);
}

CAskConnectPanel::~CAskConnectPanel()
{
	s_pAskConnectPanel = NULL;
}

void CAskConnectPanel::GetHostName( char *pOut, int maxOutBytes )
{
	V_strncpy( pOut, m_HostName, maxOutBytes );
}

void CAskConnectPanel::SetHostName( const char *pHostName )
{
	V_strncpy( m_HostName, pHostName, sizeof( m_HostName ) );
	m_pHostNameLabel->SetText( pHostName );
	
	// Update our width.
	int x, y, wide, tall;
	m_pHostNameLabel->SizeToContents();
	m_pHostNameLabel->GetBounds( x, y, wide, tall );	

	int x2, y2, wide2, tall2;
	wchar_t wcMessage[512];
	g_pVGuiLocalize->ConstructString_safe( wcMessage, g_pVGuiLocalize->Find("#Valve_ServerOfferingToConnect"), 0 );
	m_pInfoLabel->SetText( wcMessage );
	m_pInfoLabel->SizeToContents();
	m_pInfoLabel->GetBounds( x2, y2, wide2, tall2 );	

	int desiredWidth = max(x+wide,x2+wide2) + m_HostNameLabelRightSidePadding;
	if ( desiredWidth < m_OriginalWidth )
		desiredWidth = m_OriginalWidth;
	
	SetWide( desiredWidth );
}

void CAskConnectPanel::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);
	
	const char *pStr = inResourceData->GetString( "BgColor", NULL );
	if ( pStr )
	{
		int r = 0, g = 0, b = 0, a = 0;
		if ( sscanf( pStr, "%d %d %d %d", &r, &g, &b, &a ) == 4 )
		{
			m_bgColor = Color( r, g, b, a );
			SetBgColor( m_bgColor );
		}
	}
}

void CAskConnectPanel::StartSlideAnimation( float flDuration )
{
	m_flAnimationEndTime = Plat_FloatTime() + flDuration;
	
	// Figure out what key they have bound...
	const char *pKeyName = Key_NameForBinding( "askconnect_accept" );
	if ( pKeyName )
	{
		wchar_t wcKeyName[64], wcMessage[512];
		g_pVGuiLocalize->ConvertANSIToUnicode( pKeyName, wcKeyName, sizeof( wcKeyName ) );
		g_pVGuiLocalize->ConstructString_safe( wcMessage, g_pVGuiLocalize->Find("#Valve_PressKeyToAccept"), 1, wcKeyName );
		m_pAcceptLabel->SetText( wcMessage );
	}
	else
	{
		m_pAcceptLabel->SetText( "#Valve_BindKeyToAccept" );
	}

	m_pAnimationController->StartAnimationSequence( "AskConnectShow" ); 
	SetVisible( true );
	InvalidateLayout();
	UpdateCurrentPosition();
}

void CAskConnectPanel::Hide()
{
	m_flAnimationEndTime = -1;
	SetVisible( false );
}

void CAskConnectPanel::OnTick()
{
	// Do the hide animation?
	if ( m_flAnimationEndTime != -1 )
	{
		if ( Plat_FloatTime() > m_flAnimationEndTime )
		{
			m_flAnimationEndTime = -1;
			m_pAnimationController->StartAnimationSequence( "AskConnectHide" ); 
		}
	}

	m_pAnimationController->UpdateAnimations( Sys_FloatTime() );

	// Make sure vgui doesn't call Paint() on us after we're hidden.
	if ( GetAlpha() == 0 )
		SetVisible( false );

	if ( IsVisible() )
	{
		UpdateCurrentPosition();
	}

	BaseClass::OnTick();
}

void CAskConnectPanel::UpdateCurrentPosition()
{
	int x=0, y=0, wide=0, tall=0;
	if ( g_PluginManager )
		g_PluginManager->GetHudMessagePosition( x, y, wide, tall );
	
	SetPos( x, y+tall );
}

void CAskConnectPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBgColor( m_bgColor );
	SetPaintBackgroundType( 2 );
}


void SetupDefaultAskConnectAcceptKey()
{
	// If they don't have a binding for askconnect_accept, set one up.
	if ( !Key_NameForBinding( "askconnect_accept" ) )
	{
		// .. but only if they don't already have something setup for F3.
		if ( !Key_BindingForKey( KEY_F3 ) )
		{
			Key_SetBinding( KEY_F3, "askconnect_accept" );
		}
	}
}


vgui::Panel* CreateAskConnectPanel( VPANEL parent )
{
	return new CAskConnectPanel( parent );
}


void ShowAskConnectPanel( const char *pHostName, float flDuration )
{
	const int cubHostName = V_strlen( pHostName );
	if ( cubHostName <= 0 )
		return;

	// Hostname is not allowed to contain semicolon, whitespace, or control characters
	for ( int i = 0; i < cubHostName; i++ )
	{
		if ( pHostName[i] == ';' || V_isspace( pHostName[i] ) || pHostName[i] < 0x20 )
		{
			return;
		}
	}

	CAskConnectPanel *pPanel = CAskConnectPanel::s_pAskConnectPanel;
	if ( pPanel )
	{
		pPanel->SetHostName( pHostName );
		pPanel->StartSlideAnimation( flDuration );
		pPanel->MoveToFront();
	}
}


void HideAskConnectPanel()
{
	CAskConnectPanel *pPanel = CAskConnectPanel::s_pAskConnectPanel;
	if ( pPanel )
		pPanel->Hide();
}

bool IsAskConnectPanelActive( char *pHostName, int maxHostNameBytes )
{
	CAskConnectPanel *pPanel = CAskConnectPanel::s_pAskConnectPanel;
	if ( pPanel && pPanel->IsVisible() && pPanel->GetAlpha() > 0 )
	{
		pPanel->GetHostName( pHostName, maxHostNameBytes );
		return true;
	}
	else
	{
		return false;
	}
}

