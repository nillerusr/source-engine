//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================


#pragma once


#include "tier1/utlvector.h"
#include "tier1/utlsymbol.h"


//--------------------------------------------------------------------------------------------------
// Save KeyValues to a file with cleaner floats (only 1 trailing 0 after a decimal)
// and values lined up in each block
//--------------------------------------------------------------------------------------------------
void ProcessPaintKitKeyValuesFiles( const CUtlVector< CUtlSymbol > &workList );