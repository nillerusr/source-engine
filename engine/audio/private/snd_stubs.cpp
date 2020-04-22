//========= Copyright Valve Corporation, All rights reserved. ============//

#include "audio_pch.h"

//#include "matchmaking/IMatchFramework.h"
#include "tier2/tier2.h"

#include "audio/public/voice.h"

#if !defined( DEDICATED ) && ( defined( OSX ) || defined( _WIN32 ) ) && !defined( NO_STEAM )
#include "cl_steamauth.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CEngineVoiceStub *Audio_GetEngineVoiceStub()
{
	static CEngineVoiceStub s_EngineVoiceStub;
	return &s_EngineVoiceStub;
}


#if !defined( DEDICATED ) && ( defined( OSX ) || defined( _WIN32 ) ) && !defined( NO_STEAM )

class CEngineVoiceSteam : public IEngineVoice
{
public:
	CEngineVoiceSteam();

public:
	virtual bool IsHeadsetPresent( int iController );
	virtual bool IsLocalPlayerTalking( int iController );

	virtual void AddPlayerToVoiceList( XUID xPlayer, int iController );
	virtual void RemovePlayerFromVoiceList( XUID xPlayer, int iController );

	virtual void GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers );

	virtual bool VoiceUpdateData( int iController );
	virtual void GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes );
	virtual void VoiceResetLocalData( int iController );

	virtual void SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback );
	virtual void PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers = NULL );

	virtual void RemoveAllTalkers();

protected:
	void AudioInitializationUpdate();

public:
	bool m_bLocalVoice[ XUSER_MAX_COUNT ];
	CUtlVector< XUID > m_arrRemoteVoice;
	bool m_bInitializedAudio;
	byte m_pbVoiceData[ 1024 * XUSER_MAX_COUNT ];
};

CEngineVoiceSteam::CEngineVoiceSteam()
{
	memset( m_bLocalVoice, 0, sizeof( m_bLocalVoice ) );
	memset( m_pbVoiceData, 0, sizeof( m_pbVoiceData ) );
	m_bInitializedAudio = false;
}

bool CEngineVoiceSteam::IsHeadsetPresent( int iController )
{
	return false;
}

bool CEngineVoiceSteam::IsLocalPlayerTalking( int iController )
{
	uint32 nBytes = 0;
	EVoiceResult res = Steam3Client().SteamUser()->GetAvailableVoice( &nBytes, NULL, 0 );
	switch ( res )
	{
	case k_EVoiceResultOK:
	case k_EVoiceResultNoData:
	case k_EVoiceResultBufferTooSmall:
		return true;
	default:
		return false;
	}
}

void CEngineVoiceSteam::AddPlayerToVoiceList( XUID xPlayer, int iController )
{
	if ( !xPlayer && iController >= 0 && iController < XUSER_MAX_COUNT )
	{
		// Add local player
		m_bLocalVoice[ iController ] = true;
		AudioInitializationUpdate();
	}

	if ( xPlayer )
	{
		if ( m_arrRemoteVoice.Find( xPlayer ) == m_arrRemoteVoice.InvalidIndex() )
		{
			m_arrRemoteVoice.AddToTail( xPlayer );
			AudioInitializationUpdate();
		}
	}
}

void CEngineVoiceSteam::RemovePlayerFromVoiceList( XUID xPlayer, int iController )
{
	if ( !xPlayer && iController >= 0 && iController < XUSER_MAX_COUNT )
	{
		// Remove local player
		m_bLocalVoice[ iController ] = false;
		AudioInitializationUpdate();
	}

	if ( xPlayer )
	{
		int idx = m_arrRemoteVoice.Find( xPlayer );
		if ( idx != m_arrRemoteVoice.InvalidIndex() )
		{
			m_arrRemoteVoice.FastRemove( idx );
			AudioInitializationUpdate();
		}
	}
}

void CEngineVoiceSteam::GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers )
{
	if ( pNumTalkers )
		*pNumTalkers = m_arrRemoteVoice.Count();

	if ( pRemoteTalkers )
	{
		for ( int k = 0; k < m_arrRemoteVoice.Count(); ++ k )
			pRemoteTalkers[k] = m_arrRemoteVoice[k];
	}
}

