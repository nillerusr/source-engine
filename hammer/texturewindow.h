//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TEXTUREWINDOW_H
#define TEXTUREWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include "IEditorTexture.h"
#include "utlvector.h"


struct TextureWindowTex_t
{
	IEditorTexture *pTex;
	int nUsageCount;
};


class TextureWindowTexList : public CUtlVector<TextureWindowTex_t>
{
public:

	inline int Find(IEditorTexture *pTex)
	{
		for (int i = 0; i < Count(); i++)
		{
			if (Element(i).pTex == pTex)
			{
				return i;
			}
		}	

		return -1;
	}
};


class CTextureWindow : public CWnd
{
public:
	CTextureWindow();
	virtual ~CTextureWindow();

	void Create(CWnd *pParentWnd, RECT& rect);

	struct TWENUMPOS
	{
		IEditorTexture *pTex;
		RECT texrect;
		int cur_x, cur_y;
		RECT clientrect;
		int largest_y;
		int iTexIndex;
		int nUsageCount;		// The number of times this texture is used in the map. Only set in "Used Textures Only" mode.
	};

	enum
	{
		TYPEFILTER_OPAQUE = 0x1,
		TYPEFILTER_TRANSLUCENT = 0x2,
		TYPEFILTER_SELFILLUM = 0x4,
		TYPEFILTER_ENVMASK = 0x8,

		TYPEFILTER_ALL = (TYPEFILTER_OPAQUE|TYPEFILTER_TRANSLUCENT|TYPEFILTER_SELFILLUM|TYPEFILTER_ENVMASK),
	};

	void EnableUpdate(bool bEnable);
	void UpdateScrollSizes();
	BOOL EnumTexturePositions(TWENUMPOS *pTE, BOOL bStart = FALSE);
	void SetDisplaySize(int iSize);
	void HighlightCurTexture(CDC *pDC = NULL);
	void SetNameFilter(LPCTSTR pszFilter);
	void SetKeywords(const char *pszKeywords);
	void SetTextureFormat(TEXTUREFORMAT eTextureFormat);
	void SelectTexture(LPCTSTR pszTexture, BOOL bAllowRedraw = TRUE);
	void SetSpecificList(TextureWindowTexList *pList);
	void SetTypeFilter( int filter, bool enable );
	void ShowErrors( bool enable )	{ m_bShowErrors = true; }

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTextureWindow)
	//}}AFX_VIRTUAL

	char szCurTexture[128];

protected:

	bool MatchKeywords(const char *pszSearch, char **pszKeyword, int nKeywords);

	int total_x;
	int total_y;
	int iDisplaySize;
	int iTexNameCharWidth;
	BOOL bFirstPaint;
	CFont TexFont;
	TextureWindowTexList *m_pSpecificList;
	CRect rectHighlight;
	int	m_nTypeFilter;

	char m_szFilter[128];			// Name filter, space, comma, or semicolon delimited.
	int m_nFilters;					// The number of names that were parsed out of the name filter.
	char *m_Filters[64];			// The individual name filters.

	char m_szKeywords[128];			// Keyword filter, space, comma, or semicolon delimited.
	int m_nKeywords;				// The number of keywords that were parsed out of the name filter.
	char *m_Keyword[64];			// The individual keywords.

	bool m_bEnableUpdate;			// Locks GUI updates to control repaints.
	bool m_bShowErrors;

	TEXTUREFORMAT m_eTextureFormat;

	//{{AFX_MSG(CTextureWindow)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


#endif // TEXTUREWINDOW_H
