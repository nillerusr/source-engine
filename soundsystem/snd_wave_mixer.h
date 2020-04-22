//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef SND_WAVE_MIXER_H
#define SND_WAVE_MIXER_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CWaveData;
class CAudioMixer;


//-----------------------------------------------------------------------------
// Wave mixer
//-----------------------------------------------------------------------------
CAudioMixer *CreateWaveMixer( CWaveData *data, int format, int channels, int bits );


#endif // SND_WAVE_MIXER_H
