//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a window for displaying a single texture within.
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef WNDTEX_H
#define WNDTEX_H
#pragma once


class wndTex : public CStatic
{
	public:

		wndTex() : m_pTexture(NULL)
		{
		}

		void SetTexture(IEditorTexture *pTex);

		inline IEditorTexture *GetTexture(void)
		{
			return(m_pTexture);
		}

	protected:

		IEditorTexture *m_pTexture;

		afx_msg void OnPaint();
		
		DECLARE_MESSAGE_MAP();
};


#endif // WNDTEX_H
