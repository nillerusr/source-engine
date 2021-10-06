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

	SDL_Window *window = SDL_GetWindowFromID(event->tfinger.windowID);
	if( !window )
		return 0;
	
	int width, height;
	width = height = 0;
	SDL_GetWindowSize(window, &width, &height);
	
	switch ( event->type ) {
	case SDL_FINGERDOWN:
		pInputSystem->FingerDown( event->tfinger.fingerId, event->tfinger.x*width, event->tfinger.y*height );
		break;
	case SDL_FINGERUP:
		pInputSystem->FingerUp( event->tfinger.fingerId, event->tfinger.x*width, event->tfinger.y*height );
		break;
	case SDL_FINGERMOTION:
		pInputSystem->FingerMotion( event->tfinger.fingerId, event->tfinger.x*width, event->tfinger.y*height );
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

void CInputSystem::FingerDown(int fingerId, int x, int y)
{
	m_touchAccumEvent = IE_FingerDown;
	m_touchAccumFingerId = fingerId;
	m_touchAccumX = x;
	m_touchAccumY = y;
	
	PostEvent(IE_FingerDown, m_nLastSampleTick, fingerId, x, y);
}

void CInputSystem::FingerUp(int fingerId, int x, int y)
{
	m_touchAccumEvent = IE_FingerUp;
	m_touchAccumFingerId = fingerId;
	m_touchAccumX = x;
	m_touchAccumY = y;
	
	PostEvent(IE_FingerUp, m_nLastSampleTick, fingerId, x, y);	
}

void CInputSystem::FingerMotion(int fingerId, int x, int y)
{
	m_touchAccumEvent = IE_FingerMotion;	
	m_touchAccumFingerId = fingerId;
	m_touchAccumX = x;
	m_touchAccumY = y;
	
	PostEvent(IE_FingerMotion, m_nLastSampleTick, fingerId, x, y);	
}
