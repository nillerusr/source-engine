//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: X360 XAudio Version
//
//=====================================================================================//


#include "audio_pch.h"
#include "snd_dev_xaudio.h"
#include "UtlLinkedList.h"
#include "session.h"
#include "server.h"
#include "client.h"
#include "matchmaking.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The outer code mixes in PAINTBUFFER_SIZE (# of samples) chunks (see MIX_PaintChannels), we will never need more than
// that many samples in a buffer.  This ends up being about 20ms per buffer
#define XAUDIO2_BUFFER_SAMPLES	8192
// buffer return has a latency, so need a decent pool
#define MAX_XAUDIO2_BUFFERS		32


#define SURROUND_HEADPHONES		0
#define SURROUND_STEREO			2
#define SURROUND_DIGITAL5DOT1	5

// 5.1 means there are a max of 6 channels
#define MAX_DEVICE_CHANNELS		6

ConVar snd_xaudio_spew_packets( "snd_xaudio_spew_packets", "0", 0, "Spew XAudio packet delivery" );



//-----------------------------------------------------------------------------
// Implementation of XAudio
//-----------------------------------------------------------------------------
class CAudioXAudio : public CAudioDeviceBase
{
public:
	~CAudioXAudio( void );

	bool		IsActive( void ) { return true; }
	bool		Init( void );
	void		Shutdown( void );

	void		Pause( void );
	void		UnPause( void );
	int			PaintBegin( float mixAheadTime, int soundtime, int paintedtime );
	int			GetOutputPosition( void );
	void		ClearBuffer( void );
	void		TransferSamples( int end );

	const char *DeviceName( void );
	int			DeviceChannels( void )		{ return m_deviceChannels; }
	int			DeviceSampleBits( void )	{ return m_deviceSampleBits; }
	int			DeviceSampleBytes( void )	{ return m_deviceSampleBits/8; }
	int			DeviceDmaSpeed( void )		{ return m_deviceDmaSpeed; }	
	int			DeviceSampleCount( void )	{ return m_deviceSampleCount; }



	void		XAudioPacketCallback( int hCompletedBuffer );

	static		CAudioXAudio *m_pSingleton;

	CXboxVoice *GetVoiceData( void ) { return &m_VoiceData; }
	IXAudio2	*GetXAudio2( void ) { return m_pXAudio2; }

private:
	int			TransferStereo( const portable_samplepair_t *pFront, int paintedTime, int endTime, char *pOutptuBuffer );
	int			TransferSurroundInterleaved( const portable_samplepair_t *pFront, const portable_samplepair_t *pRear, const portable_samplepair_t *pCenter, int paintedTime, int endTime, char *pOutputBuffer );

	int			m_deviceChannels;					// channels per hardware output buffer (1 for quad/5.1, 2 for stereo)
	int			m_deviceSampleBits;					// bits per sample (16)
	int			m_deviceSampleCount;				// count of mono samples in output buffer
	int			m_deviceDmaSpeed;					// samples per second per output buffer
	int			m_clockDivider;

	IXAudio2				*m_pXAudio2;
	IXAudio2MasteringVoice	*m_pMasteringVoice;
	IXAudio2SourceVoice		*m_pSourceVoice;

	XAUDIO2_BUFFER		m_Buffers[MAX_XAUDIO2_BUFFERS];
	BYTE				*m_pOutputBuffer;
	int					m_bufferSizeBytes;			// size of a single hardware output buffer, in bytes
	CInterlockedUInt	m_BufferTail;
	CInterlockedUInt	m_BufferHead;
	
	CXboxVoice			m_VoiceData;
};

CAudioXAudio *CAudioXAudio::m_pSingleton = NULL;

//-----------------------------------------------------------------------------
// XAudio packet completion callback
//-----------------------------------------------------------------------------
class XAudio2VoiceCallback : public IXAudio2VoiceCallback
{
public:
    XAudio2VoiceCallback() {}
    ~XAudio2VoiceCallback() {}

    void OnStreamEnd() {}

    void OnVoiceProcessingPassEnd() {}

    void OnVoiceProcessingPassStart( UINT32 SamplesRequired ) {}

    void OnBufferEnd( void *pBufferContext ) 
	{
		CAudioXAudio::m_pSingleton->XAudioPacketCallback( (int)pBufferContext );
	}

