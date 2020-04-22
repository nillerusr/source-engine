//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOL3D_H
#define TOOL3D_H
#ifdef _WIN32
#pragma once
#endif

#include "MapAtom.h"
#include "ToolInterface.h"

class CMapDoc;
class CMapView;
class CRender;


#define inrange(a,minv,maxv)	((a) >= (minv) && (a) <= (maxv))
#pragma warning(disable: 4244)

#define	DRAG_THRESHHOLD		2
#define	HANDLE_OFFSET		6

class Tool3D : public CBaseTool
{
public:

	Tool3D(void);

	virtual int  HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles = false) = 0;
			int  HitTest(CMapView *pView, const Vector &ptWorld, bool bTestHandles = false);

			bool HitRect(CMapView *pView, const Vector2D &ptHit, const Vector &vecCenter, int extent );

			int	 GetTransformationAxis(); // 0,1,2 or -1 if translation is not axis aligned

	virtual unsigned int GetConstraints(unsigned int nKeyFlags);

			// helper functions when transforming from a map view input
			void StartTranslation( CMapView *pView, const Vector2D &vClickPoint, bool bUseDefaultPlane = true );
			void ProjectTranslation( CMapView *pView, const Vector2D &vPoint, Vector &vTransform, int nFlags = 0);
			void ProjectOnTranslationPlane( const Vector &vWorld, Vector &vTransform, int nFlags = 0 );
			void SetTransformationPlane(const Vector &vOrigin, const Vector &vHorz, const Vector &vVert, const Vector &vNormal);
			bool UpdateTranslation(CMapView *pView, const Vector2D &vPoint, UINT nFlags);
	
			bool IsTranslating(void) { return m_bIsTranslating; }
	virtual bool UpdateTranslation(const Vector &vUpdate, UINT flags);	
	virtual void TranslatePoint(Vector& vPos);
	virtual void FinishTranslation(bool bSave);

	virtual bool OnLMouseDown2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnLMouseUp2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnRMouseDown2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnRMouseUp2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnMouseMove2D( CMapView2D *pView, UINT nFlags, const Vector2D &vPoint );

	virtual bool OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
   	virtual bool OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );

			void RenderTranslationPlane(CRender *pRender);

protected:

	enum
	{
		MOUSE_LEFT = 0,
		MOUSE_RIGHT = 1,
	};

	// each translation can use a translation plane
	Vector		m_vPlaneOrigin;	// transformation plane origin
	Vector		m_vPlaneNormal; // transformation plane normal
	Vector		m_vPlaneHorz;	// transformation plane horizontal axis
	Vector		m_vPlaneVert;   // transformation plane vertical axis

	Vector		m_vTranslation;			// relative translation vector on the translation plane
	Vector		m_vTranslationStart;	// translation start point on translation plane
	bool		m_bIsTranslating;		// true while translation

	// 0 = left, 1 = right button
	bool	m_bMouseDown[2];		// True if mouse button is down, false if not.
	bool	m_bMouseDragged[2];		// Have they dragged the mouse with button down?
	Vector2D m_vMouseStart[2];		// Client pos at which last mouse was pressed.
	Vector2D m_vMousePos;			// last know mouse pos
};

#endif // TOOL3D_H
