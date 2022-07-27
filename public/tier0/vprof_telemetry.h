//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Real-Time Hierarchical Telemetry Profiling
//
// $NoKeywords: $
//=============================================================================//

#ifndef VPROF_TELEMETRY_H
#define VPROF_TELEMETRY_H

#define tmZone(...)
#define tmTryLockEx(...)
#define tmSetLockStateEx(...)
#define tmEndTryLockEx(...)
#define tmZoneFiltered(...)
#define tmDynamicString(...) ""
#define tmEnter(...)
#define tmMessage(...)
#define tmBeginTimeSpanAt(...)
#define tmEndTimeSpanAt(...)
#define tmLeave(...)
#define TM_MESSAGE(...)
#define TM_ENTER(...)
#define TM_LEAVE(...)
#define TM_ZONE(...)
#define TM_ZONE_DEFAULT(...)
#define TM_ZONE_DEFAULT_PARAM(...)
#define TelemetryTick(...)
#define tmPlotI32(...)
#define TelemetrySetLockName(...)
#define tmEndTryLock(...)
#define tmTryLock(...)
#define tmSetLockState(...)

typedef unsigned long long TmU64;

#endif // VPROF_TELEMETRY_H
