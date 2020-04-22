//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef EDITGROUPS_H
#define EDITGROUPS_H
#ifdef _WIN32
#pragma once
#endif

#include "GroupList.h"
#include "mapdoc.h"


class CVisGroup;


class CColorBox : public CStatic
{
	public:
		void SetColor(COLORREF, BOOL);
		COLORREF GetColor() { return m_c; }
		
		afx_msg void OnPaint();

	private:
		COLORREF m_c;

	protected:
		DECLARE_MESSAGE_MAP()
};


class CEditGroups : public CDialog
{
	// Construction
	public:
		CEditGroups(CWnd* pParent = NULL);   // standard constructor

	// Dialog Data
		//{{AFX_DATA(CEditGroups)
		enum { IDD = IDD_GROUPS };
		CEdit	m_cName;
		CGroupList m_cGroupList;
		//}}AFX_DATA

		CColorBox m_cColorBox;

	// Overrides
		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(CEditGroups)
		public:
		virtual BOOL DestroyWindow();
		protected:
		virtual BOOL OnInitDialog();
		virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
		//}}AFX_VIRTUAL

	// Implementation
	protected:

		void UpdateGroupList();
		void UpdateControlsForVisGroup(CVisGroup *pVisGroup);

		// Generated message map functions
		//{{AFX_MSG(CEditGroups)
		afx_msg void OnColor();
		afx_msg void OnChangeName();
		afx_msg void OnNew();
		afx_msg void OnRemove();
		afx_msg LRESULT OnSelChangeGroupList(WPARAM wParam, LPARAM lParam);
		afx_msg void OnClose();
		//}}AFX_MSG
		DECLARE_MESSAGE_MAP()
};

#endif // EDITGROUPS_H
