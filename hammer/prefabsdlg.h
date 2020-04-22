//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PrefabsDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPrefabsDlg dialog

#include "Prefabs.h"

class CPrefabsDlg : public CDialog
{
// Construction
public:
	CPrefabsDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CPrefabsDlg)
	enum { IDD = IDD_PREFABS };
	CListCtrl	m_Objects;
	CEdit	m_ObjectNotes;
	CStatic	m_LibraryNotes;
	CComboBox	m_Libraries;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPrefabsDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	int iCurObject;
	int iCurLibrary;
	BOOL bCurLibraryModified;
	void SetCurObject(int);
	int GetCurObject() { return iCurObject; }
	CPrefabLibrary * GetCurrentLibrary(int *piSel = NULL);
	CPrefab * GetCurrentObject(int *piSel = NULL);
	void AddToObjectList(CPrefab *pPrefab, int iItem = -1, 
		BOOL bReplace = FALSE);
	CImageList PrefabImages;
	void EditObjectInfo();
	void EditObjectData();

	afx_msg BOOL HandleEditObjectPopup(UINT nID);

	// Generated message map functions
	//{{AFX_MSG(CPrefabsDlg)
	afx_msg void OnAddlibrary();
	afx_msg void OnAddobject();
	afx_msg void OnEditlibrary();
	afx_msg void OnEditobject();
	afx_msg void OnExportobject();
	afx_msg void OnSelchangeLibraries();
	afx_msg void OnRemovelibrary();
	afx_msg void OnRemoveobject();
	virtual BOOL OnInitDialog();
	afx_msg void OnItemchangedObjects(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEndlabeleditObjects(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
