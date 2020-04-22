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

#ifndef SND_WAVE_MIXER_ADPCM_H
#define SND_WAVE_MIXER_ADPCM_H
#pragma once


class CAudioMixer;
class IWaveData;

CAudioMixer *CreateADPCMMixer( IWaveData *data );

#endif // SND_WAVE_MIXER_ADPCM_H
