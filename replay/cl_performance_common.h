//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef CL_PERFORMANCE_COMMON_H
#define CL_PERFORMANCE_COMMON_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/performance.h"

//----------------------------------------------------------------------------------------

enum PerformanceEventType_t
{
	EVENTTYPE_INVALID,

	EVENTTYPE_CAMERA_CHANGE_BEGIN,
	EVENTTYPE_CAMERA_CHANGE_FIRSTPERSON = EVENTTYPE_CAMERA_CHANGE_BEGIN,
	EVENTTYPE_CAMERA_CHANGE_THIRDPERSON,
	EVENTTYPE_CAMERA_CHANGE_FREE,
	EVENTTYPE_CAMERA_CHANGE_END = EVENTTYPE_CAMERA_CHANGE_FREE,

	EVENTTYPE_CHANGEPLAYER = EVENTTYPE_CAMERA_CHANGE_END + 512,	// Leave plenty of room for camera types

	EVENTTYPE_CAMERA_SETVIEW,
	EVENTTYPE_TIMESCALE,
};

//----------------------------------------------------------------------------------------

#endif // CL_PERFORMANCE_COMMON_H
