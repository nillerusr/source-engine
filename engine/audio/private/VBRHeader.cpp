//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "audio_pch.h"
#include "tier0/platform.h"
#include "MPAFile.h"	// also includes vbrheader.h
#include "tier0/dbg.h"

#ifndef MAKEFOURCC
    #define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32)(BYTE)(ch0) | ((uint32)(BYTE)(ch1) << 8) |   \
                ((uint32)(BYTE)(ch2) << 16) | ((uint32)(BYTE)(ch3) << 24 ))
#endif //defined(MAKEFOURCC)

// XING Header offset: 1. index = lsf, 2. index = mono
uint32 CVBRHeader::m_dwXINGOffsets[2][2] =
{
	// MPEG 1 (not mono, mono)
	{ 32 + MPA_HEADER_SIZE, 17 + MPA_HEADER_SIZE },
	// MPEG 2/2.5
	{  17 + MPA_HEADER_SIZE, 9 + MPA_HEADER_SIZE }
};

// first test with this static method, if it does exist
bool CVBRHeader::IsVBRHeaderAvailable( CMPAFile* pMPAFile, VBRHeaderType& HeaderType, uint32& dwOffset )
{
	Assert(pMPAFile);
	
	// where does VBR header begin (XING)
	uint32 dwNewOffset = dwOffset + m_dwXINGOffsets[pMPAFile->m_pMPAHeader->IsLSF()][pMPAFile->m_pMPAHeader->IsMono()];

	// check for XING header first
	if( CheckXING( pMPAFile, dwNewOffset ) )
	{
		HeaderType = XINGHeader;
		// seek offset back to header begin
		dwOffset = dwNewOffset - 4;
		return true;
	}

	// VBRI header always at fixed offset
	dwNewOffset = dwOffset + 32 + MPA_HEADER_SIZE;
	if( CheckVBRI( pMPAFile, dwNewOffset ) )
	{
		HeaderType = VBRIHeader;
		// seek offset back to header begin
		dwOffset = dwNewOffset - 4;
		return true;
	}
	HeaderType = NoHeader;
	return false;
}

CVBRHeader::CVBRHeader( CMPAFile* pMPAFile, VBRHeaderType HeaderType, uint32 dwOffset ) :
	m_pMPAFile( pMPAFile ), m_pnToc(NULL), m_HeaderType( HeaderType ), m_dwOffset(dwOffset), m_dwFrames(0), m_dwBytes(0)
{
	switch( m_HeaderType )
	{
		case NoHeader:
			// no Header found
			throw CMPAException( CMPAException::NoVBRHeader, pMPAFile->GetFilename(), NULL, false );
			break;
		case XINGHeader:
			if(	!ExtractXINGHeader( m_dwOffset ) )
				throw CMPAException( CMPAException::NoVBRHeader, pMPAFile->GetFilename(), NULL, false );
			break;
		case VBRIHeader:
			if( !ExtractVBRIHeader( m_dwOffset ) ) 
				throw CMPAException( CMPAException::NoVBRHeader, pMPAFile->GetFilename(), NULL, false );
			break;
	}
	// calc bitrate
	if( m_dwBytes > 0 && m_dwFrames > 0 )
	{
		// calc number of seconds
		m_dwBytesPerSec = m_pMPAFile->m_pMPAHeader->GetBytesPerSecond( m_dwFrames, m_dwBytes );
	}
	else	// incomplete header found
	{
		throw CMPAException( CMPAException::IncompleteVBRHeader, pMPAFile->GetFilename(), NULL, false );
	}
}

bool CVBRHeader::CheckID( CMPAFile* pMPAFile, char ch0, char ch1, char ch2, char ch3, uint32& dwOffset )
{
	return ( pMPAFile->ExtractBytes( dwOffset, 4 ) == MAKEFOURCC( ch3, ch2, ch1, ch0 ) );
}

bool CVBRHeader::CheckXING( CMPAFile* pMPAFile, uint32& dwOffset )
{
	// XING ID found?
	if( !CheckID( pMPAFile, 'X', 'i', 'n', 'g', dwOffset) && !CheckID( pMPAFile, 'I', 'n', 'f', 'o', dwOffset) )
		return false;
	return true;
}

bool CVBRHeader::CheckVBRI( CMPAFile* pMPAFile, uint32& dwOffset )
{
	// VBRI ID found?
	if( !CheckID( pMPAFile, 'V', 'B', 'R', 'I', dwOffset ) )
		return false;
	return true;
}


// currently not used
bool CVBRHeader::ExtractLAMETag( uint32 dwOffset )
{
	// LAME ID found?
	if( !CheckID( m_pMPAFile, 'L', 'A', 'M', 'E', dwOffset ) && !CheckID( m_pMPAFile, 'G', 'O', 'G', 'O', dwOffset ) )
		return false;

	return true;
}

bool CVBRHeader::ExtractXINGHeader( uint32 dwOffset )
{
	/* XING VBR-Header

	 size	description
	 4		'Xing' or 'Info'
	 4		flags (indicates which fields are used)
	 4		frames (optional)
	 4		bytes (optional)
	 100	toc (optional)
	 4		a VBR quality indicator: 0=best 100=worst (optional)
	
	*/
	if( !CheckXING( m_pMPAFile, dwOffset ) )
		return false;

	uint32 dwFlags;

	// get flags (mandatory in XING header)
	dwFlags = m_pMPAFile->ExtractBytes( dwOffset, 4 ); 

	// extract total number of frames in file
	if(dwFlags & FRAMES_FLAG)
		m_dwFrames = m_pMPAFile->ExtractBytes(dwOffset,4);

	// extract total number of bytes in file
	if(dwFlags & BYTES_FLAG) 
		m_dwBytes = m_pMPAFile->ExtractBytes(dwOffset,4);

	// extract TOC (for more accurate seeking)
	if (dwFlags & TOC_FLAG) 
	{
		m_dwTableSize = 100;
		m_pnToc = new int[m_dwTableSize];

		if( m_pnToc )
		{
			for(uint32 i=0;i<m_dwTableSize;i++)
				m_pnToc[i] = m_pMPAFile->ExtractBytes( dwOffset, 1 );
		}
	}

	m_dwQuality = (uint32)-1;
	if(dwFlags & VBR_SCALE_FLAG )
		m_dwQuality = m_pMPAFile->ExtractBytes(dwOffset, 4);
		
	return true;
}

