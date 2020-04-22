//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef OP_MODEL_H
#define OP_MODEL_H
#ifdef _WIN32
#pragma once
#endif

#include "ObjectPage.h"
#include "FilteredComboBox.h"

class GDclass;
class CMapStudioModel;


class COP_Model : public CObjectPage, public CFilteredComboBox::ICallbacks
{
	friend class CFilteredModelSequenceComboBox;
	DECLARE_DYNCREATE(COP_Model)

public:
	COP_Model();
	~COP_Model();

	virtual bool SaveData(void);
	virtual void UpdateData( int Mode, PVOID pData, bool bCanEdit );
	void UpdateForClass(LPCTSTR pszClass);

	void OnSelChangeSequence( int iSequence );


// Implementation of CFilteredComboBox::ICallbacks.

	virtual void OnTextChanged( const char *pText );

// Variables.

	GDclass *pObjClass;

	//{{AFX_DATA(COP_Model)
	enum { IDD = IDD_OBJPAGE_MODEL };
	CFilteredComboBox m_ComboSequence;
	CSliderCtrl m_ScrollBarFrame;
	//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DATA

	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COP_Model)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

private:
	void SetReadOnly( bool bReadOnly );

protected:

	CMapStudioModel *GetModelHelper(void);
	void UpdateFrameText(float flFrame);

	// Generated message map functions
	//{{AFX_MSG(COP_Model)
	virtual BOOL OnInitDialog();
	virtual BOOL OnKillActive(); 
	virtual BOOL OnSetActive();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	BOOL m_bOldAnimatedModels;
};

#endif // OP_MODEL_H
