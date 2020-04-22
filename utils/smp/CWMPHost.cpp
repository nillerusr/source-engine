//========= Copyright Valve Corporation, All rights reserved. ============//
// CWMPHost.cpp : Implementation of the CWMPHost
//

#include "stdafx.h"
#include "CWMPHost.h"
#include <cstdio>
#include <assert.h>
#include <string>


extern HWND		g_hBlackFadingWindow;
extern CWMPHost*g_pFrame;
extern LPTSTR	g_lpCommandLine;
extern bool		g_bFrameCreated;
extern double	g_timeAtFadeStart;
extern bool		g_bFadeIn;
extern bool		g_bFadeWindowTriggered;
extern std::string g_URL;

bool ShowFadeWindow( bool bShow );
void LogPlayerEvent( EventType_t e, float pos );


CComPtr<IWMPPlayer>	g_spWMPPlayer;
bool				g_bWantToBeFullscreen = false;
bool				g_bPlayOnRestore = false;
int					g_desiredVideoScaleMode = ID_STRETCH_TO_FIT;


#define MAX_STRING  1024

bool FAILMSG(HRESULT hr)
{
    if (FAILED(hr))
    {
        TCHAR   szError[MAX_STRING];

        ::wsprintf(szError, _T("Error code = %08X"), hr);
        ::MessageBox(NULL, szError, _T("Error"), MB_OK | MB_ICONERROR);
    }

    return FAILED(hr);
}

void LogPlayerEvent( EventType_t e )
{
	double dpos = 0.0f;

	CComPtr<IWMPControls> spWMPControls;
	if ( g_spWMPPlayer )
	{
		g_spWMPPlayer->get_controls(&spWMPControls);
		if ( spWMPControls )
		{
			spWMPControls->get_currentPosition( &dpos );
		}
	}

	LogPlayerEvent( e, ( float )dpos );
}

bool IsStretchedToFit()
{
	if ( !g_spWMPPlayer )
		return false;

	CComPtr< IWMPPlayer2 > spWMPPlayer2;
	g_spWMPPlayer.QueryInterface( &spWMPPlayer2 );
	if ( !spWMPPlayer2 )
		return false;

	VARIANT_BOOL vbStretchToFit = VARIANT_FALSE;
	spWMPPlayer2->get_stretchToFit( &vbStretchToFit );
	return vbStretchToFit == VARIANT_TRUE;
}

bool SetVideoScaleMode( int videoScaleMode )
{
	g_desiredVideoScaleMode = videoScaleMode;

	if ( !g_spWMPPlayer )
		return false;

	long width = 0, height = 0;
	CComPtr<IWMPMedia> spWMPMedia;
	g_spWMPPlayer->get_currentMedia( &spWMPMedia );
	if ( spWMPMedia )
	{
		spWMPMedia->get_imageSourceWidth ( &width  );
		spWMPMedia->get_imageSourceHeight( &height );
	}

	VARIANT_BOOL vbStretchToFit = VARIANT_FALSE;
	switch ( videoScaleMode )
	{
	case ID_HALF_SIZE:
		// TODO - set video player size
		break;
	case ID_FULL_SIZE:
		// TODO - set video player size
		break;
	case ID_DOUBLE_SIZE:
		// TODO - set video player size
		break;

	case ID_STRETCH_TO_FIT:
		vbStretchToFit = VARIANT_TRUE;
		break;

	default:
		return false;
	}

	CComPtr< IWMPPlayer2 > spWMPPlayer2;
	g_spWMPPlayer.QueryInterface( &spWMPPlayer2 );
	if ( !spWMPPlayer2 )
		return false;

	bool bStretchToFit = vbStretchToFit == VARIANT_TRUE;
	if ( bStretchToFit == IsStretchedToFit() )
		return true;

	spWMPPlayer2->put_stretchToFit( vbStretchToFit );

	return bStretchToFit == IsStretchedToFit();
}

bool IsFullScreen()
{
	if ( !g_spWMPPlayer )
		return false;

	VARIANT_BOOL vbFullscreen = VARIANT_TRUE;
	g_spWMPPlayer->get_fullScreen( &vbFullscreen );
	return vbFullscreen == VARIANT_TRUE;
}

