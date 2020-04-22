//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: sheet definitions for particles and other sprite functions
//
//===========================================================================//

#ifndef PSHEET_H
#define PSHEET_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlobjectreference.h"

class CUtlBuffer;

// classes for keeping a dictionary of sheet files in memory.  A sheet is a bunch of frames packewd
// within one texture. Each sheet has 1 or more frame sequences stored for it.


// for fast lookups to retrieve sequence data, we store the sequence information discretized into
// a fixed # of frames. If this discretenesss is a visual problem, you can lerp the blend values to get it
// perfect.
#define SEQUENCE_SAMPLE_COUNT 1024

#define MAX_SEQUENCES 64
#define MAX_IMAGES_PER_FRAME_ON_DISK 4
#define MAX_IMAGES_PER_FRAME_IN_MEMORY 2

struct SequenceSampleTextureCoords_t
{
	float m_fLeft_U0;
	float m_fTop_V0;
	float m_fRight_U0;
	float m_fBottom_V0;

	float m_fLeft_U1;
	float m_fTop_V1;
	float m_fRight_U1;
	float m_fBottom_V1;
};

struct SheetSequenceSample_t
{
	// coordinates of two rectangles (old and next frame coords)

	SequenceSampleTextureCoords_t m_TextureCoordData[MAX_IMAGES_PER_FRAME_IN_MEMORY];

	float m_fBlendFactor;

	void CopyFirstFrameToOthers(void)
	{
		// for old format files only supporting one image per frame
		for(int i=1; i < MAX_IMAGES_PER_FRAME_IN_MEMORY; i++)
		{
			m_TextureCoordData[i] = m_TextureCoordData[0];
		}
	}

};

class CSheet
{
public:
	// read form a .sht file. This is the usual thing to do
	CSheet( CUtlBuffer &buf );
	CSheet( void );
	~CSheet( void );

	// references for smart ptrs
	CUtlReferenceList<CSheet> m_References;

	SheetSequenceSample_t *m_pSamples[MAX_SEQUENCES];
	bool m_bClamp[MAX_SEQUENCES];
	bool m_bSequenceIsCopyOfAnotherSequence[MAX_SEQUENCES];
	int	m_nNumFrames[MAX_SEQUENCES];
	float m_flFrameSpan[MAX_SEQUENCES];

};


#endif   // PSHEET_H

