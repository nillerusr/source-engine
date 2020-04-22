//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SHELL_ICON_MGR_H
#define SHELL_ICON_MGR_H
#ifdef _WIN32
#pragma once
#endif


class IShellIconMgrHelper
{
public:
	virtual HINSTANCE	GetHInstance() = 0;
	virtual int			WindowProc( void *hWnd, int uMsg, long wParam, long lParam ) = 0;
};

class CShellIconMgr
{
public:
	
					CShellIconMgr();
					~CShellIconMgr();
	
	bool			Init( 
		IShellIconMgrHelper *pHelper, 
		const char *pToolTip,			// This must be allocated by the caller and must be valid as
										// long as the CShellIconMgr exists.
		int iCallbackMessage,
		int iIconResourceID );

	void			Term();

	void			ChangeIcon( int iIconResourceID );


private:

	void			CreateTrayIcon();

	LRESULT			WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	static LRESULT CALLBACK StaticWindowProc(
		  HWND hwnd,      // handle to window
		  UINT uMsg,      // message identifier
		  WPARAM wParam,  // first message parameter
		  LPARAM lParam   // second message parameter
		);
	
	HWND			m_hWnd;				// Invisible window to get timer and shell icon messages.
	ATOM			m_hWndClass;
	
	UINT			m_uTaskbarRestart;	// This message is sent to us when the taskbar is created.
	int				m_iCurIconResourceID;
	int				m_iCallbackMessage;
	const char		*m_pToolTip;

	IShellIconMgrHelper	*m_pHelper;
};


#endif // SHELL_ICON_MGR_H
