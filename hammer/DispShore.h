//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================

#ifndef DISPSHORE_H
#define DISPSHORE_H
#pragma once

#include "MapHelper.h"
#include "MapFace.h"
#include "MapOverlay.h"
#include "MapOverlayTrans.h"

class CHelperInfo;
class CMapEntity;

struct ShoreFaceData_t
{
	CMapFace	*m_pFaces[4];			// The hammer faces the points reside on.
	Vector		m_vecPoints[4];			// The shore face points.
	Vector2D	m_vecTexCoords[4];		// The shore texture coordinates.
	bool		m_bAdjWinding;
};

struct ShoreSegment_t
{
	Vector				m_vecPoints[2];				// Shore segment points.
	Vector				m_vecNormals[2];			// Shore segment normals.
	Vector2D			m_vecTexCoords[4];			// Shore segment texture coordinates.

	EditDispHandle_t	m_hDisp;					// Displacement the shore segment was created from.
	float				m_flWaterZ;

	ShoreFaceData_t		m_WorldFace;
	ShoreFaceData_t		m_WaterFace;

	// ?
	Vector				m_vecCenter;
	unsigned int		m_iStartPoint;
	bool				m_bTouch;
	bool				m_bCreated;
};

struct Shoreline_t
{
	int							m_nShorelineId;
	CUtlVector<ShoreSegment_t>	m_aSegments;		// List of shore segments making up the shore line.
	CUtlVector<int>				m_aSortedSegments;	// List of shore segments sorted (for connectivity).
	CUtlVector<CMapOverlay>		m_aOverlays;
	float						m_flLength;			// Total length of the shore line.
	ShoreEntityData_t			m_ShoreData;

	Shoreline_t();
	~Shoreline_t();
	void AddSegment( Vector &vecPoint0, Vector &vecPoint1, Vector &vecNormal, float flWaterZ, CMapFace *pWaterFace, EditDispHandle_t hDisp );
};

class IDispShoreManager
{
public:

	virtual bool		Init( void ) = 0;
	virtual void		Shutdown( void ) = 0;

	// Shoreline management.
	virtual int			GetShorelineCount( void ) = 0;
	virtual Shoreline_t *GetShoreline( int nShorelineId ) = 0;
	virtual void		AddShoreline( int nShorelineId ) = 0;
	virtual void		RemoveShoreline( int nShorelineId ) = 0;
	virtual void		BuildShoreline( int nShorelineId, CUtlVector<CMapFace*> &aFaces, CUtlVector<CMapFace*> &aWaterFaces ) = 0;

	virtual void		Draw( CRender3D *pRender ) = 0;
	virtual void		DebugDraw( CRender3D *pRender ) = 0;
};

IDispShoreManager *GetShoreManager( void );

#endif // DISPSHORE_H