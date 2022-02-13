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

#if !defined VOICEENCODER_SPEEX_H
#define VOICEENCODER_SPEEX_H

#pragma once

#include "iframeencoder.h"

#include <speex/speex.h>

class VoiceEncoder_Speex : public IFrameEncoder
{
public:
	VoiceEncoder_Speex();
	virtual ~VoiceEncoder_Speex();

	// Interfaces IFrameDecoder

	bool Init(int quality, int &rawFrameSize, int &encodedFrameSize);
	void Release();
	void DecodeFrame(const char *pCompressed, char *pDecompressedBytes);
	void EncodeFrame(const char *pUncompressedBytes, char *pCompressed);
	bool ResetState();

private:

	bool	InitStates();
	void	TermStates();

	int			m_Quality;		// voice codec quality ( 0,2,4,6,8 )
	void *		m_EncoderState;	// speex internal encoder state
	void *		m_DecoderState; // speex internal decoder state

	SpeexBits	m_Bits;	// helpful bit buffer structure
};

#endif // !defined(AFX_FRAMEENCODER_SPEEX_H__C160B146_3782_4D91_A022_0B852C57BAB9__INCLUDED_)
