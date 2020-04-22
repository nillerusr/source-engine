//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "shell_icon_mgr.h"
#include "tier1/strtools.h"
#include "tier0/dbg.h"
#include "shellapi.h"


#define SHELLICONMGR_CLASSNAME	"VMPI_ShellIconMgr"



CShellIconMgr::CShellIconMgr()
{
	m_hWnd = NULL;
	m_hWndClass = NULL;
	m_uTaskbarRestart = (UINT)-1;
	m_iCurIconResourceID = 0;
	m_pHelper = NULL;
}


CShellIconMgr::~CShellIconMgr()
{
	Term();
}


bool CShellIconMgr::Init( 
	IShellIconMgrHelper *pHelper, 
	const char *pToolTip, 
	int iCallbackMessage, 
	int iIconResourceID )
{
	Term();

	m_pHelper = pHelper;
	m_iCallbackMessage = iCallbackMessage;
	m_pToolTip = pToolTip;
	
	// Create the window class.
	WNDCLASS wndclass;
	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.lpfnWndProc = &CShellIconMgr::StaticWindowProc;
	wndclass.hInstance = (HINSTANCE)pHelper->GetHInstance();
	wndclass.hIcon = NULL;
	wndclass.hCursor = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = SHELLICONMGR_CLASSNAME;
	m_hWndClass = RegisterClass( &wndclass );
	if ( !m_hWndClass )
	{
		Assert( false );
		Warning( "RegisterClass failed.\n" );
		Term();
		return false;
	}


	// Create the window.
	m_hWnd = CreateWindow(
		SHELLICONMGR_CLASSNAME,
		SHELLICONMGR_CLASSNAME,
		WS_DISABLED,	// we only want shell notify messages
		0, 0, 0, 0,		// position
		NULL,			// parent
		NULL,			// menu
		pHelper->GetHInstance(),	// hInstance
		this			// lpParam
		);
	if ( !m_hWnd )
	{
		Assert( false );
		Warning( "CreateWindow failed.\n" );
		Term();
		return false;
	}

	m_uTaskbarRestart = RegisterWindowMessage( TEXT( "TaskbarCreated" ) );
	SetWindowLong( m_hWnd, GWL_USERDATA, (LONG)this );
	UpdateWindow( m_hWnd );

	// Don't handle errors here because the taskbar may not be created yet if we are a service.
	m_iCurIconResourceID = iIconResourceID;
	CreateTrayIcon();
	return true;
}


void CShellIconMgr::Term()
{
	if ( m_hWndClass )
	{
		UnregisterClass( SHELLICONMGR_CLASSNAME, m_pHelper->GetHInstance() );
		m_hWndClass = NULL;
	}

	if ( m_hWnd )
	{
		// Remove the shell icon if there is one.
		NOTIFYICONDATA data;
		memset( &data, 0, sizeof( data ) );
		data.cbSize = sizeof( data );
		data.hWnd = m_hWnd;
		Shell_NotifyIcon( NIM_DELETE, &data );
		
		DestroyWindow( m_hWnd );
		m_hWnd = NULL;
	}
}


void CShellIconMgr::ChangeIcon( int iIconResourceID )
{
	NOTIFYICONDATA data;
	
	memset( &data, 0, sizeof( data ) );
	data.cbSize = sizeof( data );
	data.uFlags = NIF_ICON;
	data.hWnd = m_hWnd;
	data.hIcon = LoadIcon( m_pHelper->GetHInstance(), MAKEINTRESOURCE( iIconResourceID ) );
	
	Shell_NotifyIcon( NIM_MODIFY, &data );
	
	m_iCurIconResourceID = iIconResourceID;
}


void CShellIconMgr::CreateTrayIcon()
{
	// Now create the shell icon.
	NOTIFYICONDATA data;
	
	memset( &data, 0, sizeof( data ) );
	data.cbSize = sizeof( data );
	data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	data.uCallbackMessage = m_iCallbackMessage;
	data.hWnd = m_hWnd;
	data.hIcon = LoadIcon( m_pHelper->GetHInstance(), MAKEINTRESOURCE( m_iCurIconResourceID ) );
	Q_strncpy( data.szTip, m_pToolTip, sizeof( data.szTip ) );

	Shell_NotifyIcon( NIM_ADD, &data );
}


LRESULT CShellIconMgr::WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if ( hWnd != m_hWnd )
	{
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	}

	if ( uMsg == m_uTaskbarRestart )
	{
		CreateTrayIcon();
		return 0;
	}

	return m_pHelper->WindowProc( hWnd, uMsg, wParam, lParam );
}


LRESULT CShellIconMgr::StaticWindowProc(
	HWND hwnd,      // handle to window
	UINT uMsg,      // message identifier
	WPARAM wParam,  // first message parameter
	LPARAM lParam   // second message parameter
	)
{
	CShellIconMgr *pMgr = (CShellIconMgr*)GetWindowLong( hwnd, GWL_USERDATA );
	if ( pMgr )
	{
		return pMgr->WindowProc( hwnd, uMsg, wParam, lParam );
	}
	else
	{
		return DefWindowProc( hwnd, uMsg, wParam, lParam );
	}
}

