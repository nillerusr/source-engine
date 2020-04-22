//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Data and functionality common to 2D and 3D views.
//
//===========================================================================//

#ifndef MAPVIEW_H
#define MAPVIEW_H
#ifdef _WIN32
#pragma once
#endif

class CWnd;
class CView;
class CMapAtom;
class CMapClass;
class CMapDoc;
class CCamera;
class CToolManager;

#include "mathlib/vector.h"

//
// Maximum number of hits that can be returned by ObjectsAt.
//
#define MAX_PICK_HITS			512

typedef struct HitInfo_s HitInfo_t;


enum DrawType_t
{
	VIEW_INVALID = -1,
	VIEW2D_XY = 0,
	VIEW2D_YZ,
	VIEW2D_XZ,

	VIEW3D_WIREFRAME,
	VIEW3D_POLYGON,
	VIEW3D_TEXTURED,
	VIEW3D_LIGHTMAP_GRID,
	VIEW3D_SMOOTHING_GROUP,
	VIEW3D_ENGINE,
	VIEW3D_TEXTURED_SHADED,

	VIEW_LOGICAL,

	VIEW3D_LIGHTING_PREVIEW2,
	VIEW3D_LIGHTING_PREVIEW_RAYTRACED,
	// Must be last!
	VIEW_TYPE_LAST
};


#define FLAG_OBJECTS_AT_RESOLVE_INSTANCES	0x0000001
#define FLAG_OBJECTS_AT_ONLY_SOLIDS			0x0000002


class CMapView
{
public:

	CMapView(void);

	virtual void	ActivateView(bool bActivate);
	inline bool		IsActive(void) { return(m_bActive); }
	bool	IsOrthographic();

	virtual void		SetDrawType(DrawType_t eDrawType) { m_eDrawType = eDrawType; }
	virtual DrawType_t	GetDrawType(void) { return m_eDrawType; }

	// virtual CMapClass	*ObjectAt(POINT ptClient, ULONG &ulFace) = 0;
				
	virtual void ProcessInput() = 0; // do input update
	virtual void RenderView() = 0;	// render view NOW, called usually by framework
	virtual void UpdateView( int nFlags ); // something changed, render this view with the next frame
	virtual bool ShouldRender(); // let view decide if it wants to render or not
		
	virtual CWnd	*GetViewWnd() = 0;
	virtual CMapDoc	*GetMapDoc() = 0;
			
	// get axis we look along
	virtual const Vector &GetViewAxis();
	void SetCamera(const Vector &vecPos, const Vector &vecLookAt);
	CCamera *GetCamera() { return m_pCamera; }

	// convert client view space to map world coordinates 
	// general rule: float = world, int = client view
	virtual void WorldToClient(Vector2D &ptClient, const Vector &vWorld) = 0;
	virtual void ClientToWorld(Vector &vWorld, const Vector2D &vClient ) = 0;
	virtual void BuildRay( const Vector2D &ptClient, Vector& vStart, Vector& vEnd );
	virtual int  ObjectsAt( const Vector2D &ptClient, HitInfo_t *pObjects, int nMaxObjects, unsigned int nFlags = 0 ) = 0;
	virtual bool HitTest( const Vector2D &ptClient, const Vector& mins, const Vector& maxs ) = 0;
	virtual void GetBestTransformPlane( Vector &horzAxis, Vector &vertAxis, Vector &thirdAxis) = 0;

	bool SelectAt(const Vector2D &ptClient, bool bMakeFirst, bool bFace);
		
		
		
	// protected:

	bool			m_bActive;
	bool			m_bUpdateView;
	DrawType_t		m_eDrawType;
	unsigned int	m_dwTimeLastRender;
	CCamera			*m_pCamera;			// Defines the camera position and settings for this view.
	CToolManager	*m_pToolManager;	// tool manager for this view
	int				m_nRenderedFrames;
	int m_nLastRaytracedBitmapRenderTimeStamp;

};

#endif // MAPVIEW_H
