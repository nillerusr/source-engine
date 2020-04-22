//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SCULPTOPTIONS_H
#define SCULPTOPTIONS_H
#ifdef _WIN32
#pragma once
#endif

#include "disppaint.h"
#include "afxwin.h"

class CMapView3D;
class CPaintSculptDlg;

class CSculptTool
{
public:
					CSculptTool();
					~CSculptTool();

			void	SetPaintOwner( CPaintSculptDlg *pOwner ) { m_PaintOwner = pOwner; }

	virtual bool	BeginPaint( CMapView3D *pView, const Vector2D &vPoint );
	virtual	bool	Paint( CMapView3D *pView, const Vector2D &vPoint, SpatialPaintData_t &SpatialData );
	virtual void	RenderTool3D(CRender3D *pRender) = 0;
	virtual bool	OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnLButtonUpDialog(UINT nFlags, CPoint point) { return false; }
	virtual bool	OnLButtonDownDialog(UINT nFlags, CPoint point) { return false; }
	virtual bool	OnMouseMoveDialog(UINT nFlags, CPoint point) { return false; }

protected:

	// Painting.
	virtual bool	PrePaint( CMapView3D *pView, const Vector2D &vPoint );
	virtual bool	PostPaint( bool bAutoSew );
	virtual bool	DoPaint( CMapView3D *pView, const Vector2D &vPoint );
	virtual void	DoPaintOperation( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp ) = 0;

	bool	GetStartingSpot( CMapView3D *pView, const Vector2D &vPoint );

	bool	IsPointInScreenCircle( CMapView3D *pView, CMapDisp *pDisp, CMapDisp *pOrigDisp, int nVertIndex, bool bUseOrigDisplacement = true, bool bUseCurrentPosition = false, float *pflLengthPercent = NULL );

	void	DoPaintSmooth( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp );
	bool	PaintSphereDispBBoxOverlap( const Vector &vCenter, float flRadius, const Vector &vBBoxMin, const Vector &vBBoxMax );
	bool	IsInSphereRadius( const Vector &vCenter, float flRadius2, const Vector &vPos, float &flDistance2 );
	float	CalcSmoothRadius2( const Vector &vPoint );
	bool	DoPaintSmoothOneOverExp( const Vector &vNewCenter, Vector &vPaintPos );

	void	AddToUndo( CMapDisp **pDisp );

	void	DrawDirection( CRender3D *pRender, Vector vDirection, Color Towards, Color Away );
	void	DuplicateSelectedDisp( );
	void	PrepareDispForPainting( );
	bool	FindCollisionIntercept( CCamera *pCamera, const Vector2D &vPoint, bool bUseOrigPosition, Vector &vCollisionPoint, Vector &vCollisionNormal, float &vCollisionIntercept );

private:
	void	DetermineKeysDown();

protected:
	static bool MapDispLessFunc( EditDispHandle_t const &a, EditDispHandle_t const &b )
	{
		return a < b;
	}

	static CUtlMap<EditDispHandle_t, CMapDisp *>		m_OrigMapDisp;

	Vector						m_StartingCollisionPoint, m_StartingCollisionNormal, m_OriginalCollisionPoint, m_OriginalCollisionNormal, m_CurrentCollisionPoint, m_CurrentCollisionNormal;
	float						m_StartingCollisionIntercept, m_OriginalCollisionIntercept, m_CurrentCollisionIntercept;
	float						m_StartingProjectedRadius, m_OriginalProjectedRadius, m_CurrentProjectedRadius;
	bool						m_OriginalCollisionValid, m_CurrentCollisionValid;

	Vector2D					m_MousePoint;
	bool						m_bLMBDown;				// left mouse button state
	bool						m_bRMBDown;				// right mouse button state
	bool						m_bAltDown, m_bCtrlDown, m_bShiftDown;

	bool						m_ValidPaintingSpot;

	float						m_BrushSize;

	CPaintSculptDlg				*m_PaintOwner;

	SpatialPaintData_t			m_SpatialData;
};

class CSculptPainter : public CSculptTool
{
public:
	CSculptPainter();
	~CSculptPainter();

	virtual bool	BeginPaint( CMapView3D *pView, const Vector2D &vPoint );
	virtual	bool	Paint( CMapView3D *pView, const Vector2D &vPoint, SpatialPaintData_t &spatialData );
	virtual bool	OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );

protected:
	virtual bool	DoSizing( const Vector2D &vPoint );

	bool						m_InPaintingMode;
	bool						m_InSizingMode;
	Vector2D					m_StartSizingPoint;
	float						m_OrigBrushSize;
};

