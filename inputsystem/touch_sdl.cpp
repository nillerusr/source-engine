//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Linux/Android touch implementation for inputsystem
//
//===========================================================================//

/* For force feedback testing. */
#include "inputsystem.h"
#include "tier1/convar.h"
#include "tier0/icommandline.h"
#include "SDL.h"
#include "SDL_touch.h"
// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Handle the events coming from the Touch SDL subsystem.
//-----------------------------------------------------------------------------
int TouchSDLWatcher( void *userInfo, SDL_Event *event )
{
	CInputSystem *pInputSystem = (CInputSystem *)userInfo;

	if( !event || !pInputSystem ) return 1;

	switch ( event->type ) {
	case SDL_FINGERDOWN:
		pInputSystem->FingerEvent( IE_FingerDown, event->tfinger.fingerId, event->tfinger.x, event->tfinger.y, event->tfinger.dx, event->tfinger.dy );
		break;
	case SDL_FINGERUP:
		pInputSystem->FingerEvent( IE_FingerUp, event->tfinger.fingerId, event->tfinger.x, event->tfinger.y, event->tfinger.dx, event->tfinger.dy );
		break;
	case SDL_FINGERMOTION:
		pInputSystem->FingerEvent( IE_FingerMotion ,event->tfinger.fingerId, event->tfinger.x, event->tfinger.y, event->tfinger.dx, event->tfinger.dy );
		break;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Initialize all joysticks
//-----------------------------------------------------------------------------
void CInputSystem::InitializeTouch( void )
{
	if ( m_bTouchInitialized )
		ShutdownTouch();

	// abort startup if user requests no touch
	if ( CommandLine()->FindParm("-notouch") ) return;

	memset( m_touchAccumX, 0, sizeof(m_touchAccumX) );
	memset( m_touchAccumY, 0, sizeof(m_touchAccumY) );

	m_bJoystickInitialized = true;
	SDL_AddEventWatch(TouchSDLWatcher, this);
}

void CInputSystem::ShutdownTouch()
{
	if ( !m_bTouchInitialized )
		return;

	SDL_DelEventWatch( TouchSDLWatcher, this );
	m_bTouchInitialized = false;
}

bool CInputSystem::GetTouchAccumulators( int fingerId, float &dx, float &dy )
{
	dx = m_touchAccumX[fingerId];
	dy = m_touchAccumY[fingerId];

	m_touchAccumX[fingerId] = m_touchAccumY[fingerId] = 0.f;

	return true;
}

void CInputSystem::FingerEvent(int eventType, int fingerId, float x, float y, float dx, float dy)
{
	if( fingerId >= TOUCH_FINGER_MAX_COUNT )
		return;

	if( eventType == IE_FingerUp )
	{
		m_touchAccumX[fingerId] = 0.f;
		m_touchAccumY[fingerId] = 0.f;
	}
	else
	{
		m_touchAccumX[fingerId] += dx;
		m_touchAccumY[fingerId] += dy;
	}

	int _x,_y;
	memcpy( &_x, &x, sizeof(float) );
	memcpy( &_y, &y, sizeof(float) );
	PostEvent(eventType, m_nLastSampleTick, fingerId, _x, _y);
}

