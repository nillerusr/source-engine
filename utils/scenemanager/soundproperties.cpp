//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include <mxtk/mx.h>
#include <stdio.h>
#include "resource.h"
#include "SoundProperties.h"
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
	if ( p )
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)p->ChannelToString() );
	}
	else
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)"CHAN_VOICE" );
	}

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

	if ( p )
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)p->VolumeToString() );
	}
	else
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)"VOL_NORM" );
	}

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

	if ( p )
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)p->SoundLevelToString() );
	}
	else
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)"SNDLVL_NORM" );
	}

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

	if ( p )
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)p->PitchToString() );
	}
	else
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)"PITCH_NORM" );
	}

	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"PITCH_NORM" ); 
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"PITCH_LOW" ); 
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"PITCH_HIGH" ); 
}


static void PopulateScriptList( HWND wnd, char const *curscript )
{
	HWND control = GetDlgItem( wnd, IDC_SOUNDSCRIPT );
	if ( !control )
	{
		return;
	}

	SendMessage( control, CB_RESETCONTENT, 0, 0 ); 

	if ( curscript )
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)curscript );
	}

	int c = g_pSoundEmitterSystem->GetNumSoundScripts();
	for ( int i = 0; i < c; i++ )
	{
		// add text to combo box
		SendMessage( control, CB_ADDSTRING, 0, (LPARAM)g_pSoundEmitterSystem->GetSoundScriptName( i ) );

		if ( !curscript && i == 0 )
		{
			SendMessage( control, WM_SETTEXT , 0, (LPARAM)g_pSoundEmitterSystem->GetSoundScriptName( i ) );
		}
	}
}

