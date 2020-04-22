//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "ivoicecodec.h"
#include "iframeencoder.h"

#include <stdio.h>
#include <opus/opus.h>
#include <opus/opus_custom.h>


// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"
#include "tier0/dbg.h"

#define CHANNELS 1

struct opus_options
{
	int iSampleRate;
	int iRawFrameSize;
	int iPacketSize;
};

opus_options g_OpusOpts[] =
{
        {44100, 256, 120},
        {22050, 120, 60},
        {22050, 256, 60},
        {22050, 512, 64},
};

class VoiceEncoder_Opus : public IFrameEncoder
{
public:
	VoiceEncoder_Opus();
	virtual ~VoiceEncoder_Opus();

	// Interfaces IFrameDecoder

	bool Init(int quality, int &rawFrameSize, int &encodedFrameSize);
	void Release();
	void DecodeFrame(const char *pCompressed, char *pDecompressedBytes);
	void EncodeFrame(const char *pUncompressedBytes, char *pCompressed);
	bool ResetState();

private:

	bool	InitStates();
	void	TermStates();

	OpusCustomEncoder *m_EncoderState;	// Celt internal encoder state
	OpusCustomDecoder *m_DecoderState; // Celt internal decoder state
	OpusCustomMode	*m_Mode;

	int m_iVersion;
};

extern IVoiceCodec* CreateVoiceCodec_Frame(IFrameEncoder *pEncoder);

void* CreateCeltVoiceCodec()
{
	IFrameEncoder *pEncoder = new VoiceEncoder_Opus;
	return CreateVoiceCodec_Frame( pEncoder );
}

EXPOSE_INTERFACE_FN(CreateCeltVoiceCodec, IVoiceCodec, "vaudio_opus")

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

VoiceEncoder_Opus::VoiceEncoder_Opus()
{
	m_EncoderState = NULL;
	m_DecoderState = NULL;
	m_Mode = NULL;
	m_iVersion = 0;
}

VoiceEncoder_Opus::~VoiceEncoder_Opus()
{
	TermStates();
}

bool VoiceEncoder_Opus::Init( int quality, int &rawFrameSize, int &encodedFrameSize)
{
	m_iVersion = quality;

	rawFrameSize = g_OpusOpts[m_iVersion].iRawFrameSize * BYTES_PER_SAMPLE;

	int iError = 0;

	m_Mode = opus_custom_mode_create( g_OpusOpts[m_iVersion].iSampleRate, g_OpusOpts[m_iVersion].iRawFrameSize, &iError );

	if ( iError != 0 ) {
		Msg( "Opus init failed with error %d\n", iError );
		return false;
	}

	m_EncoderState = opus_custom_encoder_create( m_Mode, CHANNELS, NULL);
	m_DecoderState = opus_custom_decoder_create( m_Mode, CHANNELS, NULL);

	if ( !InitStates() )
		return false;

	encodedFrameSize = g_OpusOpts[m_iVersion].iPacketSize;

	return true;
}

void VoiceEncoder_Opus::Release()
{
	delete this;
}

void VoiceEncoder_Opus::EncodeFrame(const char *pUncompressedBytes, char *pCompressed)
{
	unsigned char output[1024];

	opus_custom_encode( m_EncoderState, (opus_int16*)pUncompressedBytes, g_OpusOpts[m_iVersion].iRawFrameSize, output, g_OpusOpts[m_iVersion].iPacketSize );

	for ( int i = 0; i < g_OpusOpts[m_iVersion].iPacketSize; i++ )
	{
		*pCompressed = (char)output[i];
		pCompressed++;
	}
}

void VoiceEncoder_Opus::DecodeFrame(const char *pCompressed, char *pDecompressedBytes)
{
	unsigned char output[1024];
	char *out = (char *)pCompressed;

	if ( !pCompressed )
	{
		int decoded = opus_custom_decode( m_DecoderState, NULL, g_OpusOpts[m_iVersion].iPacketSize, (opus_int16 *)pDecompressedBytes, g_OpusOpts[m_iVersion].iRawFrameSize );
		return;
	}

	for ( int i = 0; i < g_OpusOpts[m_iVersion].iPacketSize; i++ )
	{
		output[i] = ( unsigned char ) ( ( *out < 0 ) ? (*out + 256) : *out );
		out++;
	}

	//celt_decoder_ctl( m_DecoderState, CELT_RESET_STATE_REQUEST, NULL );
	int decoded = opus_custom_decode( m_DecoderState, output, g_OpusOpts[m_iVersion].iPacketSize, (opus_int16 *)pDecompressedBytes, g_OpusOpts[m_iVersion].iRawFrameSize );
}

bool VoiceEncoder_Opus::ResetState()
{
	opus_custom_encoder_ctl(m_EncoderState, OPUS_RESET_STATE );
	opus_custom_decoder_ctl(m_DecoderState, OPUS_RESET_STATE );

	return true;
}

bool VoiceEncoder_Opus::InitStates()
{
	if ( !m_EncoderState || !m_DecoderState )
		return false;

	opus_custom_encoder_ctl(m_EncoderState, OPUS_RESET_STATE );
	opus_custom_decoder_ctl(m_DecoderState, OPUS_RESET_STATE );

	return true;
}

void VoiceEncoder_Opus::TermStates()
{
	if( m_EncoderState )
	{
		opus_custom_encoder_destroy( m_EncoderState );
		m_EncoderState = NULL;
	}

	if( m_DecoderState )
	{
		opus_custom_decoder_destroy( m_DecoderState );
		m_DecoderState = NULL;
	}

	opus_custom_mode_destroy( m_Mode );
}
