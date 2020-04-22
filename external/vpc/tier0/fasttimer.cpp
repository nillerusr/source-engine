//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"

#include <stdio.h>
#include "tier0/fasttimer.h"

// NOTE: This has to be the last file included!
//#include "tier0/memdbgon.h"

uint64 g_ClockSpeed;	// Clocks/sec
unsigned long g_dwClockSpeed;
double g_ClockSpeedMicrosecondsMultiplier;
double g_ClockSpeedMillisecondsMultiplier;
double g_ClockSpeedSecondsMultiplier;

// Constructor init the clock speed.
CClockSpeedInit g_ClockSpeedInit;
