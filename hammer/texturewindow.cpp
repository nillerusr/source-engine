//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#pragma warning(push, 1)
#pragma warning(disable:4701 4702 4530)
#include <fstream>
#pragma warning(pop)
#include "hammer.h"
#include "TextureWindow.h"
#include "TextureBrowser.h"
#include "CustomMessages.h"
#include "IEditorTexture.h"
#include "GameConfig.h"
#include "GlobalFunctions.h"
#include "TextureSystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


const DWORD NO_FILE_FILTER = 0xFFFFFFF0L;
const int iPadding = 4;
const int iTexNameFontHeight = 7;
const int iTexIconHeight = 12;


BEGIN_MESSAGE_MAP(CTextureWindow, CWnd)
	//{{AFX_MSG_MAP(CTextureWindow)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_KEYDOWN()
	ON_WM_MOUSEWHEEL()
	ON_WM_CHAR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CTextureWindow::CTextureWindow(void)
{
	bFirstPaint = TRUE;

	m_szFilter[0] = '\0';
	m_nFilters = 0;

	m_szKeywords[0] = '\0';
	m_nKeywords = 0;

	m_pSpecificList = NULL;
	szCurTexture[0] = '\0';

	m_eTextureFormat = g_pGameConfig->GetTextureFormat();

	m_bEnableUpdate = true;
	m_nTypeFilter = ~0;
	m_bShowErrors = true;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CTextureWindow::~CTextureWindow(void)
{
} 


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParentWnd - 
//			rect - 
//-----------------------------------------------------------------------------
void CTextureWindow::Create(CWnd *pParentWnd, RECT& rect)
{
	static CString TextureWndClassName;

	iDisplaySize = 64;

	if(TextureWndClassName.IsEmpty())
	{
		// create class
		TextureWndClassName = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | 
			CS_VREDRAW, LoadCursor(NULL, IDC_ARROW), 
			(HBRUSH) GetStockObject(BLACK_BRUSH), 
			AfxGetApp()->LoadIcon(IDI_TEXTUREWINDOW));
	}

	CWnd::Create(TextureWndClassName, "TextureBrowserWindow",
		SS_SUNKEN | WS_TABSTOP | WS_CHILD | WS_VSCROLL | WS_HSCROLL, 
		rect, pParentWnd, IDC_TEXTUREWINDOW);

	UpdateScrollSizes();

	// create font
	if(!TexFont.m_hObject)
		TexFont.CreatePointFont(iTexNameFontHeight * 10, "Courier New");

	CDC *pDC = GetDC();
	pDC->SelectObject(&TexFont);
	pDC->GetCharWidth('A', 'A', &iTexNameCharWidth);
	ReleaseDC(pDC);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bEnable - 
//-----------------------------------------------------------------------------
void CTextureWindow::EnableUpdate(bool bEnable)
{
	m_bEnableUpdate = bEnable;
}


//-----------------------------------------------------------------------------
// Purpose: Searches for all of the keywords in an array of keywords within
//			a given search string, case-insensitive.
// Input  : pszSearch - String to search for keywords within.
//			pszKeyword - Array of pointers to keywords.
//			nKeywords - Number of keywords in the array.
// Output : Returns true if all keywords were found, false otherwise.
//-----------------------------------------------------------------------------
bool CTextureWindow::MatchKeywords(const char *pszSearch, char **pszKeyword, int nKeywords)
{
	if (nKeywords != 0)
	{
		for (int i = 0; i < nKeywords; i++)
		{
			if (Q_stristr(pszSearch, pszKeyword[i]) == NULL)
			{
				return(false);
			}
		}
	}

	return(true);
}

//-----------------------------------------------------------------------------
// Changes type filter bits
//-----------------------------------------------------------------------------

void CTextureWindow::SetTypeFilter( int filter, bool enable )
{
	if (enable)
		m_nTypeFilter |= filter;
	else
		m_nTypeFilter &= ~filter;

	if (m_bEnableUpdate)
	{
		UpdateScrollSizes();
		SelectTexture(szCurTexture, false);
		
		if (IsWindow(m_hWnd))
		{
			Invalidate();
			UpdateWindow();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTE - 
//			bStart - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CTextureWindow::EnumTexturePositions(TWENUMPOS *pTE, BOOL bStart)
{
	RECT &texrect = pTE->texrect;

	if (bStart)
	{
		pTE->cur_x = iPadding;
		pTE->cur_y = iPadding;
		pTE->largest_y = 0;
		pTE->iTexIndex = 0;

		if (IsWindow(m_hWnd))
		{
			GetClientRect(&pTE->clientrect);
		}

		SetRect(&texrect, 0, 0, 0, 0);
	}
	
	bool bFound = false;
	
	do
	{
		pTE->pTex = g_Textures.EnumActiveTextures(&pTE->iTexIndex, m_eTextureFormat);

		if (pTE->pTex == NULL)
			continue;

		bFound = false;

		// If we are iterating a specific list of textures, make sure it is in the list.
		// dvs: inefficient, the specific list should control the loop, not act as a filter
		if (m_pSpecificList != NULL)
		{
			int nIndex = m_pSpecificList->Find(pTE->pTex);
			if (nIndex == -1)
				continue;
	
			pTE->nUsageCount = m_pSpecificList->Element(nIndex).nUsageCount;
		}

		// Filter by texture name.
		char szTemp[MAX_PATH];
		pTE->pTex->GetShortName(szTemp);
		if (MatchKeywords(szTemp, m_Filters, m_nFilters))
		{
			//
			// Filter by keywords.
			//

			// NOTE: Try not to access the material here when finding the position
			// because it causes the materials to be cached (slow!!)
			if (m_nKeywords)
			{
				pTE->pTex->GetKeywords(szTemp);
				if (MatchKeywords(szTemp, m_Keyword, m_nKeywords))
				{
					bFound = true;
				}
			}
			else
			{
				bFound = true;
			}
		}

		// Filter based on opacity, etc.
		// NOTE: Try not to access the material here when finding the position
		// because it causes the materials to be cached (slow!!)
		if (bFound && ((m_nTypeFilter & TYPEFILTER_ALL) != TYPEFILTER_ALL))
		{
			IMaterial* pMaterial = pTE->pTex->GetMaterial();
			if (pMaterial)
			{
				bFound = false;
				if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_SELFILLUM ) )
				{
					if (m_nTypeFilter & TYPEFILTER_SELFILLUM)
						bFound = true;
				}
				if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_BASEALPHAENVMAPMASK ) )
				{
					if (m_nTypeFilter & TYPEFILTER_ENVMASK)
						bFound = true;
				}

				if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_TRANSLUCENT ) )
				{
					if (m_nTypeFilter & TYPEFILTER_TRANSLUCENT)
						bFound = true;
				}
				else
				{
					if (m_nTypeFilter & TYPEFILTER_OPAQUE)
						bFound = true;
				}
			}
		}

		// Blow off zero-size materials, but only if they've been loaded...
		// Otherwise we have to cache everything which will take forever...
		if ( bFound && pTE->pTex->IsLoaded() )
		{
			if ((pTE->pTex->GetWidth() == 0) || (pTE->pTex->GetHeight() == 0))
			{
				bFound = false;
			}
		}
			
	} while ((pTE->pTex != NULL) && (!bFound));

	if ((!bFound) || (pTE->pTex == NULL))
	{
		return(FALSE);
	}

