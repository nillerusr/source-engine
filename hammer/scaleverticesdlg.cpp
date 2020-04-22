//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ScaleVerticesDlg.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "ScaleVerticesDlg.h"
#include "MapDoc.h"
#include "GlobalFunctions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CScaleVerticesDlg dialog


CScaleVerticesDlg::CScaleVerticesDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CScaleVerticesDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CScaleVerticesDlg)
	//}}AFX_DATA_INIT
}


void CScaleVerticesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CScaleVerticesDlg)
	DDX_Control(pDX, IDC_SCALESPIN, m_cScaleSpin);
	DDX_Control(pDX, IDC_SCALE, m_cScale);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CScaleVerticesDlg, CDialog)
	//{{AFX_MSG_MAP(CScaleVerticesDlg)
	ON_EN_CHANGE(IDC_SCALE, OnChangeScale)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SCALESPIN, OnDeltaposScalespin)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScaleVerticesDlg message handlers

void CScaleVerticesDlg::OnChangeScale() 
{
	CString str;
	m_cScale.GetWindowText(str);
	m_fScale = atof(str);

	if (m_fScale <= 0)
	{
		m_fScale = (float)0.005;
	}

	// send command to document
	CMapDoc::GetActiveMapDoc()->OnCmdMsg(ID_VSCALE_CHANGED, CN_COMMAND, NULL, NULL);
}

void CScaleVerticesDlg::OnDeltaposScalespin(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;

	CString str;
	m_cScale.GetWindowText(str);
	m_fScale = atof(str);
	m_fScale += 0.1f * float(pNMUpDown->iDelta);
	if(m_fScale <= 0)
		m_fScale = 0;
	str.Format("%.3f", m_fScale);
	m_cScale.SetWindowText(str);

	*pResult = 0;
}

BOOL CScaleVerticesDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_cScale.SetWindowText("1.0");
	m_cScaleSpin.SetRange(UD_MINVAL, UD_MAXVAL);

	return TRUE;
}

void CScaleVerticesDlg::OnClose() 
{
	CDialog::OnClose();
}
