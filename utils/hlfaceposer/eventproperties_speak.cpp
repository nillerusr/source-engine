//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <mxtk/mx.h>
#include <stdio.h>
#include "resource.h"
#include "EventProperties.h"
#include "mdlviewer.h"
#include "choreoevent.h"
#include "filesystem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "AddSoundEntry.h"
#include "SoundLookup.h"
#include "ifaceposersound.h"
#include "MatSysWin.h"

static CEventParams g_Params;

class CEventPropertiesSpeakDialog : public CBaseEventPropertiesDialog
{
	typedef CBaseEventPropertiesDialog BaseClass;

public:

	CEventPropertiesSpeakDialog()
	{
		m_bShowAll = false;
		m_szLastFilter[ 0 ] = 0;
		m_Timer = 0;
		m_flLastFilterUpdateTime;
	}

	virtual void		InitDialog( HWND hwndDlg );
	virtual BOOL		HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
	virtual void		SetTitle();
	virtual void		ShowControlsForEventType( CEventParams *params );
	virtual void		InitControlData( CEventParams *params );

private:

	void		PopulateFilterList( bool resetCurrent );
	void		OnCheckFilterUpdate();

	void		AddFilterToHistory( char const *filter );

	void		OnSoundSelected( CEventParams *params );

	void		PopulateSoundList( char const *current, HWND wnd );
	void		PopulateVolumeLevels( HWND control, CEventParams *params );

	void		FindWaveInSoundEntries( CUtlVector< int >& entryList, char const *search );
	void		OnCheckChangedVolumeLevel( CEventParams *params );

	bool			m_bShowAll;

	CUtlVector< CUtlSymbol > m_FilterHistory;
	CUtlSymbolTable			m_Symbols;

	enum
	{
		TIMER_ID	= 100,
	};

	UINT			m_Timer;
	char			m_szLastFilter[ 256 ];
	float			m_flLastFilterUpdateTime;
};

void CEventPropertiesSpeakDialog::AddFilterToHistory( char const *filter )
{
	CUtlSymbol sym = m_Symbols.AddString( filter );
	// Move it to front of list...
	m_FilterHistory.FindAndRemove( sym );
	m_FilterHistory.AddToHead( sym );

	PopulateFilterList( false );

	// Apply filter

	PopulateSoundList( g_Params.m_szParameters, GetControl( IDC_SOUNDLIST ) );
}

void CEventPropertiesSpeakDialog::PopulateFilterList(  bool resetCurrent )
{
	HWND control = GetControl( IDC_FILTER );

	char oldf[ 256 ];
	SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( oldf ), (LPARAM)oldf );

	SendMessage( control, CB_RESETCONTENT, 0, 0 );

	int c = m_FilterHistory.Count();
	if ( c == 0 )
		return;

	for ( int i = 0; i < c; ++i )
	{
		char const *str = m_Symbols.String( m_FilterHistory[ i ] );
		SendMessage( control, CB_ADDSTRING, 0, (LPARAM)str );
	}

	char const *first = m_Symbols.String( m_FilterHistory[ 0 ] );
	if ( resetCurrent && first )
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)first );
		SendMessage( control, CB_SETEDITSEL , 0, MAKELPARAM( Q_strlen(first), -1 ) );
	}
	else
	{
		SendMessage( control, WM_SETTEXT , 0, (LPARAM)oldf );
		SendMessage( control, CB_SETEDITSEL , 0, MAKELPARAM( Q_strlen(oldf), -1 ) );
	}
}

void CEventPropertiesSpeakDialog::OnCheckFilterUpdate()
{
	char curfilter[ 256 ];
	HWND control = GetControl( IDC_FILTER );
	SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( curfilter), (LPARAM)curfilter );

	if ( Q_stricmp( curfilter, m_szLastFilter ) )
	{
		Q_strncpy( m_szLastFilter, curfilter, sizeof( m_szLastFilter ) );

		AddFilterToHistory( m_szLastFilter );
	}
}

void CEventPropertiesSpeakDialog::SetTitle()
{
	SetDialogTitle( &g_Params, "Speak", "Speak Sound" );
}

