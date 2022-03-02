//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2004

#include "cbase.h"
#include "buy_preset_debug.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _DEBUG
#define BUY_PRESET_DEBUGGING 1
#else
#define BUY_PRESET_DEBUGGING 0
#endif

#if BUY_PRESET_DEBUGGING

static ConVar cl_preset_debug( "cl_preset_debug", "0", 0, "Controls debug information about buy presets" );

//--------------------------------------------------------------------------------------------------------------
bool IsPresetDebuggingEnabled()
{
	return cl_preset_debug.GetInt() >= 1.0f;
}

//--------------------------------------------------------------------------------------------------------------
bool IsPresetFullCostDebuggingEnabled()
{
	return cl_preset_debug.GetInt() == 2.0f;
}

//--------------------------------------------------------------------------------------------------------------
bool IsPresetCurrentCostDebuggingEnabled()
{
	return cl_preset_debug.GetInt() == 3.0f;
}

#else // ! BUY_PRESET_DEBUGGING

//--------------------------------------------------------------------------------------------------------------
bool IsPresetDebuggingEnabled()
{
	return false;
}

//--------------------------------------------------------------------------------------------------------------
bool IsPresetFullCostDebuggingEnabled()
{
	return false;
}

//--------------------------------------------------------------------------------------------------------------
bool IsPresetCurrentCostDebuggingEnabled()
{
	return false;
}

#endif // ! BUY_PRESET_DEBUGGING

//--------------------------------------------------------------------------------------------------------------
