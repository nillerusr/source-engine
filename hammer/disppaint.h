//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DISPPAINT_H
#define DISPPAINT_H
#pragma once

#include "mathlib/vector.h"
#include "utlvector.h"
#include "DispManager.h"

class CMapDisp;

struct SpatialPaintData_t
{
	int				m_nEffect;
	Vector			m_vCenter;
	float			m_flRadius;
	float			m_flScalar;
	Vector			m_vPaintAxis;
	unsigned int	m_uiBrushType;
	bool			m_bNudge;
	bool			m_bNudgeInit;

	// Cache
	float			m_flRadius2;
	float			m_flOORadius2;
};

class CDispPaintMgr
{
public:

	CDispPaintMgr();
	~CDispPaintMgr();

	bool Paint( SpatialPaintData_t &spatialData, bool bAutoSew );

protected:

	// Painting.
	bool PrePaint( SpatialPaintData_t &spatialData );
	bool PostPaint( bool bAutoSew );
	bool DoPaint( SpatialPaintData_t &spatialData );
	void DoPaintAdd( SpatialPaintData_t &spatialData, CMapDisp *pDisp );
	void DoPaintEqual( SpatialPaintData_t &spatialData, CMapDisp *pDisp );
	void DoPaintSmooth( SpatialPaintData_t &spatialData, CMapDisp *pDisp );

	void DoPaintOneOverR( const SpatialPaintData_t &spatialData, const Vector &vPos, float flDistance2, Vector &vNewPos );
	void DoPaintOne( const SpatialPaintData_t &spatialData, const Vector &vPos, Vector &vNewPos );
	bool DoPaintSmoothOneOverExp( const SpatialPaintData_t &spatialData, const Vector &vNewCenter, Vector &vPaintPos );

	// Utility.
	bool PaintSphereDispBBoxOverlap( const Vector &vCenter, float flRadius, const Vector &vBBoxMin, const Vector &vBBoxMax );
	bool IsInSphereRadius( const Vector &vCenter, float flRadius2, const Vector &vPos, float &flDistance2 );
	float CalcSmoothRadius2( const SpatialPaintData_t &spatialData, const Vector &vPoint );

	void AddToUndo( CMapDisp **pDisp );

	// Nudge
	void NudgeAdd( CMapDisp *pDisp, int iVert );
	void DoNudgeAdd( SpatialPaintData_t &spatialData );

protected:

	struct DispVertPair_t
	{
		EditDispHandle_t	m_hDisp;
		int					m_iVert;
	};

	CUtlVector<DispVertPair_t>	m_aNudgeData;
};

#endif // DISPPAINT_H