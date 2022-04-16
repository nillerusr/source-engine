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
#include "c_prop_portal_stats_display.h"
#include "ienginevgui.h"
#include "fmtstr.h"
#include "portal_shareddefs.h"

using namespace vgui;


#define TESTCHAMBER_STATS_FIRST_MEDAL "MedalPortalGold"
#define TESTCHAMBER_STATS_FIRST_MEDAL_POINTER "MedalPointer00"
#define TESTCHAMBER_STATS_FIRST_SUBJECT_REACTION "SubjectReactionIdle"


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CPortalStatsDisplayScreen : public CVGuiScreenPanel
{
	DECLARE_CLASS( CPortalStatsDisplayScreen, CVGuiScreenPanel );

public:
	CPortalStatsDisplayScreen( vgui::Panel *parent, const char *panelName );
	~CPortalStatsDisplayScreen();

	virtual void ApplySchemeSettings( IScheme *pScheme );

	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();

private:
	void UpdateTextFields( C_PropPortalStatsDisplay *pPropPortalStatsDisplay );
	void UpdateMedals( C_PropPortalStatsDisplay *pPropPortalStatsDisplay );

private:
	vgui::Label *m_pNumPlayerLabel;
	vgui::Label *m_pNumGoalLabel;
	vgui::Label *m_pCheatedLabel;

	float m_flNextDigitRandomizeTime;	//next time to grab a new digit while scrolling random digits
										//in un-decoded digits
	int m_iLastRandomInt;				//store the random digit between new rand calls

	bool m_bInitLabelColor;

	Color m_cPass;
	Color m_cFail;
	Color m_cUnknown;
	Color m_cInvisible;
};


DECLARE_VGUI_SCREEN_FACTORY( CPortalStatsDisplayScreen, "portal_stats_display_screen" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CPortalStatsDisplayScreen::CPortalStatsDisplayScreen( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CPortalStatsDisplayScreen", vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/PortalStatsDisplayScreen.res", "PortalStatsDisplayScreens" ) ) 
{
	m_pNumPlayerLabel = new vgui::Label( this, "NumPlayer", "" );
	m_pNumGoalLabel = new vgui::Label( this, "NumGoal", "" );
	m_pCheatedLabel = new vgui::Label( this, "CheatedLabel", "" );
	
	m_flNextDigitRandomizeTime = 0;
	m_iLastRandomInt = 0;

	m_bInitLabelColor = true;
}

CPortalStatsDisplayScreen::~CPortalStatsDisplayScreen()
{
}

void CPortalStatsDisplayScreen::ApplySchemeSettings( IScheme *pScheme )
{
	assert( pScheme );

	m_cPass = pScheme->GetColor( "CPortalStatsDisplayScreen_Pass", GetFgColor() );
	m_cFail = pScheme->GetColor( "CPortalStatsDisplayScreen_Fail", GetFgColor() );
	m_cUnknown = pScheme->GetColor( "CPortalStatsDisplayScreen_Unknown", GetFgColor() );
	m_cInvisible = Color( 0, 0, 0, 0 );	

	if( m_bInitLabelColor )
	{
		m_pNumPlayerLabel->SetFgColor( m_cUnknown );
		m_pNumGoalLabel->SetFgColor( m_cUnknown );

		m_bInitLabelColor = false;
	}
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CPortalStatsDisplayScreen::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}

void MakeTimeString( CFmtStr &str, int iHours, int iMinutes, int iSeconds )
{
	if ( iHours )
	{
		if ( iMinutes < 10 )
		{
			if ( iSeconds < 10 )
				str.sprintf( "%d:0%d:0%d", iHours, iMinutes, iSeconds );
			else
				str.sprintf( "%d:0%d:%d", iHours, iMinutes, iSeconds );
		}
		else
		{
			if ( iSeconds < 10 )
				str.sprintf( "%d:%d:0%d", iHours, iMinutes, iSeconds );
			else
				str.sprintf( "%d:%d:%d", iHours, iMinutes, iSeconds );
		}
	}
	else
	{
		if ( iSeconds < 10 )
			str.sprintf( "%d:0%d", iMinutes, iSeconds );
		else
			str.sprintf( "%d:%d", iMinutes, iSeconds );
	}
}

//-----------------------------------------------------------------------------
// Update the display string
//-----------------------------------------------------------------------------
void CPortalStatsDisplayScreen::OnTick()
{
	BaseClass::OnTick();

	if ( g_PropPortalStatsDisplays.Count() <= 0 )
		return;

	C_PropPortalStatsDisplay *pPropPortalStatsDisplay = NULL;

	for ( int iDisplayScreens = 0; iDisplayScreens < g_PropPortalStatsDisplays.Count(); ++iDisplayScreens  )
	{
		C_PropPortalStatsDisplay *pPropPortalStatsDisplayTemp = g_PropPortalStatsDisplays[ iDisplayScreens ];
		if ( pPropPortalStatsDisplayTemp && pPropPortalStatsDisplayTemp->IsEnabled() )
		{
			pPropPortalStatsDisplay = pPropPortalStatsDisplayTemp;
			break;
		}
	}

	if( !pPropPortalStatsDisplay )
	{
		// All display screens are disabled
		SetVisible( false );
		return;
	}

	SetVisible( true );

	UpdateTextFields( pPropPortalStatsDisplay );
	UpdateMedals( pPropPortalStatsDisplay );
}

