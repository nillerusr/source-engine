//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DISPDLG_H
#define DISPDLG_H
#pragma once

#include "resource.h"
#include "IconComboBox.h"
#include "afxwin.h"

//=============================================================================
//
// Displacement Create Dialog
//
class CDispCreateDlg : public CDialog
{
public:

	CDispCreateDlg( CWnd *pParent = NULL );

	//{{AFX_DATA( CDispCreateDlg )
	enum { IDD = IDD_DISP_CREATE };
	unsigned int	m_Power;
	CEdit			m_editPower;
	CSpinButtonCtrl	m_spinPower;
	//}}AFX_DATA

	//{{AFX_VIRTUAL( CDispCreateDlg )
	virtual void DoDataExchange( CDataExchange *pDX );
	virtual BOOL OnInitDialog( void );
	//}}AFX_VIRTUAL

protected:

	//{{AFX_MSG( CDispCreateDlg )
    afx_msg void OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


//=============================================================================
//
// Displacement Noise Dialog
//
class CDispNoiseDlg : public CDialog
{
public:

	CDispNoiseDlg( CWnd *pParent = NULL );

	//{{AFX_DATA( CDispNoiseDlg )
	enum { IDD = IDD_DISP_NOISE };
	float			m_Min;
	float			m_Max;
	CEdit			m_editMin;
	CEdit			m_editMax;
	CSpinButtonCtrl	m_spinMin;
	CSpinButtonCtrl m_spinMax;
	//}}AFX_DATA

	//{{AFX_VIRTUAL( CDispNoiseDlg )
	virtual void DoDataExchange( CDataExchange *pDX );
	virtual BOOL OnInitDialog( void );
	//}}AFX_VIRTUAL

protected:

	//{{AFX_MSG( CDispNoiseDlg )
	afx_msg void CDispNoiseDlg::OnSpinUpDown( NMHDR *pNMHDR, LRESULT *pResult );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


//=============================================================================
//
// Displacement Paint Dist Dialog
//
class CDispPaintDistDlg : public CDialog
{
public:

	CDispPaintDistDlg( CWnd *pParent = NULL );
	~CDispPaintDistDlg();

	void UpdateSpatialData( void );

	//{{AFX_DATA( CDispPaintDistDlg )
	enum { IDD = IDD_DISP_PAINT_DIST };
	CSliderCtrl			m_sliderDistance;
	CSliderCtrl			m_sliderRadius;
	CEdit				m_editDistance;
	CEdit				m_editRadius;
	CIconComboBox		m_comboboxBrush;
	CComboBox			m_comboboxAxis;
	float				m_Distance;
	float				m_Radius;
	//}}AFX_DATA

	//{{AFX_VIRTUAL( CDispPaintDistDlg )
	virtual void DoDataExchange( CDataExchange *pDX );
	virtual BOOL OnInitDialog( void );
	//}}AFX_VIRTUAL

protected:

	bool InitComboBoxBrushGeo( void );
	bool InitComboBoxAxis( void );
	void InitBrushType( void );
	void EnableBrushTypeButtons( void );
	void DisableBrushTypeButtons( void );
	void FilterComboBoxBrushGeo( unsigned int nEffect, bool bInit );
	void EnablePaintingComboBoxes( void );
	void DisablePaintingComboBoxes( void );

	void UpdateAxis( int nAxis );
	void SetEffectButtonGeo( unsigned int nEffect );
	void SetBrushTypeButtonGeo( unsigned int uiBrushType );

	void InitDistance( void );
	void UpdateSliderDistance( float flDistance, bool bForceInit );
	void UpdateEditBoxDistance( float flDistance, bool bForceInit );
	void InitRadius( void );
	void EnableSliderRadius( void );
	void DisableSliderRadius( void );
	void UpdateSliderRadius( float flRadius, bool bForceInit );
	void UpdateEditBoxRadius( float flRadius, bool bForceInit );

	//{{AFX_MSG( CDispPaintDistDlg )
	afx_msg void OnEffectRaiseLowerGeo( void );
	afx_msg void OnEffectRaiseToGeo( void );
	afx_msg void OnEffectSmoothGeo( void );

	afx_msg void OnBrushTypeSoftEdge( void );
	afx_msg void OnBrushTypeHardEdge( void );

	afx_msg void OnCheckSpatial( void );
	afx_msg void OnCheckAutoSew( void );

