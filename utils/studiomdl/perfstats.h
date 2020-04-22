//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PERFSTATS_H
#define PERFSTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "studio.h"
#include "optimize.h"

enum
{
	SPEWPERFSTATS_SHOWSTUDIORENDERWARNINGS = 1,
	SPEWPERFSTATS_SHOWPERF = 2,
	SPEWPERFSTATS_SPREADSHEET = 4,
};

void SpewPerfStats( studiohdr_t *pStudioHdr, const char *pFilename, unsigned int flags );

#endif // PERFSTATS_H
