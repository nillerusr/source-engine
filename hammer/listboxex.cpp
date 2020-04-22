//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// ListBoxEx.cpp : implementation file
//

#include "stdafx.h"
#include "hammer.h"
#include "ListBoxEx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

/////////////////////////////////////////////////////////////////////////////
// CListBoxEx

CListBoxEx::CListBoxEx()
{
	Items.SetSize(16);
	nItems = 0;
	iItemHeight = -1;
	dwStyle = 0;
	bControlActive = FALSE;
	bIgnoreChange = FALSE;
}

CListBoxEx::~CListBoxEx()
{
}


BEGIN_MESSAGE_MAP(CListBoxEx, CListBox)
	//{{AFX_MSG_MAP(CListBoxEx)
	ON_WM_LBUTTONDOWN()
	ON_CONTROL_REFLECT(LBN_SELCHANGE, OnSelchange)
	ON_WM_LBUTTONUP()
	ON_WM_CHAR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CListBoxEx message handlers

void CListBoxEx::SetStyle(DWORD dwStyle_)
{
	this->dwStyle = dwStyle_;
}

void CListBoxEx::AddItem(char *pszCaption, int iEditType, PVOID pData,
		int iRangeMin, int iRangeMax, const char * pszHelp)
{
	LBEXTITEMSTRUCT lbis;

	memset(&lbis, 0, sizeof lbis);

	strcpy(lbis.szCaption, pszCaption);
	lbis.pszSaveCaption = pszCaption;
	lbis.iEditType = iEditType;
	
	switch(iEditType)
	{
	case lbeYesNo:
	case lbeOnOff:
		lbis.iDataType = lbdBool;
		lbis.iDataValue = PINT(pData)[0];
		break;
	case lbeInteger:
		lbis.iDataType = lbdInteger;
		lbis.iDataValue = PINT(pData)[0];
		break;
	case lbeTexture:
	case lbeString:
		lbis.iDataType = lbdString;
		strcpy(lbis.szDataString, LPCTSTR(pData));
		break;
	case lbeChoices:
		lbis.iDataType = lbdString;
		lbis.pChoices = NULL;
		break;
	}

	lbis.pSaveTo = pData;

	lbis.iRangeMin = iRangeMin;
	lbis.iRangeMax = iRangeMax;
	lbis.pszHelp = pszHelp;

	Items[nItems++] = lbis;
	
	AddString("");	// trick windows! muahaha
}

void CListBoxEx::SetItemChoices(int iItem, CStringArray * pChoices, 
								int iDefaultChoice)
{
	LBEXTITEMSTRUCT& lbis = Items[iItem];

	lbis.pChoices = pChoices;
	lbis.iDataValue = iDefaultChoice;
	V_strcpy_safe( lbis.szDataString, pChoices->GetAt( iDefaultChoice ) );
}

void CListBoxEx::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
	if(iItemHeight == -1)
	{
		CDC *pDC = GetDC();
		TEXTMETRIC tm;
		pDC->GetOutputTextMetrics(&tm);
		iItemHeight = tm.tmHeight + 4;

		CRect r;
		GetClientRect(r);
		iCaptionWidthPixels = r.Width() / 2;

		ReleaseDC(pDC);
	}

	lpMeasureItemStruct->itemHeight = iItemHeight;
}

void CListBoxEx::GetItemText(int iItem, char *pszText)
{
	LBEXTITEMSTRUCT& lbis = Items[iItem];

	switch(lbis.iDataType)
	{
	case lbdBool:
		if(lbis.iEditType == lbeYesNo)
			strcpy(pszText, lbis.iDataValue ? "Yes" : "No");
		else
			strcpy(pszText, lbis.iDataValue ? "On" : "Off");
		break;
	case lbdString:
		strcpy(pszText, lbis.szDataString);
		break;
	case lbdInteger:
		ltoa(lbis.iDataValue, pszText, 10);
		break;
	}
}

