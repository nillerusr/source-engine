//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: event time stamping for hammer
//
//===========================================================================//

#include "stdafx.h"
#include "tier0/platform.h"
#include "mathlib/mathlib.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static int g_EventTimeCounters[100];
static float g_EventTimes[100];

void SignalUpdate(int ev)
{
	g_EventTimes[ev]=Plat_FloatTime();
	g_EventTimeCounters[ev]++;
}

int GetUpdateCounter(int ev)
{
	return g_EventTimeCounters[ev];
}

float GetUpdateTime(int ev)
{
	return g_EventTimes[ev];
}

void SignalGlobalUpdate(void)
{
	float stamp=Plat_FloatTime();
	for(int i=0;i<NELEMS(g_EventTimes);i++)
	{
		g_EventTimes[i] = stamp;
		g_EventTimeCounters[i]++;
	}
}
