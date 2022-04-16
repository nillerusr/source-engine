//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "c_vguiscreen.h"
#include "vgui_controls/Label.h"
#include "vgui_bitmappanel.h"
#include <vgui/IVGui.h>
#include "c_neurotoxin_countdown.h"
#include "ienginevgui.h"
#include "fmtstr.h"
#include "vgui_controls/ImagePanel.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CNeurotoxinCountdownScreen : public CVGuiScreenPanel
{
	DECLARE_CLASS( CNeurotoxinCountdownScreen, CVGuiScreenPanel );

public:
	CNeurotoxinCountdownScreen( vgui::Panel *parent, const char *panelName );

	virtual void ApplySchemeSettings( IScheme *pScheme );

	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();

private:
	void Update( C_NeurotoxinCountdown *pNeurotoxinCountdown );

private:
	vgui::Label *m_pDisplayTextLabel;

	int iLastSlideIndex;

	Color m_cDefault;
	Color m_cInvisible;

	bool bIsAlreadyVisible;

	//ImagePanel *(m_pNumberImages[ 6 ][ 10 ]);
};


DECLARE_VGUI_SCREEN_FACTORY( CNeurotoxinCountdownScreen, "neurotoxin_countdown_screen" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CNeurotoxinCountdownScreen::CNeurotoxinCountdownScreen( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CNeurotoxinCountdownScreen", vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/NeurotoxinCountdownScreen.res", "NeuroToxinScreen" ) ) 
{
	m_pDisplayTextLabel = new vgui::Label( this, "NumberDisplay", "x" );
	iLastSlideIndex = 0;
}

void CNeurotoxinCountdownScreen::ApplySchemeSettings( IScheme *pScheme )
{
	assert( pScheme );

	m_cDefault = pScheme->GetColor( "CNeurotoxinCountdownScreen_Default", GetFgColor() );
	m_cInvisible = Color( 0, 0, 0, 0 );	

	m_pDisplayTextLabel->SetFgColor( m_cDefault );
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CNeurotoxinCountdownScreen::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	//for ( int iDigit = 0; iDigit < 6; ++iDigit )
	//{
	//	for ( int iNumber = 0; iNumber < 6; ++iNumber )
	//	{
	//		m_pNumberImages[ iDigit ][ iNumber ] = SETUP_PANEL( new ImagePanel( this, "SlideshowImage" ) );
	//		m_pNumberImages[ iDigit ][ iNumber ]->SetImage( "" );
	//		m_pNumberImages[ iDigit ][ iNumber ]->SetZPos( -2 );
	//		m_pNumberImages[ iDigit ][ iNumber ]->SetVisible( false );
	//	}
	//}

	return true;
}

//-----------------------------------------------------------------------------
// Update the display string
//-----------------------------------------------------------------------------
void CNeurotoxinCountdownScreen::OnTick()
{
	BaseClass::OnTick();

	if ( g_NeurotoxinCountdowns.Count() <= 0 )
		return;

	C_NeurotoxinCountdown *pNeurotoxinCountdown = NULL;

	for ( int iDisplayScreens = 0; iDisplayScreens < g_NeurotoxinCountdowns.Count(); ++iDisplayScreens  )
	{
		C_NeurotoxinCountdown *pNeurotoxinCountdownTemp = g_NeurotoxinCountdowns[ iDisplayScreens ];
		if ( pNeurotoxinCountdownTemp && pNeurotoxinCountdownTemp->IsEnabled() )
		{
			pNeurotoxinCountdown = pNeurotoxinCountdownTemp;
			break;
		}
	}

	if( !pNeurotoxinCountdown )
	{
		// All display screens are disabled
		if ( bIsAlreadyVisible )
		{
			SetVisible( false );
			bIsAlreadyVisible = false;
		}
		return;
	}

	if ( !bIsAlreadyVisible )
	{
		SetVisible( true );
		bIsAlreadyVisible = true;
	}

	Update( pNeurotoxinCountdown );
}

void CNeurotoxinCountdownScreen::Update( C_NeurotoxinCountdown *pNeurotoxinCountdown )
{
	char szBuff[ 256 ];

	char szMinutesBuff[ 4 ];
	char szSecondsBuff[ 4 ];
	char szMillisecondsBuff[ 4 ];

	int iMinutes = pNeurotoxinCountdown->GetMinutes();
	int iSeconds = pNeurotoxinCountdown->GetSeconds();
	int iMilliseconds;
	
	if ( iMinutes <= 0 && iSeconds <= 0 )
	{
		iMinutes = 0;
		iSeconds = 0;
		iMilliseconds = 0;

		// Blink!
		m_pDisplayTextLabel->SetVisible( static_cast<int>( gpGlobals->curtime * 10.0f ) % 2 == 0 );
	}
	else
	{
		iMilliseconds = pNeurotoxinCountdown->GetMilliseconds();
		m_pDisplayTextLabel->SetVisible( true );
	}

	if ( iMinutes < 10 )
		sprintf( szMinutesBuff, "0%i", iMinutes );
	else
		sprintf( szMinutesBuff, "%i", iMinutes );

	if ( iSeconds < 10 )
		sprintf( szSecondsBuff, "0%i", iSeconds );
	else
		sprintf( szSecondsBuff, "%i", iSeconds );

	if ( iMilliseconds < 10 )
		sprintf( szMillisecondsBuff, "0%i", iMilliseconds );
	else
		sprintf( szMillisecondsBuff, "%i", iMilliseconds );

	sprintf( szBuff, "%s:%s:%s", szMinutesBuff, szSecondsBuff, szMillisecondsBuff );
	m_pDisplayTextLabel->SetText( szBuff );
}