	afx_msg void OnComboBoxBrushGeo( void );
	afx_msg void OnComboBoxAxis( void );

	afx_msg void OnHScroll( UINT nSBCode, UINT nPos, CScrollBar *pScrollBar );
	afx_msg void OnEditDistance( void );
	afx_msg void OnEditRadius( void );

	afx_msg void OnClose( void );
	afx_msg void OnDestroy( void );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

protected:

	// Save/Restore
	CRect			m_DialogPosRect;
	unsigned int	m_nPrevEffect;
	int				m_nPrevBrush;
	int				m_nPrevPaintAxis;
	Vector			m_vecPrevPaintAxis;
	float			m_flPrevRadius;
	float			m_flPrevDistance;
};


//=============================================================================
//
// Displacement Paint Dist Dialog
//
class CSculptPushOptions;
class CSculptCarveOptions;
class CSculptProjectOptions;

class CPaintSculptDlg : public CDialog
{
public:

	CPaintSculptDlg( CWnd *pParent = NULL );
	~CPaintSculptDlg();

	bool	GetAutoSew( ) { return m_bAutoSew; } 

	void	UpdateSpatialData( );

	//{{AFX_DATA( CPaintSculptDlg )
	enum { IDD = IDD_DISP_PAINT_SCULPT };
	//}}AFX_DATA

	//{{AFX_VIRTUAL( CPaintSculptDlg )
	virtual void DoDataExchange( CDataExchange *pDX );
	virtual BOOL OnInitDialog( );
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG( CPaintSculptDlg )
	afx_msg void OnCheckAutoSew( );

	afx_msg void OnClose( );
	afx_msg void OnDestroy( void );	
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnBnClickedSculptPush( );
	afx_msg void OnBnClickedSculptCarve( );
	afx_msg void OnBnClickedSculptProject( );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

protected:
	typedef enum
	{
		SCULPT_MODE_PUSH,
		SCULPT_MODE_CARVE,
		SCULPT_MODE_PROJECT,
	} SculptMode;

	// Save/Restore
	CRect					m_DialogPosRect;

	CSculptPushOptions		*m_PushOptions;
	CSculptCarveOptions		*m_CarveOptions;
	CSculptProjectOptions	*m_ProjectOptions;

	bool					m_bAutoSew;
	SculptMode				m_SculptMode;

	void	SetActiveMode( SculptMode NewMode );

	CStatic m_SculptOptionsLoc;
	CButton m_AutoSew;
	CButton m_PushButton;
	CButton m_CarveButton;
	CButton m_ProjectButton;
};


//=============================================================================
//
// Displacement Paint Dist Dialog
//
class CDispPaintDataDlg : public CDialog
{
public:

	CDispPaintDataDlg( CWnd *pParent = NULL );
	~CDispPaintDataDlg();

	//{{AFX_DATA( CDispPaintDataDlg )
	enum { IDD = IDD_DISP_PAINT_DATA };
	CIconComboBox		m_comboboxBrush;
	CComboBox			m_comboboxType;
	CSliderCtrl			m_sliderValue;
	CEdit				m_editValue;
	float				m_fValue;
	//}}AFX_DATA

	//{{AFX_VIRTUAL( CDispPaintDataDlg )
	virtual void DoDataExchange( CDataExchange *pDX );
	virtual BOOL OnInitDialog( void );
	//}}AFX_VIRTUAL

protected:

	bool InitComboBoxBrushData( void );
	bool InitComboBoxType( void );
	void FilterComboBoxBrushData( unsigned int uiEffect, bool bInit );

	void SetEffectButtonData( unsigned int effect );

	void InitValue( void );
	void UpdateSliderValue( float fValue );

	//{{AFX_MSG( CDispPaintDataDlg )
	afx_msg void OnEffectRaiseLowerData( void );
	afx_msg void OnEffectRaiseToData( void );
	afx_msg void OnEffectSmoothData( void );

	afx_msg void OnComboBoxBrushData( void );
	afx_msg void OnComboBoxType( void );

	afx_msg void OnHScroll( UINT nSBCode, UINT nPos, CScrollBar *pScrollBar );
	afx_msg void OnEditValue( void );

	afx_msg void OnClose( void );
	afx_msg void OnDestroy( void );	
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

protected:

	// save/restore
	CRect			m_DialogPosRect;
	unsigned int	m_uiPrevEffect;
	float			m_fPrevPaintValue;
	int				m_iPrevBrush;
};

#endif // DISPDLG_H
