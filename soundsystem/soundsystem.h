//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: DLL interface for low-level sound utilities
//
//===========================================================================//

#ifndef SOUNDSYSTEM_H
#define SOUNDSYSTEM_H

#ifdef _WIN32
#pragma once
#endif


#include "tier2/tier2.h"
#include "tier3/tier3.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class ISoundSystem;
class ISoundSystemServices;
class IAudioDevice;


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
extern ISoundSystem *g_pSoundSystem;
extern ISoundSystemServices *g_pSoundServices;
extern IAudioDevice *g_pAudioDevice;


#endif // SOUNDSYSTEM_H
