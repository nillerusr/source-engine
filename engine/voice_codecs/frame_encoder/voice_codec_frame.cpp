//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "ivoicecodec.h"
#include <string.h>
#include "tier0/dbg.h"
#include "iframeencoder.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifndef min
	#define min(a,b) ((a) < (b) ? (a) : (b))
#endif





// VoiceCodec_Frame can be used to wrap a frame encoder for the engine. As it gets sound data, it will queue it
// until it has enough for a frame, then it will compress it. Same thing for decompression.
class VoiceCodec_Frame : public IVoiceCodec
{
public:
	enum {MAX_FRAMEBUFFER_SAMPLES=1024};

					VoiceCodec_Frame(IFrameEncoder *pEncoder)
					{
						m_nEncodeBufferSamples = 0;
						m_nRawBytes = m_nRawSamples = m_nEncodedBytes = 0;
						m_pFrameEncoder = pEncoder;
					}
		
	virtual			~VoiceCodec_Frame()
	{
		if(m_pFrameEncoder)
			m_pFrameEncoder->Release();
	}
	
	virtual bool	Init( int quality )
	{
		if(m_pFrameEncoder && m_pFrameEncoder->Init(quality, m_nRawBytes, m_nEncodedBytes))
		{
			m_nRawSamples = m_nRawBytes >> 1;
			Assert(m_nRawBytes <= MAX_FRAMEBUFFER_SAMPLES && m_nEncodedBytes <= MAX_FRAMEBUFFER_SAMPLES);
			return true;
		}
		else
		{
			if(m_pFrameEncoder)
				m_pFrameEncoder->Release();

			m_pFrameEncoder = NULL;
			return false;
		}
	}

	virtual void	Release()
	{
		delete this;
	}

	virtual int		Compress(const char *pUncompressedBytes, int nSamples, char *pCompressed, int maxCompressedBytes, bool bFinal)
	{
		if(!m_pFrameEncoder)
			return 0;

		const short *pUncompressed = (const short*)pUncompressedBytes;

		int nCompressedBytes = 0;
		while((nSamples + m_nEncodeBufferSamples) >= m_nRawSamples && (maxCompressedBytes - nCompressedBytes) >= m_nEncodedBytes)
		{
			// Get the data block out.
			short samples[MAX_FRAMEBUFFER_SAMPLES];
			memcpy(samples, m_EncodeBuffer, m_nEncodeBufferSamples*BYTES_PER_SAMPLE);
			memcpy(&samples[m_nEncodeBufferSamples], pUncompressed, (m_nRawSamples - m_nEncodeBufferSamples) * BYTES_PER_SAMPLE);
			nSamples -= m_nRawSamples - m_nEncodeBufferSamples;
			pUncompressed += m_nRawSamples - m_nEncodeBufferSamples;
			m_nEncodeBufferSamples = 0;
			
			// Compress it.
			m_pFrameEncoder->EncodeFrame((const char*)samples, &pCompressed[nCompressedBytes]);
			nCompressedBytes += m_nEncodedBytes;
		}

		// Store the remaining samples.
		int nNewSamples = min(nSamples, min(m_nRawSamples-m_nEncodeBufferSamples, m_nRawSamples));
		if(nNewSamples)
		{
			memcpy(&m_EncodeBuffer[m_nEncodeBufferSamples], &pUncompressed[nSamples - nNewSamples], nNewSamples*BYTES_PER_SAMPLE);
			m_nEncodeBufferSamples += nNewSamples;
		}

		// If it must get the last data, just pad with zeros..
		if(bFinal && m_nEncodeBufferSamples && (maxCompressedBytes - nCompressedBytes) >= m_nEncodedBytes)
		{
			memset(&m_EncodeBuffer[m_nEncodeBufferSamples], 0, (m_nRawSamples - m_nEncodeBufferSamples) * BYTES_PER_SAMPLE);
			m_pFrameEncoder->EncodeFrame((const char*)m_EncodeBuffer, &pCompressed[nCompressedBytes]);
			nCompressedBytes += m_nEncodedBytes;
			m_nEncodeBufferSamples = 0;
		}

		return nCompressedBytes;
	}

	virtual int		Decompress(const char *pCompressed, int compressedBytes, char *pUncompressed, int maxUncompressedBytes)
	{
		if(!m_pFrameEncoder)
			return 0;

		Assert((compressedBytes % m_nEncodedBytes) == 0);
		int nDecompressedBytes = 0;
		int curCompressedByte = 0;
		while((compressedBytes - curCompressedByte)  >= m_nEncodedBytes && (maxUncompressedBytes - nDecompressedBytes) >= m_nRawBytes)
		{
			m_pFrameEncoder->DecodeFrame( pCompressed ? &pCompressed[curCompressedByte] : NULL, &pUncompressed[nDecompressedBytes]);
			curCompressedByte += m_nEncodedBytes;
			nDecompressedBytes += m_nRawBytes;
		}

		return nDecompressedBytes / BYTES_PER_SAMPLE;
	}

	virtual bool	ResetState()
	{
		if(m_pFrameEncoder)
			return m_pFrameEncoder->ResetState();
		else
			return false;
	}


public:
	// The codec encodes and decodes samples in fixed-size blocks, so we queue up uncompressed and decompressed data 
	// until we have blocks large enough to give to the codec.
	short				m_EncodeBuffer[MAX_FRAMEBUFFER_SAMPLES];
	int					m_nEncodeBufferSamples;

	IFrameEncoder		*m_pFrameEncoder;
	int					m_nRawBytes, m_nRawSamples;
	int					m_nEncodedBytes;
};


IVoiceCodec* CreateVoiceCodec_Frame(IFrameEncoder *pEncoder)
{
	return new VoiceCodec_Frame(pEncoder);
}