static bool WaveLessFunc( const char *const& name1, const char *const& name2 )
{
	if ( stricmp( name1, name2 ) < 0 )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
// Output : static void
//-----------------------------------------------------------------------------
static void PopulateWaveList( HWND wnd, CSoundParametersInternal *p )
{
	HWND control = GetDlgItem( wnd, IDC_WAVELIST );
	if ( !control )
		return;

	// Remove all
	SendMessage( control, LB_RESETCONTENT, 0, 0 );

	if ( !p )
		return;

	CUtlRBTree< char const *, int >		m_SortedNames( 0, 0, WaveLessFunc );

	int c = p->NumSoundNames();
	for ( int i = 0; i < c; i++ )
	{
		char const *name = strdup( g_pSoundEmitterSystem->GetWaveName( p->GetSoundNames()[ i ].symbol ) );

		m_SortedNames.Insert( name );
	}

	int j = m_SortedNames.FirstInorder();
	while ( j != m_SortedNames.InvalidIndex() )
	{
		char const *name = m_SortedNames[ j ];
		if ( name && name[ 0 ] )
		{
			SendMessage( control, LB_ADDSTRING, 0, (LPARAM)name ); 
		}

		delete name;

		j = m_SortedNames.NextInorder( j );
	}
}

static void PopulateWaveList_Available( HWND wnd )
{
	HWND control = GetDlgItem( wnd, IDC_WAVELIST_AVAILABLE );
	if ( !control )
		return;

	CWaveBrowser *wb = GetWorkspaceManager()->GetWaveBrowser();
	if ( !wb )
	{
		Assert( 0 );
		return;
	}

	CUtlRBTree< char const *, int >		m_SortedNames( 0, 0, WaveLessFunc );

	int c = wb->GetSoundCount();;
	for ( int i = 0; i < c; i++ )
	{
		CWaveFile *entry = wb->GetSound( i );
		if ( !entry )
			continue;

		char const *name = entry->GetName();
		m_SortedNames.Insert( name );
	}

	// Remove all
	SendMessage( control, LB_RESETCONTENT, 0, 0 );

	int j = m_SortedNames.FirstInorder();
	while ( j != m_SortedNames.InvalidIndex() )
	{
		char const *name = m_SortedNames[ j ];
		if ( name && name[ 0 ] )
		{
			SendMessage( control, LB_ADDSTRING, 0, (LPARAM)name ); 
		}

		j = m_SortedNames.NextInorder( j );
	}
}

static void SoundProperties_GetSelectedWaveList( HWND hwndDlg, HWND control, CUtlVector< int >& list )
{
	if ( !control )
		return;

	int count = SendMessage( control, LB_GETSELCOUNT, 0, 0 );
	if ( count == LB_ERR )
		return;

	int *s = new int[ count ];

	SendMessage( control, LB_GETSELITEMS, count, (LPARAM)s );

	for ( int i = 0 ;i < count; i++ )
	{
		list.AddToTail( s[ i ] );
	}

	delete[] s;
}

static const char *SoundProperties_GetSelectedWave( HWND hwndDlg, HWND control, int *item = NULL )
{
	if ( item )
		*item = -1;

	if ( !control )
		return NULL;

	int selectedionindex = SendMessage( control, LB_GETCURSEL, 0, 0 );
	if ( selectedionindex == LB_ERR )
		return NULL;

	static char itemtext[ 256 ];
	SendMessage( control, LB_GETTEXT, selectedionindex, (LPARAM)itemtext );

	if ( item )
	{
		*item = selectedionindex;
	}

	return itemtext;
}

static void SoundProperties_OnOK( HWND hwndDlg )
{
	CSoundParametersInternal outparams;

	// Gather info and make changes
	CSoundEntry *item = g_Params.items[ 0 ];

	int soundindex = -1;
	char const *script = NULL;

	char outsoundname[ 256 ];
	char outscriptfile[ 256 ];
						
	GetDlgItemText( hwndDlg, IDC_SOUNDNAME, outsoundname, sizeof( outsoundname ) );
	GetDlgItemText( hwndDlg, IDC_SOUNDSCRIPT, outscriptfile, sizeof( outscriptfile ) );

	if ( !g_Params.addsound )
	{
		soundindex = g_pSoundEmitterSystem->GetSoundIndex( item->GetName() );
		script = g_pSoundEmitterSystem->GetSourceFileForSound( soundindex );
	}

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

	// Retrieve wave list
	int c = SendMessage( GetDlgItem( hwndDlg, IDC_WAVELIST ), LB_GETCOUNT, 0, 0 );

	for ( int i = 0; i < c; i++ )
	{
		char wavename[ 256 ];
		SendMessage( GetDlgItem( hwndDlg, IDC_WAVELIST ), LB_GETTEXT, i, (LPARAM)wavename );

		CUtlSymbol sym = g_pSoundEmitterSystem->AddWaveName( wavename );
		SoundFile sf;
		sf.symbol = sym;
		sf.gender = GENDER_NONE;
		outparams.AddSoundName( sf );
	}

	if ( g_Params.addsound )
	{
		g_pSoundEmitterSystem->AddSound( outsoundname, outscriptfile, outparams );
	}
	else
	{
		// Update sound stuff
		g_pSoundEmitterSystem->MoveSound( item->GetName(), outscriptfile );
		item->SetScriptFile( outscriptfile );
		g_pSoundEmitterSystem->UpdateSoundParameters( item->GetName(), outparams );
		// Rename if necessary
		g_pSoundEmitterSystem->RenameSound( item->GetName(), outsoundname );
	}

	// Apply changes
	item->SetName( outsoundname );

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
static BOOL CALLBACK SoundPropertiesDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch(uMsg)
	{
    case WM_INITDIALOG:
		{
			g_Params.PositionSelf( hwndDlg );

			CSoundEntry *item = NULL;

			CSoundParametersInternal *p = NULL;
			char const *script = NULL;
				
			if ( g_Params.addsound )
			{
				Assert( g_Params.items.Count() == 0 );

				CSoundEntry *entry = new CSoundEntry( NULL, "Unnamed" );
				g_Params.items.AddToTail( entry );

				item = item = g_Params.items[ 0 ];

				SendMessage( GetDlgItem( hwndDlg, IDC_OWNERONLY ), BM_SETCHECK, 
					( WPARAM )BST_UNCHECKED,
					( LPARAM )0 );

				SetDlgItemText( hwndDlg, IDC_STATIC_SENTENCETEXT, "" );
				SetDlgItemTextW( hwndDlg, IDC_STATIC_CLOSECAPTION, L"" );
			}
			else
			{
				item = g_Params.items[ 0 ];

				int soundindex = g_pSoundEmitterSystem->GetSoundIndex( item->GetName() );

				script = g_pSoundEmitterSystem->GetSourceFileForSound( soundindex );

				p = g_pSoundEmitterSystem->InternalGetParametersForSound( soundindex );
				Assert( p );

				SendMessage( GetDlgItem( hwndDlg, IDC_OWNERONLY ), BM_SETCHECK, 
					( WPARAM ) p->OnlyPlayToOwner() ? BST_CHECKED : BST_UNCHECKED,
					( LPARAM )0 );

				SetDlgItemText( hwndDlg, IDC_STATIC_SENTENCETEXT, item->GetSentenceText( 0 ) );
				wchar_t cctext[ 1024 ];
				item->GetCCText( cctext, sizeof( cctext ) / sizeof( wchar_t ) );

				SetDlgItemTextW( hwndDlg, IDC_STATIC_CLOSECAPTION, cctext );
			}
			
			PopulateChannelList( hwndDlg, p );
			PopulateVolumeList( hwndDlg, p );
			PopulateSoundlevelList( hwndDlg, p );
			PopulatePitchList( hwndDlg, p );

			PopulateWaveList( hwndDlg, p );

			SetDlgItemText( hwndDlg, IDC_SOUNDNAME, item->GetName() );

			PopulateScriptList( hwndDlg, script );

			PopulateWaveList_Available( hwndDlg );

			SetWindowText( hwndDlg, g_Params.m_szDialogTitle );

			SetFocus( GetDlgItem( hwndDlg, IDC_SOUNDNAME ) );
		}
		return FALSE;  
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				SoundProperties_OnOK( hwndDlg );

				EndDialog( hwndDlg, 1 );
			}
			break;
        case IDCANCEL:
			EndDialog( hwndDlg, 0 );
			break;
		case IDC_WAVELIST:
			{
				if ( HIWORD(wParam) == LBN_SELCHANGE )
				{
					// Find selected item in wave list...
					char const *wave = SoundProperties_GetSelectedWave( hwndDlg, GetDlgItem( hwndDlg, IDC_WAVELIST ) );
					if ( wave )
					{
						// Look it up
						CWaveBrowser *wb = GetWorkspaceManager()->GetWaveBrowser();
						if ( wb )
						{
							CWaveFile *wavefile = wb->FindEntry( wave, true );
							if ( wavefile )
							{
								SetDlgItemText( hwndDlg, IDC_STATIC_SENTENCETEXT, wavefile->GetSentenceText() );
							}
						}
					}
				}
			}
			break;
		case IDC_WAVEPROPERTIES:
			{
				// Find selected item in wave list...
				char const *wave = SoundProperties_GetSelectedWave( hwndDlg, GetDlgItem( hwndDlg, IDC_WAVELIST ) );
				if ( wave )
				{
					// Look it up
					CWaveBrowser *wb = GetWorkspaceManager()->GetWaveBrowser();
					if ( wb )
					{
						CWaveFile *wavefile = wb->FindEntry( wave, true );
						if ( wavefile )
						{
							CWaveParams wp;
							memset( &wp, 0, sizeof( wp ) );
							
							Q_snprintf( wp.m_szDialogTitle, sizeof( wp.m_szDialogTitle ), "Wave Properties" );
							
							wp.items.AddToTail( wavefile );
							
							WaveProperties( &wp );
						}
					}
				}
			}
			break;
		case IDC_REMOVEWAVE:
			{
				CUtlVector<int> selected;
				SoundProperties_GetSelectedWaveList( hwndDlg, GetDlgItem( hwndDlg, IDC_WAVELIST ), selected );
				int count = selected.Count();
				if ( count >= 1 )
				{
					int i;
					for ( i = 0; i < count; i++ )
					{
						char wavename[ 256 ];
						
						SendMessage( GetDlgItem( hwndDlg, IDC_WAVELIST), LB_GETTEXT, selected[ i ], (LPARAM)wavename );

						// Add it to global parameters, too
						CWaveBrowser *wb = GetWorkspaceManager()->GetWaveBrowser();
						if ( wb )
						{
							CWaveFile *wavefile = wb->FindEntry( wavename, true );
							if ( wavefile )
							{
								CSoundEntry *item = g_Params.items[ 0 ];
								item->RemoveWave( wavefile );
							}
						}
					}

					for ( i = count - 1; i >= 0; i-- )
					{
						// Remove them from list
						SendMessage( GetDlgItem( hwndDlg, IDC_WAVELIST), LB_DELETESTRING, selected[ i ], 0 );
					}
				}
			}
			break;
		case IDC_ADDWAVE:
			{
				CUtlVector<int> selected;
				SoundProperties_GetSelectedWaveList( hwndDlg, GetDlgItem( hwndDlg, IDC_WAVELIST_AVAILABLE ), selected );
				int count = selected.Count();
				if ( count >= 1 )
				{
					int i;
					for ( i = 0; i < count; i++ )
					{
						char wavename[ 256 ];
						
						// Get selection wave name
						SendMessage( GetDlgItem( hwndDlg, IDC_WAVELIST_AVAILABLE), LB_GETTEXT, selected[ i ], (LPARAM)wavename );

						// Add it to global parameters, too
						CWaveBrowser *wb = GetWorkspaceManager()->GetWaveBrowser();
						if ( wb )
						{
							CWaveFile *wavefile = wb->FindEntry( wavename, true );
							if ( wavefile )
							{
								CSoundEntry *item = g_Params.items[ 0 ];
								if ( item->FindWave( wavefile ) == -1 )
								{
									item->AddWave( wavefile );
									// Add it to list
									SendMessage( GetDlgItem( hwndDlg, IDC_WAVELIST ), LB_ADDSTRING, 0, (LPARAM)wavename );
								}
							}
						}
					}
				}
			}
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
int SoundProperties( CSoundParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_SOUNDPROPERTIES ),
		(HWND)GetWorkspaceManager()->getHandle(),
		(DLGPROC)SoundPropertiesDialogProc );

	*params = g_Params;

	return retval;
}