class CSculptPushOptions : public CDialog, public CSculptPainter
{
	DECLARE_DYNAMIC(CSculptPushOptions)

public:
	CSculptPushOptions(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSculptPushOptions();

	virtual BOOL OnInitDialog( void );
	virtual void OnOK();
	virtual void OnCancel();

// Dialog Data
	enum { IDD = IDD_DISP_SCULPT_PUSH_OPTIONS };

	typedef enum
	{
		OFFSET_MODE_ADAPTIVE,
		OFFSET_MODE_ABSOLUTE
	} OffsetMode;

	typedef enum
	{
		NORMAL_MODE_BRUSH_CENTER,
		NORMAL_MODE_SCREEN,
		NORMAL_MODE_SCREEN_XY,
		NORMAL_MODE_X,
		NORMAL_MODE_Y,
		NORMAL_MODE_Z,
		NORMAL_MODE_SELECTED
	} NormalMode;

	typedef enum
	{
		DENSITY_MODE_ADDITIVE,
		DENSITY_MODE_ATTENUATED,
	} DensityMode;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnCbnSelchangeIdcSculptPushOptionNormalMode();
	CComboBox	m_OffsetModeControl;
	CEdit		m_OffsetDistanceControl;
	CEdit		m_OffsetAmountControl;
	CEdit		m_SmoothAmountControl;
	CEdit		m_FalloffPositionControl;
	CEdit		m_FalloffFinalControl;
	CComboBox	m_DensityModeControl;
	CComboBox	m_NormalModeControl;
	afx_msg void OnCbnSelchangeSculptPushOptionOffsetMode();

private:
	OffsetMode	m_OffsetMode;
	float		m_OffsetDistance, m_OffsetAmount;
	NormalMode	m_NormalMode;
	DensityMode	m_DensityMode;
	float		m_Direction;
	float		m_SmoothAmount;
	Vector		m_SelectedNormal;
	float		m_flFalloffSpot;
	float		m_flFalloffEndingValue;

	void		GetPaintAxis( CCamera *pCamera, const Vector2D &vPoint, Vector &vPaintAxis );

public:
	virtual bool BeginPaint( CMapView3D *pView, const Vector2D &vPoint );
	virtual void RenderTool3D(CRender3D *pRender);
	virtual bool OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );

protected:
	virtual void DoPaintOperation( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp );
			void DoSmoothOperation( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp );

public:
	afx_msg void OnEnChangeSculptPushOptionOffsetDistance();
	afx_msg void OnCbnSelchangeSculptPushOptionDensityMode();
	afx_msg void OnEnKillfocusSculptPushOptionSmoothAmount();
	afx_msg void OnEnKillfocusSculptPushOptionOffsetAmount();
	afx_msg void OnEnKillfocusSculptPushOptionFalloffPosition();
	afx_msg void OnEnKillfocusSculptPushOptionFalloffFinal();
};


