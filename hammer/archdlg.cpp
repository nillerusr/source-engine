//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "hammer_mathlib.h"
#include "ArchDlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


static LPCTSTR pszSection = "Arch";

extern void MakeArc(float x1, float y1, float x2, float y2, int npoints, 
					 float start_ang, float fArc, float points[][2]);


CArchDlg::CArchDlg(Vector& boxmins, Vector& boxmaxs, CWnd* pParent /*=NULL*/)
	: CDialog(CArchDlg::IDD, pParent)
{
	bmins = boxmins;
	bmaxs = boxmaxs;

	//{{AFX_DATA_INIT(CArchDlg)
	m_iSides = 0;
	m_iWallWidth = 0;
	m_iAddHeight = 0;
	m_fArc = 0.0f;
	m_fAngle = 0.0f;
	//}}AFX_DATA_INIT

	// load up old defaults
	CString str;
	m_iWallWidth = AfxGetApp()->GetProfileInt(pszSection, "Wall Width", 32);
	str = AfxGetApp()->GetProfileString(pszSection, "Arc_", "180");
	m_fArc = atof(str);
	m_iSides = AfxGetApp()->GetProfileInt(pszSection, "Sides", 8);
	str = AfxGetApp()->GetProfileString(pszSection, "Start Angle_", "0");
	m_fAngle = atof(str);
	m_iAddHeight = AfxGetApp()->GetProfileInt(pszSection, "Add Height", 0);
}


void CArchDlg::SaveValues()
{
	CString str;
	AfxGetApp()->WriteProfileInt(pszSection, "Wall Width", m_iWallWidth);
	str.Format("%f", m_fArc);
	AfxGetApp()->WriteProfileString(pszSection, "Arc_", str);
	AfxGetApp()->WriteProfileInt(pszSection, "Sides", m_iSides);
	str.Format("%f", m_fAngle);
	AfxGetApp()->WriteProfileString(pszSection, "Start Angle_", str);
	AfxGetApp()->WriteProfileInt(pszSection, "Add Height", m_iAddHeight);
}


void CArchDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(CArchDlg)
	DDX_Control(pDX, IDC_ANGLESPIN, m_cStartAngleSpin);
	DDX_Control(pDX, IDC_WALLWIDTHSPIN, m_cWallWidthSpin);
	DDX_Control(pDX, IDC_WALLWIDTH, m_cWallWidth);
	DDX_Control(pDX, IDC_SIDESSPIN, m_cSidesSpin);
	DDX_Control(pDX, IDC_SIDES, m_cSides);
	DDX_Control(pDX, IDC_ARCSPIN, m_cArcSpin);
	DDX_Control(pDX, IDC_ARC, m_cArc);
	DDX_Control(pDX, IDC_PREVIEW, m_cPreview);

	DDX_Text(pDX, IDC_WALLWIDTH, m_iWallWidth);
	DDX_Text(pDX, IDC_SIDES, m_iSides);
	DDV_MinMaxInt(pDX, m_iSides, 3, 2048);

	DDX_Text(pDX, IDC_ADDHEIGHT, m_iAddHeight);
	DDX_Text(pDX, IDC_ARC, m_fArc);
	DDV_MinMaxFloat(pDX, m_fArc, 8.f, 360.f);
	DDX_Text(pDX, IDC_ANGLE, m_fAngle);
	DDV_MinMaxFloat(pDX, m_fAngle, 0.f, 360.f);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CArchDlg, CDialog)
	//{{AFX_MSG_MAP(CArchDlg)
	ON_EN_CHANGE(IDC_ARC, OnChangeArc)
	ON_BN_CLICKED(IDC_CIRCLE, OnCircle)
	ON_EN_UPDATE(IDC_SIDES, OnUpdateSides)
	ON_EN_UPDATE(IDC_WALLWIDTH, OnUpdateWallwidth)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_ARCH_PREVIEW, OnArchPreview)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CArchDlg::OnChangeArc() 
{
}


void CArchDlg::OnCircle() 
{
	m_cArcSpin.SetPos(360);
}


void CArchDlg::OnUpdateSides() 
{
}


void CArchDlg::OnUpdateWallwidth() 
{
}


BOOL CArchDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_cArcSpin.SetRange(8, 360);
	m_cSidesSpin.SetRange(3, 100);
	m_cWallWidthSpin.SetRange(2, m_iMaxWallWidth);
	m_cStartAngleSpin.SetRange(0, 360);

	m_cPreview.ShowWindow(SW_HIDE);

	return TRUE;
}


void CArchDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// Do not call CDialog::OnPaint() for painting messages
	CBrush black(RGB(0,0,0));
	CBrush grey(RGB(128,128,128));

	CRect rcPreview;
	m_cPreview.GetWindowRect(&rcPreview);
	ScreenToClient(&rcPreview);
	dc.FillRect(rcPreview, &black);

	DrawArch(&dc);
	rcPreview.InflateRect(1,1);
	dc.FrameRect(rcPreview, &grey);

	ValidateRect(rcPreview);
}