bool CEngineVoiceSteam::VoiceUpdateData( int iController )
{
	uint32 nBytes = 0;
	EVoiceResult res = Steam3Client().SteamUser()->GetAvailableVoice( &nBytes, NULL, 0 );
	switch ( res )
	{
	case k_EVoiceResultOK:
	// case k_EVoiceResultNoData: - no data means false
	case k_EVoiceResultBufferTooSmall:
		return true;
	default:
		return false;
	}
}

void CEngineVoiceSteam::GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes )
{
	const int size = ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	byte *pbVoiceData = m_pbVoiceData + iController * ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	*ppvVoiceDataBuffer = pbVoiceData;
	
	EVoiceResult res = Steam3Client().SteamUser()->GetVoice( true, pbVoiceData, size, pnumVoiceDataBytes, false, NULL, 0, NULL, 0 );
	switch ( res )
	{
	case k_EVoiceResultNoData:
	case k_EVoiceResultOK:
		return;
	default:
		*pnumVoiceDataBytes = 0;
		*ppvVoiceDataBuffer = NULL;
		return;
	}
}

void CEngineVoiceSteam::VoiceResetLocalData( int iController )
{
	const int size = ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	byte *pbVoiceData = m_pbVoiceData + iController * ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	memset( pbVoiceData, 0, size );
}

void CEngineVoiceSteam::SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback )
{
	;
}

void CEngineVoiceSteam::PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers /* = NULL */ )
{
	for ( DWORD dwSlot = 0; dwSlot < XBX_GetNumGameUsers(); ++ dwSlot )
	{
		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( dwSlot );
		if ( pPlayer && pPlayer->GetXUID() == xuid )
			//Hack: Don't play stuff that comes from ourselves.
			return;
	}

	// Make sure voice playback is allowed for the specified user
	if ( !g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->CanPlaybackTalker( xuid ) )
		return;

	// Uncompress the voice data
	char pbUncompressedVoice[ 11025 * 2 ];
	uint32 numUncompressedBytes;
	EVoiceResult res = Steam3Client().SteamUser()->DecompressVoice( const_cast< byte * >( pbData ), dwDataSize,
		pbUncompressedVoice, sizeof( pbUncompressedVoice ), &numUncompressedBytes, 0 );

	if ( res != k_EVoiceResultOK )
		return;

	// Voice channel index
	int idxVoiceChan = 0;
	int idxRemoteTalker = m_arrRemoteVoice.Find( xuid );
	if ( idxRemoteTalker != m_arrRemoteVoice.InvalidIndex() )
		idxVoiceChan = idxRemoteTalker;

	int nChannel = Voice_GetChannel( idxVoiceChan );
	if ( nChannel == VOICE_CHANNEL_ERROR )
	{
		// Create a channel in the voice engine and a channel in the sound engine for this guy.
		nChannel = Voice_AssignChannel( idxVoiceChan, false, 0.0f );
	}

	// Give the voice engine the data (it in turn gives it to the mixer for the sound engine).
	if ( nChannel != VOICE_CHANNEL_ERROR )
	{
		Voice_AddIncomingData( nChannel, pbUncompressedVoice, numUncompressedBytes, 0, false );
	}
}

void CEngineVoiceSteam::RemoveAllTalkers()
{
	memset( m_bLocalVoice, 0, sizeof( m_bLocalVoice ) );
	m_arrRemoteVoice.RemoveAll();
	AudioInitializationUpdate();
}

void CEngineVoiceSteam::AudioInitializationUpdate()
{
	bool bHasTalkers = ( m_arrRemoteVoice.Count() > 0 );
	for ( int k = 0; k < ARRAYSIZE( m_bLocalVoice ); ++ k )
	{
		if ( m_bLocalVoice[k] )
		{
			bHasTalkers = true;
			break;
		}
	}

	// Initialized already
	if ( bHasTalkers == m_bInitializedAudio )
		return;

	// Clear out voice buffers
	memset( m_pbVoiceData, 0, sizeof( m_pbVoiceData ) );

	// Init or deinit voice system
	if ( bHasTalkers )
	{
		Voice_ForceInit();
	}
	else
	{
		Voice_Deinit();
	}

	m_bInitializedAudio = bHasTalkers;
}


IEngineVoice *Audio_GetEngineVoiceSteam()
{
	static CEngineVoiceSteam s_EngineVoiceSteam;
	return &s_EngineVoiceSteam;
}

#else

IEngineVoice *Audio_GetEngineVoiceSteam()
{
	return Audio_GetEngineVoiceStub();
}

#endif