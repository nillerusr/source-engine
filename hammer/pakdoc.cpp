//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PakDoc.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "PakDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPakDoc

IMPLEMENT_DYNCREATE(CPakDoc, CDocument)

CPakDoc::CPakDoc()
{
}

BOOL CPakDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;
	return TRUE;
}

CPakDoc::~CPakDoc()
{
}


BEGIN_MESSAGE_MAP(CPakDoc, CDocument)
	//{{AFX_MSG_MAP(CPakDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPakDoc diagnostics

#ifdef _DEBUG
void CPakDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CPakDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CPakDoc serialization

void CPakDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CPakDoc commands
