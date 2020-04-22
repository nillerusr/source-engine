//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DEBUGTIMER_H
#define DEBUGTIMER_H
#ifdef _WIN32
#pragma once
#endif

// resets the timer
void Timer_Start();

// returns the time since Timer_Start() was called, in seconds
double Timer_End();



#endif // DEBUGTIMER_H
