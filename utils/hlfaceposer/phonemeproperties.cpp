//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "hlfaceposer.h"
#include <mxtk/mx.h>
#include "resource.h"
#include "PhonemeProperties.h"
#include "expressions.h"
#include "expclass.h"
#include "mdlviewer.h"

static CPhonemeParams g_Params;

static int		g_nPhonemeCount = 0;
static HWND		*g_rgButtons = NULL;

#define IDC_PHONEME			2000

#define PHONEME_WIDTH		50
#define PHONEME_HEIGHT		18
#define PHONEME_GAP			10
#define PHONEME_VGAP		5

typedef long (__stdcall *WINPROCTYPE)( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
static WINPROCTYPE lpfnOldButtonProc;

static BOOL CALLBACK PhonemeBtnProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
	case WM_MOUSEMOVE:
		{
			HWND dialog = GetParent( hwnd );
			if ( dialog )
			{
				// Get the hint text item
				HWND helpText = GetDlgItem( dialog, IDC_STATIC_HELPTEXT );
				if ( helpText )
				{
					CExpression *exp = ( CExpression * )GetWindowLong( (HWND)hwnd, GWL_USERDATA );
					if ( exp )
					{
						SendMessage( helpText, WM_SETTEXT, 0, (LPARAM)exp->description );
					}
				}
			}
		}
		return 0;
	default:
		break;
	}

	return CallWindowProc( lpfnOldButtonProc, hwnd, uMsg, wParam, lParam );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : phoneme - 
