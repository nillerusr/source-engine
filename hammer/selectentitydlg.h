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

#pragma once


#include "MapClass.h"	// dvs: For CMapObjectList


class CSelectEntityDlg : public CDialog
{
	// Construction
	public:
		CSelectEntityDlg(const CMapObjectList *pList,
			CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
		//{{AFX_DATA(CSelectEntityDlg)
		enum { IDD = IDD_SELECTENTITY };
		CListBox	m_cEntities;
		//}}AFX_DATA

		const CMapObjectList *m_pEntityList;
		CMapEntity *m_pFinalEntity;

	// Overrides
		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(CSelectEntityDlg)
		protected:
		virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
		//}}AFX_VIRTUAL

	// Implementation
	protected:

		// Generated message map functions
		//{{AFX_MSG(CSelectEntityDlg)
		afx_msg void OnSelchangeEntities();
		virtual BOOL OnInitDialog();
		virtual void OnOK();
		//}}AFX_MSG
		DECLARE_MESSAGE_MAP()
};

