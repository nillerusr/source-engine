//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef TITLEWND_H
#define TITLEWND_H
#pragma once


class CTitleWnd : public CWnd
{
	public:

		static CTitleWnd *CreateTitleWnd(CWnd *pwndParent, UINT uID);

		void SetTitle(LPCTSTR pszTitle);

	private:

		char m_szTitle[256];
		static CFont m_FontNormal;
		static CFont m_FontActive;

	protected:

		CTitleWnd(void);

		void OnMouseButton(void);

		bool m_bMouseOver;
		bool m_bMenuOpen;

		afx_msg void OnPaint();
		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnMouseMove(UINT nFlags, CPoint point);
		afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);

		DECLARE_MESSAGE_MAP()
};



#endif // TITLEWND_H