    void OnBufferStart( void *pBufferContext ) {}

    void OnLoopEnd( void *pBufferContext ) {}

    void OnVoiceError( void *pBufferContext, HRESULT Error ) {}
};
XAudio2VoiceCallback s_XAudio2VoiceCallback;



//-----------------------------------------------------------------------------
// Create XAudio Device
//-----------------------------------------------------------------------------
IAudioDevice *Audio_CreateXAudioDevice( void )
{
	MEM_ALLOC_CREDIT();

	if ( !CAudioXAudio::m_pSingleton )
	{
		CAudioXAudio::m_pSingleton = new CAudioXAudio;
	}

	if ( !CAudioXAudio::m_pSingleton->Init() )
	{
		delete CAudioXAudio::m_pSingleton;
		CAudioXAudio::m_pSingleton = NULL;
	}

	return CAudioXAudio::m_pSingleton;
}

CXboxVoice *Audio_GetXVoice( void )
{
	if ( CAudioXAudio::m_pSingleton )
	{
		return CAudioXAudio::m_pSingleton->GetVoiceData();
	}

	return NULL;
}

IXAudio2 *Audio_GetXAudio2( void )
{
	if ( CAudioXAudio::m_pSingleton )
	{
		return CAudioXAudio::m_pSingleton->GetXAudio2();
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CAudioXAudio::~CAudioXAudio( void )
{
	m_pSingleton = NULL;
}

//-----------------------------------------------------------------------------
// Initialize XAudio
//-----------------------------------------------------------------------------
bool CAudioXAudio::Init( void )
{	
	XAUDIOSPEAKERCONFIG xAudioConfig = 0;
	XAudioGetSpeakerConfig( &xAudioConfig );
	snd_surround.SetValue( ( xAudioConfig & XAUDIOSPEAKERCONFIG_DIGITAL_DOLBYDIGITAL ) ? SURROUND_DIGITAL5DOT1 : SURROUND_STEREO );

	m_bHeadphone = false;
	m_bSurround = false;
	m_bSurroundCenter = false;

	switch ( snd_surround.GetInt() )
	{
	case SURROUND_HEADPHONES:
		m_bHeadphone = true;
		m_deviceChannels = 2;
		break;

	default:
	case SURROUND_STEREO:
		m_deviceChannels = 2;
		break;

	case SURROUND_DIGITAL5DOT1:
		m_bSurround = true;	
		m_bSurroundCenter = true;
		m_deviceChannels = 6;
		break;
	}

	m_deviceSampleBits = 16;
	m_deviceDmaSpeed = SOUND_DMA_SPEED;

    // initialize the XAudio Engine
	// Both threads on core 2
	m_pXAudio2 = NULL;
	HRESULT hr = XAudio2Create( &m_pXAudio2, 0, XboxThread5 );
	if ( FAILED( hr ) )
		return false;

	// create the mastering voice, this will upsample to the devices target hw output rate
    m_pMasteringVoice = NULL;
	hr = m_pXAudio2->CreateMasteringVoice( &m_pMasteringVoice );
	if ( FAILED( hr ) )
        return false;
	
	// 16 bit PCM
	WAVEFORMATEX waveFormatEx = { 0 };
	waveFormatEx.wFormatTag = WAVE_FORMAT_PCM;
	waveFormatEx.nChannels = m_deviceChannels;
	waveFormatEx.nSamplesPerSec = m_deviceDmaSpeed;
	waveFormatEx.wBitsPerSample = 16;
	waveFormatEx.nBlockAlign = ( waveFormatEx.nChannels * waveFormatEx.wBitsPerSample ) / 8;
	waveFormatEx.nAvgBytesPerSec = waveFormatEx.nSamplesPerSec * waveFormatEx.nBlockAlign;
	waveFormatEx.cbSize = 0;

	m_pSourceVoice = NULL;
	hr = m_pXAudio2->CreateSourceVoice( 
			&m_pSourceVoice, 
			&waveFormatEx, 
			0,
			XAUDIO2_DEFAULT_FREQ_RATIO,
			&s_XAudio2VoiceCallback,
			NULL,
			NULL );
	if ( FAILED( hr ) )
        return false;

	float volumes[MAX_DEVICE_CHANNELS];
	for ( int i = 0; i < MAX_DEVICE_CHANNELS; i++ )
	{
		if ( !m_bSurround && i >= 2 )
		{
			volumes[i] = 0;
		}
		else
		{
			volumes[i] = 1.0;
		}
	}
	m_pSourceVoice->SetChannelVolumes( m_deviceChannels, volumes );

	m_bufferSizeBytes = XAUDIO2_BUFFER_SAMPLES * (m_deviceSampleBits/8) * m_deviceChannels;
	m_pOutputBuffer = new BYTE[MAX_XAUDIO2_BUFFERS * m_bufferSizeBytes];
	ClearBuffer();

	V_memset( m_Buffers, 0, MAX_XAUDIO2_BUFFERS * sizeof( XAUDIO2_BUFFER ) );
	for ( int i = 0; i < MAX_XAUDIO2_BUFFERS; i++ )
	{
		m_Buffers[i].pAudioData = m_pOutputBuffer + i*m_bufferSizeBytes;
		m_Buffers[i].pContext = (LPVOID)i;
	}
	m_BufferHead = 0;
	m_BufferTail = 0;

	// number of mono samples output buffer may hold
	m_deviceSampleCount = MAX_XAUDIO2_BUFFERS * (m_bufferSizeBytes/(DeviceSampleBytes()));
	
	// NOTE: This really shouldn't be tied to the # of bufferable samples.
	// This just needs to be large enough so that it doesn't fake out the sampling in 
	// GetSoundTime().  Basically GetSoundTime() assumes a cyclical time stamp and finds wraparound cases
	// but that means it needs to get called much more often than once per cycle.  So this number should be
	// much larger than the framerate in terms of output time
	m_clockDivider = m_deviceSampleCount / DeviceChannels();

	// not really part of XAudio2, but mixer xma lacks one-time init, so doing it here
	XMAPlaybackInitialize();

	hr = m_pSourceVoice->Start( 0 );
    if ( FAILED( hr ) )
		return false;

	DevMsg( "XAudio Device Initialized:\n" );
	DevMsg( "   %s\n"
			"   %d channel(s)\n"
			"   %d bits/sample\n"
			"   %d samples/sec\n", DeviceName(), DeviceChannels(), DeviceSampleBits(), DeviceDmaSpeed() );

	m_VoiceData.VoiceInit();

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Shutdown XAudio
//-----------------------------------------------------------------------------
void CAudioXAudio::Shutdown( void )
{
	if ( m_pSourceVoice )
	{
		m_pSourceVoice->Stop( 0 );
		m_pSourceVoice->DestroyVoice();
		m_pSourceVoice = NULL;
		delete[] m_pOutputBuffer;
	}

	if ( m_pMasteringVoice )
	{
		m_pMasteringVoice->DestroyVoice();
		m_pMasteringVoice = NULL;
	}

	// need to release ref to XAudio2
	m_VoiceData.VoiceShutdown();

	if ( m_pXAudio2 )
	{
		m_pXAudio2->Release();
		m_pXAudio2 = NULL;
	}

	if ( this == CAudioXAudio::m_pSingleton )
	{
		CAudioXAudio::m_pSingleton = NULL;
	}
}

//-----------------------------------------------------------------------------
// XAudio has completed a packet. Assuming these are sequential
//-----------------------------------------------------------------------------
void CAudioXAudio::XAudioPacketCallback( int hCompletedBuffer )
{
	// packet completion expected to be sequential
	Assert( hCompletedBuffer == (int)( m_PacketTail % MAX_XAUDIO2_BUFFERS ) );

	m_BufferTail++;

	if ( snd_xaudio_spew_packets.GetBool() )
	{
		if ( m_BufferTail == m_BufferHead )
		{
			Warning( "XAudio: Starved\n" );
		}
		else
		{
			Msg( "XAudio: Packet Callback, Submit: %2d, Free: %2d\n", m_BufferHead - m_BufferTail, MAX_XAUDIO2_BUFFERS - ( m_BufferHead - m_BufferTail ) );
		}
	}

	if ( m_BufferTail == m_BufferHead )
	{
		// very bad, out of packets, xaudio is starving
		// mix thread didn't keep up with audio clock and submit packets
		// submit a silent buffer to keep stream playing and audio clock running
		int head = m_BufferHead++;
		XAUDIO2_BUFFER *pBuffer = &m_Buffers[head % MAX_XAUDIO2_BUFFERS];
		V_memset( pBuffer->pAudioData, 0, m_bufferSizeBytes );
		pBuffer->AudioBytes = m_bufferSizeBytes;
		m_pSourceVoice->SubmitSourceBuffer( pBuffer );
	}
}

//-----------------------------------------------------------------------------
// Return the "write" cursor.  Used to clock the audio mixing.
// The actual hw write cursor and the number of samples it fetches is unknown.
//-----------------------------------------------------------------------------
int	CAudioXAudio::GetOutputPosition( void )
{
	XAUDIO2_VOICE_STATE state;

	state.SamplesPlayed = 0;
	m_pSourceVoice->GetState( &state );

	return ( state.SamplesPlayed % m_clockDivider );
}

//-----------------------------------------------------------------------------
// Pause playback
//-----------------------------------------------------------------------------
void CAudioXAudio::Pause( void )
{
	if ( m_pSourceVoice )
	{
		m_pSourceVoice->Stop( 0 );
	}
}

//-----------------------------------------------------------------------------
// Resume playback
//-----------------------------------------------------------------------------
void CAudioXAudio::UnPause( void )
{
	if ( m_pSourceVoice )
	{
		m_pSourceVoice->Start( 0 );
	}
}

//-----------------------------------------------------------------------------
// Calc the paint position
//-----------------------------------------------------------------------------
int CAudioXAudio::PaintBegin( float mixAheadTime, int soundtime, int paintedtime )
{
	//  soundtime = total full samples that have been played out to hardware at dmaspeed
	//  paintedtime = total full samples that have been mixed at speed

	//  endtime = target for full samples in mixahead buffer at speed	
	int mixaheadtime = mixAheadTime * DeviceDmaSpeed();
	int endtime = soundtime + mixaheadtime;
	if ( endtime <= paintedtime )
	{
		return endtime;
	}

	int fullsamps = DeviceSampleCount() / DeviceChannels();

	if ( ( endtime - soundtime ) > fullsamps )
	{
		endtime = soundtime + fullsamps;
	}
	if ( ( endtime - paintedtime ) & 0x03 )
	{
		// The difference between endtime and painted time should align on 
		// boundaries of 4 samples.  This is important when upsampling from 11khz -> 44khz.
		endtime -= ( endtime - paintedtime ) & 0x03;
	}

	return endtime;
}

//-----------------------------------------------------------------------------
// Fill the output buffers with silence
//-----------------------------------------------------------------------------
void CAudioXAudio::ClearBuffer( void )
{
	V_memset( m_pOutputBuffer, 0, MAX_XAUDIO2_BUFFERS * m_bufferSizeBytes );
}

//-----------------------------------------------------------------------------
// Fill the output buffer with L/R samples
//-----------------------------------------------------------------------------
int CAudioXAudio::TransferStereo( const portable_samplepair_t *pFrontBuffer, int paintedTime, int endTime, char *pOutputBuffer )
{
	int linearCount;
	int i;
	int	val;

	int volumeFactor = S_GetMasterVolume() * 256;

	int *pFront = (int *)pFrontBuffer;
	short *pOutput = (short *)pOutputBuffer;
	
	// get size of output buffer in full samples (LR pairs)
	// number of sequential sample pairs that can be wrriten
	linearCount = g_AudioDevice->DeviceSampleCount() >> 1;

	// clamp output count to requested number of samples
	if ( linearCount > endTime - paintedTime )
	{
		linearCount = endTime - paintedTime;
	}

	// linearCount is now number of mono 16 bit samples (L and R) to xfer.
	linearCount <<= 1;

	// transfer mono 16bit samples multiplying each sample by volume.
	for ( i=0; i<linearCount; i+=2 )
	{
		// L Channel
		val = ( pFront[i] * volumeFactor ) >> 8;
		*pOutput++ = CLIP( val );

		// R Channel
		val = ( pFront[i+1] * volumeFactor ) >> 8;
		*pOutput++ = CLIP( val );
	}

	return linearCount * DeviceSampleBytes();
}

//-----------------------------------------------------------------------------
// Fill the output buffer with interleaved surround samples
//-----------------------------------------------------------------------------
int CAudioXAudio::TransferSurroundInterleaved( const portable_samplepair_t *pFrontBuffer, const portable_samplepair_t *pRearBuffer, const portable_samplepair_t *pCenterBuffer, int paintedTime, int endTime, char *pOutputBuffer )
{
	int linearCount;
	int i, j;
	int	val;

	int volumeFactor = S_GetMasterVolume() * 256;

	int *pFront = (int *)pFrontBuffer;
	int *pRear = (int *)pRearBuffer;
	int *pCenter = (int *)pCenterBuffer;
	short *pOutput = (short *)pOutputBuffer;
	
	// number of mono samples per channel
	// number of sequential samples that can be wrriten
	linearCount = m_bufferSizeBytes/( DeviceSampleBytes() * DeviceChannels() );		

	// clamp output count to requested number of samples
	if ( linearCount > endTime - paintedTime )	
	{	
		linearCount = endTime - paintedTime;
	}

	for ( i = 0, j = 0; i < linearCount; i++, j += 2 )
	{
		// FL
		val = ( pFront[j] * volumeFactor ) >> 8;
		*pOutput++ = CLIP( val );

		// FR
		val = ( pFront[j+1] * volumeFactor ) >> 8;
		*pOutput++ = CLIP( val );

		// Center
		val = ( pCenter[j] * volumeFactor) >> 8;
		*pOutput++ = CLIP( val );

		// Let the hardware mix the sub from the main channels since
		// we don't have any sub-specific sounds, or direct sub-addressing
		*pOutput++ = 0;

		// RL
		val = ( pRear[j] * volumeFactor ) >> 8;
		*pOutput++ = CLIP( val );

		// RR
		val = ( pRear[j+1] * volumeFactor ) >> 8;
		*pOutput++ = CLIP( val );
	}

	return linearCount * DeviceSampleBytes() * DeviceChannels();
}

//-----------------------------------------------------------------------------
// Transfer up to a full paintbuffer (PAINTBUFFER_SIZE) of samples out to the xaudio buffer(s).
//-----------------------------------------------------------------------------
void CAudioXAudio::TransferSamples( int endTime )
{
	XAUDIO2_BUFFER *pBuffer;

	if ( m_BufferHead - m_BufferTail >= MAX_XAUDIO2_BUFFERS )
	{
		DevWarning( "XAudio: No Free Buffers!\n" );
		return;
	}

	int sampleCount = endTime - g_paintedtime;
	if ( sampleCount > XAUDIO2_BUFFER_SAMPLES )
	{
		DevWarning( "XAudio: Overflowed mix buffer!\n" );
		endTime = g_paintedtime + XAUDIO2_BUFFER_SAMPLES;
	}

	unsigned int nBuffer = m_BufferHead++;
	pBuffer = &m_Buffers[nBuffer % MAX_XAUDIO2_BUFFERS];

	if ( !m_bSurround )
	{
		pBuffer->AudioBytes = TransferStereo( PAINTBUFFER, g_paintedtime, endTime, (char *)pBuffer->pAudioData );
	}
	else
	{
		pBuffer->AudioBytes = TransferSurroundInterleaved( PAINTBUFFER, REARPAINTBUFFER, CENTERPAINTBUFFER, g_paintedtime, endTime, (char *)pBuffer->pAudioData );
	}

    // submit buffer
	m_pSourceVoice->SubmitSourceBuffer( pBuffer );
}

//-----------------------------------------------------------------------------
// Get our device name
//-----------------------------------------------------------------------------
const char *CAudioXAudio::DeviceName( void )
{ 
	if ( m_bSurround )
	{
		return "XAudio: 5.1 Channel Surround";
	}

	return "XAudio: Stereo"; 
}

CXboxVoice::CXboxVoice()
{
	m_pXHVEngine = NULL;
}

//-----------------------------------------------------------------------------
// Initialize Voice
//-----------------------------------------------------------------------------
void CXboxVoice::VoiceInit( void )
{
	if ( !m_pXHVEngine )
	{
		// Set the processing modes
		XHV_PROCESSING_MODE rgMode = XHV_VOICECHAT_MODE;

		// Set up parameters for the voice chat engine
		XHV_INIT_PARAMS xhvParams = {0};
		xhvParams.dwMaxRemoteTalkers = MAX_PLAYERS;
		xhvParams.dwMaxLocalTalkers = XUSER_MAX_COUNT;
		xhvParams.localTalkerEnabledModes = &rgMode;
		xhvParams.remoteTalkerEnabledModes = &rgMode;
		xhvParams.dwNumLocalTalkerEnabledModes = 1;
		xhvParams.dwNumRemoteTalkerEnabledModes = 1;
		xhvParams.pXAudio2 = CAudioXAudio::m_pSingleton->GetXAudio2();

		// Create the engine
		HRESULT hr = XHV2CreateEngine( &xhvParams, NULL, &m_pXHVEngine );
		if ( hr != S_OK )
		{
			Warning( "Couldn't load XHV engine!\n" );
		}
	}

	VoiceResetLocalData( );
}

void CXboxVoice::VoiceShutdown( void )
{
	if ( !m_pXHVEngine )
		return;
	
	m_pXHVEngine->Release();
	m_pXHVEngine = NULL;
}

void CXboxVoice::AddPlayerToVoiceList( CClientInfo *pClient, bool bLocal )
{
	XHV_PROCESSING_MODE local_proc_mode = XHV_VOICECHAT_MODE;

	for ( int i = 0; i < pClient->m_cPlayers; ++i )
	{
		if ( pClient->m_xuids[i] == 0 )
			continue;

		if ( bLocal == true )
		{
 			if ( m_pXHVEngine->RegisterLocalTalker( pClient->m_iControllers[i] ) == S_OK )
			{
				m_pXHVEngine->StartLocalProcessingModes( pClient->m_iControllers[i], &local_proc_mode, 1 );
			}
		}
		else
		{
			if ( m_pXHVEngine->RegisterRemoteTalker( pClient->m_xuids[i], NULL, NULL, NULL ) == S_OK )
			{
				m_pXHVEngine->StartRemoteProcessingModes( pClient->m_xuids[i], &local_proc_mode, 1 );
			}
		}
	}
}

void CXboxVoice::RemovePlayerFromVoiceList( CClientInfo *pClient, bool bLocal )
{
	for ( int i = 0; i < pClient->m_cPlayers; ++i )
	{
		if ( pClient->m_xuids[i] == 0 )
			continue;

		if ( bLocal == true )
		{
			m_pXHVEngine->UnregisterLocalTalker( pClient->m_iControllers[i] );
		}
		else
		{
			m_pXHVEngine->UnregisterRemoteTalker( pClient->m_xuids[i] );
		}
	}
}

void CXboxVoice::PlayIncomingVoiceData( XUID xuid, const byte *pbData, DWORD pdwSize )
{
	XUID localXUID;

	XUserGetXUID( XBX_GetPrimaryUserId(), &localXUID );

	//Hack: Don't play stuff that comes from ourselves.
	if ( localXUID == xuid )
		return;

	m_pXHVEngine->SubmitIncomingChatData( xuid, pbData, &pdwSize );
}


void CXboxVoice::UpdateHUDVoiceStatus( void )
{
	for ( int iClient = 0; iClient < cl.m_nMaxClients; iClient++ )
	{
		bool bSelf = (cl.m_nPlayerSlot == iClient);

		int iIndex = iClient + 1;
		XUID id =  g_pMatchmaking->PlayerIdToXuid( iIndex );
		
		if ( id != 0 )
		{	
			bool bTalking = false;

			if ( bSelf == true )
			{
				//Make sure the player's own label is not on.
				g_pSoundServices->OnChangeVoiceStatus( iIndex, false );

				iIndex = -1;

				if ( IsPlayerTalking( XBX_GetPrimaryUserId(), true ) )
				{
					bTalking = true;
				}
			}
			else
			{
				if ( IsPlayerTalking( id, false ) )
				{
					bTalking = true;
				}
			}
		
			g_pSoundServices->OnChangeVoiceStatus( iIndex, bTalking );
		}
		else
		{
			g_pSoundServices->OnChangeVoiceStatus( iIndex, false );
		}
	}
}

bool CXboxVoice::VoiceUpdateData( void  )
{
	DWORD dwNumPackets = 0;
	DWORD dwBytes = 0;
	WORD wVoiceBytes = 0;
	bool bShouldSend = false;
	DWORD dwVoiceFlags = m_pXHVEngine->GetDataReadyFlags();

	//Update UI stuff.
	UpdateHUDVoiceStatus();

	for ( uint i = 0; i < XUSER_MAX_COUNT; ++i )
	{
		// We currently only allow one player per console
		if ( i != XBX_GetPrimaryUserId() )
		{
			continue;
		}

		if ( IsHeadsetPresent( i ) == false )
			continue;

		if ( !(dwVoiceFlags & ( 1 << i )) )
			continue;
  
		dwBytes = m_ChatBufferSize - m_wLocalDataSize;

		if( dwBytes < XHV_VOICECHAT_MODE_PACKET_SIZE )
		{
			bShouldSend = true;
		}
		else
		{
			m_pXHVEngine->GetLocalChatData( i, m_ChatBuffer + m_wLocalDataSize, &dwBytes, &dwNumPackets );
			m_wLocalDataSize += ((WORD)dwBytes) & MAXWORD;

			if( m_wLocalDataSize > ( ( m_ChatBufferSize * 7 ) / 10 ) )
			{
				bShouldSend = true;
			}
		}

		wVoiceBytes += m_wLocalDataSize & MAXWORD;
		break;
	}

	return  bShouldSend || 
		( wVoiceBytes && 
		( GetTickCount() - m_dwLastVoiceSend ) > MAX_VOICE_BUFFER_TIME );
}

void CXboxVoice::SetPlaybackPriority( XUID remoteTalker, DWORD dwUserIndex, XHV_PLAYBACK_PRIORITY playbackPriority )
{
	m_pXHVEngine->SetPlaybackPriority( remoteTalker, dwUserIndex, playbackPriority );
}

void CXboxVoice::GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers )
{
	m_pXHVEngine->GetRemoteTalkers( (DWORD*)pNumTalkers, pRemoteTalkers );
}

void CXboxVoice::GetVoiceData( CLC_VoiceData *pMessage )
{
	byte *puchVoiceData = NULL;
	pMessage->m_nLength = m_wLocalDataSize;
	XUserGetXUID( XBX_GetPrimaryUserId(), &pMessage->m_xuid );

	puchVoiceData = m_ChatBuffer;

	pMessage->m_DataOut.StartWriting( puchVoiceData, pMessage->m_nLength );	
	pMessage->m_nLength *= 8;
	pMessage->m_DataOut.SeekToBit( pMessage->m_nLength );	 // set correct writing position
}

void CXboxVoice::VoiceSendData( INetChannel *pChannel )
{
	CLC_VoiceData voiceMsg;
	GetVoiceData( &voiceMsg );

	if ( pChannel )
	{
		pChannel->SendNetMsg( voiceMsg, false, true );
		VoiceResetLocalData();
	}
}

void CXboxVoice::VoiceResetLocalData( void )
{
	m_wLocalDataSize = 0;
	m_dwLastVoiceSend = GetTickCount();
	Q_memset( m_ChatBuffer, 0, m_ChatBufferSize );
}

bool CXboxVoice::IsPlayerTalking( XUID uid, bool bLocal )
{
	if ( bLocal == true )
	{
		return m_pXHVEngine->IsLocalTalking( XBX_GetPrimaryUserId() );
	}
	else
	{
		return !g_pMatchmaking->IsPlayerMuted( XBX_GetPrimaryUserId(), uid ) && m_pXHVEngine->IsRemoteTalking( uid );
	}

	return false;
}

bool CXboxVoice::IsHeadsetPresent( int id )
{
	return m_pXHVEngine->IsHeadsetPresent( id );
}

void CXboxVoice::RemoveAllTalkers( CClientInfo *pLocal )
{
	int numRemoteTalkers;
	XUID remoteTalkers[MAX_PLAYERS];
	GetRemoteTalkers( &numRemoteTalkers, remoteTalkers );

	for ( int iRemote = 0; iRemote < numRemoteTalkers; iRemote++ )
	{
		m_pXHVEngine->UnregisterRemoteTalker( remoteTalkers[iRemote] );
	}

	if ( pLocal )
	{
		for ( int i = 0; i < pLocal->m_cPlayers; ++i )
		{
			if ( pLocal->m_xuids[i] == 0 )
				continue;

			m_pXHVEngine->UnregisterLocalTalker( pLocal->m_iControllers[i] );
		}
	}
}