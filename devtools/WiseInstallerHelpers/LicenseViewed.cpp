//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Checks to see if the user has completely viewed the EULA before activing the accept/decline radio buttons
//
//=============================================================================//
#include < windows.h >
#include < msi.h >
#include < msiquery.h >
#include <tchar.h>
#include <stdarg.h>
#include <stdio.h>
#include <commctrl.h>

#define CHILD_WINDOW_OF_LICENSE			"vHackLicenseWindowSibling092304"
#define WINDOW_CLASS_OF_EULA_WINDOW		"RichEdit20W"
#define ALLOWABLE_POS_FROM_BOTTOM	5

//-----------------------------------------------------------------------------
// Purpose: Call back function for EnumChildWindows
// Input  : hwnd - 
//			lParam - 
// Output : BOOL CALLBACK
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumChildProc(HWND hwnd,LPARAM lParam)
{
	TCHAR buf[100];
	
	GetClassName( hwnd, (LPTSTR)&buf, 100 );
	if ( _tcscmp( buf, _T( WINDOW_CLASS_OF_EULA_WINDOW ) ) == 0 )
	{
		*(HWND*)lParam = hwnd;
		return FALSE;
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Context for searching sub windows...
//-----------------------------------------------------------------------------
struct FindParams_t
{
	HWND		wnd;
	char		searchtext[ 512 ];
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwnd - 
//			lParam - 
// Output : BOOL CALLBACK
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumChildrenLookingForSpecialControl(HWND hwnd,LPARAM lParam)
{
	FindParams_t *p = ( FindParams_t *)lParam;

	char buf[ 512 ];
	GetWindowText( hwnd, buf, sizeof( buf ) );

	if ( !stricmp( buf, p->searchtext ) )
	{
		p->wnd = hwnd;   
		return FALSE;
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwnd - 
//			lParam - 
// Output : BOOL CALLBACK
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumChildWindowsProc(HWND hwnd, LPARAM lParam)
{
	// Now search for the special hidden text control inside a top level window

	FindParams_t *p = ( FindParams_t *)lParam;

	FindParams_t special;
	memset( &special, 0, sizeof( special ) );
	strcpy( special.searchtext, p->searchtext );

	EnumChildWindows( hwnd, EnumChildrenLookingForSpecialControl, (LPARAM)&special );
	if ( special.wnd != NULL )
	{
		p->wnd = hwnd;
		return FALSE;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Finds given a root window, finds a child window which itself has a special child window with the specified text string as the window title.
//  e.g.:
// if root == NULL (desktop), then you can search the top level dialogs (Half-Life 2 Setup,e.g.) for a subwindow with "vHackxxx" as the text, this would
//  return the appropriate top level dialog.
// Input  : root - 
//			*text - 
// Output : HWND
//-----------------------------------------------------------------------------
HWND FindWindowHavingChildWithSpecifiedText( HWND root, char const *text )
{
	FindParams_t params;
	memset( &params, 0, sizeof( params ) );

	strncpy( params.searchtext, text, sizeof( params.searchtext ) - 1 );

	EnumChildWindows( root, EnumChildWindowsProc, (LPARAM)&params );

	return params.wnd;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *text - 
// Output : HWND
//-----------------------------------------------------------------------------
HWND FindTopLevelWindowHavingChildWithSpecifiedText( char const *text )
{
	return FindWindowHavingChildWithSpecifiedText( GetDesktopWindow(), text );
}


//***********************************************************
//** Custom action to check if the license is completly 
//** viewed. 
//**---------------------------------------------------------
//** Return values:
//** 	(1) Always returns success
//**    (2) Sets the private property LicenseViewed when 
//**        the text is scrolled to near end.
//**---------------------------------------------------------
//** Usage:
//** 	(1) Find installer window by looking for child with specified text.  Can't
//**		just use window title because it's translated into foreign languages
//**	(2) In the license agreement dialog, on the scrollable 
//** 	    text control event, call the custom action 
//** 	    CheckLicenseViewed using a DoAction
//** 	(3) For the Next button, modify the Enable 
//** 	    ControlCondition to include the property 
//** 	    LicenseViewed
//**---------------------------------------------------------
//***********************************************************

// Needs to be exported as non-__stdcall with C name mangling (no mangling)
extern "C" __declspec( dllexport ) UINT CheckLicenseViewed(MSIHANDLE hMSI)
{
	HWND hWnd;
	HWND hWndChild=NULL;

	if (MsiEvaluateCondition(hMSI,"LicenseViewed")==MSICONDITION_FALSE)
	{
		hWnd = FindTopLevelWindowHavingChildWithSpecifiedText(CHILD_WINDOW_OF_LICENSE );
		if (hWnd) 
		{
			EnumChildWindows( hWnd, EnumChildProc, (LPARAM)&hWndChild );
			if ( hWndChild ) 
			{
				SCROLLINFO sinfo;
				memset( &sinfo, 0, sizeof( sinfo ) );
				sinfo.cbSize=sizeof(sinfo);
				sinfo.fMask=SIF_TRACKPOS | SIF_RANGE | SIF_POS | SIF_PAGE;
				GetScrollInfo(hWndChild, SB_VERT, &sinfo);
				//max range depends on page size
				UINT MaxScrollPos = sinfo.nMax  - ( sinfo.nPage - 1 );
				//max less, say ALLOWABLE_POS_FROM_BOTTOM - an allowable max.
				MaxScrollPos = MaxScrollPos - ALLOWABLE_POS_FROM_BOTTOM;

				// THIS IS A HACK, but the RichEdit20W control has a bug where while the thumb is being dragged, the position doesn't
				//  get updated.  In fact, it doesn't get updated until the next time
				UINT nScrollPos = ::SendMessage(hWndChild, EM_GETTHUMB, 0, 0);

				bool trackedPastEnd = false; // (UINT)sinfo.nTrackPos >= MaxScrollPos ? true : false;
				bool positionedPastEnd = nScrollPos >= MaxScrollPos ? true : false;
				// Note above, this method doesn't always work... but maybe it will work in win98 and the other won't, etc. etc.
				bool positionedPastEnd2 = (UINT)sinfo.nPos >= MaxScrollPos ? true : false;

				/*
				
				HDC dc = GetDC( GetDesktopWindow() );
				COLORREF oldColor = SetTextColor( dc, RGB( 255, 0, 0 ) );

				char sz[ 256 ];
				_snprintf( sz, sizeof( sz ), "max %i page %i msp %i track %u pos %u pos2 %u",
					sinfo.nMax,
					sinfo.nPage,
					MaxScrollPos,
					sinfo.nTrackPos,
					sinfo.nPos,
					nScrollPos );

				RECT rc;
				rc.left = 0;
				rc.right = 1000;
				rc.top = 20;
				rc.bottom = 50;

				DrawText( dc, sz, -1, &rc, DT_NOPREFIX );

				SetTextColor( dc, oldColor );
				ReleaseDC( GetDesktopWindow(), dc );
				*/

				if ( trackedPastEnd || positionedPastEnd || positionedPastEnd2 ) 
				{
					MsiSetProperty(hMSI, TEXT("LicenseViewed"), TEXT("1"));
				}
				else
				{
					MsiSetProperty(hMSI, TEXT("LicenseViewed"), NULL);
				}
			}
		}
	}
	return ERROR_SUCCESS;
}