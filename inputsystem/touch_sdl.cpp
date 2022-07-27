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

void CInputSystem::FingerEvent(int eventType, int fingerId, float x, float y, float dx, float dy)
{
	// Shit, but should work with arm/x86

	int data0 = fingerId << 16 | eventType;
	int _x = (int)((double)x*0xFFFF);
	int _y = (int)((double)y*0xFFFF);
	int data1 = _x << 16 | (_y & 0xFFFF);

	union{int i;float f;} ifconv;
	ifconv.f = dx;
	int _dx = ifconv.i;
	ifconv.f = dy;
	int _dy = ifconv.i;

	PostEvent(data0, m_nLastSampleTick, data1, _dx, _dy);
}
