//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ANGLEBOX_H
#define ANGLEBOX_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"


class CAngleCombo;


class CAngleBox : public CWnd
{
public:
	CAngleBox();
	virtual ~CAngleBox();

	bool GetAngles(QAngle &vecAngles);
	char *GetAngles(char *szAngles);

	void SetAngles(const QAngle &vecAngles, bool bRedraw = true);
	void SetAngles(const char *szAngles, bool bRedraw = true);
	
	void SetDifferent(bool bDifferent, bool bRedraw = true);
	inline void SetEditControl(CAngleCombo *pEdit);

	char *GetAngleEditText(char *szBuf);

	void Enable(bool bEnable);
	void Show(bool bShow);

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAngleBox)
	public:
	//}}AFX_VIRTUAL

protected:

	void UpdateAngleEditText(void);
	void UpdateLine(void);
	void DrawAngleLine(CDC *pDC);

	bool m_bDifferent;			// Set to true when we have multiselected objects with different angles.

	CDC m_DragDC;				// When dragging w/mouse.
	CPoint m_ptClientCenter;
	bool m_bDragging;

	QAngle m_vecAngles;

	CAngleCombo *m_pEdit;		// The linked angle edit box, NULL if none.

	// Generated message map functions
	//{{AFX_MSG(CAngleBox)
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:

friend class CAngleCombo;

	// Functions called by the angle combo to set our state without notification
	// back to the angle combo.
	void SetAnglesInternal(const QAngle &vecAngles, bool bRedraw = true);
	void SetDifferentInternal(bool bDifferent, bool bRedraw = true);
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAngleBox::SetEditControl(CAngleCombo *pEdit)
{
	m_pEdit = pEdit;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAngleCombo : public CComboBox
{
public:

	CAngleCombo();

	inline void SetAngleBox(CAngleBox *pBox);

protected:

	void UpdateAngleBox(char *szText);

	// Generated message map functions
	//{{AFX_MSG(CAngleBox)
	afx_msg void OnChangeAngleEdit();
	afx_msg void OnSelChangeAngleEdit();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:

friend class CAngleBox;

	void SetAnglesInternal(const char *szAngles);

	CAngleBox *m_pBox;		// The linked angle box control.
	bool m_bEnableUpdate;	// Whether we forward update notifications to the linked angle box control.
};


void CAngleCombo::SetAngleBox(CAngleBox *pBox)
{
	m_pBox = pBox;
}


#endif // ANGLEBOX_H
