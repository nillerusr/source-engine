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
#include <vgui/IVGui.h>
#include "c_plantedc4.h"
#include "ienginevgui.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CC4Panel : public CVGuiScreenPanel
{
	DECLARE_CLASS( CC4Panel, CVGuiScreenPanel );

public:
	CC4Panel( vgui::Panel *parent, const char *panelName );
	~CC4Panel();
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();

	virtual void ApplySchemeSettings( IScheme *pScheme );

private:
	vgui::Label *m_pTimeLabel;

	float m_flNextDigitRandomizeTime;	//next time to grab a new digit while scrolling random digits
										//in un-decoded digits
	int m_iLastRandomInt;				//store the random digit between new rand calls

	bool m_bInitLabelColor;

	Color m_cArmed;
	Color m_cDefused;
	Color m_cInvisible;
};


DECLARE_VGUI_SCREEN_FACTORY( CC4Panel, "c4_panel" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CC4Panel::CC4Panel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CC4Panel", vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/C4Panel.res", "ClientScheme" ) ) 
{
	SetSize( 10, 10 ); // Quiet "parent not sized yet" spew
	m_pTimeLabel = new vgui::Label( this, "TimerLabel", "" );
	
	m_flNextDigitRandomizeTime = 0;
	m_iLastRandomInt = 0;

	m_bInitLabelColor = true;
}

CC4Panel::~CC4Panel()
{
}

void CC4Panel::ApplySchemeSettings( IScheme *pScheme )
{
	assert( pScheme );

	m_cArmed = pScheme->GetColor( "C4Panel_Armed", GetFgColor() );
	m_cDefused = pScheme->GetColor( "C4Panel_Defused", GetFgColor() );
	m_cInvisible = Color( 0, 0, 0, 0 );	

	if( m_bInitLabelColor )
	{
		m_pTimeLabel->SetFgColor( m_cArmed );
		m_bInitLabelColor = false;
	}
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CC4Panel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}

//how long to spend decoding each digit
float flTransitionTimes[] = { 0.9, 0.8, 0.6, 0.45, 0.25, 0.15, 0.0 };

//the defuse code, taken from the view model animation, v_c4.mdl
char cDefuseCode[] = { '7', '3', '5', '5', '6', '0', '8', '\0' };	
char cArmedDisplay[] = { '*', '*', '*', '*', '*', '*', '*', '\0' };

//convert an integer into the readable character version of that number
#define INT_TO_CHAR(i)	( '0' + (i) )

//-----------------------------------------------------------------------------
// Update the display string
//-----------------------------------------------------------------------------
void CC4Panel::OnTick()
{
	BaseClass::OnTick();

	SetVisible( true );

	float flProgress = 1.0;

	if ( g_PlantedC4s.Count() > 0 )
	{
		C_PlantedC4 *pC4 = g_PlantedC4s[0];

		if( pC4 )
		{
			flProgress = pC4->GetDefuseProgress();
		}
		else
			return;
	}

	m_pTimeLabel->SetFgColor( m_cArmed );

	// If flProgress is less than 0, the bomb has been defused
	if( flProgress < 0.0 )
	{
		//Flash when the bomb has been defused
		if( flProgress > -0.2 )	//flash for 2 seconds
		{
			int x = (int)( flProgress * 100 );

			if( x % 2 == 0 )
				m_pTimeLabel->SetFgColor( m_cInvisible );
			else
				m_pTimeLabel->SetFgColor( m_cDefused );
		}
		else
			m_pTimeLabel->SetFgColor( m_cDefused );

		//Show the full, decoded defuse code
		m_pTimeLabel->SetText( cDefuseCode );
	}
	else if( flProgress < 1.0 )	//defuse in progress
	{
		//Initial display
		char buf[8];
		Q_strncpy( buf, cArmedDisplay, MIN( sizeof(buf), sizeof(cArmedDisplay) ) );

		int iDigitPos = 0;
		while( flProgress < flTransitionTimes[iDigitPos] )
		{
			//Fill in the previously decoded digits
			buf[iDigitPos] = cDefuseCode[iDigitPos];
			iDigitPos++;
		}

		//Animate the character that we're decoding
		//Value drawn will be based on how long we've been
		//decoding this character
		float flTimeInThisChar = 1.0 - flTransitionTimes[0];
		
		if( iDigitPos > 0 )
			flTimeInThisChar = flTransitionTimes[iDigitPos-1] - flTransitionTimes[iDigitPos];


		assert( flTimeInThisChar > 0.0 );


		float flPercentDecoding = ( flProgress - flTransitionTimes[iDigitPos] ) / flTimeInThisChar;
		
		//Determine when to next change the digit that we're decoding
		if( m_flNextDigitRandomizeTime < gpGlobals->curtime )
		{
			//Get a new random int to draw
			m_iLastRandomInt = RandomInt( 0, 9 );

			if( flPercentDecoding > 0.7 )
				m_flNextDigitRandomizeTime = gpGlobals->curtime + 0.05;
			else if( flPercentDecoding > 0.5 )
				m_flNextDigitRandomizeTime = gpGlobals->curtime + 0.1;
			else if( flPercentDecoding > 0.3 )
				m_flNextDigitRandomizeTime = gpGlobals->curtime + 0.15;
			else
				m_flNextDigitRandomizeTime = gpGlobals->curtime + 0.3;
		}

		//Settle on the real value if we're close
		if( flPercentDecoding < 0.2 )
			buf[iDigitPos] = cDefuseCode[iDigitPos];
		else	//else use a random digit
			buf[iDigitPos] = INT_TO_CHAR( m_iLastRandomInt );

		
		m_pTimeLabel->SetFgColor( m_cArmed );
		m_pTimeLabel->SetText( buf );
	}
	else
	{
		//Not being defused - draw the armed string
		m_pTimeLabel->SetFgColor( m_cArmed );
		m_pTimeLabel->SetText( cArmedDisplay );
	}	
}