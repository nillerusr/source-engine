//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HUD Timer
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "hud_numeric.h"
#include "c_basetfplayer.h"
#include "hud_timer.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>

#define MIN_TIMER_ALPHA		192

using namespace vgui;

DECLARE_HUDELEMENT( CHudTimer );
DECLARE_HUD_MESSAGE( CHudTimer, StartTimer );
DECLARE_HUD_MESSAGE( CHudTimer, SetTimer );
DECLARE_HUD_MESSAGE( CHudTimer, UpdateTimer );

//-----------------------------------------------------------------------------
// Purpose: Create the Timer
//-----------------------------------------------------------------------------
CHudTimer::CHudTimer( const char *pElementName ) : CHudNumeric( pElementName, "HudTimer" )
{
	SetDrawLabel( false );
	SetDoPulses( false );
	
	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: Reset Timer values
//-----------------------------------------------------------------------------
void CHudTimer::Init( void )
{
	m_iAlpha = MIN_TIMER_ALPHA;
	m_flCurrentTime = 0.0;
	m_flStartTime = 0.0;
	m_flPrevTimeSlice = 0.0;
	m_iPaused = false;
	m_bFixedTime = true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the Timer's start time, and tells the timer to go into fixed time mode.
//-----------------------------------------------------------------------------
void CHudTimer::SetFixedTimer( float flStartTime, float flTimeLimit )
{
	Init();

	// Setup the Timer values
	m_flStartTime = flStartTime;
	m_flTimeLimit = flTimeLimit;
	m_bFixedTime = true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the Timer to a Time, and tells the timer to go into no fixed time mode.
//-----------------------------------------------------------------------------
void CHudTimer::SetNoFixedTimer( float flTime )
{
	Init();

	// Setup the Timer values
	m_flStartTime = m_flCurrentTime = flTime;
	m_bFixedTime = false;

	// Timer starts paused
	StopTimer();
}

//-----------------------------------------------------------------------------
// Purpose: Updates the Timer to a specific timer without doing anything else
//-----------------------------------------------------------------------------
void CHudTimer::UpdateTimer( float flTime )
{
	m_flCurrentTime = flTime;
	m_flPrevTimeSlice = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Start the Timer
//-----------------------------------------------------------------------------
void CHudTimer::StartTimer( void )
{
	// Ignore start / stop if I'm in fixed time mode
	if ( m_bFixedTime )
		return;
	if ( !m_iPaused )
		return;

	// Start timer
	m_flPrevTimeSlice = gpGlobals->curtime;
	m_iPaused = false;

	// Start with a short pulse
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("TimerPulse");
}

//-----------------------------------------------------------------------------
// Purpose: Stop the Timer, without resetting it
//-----------------------------------------------------------------------------
void CHudTimer::StopTimer( void )
{
	// Ignore start / stop if I'm in fixed time mode
	if ( m_bFixedTime )
		return;
	if ( m_iPaused )
		return;

	// Pause timer values
	m_iPaused = true;

	// Stop with a short pulse
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("TimerPulse");
}

//-----------------------------------------------------------------------------
// Purpose: Get back a color for the timer from a Green <-> Yellow <-> Red ramp
//-----------------------------------------------------------------------------
Color CHudTimer::GetColor( void )
{
	Color clr = Color( 0, 0, 0, 0 );

	float flPercentagePassed = 1 - (m_flCurrentTime / m_flTimeLimit);

	if ( flPercentagePassed < 0.75 )
	{
		clr = m_TextColor;
	}
	else if ( flPercentagePassed < 0.9 )
	{
		clr = m_TextColorWarning;
	}
	else
	{
		clr = m_TextColorCritical;
	}

	// Handle pulses ( which brighten the timer & change the font )
	clr[3] = clamp( MIN_TIMER_ALPHA + ( m_flBlur ) * 128, 0, 255 );

	// Low timer always overrides to make it bright
	if ( flPercentagePassed > 0.99 )
	{
		clr[3] = 255;
	}

	return clr;
}

//-----------------------------------------------------------------------------
// Purpose: Get back a color for the timer from a Green <-> Yellow <-> Red ramp
//-----------------------------------------------------------------------------
Color CHudTimer::GetBoxColor()
{
	Color boxColor = Color( 0, 0, 0, 0 );

	float flPercentagePassed = 1 - (m_flCurrentTime / m_flTimeLimit);
	if ( flPercentagePassed < 0.75 )
	{
		boxColor = m_BoxColor;
	}
	else if ( flPercentagePassed < 0.9 )
	{
		boxColor = m_BoxColorWarning;
	}
	else
	{
		boxColor = m_BoxColorCritical;
	}

	return boxColor;
}

bool CHudTimer::GetValue( char *value, int maxlen )
{
	if ( m_flStartTime == 0.0 )
		return false;

	// Convert time to Minutes and Seconds (prevent negative times)
	int iTimerMinutes = MAX( 0, ((int)m_flCurrentTime) / 60 );
	int iTimerSeconds = MAX( 0, ((int)m_flCurrentTime) % 60 );

	Q_snprintf( value, maxlen, "%02d:%.2d", iTimerMinutes, iTimerSeconds );
	return true;
}

void CHudTimer::OnThink( void )
{
	if ( m_flStartTime == 0.0 )
		return;

	// If we're in fixed time mode, calculate the current time
	if ( m_bFixedTime )
	{
		// Don't paint at all if we have no timelimit
		if ( !m_flTimeLimit )
			return;

		m_flCurrentTime = gpGlobals->curtime - m_flStartTime;

		// If we have a timelimit, count down
		if ( m_flTimeLimit )
		{
			m_flCurrentTime = m_flTimeLimit - m_flCurrentTime;
		}

		CheckForPulse();
	}

	// Increment the time if the Timer's going, and we're not in fixed time mode.
	if ( !m_iPaused && !m_bFixedTime )
	{
		m_flCurrentTime -= gpGlobals->curtime - m_flPrevTimeSlice;
		m_flPrevTimeSlice = gpGlobals->curtime;

		// Hit the end?
		if ( m_flCurrentTime <= 0.0 )
		{
			StopTimer();
			m_flCurrentTime = 0.0;
		}
	}

	m_flLastTime = m_flCurrentTime;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if the Timer should pulse
//-----------------------------------------------------------------------------
void CHudTimer::CheckForPulse( void )
{
	int pulseInterval = 60;
	if ( m_flCurrentTime <= 60 )
	{
		pulseInterval = 10;
	}

	// See if we've crossed a minute boundary
	int iLastTimerMinutes = ((int)m_flLastTime) / pulseInterval;
	// will we get there in next half second
	int iTimerMinutes = ((int)m_flCurrentTime - 0.5f ) / pulseInterval;
	if ( iLastTimerMinutes != iTimerMinutes )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("TimerPulse");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Message Handler for the Timer Set
//-----------------------------------------------------------------------------
int CHudTimer::MsgFunc_SetTimer(bf_read &msg)
{
	float flTimeToSetTo = msg.ReadBitCoord();
	SetNoFixedTimer( flTimeToSetTo );

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Message Handler for the Timer Update
// Updates are like sets, but they don't re-evaluate the start time
// They're used to just ensure the Client Timers don't get too far out of synch with the server's timer
//-----------------------------------------------------------------------------
int CHudTimer::MsgFunc_UpdateTimer(bf_read &msg)
{
	float flTimeToSetTo = msg.ReadBitCoord();
	UpdateTimer( flTimeToSetTo );

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Message Handler for the Timer Start/Stop
//-----------------------------------------------------------------------------
int CHudTimer::MsgFunc_StartTimer( bf_read &msg )
{
	if ( msg.ReadByte() )
	{
		// Start the Timer
		StartTimer();
	}
	else
	{
		// Pause the Timer
		StopTimer();
	}

	return 1;	
}