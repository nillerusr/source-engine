//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IFRAMEENCODER_H
#define IFRAMEENCODER_H
#pragma once


// A frame encoder is a codec that encodes and decodes data in fixed-size frames.
// VoiceCodec_Frame handles queuing of data and the IVoiceCodec interface.
abstract_class IFrameEncoder
{
public:
	virtual			~IFrameEncoder() {}

	// This is called by VoiceCodec_Frame to see if it can initialize..
	// Fills in the uncompressed and encoded frame size (both are in BYTES).
	virtual bool	Init(int quality, int &rawFrameSize, int &encodedFrameSize) = 0;

	virtual void	Release() = 0;
	
	// pUncompressed is 8-bit signed mono sound data with 'rawFrameSize' bytes.
	// pCompressed is the size of encodedFrameSize.
	virtual void	EncodeFrame(const char *pUncompressed, char *pCompressed) = 0;
	
	// pCompressed is encodedFrameSize.
	// pDecompressed is where the 8-bit signed mono samples are stored and has space for 'rawFrameSize' bytes.
	virtual void	DecodeFrame(const char *pCompressed, char *pDecompressed) = 0;

	// Some codecs maintain state between Compress and Decompress calls. This should clear that state.
	virtual bool	ResetState() = 0;
};


#endif // IFRAMEENCODER_H