void CPortalStatsDisplayScreen::UpdateTextFields( C_PropPortalStatsDisplay *pPropPortalStatsDisplay )
{
	bool bIsTime = pPropPortalStatsDisplay->IsTime();
	float fNumPlayer = pPropPortalStatsDisplay->GetNumPlayerDisplay();
	float fNumGoal = pPropPortalStatsDisplay->GetNumGoalDisplay();

	// Figure out colors

	Color *( pColors[ 3 ] ) = { &m_cUnknown, &m_cFail, &m_cPass };

	int iColor = pPropPortalStatsDisplay->GetGoalSuccess() + 1;

	m_pNumPlayerLabel->SetFgColor( ( fNumPlayer != 0.0f ) ? ( *pColors[ iColor ] ) : ( m_cInvisible ) );
	m_pNumGoalLabel->SetFgColor( ( pPropPortalStatsDisplay->GetGoalVisible() ) ? ( *pColors[ iColor ] ) : ( m_cInvisible ) );

	// Fill in labels

	CFmtStr str;

	if ( !bIsTime )
	{
		str.sprintf( "%.0f", fNumPlayer );
		m_pNumPlayerLabel->SetText( str );

		str.sprintf( "%.0f", fNumGoal );
		m_pNumGoalLabel->SetText( str );
	}
	else
	{
		// break seconds into mnutes and seconds
		int iMinutes = static_cast<int>( fNumPlayer / 60.0f );
		int iHours = iMinutes / 60;
		iMinutes %= 60;
		int iSeconds = static_cast<int>( fNumPlayer ) % 60;
		MakeTimeString( str, iHours, iMinutes, iSeconds );
		m_pNumPlayerLabel->SetText( str );

		// break seconds into mnutes and seconds
		iMinutes = static_cast<int>( fNumGoal / 60.0f );
		iHours = iMinutes / 60;
		iMinutes %= 60;
		iSeconds = static_cast<int>( fNumGoal ) % 60;
		MakeTimeString( str, iHours, iMinutes, iSeconds );
		m_pNumGoalLabel->SetText( str );
	}

	// Draw cheated label when needed
	m_pCheatedLabel->SetVisible( pPropPortalStatsDisplay->HasCheated() );
}

int GetMedalOffset( int iObjective, int iLevel )
{
	return iObjective * 6 + ( 2 - iLevel ) * 2;
}

void CPortalStatsDisplayScreen::UpdateMedals( C_PropPortalStatsDisplay *pPropPortalStatsDisplay )
{
	// Get the index of the first medal image
	int iFirstChildIndex = FindChildIndexByName( TESTCHAMBER_STATS_FIRST_MEDAL );

	CBitmapPanel *pBitmapPanel;

	// Loop through all the goals
	for ( int iGoal = 0; iGoal < PORTAL_LEVEL_STAT_TOTAL; ++iGoal )
	{
		int iMedalComplete = -1;

		// Loop through bronze, silver, and gold
		for ( int iLevel = 0; iLevel < 3; ++iLevel )
		{
			if ( pPropPortalStatsDisplay->IsMedalCompleted( iLevel ) )
				iMedalComplete = iLevel;
		}

		// Loop through bronze, silver, and gold
		for ( int iLevel = 0; iLevel < 3; ++iLevel )
		{
			int iMedalIndex = iFirstChildIndex + GetMedalOffset( iGoal, iLevel );

			if ( iGoal != pPropPortalStatsDisplay->GetDisplayObjective() )
			{
				// Set both invisible
				pBitmapPanel = dynamic_cast<CBitmapPanel*>( GetChild( iMedalIndex ) );
				pBitmapPanel->SetVisible( false );

				// Set if "no medal" is visible
				pBitmapPanel = dynamic_cast<CBitmapPanel*>( GetChild( iMedalIndex + 1 ) );
				pBitmapPanel->SetVisible( false );

			}
			else
			{
				// Set if medal is visible
				pBitmapPanel = dynamic_cast<CBitmapPanel*>( GetChild( iMedalIndex ) );
				pBitmapPanel->SetVisible( ( iLevel == iMedalComplete ) ? ( true ) : ( false ) );

				// Set if "no medal" is visible
				pBitmapPanel = dynamic_cast<CBitmapPanel*>( GetChild( iMedalIndex + 1 ) );
				pBitmapPanel->SetVisible( ( iMedalComplete == -1 && iLevel == 0 ) ? ( true ) : ( false ) );
			}
		}
	}

	// Get the index of the first pointer
	iFirstChildIndex = FindChildIndexByName( TESTCHAMBER_STATS_FIRST_MEDAL_POINTER );

	// Loop through bronze, silver, and gold
	for ( int iLevel = 0; iLevel < 3; ++iLevel )
	{
		pBitmapPanel = dynamic_cast<CBitmapPanel*>( GetChild( iFirstChildIndex + iLevel ) );
		pBitmapPanel->SetVisible( ( iLevel == pPropPortalStatsDisplay->GetGoalLevelDisplay() ) ? ( true ) : ( false ) );
	}

	int iSubjectReaction = pPropPortalStatsDisplay->GetGoalSuccess() + 1;

	// Get the index of the first subject reaction
	iFirstChildIndex = FindChildIndexByName( TESTCHAMBER_STATS_FIRST_SUBJECT_REACTION );

	// Loop through idle, happy, sad
	for ( int iReaction = 0; iReaction < 3; ++iReaction )
	{
		pBitmapPanel = dynamic_cast<CBitmapPanel*>( GetChild( iFirstChildIndex + iReaction ) );
		pBitmapPanel->SetVisible( ( iReaction == iSubjectReaction ) ? ( true ) : ( false ) );
	}
}