class CSculptCarveOptions : public CDialog, public CSculptPainter
{
	DECLARE_DYNAMIC(CSculptCarveOptions)

public:
	CSculptCarveOptions(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSculptCarveOptions();

	virtual BOOL OnInitDialog( void );
	virtual void OnOK();
	virtual void OnCancel();

// Dialog Data
	enum { IDD = IDD_DISP_SCULPT_CARVE_OPTIONS };

	typedef enum
	{
		OFFSET_MODE_ADAPTIVE,
		OFFSET_MODE_ABSOLUTE
	} OffsetMode;

	typedef enum
	{
		NORMAL_MODE_BRUSH_CENTER,
		NORMAL_MODE_SCREEN,
		NORMAL_MODE_SCREEN_XY,
		NORMAL_MODE_X,
		NORMAL_MODE_Y,
		NORMAL_MODE_Z,
		NORMAL_MODE_SELECTED
	} NormalMode;

	typedef enum
	{
		DENSITY_MODE_ADDITIVE,
		DENSITY_MODE_ATTENUATED,
	} DensityMode;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

public:
	CComboBox	m_OffsetModeControl;
	CEdit		m_OffsetDistanceControl;
	CEdit		m_OffsetAmountControl;
	CEdit		m_SmoothAmountControl;
	CComboBox	m_DensityModeControl;
	CComboBox	m_NormalModeControl;
	CStatic m_CarveBrushControl;

private:
	const static int	MAX_SCULPT_SIZE = 100;
	const static int	MAX_QUEUE_SIZE = 20;

	OffsetMode	m_OffsetMode;
	float		m_OffsetDistance, m_OffsetAmount;
	NormalMode	m_NormalMode;
	DensityMode	m_DensityMode;
	float		m_Direction;
	float		m_SmoothAmount;
	Vector		m_SelectedNormal;
	float		m_BrushPoints[ MAX_SCULPT_SIZE ];
	int			m_BrushLocation;
	Vector2D	m_StartLine, m_EndLine;
	CUtlVector< Vector2D >  m_DrawPoints;
	CUtlVector< Vector2D >  m_DrawNormal;

	CUtlVector< Vector2D >  m_PointQueue;


	void		GetPaintAxis( CCamera *pCamera, const Vector2D &vPoint, Vector &vPaintAxis );
	void		AdjustBrush( int x, int y );
	void		AdjustBrushCursor( int x, int y );
	bool		CalculatePointNormal( int PointIndex, Vector2D &vNormal );
	bool		CalculateQueuePoint( Vector2D &vPoint, Vector2D &vNormal );
	void		AddQueuePoint( const Vector2D &vPoint, bool bDrawIt );

public:
	virtual bool	BeginPaint( CMapView3D *pView, const Vector2D &vPoint );
//	virtual	bool	Paint( CMapView3D *pView, const Vector2D &vPoint, SpatialPaintData_t &spatialData );
	virtual void	RenderTool3D(CRender3D *pRender);
	virtual bool	OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );

protected:
	bool	IsPointAffected( CMapView3D *pView, CMapDisp *pDisp, CMapDisp *pOrigDisp, int nVertIndex, int nBrushPoint, Vector2D &vViewVert, bool bUseOrigDisplacement = true, bool bUseCurrentPosition = false );
	virtual void DoPaintOperation( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp );

public:
	afx_msg void OnPaint();
	afx_msg void OnEnChangeSculptPushOptionOffsetDistance();
	afx_msg void OnCbnSelchangeSculptPushOptionDensityMode();
	afx_msg void OnEnKillfocusSculptPushOptionSmoothAmount();
	afx_msg void OnEnKillfocusSculptPushOptionOffsetAmount();
	afx_msg void OnCbnSelchangeIdcSculptPushOptionNormalMode();
	afx_msg void OnCbnSelchangeSculptPushOptionOffsetMode();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	virtual BOOL PreTranslateMessage( MSG* pMsg );
};

#if 0

class ITextureInternal;

// CSculptProjectOptions dialog

class CSculptProjectOptions : public CDialog, public CSculptTool
{
	DECLARE_DYNAMIC(CSculptProjectOptions)

public:
	CSculptProjectOptions(CWnd* pParent = NULL);   // standard constructor
	virtual ~CSculptProjectOptions();

	// Dialog Data
	enum { IDD = IDD_DISP_SCULPT_PROJECT_OPTIONS };

public:
	virtual	bool	Paint( CMapView3D *pView, const Vector2D &vPoint, SpatialPaintData_t &spatialData );
	virtual void	RenderTool3D(CRender3D *pRender);
	virtual bool	OnLMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnLMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool	OnMouseMove3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );

protected:
	typedef enum
	{
		PROJECT_MODE_NONE,
		PROJECT_MODE_SIZE,
		PROJECT_MODE_POSITION,
		PROJECT_MODE_TILE,
	} ToolMode;

	CFileDialog			*m_FileDialog;
	unsigned char		*m_ImagePixels;
	int					m_Width, m_Height;
	ITextureInternal	*m_pTexture;
	IMaterial			*m_pMaterial;

	Vector				m_ProjectLocation, m_ProjectSize;
	Vector				m_OriginalProjectLocation, m_OriginalProjectSize;
	int					m_ProjectX, m_ProjectY, m_ProjectWidth, m_ProjectHeight;
	float				m_TileWidth, m_TileHeight;
	float				m_OriginalTileWidth, m_OriginalTileHeight;
	ToolMode			m_ToolMode;
	Vector2D			m_StartSizingPoint;

protected:
	virtual void	DoPaintOperation( CMapView3D *pView, const Vector2D &vPoint, CMapDisp *pDisp, CMapDisp *pOrigDisp );
	virtual bool	DoSizing( const Vector2D &vPoint );
	virtual bool	DoPosition( const Vector2D &vPoint );
	virtual bool	DoTiling( const Vector2D &vPoint );
			bool	ReadImage( CString &FileName );

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedLoadImage();
public:
	CSliderCtrl m_ProjectSizeControl;
public:
	afx_msg void OnNMCustomdrawProjectSize(NMHDR *pNMHDR, LRESULT *pResult);
public:
	virtual BOOL OnInitDialog();
public:
	CStatic m_ProjectSizeNumControl;
};
#endif

#endif // SCULPTOPTIONS_H
