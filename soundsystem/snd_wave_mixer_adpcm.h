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
class CWaveData;

CAudioMixer *CreateADPCMMixer( CWaveData *data );

#endif // SND_WAVE_MIXER_ADPCM_H
