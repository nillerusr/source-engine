//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef TEXTUREBOX_H
#define TEXTUREBOX_H
#pragma once

#include "IEditorTexture.h"

class CTextureBox : public CComboBox
{
// Construction
public:
	CTextureBox();
	virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);

// Attributes
public:
	BOOL bFirstMeasure;

// Operations
public:
	void LoadGraphicList();
	void AddMRU(IEditorTexture *pTex);
	void RebuildMRU(void);
	void NotifyNewMaterial( IEditorTexture *pTex );

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTextureBox)
	public:
	virtual int CompareItem(LPCOMPAREITEMSTRUCT lpCompareItemStruct);
	virtual void DeleteItem(LPDELETEITEMSTRUCT lpDeleteItemStruct);
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTextureBox();

	// Generated message map functions
protected:
	//{{AFX_MSG(CTextureBox)
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg LRESULT OnSelectString(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


#endif // TEXTUREBOX_H

