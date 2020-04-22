//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_WAVE_MIXER_H
#define SND_WAVE_MIXER_H
#pragma once

class IWaveData;
class CAudioMixer;

CAudioMixer *CreateWaveMixer( IWaveData *data, int format, int channels, int bits, int initialStreamPosition );


#endif // SND_WAVE_MIXER_H