void CListBoxEx::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);
	dc.SaveDC();

	RECT& r = lpDrawItemStruct->rcItem;

	if(lpDrawItemStruct->itemID != -1 && 
		(lpDrawItemStruct->itemAction == ODA_DRAWENTIRE ||
		lpDrawItemStruct->itemAction == ODA_SELECT))
	{
		LBEXTITEMSTRUCT& item = Items[lpDrawItemStruct->itemID];
		dc.SetROP2(R2_COPYPEN);

		int iBackIndex = COLOR_WINDOW;
		int iForeIndex = COLOR_WINDOWTEXT;
		BOOL bDrawCaptionOnly = FALSE;

		if(lpDrawItemStruct->itemAction == ODA_SELECT &&
			(lpDrawItemStruct->itemState & ODS_SELECTED))
		{
			iBackIndex = COLOR_HIGHLIGHT;
			iForeIndex = COLOR_HIGHLIGHTTEXT;
			bDrawCaptionOnly = item.iDataType != lbdBool ? TRUE : FALSE;
		}

		// draw background
		CBrush brush;
		brush.CreateSolidBrush(GetSysColor(iBackIndex));

		if(0)//!bDrawCaptionOnly)
			dc.FillRect(&r, &brush);
		else
		{
			CRect r2(&r);
			r2.right = iCaptionWidthPixels;
			dc.FillRect(r2, &brush);
		}

		// first, draw text
		dc.SetTextColor(GetSysColor(iForeIndex));
		dc.SetBkColor(GetSysColor(iBackIndex));
		dc.TextOut(r.left + 1, r.top+ 1, item.szCaption, 
			strlen(item.szCaption));
		
		if(!bDrawCaptionOnly)
		{
			// draw value ..
			char szText[128];
			GetItemText(lpDrawItemStruct->itemID, szText);
			dc.TextOut(r.left + iCaptionWidthPixels + 1, r.top + 1, 
				szText, strlen(szText));
		}

		// draw border.
		CPen pen(PS_SOLID, 1, RGB(200, 200, 200));
		dc.SelectObject(pen);
		dc.MoveTo(r.left, r.bottom-1);
		dc.LineTo(r.right, r.bottom-1);
		dc.MoveTo(r.left + iCaptionWidthPixels, r.top);
		dc.LineTo(r.left + iCaptionWidthPixels, r.bottom-1);
	}
	else if(lpDrawItemStruct->itemAction == ODA_FOCUS)
	{
		dc.DrawFocusRect(&r);
	}

	dc.RestoreDC(-1);
}

void CListBoxEx::OnLButtonDown(UINT nFlags, CPoint point) 
{
	BOOL bOutside;
	int iItem = ItemFromPoint(point, bOutside);
	LBEXTITEMSTRUCT& lbis = Items[iItem];

	if(lbis.iDataType == lbdBool)
	{
		// toggle bool field
		lbis.iDataValue = !lbis.iDataValue;
	}

	CListBox::OnLButtonDown(nFlags, point);
}

int CListBoxEx::CompareItem(LPCOMPAREITEMSTRUCT lpCompareItemStruct) 
{
	return 0;
}

void CListBoxEx::CreateEditControl()
{
	if(IsWindow(EditCtrl.m_hWnd))
		return;

	// create edit control 
	int iItem = GetCurSel();
	if(iItem == LB_ERR)
		return;

	LBEXTITEMSTRUCT& lbis = Items[iItem];

	if(lbis.iEditType != lbeString &&
	   lbis.iEditType != lbeInteger &&
	   lbis.iEditType != lbeTexture)
	   return;
	
	CRect r;
	GetItemRect(iItem, r);
	r.InflateRect(-1, -1);
	r.left += iCaptionWidthPixels;

	// create edit ctrl
	EditCtrl.Create(ES_LEFT | ES_LOWERCASE | WS_VISIBLE | WS_TABSTOP, r, this, 
		IDC_EDITPARAMETER);
	// set font
	HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	if (hFont == NULL)
		hFont = (HFONT)GetStockObject(ANSI_VAR_FONT);
	EditCtrl.SendMessage(WM_SETFONT, (WPARAM)hFont);
	
	// set current text in edit ctrl
	char szBuf[128];
	GetItemText(iItem, szBuf);
	EditCtrl.SetWindowText(szBuf);

	EditCtrl.SetForegroundWindow();
	EditCtrl.SetSel(0, -1);

	bControlActive = TRUE;
	iControlItem = iItem;
}

