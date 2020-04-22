//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "engine/IEngineSound.h"
#include "tier0/dbg.h"
#include "sound.h"
#include "client.h"
#include "vox.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "enginesingleuserfilter.h"
#include "snd_audio_source.h"
#if defined(_X360)
#include "xmp.h"
#endif
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// HACK:  expose in sound.h maybe?
void DSP_FastReset(int dsp);

//-----------------------------------------------------------------------------
//
// Client-side implementation of the engine sound interface
//
//-----------------------------------------------------------------------------
class CEngineSoundClient : public IEngineSound
{
public:
	// constructor, destructor
	CEngineSoundClient();
	virtual ~CEngineSoundClient();

	virtual bool PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound );
	virtual bool IsSoundPrecached( const char *pSample );
	virtual void PrefetchSound( const char *pSample );

	virtual float GetSoundDuration( const char *pSample );  

	virtual void EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSample, 
		float flVolume, float flAttenuation, int iFlags, int iPitch, int iSpecialDSP, 
		const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	virtual void EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSample, 
		float flVolume, soundlevel_t iSoundLevel, int iFlags, int iPitch, int iSpecialDSP, 
		const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	virtual void EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, int iSentenceIndex, 
		float flVolume, soundlevel_t iSoundLevel, int iFlags, int iPitch, int iSpecialDSP, 
		const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	virtual void StopSound( int iEntIndex, int iChannel, const char *pSample );

	virtual void StopAllSounds(bool bClearBuffers);

	virtual void SetRoomType( IRecipientFilter& filter, int roomType );
	virtual void SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset );

	virtual void EmitAmbientSound( const char *pSample, float flVolume, 
		int iPitch, int flags, float soundtime = 0.0f );

	virtual float GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist );

	// Client .dll only functions
	virtual int		GetGuidForLastSoundEmitted();
	virtual bool	IsSoundStillPlaying( int guid );
	virtual void	StopSoundByGuid( int guid );
	// Set's master volume (0.0->1.0)
	virtual void	SetVolumeByGuid( int guid, float fvol );

	// Retrieves list of all active sounds
	virtual void	GetActiveSounds( CUtlVector< SndInfo_t >& sndlist );

	virtual void	PrecacheSentenceGroup( const char *pGroupName );
	virtual void	NotifyBeginMoviePlayback();
	virtual void	NotifyEndMoviePlayback();

private:
	void EmitSoundInternal( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSample, 
		float flVolume, soundlevel_t iSoundLevel, int iFlags, int iPitch, int iSpecialDSP,
		const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

};


//-----------------------------------------------------------------------------
// Client-server neutral sound interface accessor
//-----------------------------------------------------------------------------
static CEngineSoundClient s_EngineSoundClient;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineSoundClient, IEngineSound, 
	IENGINESOUND_CLIENT_INTERFACE_VERSION, s_EngineSoundClient );

