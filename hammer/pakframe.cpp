//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PakFrame.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "PakFrame.h"
#include "PakDoc.h"
#include "PakViewDirec.h"
#include "PakViewFiles.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPakFrame

IMPLEMENT_DYNCREATE(CPakFrame, CMDIChildWnd)

CPakFrame::CPakFrame()
{
}

CPakFrame::~CPakFrame()
{
}


BEGIN_MESSAGE_MAP(CPakFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CPakFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPakFrame message handlers

BOOL CPakFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{
	if(!SplitWnd.CreateStatic(this, 1, 2))
		return FALSE;

	RECT r;
	GetClientRect(&r);

	// create panes
	SplitWnd.CreateView(0, 0, RUNTIME_CLASS(CPakViewDirec), 
		CSize(150, r.bottom), pContext);
	SplitWnd.CreateView(0, 1, RUNTIME_CLASS(CPakViewFiles), 
		CSize(300, r.bottom), pContext);

	return TRUE;
}
