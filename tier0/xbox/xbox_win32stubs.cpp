//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: XBox win32 replacements - Mocks trivial windows flow
//
//=============================================================================//

#include "pch_tier0.h"
#include "xbox/xbox_win32stubs.h"
#include "tier0/memdbgon.h"

// On the 360, threads can run on any of 6 logical processors
DWORD g_dwProcessAffinityMask = 0x3F;

#define HWND_MAGIC	0x12345678

struct xWndClass_t
{
	char*			pClassName;
	WNDPROC			wndProc;
	xWndClass_t*	pNext;
};

struct xWnd_t
{
	xWndClass_t*	pWndClass;
	int				x;
	int				y;
	int				w;
	int				h;
	long			windowLongs[GWL_MAX];
	int				show;
	int				nMagic;
	xWnd_t*			pNext;
};

static xWndClass_t*	g_pWndClasses;
static xWnd_t*		g_pWnds;
static HWND			g_focusWindow;
 
inline bool IsWndValid( HWND hWnd )
{
	if ( !hWnd || ((xWnd_t*)hWnd)->nMagic != HWND_MAGIC )
		return false;
	return true;
}

int MessageBox(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
{
	XBX_Error( lpText );
	Assert( 0 );

	return (0);
}

LONG GetWindowLong(HWND hWnd, int nIndex)
{
	LONG	oldLong;

	if ( !IsWndValid( hWnd ) )
		return 0;

	switch (nIndex)
	{
		case GWL_WNDPROC:
		case GWL_USERDATA:
		case GWL_STYLE:
		case GWL_EXSTYLE:
			oldLong = ((xWnd_t*)hWnd)->windowLongs[nIndex];
			break;
		default:
			// not implemented
			Assert( 0 );
			return 0;
	}

	return oldLong;
}

LONG_PTR GetWindowLongPtr(HWND hWnd, int nIndex)
{
	UINT idx;

	switch ( nIndex )
	{
	case GWLP_WNDPROC:
		idx = GWL_WNDPROC;
		break;
	case GWLP_USERDATA:
		idx = GWL_USERDATA;
		break;
	default:
		// not implemented
		Assert(0);
		return 0;
	}

	return GetWindowLong( hWnd, idx );
}

LONG_PTR GetWindowLongPtrW(HWND hWnd, int nIndex)
{
	AssertMsg( false, "GetWindowLongPtrW does not exist on Xbox 360." );
	return GetWindowLongPtr( hWnd, nIndex );
}


LONG SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong)
{
	LONG	oldLong;

	if ( !IsWndValid( hWnd ) )
		return 0;

	switch ( nIndex )
	{
		case GWL_WNDPROC:
		case GWL_USERDATA:
		case GWL_STYLE:
			oldLong = ((xWnd_t*)hWnd)->windowLongs[nIndex];
			((xWnd_t*)hWnd)->windowLongs[nIndex] = dwNewLong;
			break;
		default:
			// not implemented
			Assert( 0 );
			return 0;
	}

	return oldLong;
}

