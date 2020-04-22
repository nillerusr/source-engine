//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_MIX_BUF_H
#define SND_MIX_BUF_H

#if defined( _WIN32 )
#pragma once
#endif

// OPTIMIZE: note that making this larger will not increase performance (12/27/03)
#define	PAINTBUFFER_SIZE		1020	// 44k: was 512

#define PAINTBUFFER				(g_curpaintbuffer)
#define REARPAINTBUFFER			(g_currearpaintbuffer)
#define CENTERPAINTBUFFER		(g_curcenterpaintbuffer)

enum SoundBufferType_t
{
	SOUND_BUFFER_PAINT = 0,
	SOUND_BUFFER_ROOM,
	SOUND_BUFFER_FACING,
	SOUND_BUFFER_FACINGAWAY,
	SOUND_BUFFER_DRY,
	SOUND_BUFFER_SPEAKER,

	SOUND_BUFFER_BASETOTAL,
	SOUND_BUFFER_SPECIAL_START = SOUND_BUFFER_BASETOTAL
};

// !!! if this is changed, it much be changed in native assembly too !!!
struct portable_samplepair_t
{
	int left;
	int right;
};

// sound mixing buffer
#define CPAINTFILTERMEM			3
#define CPAINTFILTERS			4			// maximum number of consecutive upsample passes per paintbuffer

struct paintbuffer_t
{
	bool factive;							// if true, mix to this paintbuffer using flags
	bool fsurround;							// if true, mix to front and rear paintbuffers using flags
	bool fsurround_center;					// if true, mix to front, rear and center paintbuffers using flags

	int idsp_specialdsp;
	int nPrevSpecialDSP;
	int nSpecialDSP;

	int flags;								// SOUND_BUSS_ROOM, SOUND_BUSS_FACING, SOUND_BUSS_FACINGAWAY, SOUND_BUSS_SPEAKER, SOUND_BUSS_SPECIAL_DSP, SOUND_BUSS_DRY
	
	portable_samplepair_t *pbuf;			// front stereo mix buffer, for 2 or 4 channel mixing
	portable_samplepair_t *pbufrear;		// rear mix buffer, for 4 channel mixing
	portable_samplepair_t *pbufcenter;		// center mix buffer, for 5 channel mixing

	int ifilter;							// current filter memory buffer to use for upsampling pass

	portable_samplepair_t fltmem[CPAINTFILTERS][CPAINTFILTERMEM];		// filter memory, for upsampling with linear or cubic interpolation
	portable_samplepair_t fltmemrear[CPAINTFILTERS][CPAINTFILTERMEM];	// filter memory, for upsampling with linear or cubic interpolation
	portable_samplepair_t fltmemcenter[CPAINTFILTERS][CPAINTFILTERMEM];	// filter memory, for upsampling with linear or cubic interpolation
};

extern "C"
{

extern portable_samplepair_t *g_paintbuffer;

// temp paintbuffer - not included in main list of paintbuffers
extern portable_samplepair_t *g_temppaintbuffer;
	
extern CUtlVector< paintbuffer_t > g_paintBuffers;

extern void MIX_SetCurrentPaintbuffer( int ipaintbuffer );
extern int MIX_GetCurrentPaintbufferIndex( void );
extern paintbuffer_t *MIX_GetCurrentPaintbufferPtr( void );
extern paintbuffer_t *MIX_GetPPaintFromIPaint( int ipaintbuffer );
extern void MIX_ClearAllPaintBuffers( int SampleCount, bool clearFilters );
extern bool MIX_InitAllPaintbuffers(void);
extern void MIX_FreeAllPaintbuffers(void);
	
extern portable_samplepair_t *g_curpaintbuffer;
extern portable_samplepair_t *g_currearpaintbuffer;
extern portable_samplepair_t *g_curcenterpaintbuffer;

};

// must be at least PAINTBUFFER_SIZE+1 for upsampling
#define	PAINTBUFFER_MEM_SIZE		(PAINTBUFFER_SIZE+4)

// size in samples of copy buffer used by pitch shifters in mixing
#if defined(_GAMECONSOLE)
#define TEMP_COPY_BUFFER_SIZE	(PAINTBUFFER_MEM_SIZE * 2)
#else
// allow more memory for this on PC for developers to pitch-shift their way through dialog
#define TEMP_COPY_BUFFER_SIZE	(PAINTBUFFER_MEM_SIZE * 4)
#endif

// hard clip input value to -32767 <= y <= 32767
#define CLIP(x) ((x) > 32767 ? 32767 : ((x) < -32767 ? -32767 : (x)))

#endif // SND_MIX_BUF_H
