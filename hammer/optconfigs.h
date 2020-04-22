//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef OPTCONFIGS_H
#define OPTCONFIGS_H
#pragma once


#include "AutoSelCombo.h"


class COPTConfigs : public CPropertyPage
{
	DECLARE_DYNCREATE(COPTConfigs)

public:

	COPTConfigs();
	~COPTConfigs();

	inline CGameConfig *GetCurrentConfig( void );

	// Dialog Data
	//{{AFX_DATA(COPTConfigs)
	enum { IDD = IDD_OPTIONS_CONFIGS };
	CEdit	m_cMapDir;
	CEdit	m_cGameExeDir;
	CEdit	m_cModDir;
	CEdit	m_cCordonTexture;
	CEdit	m_cDefaultTextureScale;
	CComboBox	m_cMapFormat;
	CComboBox	m_cTextureFormat;
	CAutoSelComboBox m_cDefaultPoint;
	CAutoSelComboBox m_cDefaultSolid;
	CListBox	m_cGDFiles;
	CComboBox	m_cConfigs;
	//}}AFX_DATA


	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COPTConfigs)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

protected:

	// Generated message map functions
	//{{AFX_MSG(COPTConfigs)
	afx_msg void OnEditconfigs();
	afx_msg void OnGdfileAdd();
	afx_msg void OnGdfileEdit();
	afx_msg void OnGdfileRemove();
	virtual BOOL OnInitDialog();
	afx_msg void OnBrowseexe();
	afx_msg void OnSelchangeConfigurations();
	afx_msg void OnConfigureExes();
	afx_msg void OnBrowsemapdir();
	afx_msg void OnBrowseGameExeDir(void);
	afx_msg void OnBrowseModDir(void);
	afx_msg void OnBrowseCordonTexture(void);
	afx_msg LRESULT OnSettingChange(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	BOOL BrowseForFolder(char *pszTitle, char *pszDirectory);
	void SaveInfo(CGameConfig*);
	void UpdateConfigList();
	void UpdateEntityLists();
	bool ConfigChanged(CGameConfig *pConfig);

	CGameConfig *m_pLastSelConfig;
	CGameConfig *m_pInitialSelectedConfig;
	CString m_strInitialGameDir;
};


//-----------------------------------------------------------------------------
// Purpose: get the last selected game configuration for this page
//  Output: return pointer to last selected game configuration
//-----------------------------------------------------------------------------
inline CGameConfig *COPTConfigs::GetCurrentConfig( void )
{
	return m_pLastSelConfig;
}


#endif // OPTCONFIGS_H
