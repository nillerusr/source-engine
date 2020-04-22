//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

/* This product contains Speex software.  The license terms of the Speex
software, distributed with this product, are as follows:

© 2002-2003, Jean-Marc Valin/Xiph.Org Foundation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or
other materials provided with the distribution.

Neither the name of the Xiph.org Foundation nor the names of its
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

This software is provided by the copyright holders and contributors "as is"
and any express or implied warranties, including, but not limited to, the
implied warranties of merchantability and fitness for a particular purpose
are disclaimed. In no event shall the foundation or contributors be liable
for any direct, indirect, incidental, special, exemplary, or consequential
damages (including, but not limited to, procurement of substitute goods or
services; loss of use, data, or profits; or business interruption) however
caused and on any theory of liability, whether in contract, strict
liability, or tort (including negligence or otherwise) arising in any way
out of the use of this software, even if advised of the possibility of such
damage. */

#include "ivoicecodec.h"
#include "iframeencoder.h"

#ifdef POSIX
#include "source/osx/config.h"
#else
#include "source/msvc/config.h"
#endif
#include <stdio.h>
#include "celt.h"


// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


#define CHANNELS 1

struct celt_versions
{
	int iSampleRate;
	int iRawFrameSize;
	int iPacketSize;
};

#define CELT_VERSION 4

celt_versions g_CeltVersion[CELT_VERSION] = 
{
	{
		44100, 256, 120 
	},

	{
		22050, 120, 60
	},

	{
		22050, 256, 60
	},

	{
		22050, 512, 64
	},
};

class VoiceEncoder_Celt : public IFrameEncoder
{
public:
	VoiceEncoder_Celt();
	virtual ~VoiceEncoder_Celt();

	// Interfaces IFrameDecoder

	bool Init(int quality, int &rawFrameSize, int &encodedFrameSize);
	void Release();
	void DecodeFrame(const char *pCompressed, char *pDecompressedBytes);
	void EncodeFrame(const char *pUncompressedBytes, char *pCompressed);
	bool ResetState();

private:

	bool	InitStates();
	void	TermStates();
		
	CELTEncoder *m_EncoderState;	// Celt internal encoder state
	CELTDecoder *m_DecoderState; // Celt internal decoder state
	CELTMode	*m_Mode;

	int m_iVersion;
};

extern IVoiceCodec* CreateVoiceCodec_Frame(IFrameEncoder *pEncoder);

void* CreateCeltVoiceCodec()
{
	IFrameEncoder *pEncoder = new VoiceEncoder_Celt;
	return CreateVoiceCodec_Frame( pEncoder );
}

EXPOSE_INTERFACE_FN(CreateCeltVoiceCodec, IVoiceCodec, "vaudio_celt")

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

VoiceEncoder_Celt::VoiceEncoder_Celt()
{
	m_EncoderState = NULL;
	m_DecoderState = NULL;
	m_Mode = NULL;
	m_iVersion = 0;
}

VoiceEncoder_Celt::~VoiceEncoder_Celt()
{
	TermStates();
}

bool VoiceEncoder_Celt::Init( int quality, int &rawFrameSize, int &encodedFrameSize)
{
	if ( quality >= CELT_VERSION )
		return false;

	m_iVersion = quality;

	rawFrameSize = g_CeltVersion[m_iVersion].iRawFrameSize * BYTES_PER_SAMPLE;

	int iError = 0;

	m_Mode = celt_mode_create( g_CeltVersion[m_iVersion].iSampleRate, g_CeltVersion[m_iVersion].iRawFrameSize, &iError );
	m_EncoderState = celt_encoder_create_custom( m_Mode, CHANNELS, NULL);
	m_DecoderState = celt_decoder_create_custom( m_Mode, CHANNELS, NULL);

	if ( !InitStates() )
		return false;

	encodedFrameSize = g_CeltVersion[m_iVersion].iPacketSize;

	return true;
}

void VoiceEncoder_Celt::Release()
{
	delete this;
}

void VoiceEncoder_Celt::EncodeFrame(const char *pUncompressedBytes, char *pCompressed)
{
	unsigned char output[1024];

	celt_encode( m_EncoderState, (celt_int16*)pUncompressedBytes, g_CeltVersion[m_iVersion].iRawFrameSize, output, g_CeltVersion[m_iVersion].iPacketSize );

	for ( int i = 0; i < g_CeltVersion[m_iVersion].iPacketSize; i++ )
	{
		*pCompressed = (char)output[i];
		pCompressed++;
	}
}

void VoiceEncoder_Celt::DecodeFrame(const char *pCompressed, char *pDecompressedBytes)
{
	unsigned char output[1024];
	char *out = (char *)pCompressed;

	if ( !pCompressed )
	{
		celt_decode( m_DecoderState, NULL, g_CeltVersion[m_iVersion].iPacketSize, (celt_int16 *)pDecompressedBytes, g_CeltVersion[m_iVersion].iRawFrameSize );
		return;
	}

	for ( int i = 0; i < g_CeltVersion[m_iVersion].iPacketSize; i++ )
	{
		output[i] = ( unsigned char ) ( ( *out < 0 ) ? (*out + 256) : *out );
		out++;
	}


	//celt_decoder_ctl( m_DecoderState, CELT_RESET_STATE_REQUEST, NULL );
	celt_decode( m_DecoderState, output, g_CeltVersion[m_iVersion].iPacketSize, (celt_int16 *)pDecompressedBytes, g_CeltVersion[m_iVersion].iRawFrameSize );
}

bool VoiceEncoder_Celt::ResetState()
{
	celt_encoder_ctl(m_EncoderState, CELT_RESET_STATE_REQUEST , NULL );
	celt_decoder_ctl(m_DecoderState, CELT_RESET_STATE_REQUEST , NULL );

	return true;
}

bool VoiceEncoder_Celt::InitStates()
{
	if ( !m_EncoderState || !m_DecoderState )
		return false;

	celt_encoder_ctl( m_EncoderState, CELT_RESET_STATE_REQUEST , NULL );
	celt_decoder_ctl( m_DecoderState, CELT_RESET_STATE_REQUEST , NULL );
	
	return true;
}

void VoiceEncoder_Celt::TermStates()
{
	if( m_EncoderState )
	{
		celt_encoder_destroy( m_EncoderState );
		m_EncoderState = NULL;
	}

	if( m_DecoderState )
	{
		celt_decoder_destroy( m_DecoderState );
		m_DecoderState = NULL;
	}

	celt_mode_destroy( m_Mode );
}
