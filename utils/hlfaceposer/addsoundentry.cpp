//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "mxtk/mx.h"
#include "resource.h"
#include "AddSoundEntry.h"
#include "mdlviewer.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "filesystem.h"

static CAddSoundParams g_Params;

static void PopulateScriptList( HWND wnd )
{
	HWND control = GetDlgItem( wnd, IDC_SOUNDSCRIPT );
	if ( !control )
	{
		return;
	}

	SendMessage( control, CB_RESETCONTENT, 0, 0 ); 

	int c = soundemitter->GetNumSoundScripts();
	for ( int i = 0; i < c; i++ )
	{
		// add text to combo box
		SendMessage( control, CB_ADDSTRING, 0, (LPARAM)soundemitter->GetSoundScriptName( i ) );

		if ( i == 0 && !g_Params.m_szScriptName[ 0 ] )
		{
			SendMessage( control, WM_SETTEXT , 0, (LPARAM)soundemitter->GetSoundScriptName( i ) );
		}
	}

	if ( g_Params.m_szScriptName[ 0 ] )
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)g_Params.m_szScriptName );
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
static BOOL CALLBACK AddSoundPropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )  
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		// Insert code here to put the string (to find and replace with)
		// into the edit controls.
		// ...
		{
			g_Params.PositionSelf( hwndDlg );

			PopulateScriptList( hwndDlg );

			SetDlgItemText( hwndDlg, IDC_SOUNDNAME, g_Params.m_szSoundName );
			if ( g_Params.m_bReadOnlySoundName )
			{
				HWND ctrl = GetDlgItem( hwndDlg, IDC_SOUNDNAME );
				if ( ctrl )
				{
					SendMessage( ctrl, EM_SETREADONLY, (WPARAM)TRUE, 0 );
				}
			}

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_SOUNDNAME ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				g_Params.m_szSoundName[ 0 ] = 0;
				g_Params.m_szScriptName[ 0 ] = 0;
				GetDlgItemText( hwndDlg, IDC_SOUNDNAME, g_Params.m_szSoundName, sizeof( g_Params.m_szSoundName ) );
				GetDlgItemText( hwndDlg, IDC_SOUNDSCRIPT, g_Params.m_szScriptName, sizeof( g_Params.m_szScriptName ) );

				// Don't exit...
				if ( !g_Params.m_szSoundName[ 0 ] || !g_Params.m_szScriptName[ 0 ] )
					return TRUE;

				// Don't stompt existing sounds
				int idx = soundemitter->GetSoundIndex( g_Params.m_szSoundName );
				if ( soundemitter->IsValidIndex( idx ) )
				{
					if ( !g_Params.m_bAllowExistingSound )
					{
						mxMessageBox( NULL, va( "Sound '%s' already exists",	
							g_Params.m_szSoundName ), g_appTitle, MX_MB_OK );
					}
					else
					{
						EndDialog( hwndDlg, 1 );
					}
					return TRUE;
				}

				// Check out script
				if ( !filesystem->FileExists( g_Params.m_szScriptName ) )
				{
					mxMessageBox( NULL, va( "Script '%s' does not exist",
						g_Params.m_szScriptName ), g_appTitle, MX_MB_OK );
					return TRUE;
				}

				if ( !filesystem->IsFileWritable( g_Params.m_szScriptName ) )
				{
					mxMessageBox( NULL, va( "Script '%s' is read-only, you need to check it out of VSS",
						g_Params.m_szScriptName ), g_appTitle, MX_MB_OK );
					return TRUE;
				}

				// Add the entry
				CSoundParametersInternal params;
				params.SetChannel( CHAN_VOICE );
				params.SetSoundLevel( SNDLVL_TALKING );
				
				soundemitter->ExpandSoundNameMacros( params, g_Params.m_szWaveFile );
				soundemitter->AddSound( g_Params.m_szSoundName, g_Params.m_szScriptName, params );

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
int AddSound( CAddSoundParams *params, HWND parent )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_ADDSOUNDENTRY ),
		parent,
		(DLGPROC)AddSoundPropertiesDialogProc );

	*params = g_Params;

	return retval;
}