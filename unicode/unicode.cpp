//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "unicode/unicode.h"

class CUnicodeWindows : public IUnicodeWindows
{
public:
	virtual LRESULT DefWindowProcW(
		HWND hWnd,
		UINT Msg,
		WPARAM wParam,
		LPARAM lParam )
	{
		return ::DefWindowProcW( hWnd, Msg, wParam, lParam );
	}

	virtual HWND CreateWindowExW(          
		DWORD dwExStyle,
	    LPCWSTR lpClassName,
	    LPCWSTR lpWindowName,
	    DWORD dwStyle,
	    int x,
	    int y,
	    int nWidth,
	    int nHeight,
	    HWND hWndParent,
	    HMENU hMenu,
	    HINSTANCE hInstance,
	    LPVOID lpParam
	)
	{
		return ::CreateWindowExW( dwExStyle, lpClassName, lpWindowName, dwStyle, x, y,
			nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam );
	}

	virtual ATOM RegisterClassW
	( 
		CONST WNDCLASSW *lpWndClass
	)
	{
		return ::RegisterClassW( lpWndClass );
	}

	virtual BOOL UnregisterClassW
	( 
		LPCWSTR lpClassName,
		HINSTANCE hInstance
	)
	{
		return ::UnregisterClassW( lpClassName, hInstance );
	}
};

EXPOSE_SINGLE_INTERFACE( CUnicodeWindows, IUnicodeWindows, VENGINE_UNICODEINTERFACE_VERSION );