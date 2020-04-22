//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#if defined( WIN32) && !defined( _X360 )
#include "winlite.h"
#endif
#include "tier0/platform.h"
#include "MPAFile.h"


// static variables
const char *CMPAHeader::m_szLayers[] = { "Layer I", "Layer II", "Layer III" };
const char *CMPAHeader::m_szMPEGVersions[] = {"MPEG 2.5", "", "MPEG 2", "MPEG 1" };
const char *CMPAHeader::m_szChannelModes[] = { "Stereo", "Joint Stereo", "Dual Channel", "Single Channel" };
const char *CMPAHeader::m_szEmphasis[] = { "None", "50/15ms", "", "CCIT J.17" };

// tolerance range, look at expected offset +/- m_dwTolerance for subsequent frames
const uint32 CMPAHeader::m_dwTolerance = 3;	// 3 bytes

// max. range where to look for frame sync
const uint32 CMPAHeader::m_dwMaxRange = ( 256 * 1024 );

// sampling rates in hertz: 1. index = MPEG Version ID, 2. index = sampling rate index
const uint32 CMPAHeader::m_dwSamplingRates[4][3] = 
{ 
	{11025, 12000, 8000,  },	// MPEG 2.5
	{0,     0,     0,     },	// reserved
	{22050, 24000, 16000, },	// MPEG 2
	{44100, 48000, 32000  }		// MPEG 1
};

// padding sizes in bytes for different layers: 1. index = layer
const uint32 CMPAHeader::m_dwPaddingSizes[3] =
{
	4,	// Layer1
	1,	// Layer2
	1	// Layer3
};

// bitrates: 1. index = LSF, 2. index = Layer, 3. index = bitrate index
const uint32 CMPAHeader::m_dwBitrates[2][3][15] =
{
	{	// MPEG 1
		{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},	// Layer1
		{0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},	// Layer2
		{0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,}	// Layer3
	},
	{	// MPEG 2, 2.5		
		{0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},		// Layer1
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},			// Layer2
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,}			// Layer3
	}
};

// Samples per Frame: 1. index = LSF, 2. index = Layer
const uint32 CMPAHeader::m_dwSamplesPerFrames[2][3] =
{
	{	// MPEG 1
		384,	// Layer1
		1152,	// Layer2	
		1152	// Layer3
	},
	{	// MPEG 2, 2.5
		384,		// Layer1
		1152,	// Layer2
		576		// Layer3
	}	
};

// Samples per Frame / 8
const uint32 CMPAHeader::m_dwCoefficients[2][3] =
{
	{	// MPEG 1
		48,		// Layer1
		144,	// Layer2
		144		// Layer3
	},
	{	// MPEG 2, 2.5
		48,		// Layer1
		144,	// Layer2
		72		// Layer3
	}	
};

// needed later for CRC check
// sideinformation size: 1.index = lsf, 2. index = layer, 3. index = mono
const uint32 CMPAHeader::m_dwSideinfoSizes[2][3][2] =
{
	{	// MPEG 1 (not mono, mono
		{0,0},	// Layer1
		{0,0},	// Layer2
		{9,17}	// Layer3
	},
	{	// MPEG 2, 2.5
		{0,0},	// Layer1
		{0,0},	// Layer2
		{17,32}	// Layer3
	}	
};

// constructor (throws exception if no frame found)
CMPAHeader::CMPAHeader( CMPAFile* pMPAFile, uint32 dwExpectedOffset, bool bSubsequentFrame, bool bReverse ) :
m_pMPAFile( pMPAFile ), m_dwSyncOffset( dwExpectedOffset ), m_dwRealFrameSize( 0 ) 
{
	// first check at expected offset (extended for not subsequent frames)
	HeaderError error = IsSync( m_dwSyncOffset, !bSubsequentFrame );
	int nStep=1;
	int nSyncOffset;

	while( error != noError )
	{
		// either look in tolerance range
		if( bSubsequentFrame )
		{
			if( nStep > m_dwTolerance )
			{
				// out of tolerance range
				throw CMPAException( CMPAException::NoFrameInTolerance, pMPAFile->GetFilename() ? pMPAFile->GetFilename() : "??" );
			}

			// look around dwExpectedOffset with increasing steps (+1,-1,+2,-2,...)
			if( m_dwSyncOffset <= dwExpectedOffset )
			{
				nSyncOffset = dwExpectedOffset + nStep;
			}
			else
			{
				nSyncOffset = dwExpectedOffset - nStep++;
			}
		}
		// just go forward/backward to find sync
		else
		{
			nSyncOffset = ((int)m_dwSyncOffset) + (bReverse?-1:+1);
		}

		// is new offset within valid range?
		if( nSyncOffset < 0 || nSyncOffset > (int)((pMPAFile->m_dwEnd - pMPAFile->m_dwBegin) - MPA_HEADER_SIZE) || abs( (long)(nSyncOffset-dwExpectedOffset) ) > m_dwMaxRange )
		{
			// out of tolerance range
			throw CMPAException( CMPAException::NoFrame, pMPAFile->GetFilename() ? pMPAFile->GetFilename() : "??" );
		}
		m_dwSyncOffset = nSyncOffset;

		// found sync?
		error = IsSync( m_dwSyncOffset, !bSubsequentFrame );
	}
}

// destructor
CMPAHeader::~CMPAHeader()
{
}

