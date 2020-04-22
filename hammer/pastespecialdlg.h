//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "BoundBox.h"

class CPasteSpecialDlg : public CDialog
{
// Construction
public:
	CPasteSpecialDlg(CWnd* pParent, BoundBox*);
	void SaveToIni();

// Dialog Data
	//{{AFX_DATA(CPasteSpecialDlg)
	enum { IDD = IDD_WC_PASTESPECIAL };
	int		m_iCopies;
	BOOL	m_bGroup;
	int		m_iOffsetX;
	int		m_iOffsetY;
	int		m_iOffsetZ;
	float	m_fRotateX;
	float	m_fRotateZ;
	float	m_fRotateY;
	BOOL	m_bCenterOriginal;
	BOOL	m_bMakeEntityNamesUnique;
	BOOL	m_bAddPrefix;
	CString	m_strPrefix;
	//}}AFX_DATA

	BoundBox ObjectsBox;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPasteSpecialDlg)
	protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void GetOffset(int iAxis, int iEditCtrl);

	// Generated message map functions
	//{{AFX_MSG(CPasteSpecialDlg)
	afx_msg void OnGetoffsetx();
	afx_msg void OnGetoffsety();
	afx_msg void OnGetoffsetz();
	afx_msg void OnCheckUncheckAddPrefix();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
