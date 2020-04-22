//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the 3D options property page.
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"

#include "GlobalFunctions.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "hammer.h"
#include "OPTView3D.h"
#include "Options.h"	


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_DYNCREATE(COPTView3D, CPropertyPage)


BEGIN_MESSAGE_MAP(COPTView3D, CPropertyPage)
	//{{AFX_MSG_MAP(COPTView3D)
	ON_WM_HSCROLL()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


COPTView3D::COPTView3D(void)
	: CPropertyPage(COPTView3D::IDD)
{
	//{{AFX_DATA_INIT(COPTView3D)
	//}}AFX_DATA_INIT
}


COPTView3D::~COPTView3D(void)
{
}

void PASCAL DDV_FOVRange(CDataExchange *pDX, int value)
{
	if ( ( value > 100 ) | ( value < 30 ) )
	{
		AfxMessageBox("FOV must be 30-100.", MB_ICONEXCLAMATION | MB_OK);
		pDX->Fail();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDX - 
//-----------------------------------------------------------------------------
void COPTView3D::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COPTView3D)
	DDX_Control(pDX, IDC_BACKTEXT, m_cBackText);
	DDX_Control(pDX, IDC_BACKPLANE, m_cBackPlane);
	DDX_Control(pDX, IDC_MODEL_DISTANCE_TEXT, m_ModelDistanceText);
	DDX_Control(pDX, IDC_MODEL_DISTANCE, m_ModelDistance);
	DDX_Control(pDX, IDC_DETAIL_DISTANCE_TEXT, m_DetailDistanceText);
	DDX_Control(pDX, IDC_DETAIL_DISTANCE, m_DetailDistance);
	DDX_Control(pDX, IDC_FORWARD_SPEED_TEXT, m_ForwardSpeedText);
	DDX_Control(pDX, IDC_FORWARD_SPEED, m_ForwardSpeedMax);
	DDX_Control(pDX, IDC_FORWARD_ACCEL_TEXT, m_TimeToMaxSpeedText);
	DDX_Control(pDX, IDC_FORWARD_ACCELERATION, m_TimeToMaxSpeed);
	DDX_Check(pDX, IDC_FILTER_TEXTURES, Options.view3d.bFilterTextures);
	DDX_Check(pDX, IDC_REVERSEY, Options.view3d.bReverseY);
	DDX_Check(pDX, IDC_USEMOUSELOOK, Options.view3d.bUseMouseLook);
	DDX_Check(pDX, IDC_ANIMATE_MODELS, Options.view3d.bAnimateModels);
	DDX_Check(pDX, IDC_REVERSE_SELECTION, Options.view3d.bReverseSelection);
	DDX_Text(pDX, IDC_FOV, Options.view3d.fFOV);
	DDX_Text(pDX, IDC_LIGHT_CONE_LENGTH, Options.view3d.fLightConeLength);

	DDV_FOVRange(pDX, Options.view3d.fFOV);
	//}}AFX_DATA_MAP

	m_cBackPlane.SetRange(500, 10000, TRUE);
	m_ModelDistance.SetRange(0, 10000, TRUE);
	m_DetailDistance.SetRange(0, 10000, TRUE);
	m_ForwardSpeedMax.SetRange(100, 10000, TRUE);
	m_TimeToMaxSpeed.SetRange(0, 10000, TRUE);

	//
	// If going from controls to data.
	//
	if (pDX->m_bSaveAndValidate)
	{
		Options.view3d.iBackPlane = m_cBackPlane.GetPos();
		Options.view3d.nModelDistance = m_ModelDistance.GetPos();
		Options.view3d.nDetailDistance = m_DetailDistance.GetPos();
		Options.view3d.nForwardSpeedMax = m_ForwardSpeedMax.GetPos();
		Options.view3d.nTimeToMaxSpeed = m_TimeToMaxSpeed.GetPos();	
	}
	//
	// Else going from data to controls.
	//
	else
	{
		CString str;

		//
		// Back plane distance.
		//
		m_cBackPlane.SetPos(Options.view3d.iBackPlane);
		int iBack = m_cBackPlane.GetPos();
		str.Format("%d", iBack);
		m_cBackText.SetWindowText(str);

		//
		// Model render distance.
		//
		m_ModelDistance.SetPos(Options.view3d.nModelDistance);
		int nModelDistance = m_ModelDistance.GetPos();
		str.Format("%d", nModelDistance);
		m_ModelDistanceText.SetWindowText(str);

		//
		// Detail render distance.
		//
		m_DetailDistance.SetPos(Options.view3d.nDetailDistance);
		int nDetailDistance = m_DetailDistance.GetPos();
		str.Format("%d", nDetailDistance);
		m_DetailDistanceText.SetWindowText(str);

		//
		// Set the max forward speed.
		//
		m_ForwardSpeedMax.SetPos(Options.view3d.nForwardSpeedMax);
		int nSpeed = m_ForwardSpeedMax.GetPos();
		str.Format("%d", nSpeed);
		m_ForwardSpeedText.SetWindowText(str);

		//
		// Set the time to max speed.
		//
		m_TimeToMaxSpeed.SetPos(Options.view3d.nTimeToMaxSpeed);
		int nTime = m_TimeToMaxSpeed.GetPos();
		str.Format("%.2f sec", (float)nTime / 1000.0f);
		m_TimeToMaxSpeedText.SetWindowText(str);	
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called before the dialog is displayed.
// Output : Returns FALSE when setting focus to a control, TRUE otherwise.
//-----------------------------------------------------------------------------
BOOL COPTView3D::OnInitDialog(void)
{
	m_bOldFilterTextures = Options.view3d.bFilterTextures;

	CPropertyPage::OnInitDialog();
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Called by the framework when the user chooses the OK or the Apply Now button.
// Output : Nonzero if the changes are accepted; otherwise zero.
//-----------------------------------------------------------------------------
BOOL COPTView3D::OnApply(void)
{
	if (Options.view3d.bFilterTextures != m_bOldFilterTextures)
	{
		AfxMessageBox("The changes to the 'Filter textures' setting will not take effect for any currently visible textures. Close all 3D views and reopen them for the new setting to completely take effect.");
	}

	Options.PerformChanges(COptions::secView3D);
	return CPropertyPage::OnApply();
}


//-----------------------------------------------------------------------------
// Purpose: Handles all the sliders in the property page.
// Input  : Per MFC OnHScroll.
//-----------------------------------------------------------------------------
void COPTView3D::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar) 
{
	//
	// If it is the back plane scroll bar, update the back plane text.
	// Also, notify the 3D view so that it can update in realtime.
	//
	if (pScrollBar->m_hWnd == m_cBackPlane.m_hWnd)
	{
		int iBack = m_cBackPlane.GetPos();

		CString str;
		str.Format("%d", iBack);
		m_cBackText.SetWindowText(str);

		CMainFrame *pMainWnd = GetMainWnd();
		if (pMainWnd != NULL)
		{
			Options.view3d.iBackPlane = m_cBackPlane.GetPos();
			pMainWnd->UpdateAllDocViews( MAPVIEW_OPTIONS_CHANGED | MAPVIEW_RENDER_NOW );
		}
	}
	//
	// Else if it is the model distance scroll bar, update the model distance text.
	// Also, notify the 3D view so that it can update in realtime.
	//
	else if (pScrollBar->m_hWnd == m_ModelDistance.m_hWnd)
	{
		int nDistance = m_ModelDistance.GetPos();

		CString str;
		str.Format("%d", nDistance);
		m_ModelDistanceText.SetWindowText(str);

		CMainFrame *pMainWnd = GetMainWnd();
		if (pMainWnd != NULL)
		{
			Options.view3d.nModelDistance = m_ModelDistance.GetPos();
			pMainWnd->UpdateAllDocViews(MAPVIEW_UPDATE_ONLY_3D | MAPVIEW_OPTIONS_CHANGED | MAPVIEW_RENDER_NOW );
		}
	}
	//
	// Else if it is the detail distance scroll bar, update the detail distance text.
	// Also, notify the 3D view so that it can update in realtime.
	//
	else if (pScrollBar->m_hWnd == m_DetailDistance.m_hWnd)
	{
		int nDistance = m_DetailDistance.GetPos();

		CString str;
		str.Format("%d", nDistance);
		m_DetailDistanceText.SetWindowText(str);

		CMainFrame *pMainWnd = GetMainWnd();
		if (pMainWnd != NULL)
		{
			Options.view3d.nDetailDistance = m_DetailDistance.GetPos();
			pMainWnd->UpdateAllDocViews(MAPVIEW_UPDATE_ONLY_3D | MAPVIEW_OPTIONS_CHANGED | MAPVIEW_RENDER_NOW );
		}
	}
	//
	// Else if it is the maximum forward speed scroll bar, update the maximum forward speed.
	//
	else if (pScrollBar->m_hWnd == m_ForwardSpeedMax.m_hWnd)
	{
		int nSpeed = m_ForwardSpeedMax.GetPos();

		CString str;
		str.Format("%d", nSpeed);
		m_ForwardSpeedText.SetWindowText(str);
	}

	//
	// Else if it is the time to max speed scroll bar, update the time to max speed.
	//
	else if (pScrollBar->m_hWnd == m_TimeToMaxSpeed.m_hWnd)
	{
		float fTimeSeconds = (float)m_TimeToMaxSpeed.GetPos() / 1000.0f;

		CString str;
		str.Format("%.2f sec", fTimeSeconds);
		m_TimeToMaxSpeedText.SetWindowText(str);
	}

	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
}


