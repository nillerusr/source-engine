//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements an owner-draw combo box containing the names and thumbnail
//			images of textures. The textures are gotten from the global texture
//			manager object, and are filtered by texture format.
//
//=============================================================================

#include "stdafx.h"
#include "GameConfig.h"
#include "IEditorTexture.h"
#include "TextureBox.h"
#include "TextureSystem.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


BEGIN_MESSAGE_MAP(CTextureBox, CComboBox)
	//{{AFX_MSG_MAP(CTextureBox)
	ON_WM_ERASEBKGND()
	ON_MESSAGE(CB_SELECTSTRING, OnSelectString)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTextureBox::CTextureBox(void)
{
	bFirstMeasure = TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTextureBox::~CTextureBox(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lpCompareItemStruct - 
// Output : int
//-----------------------------------------------------------------------------
int CTextureBox::CompareItem(LPCOMPAREITEMSTRUCT lpCompareItemStruct) 
{
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lpDeleteItemStruct - 
//-----------------------------------------------------------------------------
void CTextureBox::DeleteItem(LPDELETEITEMSTRUCT lpDeleteItemStruct) 
{
	CComboBox::DeleteItem(lpDeleteItemStruct);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lpDrawItemStruct - 
//-----------------------------------------------------------------------------
void CTextureBox::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
//	if(!pGD)
//		return;

	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);
	dc.SaveDC();

	RECT& r = lpDrawItemStruct->rcItem;

	int iFontHeight = dc.GetTextExtent("J", 1).cy;

	if (lpDrawItemStruct->itemID != -1)
	{
		IEditorTexture *pTex = (IEditorTexture *)GetItemDataPtr(lpDrawItemStruct->itemID);
		dc.SetROP2(R2_COPYPEN);
		CPalette *pOldPalette = NULL;

		if (pTex != NULL)
		{
			pTex->Load();

			pOldPalette = dc.SelectPalette(pTex->HasPalette() ? pTex->GetPalette() : g_pGameConfig->Palette, FALSE);
			dc.RealizePalette();
		}

		COLORREF dwBackColor = RGB(255,255,255);
		COLORREF dwForeColor = RGB(0,0,0);

		if (lpDrawItemStruct->itemState & ODS_SELECTED)
		{
			dwBackColor = GetSysColor(COLOR_HIGHLIGHT);
			dwForeColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
		}

		// draw background
		CBrush brush;
		brush.CreateSolidBrush(dwBackColor);
		dc.FillRect(&r, &brush);

		if (pTex == NULL)
		{
			// separator
			dc.SelectStockObject(BLACK_PEN);
			dc.MoveTo(r.left, r.top+5);
			dc.LineTo(r.right, r.top+5);
		}
		else
		{
			char szName[MAX_PATH];
			int iLen = pTex->GetShortName(szName);

			// when we get here, we are drawing a regular graphic. we
			//  check the size of the rectangle - if it's > 32 (just
			//	a nice number), we're drawing an item in the drop list.
			if ((r.bottom - r.top) > 32)
			{
				DrawTexData_t DrawTexData;
				DrawTexData.nFlags = 0;

				// draw graphic
				CRect r2(r);
				r2.InflateRect(-4, -4);
				r2.right = r2.left + 64;
				pTex->Draw(&dc, r2, 0, 0, DrawTexData);

				// draw name
				dc.SetTextColor(dwForeColor);
				dc.SetBkMode(TRANSPARENT);
				dc.TextOut(r2.right + 4, r2.top + 4, szName, iLen);
				
				// draw size
				sprintf(szName, "%dx%d", pTex->GetWidth(), pTex->GetHeight());
				dc.TextOut(r2.right + 4, r2.top + 4 + iFontHeight, szName, strlen(szName));
			}
			// if it's < 32, we're drawing the item in the "closed"
			//	combo box, so just draw the name of the texture
			else
			{
				// just draw name - 
				dc.SetTextColor(dwForeColor);
				dc.SetBkMode(TRANSPARENT);
				dc.TextOut(r.left + 4, r.top + 2, szName, iLen);
			}
		}

		if (pOldPalette)
		{
			dc.SelectPalette(pOldPalette, FALSE);
		}
	}
	else if (lpDrawItemStruct->itemState & ODS_FOCUS)
	{
		dc.DrawFocusRect(&r);
	}

	dc.RestoreDC(-1);
	dc.Detach();
}


//-----------------------------------------------------------------------------
// Purpose: Adds the given texture to the MRU for this texture list.
// Input  : pTex - Texture to add. If NULL, MRU is rebuilt from scratch.
//-----------------------------------------------------------------------------
void CTextureBox::AddMRU(IEditorTexture *pTex)
{
	if (pTex != NULL)
	{
		//
		// Add the texture to the MRU set.
		//
		g_Textures.AddMRU(pTex);

		//
		// Update the list contents based on the new MRU set.
		//
		RebuildMRU();

		//
		// Select the newly added texture, which should be at index 0.
		//
		SetCurSel(0);

		Invalidate();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Rebuilds the MRU for this texture combo box.
//-----------------------------------------------------------------------------
void CTextureBox::RebuildMRU(void)
{
	SetRedraw(FALSE);

	int nCurSel = GetCurSel();

	//
	// Delete current MRUs from list.
	//
	int nItems = GetCount();
	int nDelimiterIndex = 0;
	while (nDelimiterIndex < nItems)
	{
		//
		// The first item with a NULL item data pointer is the MRU delimiter.
		//
		if (GetItemDataPtr(nDelimiterIndex) == NULL)
		{
			break;
		}

		nDelimiterIndex++;
	}

	//
	// If the MRU delimiter was found, delete everything before it.
	//
	if (nDelimiterIndex != nItems)
	{
		do
		{
			DeleteString(0);
		} while(nDelimiterIndex--);
	}

	//
	// Add each texture from the graphics MRU to this list's MRU.
	//
	int nStrCount = 0;
	int nMRUCount = g_Textures.MRUGetCount();
	for (int nMRU = 0; nMRU < nMRUCount; nMRU++)
	{
		IEditorTexture *pTex = g_Textures.MRUGet(nMRU);
		if (pTex != NULL)
		{
			char szBuf[MAX_PATH];
			pTex->GetShortName(szBuf);

			int nIndex = InsertString(nStrCount, szBuf);
			SetItemDataPtr(nIndex, pTex);
			nStrCount++;
		}
	}

	//
	// Add the MRU seperator to the list, unless the MRU was empty.
	//
	if (nStrCount > 0)
	{
		int nIndex = InsertString(nStrCount, "");
		SetItemDataPtr(nIndex, NULL);
	}

	//
	// Restore the original selection.
	//
	SetCurSel(nCurSel);
	SetRedraw(TRUE);
	Invalidate();
}


void CTextureBox::NotifyNewMaterial( IEditorTexture *pTex )
{
	char szStr[MAX_PATH];
	pTex->GetShortName( szStr );
	int iItem = AddString( szStr );
	if ( iItem != CB_ERR )
	{
		SetItemDataPtr( iItem, (void *)pTex );
	}	
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lpMeasureItemStruct - 
//-----------------------------------------------------------------------------
void CTextureBox::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
	lpMeasureItemStruct->itemWidth = 64;

	//
	// If the item data is NULL or points to an empty string, it's the separator.
	//
	char *pszText = (char *)lpMeasureItemStruct->itemData;

	if ((pszText == NULL) || (*pszText == '\0'))
	{
		lpMeasureItemStruct->itemHeight = 9;
	}
	else
	{
		lpMeasureItemStruct->itemHeight = 64 + 8;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextureBox::LoadGraphicList(void)
{
	if (g_pGameConfig->GetTextureFormat() == tfNone)
	{
		return;
	}

	SetRedraw(FALSE);
	ResetContent();
	InitStorage(g_Textures.GetActiveTextureCount() + 32, sizeof(PVOID));

	//
	// Add the MRU textures to the list.
	//
	int nStrCount = 0;
	int nMRUCount = g_Textures.MRUGetCount();
	for (int nMRU = 0; nMRU < nMRUCount; nMRU++)
	{
		IEditorTexture *pTex = g_Textures.MRUGet(nMRU);
		if (pTex != NULL)
		{
			char szStr[MAX_PATH];
			pTex->GetShortName(szStr);
			AddString(szStr);
			SetItemDataPtr(nStrCount, (void *)pTex);
			nStrCount++;
		}	
	}

	//
	// Add the MRU seperator to the list, unless the MRU was empty.
	//
	if (nStrCount > 0)
	{
		AddString("");
		SetItemDataPtr(nStrCount, NULL);
		nStrCount++;
	}

	//
	// Add the rest of the textures to the list.
	//
	int nIndex = 0;
	IEditorTexture *pTex = g_Textures.EnumActiveTextures(&nIndex, g_pGameConfig->GetTextureFormat());
	while (pTex != NULL)
	{
		char szStr[MAX_PATH];
		pTex->GetShortName(szStr);
		int err = AddString(szStr);
		Assert( (err != CB_ERR) && (err != CB_ERRSPACE) );
		SetItemDataPtr(nStrCount, (void *)pTex);
		nStrCount++;

		pTex = g_Textures.EnumActiveTextures(&nIndex, g_pGameConfig->GetTextureFormat());
	}

	//
	// Hack: Select one that doesn't start with '+', '!', or '*', and doesn't have "door" in it.
	//
	SetCurSel(0);

	int nSel = GetCount();
	for (int i = 0; i < nSel; i++)
	{
		IEditorTexture *pTexSearch = (IEditorTexture *)GetItemDataPtr(i);
		if (pTexSearch != NULL)
		{
			char szName[MAX_PATH];
			pTexSearch->GetShortName(szName);

			if ((szName[0] != 0) && (szName[0] != '*') && (szName[0] != '+') && (szName[0] != '!') && (strstr(szName, "door") == NULL))
			{
				// this one is ok
				SetCurSel(i);
				break;
			}
		}
	}

	SetRedraw(TRUE);
	Invalidate();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : dwStyle - 
//			rect - 
//			pParentWnd - 
//			nID - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CTextureBox::Create(DWORD dwStyle, const RECT &rect, CWnd *pParentWnd, UINT nID)
{
	static BOOL bInitClass = TRUE;
	static LPCTSTR pszTextureBoxClass = "TextureBox";

	if(bInitClass)
	{
		bInitClass = FALSE;

		// get default class provided by MFC
		WNDCLASS wndclass;
		GetClassInfo(AfxGetInstanceHandle(), _T("COMBOBOX"), &wndclass);
		wndclass.hbrBackground = NULL;
		wndclass.lpszClassName = pszTextureBoxClass;

		AfxRegisterClass(&wndclass);
	}

	return CWnd::Create(pszTextureBoxClass, NULL, dwStyle, rect, pParentWnd, nID);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wParam - 
//			lParam - 
// Output : LRESULT
//-----------------------------------------------------------------------------
LRESULT CTextureBox::OnSelectString(WPARAM wParam, LPARAM lParam)
{
	LPCTSTR pszSelect = LPCTSTR(lParam);
	int nCount = GetCount();
	IEditorTexture *pTex;

	for(int i = wParam + 1; i < nCount; i++)
	{
		pTex = (IEditorTexture *)GetItemDataPtr(i);
		if (pTex != NULL)
		{
			char szName[MAX_PATH];
			pTex->GetShortName(szName);
			if (!stricmp(szName, pszSelect))
			{
				SetCurSel(i);
				return i;
			}
		}
	}

	return LB_ERR;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDC - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CTextureBox::OnEraseBkgnd(CDC *pDC) 
{
	CRect r;
	GetUpdateRect(r);
	pDC->SetROP2(R2_COPYPEN);
	FillRect(pDC->m_hDC, r, HBRUSH(GetStockObject(BLACK_BRUSH)));
	return TRUE;
}