void CArchDlg::OnArchPreview() 
{
	// 
	// Build preview.
	//
	UpdateData(TRUE);
	InvalidateRect(NULL);
	UpdateWindow();
}


CArchDlg::~CArchDlg()
{
}


void CArchDlg::DrawArch(CDC* pDC)
{
	float fOuterPoints[ARC_MAX_POINTS][2];
	float fInnerPoints[ARC_MAX_POINTS][2];

	float fScaleX;
	float fScaleY;

	CPen m_hPen, *pOldPen;

	m_hPen.CreatePen(PS_SOLID, 1, RGB(255,255,255));

	pOldPen = pDC->SelectObject(&m_hPen);

	CRect rcItem;
	m_cPreview.GetWindowRect(&rcItem);
	ScreenToClient(&rcItem);

	CPoint pt;
	pt.x = rcItem.left + rcItem.Width()  / 2;
	pt.y = rcItem.top  + rcItem.Height() / 2;

	if (bmaxs[0] - bmins[0])
		fScaleX = rcItem.Width()/(bmaxs[0] - bmins[0]);
	else
		fScaleX = 1.0f;
	
	if (bmaxs[1] - bmins[1])
		fScaleY = rcItem.Height()/(bmaxs[1] - bmins[1]);
	else
		fScaleY = 1.0f;

	int iSides, iWallWidth;
	float fArc, fStartAngle;

	fArc = m_fArc;
	fStartAngle = m_fAngle;
	iSides = m_iSides;
	iWallWidth = m_iWallWidth;


	MakeArc(bmins[0], bmins[1],
		bmaxs[0], bmaxs[1], iSides,
		fStartAngle, fArc, fOuterPoints);

	MakeArc(bmins[0] + iWallWidth, 
		bmins[1] + iWallWidth, 
		bmaxs[0] - iWallWidth, 
		bmaxs[1] - iWallWidth, iSides, 
		fStartAngle, fArc, fInnerPoints);


	// check wall width - if it's half or more of the total,
	//  set the inner poinst to the center point of the box
	//  and turn off the CreateSouthFace flag
	
	BOOL bCreateSouthFace = TRUE;
	float fCenter[3];
	for (int i = 0; i < 3; i++)
		fCenter[i] = (bmins[i] + bmaxs[i])/2.0;
	
	if((iWallWidth*2+8)  >= (bmaxs[0] - bmins[0]) ||
		(iWallWidth*2+8) >= (bmaxs[1] - bmins[1]))
	{
		for(int i = 0; i < ARC_MAX_POINTS; i++)
		{
			fInnerPoints[i][0] = fCenter[0];
			fInnerPoints[i][1] = fCenter[1];
		}
		bCreateSouthFace = FALSE;
	}

	for (int i = 0; i < iSides; i++)
	{
		int iNextPoint = i+1;
		if (iNextPoint >= iSides + 1)
			iNextPoint = 0;

		Vector points[4];	

		points[0][0] = fOuterPoints[i][0];
		points[0][1] = fOuterPoints[i][1];

		points[1][0] = fOuterPoints[iNextPoint][0];
		points[1][1] = fOuterPoints[iNextPoint][1];

		points[2][0] = fInnerPoints[iNextPoint][0];
		points[2][1] = fInnerPoints[iNextPoint][1];

		points[3][0] = fInnerPoints[i][0];
		points[3][1] = fInnerPoints[i][1];

		for (int j = 0; j < 4; j++)
		{
			points[j][0] = fScaleX * (points[j][0] - fCenter[0]);
			points[j][1] = fScaleY * (points[j][1] - fCenter[1]);
		}

		pDC->MoveTo(pt.x + (int)points[0][0], pt.y - (int)points[0][1]);
		pDC->LineTo(pt.x + (int)points[1][0], pt.y - (int)points[1][1]);
		pDC->LineTo(pt.x + (int)points[2][0], pt.y - (int)points[2][1]);
		pDC->LineTo(pt.x + (int)points[3][0], pt.y - (int)points[3][1]);
	}

	// Draw the bbox
	/*CPen hPen2;
	hPen2.CreatePen(PS_SOLID, 1, RGB(255,255,0));

	pDC->SelectObject(&hPen2);

	pDC->MoveTo(pt.x + (int)((bmins[0] - fCenter[0])*fScaleX), pt.y - (int)((bmins[1] - fCenter[1])*fScaleY));
	pDC->LineTo(pt.x + (int)((bmins[0] - fCenter[0])*fScaleX), pt.y - (int)((bmaxs[1] - fCenter[1])*fScaleY));
	pDC->LineTo(pt.x + (int)((bmaxs[0] - fCenter[0])*fScaleX), pt.y - (int)((bmaxs[1] - fCenter[1])*fScaleY));
	pDC->LineTo(pt.x + (int)((bmaxs[0] - fCenter[0])*fScaleX), pt.y - (int)((bmins[1] - fCenter[1])*fScaleY));
	*/
	pDC->SelectObject(pOldPen);

}