LONG_PTR SetWindowLongPtr(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
{
	UINT idx;

	switch ( nIndex )
	{
	case GWLP_WNDPROC:
		idx = GWL_WNDPROC;
		break;
	case GWLP_USERDATA:
		idx = GWL_USERDATA;
		break;
	default:
		// not implemented
		Assert( 0 );
		return 0;
	}

	return SetWindowLong( hWnd, idx, dwNewLong );
}

LONG_PTR SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
{
	AssertMsg( false, "SetWindowLongPtrW does not exist on Xbox 360." );
	return SetWindowLongPtr( hWnd, nIndex, dwNewLong  );
}

HWND CreateWindow(LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	// find classname
	xWndClass_t* pWndClass = g_pWndClasses;
	while ( pWndClass )
	{
		if ( !stricmp( lpClassName, pWndClass->pClassName ) )
			break;
		pWndClass = pWndClass->pNext;
	}
	if ( !pWndClass )
	{
		// no such class
		return (HWND)NULL;
	}

	// allocate and setup
	xWnd_t* pWnd = new xWnd_t;
	memset( pWnd, 0, sizeof(xWnd_t) );
	pWnd->pWndClass = pWndClass;
	pWnd->windowLongs[GWL_WNDPROC] = (LONG)pWndClass->wndProc;
	pWnd->windowLongs[GWL_STYLE] = dwStyle;
	pWnd->x = x;
	pWnd->y = y;
	pWnd->w = nWidth;
	pWnd->h = nHeight;
	pWnd->nMagic = HWND_MAGIC;

	// link into list
	pWnd->pNext = g_pWnds;
	g_pWnds = pWnd;

	// force the focus
	g_focusWindow = (HWND)pWnd;

	// send the expected message sequence
	SendMessage( (HWND)pWnd, WM_CREATE, 0, 0 );
	SendMessage( (HWND)pWnd, WM_ACTIVATEAPP, TRUE, 0 );

	return (HWND)pWnd;
}

HWND CreateWindowEx(DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	return CreateWindow( lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam );
}

BOOL DestroyWindow( HWND hWnd )
{
	if ( !IsWndValid( hWnd ) )
		return FALSE;

	xWnd_t*	pPrev = g_pWnds;
	xWnd_t*	pCur = g_pWnds;

	while ( pCur )
	{
		if ( pCur == (xWnd_t*)hWnd )
		{
			if ( pPrev == g_pWnds )
			{
				// at head of list, fixup
				g_pWnds = pCur->pNext;
			}
			else
			{
				// remove from chain
				pPrev->pNext = pCur->pNext;
			}
			pCur->nMagic = 0;
			delete pCur;
			break;
		}

		// advance through list
		pPrev = pCur;
		pCur = pCur->pNext;
	}

	return TRUE;
}

ATOM RegisterClassEx(CONST WNDCLASSEX *lpwcx)
{
	// create
	xWndClass_t* pWndClass = new xWndClass_t;
	memset(pWndClass, 0, sizeof(xWndClass_t));
	pWndClass->pClassName = new char[strlen(lpwcx->lpszClassName)+1];
	strcpy(pWndClass->pClassName, lpwcx->lpszClassName);
	pWndClass->wndProc = lpwcx->lpfnWndProc;

	// insert into list
	pWndClass->pNext = g_pWndClasses;
	g_pWndClasses = pWndClass;

	return (ATOM)pWndClass;
}

ATOM RegisterClass(CONST WNDCLASS *lpwc)
{
	// create
	xWndClass_t* pWndClass = new xWndClass_t;
	memset(pWndClass, 0, sizeof(xWndClass_t));
	pWndClass->pClassName = new char[strlen(lpwc->lpszClassName)+1];
	strcpy(pWndClass->pClassName, lpwc->lpszClassName);
	pWndClass->wndProc = lpwc->lpfnWndProc;

	// insert into list
	pWndClass->pNext = g_pWndClasses;
	g_pWndClasses = pWndClass;

	return (ATOM)pWndClass;
}

HWND GetFocus(VOID)
{
	if ( !IsWndValid( g_focusWindow ) )
		return NULL;

	return g_focusWindow;
}

HWND SetFocus( HWND hWnd )
{
	HWND hOldFocus = g_focusWindow;

	if ( IsWndValid( hWnd ) )
	{
		g_focusWindow = hWnd;
	}

	return hOldFocus;
}


LRESULT CallWindowProc(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return (lpPrevWndFunc(hWnd, Msg, wParam, lParam));
}

int GetSystemMetrics(int nIndex)
{
	XVIDEO_MODE videoMode;
	XGetVideoMode( &videoMode );
	// default to having the backbuffer the same as the mode resolution.
	int nFrameBufferWidth, nFrameBufferHeight;
	nFrameBufferWidth =  videoMode.dwDisplayWidth;
	nFrameBufferHeight = videoMode.dwDisplayHeight;

	// override for cases where we need to have a different backbuffer either for memory reasons
	// or for dealing with anamorphic modes.
	if ( !videoMode.fIsWideScreen && videoMode.dwDisplayWidth == 640 && videoMode.dwDisplayHeight == 576 )
	{
		// PAL normal
		nFrameBufferWidth = 640;
		nFrameBufferHeight = 480;
	}
	else if ( videoMode.fIsWideScreen && videoMode.dwDisplayWidth == 640 && videoMode.dwDisplayHeight == 576 )
	{
		// PAL widescreen
		nFrameBufferWidth = 848;
		nFrameBufferHeight = 480;
	}
	else if ( videoMode.fIsWideScreen && videoMode.dwDisplayWidth == 640 && videoMode.dwDisplayHeight == 480 )
	{
		// anamorphic
		nFrameBufferWidth = 848;
		nFrameBufferHeight = 480;
	}
	else if ( videoMode.fIsWideScreen && videoMode.dwDisplayWidth == 1024 && videoMode.dwDisplayHeight == 768 )
	{
		// anamorphic
		nFrameBufferWidth = 1280;
		nFrameBufferHeight = 720;
	}
	else if ( videoMode.dwDisplayWidth == 1280 && videoMode.dwDisplayHeight == 760 )
	{
		nFrameBufferWidth = 1280;
		nFrameBufferHeight = 720;
	}
	else if ( videoMode.dwDisplayWidth == 1280 && videoMode.dwDisplayHeight == 768 )
	{
		nFrameBufferWidth = 1280;
		nFrameBufferHeight = 720;
	}
	else if ( !videoMode.fIsWideScreen && videoMode.dwDisplayWidth == 1280 && videoMode.dwDisplayHeight == 1024 )
	{
		nFrameBufferWidth = 1024;
		nFrameBufferHeight = 768;
	}
	else if ( videoMode.fIsWideScreen && videoMode.dwDisplayWidth == 1280 && videoMode.dwDisplayHeight == 1024 )
	{
		// anamorphic
		nFrameBufferWidth = 1280;
		nFrameBufferHeight = 720;
	}
	else if ( videoMode.dwDisplayWidth == 1360 && videoMode.dwDisplayHeight == 768 )
	{
		nFrameBufferWidth = 1280;
		nFrameBufferHeight = 720;
	}
	else if ( videoMode.dwDisplayWidth == 1920 && videoMode.dwDisplayHeight == 1080 )
	{
		nFrameBufferWidth = 1280;
		nFrameBufferHeight = 720;
	}

	switch ( nIndex )
	{
		case SM_CXFIXEDFRAME:
		case SM_CYFIXEDFRAME:
		case SM_CYSIZE:
			return 0;
		case SM_CXSCREEN:
			return nFrameBufferWidth;
		case SM_CYSCREEN:
			return nFrameBufferHeight;
	}

	// not implemented
	Assert( 0 );
	return 0;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow)
{
	if ( !IsWndValid( hWnd ) )
		return FALSE;

	((xWnd_t*)hWnd)->show = nCmdShow;

	if ((nCmdShow == SW_SHOWDEFAULT) || (nCmdShow == SW_SHOWNORMAL) || (nCmdShow == SW_SHOW))
		g_focusWindow = hWnd;

	return TRUE;
}

LRESULT SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if ( !IsWndValid( hWnd ) )
		return 0L;

	xWnd_t* pWnd    = (xWnd_t*)hWnd;
	WNDPROC wndProc = (WNDPROC)pWnd->windowLongs[GWL_WNDPROC];
	Assert( wndProc );
	LRESULT result  = wndProc(hWnd, Msg, wParam, lParam);

	return result;
}