doresize:

	SetRect( &texrect, pTE->cur_x, pTE->cur_y,
		pTE->cur_x + iDisplaySize,
		pTE->cur_y + iDisplaySize );

	// if we've got one texture on this row already, and this one goes out of 
	// the client area, jump to the next row. we want to have at least one texture on 
	// each row, or we will sit in an infinite loop.
	if(pTE->cur_x > iPadding && texrect.right > pTE->clientrect.right)
	{
		pTE->cur_x = iPadding;
		pTE->cur_y = pTE->largest_y + iPadding;
		goto doresize;
	}

	texrect.bottom += (8 + iTexNameFontHeight + iTexIconHeight);

	if(texrect.bottom > pTE->largest_y)
		pTE->largest_y = texrect.bottom;

	// update cur_x
	pTE->cur_x = texrect.right + iPadding;

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Sets the dimensions of each texture within the texture window.
// Input  : iSize - 32 to display as 32 x 32 textures.
//					64 to display as 64 x 64 textures.
//					128 to display as 128 x 128 textures.
//					512 to display as 512 x 512 textures
//-----------------------------------------------------------------------------
void CTextureWindow::SetDisplaySize(int iSize)
{
	iDisplaySize = iSize;
	UpdateScrollSizes();
	SelectTexture(szCurTexture, FALSE);
	RedrawWindow();
}


