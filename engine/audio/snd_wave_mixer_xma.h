//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef SND_WAVE_MIXER_XMA_H
#define SND_WAVE_MIXER_XMA_H
#pragma once

class CAudioMixer;
class IWaveData;

// xma must be decoded as atomic blocks
#define XMA_BLOCK_SIZE			( 2 * 1024 )

// cannot be made slower than 15ms
// cannot be made faster than 5ms
// xma hardware needs be have stable clocking
#define XMA_POLL_RATE			15

CAudioMixer *CreateXMAMixer( IWaveData *data, int initialStreamPosition );

#endif // SND_WAVE_MIXER_XMA_H
