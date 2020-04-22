//========= Copyright Valve Corporation, All rights reserved. ============//
/***************************************************************************
 *
 *  Copyright (C) 2001 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       imaadpcm.h
 *  Content:    IMA ADPCM CODEC.
 *  History:
 *   Date       By      Reason
 *   ====       ==      ======
 *  04/29/01    dereks  Created.
 *
 ****************************************************************************/

#ifndef __IMAADPCM_H__
#define __IMAADPCM_H__

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <ctype.h>
#include <mmreg.h>
#include <msacm.h>

#define XBOX_ADPCM_SAMPLES_PER_BLOCK    64

#define WAVE_FORMAT_XBOX_ADPCM          0x0069
                                        
#define IMAADPCM_BITS_PER_SAMPLE        4
#define IMAADPCM_HEADER_LENGTH          4
                                        
#define IMAADPCM_MAX_CHANNELS           2
#define IMAADPCM_PCM_BITS_PER_SAMPLE    16

#define NUMELMS(a) (sizeof(a) / sizeof(a[0]))

typedef const WAVEFORMATEX *LPCWAVEFORMATEX;
typedef const IMAADPCMWAVEFORMAT *LPCIMAADPCMWAVEFORMAT;

#ifdef __cplusplus

//
// IMA ADPCM encoder function prototype
//

typedef BOOL (*LPFNIMAADPCMCONVERT)(LPBYTE pbSrc, LPBYTE pbDst, UINT cBlocks, UINT nBlockAlignment, UINT cSamplesPerBlock, LPINT pnStepIndexL, LPINT pnStepIndexR);

//
// Codec mode
//

enum CODEC_MODE
{
    CODEC_MODE_DECODE,
    CODEC_MODE_ENCODE_NORMAL,
    CODEC_MODE_ENCODE_OPTIMIZE_WHOLE_FILE,
    CODEC_MODE_ENCODE_OPTIMIZE_EACH_BLOCK,
};

//
// IMA ADPCM CODEC
//

class CImaAdpcmCodec
{
private:
    static const short      m_asNextStep[16];           // Step increment array
    static const short      m_asStep[89];               // Step value array
    IMAADPCMWAVEFORMAT      m_wfxEncode;                // Encoded format description
    CODEC_MODE              m_cmCodecMode;              // Codec mode
    int                     m_nStepIndexL;              // Left-channel stepping index
    int                     m_nStepIndexR;              // Right-channel stepping index
    LPFNIMAADPCMCONVERT     m_pfnConvert;               // Conversion function

public:
    CImaAdpcmCodec(void);
    ~CImaAdpcmCodec(void);

public:
    // Initialization
    BOOL Initialize(LPCIMAADPCMWAVEFORMAT pwfxEncode, CODEC_MODE cmCodecMode);

    // Size conversions
    WORD GetEncodeAlignment(void);
    WORD GetDecodeAlignment(void);
    WORD GetSourceAlignment(void);
    WORD GetDestinationAlignment(void);

    // Data conversions
    BOOL Convert(LPCVOID pvSrc, LPVOID pvDst, UINT cBlocks);
    void Reset(void);

    // Format descriptions
    static void CreatePcmFormat(WORD nChannels, DWORD nSamplesPerSec, LPWAVEFORMATEX pwfxFormat);
    static void CreateImaAdpcmFormat(WORD nChannels, DWORD nSamplesPerSec, WORD nSamplesPerBlock, LPIMAADPCMWAVEFORMAT pwfxFormat);

    static BOOL IsValidPcmFormat(LPCWAVEFORMATEX pwfxFormat);
    static BOOL IsValidImaAdpcmFormat(LPCIMAADPCMWAVEFORMAT pwfxFormat);

private:
    // En/decoded data alignment
    static WORD CalculateEncodeAlignment(WORD nSamplesPerBlock, WORD nChannels);
    
    // Data conversion functions
    static BOOL EncodeM16(LPBYTE pbSrc, LPBYTE pbDst, UINT cBlocks, UINT nBlockAlignment, UINT cSamplesPerBlock, LPINT pnStepIndexL, LPINT pnStepIndexR);
    static BOOL EncodeS16(LPBYTE pbSrc, LPBYTE pbDst, UINT cBlocks, UINT nBlockAlignment, UINT cSamplesPerBlock, LPINT pnStepIndexL, LPINT pnStepIndexR);
    static BOOL DecodeM16(LPBYTE pbSrc, LPBYTE pbDst, UINT cBlocks, UINT nBlockAlignment, UINT cSamplesPerBlock, LPINT pnStepIndexL, LPINT pnStepIndexR);
    static BOOL DecodeS16(LPBYTE pbSrc, LPBYTE pbDst, UINT cBlocks, UINT nBlockAlignment, UINT cSamplesPerBlock, LPINT pnStepIndexL, LPINT pnStepIndexR);

    static int EncodeSample(int nInputSample, int *nPredictedSample, int nStepSize);
    static int DecodeSample(int nInputSample, int nPredictedSample, int nStepSize);

    static int NextStepIndex(int nEncodedSample, int nStepIndex);
    static BOOL ValidStepIndex(int nStepIndex);

    /*static ULONGLONG CalcDifference(LPDWORD pvBuffer1, LPDWORD pvBuffer2, DWORD dwLength);*/
    ULONGLONG CalcDifference(LPBYTE pvBuffer1, LPBYTE pvBuffer2, UINT cBlocks, UINT cTotalBlocks, DWORD dwBlockSize);
};

__inline WORD CImaAdpcmCodec::GetSourceAlignment(void)
{
    return ( m_cmCodecMode == CODEC_MODE_DECODE ) ? GetEncodeAlignment() : GetDecodeAlignment();
}

__inline WORD CImaAdpcmCodec::GetDestinationAlignment(void)
{
    return ( m_cmCodecMode == CODEC_MODE_DECODE ) ? GetDecodeAlignment() : GetEncodeAlignment();
}

__inline int CImaAdpcmCodec::NextStepIndex(int nEncodedSample, int nStepIndex)
{
    nStepIndex += m_asNextStep[nEncodedSample];

    if(nStepIndex < 0)
    {
        nStepIndex = 0;
    }
    else if(nStepIndex >= NUMELMS(m_asStep))
    {
        nStepIndex = NUMELMS(m_asStep) - 1;
    }

    return nStepIndex;
}

__inline BOOL CImaAdpcmCodec::ValidStepIndex(int nStepIndex)
{
    return (nStepIndex >= 0) && (nStepIndex < NUMELMS(m_asStep));
}

#endif // __cplusplus

#endif // __IMAADPCM_H__
