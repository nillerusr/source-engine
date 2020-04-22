//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include <mxtk/mx.h>
#include <stdio.h>
#include "resource.h"
#include "SoundProperties_Multiple.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundentry.h"
#include "cmdlib.h"
#include "workspacemanager.h"
#include "wavebrowser.h"
#include "wavefile.h"
#include "waveproperties.h"

static CSoundParams g_Params;

static void PopulateChannelList( HWND wnd, CSoundParametersInternal *p )
{
	HWND control = GetDlgItem( wnd, IDC_CHANNEL );
	if ( !control )
	{
		return;
	}

	SendMessage( control, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( control, WM_SETTEXT , 0, (LPARAM)p->ChannelToString() );

	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"CHAN_VOICE" ); 

	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"CHAN_AUTO" ); 
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"CHAN_WEAPON" ); 
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"CHAN_ITEM" ); 
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"CHAN_BODY" ); 
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"CHAN_STREAM" ); 
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"CHAN_STATIC" ); 
}

static void PopulateVolumeList( HWND wnd, CSoundParametersInternal *p )
{
	HWND control = GetDlgItem( wnd, IDC_VOLUME );
	if ( !control )
	{
		return;
	}

	SendMessage( control, CB_RESETCONTENT, 0, 0 ); 

	SendMessage( control, WM_SETTEXT , 0, (LPARAM)p->VolumeToString() );

	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"VOL_NORM" ); 
}

static void PopulateSoundlevelList( HWND wnd, CSoundParametersInternal *p )
{
	HWND control = GetDlgItem( wnd, IDC_SOUNDLEVEL );
	if ( !control )
	{
		return;
	}

	SendMessage( control, CB_RESETCONTENT, 0, 0 ); 

	SendMessage( control, WM_SETTEXT , 0, (LPARAM)p->SoundLevelToString() );

	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_NORM" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_NONE" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_IDLE" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_TALKING" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_STATIC" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_GUNFIRE" );

	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_25dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_30dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_35dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_40dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_45dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_50dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_55dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_60dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_65dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_70dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_75dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_80dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_85dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_90dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_95dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_100dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_105dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_120dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_130dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_140dB" );
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"SNDLVL_150dB" );
}

static void PopulatePitchList( HWND wnd, CSoundParametersInternal *p )
{
	HWND control = GetDlgItem( wnd, IDC_PITCH );
	if ( !control )
	{
		return;
	}

	SendMessage( control, CB_RESETCONTENT, 0, 0 ); 

	SendMessage( control, WM_SETTEXT , 0, (LPARAM)p->PitchToString() );

	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"PITCH_NORM" ); 
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"PITCH_LOW" ); 
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"PITCH_HIGH" ); 
}

static void SoundProperties_Multiple_OnOK( HWND hwndDlg )
{
	int c = g_Params.items.Count();
	for ( int i = 0; i < c; i++ )
	{
		// Gather info and make changes
		CSoundEntry *item = g_Params.items[ i ];
		Assert( item );
		if ( !item )
			continue;

		int idx = g_pSoundEmitterSystem->GetSoundIndex( item->GetName() );
		if ( !g_pSoundEmitterSystem->IsValidIndex( idx ) )
		{
			continue;
		}


		CSoundParametersInternal *baseparams = g_pSoundEmitterSystem->InternalGetParametersForSound( idx );
		if ( !baseparams )
			return;

		// Start with old settings
		CSoundParametersInternal outparams;
		outparams.CopyFrom( *baseparams );

		char sz[ 128 ];
		GetDlgItemText( hwndDlg, IDC_CHANNEL, sz, sizeof( sz ) );
		outparams.ChannelFromString( sz );

		GetDlgItemText( hwndDlg, IDC_VOLUME, sz, sizeof( sz ) );
		outparams.VolumeFromString( sz );

		GetDlgItemText( hwndDlg, IDC_SOUNDLEVEL, sz, sizeof( sz ) );
		outparams.SoundLevelFromString( sz );

		GetDlgItemText( hwndDlg, IDC_PITCH, sz, sizeof( sz ) );
		outparams.PitchFromString( sz );

		bool owneronly = SendMessage( GetDlgItem( hwndDlg, IDC_OWNERONLY ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;

		outparams.SetOnlyPlayToOwner( owneronly );

		g_pSoundEmitterSystem->UpdateSoundParameters( item->GetName(), outparams );
	}

	// Repopulate things
	GetWorkspaceManager()->RefreshBrowsers();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK SoundProperties_MultipleDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		{
			g_Params.PositionSelf( hwndDlg );

			CSoundEntry *entry = g_Params.items[ 0 ];
			Assert( entry );

			CSoundParametersInternal *p = entry->GetSoundParameters();
			Assert( p );

			SendMessage( GetDlgItem( hwndDlg, IDC_OWNERONLY ), BM_SETCHECK, 
				( WPARAM ) p->OnlyPlayToOwner() ? BST_CHECKED : BST_UNCHECKED,
				( LPARAM )0 );

			PopulateChannelList( hwndDlg, p );
			PopulateVolumeList( hwndDlg, p );
			PopulateSoundlevelList( hwndDlg, p );
			PopulatePitchList( hwndDlg, p );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_CHANNEL ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				SoundProperties_Multiple_OnOK( hwndDlg );

				EndDialog( hwndDlg, 1 );
			}
			break;
        case IDCANCEL:
			EndDialog( hwndDlg, 0 );
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
int SoundProperties_Multiple( CSoundParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_SOUNDPROPERTIES_MULTIPLE ),
		(HWND)GetWorkspaceManager()->getHandle(),
		(DLGPROC)SoundProperties_MultipleDialogProc );

	*params = g_Params;

	return retval;
}