//-----------------------------------------------------------------------------
// Purpose: Sets the name filter that is used to filter the texture window contents.
// Input  : pszFilter - Space, comma, or semicolon delimited names to filter against.
//-----------------------------------------------------------------------------
void CTextureWindow::SetNameFilter(LPCTSTR pszFilter)
{
	if (m_bEnableUpdate)
	{
		// kill highlight
		HighlightCurTexture();
	}

	// set filter
	strcpy(m_szFilter, pszFilter);
	strupr(m_szFilter);

	// delimit the filter
	m_nFilters = 0;
	char *p = strtok(m_szFilter, " ,;");
	while (p != NULL)
	{	
		m_Filters[m_nFilters++] = p;
		p = strtok(NULL, " ,;");
	}

	if (m_bEnableUpdate)
	{
		UpdateScrollSizes();
		SelectTexture(szCurTexture, false);
		
		if (IsWindow(m_hWnd))
		{
			Invalidate();
			UpdateWindow();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the keywords that are used to filter the texture window contents.
// Input  : pszFilter - Space, comma, or semicolon delimited keywords to filter against.
//-----------------------------------------------------------------------------
void CTextureWindow::SetKeywords(LPCTSTR pszKeywords)
{
	if (m_bEnableUpdate)
	{
		// kill highlight
		HighlightCurTexture();
	}

	// set keyword filter
	strcpy(m_szKeywords, pszKeywords);
	strupr(m_szKeywords);

	// delimit the filter
	m_nKeywords = 0;
	char *p = strtok(m_szKeywords, " ,;");
	while (p != NULL)
	{	
		m_Keyword[m_nKeywords++] = p;
		p = strtok(NULL, " ,;");
	}

	if (m_bEnableUpdate)
	{
		UpdateScrollSizes();
		SelectTexture(szCurTexture, false);
		
		if (IsWindow(m_hWnd))
		{
			Invalidate();
			UpdateWindow();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextureWindow::UpdateScrollSizes(void)
{
	TWENUMPOS TE;

	total_x = total_y = 0;

	if(EnumTexturePositions(&TE, TRUE))
		do {
			if(TE.texrect.right > total_x)
				total_x = TE.texrect.right;
			if(TE.texrect.bottom > total_y)
				total_y = TE.texrect.bottom;
		} while(EnumTexturePositions(&TE));

	// update total_x and total_y
	total_x += iPadding;
	total_y += iPadding;

	SCROLLINFO si;
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nPos = 0;
	si.nMax = total_x;
	si.nPage = TE.clientrect.right;
	SetScrollInfo(SB_HORZ, &si, TRUE);

	si.nMax = total_y;
	si.nPage = TE.clientrect.bottom;
	SetScrollInfo(SB_VERT, &si, TRUE);

	char szbuf[100];
	sprintf(szbuf, "Size = %d %d\n", total_y, TE.clientrect.bottom);
	TRACE0(szbuf);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextureWindow::OnPaint(void)
{
	CPaintDC dc(this); // device context for painting

	// setup font
	dc.SelectObject(&TexFont);
	dc.SetTextColor(RGB(255, 255, 255));
	//dc.SetBkColor(RGB(0,0,0));
	dc.SetBkMode(TRANSPARENT);

	CRect clientrect;
	GetClientRect(clientrect);

	dc.SetWindowOrg(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));

	TWENUMPOS TE;
	BOOL bNotDone = EnumTexturePositions(&TE, TRUE);
	BOOL bFoundHighlight = FALSE;
	rectHighlight.left = -1;
	BOOL bFirst = TRUE;
	char szDrawTexture[128];
	char szFirstDrawnTexture[128];

	while (bNotDone)
	{
		TE.pTex->GetShortName(szDrawTexture);

		if (!strcmpi(szCurTexture, szDrawTexture))
		{
			rectHighlight = TE.texrect;
			rectHighlight.InflateRect(2, 4);
			bFoundHighlight = TRUE;
		}

		if (dc.RectVisible(&TE.texrect))
		{
			// ensure loaded
			TE.pTex->Load();

			CPalette *pOld = dc.SelectPalette(TE.pTex->HasPalette() ? TE.pTex->GetPalette() : g_pGameConfig->Palette, FALSE);
			dc.RealizePalette();

			int flags = drawCaption | drawIcons;
			if (m_bShowErrors)
				flags |= drawErrors;

			DrawTexData_t DrawTexData;
			DrawTexData.nFlags = flags | (m_pSpecificList ? drawUsageCount : 0);
			DrawTexData.nUsageCount = TE.nUsageCount;
			TE.pTex->Draw(&dc, TE.texrect, iTexNameFontHeight, iTexIconHeight, DrawTexData);

			dc.SelectPalette(pOld, FALSE);
		}

		//
		// Save the name of the first drawn texture in case we need to highlight it.
		//
		if (bFirst)
		{
			bFirst = FALSE;
			strcpy(szFirstDrawnTexture, szDrawTexture);
		}

		// next texture & position
		bNotDone = EnumTexturePositions(&TE);
	}

	if(bFoundHighlight)
	{
		HighlightCurTexture(&dc);
	}
	else
	{
		// select first texture
		SelectTexture(szFirstDrawnTexture);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nType - 
//			cx - 
//			cy - 
//-----------------------------------------------------------------------------
void CTextureWindow::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	UpdateScrollSizes();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nSBCode - 
//			nPos - 
//			pScrollBar - 
//-----------------------------------------------------------------------------
void CTextureWindow::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	int iPos = int(nPos);
	SCROLLINFO si;

	GetScrollInfo(SB_HORZ, &si);
	int iCurPos = GetScrollPos(SB_HORZ);
	int iLimit = GetScrollLimit(SB_HORZ);

	switch(nSBCode)
	{
	case SB_LINELEFT:
		iPos = -int(si.nPage / 4);
		break;
	case SB_LINERIGHT:
		iPos = int(si.nPage / 4);
		break;
	case SB_PAGELEFT:
		iPos = -int(si.nPage / 2);
		break;
	case SB_PAGERIGHT:
		iPos = int(si.nPage / 2);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		iPos -= iCurPos;
		break;
	}

	if(iCurPos + iPos < 0)
		iPos = -iCurPos;
	if(iCurPos + iPos > iLimit)
		iPos = iLimit - iCurPos;
	if(iPos)
	{
		SetScrollPos(SB_HORZ, iCurPos + iPos);
		ScrollWindow(-iPos, 0);
		UpdateWindow();
	}
	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nSBCode - 
//			nPos - 
//			pScrollBar - 
//-----------------------------------------------------------------------------
void CTextureWindow::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	SCROLLINFO si;

	GetScrollInfo(SB_VERT, &si);
	int iCurPos = GetScrollPos(SB_VERT);
	int iLimit = GetScrollLimit(SB_VERT);
	int iPos = int(si.nPos);
	
	switch(nSBCode)
	{
	case SB_LINEUP:
		iPos = -int(si.nPage / 4);
		break;
	case SB_LINEDOWN:
		iPos = int(si.nPage / 4);
		break;
	case SB_PAGEUP:
		iPos = -int(si.nPage / 2);
		break;
	case SB_PAGEDOWN:
		iPos = int(si.nPage / 2);
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		iPos = si.nTrackPos - iCurPos;
		break;
	case SB_ENDSCROLL:
		iPos = 0;
		break;
	}

	if(iCurPos + iPos < 0)
		iPos = -iCurPos;
	if(iCurPos + iPos > iLimit)
		iPos = iLimit - iCurPos;
	if(iPos)
	{
		SetScrollPos(SB_VERT, iCurPos + iPos);
		ScrollWindow(0, -iPos);
		UpdateWindow();
	}

	CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDC - 
//-----------------------------------------------------------------------------
void CTextureWindow::HighlightCurTexture(CDC *pDC)
{
	CDC dc;
	BOOL bMadeDC = FALSE;

	// nothing to draw
	if(rectHighlight.left < 0)
		return;

	if(!pDC)
	{
		dc.Attach(::GetDC(m_hWnd));
		dc.SetWindowOrg(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT));
		bMadeDC = TRUE;
		pDC = &dc;
	}

	pDC->SelectStockObject(WHITE_PEN);
	pDC->SelectStockObject(NULL_BRUSH);
	pDC->SetROP2(R2_XORPEN);
	pDC->Rectangle(rectHighlight);

	if(bMadeDC)
		::ReleaseDC(m_hWnd, dc.Detach());
}


//-----------------------------------------------------------------------------
// Purpose: Selects a given texture by name, scrolling the window if necessary
//			to make the selected texture visible.
// Input  : pszTexture - Texture to select.
//-----------------------------------------------------------------------------
void CTextureWindow::SelectTexture(LPCTSTR pszTexture, BOOL bAllowRedraw)
{
	TWENUMPOS TE;
	BOOL bNotDone = EnumTexturePositions(&TE, TRUE);
	int iTextureIndex = 0;

	IEditorTexture *pTex = g_Textures.FindActiveTexture(pszTexture);

	CRect r(100, 100, 500, 500);
	if (IsWindow(m_hWnd))
	{
		GetClientRect(r);
	}
	int iClientHeight = r.Height();

	if (pTex == NULL)
	{
		return;
	}

	while (bNotDone)
	{
		if (pTex == TE.pTex)
		{
			// found it - make sure it's visible
			if (IsWindow(m_hWnd))
			{
				int iScrollPos = GetScrollPos(SB_VERT);
				if (iScrollPos + iClientHeight < TE.texrect.top || TE.texrect.bottom < iScrollPos)
				{
					SetScrollPos(SB_VERT, TE.texrect.top);
					ScrollWindow(0, iScrollPos - TE.texrect.top);

					if (bAllowRedraw)
					{
						RedrawWindow();
					}
				}

				// first remove current highlight
				HighlightCurTexture();
			}

			pTex->GetShortName(szCurTexture);

			// highlight new texture
			if (IsWindow(m_hWnd))
			{
				rectHighlight = CRect(&TE.texrect);
				rectHighlight.InflateRect(2, 4);
				HighlightCurTexture();
			}

			GetParent()->PostMessage(TWN_SELCHANGED);

			return;
		}

		// next texture & position
		bNotDone = EnumTexturePositions(&TE);
		++iTextureIndex;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CTextureWindow::OnLButtonDown(UINT nFlags, CPoint point) 
{
	// find clicked texture
	TWENUMPOS TE;
	BOOL bNotDone = EnumTexturePositions(&TE, TRUE);
	int iTextureIndex = 0;

	int iHorzPos = GetScrollPos(SB_HORZ);
	int iVertPos = GetScrollPos(SB_VERT);

	char szNewTexture[128];

	point += CPoint(iHorzPos, iVertPos);

	while (bNotDone)
	{
		if (PtInRect(&TE.texrect, point))
		{
			TE.pTex->GetShortName(szNewTexture);
			break;
		}

		// next texture & position
		bNotDone = EnumTexturePositions(&TE);
		++iTextureIndex;
	}

	if(!bNotDone)
	{
		// no texture was hit
		return;
	}

	// first remove current highlight
	HighlightCurTexture();

	// highlight new texture
	strcpy(szCurTexture, szNewTexture);
	rectHighlight = CRect(&TE.texrect);
	rectHighlight.InflateRect(2, 4);
	HighlightCurTexture();

	// tell parent we changed selection
	GetParent()->PostMessage(TWN_SELCHANGED);

	SetFocus();

	CWnd::OnLButtonDown(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: Notifies our parent window of a double-click event.
// Input  : nFlags - 
//			point - 
//-----------------------------------------------------------------------------
void CTextureWindow::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	GetParent()->PostMessage(TWN_LBUTTONDBLCLK);
	CWnd::OnLButtonDblClk(nFlags, point);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nChar - 
//			nRepCnt - 
//			nFlags - 
//-----------------------------------------------------------------------------
void CTextureWindow::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nFlags - 
//			zDelta - 
//			point - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CTextureWindow::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
	int nScrollCode;

	if (zDelta > 0)
	{
		nScrollCode = SB_LINEUP;
	}
	else
	{
		nScrollCode = SB_LINEDOWN;
	}

	SCROLLINFO si;
	GetScrollInfo(SB_VERT, &si);
	int iCurPos = GetScrollPos(SB_VERT);
	int iLimit = GetScrollLimit(SB_VERT);
	int iPos = int(si.nPos);
	
	switch (nScrollCode)
	{
		case SB_LINEUP:
		{
			iPos = -(int)si.nPage / 4;
			break;
		}

		case SB_LINEDOWN:
		{
			iPos = si.nPage / 4;
			break;
		}
	}

	if (iCurPos + iPos < 0)
	{
		iPos = -iCurPos;
	}

	if (iCurPos + iPos > iLimit)
	{
		iPos = iLimit - iCurPos;
	}

	if (iPos)
	{
		SetScrollPos(SB_VERT, iCurPos + iPos);
		ScrollWindow(0, -iPos);
		UpdateWindow();
	}

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nChar - 
//			nRepCnt - 
//			nFlags - 
//-----------------------------------------------------------------------------
void CTextureWindow::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	CWnd::OnChar(nChar, nRepCnt, nFlags);
}


//-----------------------------------------------------------------------------
// Purpose: Sets the contents of the texture window to a specific list of textures.
// Input  : pList - Textures with which to populate the texture window.
//-----------------------------------------------------------------------------
void CTextureWindow::SetSpecificList(TextureWindowTexList *pList)
{
	m_pSpecificList = pList;

	if (m_hWnd != NULL)
	{
		UpdateScrollSizes();
		SelectTexture(szCurTexture, FALSE);
		RedrawWindow();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eTextureFormat - 
//-----------------------------------------------------------------------------
void CTextureWindow::SetTextureFormat(TEXTUREFORMAT eTextureFormat)
{
	m_eTextureFormat = eTextureFormat;
}


