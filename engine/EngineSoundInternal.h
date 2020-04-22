//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENGINESOUNDINTERNAL_H
#define ENGINESOUNDINTERNAL_H

#if defined( _WIN32 )
#pragma once
#endif

#include "engine/IEngineSound.h"

//-----------------------------------------------------------------------------
// Method to get at the singleton implementations of the engine sound interfaces
//-----------------------------------------------------------------------------
IEngineSound* EngineSoundClient();
IEngineSound* EngineSoundServer();

#endif // SOUNDENGINEINTERNAL_H
