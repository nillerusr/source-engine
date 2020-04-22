//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "TextureBar.h"
#include "ControlBarIDs.h"
#include "StockSolids.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "GlobalFunctions.h"
#include "History.h"
#include "IEditorTexture.h"
#include "Options.h"
#include "ReplaceTexDlg.h"
#include "TextureBrowser.h"
#include "TextureSystem.h"
#include "Selection.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


BEGIN_MESSAGE_MAP(CTextureBar, CHammerBar)
	ON_CBN_SELCHANGE(IDC_TEXTURES, OnSelChangeTexture)
	ON_UPDATE_COMMAND_UI(IDC_TEXTURES, UpdateControl)
	ON_CBN_SELCHANGE(IDC_TEXTUREGROUPS, OnChangeTextureGroup)
	ON_UPDATE_COMMAND_UI(IDC_TEXTUREGROUPS, UpdateControl)
	ON_BN_CLICKED(IDC_BROWSE, OnBrowse)
	ON_UPDATE_COMMAND_UI(IDC_BROWSE, UpdateControl)
	ON_BN_CLICKED(IDC_REPLACE, OnReplace)
	ON_UPDATE_COMMAND_UI(IDC_REPLACE, UpdateControl)
	ON_WM_WINDOWPOSCHANGED()
END_MESSAGE_MAP()


static char szDefaultTexture[128];

static char szNullTexture[128] = {"editor/obsolete"};


