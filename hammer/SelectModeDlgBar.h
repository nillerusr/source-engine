//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SELECTMODEDLGBAR_H
#define SELECTMODEDLGBAR_H
#ifdef _WIN32
#pragma once
#endif


#include "resource.h"
#include "GroupList.h"
#include "HammerBar.h"


class CSelectModeDlgBar : public CHammerBar
{
public:
	BOOL Create(CWnd *pParentWnd);

private:
	//{{AFX_DATA(CFilterControl)
	enum { IDD = IDD_SELECT_MODE_BAR };
	//}}AFX_DATA

protected:
	//{{AFX_MSG(CFilterControl)
	afx_msg void OnGroups();
	afx_msg void OnObjects();
	afx_msg void OnSolids();
	afx_msg void UpdateControlGroups(CCmdUI *pCmdUI);
	afx_msg void UpdateControlObjects(CCmdUI *pCmdUI);
	afx_msg void UpdateControlSolids(CCmdUI *pCmdUI);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

#endif // SELECTMODEDLGBAR_H