LRESULT	SendMessageTimeout( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult )
{
	*lpdwResult = SendMessage( hWnd, Msg, wParam, lParam );

	return -1;
}

BOOL GetClientRect(HWND hWnd, LPRECT lpRect)
{
	if ( !IsWndValid( hWnd ) )
		return FALSE;

	xWnd_t* pWnd = (xWnd_t*)hWnd;
	lpRect->left = 0;
	lpRect->top = 0;
	lpRect->right  = pWnd->w;
	lpRect->bottom = pWnd->h;

	return TRUE;
}

int GetDeviceCaps(HDC hdc, int nIndex)
{
	switch (nIndex)
	{
		case HORZRES:
			return GetSystemMetrics( SM_CXSCREEN );
		case VERTRES:
			return GetSystemMetrics( SM_CYSCREEN );
		case VREFRESH:
			return 60;  
	}

	Assert( 0 );
	return 0;
}

BOOL SetWindowPos( HWND hWnd, HWND hWndInsertAfter, int x, int y, int cx, int cy, UINT uFlags )
{
	if ( !IsWndValid( hWnd ) )
		return FALSE;

	xWnd_t* pWnd = (xWnd_t*)hWnd;

	if ( !( uFlags & SWP_NOMOVE ) )
	{
		pWnd->x = x;
		pWnd->y = y;
	}

	if ( !( uFlags & SWP_NOSIZE ) )
	{
		pWnd->w = cx;
		pWnd->h = cy;
	}

	return TRUE;
}

