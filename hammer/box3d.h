//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BOX3D_H
#define BOX3D_H
#pragma once


#include "Tool3D.h"
#include "BoundBox.h"


class CMapView2D;
class CRender3D;


//
// Formats for displaying world units.
//
enum WorldUnits_t
{
	Units_None,
	Units_Inches,
	Units_Feet_Inches,
};


class Box3D : public Tool3D, public BoundBox
{

public:

	Box3D(void);

	static inline void SetWorldUnits(WorldUnits_t eWorldUnits);
	static inline WorldUnits_t GetWorldUnits(void);

	//
	// CBaseTool implementation.
	//
	virtual void SetEmpty();
	virtual void RenderTool2D(CRender2D *pRender);
	virtual void RenderTool3D(CRender3D *pRender);

	virtual void UpdateStatusBar();

protected:

	enum
	{
		expandbox = 0x01,
		thicklines = 0x04,
		boundstext = 0x08,
	};

	enum TransformMode_t
	{
		modeNone = 0,
		modeMove,
		modeScale,
		modeRotate,
		modeShear,
		modeLast,
	};

	void StartNew( CMapView *pView, const Vector2D &vPoint, const Vector &vecStart, const Vector &vecSize);

	inline int GetTranslateMode() { return m_TranslateMode; }
	
	virtual void ToggleTranslateMode(void);
	void EnableHandles(bool bEnable);

	void SetDrawFlags(DWORD dwFlags);
	DWORD GetDrawFlags() { return m_dwDrawFlags; }
	void SetDrawColors(COLORREF dwHandleColor, COLORREF dwBoxColor);

	virtual void GetStatusString(char *pszBuf);
    unsigned long UpdateCursor(CMapView *pView, const Vector &vHandleHit, TransformMode_t eTransformMode);

	void HandleToWorld( Vector &vWorld, const Vector &vHandle, const Vector *pCustomHandleBox = NULL);
	const Vector NearestCorner(const Vector2D &vPoint, CMapView *pView, const Vector *pCustomHandleBox = NULL);
	int GetVisibleHandles( Vector *handles, CMapView *, int nMode );

	void RenderHandles2D(CRender2D *pRender, const Vector &mins, const Vector &maxs );
	void RenderHandles3D(CRender3D *pRender, const Vector &mins, const Vector &maxs);
	

	//
	// Tool3D implementation.
	//

public:
	virtual int  HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles = false);
	
	// If pCustomHandleBox is non-null, it points at an array 2 vectors (min and max), and
	// it will use those bounds to figure out the corners that it will align to the grid.
	virtual void StartTranslation( CMapView *pView, const Vector2D &vPoint, const Vector &vHandleOrigin, const Vector *pRefPoint = NULL, const Vector *pCustomHandleBox = NULL );
	
	virtual bool UpdateTranslation(const Vector &vUpdate, UINT uConstraints);
	virtual void FinishTranslation(bool bSave);
	virtual void TranslatePoint(Vector& pt);
			void TranslateBox(Vector& mins, Vector& maxs);
	virtual const VMatrix& GetTransformMatrix();

protected:

			void UpdateTransformMatrix();

	static WorldUnits_t m_eWorldUnits;

	COLORREF m_clrHandle;
	COLORREF m_clrBox;

	TransformMode_t	m_TranslateMode;		// current translation mode	
	Vector			m_TranslateHandle;		// current translation handle/corner
	Vector			m_vTranslationFixPoint;	// fix point, meaning it remains unchanged by translation, eg rotation center etc.
	VMatrix			m_TransformMatrix;
	
	bool			m_bEnableHandles;		// check/show handles yes/no
	Vector			m_LastHitTestHandle;	// handle hit by last HitTest call
	TransformMode_t	m_LastTranslateMode;	// last translate mode 
	
	bool	m_bPreventOverlap;
	DWORD	m_dwDrawFlags;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
WorldUnits_t Box3D::GetWorldUnits(void)
{
	return(m_eWorldUnits);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Box3D::SetWorldUnits(WorldUnits_t eWorldUnits)
{
	m_eWorldUnits = eWorldUnits;
}


#endif // BOX3D_H
