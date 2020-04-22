//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ENTITYHELPDLG_H
#define ENTITYHELPDLG_H
#ifdef _WIN32
#pragma once
#endif

#include "Resource.h"


class GDclass;
class CRichEditCtrlEx;


class CEntityHelpDlg : public CDialog
{
	public:

		// Construction
		CEntityHelpDlg(CWnd *pwndParent = NULL);
		~CEntityHelpDlg(void);

		//{{AFX_DATA(CEntityHelpDlg)
		enum { IDD = IDD_ENTITY_HELP };
		CRichEditCtrlEx *m_pHelpText;
		//}}AFX_DATA

		static void ShowEntityHelpDialog(void);
		static void SetEditGameClass(GDclass *pClass);

	protected:
		
		void UpdateClass(GDclass *pClass);

		int GetTextWidth(const char *pszText, CDC *pDC = NULL);
		int GetMaxVariableWidth(GDclass *pClass);

		void UpdateHelp(void);

		// ClassWizard generate virtual function overrides
		//{{AFX_VIRTUAL(CEntityHelpDlg)
		virtual void DoDataExchange(CDataExchange *pDX);
		//}}AFX_VIRTUAL

		// Generated message map functions
		//{{AFX_MSG(CEntityHelpDlg)
		virtual BOOL OnInitDialog(void);
		virtual void OnDestroy(void);
		virtual void OnClose(void);
		afx_msg void OnSize( UINT nType, int cx, int cy );
		//}}AFX_MSG

		GDclass *m_pClass;

		DECLARE_MESSAGE_MAP()
};

#endif // ENTITYHELPDLG_H
