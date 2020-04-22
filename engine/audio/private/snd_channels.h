//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef SND_CHANNELS_H
#define SND_CHANNELS_H

#include "mathlib/vector.h"

#if defined( _WIN32 )
#pragma once
#endif

class CSfxTable;
class CAudioMixer;
typedef int SoundSource;

// DO NOT REORDER: indices to fvolume arrays in channel_t 

#define IFRONT_LEFT		0			// NOTE: must correspond to order of fvolume array below!
#define IFRONT_RIGHT	1
#define	IREAR_LEFT		2
#define IREAR_RIGHT		3
#define IFRONT_CENTER	4
#define IFRONT_CENTER0	5			// dummy slot - center channel is mono, but mixers reference volume[1] slot

#define IFRONT_LEFTD	6			// start of doppler right array			
#define IFRONT_RIGHTD	7
#define	IREAR_LEFTD		8
#define IREAR_RIGHTD	9
#define IFRONT_CENTERD	10
#define IFRONT_CENTERD0 11			// dummy slot - center channel is mono, but mixers reference volume[1] slot

#define CCHANVOLUMES	12

//-----------------------------------------------------------------------------
// Purpose: Each currently playing wave is stored in a channel
//-----------------------------------------------------------------------------
// NOTE: 128bytes.  These are memset to zero at some points.  Do not add virtuals without changing that pattern.
// UNDONE: now 300 bytes...
struct channel_t
{
	int			guid;			// incremented each time a channel is allocated (to match with channel free in tools, etc.)
	int			userdata;		// user specified data for syncing to tools

	CSfxTable	*sfx;			// the actual sound
	CAudioMixer	*pMixer;		// The sound's instance data for this channel
	
	// speaker channel volumes, indexed using IFRONT_LEFT to IFRONT_CENTER.
	// NOTE: never access these fvolume[] elements directly! Use channel helpers in snd_dma.cpp.

	float		fvolume[CCHANVOLUMES];			// 0.0-255.0 current output volumes
	float		fvolume_target[CCHANVOLUMES];	// 0.0-255.0 target output volumes
	float		fvolume_inc[CCHANVOLUMES];		// volume increment, per frame, moves volume[i] to vol_target[i] (per spatialization)		
	uint		nFreeChannelAtSampleTime;

	SoundSource	soundsource;	// see iclientsound.h for description.
	int			entchannel;		// sound channel (CHAN_STREAM, CHAN_VOICE, etc.)
	int			speakerentity;  // if a sound is being played through a speaker entity (e.g., on a monitor,), this is the
								//  entity upon which to show the lips moving, if the sound has sentence data
	short		master_vol;		// 0-255 master volume
	short		basePitch;		// base pitch percent (100% is normal pitch playback)
	float		pitch;			// real-time pitch after any modulation or shift by dynamic data
	int			mixgroups[8];	// sound belongs to these mixgroups: world, actor, player weapon, explosion etc.
	int			last_mixgroupid;// last mixgroupid selected
	float		last_vol;		// last volume after spatialization

	Vector		origin;			// origin of sound effect
	Vector		direction;		// direction of the sound
	float		dist_mult;		// distance multiplier (attenuation/clipK)


	float		dspmix;			// 0 - 1.0 proportion of dsp to mix with original sound, based on distance
	float		dspface;		// -1.0 - 1.0 (1.0 = facing listener)
	float		distmix;		// 0 - 1.0 proportion based on distance from listner (1.0 - 100% wav right - far)
	float		dsp_mix_min;	// for dspmix calculation - set by current preset in SND_GetDspMix
	float		dsp_mix_max;	// for dspmix calculation - set by current preset in SND_GetDspMix

	float		radius;			// Radius of this sound effect (spatialization is different within the radius)

	float		ob_gain;		// gain drop if sound source obscured from listener
	float		ob_gain_target;	// target gain while crossfading between ob_gain & ob_gain_target
	float		ob_gain_inc;	// crossfade increment

