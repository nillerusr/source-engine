//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// vkeyeditDoc.h : interface of the CVkeyeditDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_VKEYEDITDOC_H__BC66A515_A393_4A6A_838B_076094A14BC0__INCLUDED_)
#define AFX_VKEYEDITDOC_H__BC66A515_A393_4A6A_838B_076094A14BC0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <KeyValues.h>


class CVkeyeditDoc : public CDocument
{
protected: // create from serialization only
	CVkeyeditDoc();
	DECLARE_DYNCREATE(CVkeyeditDoc)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVkeyeditDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CVkeyeditDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	KeyValues *m_pKeyValues;
	
// Generated message map functions
protected:
	//{{AFX_MSG(CVkeyeditDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VKEYEDITDOC_H__BC66A515_A393_4A6A_838B_076094A14BC0__INCLUDED_)