// Output : static void
//-----------------------------------------------------------------------------
static void ClickedPhoneme( HWND hwndDlg, int phoneme )
{
	HWND ctrl = GetDlgItem( hwndDlg, IDC_EDIT_PHONEME );
	if ( !ctrl )
		return;

	if ( !g_Params.m_bMultiplePhoneme )
	{
		g_Params.m_szName[ 0 ] = 0;
	}
	else
	{
		SendMessage( ctrl, WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szName ), (LPARAM)g_Params.m_szName );
	}
	if ( phoneme >= g_nPhonemeCount || phoneme < 0 )
	{
		Assert( 0 );
		return;
	}

	HWND button = g_rgButtons[ phoneme ];
	CExpression *exp = ( CExpression * )GetWindowLong( (HWND)button, GWL_USERDATA );
	if ( exp )
	{
		if ( strlen( g_Params.m_szName ) > 0 )
		{
			strcat( g_Params.m_szName, " " );
		}
		strcat( g_Params.m_szName, exp->name );

		if ( g_Params.m_bMultiplePhoneme )
		{
			SetFocus( ctrl );
			SendMessage( ctrl, WM_SETTEXT, 0, (LPARAM)g_Params.m_szName );
			SendMessage( ctrl, EM_SETSEL, 0, MAKELONG(0, 0xffff) );
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
// Output : static void
//-----------------------------------------------------------------------------
static void CreateAndLayoutControls( HWND hwndDlg, CPhonemeParams* params )
{
	g_nPhonemeCount = 0;
	// Find phomemes
	// Make sure phonemes are loaded
	FacePoser_EnsurePhonemesLoaded();

	CExpClass *cl = expressions->FindClass( "phonemes", true );
	if ( !cl )
		return;

	g_nPhonemeCount = cl->GetNumExpressions();
	if ( g_nPhonemeCount == 0 )
		return;

	g_rgButtons = new HWND[ g_nPhonemeCount ];
	Assert( g_rgButtons );

	int columns = 7;
	int rows = ( g_nPhonemeCount / columns ) + 1;

	int dialogW = columns * ( PHONEME_WIDTH + PHONEME_GAP ) + 2 * PHONEME_GAP;
	int dialogH = rows * ( PHONEME_HEIGHT + PHONEME_VGAP ) + 40 + 55 + 30;

	int startx = PHONEME_GAP;
	int starty = 40;

	if ( params->m_bPositionDialog )
	{
		int top = params->m_nTop - dialogH - 5;
		int left = params->m_nLeft;

		MoveWindow( hwndDlg,
			left,
			top,
			dialogW,
			dialogH,
			TRUE );
	}
	else
	{
		MoveWindow( hwndDlg, 
			( GetSystemMetrics( SM_CXFULLSCREEN ) - dialogW ) / 2,
			( GetSystemMetrics( SM_CYFULLSCREEN ) - dialogH ) / 2,
			dialogW,
			dialogH,
			TRUE );
	}

	HWND ctrl = GetDlgItem( hwndDlg, IDOK );
	if ( ctrl )
	{
		MoveWindow( ctrl, dialogW - 220, dialogH - 58, 100, 20, TRUE );
	}
	ctrl = GetDlgItem( hwndDlg, IDCANCEL );
	if ( ctrl )
	{
		MoveWindow( ctrl, dialogW - 110, dialogH - 58, 100, 20, TRUE );
	}
	ctrl = GetDlgItem( hwndDlg, IDC_PHONEMETEXTPROMPT );
	if ( ctrl )
	{
		MoveWindow( ctrl, startx, dialogH - 55, 50, 20, TRUE );
	}
	ctrl = GetDlgItem( hwndDlg, IDC_EDIT_PHONEME );
	if ( ctrl )
	{
		MoveWindow( ctrl, startx + 50, dialogH - 58, 100, 20, TRUE );
	}
	ctrl = GetDlgItem( hwndDlg, IDC_STATIC_HELPTEXT );
	if ( ctrl )
	{
		MoveWindow( ctrl, startx, dialogH - 85, dialogW - startx - 20, 20, TRUE );
	}

	int r = 0; 
	int c = 0;
	for ( int i = 0; i < g_nPhonemeCount; i++ )
	{
		CExpression *exp = cl->GetExpression( i );
		if ( !exp )
			continue;

		HWND button = CreateWindowEx( 
			0,
			"BUTTON",
			va( "%s", exp->name ),
			WS_CHILD | WS_VISIBLE | BS_LEFT,
			startx + c * ( PHONEME_WIDTH + PHONEME_GAP ),
			starty + r * ( PHONEME_HEIGHT + PHONEME_VGAP ),
			PHONEME_WIDTH,
			PHONEME_HEIGHT,
			hwndDlg,
			(HMENU)( IDC_PHONEME + i ),
			(HINSTANCE)GetModuleHandle( 0 ),
			NULL );
		Assert( button );
		SetWindowLong( (HWND)button, GWL_USERDATA, (LONG)exp );

		// Subclass it
		lpfnOldButtonProc = (WINPROCTYPE)SetWindowLong( (HWND)button, GWL_WNDPROC, (LONG)PhonemeBtnProc );

		SendMessage ((HWND)button, WM_SETFONT, (WPARAM) (HFONT) GetStockObject (ANSI_VAR_FONT), MAKELPARAM (TRUE, 0));

		g_rgButtons[ i ] = button;

		c++;
		if ( c >= columns )
		{
			r++;
			c = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
// Output : static void
//-----------------------------------------------------------------------------
static void DestroyControls( HWND hwndDlg )
{
	for ( int i = 0 ; i < g_nPhonemeCount; i++ )
	{
		if ( g_rgButtons[ i ] )
		{
			DestroyWindow( g_rgButtons[ i ] );
			g_rgButtons[ i ] = NULL;
		}
	}

	delete[] g_rgButtons;
	g_nPhonemeCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
// Output : static void
//-----------------------------------------------------------------------------
static void PhonemePropertiesDialogExit( HWND hwndDlg, int exitCode )
{
	DestroyControls( hwndDlg );

	EndDialog( hwndDlg, exitCode );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : allowmultiple - 
//			*input - 
//			*output - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool ValidatePhonemeString( bool allowmultiple, char const *input, char *output )
{
	// Make sure phonemes are loaded
	FacePoser_EnsurePhonemesLoaded();

	CExpClass *cl = expressions->FindClass( "phonemes", true );
	if ( !cl )
		return false;

	if ( !input || !input[ 0 ] )
		return false;

	// Go one by one
	int count = 1;
	char phoneme[ 128 ];
	char *in, *out;

	*output = 0;

	in = (char *)input;
	do
	{
		out = phoneme;

		while ( *in > 32 )
		{
			*out++ = *in++;
		}
		*out = 0;

		// Validate phoneme entered
		for ( int i = 0; i < g_nPhonemeCount; i++ )
		{
			CExpression *exp = cl->GetExpression( i );
			if ( !exp )
				continue;
			
			if ( !stricmp( exp->name, phoneme ) )
			{
				// Found it
				if ( count != 1 )
				{
					strcat( output, " " );
				}
				strcat( output, phoneme );
				break;
			}
		}

		if ( !*in )
			break;

		// Skip whitespace
		in++;
		count++;

		// Only keep first one
		if ( !allowmultiple )
			break;

	} while ( 1 );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK PhonemePropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			CreateAndLayoutControls( hwndDlg, &g_Params );

			HWND control = GetDlgItem( hwndDlg, IDC_PHONEMENAME );
			if ( control )
			{
				SendMessage( control, WM_SETTEXT , 0, 
					( LPARAM )( 
					g_Params.m_bMultiplePhoneme ?
					"Click or enter one or more phonemes from list below"
					:
					va( "Phoneme/Viseme:  %s", g_Params.m_szName ) ) );
			}

			control = GetDlgItem( hwndDlg, IDC_EDIT_PHONEME );
			if ( control )
			{
				SetFocus( control );
				SendMessage( control, WM_SETTEXT , 0, ( LPARAM )g_Params.m_szName );
				SendMessage( control, EM_SETSEL, 0, MAKELONG(0, 0xffff) );
				return FALSE;
			}
		}
		return TRUE;  
		
    case WM_COMMAND:
		{
			int cmd = LOWORD( wParam );

			if ( ( cmd >= IDC_PHONEME ) && 
				( cmd < ( IDC_PHONEME + g_nPhonemeCount )  ) )
			{
				ClickedPhoneme( hwndDlg, cmd - IDC_PHONEME );
				if ( !g_Params.m_bMultiplePhoneme )
				{
					PhonemePropertiesDialogExit( hwndDlg, 1 );
				}
			}
			else if ( cmd != IDC_EDIT_PHONEME )
			{
				switch ( cmd )
				{
				case IDOK:
					{
						// Retrieve text
						char szPhoneme[ 256 ];
						HWND ctrl = GetDlgItem( hwndDlg, IDC_EDIT_PHONEME );
						if ( ctrl )
						{
							SendMessage( ctrl, WM_GETTEXT, (WPARAM)sizeof( szPhoneme ), (LPARAM)szPhoneme );
							
							ValidatePhonemeString( g_Params.m_bMultiplePhoneme, szPhoneme, g_Params.m_szName );
						}
						PhonemePropertiesDialogExit( hwndDlg, 1 );
					}
					break;
				case IDCANCEL:
					PhonemePropertiesDialogExit( hwndDlg, 0 );
					break;
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *view - 
//			*actor - 
// Output : int
//-----------------------------------------------------------------------------
int PhonemeProperties( CPhonemeParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_PHONEMEPROPERTIES ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)PhonemePropertiesDialogProc );

	*params = g_Params;

	return retval;
}