void CEventPropertiesSpeakDialog::InitControlData( CEventParams *params )
{
	BaseClass::InitControlData( params );

	m_flLastFilterUpdateTime = (float)mx::getTickCount() / 1000.0f;

	m_Timer = SetTimer( m_hDialog, TIMER_ID, 1, 0 );

	HWND choices1 = GetControl( IDC_SOUNDLIST );
	SendMessage( choices1, LB_RESETCONTENT, 0, 0 ); 

	HWND choices2 = GetControl( IDC_EVENTCHOICES2 );
	SendMessage( choices2, CB_RESETCONTENT, 0, 0 ); 
	SendMessage( choices2, WM_SETTEXT , 0, (LPARAM)params->m_szParameters2 );

	HWND attenuate = GetControl( IDC_CAPTION_ATTENUATION );
	SendMessage( attenuate, BM_SETCHECK, (WPARAM) params->m_bCloseCaptionNoAttenuate ? BST_CHECKED : BST_UNCHECKED, 0 );

	PopulateSoundList( params->m_szParameters, choices1 );

	OnSoundSelected( params );

	PopulateFilterList( true );
}

void CEventPropertiesSpeakDialog::InitDialog( HWND hwndDlg )
{
	m_hDialog = hwndDlg;

	g_Params.PositionSelf( m_hDialog );
	
	// Set working title for dialog, etc.
	SetTitle();

	// Show/Hide dialog controls
	ShowControlsForEventType( &g_Params );
	InitControlData( &g_Params );

	UpdateTagRadioButtons( &g_Params );

	SetFocus( GetControl( IDC_EVENTNAME ) );
}

static CEventPropertiesSpeakDialog g_EventPropertiesSpeakDialog;

