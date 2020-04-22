//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "mxtk/mx.h"
#include "resource.h"
#include "SoundLookup.h"
#include "mdlviewer.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "addsoundentry.h"

static CSoundLookupParams g_Params;

static void PopulateSoundEntryList( HWND wnd, CSoundLookupParams *params )
{
	HWND control = GetDlgItem( wnd, IDC_SOUNDENTRYLIST );
	if ( !control )
		return;

	SendMessage( control, LB_RESETCONTENT, 0, 0 ); 
	SendMessage( control, WM_SETFONT, (WPARAM) (HFONT) GetStockObject (ANSI_FIXED_FONT), MAKELPARAM (TRUE, 0) );

	int c = params->entryList->Count();
	for ( int i = 0; i < c; i++ )
	{
		int soundindex = (*params->entryList)[ i ];
		char text[ 128 ];
		text[ 0 ]  = 0;
		Q_strncpy( text, soundemitter->GetSoundName( soundindex ), sizeof( text ) );
		if ( text[0] )
		{
			char const *script = soundemitter->GetSourceFileForSound( soundindex );

			int idx = SendMessage( control, LB_ADDSTRING, 0, (LPARAM)va( "%20s:  '%s'", script, text ) ); 
			SendMessage( control, LB_SETITEMDATA, idx, (LPARAM)soundindex );
		}
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
static BOOL CALLBACK SoundLookupDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			PopulateSoundEntryList( hwndDlg, &g_Params );

			SetDlgItemText( hwndDlg, IDC_STATIC_PROMPT, g_Params.m_szPrompt );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_SOUNDENTRYLIST ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				int selindex = SendMessage( GetDlgItem( hwndDlg, IDC_SOUNDENTRYLIST ), LB_GETCURSEL, 0, 0 );
				if ( selindex == LB_ERR )
				{
					mxMessageBox( NULL, "You must select an entry from the list", g_appTitle, MB_OK );
					return TRUE;
				}

				int soundindex = SendMessage( GetDlgItem( hwndDlg, IDC_SOUNDENTRYLIST ), LB_GETITEMDATA, selindex, 0 );

				Assert( soundindex != LB_ERR );

				Q_strncpy( g_Params.m_szSoundName, soundemitter->GetSoundName( soundindex ), sizeof ( g_Params.m_szSoundName ) );
				EndDialog( hwndDlg, 1 );
			}
			break;
        case IDCANCEL:
			EndDialog( hwndDlg, 0 );
			break;
		case IDC_ADDENTRY:
			{
				// Create a new sound entry for this sound
				CAddSoundParams params;
				Q_memset( &params, 0, sizeof( params ) );
				Q_strcpy( params.m_szDialogTitle, "Add Sound Entry" );
				Q_strcpy( params.m_szWaveFile, g_Params.m_szWaveFile );

				if ( AddSound( &params, hwndDlg ) )
				{
					// Add it to soundemitter and check out script files
					if ( params.m_szSoundName[ 0 ] && 
						 params.m_szScriptName[ 0 ] )
					{
						Q_strcpy( g_Params.m_szSoundName, params.m_szSoundName );
						// Press the OK button for the user...
						EndDialog( hwndDlg, 1 );
					}
				}
			}
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
int SoundLookup( CSoundLookupParams *params, HWND parent )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_WAVELOOKUP ),
		parent,
		(DLGPROC)SoundLookupDialogProc );

	*params = g_Params;

	return retval;
}