//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The UI for simple map compiles.
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "RunMap.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static LPCTSTR pszSection = "Run Map";


CRunMap::CRunMap(CWnd* pParent /*=NULL*/)
	: CDialog(CRunMap::IDD, pParent)
{
	m_bSwitchMode = FALSE;

	//{{AFX_DATA_INIT(CRunMap)
	m_iVis = -1;
	m_bNoQuake = FALSE;
	m_strQuakeParms = _T("");
	m_iLight = -1;
	m_iQBSP = -1;
	m_bHDRLight = FALSE;
	//}}AFX_DATA_INIT

	// read from ini
	CWinApp *App = AfxGetApp();
	m_iQBSP = App->GetProfileInt(pszSection, "QBSP", 1);

	m_iVis = App->GetProfileInt(pszSection, "Vis", 1);
	m_iLight = App->GetProfileInt(pszSection, "Light", 1);
	m_bHDRLight = App->GetProfileInt(pszSection, "HDRLight", 0);
	m_bNoQuake = App->GetProfileInt(pszSection, "No Game", 0);
	m_strQuakeParms = App->GetProfileString(pszSection, "Game Parms", "");
}


void CRunMap::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRunMap)
	DDX_Check(pDX, IDC_NOQUAKE, m_bNoQuake);
	DDX_Text(pDX, IDC_QUAKEPARMS, m_strQuakeParms);
	DDX_Radio(pDX, IDC_BSP0, m_iQBSP);
	DDX_Radio(pDX, IDC_VIS0, m_iVis);
	DDX_Radio(pDX, IDC_RAD0, m_iLight);
	DDX_Check(pDX, IDC_RAD_HDR, m_bHDRLight);
	//}}AFX_DATA_MAP
}


void CRunMap::SaveToIni(void)
{
	CWinApp *App = AfxGetApp();
	App->WriteProfileInt(pszSection, "QBSP", m_iQBSP);
	App->WriteProfileInt(pszSection, "Vis", m_iVis);
	App->WriteProfileInt(pszSection, "Light", m_iLight);
	App->WriteProfileInt(pszSection, "HDRLight", m_bHDRLight);
	App->WriteProfileInt(pszSection, "No Game", m_bNoQuake);
	App->WriteProfileString(pszSection, "Game Parms", m_strQuakeParms);
}


BEGIN_MESSAGE_MAP(CRunMap, CDialog)
	//{{AFX_MSG_MAP(CRunMap)
	ON_BN_CLICKED(IDC_EXPERT, OnExpert)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRunMap message handlers

void CRunMap::OnExpert() 
{
	m_bSwitchMode = TRUE;
	EndDialog(IDOK);
}

BOOL CRunMap::OnInitDialog() 
{
	CDialog::OnInitDialog();
	return TRUE;
}
