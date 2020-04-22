//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "resource.h"
#include "ChoiceProperties.h"
#include <mxtk/mx.h>
#include "mdlviewer.h"

static CChoiceParams g_Params;

static void PopulateChoiceList( HWND wnd, CChoiceParams *params )
{
	HWND control = GetDlgItem( wnd, IDC_CHOICE );
	if ( !control )
		return;

	SendMessage( control, CB_RESETCONTENT, 0, 0 ); 

	int c = params->m_Choices.Count();

	if ( params->m_nSelected == -1 )
		params->m_nSelected = 0;

	if ( params->m_nSelected >= 0 && params->m_nSelected < c )
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)params->m_Choices[ params->m_nSelected ].choice );
	}

	for ( int i = 0; i < c; i++ )
	{
		char const *text = params->m_Choices[ i ].choice;
		SendMessage( control, CB_ADDSTRING, 0, (LPARAM)text ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK ChoicePropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			PopulateChoiceList( hwndDlg, &g_Params );

			SetDlgItemText( hwndDlg, IDC_STATIC_PROMPT, g_Params.m_szPrompt );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );
		}
		return TRUE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				char selected[ MAX_CHOICE_TEXT_SIZE ];
				selected[ 0 ] = 0;
				HWND control = GetDlgItem( hwndDlg, IDC_CHOICE );
				if ( control )
				{
					SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( selected ), (LPARAM)selected );
				}

				g_Params.m_nSelected = -1;
				int c = g_Params.m_Choices.Count();

				for ( int i = 0; i < c; i++ )
				{
					char const *text = g_Params.m_Choices[ i ].choice;
					if ( stricmp( text, selected ) )
					{
						continue;
					}

					g_Params.m_nSelected = i;
					break;
				}

				EndDialog( hwndDlg, 1 );
			}
			break;
        case IDCANCEL:
			EndDialog( hwndDlg, 0 );
			break;
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
int ChoiceProperties( CChoiceParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_CHOICEDIALOG ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)ChoicePropertiesDialogProc );

	*params = g_Params;

	return retval;
}