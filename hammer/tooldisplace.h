//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOOLDISPLACE_H
#define TOOLDISPLACE_H
#ifdef _WIN32
#pragma once
#endif


//=============================================================================

#include "ToolInterface.h"
#include "MapDisp.h"
#include "DispMapImageFilter.h"
#include "MapFace.h"
#include "DispPaint.h"

class CMapView3D;

//=============================================================================

#define DISPTOOL_NONE				0
#define DISPTOOL_SELECT				1
#define DISPTOOL_PAINT				2
#define DISPTOOL_SELECT_DISP_FACE	3
#define DISPTOOL_TAG_WALKABLE		4
#define DISPTOOL_TAG_BUILDABLE		5
#define DISPTOOL_TAG_REMOVE			6
#define DISPTOOL_PAINT_SCULPT		7

#define DISPPAINT_EFFECT_RAISELOWER	0
#define DISPPAINT_EFFECT_RAISETO	1
#define DISPPAINT_EFFECT_SMOOTH		2
#define DISPPAINT_EFFECT_MODULATE	3

#define DISPPAINT_AXIS_X			0
#define DISPPAINT_AXIS_Y			1
#define DISPPAINT_AXIS_Z			2
#define DISPPAINT_AXIS_SUBDIV		3
#define DISPPAINT_AXIS_FACE			4

#define DISPPAINT_BRUSHTYPE_SOFT	0
#define DISPPAINT_BRUSHTYPE_HARD	1

class CSculptTool;

//=============================================================================
//
// Displacement Tool Class
//
class CToolDisplace : public CBaseTool
{
public:

	//=========================================================================
	//
	// Constructor/Deconstructor
	//
    CToolDisplace();
    ~CToolDisplace();

	//=========================================================================
	//
	// CBaseTool implementation.
	//
	virtual void OnActivate();
	virtual void OnDeactivate();
	virtual ToolID_t GetToolID(void) { return TOOL_FACEEDIT_DISP; }

	virtual bool OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
    virtual bool OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
    virtual bool OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );

	virtual void RenderTool3D( CRender3D *pRender );

	//=========================================================================
	//
	// Attribute Functions
	//
	inline void SetTool( unsigned int uiTool );
	inline unsigned int GetTool( void );
	inline void SetEffect( unsigned int uiEffect );
	inline unsigned int GetEffect( void );

	inline void SetBrushType( unsigned int uiBrushType )				{ m_uiBrushType = uiBrushType; }
	inline unsigned int GetBrushType( void )							{ return m_uiBrushType; }

	inline void SetChannel( int iType, float flValue );
	inline void GetChannel( int iType, float &flValue );

	inline void SetPaintAxis( int iType, Vector const &vecAxis );
	inline void GetPaintAxis( int &iType, Vector &vecAxis );

	inline CDispMapImageFilterManager *GetFilterRaiseLowerMgr( void );
	inline CDispMapImageFilterManager *GetFilterRaiseToMgr( void );
	inline CDispMapImageFilterManager *GetFilterSmoothMgr( void );

	inline void			SetSculptPainter( CSculptTool *Painter ) { m_SculptTool = Painter; }
	inline CSculptTool	*GetSculptPainter( void ) { return m_SculptTool; }

	// flags
	inline bool GetAutoSew( void );
	inline void ToggleAutoSew( void );

	inline bool IsNudging( void );

	inline void SetSelectMask( bool bSelect );
	inline void ToggleSelectMask( void );
	inline bool HasSelectMask( void );
	inline void SetGridMask( bool bGrid );
	inline void ToggleGridMask( void );
	inline bool HasGridMask( void );

	//=========================================================================
	//
	// Spatial Painting
	//
	inline void ToggleSpatialPainting( void );
	inline bool IsSpatialPainting( void );

	inline void SetSpatialRadius( float flRadius );
	inline float GetSpatialRadius( void );

