//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include <windows.h>
#undef PropertySheet

#include "filesystem.h"
#include "dme_controls/soundpicker.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"
#include "dme_controls/filtercombobox.h"
#include "vgui/ISurface.h"
#include "vgui/iinput.h"
#include "dme_controls/dmecontrols.h"
#include "soundemittersystem/isoundemittersystembase.h"
#include "mathlib/mathlib.h"

// FIXME: Move sound code out of the engine + into a library!
#include "toolframework/ienginetool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// Sound Picker
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Sort by sound name
//-----------------------------------------------------------------------------
static int __cdecl GameSoundSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	bool bRoot1 = item1.kv->GetInt("root") != 0;
	bool bRoot2 = item2.kv->GetInt("root") != 0;
	if ( bRoot1 != bRoot2 )
		return bRoot1 ? -1 : 1;
	const char *string1 = item1.kv->GetString("gamesound");
	const char *string2 = item2.kv->GetString("gamesound");
	return Q_stricmp( string1, string2 );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSoundPicker::CSoundPicker( vgui::Panel *pParent, int nFlags ) : 
	BaseClass( pParent, "Sound Files", "wav", "sound", "wavName" )
{
	m_nSoundSuppressionCount = 0;
	m_nPlayingSound = 0;

	// Connection problem if this failed
	Assert( SoundEmitterSystem() );

	m_pViewsSheet = new vgui::PropertySheet( this, "ViewsSheet" );
 	m_pViewsSheet->AddActionSignalTarget( this );

	// game sounds
	m_pGameSoundPage = NULL;
	m_pGameSoundList = NULL;
	if ( nFlags & PICK_GAMESOUNDS )
	{
		m_pGameSoundPage = new PropertyPage( m_pViewsSheet, "GameSoundPage" );
		m_pGameSoundList = new ListPanel( m_pGameSoundPage, "GameSoundsList" );
 		m_pGameSoundList->AddColumnHeader( 0, "GameSound", "Game Sound", 52, 0 );
		m_pGameSoundList->AddActionSignalTarget( this );
		m_pGameSoundList->SetSelectIndividualCells( true );
 		m_pGameSoundList->SetEmptyListText("No game sounds");
		m_pGameSoundList->SetDragEnabled( true );
		m_pGameSoundList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
		m_pGameSoundList->SetSortFunc( 0, GameSoundSortFunc );
		m_pGameSoundList->SetSortColumn( 0 );
		m_pGameSoundList->SetMultiselectEnabled( ( nFlags & ALLOW_MULTISELECT ) != 0 );

		// filter selection
		m_pGameSoundFilter = new TextEntry( m_pGameSoundPage, "GameSoundFilter" );
		m_pGameSoundFilter->AddActionSignalTarget( this );

        m_pGameSoundPage->LoadControlSettings( "resource/soundpickergamesoundpage.res" );

		m_pViewsSheet->AddPage( m_pGameSoundPage, "Game Sounds" );
	}

	// wav files
	m_pWavPage = NULL;
	if ( nFlags & PICK_WAVFILES )
	{
		m_pWavPage = new PropertyPage( m_pViewsSheet, "WavPage" );
		bool bAllowMultiselect = ( nFlags & ALLOW_MULTISELECT ) != 0;
		CreateStandardControls( m_pWavPage, bAllowMultiselect );
		AddExtension( "mp3" );

		m_pWavPage->LoadControlSettings( "resource/soundpickerwavpage.res" );
 		m_pViewsSheet->AddPage( m_pWavPage, "WAVs" );
	}

	LoadControlSettings( "resource/soundpicker.res" );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSoundPicker::~CSoundPicker()
{
	StopSoundPreview();
}


//-----------------------------------------------------------------------------
// Purpose: called to open
//-----------------------------------------------------------------------------
void CSoundPicker::Activate()
{
	BaseClass::Activate();
	if ( m_pGameSoundPage )
	{
		BuildGameSoundList();
	}
}


//-----------------------------------------------------------------------------
// Sets the current sound choice
//-----------------------------------------------------------------------------
void CSoundPicker::SetSelectedSound( PickType_t type, const char *pSoundName )
{
	if ( type == PICK_NONE || !pSoundName )
		return;
	
	if ( m_pGameSoundPage && ( type == PICK_GAMESOUNDS ) )
	{
		m_pViewsSheet->SetActivePage( m_pGameSoundPage );
		m_pGameSoundFilter->SetText( pSoundName );
	}

	if ( m_pWavPage && ( type == PICK_WAVFILES ) )
	{
		m_pViewsSheet->SetActivePage( m_pWavPage );
		SetInitialSelection( pSoundName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSoundPicker::OnKeyCodePressed( KeyCode code )
{
	if ( m_pGameSoundPage && ( m_pViewsSheet->GetActivePage() == m_pGameSoundPage ) )
	{
		if (( code == KEY_UP ) || ( code == KEY_DOWN ) || ( code == KEY_PAGEUP ) || ( code == KEY_PAGEDOWN ))
		{
			KeyValues *pMsg = new KeyValues( "KeyCodePressed", "code", code );
			vgui::ipanel()->SendMessage( m_pGameSoundList->GetVPanel(), pMsg, GetVPanel() );
			pMsg->deleteThis();
			return;
		}
	}

	BaseClass::OnKeyCodePressed( code );
}


//-----------------------------------------------------------------------------
// Purpose: builds the gamesound list
//-----------------------------------------------------------------------------
bool CSoundPicker::IsGameSoundVisible( int hGameSound )
{
	const char *pSoundName = SoundEmitterSystem()->GetSoundName( hGameSound );
	return ( !m_GameSoundFilter.Length() || Q_stristr( pSoundName, m_GameSoundFilter.Get() ) );
}


//-----------------------------------------------------------------------------
// Updates the column header in the chooser
//-----------------------------------------------------------------------------
void CSoundPicker::UpdateGameSoundColumnHeader( int nMatchCount, int nTotalCount )
{
	char pColumnTitle[512];
	Q_snprintf( pColumnTitle, sizeof(pColumnTitle), "%s (%d/%d)",
		"Game Sound", nMatchCount, nTotalCount );
	m_pGameSoundList->SetColumnHeaderText( 0, pColumnTitle );
}

	
//-----------------------------------------------------------------------------
// Purpose: builds the gamesound list
//-----------------------------------------------------------------------------
void CSoundPicker::BuildGameSoundList()
{
	if ( !m_pGameSoundList )
		return;

	m_pGameSoundList->RemoveAll();

	int nTotalCount = 0;
	int i = SoundEmitterSystem()->First();
	while ( i != SoundEmitterSystem()->InvalidIndex() )
	{
		const char *pSoundName = SoundEmitterSystem()->GetSoundName( i );

		bool bInRoot = !strchr( pSoundName, '\\' ) && !strchr( pSoundName, '/' );
		KeyValues *kv = new KeyValues( "node", "gamesound", pSoundName );
		kv->SetInt( "gameSoundHandle", i );
		kv->SetInt( "root", bInRoot );

		int nItemID = m_pGameSoundList->AddItem( kv, 0, false, false );
		m_pGameSoundList->SetItemVisible( nItemID, IsGameSoundVisible( i ) );
		KeyValues *pDrag = new KeyValues( "drag", "text", pSoundName );
		pDrag->SetString( "texttype", "gamesoundName" );
		m_pGameSoundList->SetItemDragData( nItemID, pDrag );
	    ++nTotalCount;

		i = SoundEmitterSystem()->Next( i );
	}

	m_pGameSoundList->SortList();
	if ( m_pGameSoundList->GetItemCount() > 0 )
	{
		int nItemID = m_pGameSoundList->GetItemIDFromRow( 0 );

		// This prevents the refreshing of the sound list from playing the sound
		++m_nSoundSuppressionCount;
		m_pGameSoundList->SetSelectedCell( nItemID, 0 );
	}

	UpdateGameSoundColumnHeader( nTotalCount, nTotalCount );
}


//-----------------------------------------------------------------------------
// Purpose: refreshes the gamesound list
//-----------------------------------------------------------------------------
void CSoundPicker::RefreshGameSoundList()
{
	if ( !m_pGameSoundList )
		return;

	// Check the filter matches
	int nMatchingGameSounds = 0;
	int nTotalCount = 0;
	for ( int nItemID = m_pGameSoundList->FirstItem(); nItemID != m_pGameSoundList->InvalidItemID(); nItemID = m_pGameSoundList->NextItem( nItemID ) )
	{
		KeyValues *kv = m_pGameSoundList->GetItem( nItemID );
		int hGameSound = kv->GetInt( "gameSoundHandle", SoundEmitterSystem()->InvalidIndex() );
		if ( hGameSound == SoundEmitterSystem()->InvalidIndex() )
			continue;
		bool bIsVisible = IsGameSoundVisible( hGameSound );
		m_pGameSoundList->SetItemVisible( nItemID, bIsVisible );
		if ( bIsVisible )
		{
			++nMatchingGameSounds;
		}
		++nTotalCount;
	}

	UpdateGameSoundColumnHeader( nMatchingGameSounds, nTotalCount );

	if ( ( m_pGameSoundList->GetSelectedItemsCount() == 0 ) && ( m_pGameSoundList->GetItemCount() > 0 ) )
	{
		int nItemID = m_pGameSoundList->GetItemIDFromRow( 0 );
		// This prevents the refreshing of the sound list from playing the sound
		++m_nSoundSuppressionCount;
		m_pGameSoundList->SetSelectedCell( nItemID, 0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update filter when text changes
//-----------------------------------------------------------------------------
void CSoundPicker::OnGameSoundFilterTextChanged( )
{
	int nLength = m_pGameSoundFilter->GetTextLength();
	m_GameSoundFilter.SetLength( nLength );
	if ( nLength > 0 )
	{
		m_pGameSoundFilter->GetText( m_GameSoundFilter.GetForModify(), nLength+1 );
	}
	RefreshGameSoundList();
}


//-----------------------------------------------------------------------------
// Purpose: refreshes dialog on filter changing
//-----------------------------------------------------------------------------
void CSoundPicker::OnTextChanged( KeyValues *pKeyValues )
{
	vgui::Panel *pSource = (vgui::Panel*)pKeyValues->GetPtr( "panel" );
	if ( pSource == m_pGameSoundFilter )
	{
		OnGameSoundFilterTextChanged();
		return;
	}

	BaseClass::OnTextChanged( pKeyValues );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a page is shown
//-----------------------------------------------------------------------------
void CSoundPicker::RequestGameSoundFilterFocus( )
{
	m_pGameSoundFilter->SelectAllOnFirstFocus( true );
	m_pGameSoundFilter->RequestFocus();
}


//-----------------------------------------------------------------------------
// Purpose: Called when a page is shown
//-----------------------------------------------------------------------------
void CSoundPicker::OnPageChanged( )
{
	StopSoundPreview();
	if ( m_pGameSoundPage && ( m_pViewsSheet->GetActivePage() == m_pGameSoundPage ) )
	{
		RequestGameSoundFilterFocus();
	}
	if ( m_pWavPage && ( m_pViewsSheet->GetActivePage() == m_pWavPage ) )
	{
		RequestFilterFocus();
	}
}


//-----------------------------------------------------------------------------
// Stop sound preview
//-----------------------------------------------------------------------------
void CSoundPicker::StopSoundPreview( )
{
	if ( m_nPlayingSound != 0 )
	{
		EngineTool()->StopSoundByGuid( m_nPlayingSound );
		m_nPlayingSound = 0;
	}
}


//-----------------------------------------------------------------------------
// Plays a gamesound
//-----------------------------------------------------------------------------
void CSoundPicker::PlayGameSound( const char *pSoundName )
{
	StopSoundPreview();

	CSoundParameters params;
	if ( SoundEmitterSystem()->GetParametersForSound( pSoundName, params, GENDER_NONE ) )
	{
		m_nPlayingSound = EngineTool()->StartSound( 0, true, -1, CHAN_STATIC, params.soundname, 
			params.volume, params.soundlevel, vec3_origin, vec3_origin, 0, 
			params.pitch, false, params.delay_msec / 1000.0f );
	}
}


//-----------------------------------------------------------------------------
// Plays a wav file
//-----------------------------------------------------------------------------
void CSoundPicker::PlayWavSound( const char *pSoundName )
{
	StopSoundPreview();
	m_nPlayingSound = EngineTool()->StartSound( 0, true, -1, CHAN_STATIC, pSoundName, 
		VOL_NORM, SNDLVL_NONE, vec3_origin, vec3_origin, 0, PITCH_NORM, false, 0 );
}


//-----------------------------------------------------------------------------
// Don't play a sound when the next selection is a default selection
//-----------------------------------------------------------------------------
void CSoundPicker::OnNextSelectionIsDefault()
{
	++m_nSoundSuppressionCount;
}

	
//-----------------------------------------------------------------------------
// Derived classes have this called when the previewed asset changes
//-----------------------------------------------------------------------------
void CSoundPicker::OnSelectedAssetPicked( const char *pAssetName )
{
	bool bPlaySounds = true;
	if ( m_nSoundSuppressionCount > 0 )
	{
		--m_nSoundSuppressionCount;
		bPlaySounds = false;
	}

	if ( pAssetName && bPlaySounds )
	{
		PlayWavSound( pAssetName );
	}
}

	
//-----------------------------------------------------------------------------
// Purpose: refreshes dialog on text changing
//-----------------------------------------------------------------------------
void CSoundPicker::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( m_pGameSoundList && (pPanel == m_pGameSoundList ) )
	{
		bool bPlaySounds = true;
		if ( m_nSoundSuppressionCount > 0 )
		{
			--m_nSoundSuppressionCount;
			bPlaySounds = false;
		}

		const char *pGameSoundName = GetSelectedSoundName();
		if ( pGameSoundName && bPlaySounds )
		{
			int len = V_strlen( pGameSoundName );
			char *soundname = ( char* )stackalloc( len + 2 );
			soundname[ 0 ] = '#'; // mark sound to bypass the dsp
			V_strncpy( soundname + 1, pGameSoundName, len + 1 );

			PlayGameSound( soundname );
		}
		return;
	}

	BaseClass::OnItemSelected( kv );
}


//-----------------------------------------------------------------------------
// Gets the selected sound type
//-----------------------------------------------------------------------------
CSoundPicker::PickType_t CSoundPicker::GetSelectedSoundType( )
{
	if ( m_pGameSoundPage && ( m_pViewsSheet->GetActivePage() == m_pGameSoundPage ) )
		return PICK_GAMESOUNDS;
	if ( m_pWavPage && ( m_pViewsSheet->GetActivePage() == m_pWavPage ) )
		return PICK_WAVFILES;
	return PICK_NONE;
}


//-----------------------------------------------------------------------------
// Returns the selected sound count
//-----------------------------------------------------------------------------
int CSoundPicker::GetSelectedSoundCount()
{
	if ( m_pGameSoundPage && ( m_pViewsSheet->GetActivePage() == m_pGameSoundPage ) )
		return m_pGameSoundList->GetSelectedItemsCount();

	if ( m_pWavPage && ( m_pViewsSheet->GetActivePage() == m_pWavPage ) )
		return GetSelectedAssetCount();

	return 0;
}


//-----------------------------------------------------------------------------
// Returns the selected sound
//-----------------------------------------------------------------------------
const char *CSoundPicker::GetSelectedSoundName( int nSelectionIndex )
{
	if ( m_pGameSoundPage && ( m_pViewsSheet->GetActivePage() == m_pGameSoundPage ) )
	{
		int nCount = m_pGameSoundList->GetSelectedItemsCount();
		if ( nCount == 0 )
			return NULL;

		if ( nSelectionIndex < 0 )
		{
			nSelectionIndex = nCount - 1;
		}
		int nIndex = m_pGameSoundList->GetSelectedItem( nSelectionIndex );
		if ( nIndex >= 0 )
		{
			KeyValues *pkv = m_pGameSoundList->GetItem( nIndex );
			return pkv->GetString( "gamesound", NULL );
		}
		return NULL;
	}

	if ( m_pWavPage && ( m_pViewsSheet->GetActivePage() == m_pWavPage ) )
		return GetSelectedAsset( nSelectionIndex );

	return NULL;
}


//-----------------------------------------------------------------------------
//
// Purpose: Modal picker frame
//
//-----------------------------------------------------------------------------
CSoundPickerFrame::CSoundPickerFrame( vgui::Panel *pParent, const char *pTitle, int nFlags ) : 
	BaseClass( pParent )
{
	SetAssetPicker( new CSoundPicker( this, nFlags ) );
	LoadControlSettingsAndUserConfig( "resource/soundpickerframe.res" );
	SetTitle( pTitle, false );
}

CSoundPickerFrame::~CSoundPickerFrame()
{
}


//-----------------------------------------------------------------------------
// Purpose: Activate the dialog
//-----------------------------------------------------------------------------
void CSoundPickerFrame::DoModal( CSoundPicker::PickType_t initialType, const char *pInitialValue, KeyValues *pContextKeyValues )
{
	vgui::surface()->SetCursor( dc_hourglass );
	CSoundPicker *pPicker = static_cast <CSoundPicker*>( GetAssetPicker() );
	if ( initialType != CSoundPicker::PICK_NONE && pInitialValue )
	{
		pPicker->SetSelectedSound( initialType, pInitialValue );
	}
	BaseClass::DoModal( pContextKeyValues );
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CSoundPickerFrame::OnCommand( const char *pCommand )
{
	CSoundPicker *pPicker = static_cast <CSoundPicker*>( GetAssetPicker() );
	if ( !Q_stricmp( pCommand, "Open" ) )
	{
		CSoundPicker::PickType_t type = pPicker->GetSelectedSoundType( );
		if (( type == CSoundPicker::PICK_GAMESOUNDS ) || ( type == CSoundPicker::PICK_WAVFILES ))
		{
			const char *pSoundName = pPicker->GetSelectedSoundName();

			int len = V_strlen( pSoundName );
			char *soundname = ( char* )stackalloc( len + 2 );
			soundname[ 0 ] = '#'; // mark sound to bypass the dsp
			V_strncpy( soundname + 1, pSoundName, len + 1 );

			int nSoundCount = pPicker->GetSelectedSoundCount();

			KeyValues *pActionKeys = new KeyValues( "SoundSelected" );
			pActionKeys->SetInt( "count", nSoundCount );
			KeyValues *pSoundList = NULL;
			if ( type == CSoundPicker::PICK_GAMESOUNDS )
			{
				pActionKeys->SetString( "gamesound", soundname );
				if ( pPicker->IsMultiselectEnabled() )
				{
					pSoundList = pActionKeys->FindKey( "gamesounds", true );
				}
			}
			else
			{
				pActionKeys->SetString( "wav", soundname );
				if ( pPicker->IsMultiselectEnabled() )
				{
					pSoundList = pActionKeys->FindKey( "wavs", true );
				}
			}

			if ( pSoundList )
			{
				// Adds them in selection order
				for ( int i = 0; i < nSoundCount; ++i )
				{
					char pBuf[32];
					Q_snprintf( pBuf, sizeof(pBuf), "%d", i );
					pSoundName = pPicker->GetSelectedSoundName( i );

					len = V_strlen( pSoundName );
					soundname = ( char* )malloc( len + 2 );
					soundname[ 0 ] = '#'; // mark sound to bypass the dsp
					V_strncpy( soundname + 1, pSoundName, len + 1 );

					pSoundList->SetString( pBuf, soundname );
					free( soundname );
				}
			}

			PostMessageAndClose( pActionKeys );
			CloseModal();
		}
		return;
	}

	BaseClass::OnCommand( pCommand );
}

	
