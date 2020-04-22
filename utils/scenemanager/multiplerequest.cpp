//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "resource.h"
#include "MultipleRequest.h"
#include "workspacemanager.h"

static CMultipleParams g_Params;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK MultipleRequestDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			SetDlgItemText( hwndDlg, IDC_PROMPT, g_Params.m_szPrompt );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_INPUTSTRING ) );
			SendMessage( GetDlgItem( hwndDlg, IDC_INPUTSTRING ), EM_SETSEL, 0, MAKELONG(0, 0xffff) );

		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_YESALL:
			EndDialog( hwndDlg, CMultipleParams::YES_ALL );
			break;
		case IDC_YES:
			EndDialog( hwndDlg, CMultipleParams::YES );
			break;
		case IDC_NOSINGLE:
			EndDialog( hwndDlg, CMultipleParams::NO );
			break;
		case IDC_NOALL:
			EndDialog( hwndDlg, CMultipleParams::NO_ALL );
			break;
        //case IDCANCEL:
		//	EndDialog( hwndDlg, CMultipleParams::CANCEL );
		//	break;
		}
		return TRUE;
	}
	return FALSE;
}

static int g_MRContext = 1;
static int g_MRCurrentContext;
static int g_MRLastResult = -1;

void MultipleRequestChangeContext()
{
	++g_MRContext;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *view - 
//			*actor - 
// Output : int
//-----------------------------------------------------------------------------
int _MultipleRequest( CMultipleParams *params )
{
	int retval = -1;

	if ( g_MRCurrentContext == g_MRContext &&
		g_MRLastResult != -1 )
	{
		if ( g_MRLastResult == CMultipleParams::YES_ALL )
		{
			retval = 0;
		}
		if ( g_MRLastResult == CMultipleParams::NO_ALL )
		{
			retval = 1;
		}
	}

	if ( retval == -1 )
	{
		g_Params = *params;
	
		retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
			MAKEINTRESOURCE( IDD_MULTIPLEQUESTION ),
			(HWND)GetWorkspaceManager()->getHandle(),
			(DLGPROC)MultipleRequestDialogProc );
	
		*params = g_Params;
	}

	g_MRCurrentContext = g_MRContext;
	g_MRLastResult = retval;
	switch ( retval )
	{
	case CMultipleParams::YES_ALL:
	case CMultipleParams::YES:
		return 0;
	case CMultipleParams::NO_ALL:
	case CMultipleParams::NO:
		return 1;
	default:
	case CMultipleParams::CANCEL:
		return 2;
	}

	Assert( 0 );
	return 1;
}

int MultipleRequest( char const *prompt )
{
	CMultipleParams params;
	memset( &params, 0, sizeof( params ) );
	Q_strcpy( params.m_szDialogTitle, g_appTitle );
	Q_strncpy( params.m_szPrompt, prompt, sizeof( params.m_szPrompt ) );

	return _MultipleRequest( &params );
}