protected:

	void ApplyPaintTool( UINT nFlags, const Vector2D &vPoint, CMapDisp *pDisp );
	void ApplySpatialPaintTool( UINT nFlags,const Vector2D &vPoint, CMapDisp *pDisp );
	void ApplySculptSpatialPaintTool( CMapView3D *pView, UINT nFlags,const Vector2D &vPoint );
	void LiftFaceNormal( CMapView3D *pView, const Vector2D &vPoint );
	void ResizeSpatialRadius_Activate( CMapView3D *pView );
	void ResizeSpatialRadius_Do( void );
	void ResizeSpatialRadius_Deactivate( void );

	void Nudge_Activate( CMapView3D *pView, EditDispHandle_t dispHandle );
	void Nudge_Deactivate( void );
	void Nudge_Do( void );

	void HandleSelection( CMapView3D *pView, const Vector2D &vPoint );
	EditDispHandle_t GetHitPos( CMapView3D *pView, const Vector2D &vPoint );
	inline CMapDisp *GetEditDisp( void );

	void HandleTagging( CMapView3D *pView, const Vector2D &vPoint );
	void HandleTaggingRemove( CMapDisp *pDisp, int nTriIndex );
	void HandleTaggingReset( CMapView3D *pView, const Vector2D &vPoint );
		
private:

	void AddFiltersToManagers( void );
	bool LoadFilters( const char *filename );
	static ChunkFileResult_t LoadFiltersCallback( CChunkFile *pFile, CToolDisplace *pDisplaceTool );

	int GetSelectedDisps( void );
	EditDispHandle_t CollideWithSelectedDisps( const Vector &rayStart, const Vector &rayEnd );
	bool RayAABBTest( CMapDisp *pDisp, const Vector &rayStart, const Vector &rayEnd );
	void BuildParallelepiped( const Vector &boxMin, const Vector &boxMax, PLANE planes[6] );
	bool RayPlaneTest( PLANE *pPlane, const Vector& rayStart, const Vector& rayEnd /*, float *fraction*/ );
	float DistFromPointToRay( const Vector& rayStart, const Vector& rayEnd, const Vector& point );

	inline void UpdateMapViews( CMapView3D *pView );
	inline void CalcViewCenter( CMapView3D *pView );

	void RenderPaintSphere( CRender3D *pRender );
	void RenderHitBox( CRender3D *pRender );
	
