//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CEconExperiment
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFEXPERIMENT_H
#define TFEXPERIMENT_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/schemasharedobject.h"

//---------------------------------------------------------------------------------
// Purpose: 
//---------------------------------------------------------------------------------
class CEconExperiment : public GCSDK::CSchemaSharedObject< CSchExperiment, k_EEconTypeExperiment >
{
#ifdef GC_DLL
	DECLARE_CLASS_MEMPOOL( CEconExperiment );
#endif

};

#endif // TFEXPERIMENT_H
