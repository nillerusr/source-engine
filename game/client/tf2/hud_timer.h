//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_TIMER_H
#define HUD_TIMER_H
#ifdef _WIN32
#pragma once
#endif

#include "hud_numeric.h"
#include <vgui_controls/Panel.h>


//-----------------------------------------------------------------------------
// Purpose: TF2 Countdown Timer
//-----------------------------------------------------------------------------
class CHudTimer : public CHudNumeric
{
	DECLARE_CLASS_SIMPLE( CHudTimer, CHudNumeric )
public:
	CHudTimer( const char *pElementName );

	virtual void Init( void );
	virtual void OnThink( void );

	virtual const char *GetLabelText() { return m_szTimerLabel; }
	virtual const char *GetPulseEvent( bool increment ) { return ""; }
	virtual bool		GetValue( char *val, int maxlen );

	virtual Color GetColor();
	virtual Color GetBoxColor();

	// Handling
	void SetNoFixedTimer( float flTime );
	void SetFixedTimer( float flStartTime, float flTimeLimit );
	void UpdateTimer( float flTime );
	void StartTimer( void );
	void StopTimer( void );
	void CheckForPulse( void );
	int  MsgFunc_SetTimer( bf_read &msg );
	int  MsgFunc_StartTimer( bf_read &msg );
	int  MsgFunc_UpdateTimer( bf_read &msg );

public:
	int		m_iAlpha;
	int		m_iPaused;
	float	m_flCurrentTime;
	float	m_flStartTime;
	float	m_flPrevTimeSlice;
	float	m_flPulseTime;
	float	m_flTimeLimit;
	float	m_flLastTime;

	// The Timer has two modes of operation:
	//	m_bFixedTime == true:  Timer can't be paused. Time displayed is an offset from the originally specified start time.
	//  m_bFixedTime == false: Timer can be stopped & started. Time displayed is calculated on the client, and regularly updated by the server
	//						   to ensure it doesn't get too far off the server's time.
	bool	m_bFixedTime;

private:
	float		m_flOpenCloseTime;

	CPanelAnimationStringVar( 128, m_szTimerLabel, "TimerLabel", "" );
};

#endif // HUD_TIMER_H
