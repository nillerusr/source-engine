//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "econ_experiment.h"

using namespace GCSDK;

#ifdef GC_DLL
IMPLEMENT_CLASS_MEMPOOL( CEconExperiment, 10 * 10000, UTLMEMORYPOOL_GROW_SLOW );
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
