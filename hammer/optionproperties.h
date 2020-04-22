//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// OptionProperties.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionProperties

#include "OPTGeneral.h"
#include "OPTView2D.h"
#include "OPTView3D.h"
#include "OPTTextures.h"
#include "OPTConfigs.h"
#include "OPTBuild.h"

class COptionProperties : public CPropertySheet
{
	DECLARE_DYNAMIC(COptionProperties)

// Construction
public:
	COptionProperties(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	COptionProperties(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

// Attributes
public:
	COPTGeneral General;
	COPTView2D View2D;
	COPTView3D View3D;
	COPTTextures Textures;
	COPTConfigs Configs;
	COPTBuild Build;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COptionProperties)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~COptionProperties();
	void DoStandardInit();

	// Generated message map functions
protected:
	//{{AFX_MSG(COptionProperties)
	afx_msg void OnClose();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
