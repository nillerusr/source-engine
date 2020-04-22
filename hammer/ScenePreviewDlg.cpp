//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "ScenePreviewDlg.h"
#include "choreoscene.h"
#include "soundsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#define WM_SCENEPREVIEW_IDLE	(WM_USER+1)


BEGIN_MESSAGE_MAP(CScenePreviewDlg, CDialog)
	//{{AFX_MSG_MAP(CScenePreviewDlg)
	ON_BN_CLICKED(IDCANCEL, OnCancel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


CScenePreviewDlg::CScenePreviewDlg( CChoreoScene *pScene, const char *pFilename, CWnd* pParent /*=NULL*/ )
	: CDialog(CScenePreviewDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CScenePreviewDlg)
	m_pScene = pScene;
	m_iLastEventPlayed = -2; // Don't do anything yet.
	m_hExitThreadEvent = NULL;
	m_hIdleEventHandledEvent = NULL;
	m_hIdleThread = NULL;
	V_strncpy( m_SceneFilename, pFilename, sizeof( m_SceneFilename ) );
}


CScenePreviewDlg::~CScenePreviewDlg()
{
	EndThread();
	delete m_pScene;
}


void CScenePreviewDlg::EndThread()
{
	if ( m_hIdleThread )
	{
		SetEvent( m_hExitThreadEvent );
		WaitForSingleObject( m_hIdleThread, INFINITE );
		CloseHandle( m_hIdleThread );
		CloseHandle( m_hExitThreadEvent );
		CloseHandle( m_hIdleEventHandledEvent );
		m_hIdleThread = m_hExitThreadEvent = m_hIdleEventHandledEvent = NULL;
	}
}


BOOL CScenePreviewDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Setup the control showing the scene name.
	CString str;
	GetDlgItemText( IDC_SCENE_NAME, str );
	str = str + " " + m_SceneFilename;
	SetDlgItemText( IDC_SCENE_NAME, str );

	CString strNone;
	strNone.LoadString( IDS_NONE );
	SetDlgItemText( IDC_CURRENT_SOUND, strNone );
	SetDlgItemText( IDC_NEXT_SOUND, strNone );

	m_iLastEventPlayed = -1;
	m_flStartTime = Plat_FloatTime();
	
	// Start on the first event..
	for ( int i = 0; i < m_pScene->GetNumEvents(); i++ )
	{
		CChoreoEvent *e = m_pScene->GetEvent( i );
		if ( e && e->GetType() == CChoreoEvent::SPEAK )
		{
			m_flStartTime -= e->GetStartTime();
			break;
		}
	}
	
	// Create our idle thread.
	m_hExitThreadEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	m_hIdleEventHandledEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	m_hIdleThread = CreateThread( NULL, 0, &CScenePreviewDlg::StaticIdleThread, this, 0, NULL );
	
	return TRUE;
}


DWORD CScenePreviewDlg::StaticIdleThread( LPVOID pParameter )
{
	return ((CScenePreviewDlg*)pParameter)->IdleThread();
}

DWORD CScenePreviewDlg::IdleThread()
{
	HANDLE handles[2] = {m_hExitThreadEvent, m_hIdleEventHandledEvent};
	
	while ( 1 )
	{
		// Send the event to the window.
		PostMessage( WM_SCENEPREVIEW_IDLE, 0, 0 );
		
		DWORD ret = WaitForMultipleObjects( ARRAYSIZE( handles ), handles, false, INFINITE );
		if ( ret == WAIT_OBJECT_0 || ret == WAIT_TIMEOUT )
			return 0;

		Sleep( 100 );	// Only handle idle 10x/sec.
	}
}


void CScenePreviewDlg::OnIdle()
{
	double flElapsed = Plat_FloatTime() - m_flStartTime;
	
	// Find the next sound to play.
	int iLastSound = -1;
	for ( int i = 0; i < m_pScene->GetNumEvents(); i++ )
	{
		CChoreoEvent *e = m_pScene->GetEvent( i );
		if ( !e || e->GetType() != CChoreoEvent::SPEAK )
			continue;
		
		if ( flElapsed > e->GetStartTime() )
			iLastSound = i;
	}

	// Is there another sound to play in here?
	if ( iLastSound >= 0 && iLastSound != m_iLastEventPlayed )
	{
		m_iLastEventPlayed = iLastSound;

		// Play the current sound.
		CChoreoEvent *pChoreoEvent = m_pScene->GetEvent( iLastSound );
		const char *pSoundName = pChoreoEvent->GetParameters();
		
		SoundType_t soundType;
		int nIndex;
		if ( g_Sounds.FindSoundByName( pSoundName, &soundType, &nIndex ) )
		{
			bool bRet = g_Sounds.Play( soundType, nIndex );
			
			CString curSound = pSoundName;
			if ( !bRet )
			{
				CString strErrorPlaying;
				strErrorPlaying.LoadString( IDS_ERROR_PLAYING );
				curSound += " " + strErrorPlaying;
			}
			SetDlgItemText( IDC_CURRENT_SOUND, curSound );
		}

		// Find the next sound event.
		CString str;
		str.LoadString( IDS_NONE );
		for ( int i=iLastSound+1; i < m_pScene->GetNumEvents(); i++ )
		{
			CChoreoEvent *e = m_pScene->GetEvent( i );
			if ( e && e->GetType() == CChoreoEvent::SPEAK )
			{
				str = e->GetParameters();
				break;
			}
		}
		SetDlgItemText( IDC_NEXT_SOUND, str );
	}
}


LRESULT CScenePreviewDlg::DefWindowProc( UINT message, WPARAM wParam, LPARAM lParam )
{
	// We handle the enter key specifically because the default combo box behavior is to
	// reset the text and all this stuff we don't want.
	if ( message == WM_SCENEPREVIEW_IDLE )
	{
		OnIdle();
		SetEvent( m_hIdleEventHandledEvent );
		return 0;
	}
	
	return CDialog::DefWindowProc( message, wParam, lParam );
}


void CScenePreviewDlg::OnCancel(void)
{
	g_Sounds.StopSound();
	EndThread();
	EndDialog( 0 );
}
