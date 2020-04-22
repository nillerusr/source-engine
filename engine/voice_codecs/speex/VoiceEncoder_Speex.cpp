//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
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
#include "VoiceEncoder_Speex.h"
#include <stdio.h>

#define SAMPLERATE			8000	// get 8000 samples/sec
#define RAW_FRAME_SIZE		160		// in 160 samples per frame

// each quality has a differnt farme size
const int ENCODED_FRAME_SIZE [11] = {6,6,15,15,20,20,28,28,38,38,38};	

/* useful Speex voice qualities are 0,2,4,6 and 8. each quality level
   has a diffrent encoded frame size and needed bitrate:

	Quality 0 :  6 bytes/frame,  2400bps
	Quality 2 : 15 bytes/frame,  6000bps
	Quality 4 : 20 bytes/frame,  8000bps
	Quality 6 : 28 bytes/frame, 11200bps
	Quality 8 : 38 bytes/frame, 15200bps */


extern IVoiceCodec* CreateVoiceCodec_Frame(IFrameEncoder *pEncoder);

void* CreateSpeexVoiceCodec()
{
	IFrameEncoder *pEncoder = new VoiceEncoder_Speex;
	return CreateVoiceCodec_Frame( pEncoder );
}

EXPOSE_INTERFACE_FN(CreateSpeexVoiceCodec, IVoiceCodec, "vaudio_speex")

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

VoiceEncoder_Speex::VoiceEncoder_Speex()
{
	m_EncoderState = NULL;
	m_DecoderState = NULL;
	m_Quality = 0;
}

VoiceEncoder_Speex::~VoiceEncoder_Speex()
{
	TermStates();
}

bool VoiceEncoder_Speex::Init(int quality, int &rawFrameSize, int &encodedFrameSize)
{
	if ( !InitStates() )
		return false;

	rawFrameSize = RAW_FRAME_SIZE * BYTES_PER_SAMPLE;

	// map gerneral voice quality 1-5 to speex quality levels
	switch ( quality )
	{
		case 1 :	m_Quality = 0; break;
		case 2 :	m_Quality = 2; break;
		case 3 :	m_Quality = 4; break;
		case 4 :	m_Quality = 6; break;
		case 5 : 	m_Quality = 8; break;
		default	:	m_Quality = 0; break;
	}

	encodedFrameSize = ENCODED_FRAME_SIZE[m_Quality];

	speex_encoder_ctl( m_EncoderState, SPEEX_SET_QUALITY, &m_Quality);
	speex_decoder_ctl( m_DecoderState, SPEEX_SET_QUALITY, &m_Quality);

	int postfilter = 1; // Set the perceptual enhancement on
	speex_decoder_ctl( m_DecoderState, SPEEX_SET_ENH, &postfilter);

	int samplerate = SAMPLERATE;
	speex_decoder_ctl( m_DecoderState, SPEEX_SET_SAMPLING_RATE, &samplerate );
	speex_encoder_ctl( m_EncoderState, SPEEX_SET_SAMPLING_RATE, &samplerate );

	return true;
}

void VoiceEncoder_Speex::Release()
{
	delete this;
}

void VoiceEncoder_Speex::EncodeFrame(const char *pUncompressedBytes, char *pCompressed)
{
	float input[RAW_FRAME_SIZE];
	short * in = (short*)pUncompressedBytes;

	/*Copy the 16 bits values to float so Speex can work on them*/
	for (int i=0;i<RAW_FRAME_SIZE;i++)
	{
		input[i]=(float)*in;
		in++;
	}

	/*Flush all the bits in the struct so we can encode a new frame*/
	speex_bits_reset( &m_Bits );

	/*Encode the frame*/
	speex_encode( m_EncoderState, input, &m_Bits );

	/*Copy the bits to an array of char that can be written*/
	int size = speex_bits_write(&m_Bits, pCompressed, ENCODED_FRAME_SIZE[m_Quality] );
	NOTE_UNUSED( size );

	// char text[255];	_snprintf(text, 255, "outsize %i,", size ); OutputDebugStr( text );
}

void VoiceEncoder_Speex::DecodeFrame(const char *pCompressed, char *pDecompressedBytes)
{
	float output[RAW_FRAME_SIZE];
	short * out = (short*)pDecompressedBytes;

	/*Copy the data into the bit-stream struct*/
	speex_bits_read_from(&m_Bits, (char *)pCompressed, ENCODED_FRAME_SIZE[m_Quality] );

	/*Decode the data*/
	speex_decode(m_DecoderState, &m_Bits, output);
	
	/*Copy from float to short (16 bits) for output*/
	for (int i=0;i<RAW_FRAME_SIZE;i++)
	{
		*out = (short)output[i];
		out++;
	}
}

bool VoiceEncoder_Speex::ResetState()
{
	speex_encoder_ctl(m_EncoderState, SPEEX_RESET_STATE , NULL );
	speex_decoder_ctl(m_DecoderState, SPEEX_RESET_STATE , NULL );
	return true;
}

bool VoiceEncoder_Speex::InitStates()
{
	speex_bits_init(&m_Bits);
	
	m_EncoderState = speex_encoder_init( &speex_nb_mode );	// narrow band mode 8kbp

	m_DecoderState = speex_decoder_init( &speex_nb_mode );
	
	return m_EncoderState && m_DecoderState;
}

void VoiceEncoder_Speex::TermStates()
{
	if(m_EncoderState)
	{
		speex_encoder_destroy( m_EncoderState );
		m_EncoderState = NULL;
	}

	if(m_DecoderState)
	{
		speex_decoder_destroy( m_DecoderState );
		m_DecoderState = NULL;
	}

	speex_bits_destroy( &m_Bits );
}
