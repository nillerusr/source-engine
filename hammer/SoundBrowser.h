//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#if !defined(AFX_SOUNDBROWSER_H__33046A12_7CF9_4031_AD10_A76200E73280__INCLUDED_)
#define AFX_SOUNDBROWSER_H__33046A12_7CF9_4031_AD10_A76200E73280__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SoundBrowser.h : header file
//

#include "soundsystem.h"
#include "AutoSelCombo.h"

/////////////////////////////////////////////////////////////////////////////
// CSoundBrowser dialog

class CSoundBrowser : public CDialog
{
// Construction
public:
	CSoundBrowser( const char *pCurrentSoundName, CWnd* pParent = NULL);   // standard constructor
	const char *GetSelectedSound();

// Dialog Data
	//{{AFX_DATA(CSoundBrowser)
	enum { IDD = IDD_SOUNDBROWSER };
	CListBox	m_SoundList;
	CString	m_SoundNameSelected;
	int		m_SoundType;
	BOOL	m_Autoplay;
	CString	m_SoundFile;
	CString	m_SoundSource;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSoundBrowser)
	public:
	virtual int DoModal();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	void SaveValues();

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSoundBrowser)
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	afx_msg void OnChangeFilter();
	afx_msg void OnUpdateFilterNOW();
	afx_msg void OnSelchangeSoundType();
	afx_msg void OnSelchangeSoundList();
	afx_msg void OnDblclkSoundList();
	afx_msg void OnPreview();
	afx_msg void OnAutoplay();
	afx_msg void OnBnClickedStopsound();
	afx_msg void OnRefreshSounds();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnOpenSource();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void Shutdown();
	void ClearSoundList();
	void PopulateSoundList();
	void CopySoundNameToSelected();
	SoundType_t GetSoundType() const;
	bool ShowSoundInList( const char *pSoundName );
	void OnFilterChanged( const char *pFilter );

	DWORD m_uLastFilterChange;
	BOOL m_bFilterChanged;

	BOOL m_bSoundPlayed;			// used so we can do a timer query to keep disable the stop sound button
	DWORD m_uSoundPlayTime;

	int m_nSelectedSoundIndex;

	CAutoSelComboBox m_cFilter;
	char m_szFilter[256];			// Name filter, space, comma, or semicolon delimited.
	int m_nFilters;					// The number of names that were parsed out of the name filter.
	char *m_Filters[64];			// The individual name filters.

	static CStringArray m_FilterHistory;
	static int m_nFilterHistory;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SOUNDBROWSER_H__33046A12_7CF9_4031_AD10_A76200E73280__INCLUDED_)