// skips first 32kbit/s or lower bitrate frames to estimate bitrate (returns true if bitrate is variable)
bool CMPAHeader::SkipEmptyFrames()
{
	if( m_dwBitrate > 32 )
		return false;

	uint32 dwHeader;
	try
	{
		while( m_dwBitrate <= 32 )
		{
			m_dwSyncOffset += m_dwComputedFrameSize + MPA_HEADER_SIZE;
			dwHeader = m_pMPAFile->ExtractBytes( m_dwSyncOffset, MPA_HEADER_SIZE, false );
			
			if( IsSync( dwHeader, false ) != noError ) 
				return false;
		}
	}
	catch(CMPAException& /*Exc*/) // just catch the exception and return false
	{
		return false;
	}
	return true;
}

// in dwHeader stands 32bit header in big-endian format: frame sync at the end!
// because shifts do only work for integral types!!!
CMPAHeader::HeaderError CMPAHeader::DecodeHeader( uint32 dwHeader, bool bSimpleDecode )
{
	// Check SYNC bits (last eleven bits set)
	if( (dwHeader >> 24 != 0xff) || ((((dwHeader >> 16))&0xe0) != 0xe0) )
		return noSync;

	// get MPEG version
	m_Version = (MPAVersion)((dwHeader >> 19) & 0x03);	// mask only the rightmost 2 bits
	if( m_Version == MPEGReserved )
		return headerCorrupt;

	if( m_Version == MPEG1 )
		m_bLSF = false;
	else
		m_bLSF = true;

	// get layer (0 = layer1, 2 = layer2, ...)
	m_Layer = (MPALayer)(3 - ((dwHeader >> 17) & 0x03));	
	if( m_Layer == LayerReserved )
		return headerCorrupt;

	// protection bit (inverted)
	m_bCRC = !((dwHeader >> 16) & 0x01);

	// bitrate
	BYTE bIndex = (BYTE)((dwHeader >> 12) & 0x0F);
	if( bIndex == 0x0F )		// all bits set is reserved
		return headerCorrupt;
	m_dwBitrate = m_dwBitrates[m_bLSF][m_Layer][bIndex] * 1000; // convert from kbit to bit

	if( m_dwBitrate == 0 )	// means free bitrate (is unsupported yet)
		return freeBitrate;	

	// sampling rate
	bIndex = (BYTE)((dwHeader >> 10) & 0x03);
	if( bIndex == 0x03 )		// all bits set is reserved
		return headerCorrupt;
	m_dwSamplesPerSec = m_dwSamplingRates[m_Version][bIndex];

	// padding bit
	m_dwPaddingSize = m_dwPaddingSizes[m_Layer] * ((dwHeader >> 9) & 0x01);

	// calculate frame size
	m_dwComputedFrameSize = (m_dwCoefficients[m_bLSF][m_Layer] * m_dwBitrate / m_dwSamplesPerSec) + m_dwPaddingSize;
	m_dwSamplesPerFrame = m_dwSamplesPerFrames[m_bLSF][m_Layer];

	if( !bSimpleDecode )
	{
		// private bit
		m_bPrivate = (dwHeader >> 8) & 0x01;

		// channel mode
		m_ChannelMode = (ChannelMode)((dwHeader >> 6) & 0x03);

		// mode extension (currently not used)
		m_ModeExt = (BYTE)((dwHeader >> 4) & 0x03);

		// copyright bit
		m_bCopyright = (dwHeader >> 3) & 0x01;
		
		// original bit
		m_bCopyright = (dwHeader >> 2) & 0x01;

		// emphasis
		m_Emphasis = (Emphasis)(dwHeader & 0x03);
		if( m_Emphasis == EmphReserved )
			return headerCorrupt;
	}
	return noError;
}

CMPAHeader::HeaderError CMPAHeader::IsSync( uint32 dwOffset,  bool bExtended  )
{
	HeaderError error = noSync;
	uint32 dwHeader = m_pMPAFile->ExtractBytes( dwOffset, MPA_HEADER_SIZE, false );
		
	// sync bytes found?
	if( (dwHeader & 0xFFE00000) ==  0xFFE00000 )
	{
		error = DecodeHeader( dwHeader );
		if( error == noError )
		{	
			// enough buffer to do extended check?
			if( bExtended )
			{	
				// recursive call (offset for next frame header)
				dwOffset = m_dwSyncOffset+m_dwComputedFrameSize;
				try
				{
					CMPAHeader m_SubsequentFrame( m_pMPAFile, dwOffset, true );	
					m_dwRealFrameSize = m_SubsequentFrame.m_dwSyncOffset - m_dwSyncOffset;
				}
				catch( CMPAException& Exc )
				{
					// could not find any subsequent frame, assume it is the last frame
					if( Exc.GetErrorID() == CMPAException::NoFrame )
					{
						if( dwOffset + m_pMPAFile->m_dwBegin > m_pMPAFile->m_dwEnd )
							m_dwRealFrameSize = m_pMPAFile->m_dwEnd - m_pMPAFile->m_dwBegin - m_dwSyncOffset;
						else
							m_dwRealFrameSize = m_dwComputedFrameSize;
						error = noError;		
					}
					else
						error = noSync;
				}	
			}
		}
	}
	return error;
}

// CRC-16 lookup table
const uint16 CMPAHeader::wCRC16Table[256] =
{
 0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
 0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
 0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
 0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
 0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
 0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
 0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
 0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
 0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
 0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
 0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
 0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
 0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
 0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
 0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
 0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
 0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
 0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
 0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
 0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
 0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
 0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
 0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
 0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
 0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
 0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
 0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
 0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
 0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
 0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
 0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
 0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};
