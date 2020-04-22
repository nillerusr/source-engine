//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Methods related to input
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef INPUT_H
#define INPUT_H

#ifdef _WIN32
#pragma once
#endif

struct InputEvent_t;


//-----------------------------------------------------------------------------
// Initializes the input system
//-----------------------------------------------------------------------------
void InitInput();


//-----------------------------------------------------------------------------
// Hooks input listening up to a window
//-----------------------------------------------------------------------------
void InputAttachToWindow(void *hwnd);
void InputDetachFromWindow(void *hwnd);

// If input isn't hooked, this forwards messages to vgui.
void InputHandleWindowMessage( void *hwnd, unsigned int uMsg, unsigned int wParam, long lParam );

//-----------------------------------------------------------------------------
// Handles an input event, returns true if the event should be filtered
// from the rest of the game
//-----------------------------------------------------------------------------
bool InputHandleInputEvent( const InputEvent_t &event );


//-----------------------------------------------------------------------------
// Enables/disables input (enabled by default)
//-----------------------------------------------------------------------------
void EnableInput( bool bEnable );


#endif	// INPUT_H