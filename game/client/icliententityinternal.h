//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef ICLIENTENTITYINTERNAL_H
#define ICLIENTENTITYINTERNAL_H
#ifdef _WIN32
#pragma once
#endif

#include "IClientEntity.h"
#include "ClientLeafSystem.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

class ClientClass;


//-----------------------------------------------------------------------------
// represents a handle used only by the client DLL
//-----------------------------------------------------------------------------

typedef CBaseHandle ClientEntityHandle_t;
typedef unsigned short SpatialPartitionHandle_t;



#endif // ICLIENTENTITYINTERNAL_H