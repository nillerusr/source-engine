//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MDICLIENTWND_H
#define MDICLIENTWND_H
#ifdef _WIN32
#pragma once
#endif


class CMDIClientWnd : public CWnd
{
public:

	CMDIClientWnd();
	virtual ~CMDIClientWnd();

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMDIClientWnd)
	//}}AFX_VIRTUAL

protected:

	//{{AFX_MSG(CMDIClientWnd)
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

};


#endif // MDICLIENTWND_H
