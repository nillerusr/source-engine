//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CWAPI code for GC access to the Web API server
//
//=============================================================================

#include "stdafx.h"
#include "gcwebapi.h"
#include "enumutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CUtlVector< GCWebAPIInterfaceMapCreationFunc_t > & CGCWebAPIInterfaceMapRegistrar::VecInstance()
{
	static CUtlVector< GCWebAPIInterfaceMapCreationFunc_t > sm_vecInterfaceFuncs;
	return sm_vecInterfaceFuncs;
}


ENUMSTRINGS_START( EWebApiParamType )
{ k_EWebApiParamTypeInvalid, "Invalid" },
{ k_EWebApiParamTypeInt32, "int32" },
{ k_EWebApiParamTypeUInt32, "uint32" },
{ k_EWebApiParamTypeInt64, "int64" },
{ k_EWebApiParamTypeUInt64, "uint64" },
{ k_EWebApiParamTypeFloat, "float" },
{ k_EWebApiParamTypeString, "string" },
{ k_EWebApiParamTypeBool, "bool" },
{ k_EWebApiParamTypeRawBinary, "rawbinary" }
ENUMSTRINGS_REVERSE( EWebApiParamType, k_EWebApiParamTypeInvalid )
