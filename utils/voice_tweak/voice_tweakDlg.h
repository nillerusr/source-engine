//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// voice_tweakDlg.h : header file
//

#if !defined(AFX_VOICE_TWEAKDLG_H__DC9D9CC1_33D0_4783_ABEB_F491ABE65935__INCLUDED_)
#define AFX_VOICE_TWEAKDLG_H__DC9D9CC1_33D0_4783_ABEB_F491ABE65935__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "voice_mixer_controls.h"
#include "win_idle.h"
#include "voice_tweak.h"


#define WM_TWEAKIDLE	(WM_USER + 111)


/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakDlg dialog

class CVoiceTweakDlg : public CDialog
{
// Construction
public:
				CVoiceTweakDlg(CWnd* pParent = NULL);	// standard constructor
	virtual		~CVoiceTweakDlg();
	
	void		SetString(int childControl, int stringID);
	void		Term();

// Dialog Data
	//{{AFX_DATA(CVoiceTweakDlg)
	enum { IDD = IDD_VOICE_TWEAK_DIALOG };
	CButton	m_HardwareGain;
	CProgressCtrl	m_VoiceVolume;
	CSliderCtrl	m_VolumeSlider;
	CEdit	m_InstructionText;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVoiceTweakDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON		m_hIcon;
	CWinIdle	m_WinIdle;

	CAutoGain			m_Gain;

	// Old settings (for cancel button).
	float				m_OldVolume;


	// Generated message map functions
	//{{AFX_MSG(CVoiceTweakDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg LONG OnIdle(UINT a, LONG b);	
	afx_msg void OnDestroy();
	afx_msg void OnHardwareGain();
	virtual void OnCancel();
	afx_msg void OnFurtherhelp();
	afx_msg void OnSystemSetup();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_VOICE_TWEAKDLG_H__DC9D9CC1_33D0_4783_ABEB_F491ABE65935__INCLUDED_)
