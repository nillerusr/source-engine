//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include <mxtk/mx.h>
#include <stdio.h>
#include "resource.h"
#include "WaveProperties.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundentry.h"
#include "cmdlib.h"
#include "workspacemanager.h"
#include "wavebrowser.h"
#include "wavefile.h"
#include "multiplerequest.h"

static CWaveParams g_Params;

static bool WaveLessFunc( const char *const& name1, const char *const& name2 )
{
	if ( stricmp( name1, name2 ) < 0 )
		return true;
	return false;
}

static void WaveProperties_OnOK( HWND hwndDlg )
{
	// Gather info and make changes
	CWaveFile *item = g_Params.items[ 0 ];
	char sentencetext[ 512 ];
						
	GetDlgItemText( hwndDlg, IDC_SENTENCETEXT, sentencetext, sizeof( sentencetext ) );

	bool voiceduck = SendMessage( GetDlgItem( hwndDlg, IDC_VOICEDUCK ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;

	MultipleRequestChangeContext();

	// Update ducking on wav files
	item->SetVoiceDuck( voiceduck );
	item->SetSentenceText( sentencetext );

	// Repopulate things
	GetWorkspaceManager()->RefreshBrowsers();
}

char *Q_stristr_slash( char const *pStr, char const *pSearch );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//-----------------------------------------------------------------------------
static void WaveProperties_ExportSentence()
{
	int c = g_Params.items.Count();
	if ( c <= 0 )
		return;

	MultipleRequestChangeContext();

	int i;
	for ( i = 0; i < c; i++ )
	{
		CWaveFile *item = g_Params.items[ i ];
		Assert( item );

		char relative[ 512 ];
		item->GetPhonemeExportFile( relative, sizeof( relative ) );
		if ( filesystem->FileExists( relative ) )
		{
			int retval = MultipleRequest( va( "Overwrite '%s'?", relative ) );	
			if ( retval != 0 )
				continue;

			filesystem->RemoveFile( relative );
		}

		item->ExportValveDataChunk( relative );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//-----------------------------------------------------------------------------
static void WaveProperties_ImportSentence()
{
	int c = g_Params.items.Count();
	if ( c <= 0 )
		return;

	int i;
	for ( i = 0; i < c; i++ )
	{
		CWaveFile *item = g_Params.items[ i ];
		Assert( item );

		char relative[ 512 ];
		item->GetPhonemeExportFile( relative, sizeof( relative ) );
		if ( filesystem->FileExists( relative ) )
		{
			item->ImportValveDataChunk( relative );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
// Output : static
//-----------------------------------------------------------------------------
static void WaveProperties_InitSentenceData( HWND hwndDlg )
{
	CWaveFile *item = g_Params.items[ 0 ];

	SetDlgItemText( hwndDlg, IDC_WAVENAME, item->GetName() );
	SetDlgItemText( hwndDlg, IDC_SENTENCETEXT, item->GetSentenceText() );

	SendMessage( GetDlgItem( hwndDlg, IDC_VOICEDUCK ), BM_SETCHECK, 
		( WPARAM ) item->GetVoiceDuck() ? BST_CHECKED : BST_UNCHECKED,
		( LPARAM )0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK WavePropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		{
			g_Params.PositionSelf( hwndDlg );

			WaveProperties_InitSentenceData( hwndDlg );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_WAVENAME ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				WaveProperties_OnOK( hwndDlg );

				EndDialog( hwndDlg, 1 );
			}
			break;
        case IDCANCEL:
			EndDialog( hwndDlg, 0 );
			break;
		case IDC_EXPORTSENTENCE:
			WaveProperties_ExportSentence();
			break;
		case IDC_IMPORTSENTENCE:
			WaveProperties_ImportSentence();
			WaveProperties_InitSentenceData( hwndDlg );
			break;
		}
		return FALSE;
	}
	return FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *view - 
//			*actor - 
// Output : int
//-----------------------------------------------------------------------------
int WaveProperties( CWaveParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_WAVEPROPERTIES ),
		(HWND)GetWorkspaceManager()->getHandle(),
		(DLGPROC)WavePropertiesDialogProc );

	*params = g_Params;

	return retval;
}