//========= Copyright Valve Corporation, All rights reserved. ============//

#ifndef MAPDIFFDLG_H
#define MAPDIFFDLG_H
#ifdef _WIN32
#pragma once
#endif

#include "mapdoc.h"

// MapDiffDlg dialog

class CMapDiffDlg : public CDialog
{
public:
	static void MapDiff(CWnd *pwndParent, CMapDoc *p_CurrentMap);

private:

	CMapDiffDlg(CWnd* pParent  = NULL);   // standard constructor
	enum { IDD = IDD_DIFFMAP };

	BOOL	m_bCheckSimilar;
	CEdit	m_mapName;
	

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	virtual void OnOK();
	afx_msg void OnDestroy();

	
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedSimilarcheck();
	afx_msg void OnBnClickedMapbrowse();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
};

#endif //MAPDIFFDLG_H