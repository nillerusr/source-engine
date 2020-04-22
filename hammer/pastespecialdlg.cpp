//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "PasteSpecialDlg.h"

#pragma warning(disable:4244)

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static LPCTSTR pszIni = "Paste Special";


CPasteSpecialDlg::CPasteSpecialDlg(CWnd* pParent /*=NULL*/, BoundBox* pBox)
	: CDialog(CPasteSpecialDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CPasteSpecialDlg)
	m_iCopies = 1;
	m_bGroup = FALSE;
	m_iOffsetX = 0;
	m_iOffsetY = 0;
	m_iOffsetZ = 0;
	m_fRotateX = 0.0f;
	m_fRotateZ = 0.0f;
	m_fRotateY = 0.0f;
	m_bCenterOriginal = TRUE;
	m_bMakeEntityNamesUnique = FALSE;
	m_bAddPrefix = FALSE;
	//}}AFX_DATA_INIT

	CWinApp *App = AfxGetApp();
	CString str;
	LPCTSTR p;

	m_iCopies = App->GetProfileInt(pszIni, "Copies", 1);
	m_bGroup = App->GetProfileInt(pszIni, "Group", FALSE);

	str = App->GetProfileString(pszIni, "Offset", "0 0 0");
	p = str.GetBuffer(0);
	m_iOffsetX = atoi(p);
	m_iOffsetY = atoi(strchr(p, ' ')+1);
	m_iOffsetZ = atoi(strrchr(p, ' ')+1);

	str = App->GetProfileString(pszIni, "Rotate", "0 0 0");
	p = str.GetBuffer(0);
	m_fRotateX = atof(p);
	m_fRotateY = atof(strchr(p, ' ')+1);
	m_fRotateZ = atof(strrchr(p, ' ')+1);

	m_bCenterOriginal = App->GetProfileInt(pszIni, "Center", TRUE);

	m_bMakeEntityNamesUnique = App->GetProfileInt(pszIni, "MakeNamesUnique", FALSE);

	m_bAddPrefix = App->GetProfileInt(pszIni, "AddPrefix", FALSE);
	m_strPrefix = App->GetProfileString(pszIni, "Prefix", "");

	ObjectsBox = *pBox;
}

void CPasteSpecialDlg::SaveToIni()
{
	CWinApp *App = AfxGetApp();
	CString str;

	App->WriteProfileInt(pszIni, "Copies", m_iCopies);
	App->WriteProfileInt(pszIni, "Group", m_bGroup);

	str.Format("%d %d %d", m_iOffsetX, m_iOffsetY, m_iOffsetZ);
	App->WriteProfileString(pszIni, "Offset", str);

	str.Format("%.1f %.1f %.1f", m_fRotateX, m_fRotateY, m_fRotateZ);
	App->WriteProfileString(pszIni, "Rotate", str);

	App->WriteProfileInt(pszIni, "Center", m_bCenterOriginal);
	App->WriteProfileInt(pszIni, "MakeNamesUnique", m_bMakeEntityNamesUnique);

	App->WriteProfileInt(pszIni, "AddPrefix", m_bAddPrefix);
	App->WriteProfileString(pszIni, "Prefix", m_strPrefix);
}


BOOL CPasteSpecialDlg::OnInitDialog()
{
	BOOL bEnable = m_bAddPrefix ? TRUE : FALSE;
	GetDlgItem( IDC_PASTE_SPECIAL_PREFIX_TEXT )->EnableWindow( bEnable );
	
	return CDialog::OnInitDialog();
}

void CPasteSpecialDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPasteSpecialDlg)
	DDX_Text(pDX, IDC_COPIES, m_iCopies);
	DDV_MinMaxInt(pDX, m_iCopies, 1, 256);
	DDX_Check(pDX, IDC_GROUP, m_bGroup);
	DDX_Text(pDX, IDC_OFFSETX, m_iOffsetX);
	DDX_Text(pDX, IDC_OFFSETY, m_iOffsetY);
	DDX_Text(pDX, IDC_OFFSETZ, m_iOffsetZ);
	DDX_Text(pDX, IDC_ROTATEX, m_fRotateX);
	DDV_MinMaxFloat(pDX, m_fRotateX, 0.f, 360.f);
	DDX_Text(pDX, IDC_ROTATEZ, m_fRotateZ);
	DDV_MinMaxFloat(pDX, m_fRotateZ, 0.f, 360.f);
	DDX_Text(pDX, IDC_ROTATEY, m_fRotateY);
	DDV_MinMaxFloat(pDX, m_fRotateY, 0.f, 360.f);
	DDX_Check(pDX, IDC_CENTERORIGINAL, m_bCenterOriginal);
	DDX_Check(pDX, IDC_PASTE_SPECIAL_MAKE_UNIQUE, m_bMakeEntityNamesUnique);
	DDX_Check(pDX, IDC_PASTE_SPECIAL_ADD_PREFIX, m_bAddPrefix);
	DDX_Text(pDX, IDC_PASTE_SPECIAL_PREFIX_TEXT, m_strPrefix);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPasteSpecialDlg, CDialog)
	//{{AFX_MSG_MAP(CPasteSpecialDlg)
	ON_BN_CLICKED(IDC_GETOFFSETX, OnGetoffsetx)
	ON_BN_CLICKED(IDC_GETOFFSETY, OnGetoffsety)
	ON_BN_CLICKED(IDC_GETOFFSETZ, OnGetoffsetz)
	ON_BN_CLICKED(IDC_PASTE_SPECIAL_ADD_PREFIX, OnCheckUncheckAddPrefix)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CPasteSpecialDlg::GetOffset(int iAxis, int iEditCtrl)
{
	CWnd *pWnd = GetDlgItem(iEditCtrl);

	Assert(pWnd);

	// get current value
	CString strValue;
	pWnd->GetWindowText(strValue);
	int iValue = atoi(strValue);

	int iAxisSize = ObjectsBox.bmaxs[iAxis] - ObjectsBox.bmins[iAxis];

	if(iValue == iAxisSize)	// if it's already positive, make it neg
		strValue.Format("%d", -iAxisSize);
	else	// it's negative or !=, set it positive
		strValue.Format("%d", iAxisSize);

	// set the window text
	pWnd->SetWindowText(strValue);
}

void CPasteSpecialDlg::OnGetoffsetx() 
{
	GetOffset(0, IDC_OFFSETX);
}

void CPasteSpecialDlg::OnGetoffsety() 
{
	GetOffset(1, IDC_OFFSETY);
}

void CPasteSpecialDlg::OnGetoffsetz() 
{
	GetOffset(2, IDC_OFFSETZ);
}

void CPasteSpecialDlg::OnCheckUncheckAddPrefix()
{
	CButton *pButton = ( CButton * )GetDlgItem( IDC_PASTE_SPECIAL_ADD_PREFIX );
	BOOL bEnable = pButton->GetCheck();
	GetDlgItem( IDC_PASTE_SPECIAL_PREFIX_TEXT )->EnableWindow( bEnable );
}
