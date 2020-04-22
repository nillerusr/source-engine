//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef LPRVWINDOW_H
#define LPRVWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"


class CLightingPreviewResultsWindow : public CWnd
{
public:
	CLightingPreviewResultsWindow();
	virtual ~CLightingPreviewResultsWindow();

	void Create(CWnd *pParentWnd );

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLightingPreviewResultsWindow)
	//}}AFX_VIRTUAL


protected:

	//{{AFX_MSG(CLightingPreviewResultsWindow)
 	afx_msg void OnPaint();
	afx_msg void OnClose(); 
// 	afx_msg void OnSize(UINT nType, int cx, int cy);
// 	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
// 	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
// 	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
// 	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
// 	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
// 	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
// 	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


#endif // LPRVWINDOW_H
