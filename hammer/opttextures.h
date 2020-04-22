//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef OPTTEXTURES_H
#define OPTTEXTURES_H
#pragma once


class COPTTextures : public CPropertyPage
{
	DECLARE_DYNCREATE( COPTTextures )

public:

	//=========================================================================
	//
	// Construction/Deconstruction
	//
	COPTTextures();
	~COPTTextures();

	//=========================================================================
	//
	// Dialog Data
	//
	//{{AFX_DATA(COPTTextures)
	enum { IDD = IDD_OPTIONS_TEXTURES };
	CListBox	m_TextureFiles;
	CSliderCtrl	m_cBrightness;
	CListBox    m_MaterialExcludeList;
	//}}AFX_DATA

	//=========================================================================
	//
	// Overrides
	// ClassWizard generate virtual function overrides
	//
	//{{AFX_VIRTUAL(COPTTextures)
	public:
	virtual BOOL OnApply();
	BOOL OnSetActive( void );
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	BOOL			m_bDeleted;
	CGameConfig		*m_pMaterialConfig;				// copy of the current gaming config

	BOOL BrowseForFolder( char *pszTitle, char *pszDirectory );
	void MaterialExcludeUpdate( void );

	//=========================================================================
	//
	// Generated message map functions
	//
	//{{AFX_MSG(COPTTextures)
	virtual BOOL OnInitDialog();
	afx_msg void OnExtract();
	afx_msg void OnAddtexfile();
	afx_msg void OnRemovetexfile();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnAddtexfile2();
	afx_msg void OnMaterialExcludeAdd( void );
	afx_msg void OnMaterialExcludeRemove( void );
	afx_msg void OnMaterialExcludeListSel( void );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // OPTTEXTURES_H