int XBX_unlink( const char* filename )
{	
	bool bSuccess = DeleteFile( filename ) != 0;
	if ( !bSuccess )
	{
		if ( GetLastError() == ERROR_FILE_NOT_FOUND )
		{
			// not a real failure
			return 0;
		}
	}
	// 0 = sucess, -1 = failure
	return bSuccess ? 0 : -1;
}

int XBX_mkdir( const char *pszDir )
{
	char			dirPath[MAX_PATH];
	char*			ptr;
	BOOL			bSuccess;

	// prime and skip to first seperator after the drive path
	// must create directory one path at a time
	bSuccess = false;
	strcpy( dirPath, pszDir );
	ptr = strchr( dirPath, '\\' );
	while ( ptr )
	{		
		ptr = strchr( ptr+1, '\\' );
		if ( ptr )
		{
			*ptr = '\0';
			bSuccess = CreateDirectory( dirPath, XBOX_DONTCARE );
			if ( !bSuccess && GetLastError() == ERROR_ALREADY_EXISTS )
			{
				// not a real error
				bSuccess = true;
			}
			*ptr = '\\';
		}
	}

	return ( bSuccess ? 0 : -1 );
}

char *XBX_getcwd( char *buf, size_t size )
{
	if ( !buf )
	{
		buf = (char*)malloc( 4 );
	}
	strncpy( buf, "D:", size );
	return buf;
}

int XBX_access( const char *path, int mode )
{
	if ( !path )
	{
		return -1;
	}

	// get the fatx attributes
	DWORD dwAttr = GetFileAttributes( path );
	if ( dwAttr == (DWORD)-1 )
	{
		return -1;
	}

	if ( mode == 0 )
	{
		// is file exist?
		return 0;
	}
	else if ( mode == 2 )
	{
		// is file write only?
		return -1;
	}
	else if ( mode == 4 )
	{
		// is file read only?
		if ( dwAttr & FILE_ATTRIBUTE_READONLY )
			return 0;
		else
			return -1;
	}
	else if ( mode == 6 )
	{
		// is file read and write?
		if ( !( dwAttr & FILE_ATTRIBUTE_READONLY ) )
			return 0;
		else
			return -1;
	}

	return -1;
}

DWORD XBX_GetCurrentDirectory( DWORD nBufferLength, LPTSTR lpBuffer )
{
	XBX_getcwd( lpBuffer, nBufferLength );
	return strlen( lpBuffer );
}

DWORD XBX_GetModuleFileName( HMODULE hModule, LPTSTR lpFilename, DWORD nSize )
{
	int		len;
	char	*pStr;
	char	*pEnd;
	char	xexName[MAX_PATH];

	if ( hModule == GetModuleHandle( NULL ) )
	{
		// isolate xex of command line
		pStr = GetCommandLine();
		if ( pStr )
		{
			// cull possible quotes around xex
			if ( pStr[0] == '\"' )
			{
				pStr++;
				pEnd = strchr( pStr, '\"' );
				if ( !pEnd )
				{
					// no ending matching quote
					return 0;
				}
			}
			else
			{
				// find possible first argument
				pEnd = strchr( lpFilename, ' ' );
				if ( !pEnd )
				{
					pEnd = pStr+strlen( pStr );
				}
			}
			len = pEnd-pStr;
			memcpy( xexName, pStr, len );
			xexName[len] = '\0';

			len = _snprintf( lpFilename, nSize, "D:\\%s", xexName );
			if ( len == -1 )
				lpFilename[nSize-1] = '\0';

			return strlen( lpFilename );
		}
	}
	return 0;
}
