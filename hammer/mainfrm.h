//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MAINFRM_H
#define MAINFRM_H
#ifdef _WIN32
#pragma once
#endif

#include "MDIClientWnd.h"
#include "FilterControl.h"
#include "ObjectBar.h"
#include "Texturebar.h"
#include "MapAnimationDlg.h"
#include "SelectModeDlgBar.h"
#include "materialdlg.h"
#include "ManifestDialog.h"

class CChildFrame;
class CObjectProperties;
class CTextureBrowser;
class CSearchReplaceDlg;
class CFaceEditSheet;
class CMessageWnd;
class CLightingPreviewResultsWindow;


class CMainFrame : public CMDIFrameWnd
{
	DECLARE_DYNAMIC( CMainFrame )

public:

	CMainFrame();
	virtual ~CMainFrame();

	bool VerifyBarState(void);

	void BeginShellSession(void);
	void EndShellSession(void);
	bool IsShellSessionActive(void);

	BOOL IsUndoActive() { return m_bUndoActive; }
	void SetUndoActive(BOOL bActive);

	void SetBrightness(float fBrightness);
	void UpdateAllDocViews(DWORD dwCmd);
	void Configure();
	void GlobalNotify(int nCode);
	void OnDeleteActiveDocument(void);

	void LoadWindowStates(std::fstream *pFile = NULL);

	inline CFaceEditSheet *GetFaceEditSheet( void ) { return m_pFaceEditSheet; }
	inline CStatusBar *GetStatusBar() { return &m_wndStatusBar; }

	void ShowSearchReplaceDialog(void);
	void ShowFaceEditSheetOrTextureBar( bool bShowFaceEditSheet );
	HACCEL GetAccelTable( void ) { return m_hAccelTable; }

	CFaceSmoothingVisualDlg *GetSmoothingGroupDialog( void )	{ return &m_SmoothingGroupDlg; }

	void ResetAutosaveTimer();

	bool IsInFaceEditMode();

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	void OpenURL(const char *pszURL);
	void OpenURL(UINT nID);

	//
	// Public attributes. FIXME: eliminate!
	//
	CTextureBrowser			*pTextureBrowser;
	CObjectProperties		*pObjectProperties;

	CFilterControl			m_FilterControl;
	CObjectBar				m_ObjectBar;
	CTextureBar				m_TextureBar;
	CManifestFilter			m_ManifestFilterControl;
	CFaceEditSheet			*m_pFaceEditSheet;
	CLightingPreviewResultsWindow *m_pLightingPreviewOutputWindow;
	bool					m_bLightingPreviewOutputWindowShowing;

	//CMapAnimationDlg m_AnimationDlg;

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	public:
	virtual void WinHelp(DWORD dwData, UINT nCmd);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	//}}AFX_VIRTUAL

protected:

	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnEditProperties();
	afx_msg void OnViewMessages();
	afx_msg void OnUpdateViewMessages(CCmdUI* pCmdUI);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnToolsOptions();
	afx_msg void OnViewShowconnections();
	afx_msg void OnToolsPrefabfactory();
	afx_msg BOOL OnHelpOpenURL(UINT nID);
	afx_msg void OnHelpFinder();
	afx_msg void OnEditUndoredoactive();
	afx_msg void OnUpdateEditUndoredoactive(CCmdUI* pCmdUI);
	afx_msg BOOL OnFileNew(UINT);
	afx_msg void OnSavewindowstate();
	afx_msg void OnLoadwindowstate();
	afx_msg void OnFileOpen();
	afx_msg BOOL OnChange3dViewType(UINT nID);
	afx_msg BOOL OnUnits(UINT nID);
	afx_msg void OnUpdateUnits(CCmdUI *pCmdUI);
	afx_msg void OnUpdateView3d(CCmdUI *pCmdUI);
	afx_msg void OnUpdateView2d(CCmdUI *pCmdUI);
	afx_msg void OnUpdateToolUI(CCmdUI *pUI);
	afx_msg BOOL OnView3dChangeBrightness(UINT nID);
	afx_msg void OnUpdateApplicatorUI(CCmdUI *pUI);
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnEnterMenuLoop( BOOL bIsTrackPopupMenu );
#if _MSC_VER < 1300
	afx_msg void OnActivateApp(BOOL bActive, HTASK hTask);
#else
	afx_msg void OnActivateApp(BOOL bActive, DWORD hThread);
#endif
	afx_msg void OnUpdateEditFunction(CCmdUI *pCmdUI);
	afx_msg BOOL OnApplicator(UINT nID);
	afx_msg BOOL OnSoundBrowser(UINT nID);
	afx_msg BOOL OnReloadSounds(UINT nID);
	afx_msg void OnUpdateOpaqueMaterials(CCmdUI *pCmdUI);
	afx_msg void OnOpaqueMaterials();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg BOOL OnChangeTool(UINT nID);
	afx_msg void OnInitMenu( CMenu *pMenu );
	afx_msg void OnHDR( void );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:

	void DockControlBarLeftOf(CControlBar *Bar, CControlBar *LeftOf);

	void SaveWindowStates(std::fstream *pFile = NULL);

	CChildFrame *GetNextMDIChildWnd(CChildFrame *pCurChild);
	CChildFrame *GetNextMDIChildWndRecursive(CWnd *pCurChild);

	void EnableFaceEditMode(bool bEnable);
	void Autosave( void );
	void LoadOldestAutosave( void );

	CMDIClientWnd			wndMDIClient;			// dvs: what in God's name is this for?

	CSearchReplaceDlg		*m_pSearchReplaceDlg;

	BOOL					m_bUndoActive;

	CStatusBar				m_wndStatusBar;
	CToolBar				m_wndMapToolBar;
	CToolBar				m_wndUndoRedoToolBar;
	CToolBar				m_wndMapEditToolBar;
	CToolBar				m_wndMapOps;

	CSelectModeDlgBar		m_SelectModeDlg;

	CFaceSmoothingVisualDlg	m_SmoothingGroupDlg;

	bool					m_bMinimized;
	bool					m_bShellSessionActive;		// Whether a client has initiated a remote shell editing session.
	CBitmap					m_bmMapEditTools256;

	enum
	{
		AUTOSAVE_TIMER,
		FIRST_TIMER
	};

};


CMainFrame *GetMainWnd();


#endif // MAINFRM_H
