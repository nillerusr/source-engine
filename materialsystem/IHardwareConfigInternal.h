//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#ifndef IHARDWARECONFIGINTERNAL_H
#define IHARDWARECONFIGINTERNAL_H

#ifdef _WIN32
#pragma once
#endif


#include "materialsystem/imaterialsystemhardwareconfig.h"

//-----------------------------------------------------------------------------
// Material system configuration
//-----------------------------------------------------------------------------
class IHardwareConfigInternal : public IMaterialSystemHardwareConfig
{
public:
	// Gets at the HW specific shader DLL name
	virtual const char *GetHWSpecificShaderDLLName() const = 0;
};


#endif // IHARDWARECONFIGINTERNAL_H
