//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef MPAHEADER_H
#define MPAHEADER_H
#ifdef _WIN32
#pragma once
#endif

#pragma once

#define MPA_HEADER_SIZE 4	// MPEG-Audio Header Size 32bit
#define MAXTIMESREAD 5

class CMPAFile;

class CMPAHeader
{
public:
	CMPAHeader( CMPAFile* pMPAFile, uint32 dwExpectedOffset = 0, bool bSubsequentFrame = false, bool bReverse = false );
	~CMPAHeader();

	bool SkipEmptyFrames();

	// bitrate is in bit per second, to calculate in bytes => (/ 8)
	uint32 GetBytesPerSecond() const { return m_dwBitrate / 8; };
	// calc number of seconds from number of frames
	uint32 GetLengthSecond(uint32 dwNumFrames) const { return dwNumFrames * m_dwSamplesPerFrame / m_dwSamplesPerSec; };
	uint32 GetBytesPerSecond( uint32 dwNumFrames, uint32 dwNumBytes ) const { return dwNumBytes / GetLengthSecond( dwNumFrames ); };
	bool IsMono() const { return (m_ChannelMode == SingleChannel)?true:false; };
	// true if MPEG2/2.5 otherwise false
	bool IsLSF() const { return m_bLSF;	};	

private:
	static const uint32 m_dwMaxRange;
	static const uint32 m_dwTolerance;
	static const uint32 m_dwSamplingRates[4][3];
	static const uint32 m_dwPaddingSizes[3];
	static const uint32 m_dwBitrates[2][3][15];
	static const uint32 m_dwSamplesPerFrames[2][3];
	static const uint32 m_dwCoefficients[2][3];

	// necessary for CRC check (not yet implemented)
	static const uint32 m_dwSideinfoSizes[2][3][2];
	static const uint16 wCRC16Table[256];

	bool m_bLSF;		// true means lower sampling frequencies (=MPEG2/MPEG2.5)
	CMPAFile* m_pMPAFile;

public:
	static const char * m_szLayers[];
	static const char * m_szMPEGVersions[];
	static const char * m_szChannelModes[];
	static const char * m_szEmphasis[];

	enum MPAVersion
	{
		MPEG25 = 0,
		MPEGReserved,
		MPEG2,
		MPEG1		
	}m_Version;

	enum MPALayer
	{
		Layer1 = 0,
		Layer2,
		Layer3,
		LayerReserved
	}m_Layer;

	enum Emphasis
	{
		EmphNone = 0,
		Emph5015,
		EmphReserved,
		EmphCCITJ17
	}m_Emphasis;

	enum ChannelMode
	{
		Stereo,
		JointStereo,
		DualChannel,
		SingleChannel
	}m_ChannelMode;
	
	uint32 m_dwSamplesPerSec;
	uint32 m_dwSamplesPerFrame;
	uint32 m_dwBitrate;	// in bit per second (1 kb = 1000 bit, not 1024)
	uint32 m_dwSyncOffset;
	uint32 m_dwComputedFrameSize, m_dwRealFrameSize;
	uint32 m_dwPaddingSize;

	// flags
	bool m_bCopyright, m_bPrivate, m_bOriginal;
	bool m_bCRC; 
	uint8 m_ModeExt;
	
private:
	enum HeaderError
	{
		noError,
		noSync,
		freeBitrate,
		headerCorrupt
	};

	HeaderError DecodeHeader( uint32 dwHeader, bool bSimpleDecode = false );
	inline HeaderError IsSync( uint32 dwOffset, bool bExtended );
};

#endif // MPAHEADER_H