bool SetFullScreen( bool bWantToBeFullscreen )
{
	g_bWantToBeFullscreen = bWantToBeFullscreen;

	if ( !g_spWMPPlayer )
		return false;

	LogPlayerEvent( bWantToBeFullscreen ? ET_MAXIMIZE : ET_RESTORE );

	bool bIsFullscreen = IsFullScreen();

	CComPtr<IWMPPlayer2> spWMPPlayer2;
	HRESULT hr = g_spWMPPlayer.QueryInterface(&spWMPPlayer2);
	if ( hr != S_OK )
	{
		assert( spWMPPlayer2 == NULL ); // the MS documentation claims this shouldn't happen, but it does...
		spWMPPlayer2 = NULL;
	}

	if ( spWMPPlayer2 )
	{
		VARIANT_BOOL vbStretch = VARIANT_TRUE;
		spWMPPlayer2->get_stretchToFit( &vbStretch );
		bool bStretch = vbStretch == VARIANT_TRUE;
		if ( bStretch != g_bWantToBeFullscreen )
		{
			bIsFullscreen = !g_bWantToBeFullscreen;
		}
	}

	if ( g_bWantToBeFullscreen == bIsFullscreen )
		return true;

	if ( g_bWantToBeFullscreen )
	{
		g_spWMPPlayer->put_uiMode(L"none");

		g_spWMPPlayer->put_fullScreen(VARIANT_TRUE);

		if ( spWMPPlayer2 )
		{
			spWMPPlayer2->put_stretchToFit( VARIANT_TRUE );
		}

		while ( ShowCursor( FALSE ) >= 0 )
			;
	}
	else
	{
		g_spWMPPlayer->put_fullScreen(VARIANT_FALSE);

		g_spWMPPlayer->put_uiMode(L"full");

		if ( spWMPPlayer2 )
		{
			spWMPPlayer2->put_stretchToFit( g_desiredVideoScaleMode == ID_STRETCH_TO_FIT ? VARIANT_TRUE : VARIANT_FALSE );
		}

		g_pFrame->ShowWindow( SW_RESTORE );

		while ( ShowCursor( TRUE ) < 0 )
			;
	}

	bIsFullscreen = IsFullScreen();

	if ( bIsFullscreen != g_bWantToBeFullscreen )
	{
		g_bWantToBeFullscreen = bIsFullscreen;
		OutputDebugString( "SetFullScreen FAILED!\n" );
		return false;
	}

	if ( spWMPPlayer2 )
	{
		if ( g_bWantToBeFullscreen )
		{
			VARIANT_BOOL vbStretch = VARIANT_TRUE;
			spWMPPlayer2->get_stretchToFit( &vbStretch );
			if ( vbStretch != VARIANT_TRUE )
			{
				OutputDebugString( "SetFullScreen FAILED to set stretchToFit!\n" );
				return false;
			}
		}
	}

	return true;
}

bool IsVideoPlaying()
{
	WMPPlayState playState;
	g_spWMPPlayer->get_playState(&playState);
	return playState == wmppsPlaying;
}

