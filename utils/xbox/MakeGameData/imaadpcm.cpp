//========= Copyright Valve Corporation, All rights reserved. ============//
/***************************************************************************
 *
 *  Copyright (C) 2001 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       imaadpcm.cpp
 *  Content:    IMA ADPCM CODEC.
 *  History:
 *   Date       By      Reason
 *   ====       ==      ======
 *  04/29/01    dereks  Created.
 *  06/12/01    jharding Adapted for command-line encode
 *
 ****************************************************************************/

#include <stdio.h>
#include <wtypes.h>
#include <assert.h>
#include "imaadpcm.h"

// 1/2 the range of the searchable step indices
// for a particular block when optimizing on a
// per-block basis.  Widening or narrowing this
// range may produce better/worse encodings.
// Experimentation may be necessary.  Higher values
// cause each block to be encoded better, but may
// produce popping in particularly fast attacks across
// blocks, while smaller values limit the number
// of encodings you consider
#define STEPINDEXSEARCHRANGE (24)

/****************************************************************************
 *
 *  CImaAdpcmCodec
 *
 *  Description:
 *      Object constructor.
 *
 *  Arguments:
 *      (void)
 *
 *  Returns:  
 *      (void)
 *
 ****************************************************************************/

//
// This array is used by NextStepIndex to determine the next step index to use.  
// The step index is an index to the m_asStep[] array, below.
//