bool CVBRHeader::ExtractVBRIHeader( uint32 dwOffset )
{
	/* FhG VBRI Header

	size	description
	4		'VBRI' (ID)
	2		version
	2		delay
	2		quality
	4		# bytes
	4		# frames
	2		table size (for TOC)
	2		table scale (for TOC)
	2		size of table entry (max. size = 4 byte (must be stored in an integer))
	2		frames per table entry
	
	??		dynamic table consisting out of frames with size 1-4
			whole length in table size! (for TOC)

	*/

	if( !CheckVBRI( m_pMPAFile, dwOffset ) )
		return false;

	// extract all fields from header (all mandatory)
	m_dwVersion = m_pMPAFile->ExtractBytes(dwOffset, 2 );
	m_fDelay = (float)m_pMPAFile->ExtractBytes(dwOffset, 2 );
	m_dwQuality = m_pMPAFile->ExtractBytes(dwOffset, 2 );
	m_dwBytes = m_pMPAFile->ExtractBytes(dwOffset, 4 );
	m_dwFrames = m_pMPAFile->ExtractBytes(dwOffset, 4 );
	m_dwTableSize = m_pMPAFile->ExtractBytes(dwOffset, 2 ) + 1;	//!!!
	m_dwTableScale = m_pMPAFile->ExtractBytes(dwOffset, 2 );
	m_dwBytesPerEntry = m_pMPAFile->ExtractBytes(dwOffset, 2 );
	m_dwFramesPerEntry = m_pMPAFile->ExtractBytes(dwOffset, 2 );

	// extract TOC  (for more accurate seeking)
	m_pnToc = new int[m_dwTableSize];
	if( m_pnToc )
	{
		for ( unsigned int i = 0 ; i < m_dwTableSize ; i++)
		{
			m_pnToc[i] = m_pMPAFile->ExtractBytes(dwOffset, m_dwBytesPerEntry );
		}
	}
	return true;
}

CVBRHeader::~CVBRHeader(void)
{
	if( m_pnToc )
		delete[] m_pnToc;
}

// get byte position for percentage value (fPercent) of file
bool CVBRHeader::SeekPoint(float fPercent, uint32& dwSeekPoint)
{
	if( !m_pnToc || m_dwBytes == 0 )
		return false;

	if( fPercent < 0.0f )   
		fPercent = 0.0f;
	if( fPercent > 100.0f ) 
		fPercent = 100.0f;

	switch( m_HeaderType )
	{
		case XINGHeader:
			dwSeekPoint = SeekPointXING( fPercent );
			break;
		case VBRIHeader:
			dwSeekPoint = SeekPointVBRI( fPercent );
			break;
	}
	return true;
}

uint32 CVBRHeader::SeekPointXING(float fPercent) const
{
	// interpolate in TOC to get file seek point in bytes
	int a;
	float fa, fb, fx;

	a = (int)fPercent;
	if( a > 99 ) a = 99;
	fa = (float)m_pnToc[a];
	
	if( a < 99 ) 
	{
		fb = (float)m_pnToc[a+1];
	}
	else 
	{
		fb = 256.0f;
	}

	fx = fa + (fb-fa)*(fPercent-a);

	uint32 dwSeekpoint = (int)((1.0f/256.0f)*fx*m_dwBytes); 
	return dwSeekpoint;
}

uint32 CVBRHeader::SeekPointVBRI(float fPercent) const
{
	return SeekPointByTimeVBRI( (fPercent/100.0f) * m_pMPAFile->m_pMPAHeader->GetLengthSecond( m_dwFrames ) * 1000.0f );
}

uint32 CVBRHeader::SeekPointByTimeVBRI(float fEntryTimeMS) const
{
	unsigned int i=0,  fraction = 0;
	uint32 dwSeekPoint = 0;

	float fLengthMS;
	float fLengthMSPerTOCEntry;
	float fAccumulatedTimeMS = 0.0f ;
	 
	fLengthMS = (float)m_pMPAFile->m_pMPAHeader->GetLengthSecond( m_dwFrames ) * 1000.0f ;
	fLengthMSPerTOCEntry = fLengthMS / (float)m_dwTableSize;
	 
	if ( fEntryTimeMS > fLengthMS ) 
		fEntryTimeMS = fLengthMS; 
	 
	while ( fAccumulatedTimeMS <= fEntryTimeMS )
	{
		dwSeekPoint += m_pnToc[i++];
		fAccumulatedTimeMS += fLengthMSPerTOCEntry;
	}
	  
	// Searched too far; correct result
	fraction = ( (int)(((( fAccumulatedTimeMS - fEntryTimeMS ) / fLengthMSPerTOCEntry ) 
				+ (1.0f/(2.0f*(float)m_dwFramesPerEntry))) * (float)m_dwFramesPerEntry));

	dwSeekPoint -= (uint32)((float)m_pnToc[i-1] * (float)(fraction) 
					/ (float)m_dwFramesPerEntry);

	return dwSeekPoint;
}

