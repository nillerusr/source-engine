//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: sheet code for particles and other sprite functions
//
//===========================================================================//

#include "psheet.h"
#include "tier1/UtlStringMap.h"
#include "tier1/utlbuffer.h"
#include "tier2/fileutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CSheet::CSheet( void )
{
	memset( m_pSamples, 0, sizeof( m_pSamples ) );
	memset( m_bClamp, 0, sizeof( m_bClamp ) );
}

CSheet::CSheet( CUtlBuffer &buf )
{
	memset( m_pSamples, 0, sizeof( m_pSamples ) );
	memset( m_bClamp, 0, sizeof( m_bClamp ) );
	memset( m_bSequenceIsCopyOfAnotherSequence, 0, sizeof( m_bSequenceIsCopyOfAnotherSequence ) );

	// lets read a sheet
	buf.ActivateByteSwappingIfBigEndian();
	int nVersion = buf.GetInt();								// version#
	int nNumCoordsPerFrame = (nVersion)?MAX_IMAGES_PER_FRAME_ON_DISK:1;

	int nNumSequences = buf.GetInt();


	while ( nNumSequences-- )
	{
		int nSequenceNumber = buf.GetInt();
		if ( ( nSequenceNumber < 0 ) || (nSequenceNumber >= MAX_SEQUENCES ) )
		{
			Warning("sequence number %d too high in sheet file!!!\n", nSequenceNumber);
			return;
		}
		m_bClamp[ nSequenceNumber ] = ( buf.GetInt() != 0 );
		int nFrameCount = buf.GetInt();
		// Save off how many frames we have for this sequence
		m_nNumFrames[ nSequenceNumber ] = nFrameCount;
		bool bSingleFrameSequence = ( nFrameCount == 1 );

		int nTimeSamples = bSingleFrameSequence ? 1 : SEQUENCE_SAMPLE_COUNT;

		m_pSamples[ nSequenceNumber ] = 
			new SheetSequenceSample_t[ nTimeSamples ];

		int fTotalSequenceTime = buf.GetFloat();
		float InterpKnot[SEQUENCE_SAMPLE_COUNT];
		float InterpValue[SEQUENCE_SAMPLE_COUNT];
		SheetSequenceSample_t Samples[SEQUENCE_SAMPLE_COUNT];
		float fCurTime = 0.;
		for( int nFrm = 0 ; nFrm < nFrameCount; nFrm++ )
		{
			float fThisDuration = buf.GetFloat();
			InterpValue[ nFrm ] = nFrm;
			InterpKnot [ nFrm ] = SEQUENCE_SAMPLE_COUNT*( fCurTime/ fTotalSequenceTime );
			SheetSequenceSample_t &seq = Samples[ nFrm ];
			seq.m_fBlendFactor = 0.0f;
			for(int nImage = 0 ; nImage< nNumCoordsPerFrame; nImage++ )
			{
				SequenceSampleTextureCoords_t &s=seq.m_TextureCoordData[nImage];
				s.m_fLeft_U0	= buf.GetFloat();
				s.m_fTop_V0		= buf.GetFloat();
				s.m_fRight_U0	= buf.GetFloat();
				s.m_fBottom_V0	= buf.GetFloat();
			}
			if ( nNumCoordsPerFrame == 1 )
				seq.CopyFirstFrameToOthers();
			fCurTime += fThisDuration;
			m_flFrameSpan[nSequenceNumber] = fCurTime;
		}
		// now, fill in the whole table
		for( int nIdx = 0; nIdx < nTimeSamples; nIdx++ )
		{
			float flIdxA, flIdxB, flInterp;
			GetInterpolationData( InterpKnot, InterpValue, nFrameCount,
								  SEQUENCE_SAMPLE_COUNT,
								  nIdx, 
								  ! ( m_bClamp[nSequenceNumber] ),
								  &flIdxA, &flIdxB, &flInterp );
			SheetSequenceSample_t sA = Samples[(int) flIdxA];
			SheetSequenceSample_t sB = Samples[(int) flIdxB];
			SheetSequenceSample_t &oseq = m_pSamples[nSequenceNumber][nIdx];

			oseq.m_fBlendFactor = flInterp;
			for(int nImage = 0 ; nImage< MAX_IMAGES_PER_FRAME_IN_MEMORY; nImage++ )
			{
				SequenceSampleTextureCoords_t &src0=sA.m_TextureCoordData[nImage];
				SequenceSampleTextureCoords_t &src1=sB.m_TextureCoordData[nImage];
				SequenceSampleTextureCoords_t &o=oseq.m_TextureCoordData[nImage];
				o.m_fLeft_U0 = src0.m_fLeft_U0;
				o.m_fTop_V0	= src0.m_fTop_V0;
				o.m_fRight_U0 = src0.m_fRight_U0;
				o.m_fBottom_V0 = src0.m_fBottom_V0;
				o.m_fLeft_U1 = src1.m_fLeft_U0;
				o.m_fTop_V1	= src1.m_fTop_V0;
				o.m_fRight_U1 = src1.m_fRight_U0;
				o.m_fBottom_V1 = src1.m_fBottom_V0;
			}
		}
	}
	// now, fill in all unseen sequences with copies of the first seen sequence to prevent crashes
	// while editing
	int nFirstSequence = -1;
	for(int i=0 ; i<MAX_SEQUENCES ; i++)
	{
		if ( m_pSamples[i] )
		{
			nFirstSequence = i;
			break;
		}
	}

	if ( nFirstSequence != -1 )
	{
		for(int i=0 ; i<MAX_SEQUENCES ; i++)
		{
			if ( m_pSamples[i] == NULL )
			{
				m_pSamples[i] = m_pSamples[nFirstSequence];
				m_bClamp[i]= m_bClamp[nFirstSequence];
				m_nNumFrames[i] = m_nNumFrames[nFirstSequence];
				m_bSequenceIsCopyOfAnotherSequence[i] = true;
			}
		}
	}
}

CSheet::~CSheet( void )
{
	for( int i=0; i<NELEMS(m_pSamples); i++ )
	{
		if ( m_pSamples[i] && ( ! m_bSequenceIsCopyOfAnotherSequence[i] ) )
		{
			delete[] m_pSamples[i];
		}
	}
}
