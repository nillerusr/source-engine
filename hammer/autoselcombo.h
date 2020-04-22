//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef AUTOSELCOMBO_H
#define AUTOSELCOMBO_H
#ifdef _WIN32
#pragma once
#endif


class CAutoSelComboBox : public CComboBox
{
	typedef CComboBox BaseClass;

	public:

		CAutoSelComboBox(void);

		void SetTextColor(COLORREF dwColor);
		void SubclassDlgItem(UINT nID, CWnd *pParent);

	protected:

		// Called by OnEditUpdate when the user types in the edit box
		virtual void OnUpdateText(void);

	protected:

		void OnSetFocus(CWnd *pOldWnd);
		afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);
		afx_msg BOOL OnEditUpdate(void);

		DWORD m_dwTextColor;		// RGB color of edit box text.
		char m_szLastText[256];		// Last text typed by the user, for autocomplete code.
		int m_nLastSel;				// Index of last item we autoselected.

		bool m_bNotifyParent;		// Whether we allow our parent to hook our notification messages.
									// This is necessary because CControlBar-derived classes result in multiple
									// message reflections unless we disable parent notification.

		DECLARE_MESSAGE_MAP()
};

#endif // AUTOSELCOMBO_H
