//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vkeyeditDoc.cpp : implementation of the CVkeyeditDoc class
//

#include "stdafx.h"
#include "vkeyedit.h"

#include "vkeyeditDoc.h"


/////////////////////////////////////////////////////////////////////////////
// CVkeyeditDoc

IMPLEMENT_DYNCREATE(CVkeyeditDoc, CDocument)

BEGIN_MESSAGE_MAP(CVkeyeditDoc, CDocument)
	//{{AFX_MSG_MAP(CVkeyeditDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditDoc construction/destruction

CVkeyeditDoc::CVkeyeditDoc()
{
	// TODO: add one-time construction code here
	m_pKeyValues = NULL;
}

CVkeyeditDoc::~CVkeyeditDoc()
{
	m_pKeyValues->deleteThis();
}

BOOL CVkeyeditDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CVkeyeditDoc serialization

void CVkeyeditDoc::Serialize(CArchive& ar)
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
// CVkeyeditDoc diagnostics

#ifdef _DEBUG
void CVkeyeditDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CVkeyeditDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CVkeyeditDoc commands

BOOL CVkeyeditDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;

	if ( m_pKeyValues != NULL )
	{
		m_pKeyValues->deleteThis();
	}

	const char *filename = lpszPathName;

	m_pKeyValues = new KeyValues( filename );

	m_pKeyValues->LoadFromFile( g_pFileSystem, filename );

	UpdateAllViews( NULL, 1, (CObject*)m_pKeyValues );
 	
	// TODO: Add your specialized creation code here
	
	return TRUE;
}
