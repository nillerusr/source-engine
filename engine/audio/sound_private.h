//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "basetypes.h"
#include "snd_fixedint.h"

#ifndef SOUND_PRIVATE_H
#define SOUND_PRIVATE_H
#pragma once

// Forward declarations
struct portable_samplepair_t;
struct channel_t;
typedef int SoundSource;
class CAudioSource;
struct channel_t;
class CSfxTable;
class IAudioDevice;

// ====================================================================

#define		SAMPLE_16BIT_SHIFT		1

void S_Startup (void);
void S_FlushSoundData(int rate);

CAudioSource *S_LoadSound( CSfxTable *s, channel_t *ch );
void S_TouchSound (char *sample);
CSfxTable *S_FindName (const char *name, int *pfInCache);

// spatializes a channel
void SND_Spatialize(channel_t *ch);
void SND_ActivateChannel( channel_t *ch );

// shutdown the DMA xfer.
void SNDDMA_Shutdown(void);

// ====================================================================
// User-setable variables
// ====================================================================

extern int g_paintedtime;

extern bool	snd_initialized;

extern class Vector listener_origin;

void S_LocalSound (char *s);

void SND_InitScaletable (void);

void S_AmbientOff (void);
void S_AmbientOn (void);
void S_FreeChannel(channel_t *ch);

// resync the sample-timing adjustment clock (for scheduling a group of waves with precise timing - e.g. machine gun sounds)
extern void S_SyncClockAdjust( clocksync_index_t );

//=============================================================================

// UNDONE: Move this global?
extern IAudioDevice *g_AudioDevice;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void S_TransferStereo16 (void *pOutput, const portable_samplepair_t *pfront, int lpaintedtime, int endtime);
void S_TransferPaintBuffer(void *pOutput, const portable_samplepair_t *pfront, int lpaintedtime, int endtime);
void S_MixBufferUpsample2x( int count, portable_samplepair_t *pbuffer, portable_samplepair_t *pfiltermem, int cfltmem, int filtertype );

extern void Mix8MonoWavtype( channel_t *pChannel, portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount );
extern void Mix8StereoWavtype( channel_t *pChannel, portable_samplepair_t *pOutput, int *volume, byte *pData, int inputOffset, fixedint rateScaleFix, int outCount );
extern void Mix16MonoWavtype( channel_t *pChannel, portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount );
extern void Mix16StereoWavtype( channel_t *pChannel, portable_samplepair_t *pOutput, int *volume, short *pData, int inputOffset, fixedint rateScaleFix, int outCount );

extern void SND_MoveMouth8(channel_t *pChannel, CAudioSource *pSource, int count);
extern void SND_CloseMouth(channel_t *pChannel);
extern void SND_InitMouth( channel_t *pChannel );
extern void SND_UpdateMouth( channel_t *pChannel );
extern void SND_ClearMouth( channel_t *pChannel );
extern bool SND_IsMouth( channel_t *pChannel );
extern bool SND_ShouldPause( channel_t *pChannel );
extern bool SND_IsRecording();

void MIX_PaintChannels( int endtime, bool bIsUnderwater );
// Play a big of zeroed out sound
void MIX_PaintNullChannels( int endtime );

bool AllocDsps( bool bLoadPresetFile );
void FreeDsps( bool bReleaseTemplateMemory );
void ForceCleanDspPresets( void );
void CheckNewDspPresets( void );

void DSP_Process( int idsp, portable_samplepair_t *pbfront, portable_samplepair_t *pbrear, portable_samplepair_t *pbcenter, int sampleCount );
void DSP_ClearState();

extern int idsp_room;
extern int idsp_water;
extern int idsp_player;
extern int idsp_facingaway;
extern int idsp_speaker;
extern int idsp_spatial;

extern float g_DuckScale;

// Legacy DSP Routines

void SX_Init (void);
void SX_Free (void);
void SX_ReloadRoomFX();
void SX_RoomFX(int endtime, int fFilter, int fTimefx);

// DSP Routines

void DSP_InitAll(bool bLoadPresetFile);
void DSP_FreeAll(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif			// SOUND_PRIVATE_H