	short		activeIndex;
	char		wavtype;		// 0 default, CHAR_DOPPLER, CHAR_DIRECTIONAL, CHAR_DISTVARIANT
	char		pad;

	char		sample_prev[8];	// last sample(s) in previous input data buffer - space for 2, 16 bit, stereo samples

	int			initialStreamPosition;

	int			special_dsp;

	union
	{
		unsigned int flagsword;
		struct
		{
			bool		bUpdatePositions : 1; // if true, assume sound source can move and update according to entity
			bool		isSentence : 1;		// true if playing linked sentence
			bool		bdry : 1;			// if true, bypass all dsp processing for this sound (ie: music)	
			bool		bSpeaker : 1;		// true if sound is playing through in-game speaker entity.
			bool		bstereowav : 1;		// if true, a stereo .wav file is the sample data source

			bool		delayed_start : 1;  // If true, sound had a delay and so same sound on same channel won't channel steal from it
			bool		fromserver : 1;		// for snd_show, networked sounds get colored differently than local sounds

			bool		bfirstpass : 1;		// true if this is first time sound is spatialized
			bool		bTraced : 1;		// true if channel was already checked this frame for obscuring
			bool		bfast_pitch : 1;	// true if using low quality pitch (fast, but no interpolation)
			
			bool		m_bIsFreeingChannel : 1;	// true when inside S_FreeChannel - prevents reentrance
			bool		m_bCompatibilityAttenuation : 1;	// True when we want to use goldsrc compatibility mode for the attenuation
															// In that case, dist_mul is set to a relatively meaningful value in StartDynamic/StartStaticSound,
															// but we interpret it totally differently in SND_GetGain.
			bool		m_bShouldPause : 1;	// if true, sound should pause when the game is paused
			bool		m_bIgnorePhonemes : 1;	// if true, we don't want to drive animation w/ phoneme data
		} flags;
	};
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#define	MAX_CHANNELS			128
#define	MAX_DYNAMIC_CHANNELS	64

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

extern	channel_t   channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to total_channels = static sounds

extern	int			total_channels;

class CChannelList
{
public:
	int		Count();
	int		GetChannelIndex( int listIndex );
	channel_t *GetChannel( int listIndex );
	void	RemoveChannelFromList( int listIndex );
	bool	IsQuashed( int listIndex );

	int		m_count;
	short	m_list[MAX_CHANNELS];
	bool	m_quashed[MAX_CHANNELS]; // if true, the channel should be advanced, but not mixed, because it's been heuristically suppressed

	CUtlVector< int >	m_nSpecialDSPs;

	bool	m_hasSpeakerChannels : 1;
	bool	m_hasDryChannels : 1;
	bool	m_has11kChannels : 1;
	bool	m_has22kChannels : 1;
	bool	m_has44kChannels : 1;
};

inline int CChannelList::Count()
{
	return m_count;
}

inline int CChannelList::GetChannelIndex( int listIndex )
{
	return m_list[listIndex];
}
inline channel_t *CChannelList::GetChannel( int listIndex )
{
	return &channels[GetChannelIndex(listIndex)];
}

inline bool CChannelList::IsQuashed( int listIndex )
{
	return m_quashed[listIndex];
}

inline void CChannelList::RemoveChannelFromList( int listIndex )
{
	// decrease the count by one, and swap the deleted channel with
	// the last one.
	m_count--;
	if ( m_count > 0 && listIndex != m_count )
	{
		m_list[listIndex] = m_list[m_count];
		m_quashed[listIndex] = m_quashed[m_count];
	}
}

class CActiveChannels
{
public:
	void Add( channel_t *pChannel );
	void Remove( channel_t *pChannel );

	void GetActiveChannels( CChannelList &list );

	void Init();
	int	 GetActiveCount() { return m_count; }
private:
	int		m_count;
	short	m_list[MAX_CHANNELS];
};

extern CActiveChannels	g_ActiveChannels;

//=============================================================================

#endif // SND_CHANNELS_H