const short CImaAdpcmCodec::m_asNextStep[16] =
{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

//
// This array contains the array of step sizes used to encode the ADPCM
// samples.  The step index in each ADPCM block is an index to this array.
//

const short CImaAdpcmCodec::m_asStep[89] =
{
        7,     8,     9,    10,    11,    12,    13,
       14,    16,    17,    19,    21,    23,    25,
       28,    31,    34,    37,    41,    45,    50,
       55,    60,    66,    73,    80,    88,    97,
      107,   118,   130,   143,   157,   173,   190,
      209,   230,   253,   279,   307,   337,   371,
      408,   449,   494,   544,   598,   658,   724,
      796,   876,   963,  1060,  1166,  1282,  1411,
     1552,  1707,  1878,  2066,  2272,  2499,  2749,
     3024,  3327,  3660,  4026,  4428,  4871,  5358,
     5894,  6484,  7132,  7845,  8630,  9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767
};

CImaAdpcmCodec::CImaAdpcmCodec
(
    void
)
{
}


/****************************************************************************
 *
 *  ~CImaAdpcmCodec
 *
 *  Description:
 *      Object destructor.
 *
 *  Arguments:
 *      (void)
 *
 *  Returns:  
 *      (void)
 *
 ****************************************************************************/

CImaAdpcmCodec::~CImaAdpcmCodec
(
    void
)
{
}


/****************************************************************************
 *
 *  Initialize
 *
 *  Description:
 *      Initializes the object.
 *
 *  Arguments:
 *      LPCIMAADPCMWAVEFORMAT [in]: encoded data format.
 *      BOOL [in]: TRUE to initialize the object as an encoder.
 *
 *  Returns:  
 *      BOOL: TRUE on success.
 *
 ****************************************************************************/

BOOL
CImaAdpcmCodec::Initialize
(
    LPCIMAADPCMWAVEFORMAT               pwfxEncode, 
    CODEC_MODE                          cmCodecMode
)
{
    static const LPFNIMAADPCMCONVERT    apfnConvert[2][2] = 
    { 
        {
            DecodeM16,
            DecodeS16 
        },
        {
            EncodeM16,
            EncodeS16 
        }
    };
    
    if(!IsValidImaAdpcmFormat(pwfxEncode))
    {
        return FALSE;
    }

    //
    // Save the format data
    //

    m_wfxEncode = *pwfxEncode;
    m_cmCodecMode = cmCodecMode;

    //
    // Set up the conversion function
    //

    m_pfnConvert = apfnConvert[!(m_cmCodecMode == CODEC_MODE_DECODE)][m_wfxEncode.wfx.nChannels - 1];

    //
    // Initialize the stepping indices
    //

    m_nStepIndexL = m_nStepIndexR = 0;

    return TRUE;
}


/****************************************************************************
 *
 *  Convert
 *
 *  Description:
 *      Converts data from the source to destination format.
 *
 *  Arguments:
 *      LPCVOID [in]: source buffer.
 *      LPVOID [out]: destination buffer.
 *      UINT [in]: block count.
 *
 *  Returns:  
 *      BOOL: TRUE on success.
 *
 ****************************************************************************/

BOOL
CImaAdpcmCodec::Convert
(
    LPCVOID                 pvSrc,
    LPVOID                  pvDst,
    UINT                    cBlocks
)
{
    // Array of decoders
    static const LPFNIMAADPCMCONVERT    apfnDecoders[2] = 
    { 
        DecodeM16,
        DecodeS16 
    };

    // Both destination and source block sizes
    DWORD dwSrcBlockSize = m_wfxEncode.wfx.nChannels * m_wfxEncode.wSamplesPerBlock * sizeof(short);
    DWORD dwDstBlockSize = m_wfxEncode.wfx.nBlockAlign;

    // Zero out the output
    ZeroMemory( pvDst, cBlocks * dwDstBlockSize );

    switch( m_cmCodecMode )
    {
        case CODEC_MODE_DECODE:
            // If we are decoding, just do it
            return m_pfnConvert( (LPBYTE)pvSrc, (LPBYTE)pvDst, cBlocks, m_wfxEncode.wfx.nBlockAlign, m_wfxEncode.wSamplesPerBlock, &m_nStepIndexL, &m_nStepIndexR );

        case CODEC_MODE_ENCODE_NORMAL:
            // Normal encode
            // We have some output right now, so this becomes a separate case.
            // Otherwise, it would be the same as CODEC_MODE_DECODE
            {
                printf("Using normal encoding...\n");

                // Allocate temporary buffers
                LPBYTE pvDecoded = new BYTE[cBlocks * dwSrcBlockSize];
                if( !pvDecoded )
                    return FALSE;

                // Find the decoder
                LPFNIMAADPCMCONVERT pfnOppConvert;
                pfnOppConvert = apfnDecoders[m_wfxEncode.wfx.nChannels - 1];

                // Encode the stream
                if( !m_pfnConvert( (LPBYTE)pvSrc, (LPBYTE)pvDst, cBlocks, m_wfxEncode.wfx.nBlockAlign, m_wfxEncode.wSamplesPerBlock, &m_nStepIndexL, &m_nStepIndexR ) )
                {
                    delete[] pvDecoded;
                    return FALSE;
                }

                // Decode it back
                if( !pfnOppConvert( (LPBYTE)pvDst, pvDecoded, cBlocks, m_wfxEncode.wfx.nBlockAlign, XBOX_ADPCM_SAMPLES_PER_BLOCK, &m_nStepIndexL, &m_nStepIndexR ) )
                {
                    delete[] pvDecoded;
                    return FALSE;
                }

                // Report the normal difference
                printf( "Difference between original and decoded streams: 0x%I64x\n",
                        CalcDifference( (LPBYTE)pvSrc, pvDecoded, cBlocks, cBlocks, dwSrcBlockSize ) );

                delete[] pvDecoded;
            }
            break;

        case CODEC_MODE_ENCODE_OPTIMIZE_WHOLE_FILE:
            // Optimize whole file encode
            // Encode the file with each possible starting step index
            // and pick the best one
            {
                printf("Using whole file encoding...\n");

                // Allocate temporary buffers
                LPBYTE pvTempDst = new BYTE[cBlocks * dwDstBlockSize];
                if(!pvTempDst)
                    return FALSE;

                LPBYTE pvDecoded = new BYTE[cBlocks * dwSrcBlockSize];
                if(!pvDecoded)
                {
                    delete[] pvTempDst;
                    return FALSE;
                }

                // Find the decoder
                LPFNIMAADPCMCONVERT pfnOppConvert;
                pfnOppConvert = apfnDecoders[m_wfxEncode.wfx.nChannels - 1];

                // Keep track of the best encoding, as well as the chosen step index
                ULONGLONG ullLeastDiff = (ULONGLONG)-1;
                UINT      uChosen = (UINT)-1;

                // Encode the entire stream with each step index and choose the best one
                for( UINT i = 0; i < ARRAYSIZE(m_asStep); ++i )
                {
                    ZeroMemory( pvTempDst, cBlocks * dwDstBlockSize );
                    ZeroMemory( pvDecoded, cBlocks * dwSrcBlockSize );

                    // Encode
                    m_nStepIndexL = m_nStepIndexR = i;
                    if( !m_pfnConvert( (LPBYTE)pvSrc, pvTempDst, cBlocks, m_wfxEncode.wfx.nBlockAlign, m_wfxEncode.wSamplesPerBlock, &m_nStepIndexL, &m_nStepIndexR ) )
                        continue;

                    // Decode
                    if( !pfnOppConvert( pvTempDst, pvDecoded, cBlocks, m_wfxEncode.wfx.nBlockAlign, XBOX_ADPCM_SAMPLES_PER_BLOCK, &m_nStepIndexL, &m_nStepIndexR ) )
                        continue;

                    // Diff
                    ULONGLONG ullDiff = CalcDifference( (LPBYTE)pvSrc, pvDecoded, cBlocks, cBlocks, dwSrcBlockSize );
                    if( ullDiff < ullLeastDiff )
                    {
                        ullLeastDiff = ullDiff;
                        uChosen = i;
                        CopyMemory( (LPBYTE)pvDst, pvTempDst, cBlocks * dwDstBlockSize );
                    }
                }

                // Report the optimized difference
                printf( "Difference between original and decoded streams: 0x%I64x\n", ullLeastDiff );
                printf( "Step index chosen: %d\n", uChosen );

                delete[] pvTempDst;
                delete[] pvDecoded;
            }
            break;

        case CODEC_MODE_ENCODE_OPTIMIZE_EACH_BLOCK:
            // Optimize per block encode
            // Encode each block within the file with each
            // possible starting step index and pick the
            // best one for each block
            {
                printf("Using per-block encoding\n\n");

                // Allocate temporary buffers
                LPBYTE pvTempDst = new BYTE[dwDstBlockSize];
                if( !pvTempDst )
                    return FALSE;

                LPBYTE pvDecoded = new BYTE[dwSrcBlockSize];
                if( !pvDecoded )
                {
                    delete[] pvTempDst;
                    return FALSE;
                }

                // Find the decoder
                LPFNIMAADPCMCONVERT pfnOppConvert;
                pfnOppConvert = apfnDecoders[m_wfxEncode.wfx.nChannels - 1];

                // We keep track of the best step index of the previous block
                // This enables us to search a small range of values close to
                // this value (the size of 2*STEPINDEXSEARCHRANGE)
                // To begin, the previous block's best step index was -1.
                INT iPreviousBestStepIndex = -1;

                for( UINT c = 0; c < cBlocks; ++c )
                {
                    ULONGLONG ullLeastDiff = (ULONGLONG)-1;
                    INT iThisBestStepIndex = -1;
                    INT iStartIndex, iStopIndex;

                    // Setup the start/stop indices properly
                    if( iPreviousBestStepIndex == -1 )
                    {
                        // If the previous best step index is -1,
                        // then we haven't yet encoded a block.  Search
                        // through the entire range of step indices,
                        // rather than just in a limited range
                        iStartIndex = 0;
                        iStopIndex = ARRAYSIZE( m_asStep );
                    }
                    else
                    {
                        // Keep the range of indices to search limited
                        // to around the previously chosen step index
                        iStartIndex = iPreviousBestStepIndex - STEPINDEXSEARCHRANGE;
                        iStopIndex = iPreviousBestStepIndex + STEPINDEXSEARCHRANGE + 1;
                    }

                    // Try each step index in the searchable range and choose the best one
                    // for this block
                    for( INT i = iStartIndex; i < iStopIndex; ++i )
                    {
                        // Don't consider anything out of range
                        if( i < 0 || i >= ARRAYSIZE( m_asStep ) )
                            continue;

                        // Zero out the temporary buffers
                        ZeroMemory( pvTempDst, dwDstBlockSize );
                        ZeroMemory( pvDecoded, dwSrcBlockSize );

                        // Encode
                        m_nStepIndexL = m_nStepIndexR = i;
                        if( !m_pfnConvert( (LPBYTE)pvSrc + c*dwSrcBlockSize, pvTempDst, 1, m_wfxEncode.wfx.nBlockAlign, m_wfxEncode.wSamplesPerBlock, &m_nStepIndexL, &m_nStepIndexR ) )
                            continue;

                        // Decode
                        if( !pfnOppConvert( pvTempDst, pvDecoded, 1, m_wfxEncode.wfx.nBlockAlign, XBOX_ADPCM_SAMPLES_PER_BLOCK, &m_nStepIndexL, &m_nStepIndexR ) )
                            continue;
                        
                        // Diff
                        ULONGLONG ullDiff = CalcDifference( (LPBYTE)pvSrc + c*dwSrcBlockSize, pvDecoded, 1, cBlocks, dwSrcBlockSize );
                        if( ullDiff < ullLeastDiff )
                        {
                            ullLeastDiff        = ullDiff;
                            iThisBestStepIndex  = i;
                            CopyMemory( (LPBYTE)pvDst + c*dwDstBlockSize, (LPBYTE)pvTempDst, dwDstBlockSize );
                        }
                    }

                    // Save the best step index for this block
                    iPreviousBestStepIndex = iThisBestStepIndex;
                }

                delete[] pvTempDst;
                delete[] pvDecoded;

                // Report on the optimized difference
                pvDecoded = new BYTE[cBlocks * dwSrcBlockSize];
                pfnOppConvert( (LPBYTE)pvDst, pvDecoded, cBlocks, m_wfxEncode.wfx.nBlockAlign, XBOX_ADPCM_SAMPLES_PER_BLOCK, &m_nStepIndexL, &m_nStepIndexR );
                printf( "Difference between original and decoded streams: 0x%I64x\n", CalcDifference( (LPBYTE)pvSrc, pvDecoded, cBlocks, cBlocks, dwSrcBlockSize ) );

                delete[] pvDecoded;
            }
            break;
    }

    return TRUE;
}


/****************************************************************************
 *
 *  Reset
 *
 *  Description:
 *      Resets the conversion operation.
 *
 *  Arguments:
 *      (void)
 *
 *  Returns:  
 *      (void)
 *
 ****************************************************************************/

void
CImaAdpcmCodec::Reset
(
    void
)
{
    //
    // Reset the stepping indices
    //

    m_nStepIndexL = m_nStepIndexR = 0;
}


/****************************************************************************
 *
 *  GetEncodeAlignment
 *
 *  Description:
 *      Gets the alignment of an encoded buffer.
 *
 *  Arguments:
 *      (void)
 *
 *  Returns:  
 *      WORD: alignment, in bytes.
 *
 ****************************************************************************/

WORD
CImaAdpcmCodec::GetEncodeAlignment
(
    void
)
{
    return m_wfxEncode.wfx.nBlockAlign;
}


/****************************************************************************
 *
 *  GetDecodeAlignment
 *
 *  Description:
 *      Gets the alignment of a decoded buffer.
 *
 *  Arguments:
 *      (void)
 *
 *  Returns:  
 *      DWORD: alignment, in bytes.
 *
 ****************************************************************************/

WORD
CImaAdpcmCodec::GetDecodeAlignment
(
    void
)
{
    return m_wfxEncode.wSamplesPerBlock * m_wfxEncode.wfx.nChannels * IMAADPCM_PCM_BITS_PER_SAMPLE / 8;
}


/****************************************************************************
 *
 *  CalculateEncodeAlignment
 *
 *  Description:
 *      Calculates an encoded data block alignment based on a PCM sample
 *      count and an alignment multiplier.
 *
 *  Arguments:
 *      WORD [in]: channel count.
 *      WORD [in]: PCM samples per block.
 *
 *  Returns:  
 *      WORD: alignment, in bytes.
 *
 ****************************************************************************/

WORD
CImaAdpcmCodec::CalculateEncodeAlignment
(
    WORD                    nChannels,
    WORD                    nSamplesPerBlock
)
{
    const WORD              nEncodedSampleBits  = nChannels * IMAADPCM_BITS_PER_SAMPLE;
    const WORD              nHeaderBytes        = nChannels * IMAADPCM_HEADER_LENGTH;
    INT                     nBlockAlign;

    //
    // Calculate the raw block alignment that nSamplesPerBlock dictates.  This
    // value may include a partial encoded sample, so be sure to round up.
    //
    // Start with the samples-per-block, minus 1.  The first sample is actually
    // stored in the header.
    //

    nBlockAlign = nSamplesPerBlock - 1;

    //
    // Convert to encoded sample size
    //

    nBlockAlign *= nEncodedSampleBits;
    nBlockAlign += 7;
    nBlockAlign /= 8;

    //
    // The stereo encoder requires that there be at least two DWORDs to process
    //

    nBlockAlign += 7;
    nBlockAlign /= 8;
    nBlockAlign *= 8;

    //
    // Add the header
    //

    nBlockAlign += nHeaderBytes;

    // We used an INT temporarily, but the final result should fit into a WORD
    assert( nBlockAlign < 0xFFFF );
    return (WORD)nBlockAlign;
}


/****************************************************************************
 *
 *  CreatePcmFormat
 *
 *  Description:
 *      Creates a PCM format descriptor.
 *
 *  Arguments:
 *      WORD [in]: channel count.
 *      DWORD [in]: sampling rate.
 *      LPWAVEFORMATEX [out]: format descriptor.
 *
 *  Returns:  
 *      (void)
 *
 ****************************************************************************/

void
CImaAdpcmCodec::CreatePcmFormat
(
    WORD                    nChannels, 
    DWORD                   nSamplesPerSec, 
    LPWAVEFORMATEX          pwfx
)
{
    pwfx->wFormatTag = WAVE_FORMAT_PCM;
    pwfx->nChannels = nChannels;
    pwfx->nSamplesPerSec = nSamplesPerSec;
    pwfx->nBlockAlign = nChannels * IMAADPCM_PCM_BITS_PER_SAMPLE / 8;
    pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
    pwfx->wBitsPerSample = IMAADPCM_PCM_BITS_PER_SAMPLE;
}


/****************************************************************************
 *
 *  CreateImaAdpcmFormat
 *
 *  Description:
 *      Creates an IMA ADPCM format descriptor.
 *
 *  Arguments:
 *      WORD [in]: channel count.
 *      DWORD [in]: sampling rate.
 *      LPIMAADPCMWAVEFORMAT [out]: format descriptor.
 *
 *  Returns:  
 *      (void)
 *
 ****************************************************************************/

void
CImaAdpcmCodec::CreateImaAdpcmFormat
(
    WORD                    nChannels, 
    DWORD                   nSamplesPerSec, 
    WORD                    nSamplesPerBlock,
    LPIMAADPCMWAVEFORMAT    pwfx
)
{
    pwfx->wfx.wFormatTag = WAVE_FORMAT_XBOX_ADPCM;
    pwfx->wfx.nChannels = nChannels;
    pwfx->wfx.nSamplesPerSec = nSamplesPerSec;
    pwfx->wfx.nBlockAlign = CalculateEncodeAlignment(nChannels, nSamplesPerBlock);
    pwfx->wfx.nAvgBytesPerSec = nSamplesPerSec * pwfx->wfx.nBlockAlign / nSamplesPerBlock;
    pwfx->wfx.wBitsPerSample = IMAADPCM_BITS_PER_SAMPLE;
    pwfx->wfx.cbSize = sizeof(*pwfx) - sizeof(pwfx->wfx);
    pwfx->wSamplesPerBlock = nSamplesPerBlock;
}


/****************************************************************************
 *
 *  IsValidPcmFormat
 *
 *  Description:
 *      Validates a format structure.
 *
 *  Arguments:
 *      LPCWAVEFORMATEX [in]: format.
 *
 *  Returns:  
 *      BOOL: TRUE on success.
 *
 ****************************************************************************/

BOOL 
CImaAdpcmCodec::IsValidPcmFormat
(
    LPCWAVEFORMATEX         pwfx
)
{
    if(WAVE_FORMAT_PCM != pwfx->wFormatTag)
    {
        return FALSE;
    }
    
    if((pwfx->nChannels < 1) || (pwfx->nChannels > IMAADPCM_MAX_CHANNELS))
    {
        return FALSE;
    }

    if(IMAADPCM_PCM_BITS_PER_SAMPLE != pwfx->wBitsPerSample)
    {
        return FALSE;
    }

    if(pwfx->nChannels * pwfx->wBitsPerSample / 8 != pwfx->nBlockAlign)
    {
        return FALSE;
    }

    if(pwfx->nBlockAlign * pwfx->nSamplesPerSec != pwfx->nAvgBytesPerSec)
    {
        return FALSE;
    }

    return TRUE;
}


/****************************************************************************
 *
 *  IsValidXboxAdpcmFormat
 *
 *  Description:
 *      Validates a format structure.
 *
 *  Arguments:
 *      LPCIMAADPCMWAVEFORMAT [in]: format.
 *
 *  Returns:  
 *      BOOL: TRUE on success.
 *
 ****************************************************************************/

BOOL 
CImaAdpcmCodec::IsValidImaAdpcmFormat
(
    LPCIMAADPCMWAVEFORMAT   pwfx
)
{
    if(WAVE_FORMAT_XBOX_ADPCM != pwfx->wfx.wFormatTag)
    {
        return FALSE;
    }

    if(sizeof(*pwfx) - sizeof(pwfx->wfx) != pwfx->wfx.cbSize)
    {
        return FALSE;
    }
    
    if((pwfx->wfx.nChannels < 1) || (pwfx->wfx.nChannels > IMAADPCM_MAX_CHANNELS))
    {
        return FALSE;
    }

    if(IMAADPCM_BITS_PER_SAMPLE != pwfx->wfx.wBitsPerSample)
    {
        return FALSE;
    }

    if(CalculateEncodeAlignment(pwfx->wfx.nChannels, pwfx->wSamplesPerBlock) != pwfx->wfx.nBlockAlign)
    {
        return FALSE;
    }

    return TRUE;
}


/****************************************************************************
 *
 * CalcDifference
 *
 * Description:
 *     Calculates the error between two audio buffers.  The error is clamped
 *     at (ULONGLONG)-1.  Also, the error of a block acts as a percentage of
 *     the maximum possible contribution of a block.
 *
 *  Arguments:
 *     LPBYTE   [in]:   First buffer
 *     LPBYTE   [in]:   Second buffer
 *     UINT     [in]:   Number of blocks-worth to compare
 *     UINT     [in]:   Total number of blocks being converted
 *     DWORD    [in]:   Size of a single block in bytes
 *
 *  Returns:  
 *     ULONGLONG: Difference of the two buffers
 *
 ****************************************************************************/
ULONGLONG CImaAdpcmCodec::CalcDifference(LPBYTE pvBuffer1, LPBYTE pvBuffer2, UINT cBlocks, UINT cTotalBlocks, DWORD dwBlockSize)
{
    ULONGLONG ullDiff = 0;

    // Each block worth of error can contribute a maximum of this value
    const ULONGLONG ullMaxBlockContribution = ( (ULONGLONG)-1 / cTotalBlocks );

    // The maximum error in a block is
    // (2^16)^2 * m_wfxEncode.wSamplesPerBlock
    //   = ( 1 << 32 ) * m_wfxEncode.wSamplesPerBlock
    const ULONGLONG ullMaxBlockDiff = ( (ULONGLONG)1 << 32 ) * m_wfxEncode.wSamplesPerBlock;

    // Now we go through the buffers sample by sample and find the difference
    // on a block-by-block basis.  The factored difference of a block is
    // ullBlockDiff / ullMaxBlockDiff * ullMaxBlockContribution
    for( UINT i = 0; i < cBlocks; ++i )
    {
        PSHORT pSamples1 = (PSHORT)(pvBuffer1 + i * dwBlockSize);
        PSHORT pSamples2 = (PSHORT)(pvBuffer2 + i * dwBlockSize);
        ULONGLONG ullBlockDiff = 0;

        // Find the block difference
        for( UINT j = 0; j < m_wfxEncode.wSamplesPerBlock; ++j )
        {
            ULONGLONG ullSampleDiff = (ULONGLONG)(pSamples2[j]) - (ULONGLONG)(pSamples1[j]);
            ullBlockDiff += ( ullSampleDiff * ullSampleDiff );
        }

        // Assert that we didn't go over the maximum possible
        assert( ullBlockDiff <= ullMaxBlockDiff );

        // Add the contribution of this block to the error
        ullDiff += (ULONGLONG)( ( (DOUBLE)ullBlockDiff / (DOUBLE)ullMaxBlockDiff ) * ullMaxBlockContribution );
    }

    assert( ullDiff <= cBlocks * ullMaxBlockContribution );

    return ullDiff;
}


/****************************************************************************
 *
 *  EncodeSample
 *
 *  Description:
 *      Encodes a sample.
 *
 *  Arguments:
 *      int [in]: the sample to be encoded.
 *      LPINT [in/out]: the predicted value of the sample.
 *      int [in]: the quantization step size used to encode the sample.
 *
 *  Returns:  
 *      int: the encoded ADPCM sample.
 *
 ****************************************************************************/

int
CImaAdpcmCodec::EncodeSample
(
    int                 nInputSample,
    LPINT               pnPredictedSample,
    int                 nStepSize
)
{
    int                 nPredictedSample;
    LONG                lDifference;
    int                 nEncodedSample;
    
    nPredictedSample = *pnPredictedSample;

    lDifference = nInputSample - nPredictedSample;
    nEncodedSample = 0;

    if(lDifference < 0) 
    {
        nEncodedSample = 8;
        lDifference = -lDifference;
    }

    if(lDifference >= nStepSize)
    {
        nEncodedSample |= 4;
        lDifference -= nStepSize;
    }

    nStepSize >>= 1;

    if(lDifference >= nStepSize)
    {
        nEncodedSample |= 2;
        lDifference -= nStepSize;
    }

    nStepSize >>= 1;

    if(lDifference >= nStepSize)
    {
        nEncodedSample |= 1;
        lDifference -= nStepSize;
    }

    if(nEncodedSample & 8)
    {
        nPredictedSample = nInputSample + lDifference - (nStepSize >> 1);
    }
    else
    {
        nPredictedSample = nInputSample - lDifference + (nStepSize >> 1);
    }

    if(nPredictedSample > 32767)
    {
        nPredictedSample = 32767;
    }
    else if(nPredictedSample < -32768)
    {
        nPredictedSample = -32768;
    }

    *pnPredictedSample = nPredictedSample;
    
    return nEncodedSample;
}


/****************************************************************************
 *
 *  DecodeSample
 *
 *  Description:
 *      Decodes an encoded sample.
 *
 *  Arguments:
 *      int [in]: the sample to be decoded.
 *      int [in]: the predicted value of the sample.
 *      int [i]: the quantization step size used to encode the sample.
 *
 *  Returns:  
 *      int: the decoded PCM sample.
 *
 ****************************************************************************/

int
CImaAdpcmCodec::DecodeSample
(
    int                 nEncodedSample,
    int                 nPredictedSample,
    int                 nStepSize
)
{
    LONG                lDifference;
    LONG                lNewSample;

    lDifference = nStepSize >> 3;

    if(nEncodedSample & 4) 
    {
        lDifference += nStepSize;
    }

    if(nEncodedSample & 2) 
    {
        lDifference += nStepSize >> 1;
    }

    if(nEncodedSample & 1) 
    {
        lDifference += nStepSize >> 2;
    }

    if(nEncodedSample & 8)
    {
        lDifference = -lDifference;
    }

    lNewSample = nPredictedSample + lDifference;

    if((LONG)(short)lNewSample != lNewSample)
    {
        if(lNewSample < -32768)
        {
            lNewSample = -32768;
        }
        else
        {
            lNewSample = 32767;
        }
    }

    return (int)lNewSample;
}


/****************************************************************************
 *
 *  Conversion Routines
 *
 *  Description:
 *      Converts a PCM buffer to ADPCM, or the reverse.
 *
 *  Arguments:
 *      LPBYTE [in]: source buffer.
 *      LPBYTE [out]: destination buffer.
 *      UINT [in]: block count.
 *      UINT [in]: block alignment of the ADPCM data, in bytes.
 *      UINT [in]: the number of samples in each ADPCM block (not used in
 *                 decoding).
 *      LPINT [in/out]: left-channel stepping index.
 *      LPINT [in/out]: right-channel stepping index.
 *
 *  Returns:  
 *      BOOL: TRUE on success.
 *
 ****************************************************************************/

BOOL
CImaAdpcmCodec::EncodeM16
(
    LPBYTE                  pbSrc,
    LPBYTE                  pbDst,
    UINT                    cBlocks,
    UINT                    nBlockAlignment,
    UINT                    cSamplesPerBlock,
    LPINT                   pnStepIndexL,
    LPINT
)
{
    LPBYTE                  pbBlock;
    UINT                    cSamples;
    int                     nSample;
    int                     nStepSize;
    int                     nEncSample1;
    int                     nEncSample2;
    int                     nPredSample;
    int                     nStepIndex;

    //
    // Save a local copy of the step index so we're not constantly 
    // dereferencing a pointer.
    //
    
    nStepIndex = *pnStepIndexL;

    //
    // Enter the main loop
    //
    
    while(cBlocks--)
    {
        pbBlock = pbDst;
        cSamples = cSamplesPerBlock - 1;

        //
        // Block header
        //

        nPredSample = *(short *)pbSrc;
        pbSrc += sizeof(short);

        *(LONG *)pbBlock = MAKELONG(nPredSample, nStepIndex);
        pbBlock += sizeof(LONG);

        //
        // We have written the header for this block--now write the data
        // chunk (which consists of a bunch of encoded nibbles).  Note
        // that if we don't have enough data to fill a complete byte, then
        // we add a 0 nibble on the end.
        //

        while(cSamples)
        {
            //
            // Sample 1
            //

            nSample = *(short *)pbSrc;
            pbSrc += sizeof(short);
            cSamples--;

            nStepSize = m_asStep[nStepIndex];
            nEncSample1 = EncodeSample(nSample, &nPredSample, nStepSize);
            nStepIndex = NextStepIndex(nEncSample1, nStepIndex);

            //
            // Sample 2
            //

            if(cSamples)
            {
                nSample = *(short *)pbSrc;
                pbSrc += sizeof(short);
                cSamples--;

                nStepSize = m_asStep[nStepIndex];
                nEncSample2 = EncodeSample(nSample, &nPredSample, nStepSize);
                nStepIndex = NextStepIndex(nEncSample2, nStepIndex);
            }
            else
            {
                nEncSample2 = 0;
            }

            //
            // Write out encoded byte.
            //

            *pbBlock++ = (BYTE)(nEncSample1 | (nEncSample2 << 4));
        }

        //
        // Skip padding
        //

        pbDst += nBlockAlignment;
    }

    //
    // Restore the value of the step index to be used on the next buffer.
    //

    *pnStepIndexL = nStepIndex;

    return TRUE;
}


BOOL
CImaAdpcmCodec::EncodeS16
(
    LPBYTE                  pbSrc,
    LPBYTE                  pbDst,
    UINT                    cBlocks,
    UINT                    nBlockAlignment,
    UINT                    cSamplesPerBlock,
    LPINT                   pnStepIndexL,
    LPINT                   pnStepIndexR
)
{
    LPBYTE                  pbBlock;
    UINT                    cSamples;
    UINT                    cSubSamples;
    int                     nSample;
    int                     nStepSize;
    DWORD                   dwLeft;
    DWORD                   dwRight;
    int                     nEncSampleL;
    int                     nPredSampleL;
    int                     nStepIndexL;
    int                     nEncSampleR;
    int                     nPredSampleR;
    int                     nStepIndexR;
    UINT                    i;

    //
    // Save a local copy of the step indices so we're not constantly 
    // dereferencing a pointer.
    //
    
    nStepIndexL = *pnStepIndexL;
    nStepIndexR = *pnStepIndexR;

    //
    // Enter the main loop
    //
    
    while(cBlocks--)
    {
        pbBlock = pbDst;
        cSamples = cSamplesPerBlock - 1;

        //
        // LEFT channel block header
        //

        nPredSampleL = *(short *)pbSrc;
        pbSrc += sizeof(short);

        *(LONG *)pbBlock = MAKELONG(nPredSampleL, nStepIndexL);
        pbBlock += sizeof(LONG);

        //
        // RIGHT channel block header
        //

        nPredSampleR = *(short *)pbSrc;
        pbSrc += sizeof(short);

        *(LONG *)pbBlock = MAKELONG(nPredSampleR, nStepIndexR);
        pbBlock += sizeof(LONG);

        //
        // We have written the header for this block--now write the data
        // chunk.  This consists of 8 left samples (one DWORD of output)
        // followed by 8 right samples (also one DWORD).  Since the input
        // samples are interleaved, we create the left and right DWORDs
        // sample by sample, and then write them both out.
        //

        while(cSamples)
        {
            dwLeft = 0;
            dwRight = 0;

            cSubSamples = min(cSamples, 8);

            for(i = 0; i < cSubSamples; i++)
            {
                //
                // LEFT channel
                //

                nSample = *(short *)pbSrc;
                pbSrc += sizeof(short);

                nStepSize = m_asStep[nStepIndexL];
                
                nEncSampleL = EncodeSample(nSample, &nPredSampleL, nStepSize);

                nStepIndexL = NextStepIndex(nEncSampleL, nStepIndexL);
                dwLeft |= (DWORD)nEncSampleL << (4 * i);

                //
                // RIGHT channel
                //

                nSample = *(short *)pbSrc;
                pbSrc += sizeof(short);

                nStepSize = m_asStep[nStepIndexR];
                
                nEncSampleR = EncodeSample(nSample, &nPredSampleR, nStepSize);

                nStepIndexR = NextStepIndex(nEncSampleR, nStepIndexR);
                dwRight |= (DWORD)nEncSampleR << (4 * i);
            }

            //
            // Write out encoded DWORDs.
            //

            *(LPDWORD)pbBlock = dwLeft;
            pbBlock += sizeof(DWORD);

            *(LPDWORD)pbBlock = dwRight;
            pbBlock += sizeof(DWORD);

            cSamples -= cSubSamples;
        }

        //
        // Skip padding
        //

        pbDst += nBlockAlignment;
    }

    //
    // Restore the value of the step index to be used on the next buffer.
    //
    
    *pnStepIndexL = nStepIndexL;
    *pnStepIndexR = nStepIndexR;

    return TRUE;

}


BOOL
CImaAdpcmCodec::DecodeM16   
(
    LPBYTE                  pbSrc,
    LPBYTE                  pbDst,
    UINT                    cBlocks,
    UINT                    nBlockAlignment,
    UINT                    cSamplesPerBlock,
    LPINT,
    LPINT
)
{
    BOOL                    fSuccess    = TRUE;
    LPBYTE                  pbBlock;
    UINT                    cSamples;
    BYTE                    bSample;
    int                     nStepSize;
    int                     nEncSample;
    int                     nPredSample;
    int                     nStepIndex;
    DWORD                   dwHeader;

    //
    // Enter the main loop
    //
    
    while(cBlocks--)
    {
        pbBlock = pbSrc;
        cSamples = cSamplesPerBlock - 1;
        
        //
        // Block header
        //

        dwHeader = *(LPDWORD)pbBlock;
        pbBlock += sizeof(DWORD);

        nPredSample = (int)(short)LOWORD(dwHeader);
        nStepIndex = (int)(BYTE)HIWORD(dwHeader);

        if(!ValidStepIndex(nStepIndex))
        {
            //
            // The step index is out of range - this is considered a fatal
            // error as the input stream is corrupted.  We fail by returning
            // zero bytes converted.
            //

            fSuccess = FALSE;
            break;
        }
        
        //
        // Write out first sample
        //

        *(short *)pbDst = (short)nPredSample;
        pbDst += sizeof(short);

        //
        // Enter the block loop
        //

        while(cSamples)
        {
            bSample = *pbBlock++;

            //
            // Sample 1
            //

            nEncSample = (bSample & (BYTE)0x0F);
            nStepSize = m_asStep[nStepIndex];
            nPredSample = DecodeSample(nEncSample, nPredSample, nStepSize);
            nStepIndex = NextStepIndex(nEncSample, nStepIndex);

            *(short *)pbDst = (short)nPredSample;
            pbDst += sizeof(short);

            cSamples--;

            //
            // Sample 2
            //

            if(cSamples)
            {
                nEncSample = (bSample >> 4);
                nStepSize = m_asStep[nStepIndex];
                nPredSample = DecodeSample(nEncSample, nPredSample, nStepSize);
                nStepIndex = NextStepIndex(nEncSample, nStepIndex);

                *(short *)pbDst = (short)nPredSample;
                pbDst += sizeof(short);

                cSamples--;
            }
        }

        //
        // Skip padding
        //

        pbSrc += nBlockAlignment;
    }

    return fSuccess;
}


BOOL
CImaAdpcmCodec::DecodeS16
(
    LPBYTE                  pbSrc,
    LPBYTE                  pbDst,
    UINT                    cBlocks,
    UINT                    nBlockAlignment,
    UINT                    cSamplesPerBlock,
    LPINT,
    LPINT
)
{
    BOOL                    fSuccess    = TRUE;
    LPBYTE                  pbBlock;
    UINT                    cSamples;
    UINT                    cSubSamples;
    int                     nStepSize;
    DWORD                   dwHeader;
    DWORD                   dwLeft;
    DWORD                   dwRight;
    int                     nEncSampleL;
    int                     nPredSampleL;
    int                     nStepIndexL;
    int                     nEncSampleR;
    int                     nPredSampleR;
    int                     nStepIndexR;
    UINT                    i;

    //
    // Enter the main loop
    //
    
    while(cBlocks--)
    {
        pbBlock = pbSrc;
        cSamples = cSamplesPerBlock - 1;

        //
        // LEFT channel header
        //

        dwHeader = *(LPDWORD)pbBlock;
        pbBlock += sizeof(DWORD);
        
        nPredSampleL = (int)(short)LOWORD(dwHeader);
        nStepIndexL = (int)(BYTE)HIWORD(dwHeader);

        if(!ValidStepIndex(nStepIndexL)) 
        {
            //
            // The step index is out of range - this is considered a fatal
            // error as the input stream is corrupted.  We fail by returning
            // zero bytes converted.
            //

            fSuccess = FALSE;
            break;
        }
        
        //
        // RIGHT channel header
        //

        dwHeader = *(LPDWORD)pbBlock;
        pbBlock += sizeof(DWORD);
        
        nPredSampleR = (int)(short)LOWORD(dwHeader);
        nStepIndexR = (int)(BYTE)HIWORD(dwHeader);

        if(!ValidStepIndex(nStepIndexR))
        {
            //
            // The step index is out of range - this is considered a fatal
            // error as the input stream is corrupted.  We fail by returning
            // zero bytes converted.
            //

            fSuccess = FALSE;
            break;
        }

        //
        // Write out first sample
        //

        *(LPDWORD)pbDst = MAKELONG(nPredSampleL, nPredSampleR);
        pbDst += sizeof(DWORD);

        //
        // The first DWORD contains 4 left samples, the second DWORD
        // contains 4 right samples.  We process the source in 8-byte
        // chunks to make it easy to interleave the output correctly.
        //

        while(cSamples)
        {
            dwLeft = *(LPDWORD)pbBlock;
            pbBlock += sizeof(DWORD);
            dwRight = *(LPDWORD)pbBlock;
            pbBlock += sizeof(DWORD);

            cSubSamples = min(cSamples, 8);
            
            for(i = 0; i < cSubSamples; i++)
            {
                //
                // LEFT channel
                //

                nEncSampleL = (dwLeft & 0x0F);
                nStepSize = m_asStep[nStepIndexL];
                nPredSampleL = DecodeSample(nEncSampleL, nPredSampleL, nStepSize);
                nStepIndexL = NextStepIndex(nEncSampleL, nStepIndexL);

                //
                // RIGHT channel
                //

                nEncSampleR = (dwRight & 0x0F);
                nStepSize = m_asStep[nStepIndexR];
                nPredSampleR = DecodeSample(nEncSampleR, nPredSampleR, nStepSize);
                nStepIndexR = NextStepIndex(nEncSampleR, nStepIndexR);

                //
                // Write out sample
                //

                *(LPDWORD)pbDst = MAKELONG(nPredSampleL, nPredSampleR);
                pbDst += sizeof(DWORD);

                //
                // Shift the next input sample into the low-order 4 bits.
                //

                dwLeft >>= 4;
                dwRight >>= 4;
            }

            cSamples -= cSubSamples;
        }

        //
        // Skip padding
        //

        pbSrc += nBlockAlignment;
    }

    return fSuccess;
}


int XboxADPCMSize( int sampleCount, int channelCount, int sampleRate )
{
	CImaAdpcmCodec codec;
	IMAADPCMWAVEFORMAT wfxEncode;

	// Create an APDCM format structure based off the source format
	codec.CreateImaAdpcmFormat( (WORD)channelCount, sampleRate, XBOX_ADPCM_SAMPLES_PER_BLOCK, &wfxEncode );

	// Calculate number of ADPCM blocks and length of ADPCM data
	DWORD dwDestBlocks  = sampleCount / XBOX_ADPCM_SAMPLES_PER_BLOCK;
	DWORD dwDestLength  = dwDestBlocks * wfxEncode.wfx.nBlockAlign;

	return dwDestLength;
}

void Convert16ToXboxADPCM( const short *pInputBuffer, byte *pOutputBuffer, byte *pOutFormat, int sampleCount, int channelCount, int sampleRate )
{
	CImaAdpcmCodec codec;
	IMAADPCMWAVEFORMAT wfxEncode;

	// Create an APDCM format structure based off the source format
	codec.CreateImaAdpcmFormat( (WORD)channelCount, sampleRate, XBOX_ADPCM_SAMPLES_PER_BLOCK, &wfxEncode );
	if ( pOutFormat )
	{
		memcpy( pOutFormat, &wfxEncode, sizeof(wfxEncode) );
	}

	// Initialize the codec
	if ( FALSE == codec.Initialize( &wfxEncode, CODEC_MODE_ENCODE_OPTIMIZE_EACH_BLOCK ) )
	{
		printf( "Couldn't initialize codec.\n" );
		return;
	}

	// Convert the data
	DWORD dwDestBlocks  = sampleCount / XBOX_ADPCM_SAMPLES_PER_BLOCK;
	if ( FALSE == codec.Convert( (const byte *)pInputBuffer, pOutputBuffer, dwDestBlocks ) )
		return;
}