void CEventPropertiesSpeakDialog::PopulateVolumeLevels( HWND control, CEventParams *params )
{
	SendMessage( control, CB_RESETCONTENT, 0, 0 ); 

	// Assume uneditable
	SendMessage( control, CB_ADDSTRING, 0, (LPARAM)"VOL_NORM" );

	SendMessage( control, WM_SETTEXT , 0, (LPARAM)"VOL_NORM" );

	bool enabled = false;

	if ( !Q_stristr( params->m_szParameters, ".wav" ) )
	{
		// Look up the sound level from the soundemitter system
		int soundindex = soundemitter->GetSoundIndex( params->m_szParameters );
		if ( soundindex >= 0 )
		{
			// Look up the sound level from the soundemitter system
			CSoundParametersInternal *params = soundemitter->InternalGetParametersForSound( soundindex );
			if ( params )
			{
				// Found it
				SendMessage( control, WM_SETTEXT , 0, (LPARAM)params->VolumeToString() );

				// 
				// See if the .txt file is writable
				char const *scriptfile = soundemitter->GetSourceFileForSound( soundindex );
				if ( scriptfile )
				{
					// See if it's writable
					if ( filesystem->FileExists( scriptfile ) &&
						 filesystem->IsFileWritable( scriptfile ) )
					{
						enabled = true;
					}
				}
			}
		}
	}

	EnableWindow( control, enabled ? TRUE : FALSE );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
// Output : static void
//-----------------------------------------------------------------------------
void CEventPropertiesSpeakDialog::PopulateSoundList( char const *current, HWND wnd  )
{
	extern bool NameLessFunc( const char *const& name1, const char *const& name2 );

	CUtlRBTree< char const *, int >		m_SortedNames( 0, 0, NameLessFunc );

	int c = soundemitter->GetSoundCount();
	for ( int i = 0; i < c; i++ )
	{
		char const *name = soundemitter->GetSoundName( i );

		if ( name && name[ 0 ] )
		{
			bool add = true;
			if ( !m_bShowAll )
			{
				CSoundParameters params;
				if ( soundemitter->GetParametersForSound( name, params, GENDER_NONE ) )
				{
					if ( params.channel != CHAN_VOICE )
					{
						add = false;
					}
				}
			}

			// Apply filter
			if ( m_szLastFilter[ 0 ] != 0 )
			{
				if ( !Q_stristr( name, m_szLastFilter ) )
				{
					add = false;
				}
			}

			if ( add )
			{
				m_SortedNames.Insert( name );
			}
		}
	}

	SendMessage( wnd, WM_SETREDRAW , (WPARAM)FALSE, (LPARAM)0 );

	// Remove all
	SendMessage( wnd, LB_RESETCONTENT, 0, 0 );

	int selectslot = 0;

	int j = m_SortedNames.FirstInorder();
	while ( j != m_SortedNames.InvalidIndex() )
	{
		char const *name = m_SortedNames[ j ];
		if ( name && name[ 0 ] )
		{
			int temp = SendMessage( wnd, LB_ADDSTRING, 0, (LPARAM)name ); 

			if ( !Q_stricmp( name, current ) )
			{
				selectslot = temp;
			}
		}

		j = m_SortedNames.NextInorder( j );
	}

	SendMessage( wnd, LB_SETCURSEL, (WPARAM)selectslot, 0 );

	SendMessage( wnd, WM_SETREDRAW , (WPARAM)TRUE, (LPARAM)0 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : wnd - 
//			*params - 
// Output : static
//-----------------------------------------------------------------------------

void CEventPropertiesSpeakDialog::ShowControlsForEventType( CEventParams *params )
{
	BaseClass::ShowControlsForEventType( params );
}

void CEventPropertiesSpeakDialog::FindWaveInSoundEntries( CUtlVector< int >& entryList, char const *search )
{
	int c = soundemitter->GetSoundCount();
	for ( int i = 0; i < c; i++ )
	{
		CSoundParametersInternal *params = soundemitter->InternalGetParametersForSound( i );
		if ( !params )
			continue;

		int waveCount = params->NumSoundNames();
		for ( int wave = 0; wave < waveCount; wave++ )
		{
			char const *waveName = soundemitter->GetWaveName( params->GetSoundNames()[ wave ].symbol );

			if ( !Q_stricmp( waveName, search ) )
			{
				entryList.AddToTail( i );
				break;
			}
		}
	}
}

void CEventPropertiesSpeakDialog::OnCheckChangedVolumeLevel( CEventParams *params )
{
	HWND control = GetControl( IDC_EVENTCHOICES2 );
	if ( !IsWindowEnabled( control ) )
	{
		return;
	}

	if ( Q_stristr( params->m_szParameters, ".wav" ) )
	{
		return;
	}

	int soundindex = soundemitter->GetSoundIndex( params->m_szParameters );
	if ( soundindex < 0 )
		return;

	// Look up the sound level from the soundemitter system
	CSoundParametersInternal *soundparams = soundemitter->InternalGetParametersForSound( soundindex );
	if ( !params )
	{
		return;
	}

	// See if it's writable, if not then bail
	char const *scriptfile = soundemitter->GetSourceFileForSound( soundindex );
	if ( !scriptfile || 
		 !filesystem->FileExists( scriptfile ) ||
		 !filesystem->IsFileWritable( scriptfile ) )
	{
		return;
	}

	// Copy the parameters
	CSoundParametersInternal newparams;
	newparams.CopyFrom( *soundparams );

	// Get the value from the control
	char newvolumelevel[ 256 ];
	SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( newvolumelevel ), (LPARAM)newvolumelevel );

	newparams.VolumeFromString( newvolumelevel );

	// No change
	if ( newparams == *soundparams )
	{
		return;
	}

	soundemitter->UpdateSoundParameters( params->m_szParameters , newparams );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hwndDlg - 
//			uMsg - 
//			wParam - 
//			lParam - 
// Output : static BOOL CALLBACK
//-----------------------------------------------------------------------------
static BOOL CALLBACK EventPropertiesSpeakDialog( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_EventPropertiesSpeakDialog.HandleMessage( hwndDlg, uMsg, wParam, lParam );
};

void CEventPropertiesSpeakDialog::OnSoundSelected( CEventParams *params )
{
	PopulateVolumeLevels( GetControl( IDC_EVENTCHOICES2 ), params );
	SendMessage( GetControl( IDC_SOUNDNAME ), WM_SETTEXT, 0, (LPARAM)params->m_szParameters );

	// Update script name and wavename fields
	HWND scriptname = GetControl( IDC_STATIC_SCRIPTFILE );
	HWND wavename = GetControl( IDC_STATIC_WAVEFILENAME );

	SendMessage( scriptname, WM_SETTEXT, (WPARAM)1, (LPARAM)"" );
	SendMessage( wavename, WM_SETTEXT, (WPARAM)1, (LPARAM)"" );

	int soundindex = soundemitter->GetSoundIndex( params->m_szParameters );
	if ( soundindex >= 0 )
	{
		char const *script = soundemitter->GetSourceFileForSound( soundindex );
		if ( script && script [ 0 ] )
		{
			SendMessage( scriptname, WM_SETTEXT, (WPARAM)Q_strlen( script ) + 1, (LPARAM)script );
		
			// Look up the sound level from the soundemitter system
			CSoundParametersInternal *params = soundemitter->InternalGetParametersForSound( soundindex );
			if ( params )
			{
				// Get wave name
				char const *w = soundemitter->GetWaveName( params->GetSoundNames()[ 0 ].symbol );
				if ( w && w[ 0 ] )
				{
					SendMessage( wavename, WM_SETTEXT, (WPARAM)Q_strlen( w ) + 1, (LPARAM)w );
				}
			}
		}
	}
}

BOOL CEventPropertiesSpeakDialog::HandleMessage( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	m_hDialog = hwndDlg;

	bool handled = false;
	BOOL bret = InternalHandleMessage( &g_Params, hwndDlg, uMsg, wParam, lParam, handled );
	if ( handled )
		return bret;

	switch(uMsg)
	{
	case WM_PAINT:
		{
			PAINTSTRUCT ps; 
			HDC hdc;
			
            hdc = BeginPaint(hwndDlg, &ps); 
			DrawSpline( hdc, GetControl( IDC_STATIC_SPLINE ), g_Params.m_pEvent );
            EndPaint(hwndDlg, &ps); 

            return FALSE; 
		}
		break;
	case WM_VSCROLL:
		{
			RECT rcOut;
			GetSplineRect( GetControl( IDC_STATIC_SPLINE ), rcOut );

			InvalidateRect( hwndDlg, &rcOut, TRUE );
			UpdateWindow( hwndDlg );
			return FALSE;
		}
		break;
    case WM_INITDIALOG:
		{
			InitDialog( hwndDlg );
		}
		return FALSE;  
	case WM_TIMER:
		{
			g_pMatSysWindow->Frame();

			float curtime = (float)mx::getTickCount() / 1000.0f;
			if ( curtime - m_flLastFilterUpdateTime > 0.5f )
			{
				m_flLastFilterUpdateTime = curtime;
				OnCheckFilterUpdate();
			}
		}
		return FALSE;
		
    case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				char sz[ 512 ];
				GetDlgItemText( m_hDialog, IDC_SOUNDNAME, sz, sizeof( sz ) );

				Q_FixSlashes( sz );

				// Strip off game directory stuff
				Q_strncpy( g_Params.m_szParameters, sz, sizeof( g_Params.m_szParameters ) );
				char *p = Q_strstr( sz, "\\sound\\" );
				if ( p )
				{
					Q_strncpy( g_Params.m_szParameters, p + strlen( "\\sound\\" ), sizeof( g_Params.m_szParameters ) );
				}

				OnCheckChangedVolumeLevel( &g_Params );

				GetDlgItemText( m_hDialog, IDC_EVENTNAME, g_Params.m_szName, sizeof( g_Params.m_szName ) );

				if ( !g_Params.m_szName[ 0 ] )
				{
					Q_strncpy( g_Params.m_szName, sz, sizeof( g_Params.m_szName ) );
				}

				char szTime[ 32 ];
				GetDlgItemText( m_hDialog, IDC_STARTTIME, szTime, sizeof( szTime ) );
				g_Params.m_flStartTime = atof( szTime );
				GetDlgItemText( m_hDialog, IDC_ENDTIME, szTime, sizeof( szTime ) );
				g_Params.m_flEndTime = atof( szTime );

				// Parse tokens from tags
				ParseTags( &g_Params );

				KillTimer( m_hDialog, m_Timer );

				EndDialog( hwndDlg, 1 );
			}
			break;
        case IDCANCEL:
			{
				KillTimer( m_hDialog, m_Timer );
				EndDialog( hwndDlg, 0 );
			}
			break;
		case IDC_CAPTION_ATTENUATION:
			{
				HWND control = GetControl( IDC_CAPTION_ATTENUATION );
				g_Params.m_bCloseCaptionNoAttenuate = SendMessage( control, BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
			}
			break;
		case IDC_PLAY_SOUND:
			{
				// Get sound name from soundemitter
				sound->PlaySound( 
					NULL, 
					1.0f,
					va( "sound/%s", FacePoser_TranslateSoundName( g_Params.m_szParameters ) ),
					NULL );
			}
			break;
		case IDC_OPENSOURCE:
			{
				// Look up the sound level from the soundemitter system
				int soundindex = soundemitter->GetSoundIndex( g_Params.m_szParameters );
				if ( soundindex >= 0 )
				{
					// Look up the sound level from the soundemitter system
					CSoundParametersInternal *params = soundemitter->InternalGetParametersForSound( soundindex );
					if ( params )
					{
						// See if the .txt file is writable
						char const *scriptfile = soundemitter->GetSourceFileForSound( soundindex );
						if ( scriptfile )
						{
							char relative_path[MAX_PATH];
							Q_snprintf( relative_path, MAX_PATH, "%s", scriptfile );

							char full_path[MAX_PATH];
							if ( filesystem->GetLocalPath( relative_path, full_path, MAX_PATH ) )
							{
								ShellExecute( NULL, "open", full_path, NULL, NULL, SW_SHOWNORMAL );
							}
						}
					}
				}
			}
			break;
		case IDC_SHOW_ALL_SOUNDS:
			{
				m_bShowAll = SendMessage( GetControl( IDC_SHOW_ALL_SOUNDS ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;

				PopulateSoundList( g_Params.m_szParameters, GetControl( IDC_EVENTCHOICES ) );
			}
			break;
		case IDC_CHECK_ENDTIME:
			{
				g_Params.m_bHasEndTime = SendMessage( GetControl( IDC_CHECK_ENDTIME ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
				if ( !g_Params.m_bHasEndTime )
				{
					ShowWindow( GetControl( IDC_ENDTIME ), SW_HIDE );
				}
				else
				{
					ShowWindow( GetControl( IDC_ENDTIME ), SW_RESTORE );
				}
			}
			break;
		case IDC_CHECK_RESUMECONDITION:
			{
				g_Params.m_bResumeCondition = SendMessage( GetControl( IDC_CHECK_RESUMECONDITION ), BM_GETCHECK, 0, 0 ) == BST_CHECKED ? true : false;
			}
			break;
		case IDC_SOUNDLIST:
			{
				HWND control = (HWND)lParam;
				if ( control )
				{
					int cursel = SendMessage( control, LB_GETCURSEL, 0, 0 );
					if ( cursel != LB_ERR )
					{
						SendMessage( control, LB_GETTEXT, cursel, (LPARAM)g_Params.m_szParameters );
						OnSoundSelected( &g_Params );

						if ( HIWORD( wParam ) == LBN_DBLCLK )
						{
							// Get sound name from soundemitter
							sound->PlaySound( 
								NULL, 
								1.0f,
								va( "sound/%s", FacePoser_TranslateSoundName( g_Params.m_szParameters ) ),
								NULL );
						}
					}
				}
			}
			break;
		case IDC_EVENTCHOICES2:
			{
				HWND control = (HWND)lParam;
				if ( control )
				{
					SendMessage( control, WM_GETTEXT, (WPARAM)sizeof( g_Params.m_szParameters2 ), (LPARAM)g_Params.m_szParameters2 );
				}
			}
			break;
		case IDC_ABSOLUTESTART:
			{
				g_Params.m_bUsesTag = false;
				UpdateTagRadioButtons( &g_Params );
			}
			break;
		case IDC_RELATIVESTART:
			{
				g_Params.m_bUsesTag = true;
				UpdateTagRadioButtons( &g_Params );
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
int EventProperties_Speak( CEventParams *params )
{
	g_Params = *params;

	int retval = DialogBox( (HINSTANCE)GetModuleHandle( 0 ), 
		MAKEINTRESOURCE( IDD_EVENTPROPERTIES_SPEAK ),
		(HWND)g_MDLViewer->getHandle(),
		(DLGPROC)EventPropertiesSpeakDialog );

	*params = g_Params;

	return retval;
}