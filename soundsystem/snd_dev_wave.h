//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_DEV_WAVE_H
#define SND_DEV_WAVE_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IAudioDevice;


//-----------------------------------------------------------------------------
// Creates a device that mixes WAVs using windows
//-----------------------------------------------------------------------------
IAudioDevice *Audio_CreateWaveDevice( void );


#endif // SND_DEV_WAVE_H
