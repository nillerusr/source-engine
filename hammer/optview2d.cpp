//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// OPTView2D.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "OPTView2D.h"
#include "Options.h"	

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// COPTView2D property page

IMPLEMENT_DYNCREATE(COPTView2D, CPropertyPage)

COPTView2D::COPTView2D() : CPropertyPage(COPTView2D::IDD)
{
	//{{AFX_DATA_INIT(COPTView2D)
	//}}AFX_DATA_INIT
}

COPTView2D::~COPTView2D()
{
}

void COPTView2D::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(COPTView2D)
	DDX_Control(pDX, IDC_GRIDINTENSITY, m_cGridIntensity);
	//}}AFX_DATA_MAP

	m_cGridIntensity.SetRange(10, 100, TRUE);

	DDX_Check(pDX, IDC_CROSSHAIRS, Options.view2d.bCrosshairs);
	DDX_Check(pDX, IDC_GROUPCARVE, Options.view2d.bGroupCarve);
	DDX_Check(pDX, IDC_ROTATECONSTRAIN, Options.view2d.bRotateConstrain);
	DDX_Check(pDX, IDC_SCROLLBARS, Options.view2d.bScrollbars);
	DDX_Check(pDX, IDC_DRAWVERTICES, Options.view2d.bDrawVertices);
	DDX_Check(pDX, IDC_DRAWMODELSIN2D, Options.view2d.bDrawModels);
	DDX_Check(pDX, IDC_WHITEONBLACK, Options.view2d.bWhiteOnBlack);
	DDX_Check(pDX, IDC_GRIDHIGH10, Options.view2d.bGridHigh10);
	DDX_Check(pDX, IDC_HIDESMALLGRID, Options.view2d.bHideSmallGrid);
	DDX_Check(pDX, IDC_ORIENTPRIMITIVES, Options.view2d.bOrientPrimitives);
	DDX_Check(pDX, IDC_NUDGE, Options.view2d.bNudge);
	DDX_Check(pDX, IDC_AUTOSELECT, Options.view2d.bAutoSelect);
	DDX_Check(pDX, IDC_SELECTBYHANDLES, Options.view2d.bSelectbyhandles);
	DDX_Check(pDX, IDC_KEEPCLONEGROUP, Options.view2d.bKeepclonegroup);
	DDX_Check(pDX, IDC_GRIDHIGH64, Options.view2d.bGridHigh64);
	DDX_Check(pDX, IDC_GRIDHIGH1024, Options.view2d.bGridHigh1024);
	DDX_Check(pDX, IDC_CENTERONCAMERA, Options.view2d.bCenteroncamera);
	DDX_Check(pDX, IDC_USEGROUPCOLORS, Options.view2d.bUsegroupcolors);
	DDX_Check(pDX, IDC_GRIDDOTS, Options.view2d.bGridDots);
	DDX_Text(pDX, IDC_GRIDHIGHSPEC, Options.view2d.iGridHighSpec);

	CString strTemp;
	strTemp.Format("%d", Options.view2d.iDefaultGrid);
	DDX_CBString(pDX, IDC_GRID, strTemp);
	if(pDX->m_bSaveAndValidate)	// converting back
	{
		Options.view2d.iDefaultGrid = atoi(strTemp);
		Options.view2d.iGridIntensity = m_cGridIntensity.GetPos();
	}
	else
	{
		m_cGridIntensity.SetPos(Options.view2d.iGridIntensity);
	}
}


BEGIN_MESSAGE_MAP(COPTView2D, CPropertyPage)
	//{{AFX_MSG_MAP(COPTView2D)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COPTView2D message handlers

BOOL COPTView2D::OnInitDialog() 
{
	CWnd *pwnd = GetDlgItem(IDC_GRIDHIGH1024);
	if (pwnd != NULL)
	{
		pwnd->EnableWindow(FALSE);
		pwnd->ShowWindow(SW_HIDE);
	}

	CPropertyPage::OnInitDialog();
	return TRUE;
}

BOOL COPTView2D::OnApply() 
{
	Options.PerformChanges(COptions::secView2D);	
	return CPropertyPage::OnApply();
}