IEngineSound *EngineSoundClient()
{
	return &s_EngineSoundClient;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CEngineSoundClient::CEngineSoundClient()
{
}

CEngineSoundClient::~CEngineSoundClient()
{
}


//-----------------------------------------------------------------------------
// Precache a particular sample
//-----------------------------------------------------------------------------
bool CEngineSoundClient::PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound )
{
	CSfxTable *pTable = S_PrecacheSound( pSample );
	if ( pTable )
	{
		if ( bIsUISound )
		{
			S_MarkUISound( pTable );
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
//-----------------------------------------------------------------------------
void CEngineSoundClient::PrefetchSound( const char *pSample )
{
	S_PrefetchSound( pSample, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngineSoundClient::IsSoundPrecached( const char *pSample )
{
	if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
	{
		return true;
	}

	int idx = cl.LookupSoundIndex( pSample );
	if ( idx == -1 )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Actually does the work of emitting a sound
//-----------------------------------------------------------------------------
void CEngineSoundClient::EmitSoundInternal( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSample, 
	float flVolume, soundlevel_t iSoundLevel, int iFlags, int iPitch, int iSpecialDSP,  
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	if (flVolume < 0 || flVolume > 1)
	{
		Warning ("EmitSound: volume out of bounds = %f\n", flVolume);
		return;
	}

	if ( ( iSoundLevel < soundlevel_t(MIN_SNDLVL_VALUE) ) || ( iSoundLevel > soundlevel_t(MAX_SNDLVL_VALUE) ) )
	{
		Warning ("EmitSound: soundlevel out of bounds = %d\n", iSoundLevel);
		return;
	}

	if (iPitch < 0 || iPitch > 255)
	{
		Warning ("EmitSound: pitch out of bounds = %i\n", iPitch);
		return;
	}

	int iSoundSource = iEntIndex;

	// Handle client UI sounds
	if ( iSoundSource != SOUND_FROM_UI_PANEL )
	{
		if (iSoundSource < 0)
			iSoundSource = cl.m_nViewEntity;

		// See if local player is a recipient
		int i = 0;
		int c = filter.GetRecipientCount();
		for ( ; i < c ; i++ )
		{
			int index = filter.GetRecipientIndex( i );
			if ( index == cl.m_nPlayerSlot + 1 )
				break;
		}

		// Local player not receiving sound
		if ( i >= c )
			return;
	}

	CSfxTable *pSound = S_PrecacheSound(pSample);
	if (!pSound)
		return;

	Vector vecDummyOrigin;
	Vector vecDirection;
	if ( iSoundSource == SOUND_FROM_UI_PANEL )
	{
		vecDummyOrigin.Init();
		vecDirection.Init();
		pOrigin = &vecDummyOrigin;
		pDirection = &vecDirection;
	}
	else
	{
		// Point at origin if they didn't specify a sound source.
		if (!pOrigin)
		{
			// Try to use the origin of the entity
			IClientEntity *pEnt = entitylist->GetClientEntity( iEntIndex );
			// don't update position if we stop this sound
			if (pEnt && !(iFlags & SND_STOP) )
			{
				vecDummyOrigin = pEnt->GetRenderOrigin();
			}
			else
			{
				vecDummyOrigin.Init();
			}

			pOrigin = &vecDummyOrigin;
		}

		if (!pDirection)
		{
			IClientEntity *pEnt = entitylist->GetClientEntity( iEntIndex );
			if (pEnt && !(iFlags & SND_STOP))
			{
				QAngle angles;
				angles = pEnt->GetAbsAngles();
				AngleVectors( angles, &vecDirection );
			}
			else
			{
				vecDirection.Init();
			}

			pDirection = &vecDirection;
		}
	}

	if ( pUtlVecOrigins )
	{
		(*pUtlVecOrigins).AddToTail( *pOrigin );
	}

	float delay = 0.0f;
	if ( soundtime != 0.0f )
	{
		// this sound was played directly on the client, use its clock sync
		delay = S_ComputeDelayForSoundtime( soundtime, CLOCK_SYNC_CLIENT );
#if 0
		static float lastSoundTime = 0;
		Msg("[%.3f] Play %s at %.3f %.1fsms delay\n", soundtime - lastSoundTime, pSample, soundtime, delay * 1000.0f );
		lastSoundTime = soundtime;
#endif
		// anything over 250ms is assumed to be intentional skipping
		if ( delay <= 0 && delay > -0.250f )
		{
			// leave a little delay to flag the channel in the low-level sound system
			delay = 1e-6f;
		}
	}

	StartSoundParams_t params;
	params.staticsound = iChannel == CHAN_STATIC;
	params.soundsource = iSoundSource;
	params.entchannel = iChannel;
	params.pSfx = pSound;
	params.origin = *pOrigin;
	params.direction = *pDirection;
	params.bUpdatePositions = bUpdatePositions;
	params.fvol = flVolume;
	params.soundlevel = iSoundLevel;
	params.flags = iFlags;
	params.pitch = iPitch;
	params.specialdsp = iSpecialDSP;
	params.fromserver = false;
	params.delay = delay;
	params.speakerentity = speakerentity;

	S_StartSound( params );
}


//-----------------------------------------------------------------------------
// Plays a sentence
//-----------------------------------------------------------------------------
void CEngineSoundClient::EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, 
	int iSentenceIndex, float flVolume, soundlevel_t iSoundLevel, int iFlags, int iPitch, int iSpecialDSP, 
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePosition, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	if ( iSentenceIndex >= 0 )
	{
		char pName[8];
		Q_snprintf( pName, sizeof(pName), "!%d", iSentenceIndex );
		EmitSoundInternal( filter, iEntIndex, iChannel, pName, flVolume, iSoundLevel, 
			iFlags, iPitch, iSpecialDSP, pOrigin, pDirection, pUtlVecOrigins, bUpdatePosition, soundtime, speakerentity );
	}
}


//-----------------------------------------------------------------------------
// Emits a sound
//-----------------------------------------------------------------------------
void CEngineSoundClient::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSample, 
	float flVolume, float flAttenuation, int iFlags, int iPitch, int iSpecialDSP, 
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	VPROF( "CEngineSoundClient::EmitSound" );
	EmitSound( filter, iEntIndex, iChannel, pSample, flVolume, ATTN_TO_SNDLVL( flAttenuation ), iFlags, 
		iPitch, iSpecialDSP, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );

}


void CEngineSoundClient::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSample, 
	float flVolume, soundlevel_t iSoundLevel, int iFlags, int iPitch, int iSpecialDSP, 
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	VPROF( "CEngineSoundClient::EmitSound" );
	if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
	{
		int iSentenceIndex = -1;
		VOX_LookupString( PSkipSoundChars(pSample), &iSentenceIndex );
		if (iSentenceIndex >= 0)
		{
			EmitSentenceByIndex( filter, iEntIndex, iChannel, iSentenceIndex, flVolume,
				iSoundLevel, iFlags, iPitch, iSpecialDSP, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
		}
		else
		{
			DevWarning( 2, "Unable to find %s in sentences.txt\n", PSkipSoundChars(pSample));
		}
	}
	else
	{
		EmitSoundInternal( filter, iEntIndex, iChannel, pSample, flVolume, iSoundLevel,
			iFlags, iPitch, iSpecialDSP, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	}
}


//-----------------------------------------------------------------------------
// Stops a sound
//-----------------------------------------------------------------------------
void CEngineSoundClient::StopSound( int iEntIndex, int iChannel, const char *pSample )
{
	CEngineSingleUserFilter filter( cl.m_nPlayerSlot + 1 );
	EmitSound( filter, iEntIndex, iChannel, pSample, 0, SNDLVL_NONE, SND_STOP, PITCH_NORM, 0, 
		NULL, NULL, NULL, true );
}


void CEngineSoundClient::SetRoomType( IRecipientFilter& filter, int roomType )
{
	extern ConVar dsp_room;
	dsp_room.SetValue( roomType );
}

void CEngineSoundClient::SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset )
{
	extern ConVar dsp_player;
	dsp_player.SetValue( dspType );
	if ( fastReset )
	{
		DSP_FastReset( dspType );
	}
}


void CEngineSoundClient::EmitAmbientSound( const char *pSample, float flVolume, 
										  int iPitch, int flags, float soundtime /*= 0.0f*/ )
{
	float delay = 0.0f;
	if ( soundtime != 0.0f )
	{
		delay = soundtime - cl.m_flLastServerTickTime;
	}

	CSfxTable *pSound = S_PrecacheSound(pSample);

	StartSoundParams_t params;
	params.staticsound = true;
	params.soundsource = SOUND_FROM_LOCAL_PLAYER;
	params.entchannel = CHAN_STATIC;
	params.pSfx = pSound;
	params.origin = vec3_origin;
	params.fvol = flVolume;
	params.soundlevel = SNDLVL_NONE;
	params.flags = flags;
	params.pitch = iPitch;
	params.specialdsp = 0;
	params.fromserver = false;
	params.delay = delay;

	S_StartSound( params );
}

void CEngineSoundClient::StopAllSounds(bool bClearBuffers)
{
	S_StopAllSounds( bClearBuffers );
}

float CEngineSoundClient::GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist )
{
	return S_GetGainFromSoundLevel( soundlevel, dist );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
// Output : float
//-----------------------------------------------------------------------------
float CEngineSoundClient::GetSoundDuration( const char *pSample )
{
	return AudioSource_GetSoundDuration( pSample );
}

// Client .dll only functions
//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : int	
//-----------------------------------------------------------------------------
int CEngineSoundClient::GetGuidForLastSoundEmitted()
{
	return S_GetGuidForLastSoundEmitted();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngineSoundClient::IsSoundStillPlaying( int guid )
{
	return S_IsSoundStillPlaying( guid );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
//-----------------------------------------------------------------------------
void CEngineSoundClient::StopSoundByGuid( int guid )
{
	S_StopSoundByGuid( guid );
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves list of all active sounds
// Input  : sndlist - 
//-----------------------------------------------------------------------------
void CEngineSoundClient::GetActiveSounds( CUtlVector< SndInfo_t >& sndlist )
{
	S_GetActiveSounds( sndlist );
}

//-----------------------------------------------------------------------------
// Purpose: Set's master volume (0.0->1.0)
// Input  : guid - 
//			fvol - 
//-----------------------------------------------------------------------------
void CEngineSoundClient::SetVolumeByGuid( int guid, float fvol )
{
	S_SetVolumeByGuid( guid, fvol );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEngineSoundClient::PrecacheSentenceGroup( const char *pGroupName )
{
	VOX_PrecacheSentenceGroup( this, pGroupName );
}

void CEngineSoundClient::NotifyBeginMoviePlayback()
{
	StopAllSounds(true);
#if _X360
	XMPOverrideBackgroundMusic();
#endif
}

void CEngineSoundClient::NotifyEndMoviePlayback()
{
#if _X360
	XMPRestoreBackgroundMusic();
#endif
}

