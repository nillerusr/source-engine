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

class CWaveData;
class CAudioMixer;

CAudioMixer *CreateWaveMixer( CWaveData *data, int format, int channels, int bits );


#endif // SND_WAVE_MIXER_H
