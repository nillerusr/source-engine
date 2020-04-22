//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TEXTUREBAR_H
#define TEXTUREBAR_H
#ifdef _WIN32
#pragma once
#endif


#include "TextureBox.h"
#include "IEditorTexture.h"
#include "wndTex.h"
#include "ControlBarIDs.h"
#include "HammerBar.h"


class IEditorTexture;


class CTextureBar : public CHammerBar
{
	public:

		CTextureBar() : CHammerBar() {}
		BOOL Create(CWnd *pParentWnd, int IDD = IDD_TEXTUREBAR, int iBarID = IDCB_TEXTUREBAR);

		void NotifyGraphicsChanged(void);
		void NotifyNewMaterial( IEditorTexture *pTexture );
		void SelectTexture(LPCSTR pszTextureName);

	protected:

		void UpdateTexture(void);

		IEditorTexture *m_pCurTex;
		CTextureBox m_TextureList;
		CComboBox m_TextureGroupList;
		wndTex m_TexturePic;

		afx_msg void UpdateControl(CCmdUI *);
		afx_msg void OnBrowse(void);
		afx_msg void OnChangeTextureGroup(void);
		afx_msg void OnReplace(void);
		afx_msg void OnUpdateTexname(void);
		afx_msg void OnWindowPosChanged(WINDOWPOS *pPos);
		virtual afx_msg void OnSelChangeTexture(void);

		DECLARE_MESSAGE_MAP()
};


#endif // TEXTUREBAR_H
