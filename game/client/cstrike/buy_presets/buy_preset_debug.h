//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BUY_PRESET_DEBUG_H
#define BUY_PRESET_DEBUG_H
#ifdef _WIN32
#pragma once
#endif

//--------------------------------------------------------------------------------------------------------------
// Utility functions for the debugging macros below
bool IsPresetDebuggingEnabled();			///< cl_preset_debug >= 1.0f
bool IsPresetFullCostDebuggingEnabled();	///< cl_preset_debug == 2.0f
bool IsPresetCurrentCostDebuggingEnabled();	///< cl_preset_debug == 3.0f

//--------------------------------------------------------------------------------------------------------------
// Macros for conditional debugging of buy presets
#define PRESET_DEBUG		if (IsPresetDebuggingEnabled())				DevMsg
#define FULLCOST_DEBUG		if (IsPresetFullCostDebuggingEnabled())		DevMsg
#define CURRENTCOST_DEBUG	if (IsPresetCurrentCostDebuggingEnabled())	DevMsg

#endif // BUY_PRESET_DEBUG_H
