//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VBSP_DISPINFO_H
#define VBSP_DISPINFO_H
#ifdef _WIN32
#pragma once
#endif

#include "vbsp.h"

#include "utlvector.h"
#include "disp_tesselate.h"

class CPhysCollisionEntry;
class CCoreDispInfo;
struct dmodel_t;

// This provides the template functions that the engine's tesselation code needs
// so we can share the code in VBSP.
class CVBSPTesselateHelper : public CBaseTesselateHelper
{
public:
	void EndTriangle()
	{
		m_pIndices->AddToTail( m_TempIndices[0] );
		m_pIndices->AddToTail( m_TempIndices[1] );
		m_pIndices->AddToTail( m_TempIndices[2] );
	}

	DispNodeInfo_t& GetNodeInfo( int iNodeBit )
	{
		// VBSP doesn't care about these. Give it back something to play with.
		static DispNodeInfo_t dummy;
		return dummy;
	}

public:
	CUtlVector<unsigned short> *m_pIndices;
};

extern CUtlVector<CCoreDispInfo*> g_CoreDispInfos;


// Setup initial entries in g_dispinfo with some of the vertex data from the mapdisps.
void EmitInitialDispInfos();

// Resample vertex alpha into lightmap alpha for displacement surfaces so LOD popping artifacts are 
// less noticeable on the mid-to-high end.
//
// Also builds neighbor data.
void EmitDispLMAlphaAndNeighbors();

// Setup a CCoreDispInfo given a mapdispinfo_t.
// If pFace is non-NULL, then lightmap texture coordinates will be generated.
void DispMapToCoreDispInfo( mapdispinfo_t *pMapDisp,
	CCoreDispInfo *pCoreDispInfo, dface_t *pFace, int *pSwappedTexInfos );


void DispGetFaceInfo( mapbrush_t *pBrush );
bool HasDispInfo( mapbrush_t *pBrush );

// Computes the bounds for a disp info
void ComputeDispInfoBounds( int dispinfo, Vector& mins, Vector& maxs );

#endif // VBSP_DISPINFO_H
