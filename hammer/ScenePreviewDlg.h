//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef SCENEPREVIEWDLG_H
#define SCENEPREVIEWDLG_H
#ifdef _WIN32
#pragma once
#endif


class CChoreoScene;


class CScenePreviewDlg : public CDialog
{
// Construction
public:
	// Note: the dialog now owns the scene and it'll delete it when it goes away.
	CScenePreviewDlg( CChoreoScene *pScene, const char *pSceneFilename, CWnd* pParent = NULL );   // standard constructor
	virtual ~CScenePreviewDlg();
	
// Dialog Data
	//{{AFX_DATA(CSoundBrowser)
	DECLARE_MESSAGE_MAP()
	enum { IDD = IDD_SCENE_PREVIEW };


protected:
	BOOL OnInitDialog();
	virtual LRESULT DefWindowProc( UINT message, WPARAM wParam, LPARAM lParam );
	virtual void OnCancel(void);


private:
	static DWORD WINAPI StaticIdleThread( LPVOID pParameter );
	DWORD IdleThread();
	
	void OnIdle();
	void EndThread();
	
	
private:
	CChoreoScene *m_pScene;

	HANDLE m_hExitThreadEvent;
	HANDLE m_hIdleEventHandledEvent;
	
	HANDLE m_hIdleThread;
	
	int m_iLastEventPlayed;	// Last sound event we handled.
	double m_flStartTime;	// When we started playing the scene.
	char m_SceneFilename[MAX_PATH];
};


#endif // SCENEPREVIEWDLG_H