protected:

    unsigned int				m_uiTool;               // active displacement tool
	unsigned int				m_uiEffect;				// active displacement effect
	unsigned int				m_uiBrushType;			// active brush type (soft, hard edged)

	CDispMapImageFilterManager	m_FilterLoaderMgr;		// load all the filters into this manager initially
	CDispMapImageFilterManager	m_FilterRaiseLowerMgr;	// filter manager for raise/lower filters
	CDispMapImageFilterManager	m_FilterRaiseToMgr;		// filter manager for raise to filters
	CDispMapImageFilterManager	m_FilterSmoothMgr;		// filter manager for smoothing filters

	int							m_iPaintChannel;		// the paint channel - distance, alpha, etc...
	float						m_flPaintValueGeo;		// the paint value - scalar distance
	float						m_flPaintValueData;		// the paint value - scalar alpha, etc...
	int							m_iPaintAxis;			// the paint axis type xyz-axis, subdiv normal, face normal
	Vector						m_vecPaintAxis;			// the paint axis vector (for subdiv and face normal)

	bool						m_bAutoSew;				// is the auto-sew functionality enabled
	bool						m_bSpatial;				// painting spatially - set spatial default
	float						m_flSpatialRadius;		// spatial painting radius
	bool						m_bSpatialRadius;		// adjust the spatial radius

	bool						m_bSelectMaskTool;		// show the "red" selection state (true/false)
	bool						m_bGridMaskTool;		// show the displacement overlay (true/false)

	bool						m_bNudge;				// special painting style
	bool						m_bNudgeInit;
	EditDispHandle_t			m_EditDispHandle;		// displacement currently being nudged or painted on
	CPoint						m_viewCenter;			// center point of the given view

	Vector2D					m_MousePoint;
	bool						m_bLMBDown;				// left mouse button state
	bool						m_bRMBDown;				// right mouse button state
	CDispPaintMgr				m_DispPaintMgr;			// displacement painting manager
	CSculptTool					*m_SculptTool;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::SetTool( unsigned int uiTool )
{
	m_uiTool = uiTool;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline unsigned int CToolDisplace::GetTool( void )
{
	return m_uiTool;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::SetEffect( unsigned int uiEffect )
{
	m_uiEffect = uiEffect;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline unsigned int CToolDisplace::GetEffect( void )
{
	return m_uiEffect;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::SetChannel( int iType, float flValue )
{
	m_iPaintChannel = iType;
	if ( iType == DISPPAINT_CHANNEL_POSITION )
	{
		m_flPaintValueGeo = flValue;
	}
	else if ( iType == DISPPAINT_CHANNEL_ALPHA )
	{
		m_flPaintValueData = flValue;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::GetChannel( int iType, float &flValue )
{
	if ( iType == DISPPAINT_CHANNEL_POSITION )
	{
		flValue = m_flPaintValueGeo;
	}
	else if ( iType == DISPPAINT_CHANNEL_ALPHA )
	{
		flValue = m_flPaintValueData;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::SetPaintAxis( int iType, Vector const &vecAxis )
{
	m_iPaintAxis = iType;
	m_vecPaintAxis = vecAxis;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::GetPaintAxis( int &iType, Vector &vecAxis )
{
	iType = m_iPaintAxis;
	vecAxis = m_vecPaintAxis;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CDispMapImageFilterManager *CToolDisplace::GetFilterRaiseLowerMgr( void )
{
	return &m_FilterRaiseLowerMgr;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CDispMapImageFilterManager *CToolDisplace::GetFilterRaiseToMgr( void )
{
	return &m_FilterRaiseToMgr;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CDispMapImageFilterManager *CToolDisplace::GetFilterSmoothMgr( void )
{
	return &m_FilterSmoothMgr;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CToolDisplace::GetAutoSew( void )
{
	return m_bAutoSew;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::ToggleAutoSew( void )
{
	m_bAutoSew = !m_bAutoSew;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CToolDisplace::IsNudging( void )
{
	return m_bNudge;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::SetSelectMask( bool bSelect )
{
	m_bSelectMaskTool = bSelect;
	CMapDisp::SetSelectMask( m_bSelectMaskTool );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::ToggleSelectMask( void )
{
	m_bSelectMaskTool = !m_bSelectMaskTool;
	CMapDisp::SetSelectMask( m_bSelectMaskTool );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CToolDisplace::HasSelectMask( void )
{
	return m_bSelectMaskTool;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::SetGridMask( bool bGrid )
{
	m_bGridMaskTool = bGrid;
	CMapDisp::SetGridMask( m_bGridMaskTool );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CToolDisplace::ToggleGridMask( void )
{
	m_bGridMaskTool = !m_bGridMaskTool;
	CMapDisp::SetGridMask( m_bGridMaskTool );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CToolDisplace::HasGridMask( void )
{
	return m_bGridMaskTool;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the displacement that we are working on, based on the last
//			button down event that we received..
//-----------------------------------------------------------------------------
inline CMapDisp *CToolDisplace::GetEditDisp( void )
{
	// sanity check
	if ( m_EditDispHandle == EDITDISPHANDLE_INVALID )
		return NULL;

	return EditDispMgr()->GetDisp( m_EditDispHandle );
}


inline void CToolDisplace::ToggleSpatialPainting( void ) { m_bSpatial = !m_bSpatial; }
inline bool CToolDisplace::IsSpatialPainting( void ) { return m_bSpatial; }

inline void CToolDisplace::SetSpatialRadius( float flRadius ) { m_flSpatialRadius = flRadius; }
inline float CToolDisplace::GetSpatialRadius( void ) { return m_flSpatialRadius; }

#endif // TOOLDISPLACE_H