void CListBoxEx::CreateComboControl()
{
	if(IsWindow(ComboCtrl.m_hWnd))
		return;

	// create edit control 
	int iItem = GetCurSel();
	if(iItem == LB_ERR)
		return;

	LBEXTITEMSTRUCT& lbis = Items[iItem];

	if(lbis.iEditType != lbeChoices)
	   return;
	
	CRect r;
	GetItemRect(iItem, r);
	r.left += iCaptionWidthPixels;
	r.bottom += 80;

	// create combo ctrl
	ComboCtrl.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | 
		WS_TABSTOP, r, this, IDC_EDITPARAMETER);
	// set font
	HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	if (hFont == NULL)
		hFont = (HFONT)GetStockObject(ANSI_VAR_FONT);
	ComboCtrl.SendMessage(WM_SETFONT, (WPARAM)hFont);
	
	// add strings to combo ctrl
	CStringArray * pChoices = lbis.pChoices;
	Assert(pChoices);

	for(int i = 0; i < pChoices->GetSize(); i++)
		ComboCtrl.AddString(pChoices->GetAt(i));

	// set current selection in combo ctrl
	ComboCtrl.SetCurSel(lbis.iDataValue);
	ComboCtrl.SetForegroundWindow();

	bControlActive = TRUE;
	iControlItem = iItem;
}

void CListBoxEx::DestroyControls()
{
	// get rid of window if there is one
	if(::IsWindow(EditCtrl.m_hWnd))
	{
		EditCtrl.DestroyWindow();
	}
	if(::IsWindow(ComboCtrl.m_hWnd))
	{
		ComboCtrl.DestroyWindow();
	}

	bControlActive = FALSE;
}

void CListBoxEx::OnSelchange() 
{
	if(bControlActive)
	{
		// on combobox/edit controls, save string back to data
		LBEXTITEMSTRUCT& lbis = Items[iControlItem];

		if(lbis.iEditType == lbeChoices)
		{
			ComboCtrl.GetLBText(ComboCtrl.GetCurSel(), lbis.szDataString);
			lbis.iDataValue = ComboCtrl.GetCurSel();
		}
		else if(lbis.iDataType == lbdString)
		{
			EditCtrl.GetWindowText(lbis.szDataString, 128);
		}
		else if(lbis.iDataType == lbdInteger)
		{
			EditCtrl.GetWindowText(lbis.szDataString, 128);
			lbis.iDataValue = atoi(lbis.szDataString);
		}
	}

	DestroyControls();

	int iCurItem = GetCurSel();
	LBEXTITEMSTRUCT& lbis = Items[iCurItem];

	if(lbis.iEditType == lbeChoices)
	{
		CreateComboControl();
	}
	else
	{
		CreateEditControl();
	}
}

void CListBoxEx::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CListBox::OnLButtonUp(nFlags, point);

	int iItem = GetCurSel();
	if(iItem == LB_ERR)
		return;

	LBEXTITEMSTRUCT& lbis = Items[iItem];

	if(lbis.iDataType == lbdBool)
	{
		lbis.iDataValue = !lbis.iDataValue;
		Invalidate();
	}
}

void CListBoxEx::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	CListBox::OnChar(nChar, nRepCnt, nFlags);

	return;

	int iItem = GetCurSel();
	if(iItem == LB_ERR)
		return;

	LBEXTITEMSTRUCT& lbis = Items[iItem];

	switch(nChar)
	{
	case VK_RETURN:
		if(!(nFlags & 0x8000))
			break;
		if(lbis.iDataType == lbdBool)
		{
			// toggle bool field
			lbis.iDataValue = !lbis.iDataValue;

			Invalidate();
		}
		else if(lbis.iEditType == lbeChoices)
		{
			CreateComboControl();
		}
		else if(lbis.iEditType == lbeString ||
			lbis.iEditType == lbeInteger ||
			lbis.iEditType == lbeTexture)
		{
			CreateEditControl();
		}
		break;
	}
}