//-----------------------------------------------------------------------------
// Purpose: 
// Output : LPCTSTR
//-----------------------------------------------------------------------------
void SetDefaultTextureName( const char *szTexName )
{
	int length = strlen( szTexName );
	Assert( length < 128 );

	strcpy( szDefaultTexture, szTexName );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : LPCTSTR
//-----------------------------------------------------------------------------
LPCTSTR GetDefaultTextureName(void)
{
	return szDefaultTexture;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : LPCTSTR
//-----------------------------------------------------------------------------

LPCTSTR GetNullTextureName()
{
	return szNullTexture;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParentWnd - 
//			IDD - 
//			iBarID - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CTextureBar::Create(CWnd *pParentWnd, int IDD, int iBarID)
{
	m_pCurTex = NULL;

	if (!CHammerBar::Create(pParentWnd, IDD, CBRS_RIGHT, iBarID))
	{
		return(FALSE);
	}

	SetWindowText("Textures");

	// set up controls
	m_TextureGroupList.SubclassDlgItem(IDC_TEXTUREGROUPS, this);
	m_TextureList.SubclassDlgItem(IDC_TEXTURES, this);
	m_TexturePic.SubclassDlgItem(IDC_TEXTUREPIC, this);

	NotifyGraphicsChanged();

	UpdateTexture();

	return(TRUE);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextureBar::NotifyGraphicsChanged()
{
	if (!IsWindow(m_hWnd))
	{
		return;
	}

	// load groups into group list
	CString str;
	int iCurSel = m_TextureGroupList.GetCurSel();
	if (iCurSel != LB_ERR)
	{
		m_TextureGroupList.GetLBText(iCurSel, str);
	}

	m_TextureGroupList.SetRedraw(FALSE);
	m_TextureGroupList.ResetContent();
	m_TextureGroupList.AddString("All Textures");

	int nCount = g_Textures.GroupsGetCount();
	if (nCount > 1)
	{
		//
		// Skip first group ("All Textures").
		//
		for (int i = 1; i < nCount; i++)
		{
			CTextureGroup *pGroup = g_Textures.GroupsGet(i);
			if (pGroup->GetTextureFormat() == g_pGameConfig->GetTextureFormat())
			{
				const char *p = strstr(pGroup->GetName(), "textures\\");
				if (p)
				{
					p += strlen("textures\\");
				}
				else
				{
					p = pGroup->GetName();
				}

				m_TextureGroupList.AddString(p);
			}
		}
	}
	m_TextureGroupList.SetRedraw(TRUE);

	if (iCurSel == LB_ERR || m_TextureGroupList.SelectString(-1, str) == LB_ERR)
	{
		m_TextureGroupList.SetCurSel(0);
	}
	
	m_TextureGroupList.Invalidate();

	char szName[MAX_PATH];
	m_TextureGroupList.GetLBText(m_TextureGroupList.GetCurSel(), szName);
	g_Textures.SetActiveGroup(szName);

	//
	// This is called when the loaded graphics list is changed,
	// or on first init by this->Create().
	//
	m_TextureList.LoadGraphicList();
	UpdateTexture();
}


void CTextureBar::NotifyNewMaterial( IEditorTexture *pTex )
{
	m_TextureList.NotifyNewMaterial( pTex );
}


//-----------------------------------------------------------------------------
// Purpose: Disables the dialog controls when there's no active document.
// Input  : pCmdUI - Interface to control being updated.
//-----------------------------------------------------------------------------
void CTextureBar::UpdateControl(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(CMapDoc::GetActiveMapDoc() ? TRUE : FALSE);
}


//-----------------------------------------------------------------------------
// Purpose: Handles user-initiated selection changes. Updates the control state
//			and adds the selected texture to the MRU list.
//-----------------------------------------------------------------------------
void CTextureBar::OnSelChangeTexture(void)
{
	UpdateTexture();

	if (m_pCurTex != NULL)
	{
		m_TextureList.AddMRU(m_pCurTex);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Updates the m_pTexture data member based on the current selection.
//			Also updates the window text and the texture picture.
//-----------------------------------------------------------------------------
void CTextureBar::UpdateTexture(void)
{
	int iSel = m_TextureList.GetCurSel();

	if (iSel == LB_ERR)
	{
		m_TexturePic.SetTexture(NULL);
		m_pCurTex = NULL;
		return;
	}

	m_pCurTex = (IEditorTexture *)m_TextureList.GetItemDataPtr(iSel);
	m_TexturePic.SetTexture(m_pCurTex);

	if (m_pCurTex)
	{
		// Make sure the current material is loaded..
		m_pCurTex->Load();
		char szBuf[128];
		sprintf(szBuf, "%dx%d", m_pCurTex->GetWidth(), m_pCurTex->GetHeight());
		GetDlgItem(IDC_TEXTURESIZE)->SetWindowText(szBuf);
		m_pCurTex->GetShortName(szDefaultTexture);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextureBar::OnUpdateTexname(void)
{
	// get texture window and set texture in there
	CString strTex;
	m_TextureList.GetWindowText(strTex);
	IEditorTexture *pTex = g_Textures.FindActiveTexture(strTex);
	m_TexturePic.SetTexture(pTex);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextureBar::OnChangeTextureGroup(void)
{
	int iGroup = m_TextureGroupList.GetCurSel();

	//
	// Set the active texture group by name.
	//
	char szName[MAX_PATH];
	m_TextureGroupList.GetLBText(iGroup, szName);
	g_Textures.SetActiveGroup(szName);

	//
	// Refresh the texture list contents.
	//
	m_TextureList.LoadGraphicList();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTextureBar::OnBrowse(void)
{
	CTextureBrowser *pBrowser = GetMainWnd()->pTextureBrowser;

	int iSel = m_TextureList.GetCurSel();

	if (iSel != LB_ERR)
	{
		IEditorTexture *pTex = (IEditorTexture *)m_TextureList.GetItemDataPtr(iSel);
		if (pTex != NULL)
		{
			char sz[128];
	
			pTex->GetShortName(sz);
			pBrowser->SetInitialTexture(sz);
		}
	}

	if (pBrowser->DoModal() == IDOK)
	{
		IEditorTexture *pTex = g_Textures.FindActiveTexture(pBrowser->m_cTextureWindow.szCurTexture);
		if (pTex != NULL)
		{
			int iCount = m_TextureList.GetCount();
			for (int i = 0; i < iCount; i++)
			{
				if (pTex == (IEditorTexture *)m_TextureList.GetItemDataPtr(i))
				{
					m_TextureList.SetCurSel(i);
					UpdateTexture();
					m_TextureList.AddMRU(pTex);
					break;
				}

			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handles being shown or hidden.
// Input  : pPos - Information about show or hide state.
//-----------------------------------------------------------------------------
void CTextureBar::OnWindowPosChanged(WINDOWPOS *pPos)
{
	if (GetMainWnd() != NULL)
	{
		//
		// Rebuild our MRU list if we are being shown, because it may
		// have changed since we were shown last.
		//
		if (pPos->flags & SWP_SHOWWINDOW)
		{
			m_TextureList.RebuildMRU();
			UpdateTexture();
		}
	}

	CHammerBar::OnWindowPosChanged(pPos);
}


//-----------------------------------------------------------------------------
// Purpose: Invokes the texture replace dialog.
//-----------------------------------------------------------------------------
void CTextureBar::OnReplace(void)
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc)
	{
		return;
	}

	CReplaceTexDlg dlg(pDoc->GetSelection()->GetCount());
	dlg.m_strFind = GetDefaultTextureName();
	if (dlg.DoModal() != IDOK)
	{
		return;
	}
	
	GetHistory()->MarkUndoPosition(pDoc->GetSelection()->GetList(), "Replace Textures");
	dlg.DoReplaceTextures();
}


//-----------------------------------------------------------------------------
// Purpose: Selects a texture by name.
// Input  : pszTextureName - Texture name to select.
//-----------------------------------------------------------------------------
void CTextureBar::SelectTexture(LPCSTR pszTextureName)
{
	int nIndex = m_TextureList.SelectString(-1, pszTextureName);

	//
	// If the texture is not in the list, add it to the list.
	//
	if (nIndex == LB_ERR)
	{
		IEditorTexture *pTex = g_Textures.FindActiveTexture(pszTextureName);
		if (pTex != NULL)
		{
			nIndex = m_TextureList.AddString(pszTextureName);
			m_TextureList.SetItemDataPtr(nIndex, pTex);
			m_TextureList.SetCurSel(nIndex);
		}
	}

	UpdateTexture();

	if (nIndex != LB_ERR)
	{
		IEditorTexture *pTex = (IEditorTexture *)m_TextureList.GetItemDataPtr(nIndex);
		m_TextureList.AddMRU(pTex);
	}

}


//-----------------------------------------------------------------------------
// This class renders a given IEditorTexture in its OnPaint handler. It is used in the
// texture Find/Replace dialog, the Face Properties dialog, and the texture bar.
//-----------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(wndTex, CStatic)
	ON_WM_PAINT()
END_MESSAGE_MAP()


//-----------------------------------------------------------------------------
// Purpose: Sets the texture to render in the window.
// Input  : pTex - Texture to render when painting this window.
//-----------------------------------------------------------------------------
void wndTex::SetTexture(IEditorTexture *pTex)
{
	m_pTexture = pTex;
	Invalidate();	
}


//-----------------------------------------------------------------------------
// Purpose: Paints the texture image (if any) in the window. If not, just fills
//			with gray.
//-----------------------------------------------------------------------------
void wndTex::OnPaint(void)
{
	// texturewindow.cpp:
	CPaintDC dc(this);

	CRect r;
	GetClientRect(r);

	if (!m_pTexture)
	{
		FillRect(dc.m_hDC, r, HBRUSH(GetStockObject(BLACK_BRUSH)));
		return;
	}

	m_pTexture->Load();

	DrawTexData_t DrawTexData;
	DrawTexData.nFlags = drawResizeAlways;

	dc.SelectPalette(m_pTexture->HasPalette() ? m_pTexture->GetPalette() : g_pGameConfig->Palette, FALSE);
	dc.RealizePalette();
	m_pTexture->Draw(&dc, r, 0, 0, DrawTexData);
}