void PlayVideo( bool bPlay )
{
	CComPtr<IWMPControls> spWMPControls;
	g_spWMPPlayer->get_controls(&spWMPControls);
	if ( spWMPControls )
	{
		if ( bPlay )
		{
			spWMPControls->play();
		}
		else
		{
			spWMPControls->pause();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CWMPHost

LRESULT CWMPHost::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	AtlAxWinInit();
	CComPtr<IAxWinHostWindow>		   spHost;
	CComPtr<IConnectionPointContainer>  spConnectionContainer;
	CComWMPEventDispatch				*pEventListener = NULL;
	CComPtr<IWMPEvents>				 spEventListener;
	HRESULT							 hr;
	RECT								rcClient;

	m_dwAdviseCookie = 0;
	m_hPopupMenu = 0;

	// create window

	GetClientRect(&rcClient);
	m_wndView.Create(m_hWnd, rcClient, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
	if (NULL == m_wndView.m_hWnd)
		goto FAILURE;
	
	// load OCX in window

	hr = m_wndView.QueryHost(&spHost);
	if (FAILMSG(hr))
		goto FAILURE;

	hr = spHost->CreateControl(CComBSTR(_T("{6BF52A52-394A-11d3-B153-00C04F79FAA6}")), m_wndView, 0);
	if (FAILMSG(hr))
		goto FAILURE;

	hr = m_wndView.QueryControl(&g_spWMPPlayer);
	if (FAILMSG(hr))
		goto FAILURE;

	// start listening to events

	hr = CComWMPEventDispatch::CreateInstance(&pEventListener);
	spEventListener = pEventListener;
	if (FAILMSG(hr))
		goto FAILURE;

	hr = g_spWMPPlayer->QueryInterface(&spConnectionContainer);
	if (FAILMSG(hr))
		goto FAILURE;

	// See if OCX supports the IWMPEvents interface
	hr = spConnectionContainer->FindConnectionPoint(__uuidof(IWMPEvents), &m_spConnectionPoint);
	if (FAILED(hr))
	{
		// If not, try the _WMPOCXEvents interface, which will use IDispatch
		hr = spConnectionContainer->FindConnectionPoint(__uuidof(_WMPOCXEvents), &m_spConnectionPoint);
		if (FAILMSG(hr))
			goto FAILURE;
	}

	hr = m_spConnectionPoint->Advise(spEventListener, &m_dwAdviseCookie);
	if (FAILMSG(hr))
		goto FAILURE;

	IWMPSettings *spWMPSettings;
	g_spWMPPlayer->get_settings( &spWMPSettings );
	if ( spWMPSettings )
	{
		spWMPSettings->put_volume( 100 );
	}

	g_spWMPPlayer->put_enableContextMenu( VARIANT_FALSE );

	// set the url of the movie
	hr = g_spWMPPlayer->put_URL( CT2W( g_URL.c_str() ) );
	if (FAILED(hr))
		OutputDebugString( "put_URL failed\n" );

	return 0;

FAILURE:
	OutputDebugString( "CWMPHost::OnCreate FAILED!\n" );
	DestroyWindow();
	if ( g_hBlackFadingWindow )
	{
		::DestroyWindow( g_hBlackFadingWindow );
	}
//	::PostQuitMessage(0);
	return 0;
}

LRESULT CWMPHost::OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LogPlayerEvent( ET_CLOSE );

	DestroyWindow();
	if ( g_hBlackFadingWindow )
	{
		::DestroyWindow( g_hBlackFadingWindow );
	}

	return 0;
}

LRESULT CWMPHost::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	// stop listening to events
	if (m_spConnectionPoint)
	{
		if (0 != m_dwAdviseCookie)
			m_spConnectionPoint->Unadvise(m_dwAdviseCookie);
		m_spConnectionPoint.Release();
	}

	// close the OCX
	if (g_spWMPPlayer)
	{
		g_spWMPPlayer->close();
		g_spWMPPlayer.Release();
	}

	m_hWnd = NULL;
	g_bFrameCreated = false;

	bHandled = FALSE;
	return 1;
}

LRESULT CWMPHost::OnErase(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& bHandled)
{
	if ( g_bWantToBeFullscreen && !IsFullScreen() )
	{
		g_pFrame->BringWindowToTop();
		SetFullScreen( false );
	}

	bHandled = TRUE;
	return 0;
}

LRESULT CWMPHost::OnSize(UINT /* uMsg */, WPARAM wParam, LPARAM lParam, BOOL& /* lResult */)
{
	if ( wParam == SIZE_MAXIMIZED )
	{
		SetFullScreen( true );
	}
	else
	{
		if ( wParam == SIZE_MINIMIZED )
		{
			LogPlayerEvent( ET_MINIMIZE );
			if ( IsVideoPlaying() )
			{
				g_bPlayOnRestore = true;
				PlayVideo( false );
			}
		}
		else if ( wParam == SIZE_RESTORED )
		{
			LogPlayerEvent( ET_RESTORE );
		}

		RECT rcClient;
		GetClientRect(&rcClient);
		m_wndView.MoveWindow(rcClient.left, rcClient.top, rcClient.right-rcClient.left, rcClient.bottom-rcClient.top);
	}

	return 0;
}

LRESULT CWMPHost::OnContextMenu(UINT /* uMsg */, WPARAM /* wParam */, LPARAM lParam, BOOL& /* lResult */)
{
	if ( !m_hPopupMenu )
	{
		m_hPopupMenu = CreatePopupMenu();
//		AppendMenu( m_hPopupMenu, MF_STRING, ID_HALF_SIZE, "Zoom 50%" );
		AppendMenu( m_hPopupMenu, MF_STRING, ID_FULL_SIZE, "Zoom 100%" );
//		AppendMenu( m_hPopupMenu, MF_STRING, ID_DOUBLE_SIZE, "Zoom 200%" );
		AppendMenu( m_hPopupMenu, MF_STRING, ID_STRETCH_TO_FIT, "Stretch to fit window" );
	}

	int x = GET_X_LPARAM( lParam );
	int y = GET_Y_LPARAM( lParam );
	TrackPopupMenu( m_hPopupMenu, TPM_LEFTALIGN | TPM_TOPALIGN, x, y, 0, m_hWnd, NULL );

	return 0;
}

LRESULT CWMPHost::OnClick(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* lResult */)
{
	if ( IsFullScreen() )
	{
		SetFullScreen( false );
	}
	return 1;
}

LRESULT CWMPHost::OnLeftDoubleClick(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* lResult */)
{
	SetFullScreen( !IsFullScreen() );
	return 1;
}

LRESULT CWMPHost::OnSysKeyDown(UINT /* uMsg */, WPARAM wParam, LPARAM /* lParam */, BOOL& /* lResult */)
{
	switch ( wParam )
	{
	case VK_RETURN:
		SetFullScreen( !IsFullScreen() );
		break;
	}

	return 1;
}

LRESULT CWMPHost::OnKeyDown(UINT /* uMsg */, WPARAM wParam, LPARAM /* lParam */, BOOL& /* lResult */)
{
	switch ( wParam )
	{
	case VK_SPACE:
		PlayVideo( !IsVideoPlaying() );
		break;

	case VK_LEFT:
	case VK_RIGHT:
		{
			CComPtr<IWMPControls> spWMPControls;
			g_spWMPPlayer->get_controls(&spWMPControls);
			if ( !spWMPControls )
				break;

			CComPtr<IWMPControls2> spWMPControls2;
			spWMPControls.QueryInterface(&spWMPControls2);
			if ( !spWMPControls2 )
				break;

			spWMPControls2->step( wParam == VK_LEFT ? -1 : 1 );

			LogPlayerEvent( wParam == VK_LEFT ? ET_STEPBCK : ET_STEPFWD );
		}
		break;

	case VK_UP:
	case VK_DOWN:
		{
			CComPtr<IWMPControls> spWMPControls;
			g_spWMPPlayer->get_controls(&spWMPControls);
			if ( !spWMPControls )
				break;

			double curpos = 0.0f;
			if ( spWMPControls->get_currentPosition( &curpos ) != S_OK )
				break;

			curpos += wParam == VK_UP ? -5.0f : 5.0f;

			spWMPControls->put_currentPosition( curpos );
			if ( !IsVideoPlaying() )
			{
				spWMPControls->play();
				spWMPControls->pause();
			}

			LogPlayerEvent( wParam == VK_UP ? ET_JUMPBCK : ET_JUMPFWD );
		}
		break;

	case VK_HOME:
		{
			CComPtr<IWMPControls> spWMPControls;
			g_spWMPPlayer->get_controls(&spWMPControls);
			if ( !spWMPControls )
				break;

			spWMPControls->put_currentPosition( 0.0f );
			if ( !IsVideoPlaying() )
			{
				spWMPControls->play();
				spWMPControls->pause();
			}

			LogPlayerEvent( ET_JUMPHOME );
		}
		break;

	case VK_END:
		{
			CComPtr<IWMPMedia> spWMPMedia;
			g_spWMPPlayer->get_currentMedia( &spWMPMedia );
			if ( !spWMPMedia )
				break;

			CComPtr<IWMPControls> spWMPControls;
			g_spWMPPlayer->get_controls(&spWMPControls);
			if ( !spWMPControls )
				break;

			double duration = 0.0f;
			if ( spWMPMedia->get_duration( &duration ) != S_OK )
				break;

			spWMPControls->put_currentPosition( duration - 0.050 ); // back a little more than a frame - this avoids triggering the end fade
			spWMPControls->play();
			spWMPControls->pause();

			LogPlayerEvent( ET_JUMPEND );
		}
		break;

	case VK_ESCAPE:
		if ( IsFullScreen() && !g_bFadeWindowTriggered )
		{
			CComPtr<IWMPControls> spWMPControls;
			g_spWMPPlayer->get_controls(&spWMPControls);
			if ( spWMPControls )
			{
				spWMPControls->stop();
			}

			LogPlayerEvent( ET_FADEOUT );

			g_bFadeWindowTriggered = true;
			ShowFadeWindow( true );
		}
		break;
	}

	return 0;
}

LRESULT CWMPHost::OnNCActivate(UINT /* uMsg */, WPARAM wParam, LPARAM /* lParam */, BOOL& /* lResult */ )
{
	if ( wParam )
	{
		if ( g_bWantToBeFullscreen )
		{
			SetFullScreen( true );
		}
		if ( g_bPlayOnRestore )
		{
			g_bPlayOnRestore = false;
			PlayVideo( true );
		}
	}

	return 1;
}

LRESULT CWMPHost::OnVideoScale(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	SetVideoScaleMode( wID );
	return 0;
}
