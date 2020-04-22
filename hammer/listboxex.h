//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ListBoxEx.h : header file
//

#ifndef _LISTBOXEX_H
#define _LISTBOXEX_H

#include <afxtempl.h>

enum
{
	lbdBool,
	lbdString,
	lbdInteger
};

enum
{
	lbeYesNo,
	lbeOnOff,
	lbeString,
	lbeInteger,
	lbeTexture,
	lbeChoices
};

typedef struct
{
	char szCaption[128];	// field named
	char *pszSaveCaption;	// save caption to this, if supported
	int iDataType;			// how to display	(lbdxxx)
	int iEditType;			// how to edit		(lbexxx)
	
	int iDataValue;			// int value for integer/bool fields
	char szDataString[128];	// str value for string fields
	
	PVOID pSaveTo;
	const char * pszHelp;	// help text		(ptr or NULL)
	int iRangeMin;			// ranged value min	(-1 if no range)
	int iRangeMax;			// ranged value max	(-1 if no range)
	CStringArray* pChoices;	// choices, if lbdChoices

} LBEXTITEMSTRUCT;

// listboxex styles
enum
{
	LBES_EDITCAPTIONS = 0x01
};

/////////////////////////////////////////////////////////////////////////////
// CListBoxEx window

class CListBoxEx : public CListBox
{
// Construction
public:
	CListBoxEx();

// Attributes
public:
	int iCaptionWidthUnits;
	int iCaptionWidthPixels;
	int iItemHeight;
	CEdit EditCtrl;
	CComboBox ComboCtrl;
	DWORD dwStyle;
	BOOL bControlActive;
	BOOL bIgnoreChange;
	int iControlItem;

	void SetStyle(DWORD dwStyle);

	void AddItem(char * pszCaption, int iEditType, PVOID pData,
		int iRangeMin = -1, int iRangeMax = -1, const char * pszHelp = NULL);
	void SetItemChoices(int iItem, CStringArray * pChoices, 
		int iDefaultChoice = 0);
	void GetItemText(int iItem, char *pszText);
	void CreateEditControl();
	void CreateComboControl();
	void DestroyControls();

	CArray<LBEXTITEMSTRUCT, LBEXTITEMSTRUCT&> Items;
	unsigned nItems;

// Operations
public:
	BOOL bActivateOnRelease;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CListBoxEx)
	public:
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual int CompareItem(LPCOMPAREITEMSTRUCT lpCompareItemStruct);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CListBoxEx();

	// Generated message map functions
protected:
	//{{AFX_MSG(CListBoxEx)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnSelchange();
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

#endif // _LISTBOXEX_H
