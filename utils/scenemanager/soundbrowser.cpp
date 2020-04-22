//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "workspacebrowser.h"
#include "workspace.h"
#include "project.h"
#include <windows.h>
#include "resource.h"
#include "project.h"
#include "vcdfile.h"
#include "soundentry.h"
#include "scene.h"

#include "workspacemanager.h"
#include "soundbrowser.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "iscenemanagersound.h"
#include "snd_wave_source.h"
#include "cmdlib.h"
#include "tabwindow.h"
#include "soundproperties.h"
#include "soundproperties_multiple.h"
#include "wavebrowser.h"
#include "wavefile.h"
#include "inputproperties.h"
#include "drawhelper.h"
#include "utlbuffer.h"

#define ENTRY_ALLSOUNDS			"AllSounds"
#define ENTRY_ALL_INDEX			0
#define ENTRY_SEARCHRESULTS		"Search results"
#define ENTRY_SEARCH_INDEX		1

enum
{
	// Controls
	IDC_SB_LISTVIEW = 101,
	IDC_SB_FILTERTAB,

	// Messages
	IDC_SB_PLAY = 1000,
	IDC_SB_SOUNDPROPERTIES,
	IDC_SB_SHOWINWAVEBROWSER,
	IDC_SB_ADDSOUND,
	IDC_SB_REMOVESOUND,
	IDC_SB_GETSENTENCE,
};

enum
{
	COL_SOUND = 0,
	COL_COUNT,
	COL_WAV,
	COL_SENTENCE,
	COL_CHANNEL,
	COL_VOLUME,
	COL_SOUNDLEVEL,
	COL_PITCH,
	COL_SCRIPT,
	COL_CC,
};

class CSoundList : public mxListView
{
public:
	CSoundList( mxWindow *parent, int id = 0 ) 
		: mxListView( parent, 0, 0, 0, 0, id )
	{
		// SendMessage ( (HWND)getHandle(), WM_SETFONT, (WPARAM) (HFONT) GetStockObject (ANSI_FIXED_FONT), MAKELPARAM (TRUE, 0));

		//HWND wnd = (HWND)getHandle();
		//DWORD style = GetWindowLong( wnd, GWL_STYLE );
		//style |= LVS_SORTASCENDING;
		//SetWindowLong( wnd, GWL_STYLE, style );

		//SceneManager_AddWindowStyle( this, LVS_SORTASCENDING );

		// Add column headers
		insertTextColumn( COL_SOUND, 200, "Sound" );
		insertTextColumn( COL_COUNT, 20, "#" );
		insertTextColumn( COL_WAV, 220, "WAV Filename" );
		insertTextColumn( COL_SENTENCE, 300, "Sentence Text" );
		insertTextColumn( COL_CHANNEL, 100, "Channel" );
		insertTextColumn( COL_VOLUME, 100, "Volume" );
		insertTextColumn( COL_SOUNDLEVEL, 120, "Soundlevel" );
		insertTextColumn( COL_PITCH, 100, "Pitch" );
		insertTextColumn( COL_SCRIPT, 150, "Script File" );
		insertTextColumn( COL_CC, 300, "CC Text" );
	}
};

class CSoundFilterTab : public CTabWindow
{
public:
	typedef CTabWindow BaseClass;

	CSoundFilterTab( mxWindow *parent, int x, int y, int w, int h, int id = 0, int style = 0 ) :
		CTabWindow( parent, x, y, w, h, id, style )
	{
		SetInverted( true );
		SetRowHeight( 20 );
	}

	virtual void ShowRightClickMenu( int mx, int my )
	{
		// Nothing
	}

	void	Init( CUtlSymbolTable& table, CUtlVector< CUtlSymbol >& scripts )
	{
		add( ENTRY_ALLSOUNDS );
		add( ENTRY_SEARCHRESULTS );

		int c = scripts.Count();
		for ( int i = 0; i < c; i++ )
		{
			CUtlSymbol& sym = scripts[ i ];
			add( table.String( sym ) );
		}
		select( 0 );
	}

	void UpdatePrefixes()
	{
		int c = getItemCount();
		// Skip All and search results
		for ( int i = 2; i < c; i++ )
		{
			setPrefix( i, "" );

			char const *script = getLabel( i );
			if ( !script )
				continue;

			int scriptindex = g_pSoundEmitterSystem->FindSoundScript( va( "scripts/%s.txt", script ) );
			if ( scriptindex < 0 )
				continue;

			if ( g_pSoundEmitterSystem->IsSoundScriptDirty( scriptindex ) )
			{
				setPrefix( i, "* " );
			}
		}

		RecomputeLayout( w2() );
		redraw();
	}
};


class COptionsWindow : public mxWindow
{
	typedef mxWindow BaseClass;
public:
	enum
	{
		IDC_VOICE_ONLY = 1000,
			IDC_PLAY_SOUND,
			IDC_STOP_SOUNDS,
			IDC_SEARCH,
	};
	
	COptionsWindow( CSoundBrowser *browser ) : BaseClass( browser, 0, 0, 0, 0 ), m_pBrowser( browser )
	{
		SceneManager_AddWindowStyle( this, WS_CLIPSIBLINGS | WS_CLIPCHILDREN );

		m_szSearchString[0]=0;

		m_pChanVoiceOnly = new mxCheckBox( this, 0, 0, 0, 0, "CHAN_VOICE only", IDC_VOICE_ONLY );
		m_pChanVoiceOnly->setChecked( true );
		
		m_pPlay = new mxButton( this, 0, 0, 0, 0, "Play", IDC_PLAY_SOUND );
		
		m_pStopSounds = new mxButton( this, 0, 0, 0, 0, "Stop Sounds", IDC_STOP_SOUNDS );
		
		m_pSearch = new mxButton( this, 0, 0, 0, 0, "Search...", IDC_SEARCH );

		m_pSearchString = new mxLabel( this, 0, 0, 0, 0, "" );
	}
	
	bool PaintBackground( void )
	{
		redraw();
		return false;
	}


	virtual void redraw()
	{
		CDrawHelper drawHelper( this, GetSysColor( COLOR_BTNFACE ) );
	}

	virtual int handleEvent( mxEvent *event )
	{
		int iret = 0;
		switch ( event->event )
		{
		default:
			break;
		case mxEvent::Size:
			{
				iret = 1;
				
				int split = 120;
				
				int x = 1;
				
				m_pPlay->setBounds( x, 1, split, h2() - 2 );
				
				x += split + 10;
				
				m_pStopSounds->setBounds( x, 1, split, h2()-2 );
				
				x += split + 10;
				
				m_pChanVoiceOnly->setBounds( x, 1, split, h2() - 2 );
								
				x += split + 10;
				
				m_pSearch->setBounds( x, 1, split, h2() - 2 );

				x += split + 10;

				m_pSearchString->setBounds( x, 2, split * 2, h2() - 4 );

				x += split * 2 + 10;

			}
			break;
		case mxEvent::Action:
			{
				switch ( event->action )
				{
				case IDC_STOP_SOUNDS:
					{
						iret = 1;
						sound->StopAll();
					}
					break;
				case IDC_PLAY_SOUND:
					{
						iret = 1;
						m_pBrowser->OnPlay();
					}
					break;
				case IDC_VOICE_ONLY:
					{
						iret = 1;
						m_pBrowser->RepopulateTree();
					};
					break;
				case IDC_SEARCH:
					{
						iret = 1;
						OnSearch();
					};
					break;
				default:
					break;
				}
			}
			break;
		}
		
		return iret;
	}
	
	bool IsChanVoiceOnly() const
	{
		return m_pChanVoiceOnly->isChecked();
	}
	
	char const	*GetSearchString()
	{
		return m_szSearchString;
	}

	void OnSearch()
	{
		CInputParams params;
		memset( &params, 0, sizeof( params ) );
		Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Search" );
		Q_strcpy( params.m_szPrompt, "Find:" );
		Q_strcpy( params.m_szInputText, m_szSearchString );

		if ( !InputProperties( &params ) )
			return;

		Q_strcpy( m_szSearchString, params.m_szInputText );

		m_pSearchString->setLabel( va( "Search:  '%s'", GetSearchString() ) );

		m_pBrowser->OnSearch();
	}
	
private:
	
	mxCheckBox		*m_pChanVoiceOnly;
	mxButton		*m_pStopSounds;
	mxButton		*m_pPlay;
	mxButton		*m_pSearch;
	mxLabel			*m_pSearchString;
	
	CSoundBrowser	*m_pBrowser;

	char			m_szSearchString[ 256 ];
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CSoundBrowser::CSoundBrowser( mxWindow *parent, CWorkspaceManager *manager, int id ) :
	BaseClass( parent, 0, 0, 0, 0, "Sound Browser", id )
{
	m_pManager = manager;
	
	SceneManager_MakeToolWindow( this, false );

	m_pListView = new CSoundList( this, IDC_SB_LISTVIEW );
	m_pFilter = new CSoundFilterTab( this, 0, 0, 0, 0, IDC_SB_FILTERTAB );
	m_pOptions = new COptionsWindow( this );

	HIMAGELIST list = GetWorkspaceManager()->CreateImageList();

	// Associate the image list with the tree-view control. 
    m_pListView->setImageList( (void *)list ); 

	LoadAllSounds();

	m_pFilter->select( 0 );
	RepopulateTree();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSoundBrowser::OnDelete()
{
	RemoveAllSounds();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : int
//-----------------------------------------------------------------------------
int CSoundBrowser::handleEvent( mxEvent *event )
{
	int iret = 0;
	switch ( event->event )
	{
	default:
		break;
	case mxEvent::Action:
		{
			iret = 1;
			switch ( event->action )
			{
			default:
				{
					iret = 0;
				}
				break;
			case IDC_SB_FILTERTAB:
				{
					RepopulateTree();
				}
				break;
			case IDC_SB_LISTVIEW:
				{
					bool rightmouse = ( event->flags == mxEvent::RightClicked ) ? true : false;
					bool doubleclicked = ( event->flags == mxEvent::DoubleClicked ) ? true : false;

					if ( rightmouse )
					{
						ShowContextMenu();
					}
					else if ( doubleclicked )
					{
						if ( m_pListView->getNumSelected() == 1 )
						{
							int index = m_pListView->getNextSelectedItem( -1 );
							if ( index >= 0 )
							{
								CSoundEntry *se = (CSoundEntry *)m_pListView->getUserData( index, 0 );
								if ( se )
								{
									se->Play();

									CWaveBrowser *wb = GetWorkspaceManager()->GetWaveBrowser();
									if ( wb && se->GetWaveCount() > 0 )
									{
										CWaveFile *firstwave = se->GetWave( 0 );
										Assert( firstwave );
										if ( firstwave )
										{
											wb->JumpToItem( firstwave );
										}
									}
								}
							}
						}
					}
				}
				break;
			case IDC_SB_PLAY:
				{
					OnPlay();
				}
				break;
			case IDC_SB_GETSENTENCE:
				{
					OnGetSentence();
				}
				break;
			case IDC_SB_SOUNDPROPERTIES:
				{
					OnSoundProperties();
				}
				break;
			case IDC_SB_SHOWINWAVEBROWSER:
				{
					OnShowInWaveBrowser();
				}
				break;
			case IDC_SB_ADDSOUND:
				{
					OnAddSound();
				}
				break;
			case IDC_SB_REMOVESOUND:
				{
					OnRemoveSound();
				}
				break;
			}
		}
		break;
	case mxEvent::Size:
		{
			int optionsh = 20;
			int filterh = m_pFilter->GetBestHeight( w2() );

			m_pOptions->setBounds( 0, 0, w2(), optionsh );
			m_pListView->setBounds( 0, optionsh, w2(), h2() - filterh - optionsh );
			m_pFilter->setBounds( 0, h2() - filterh, w2(), filterh );

			GetWorkspaceManager()->SetWorkspaceDirty();

			iret = 1;
		}
		break;
	case mxEvent::Close:
		{
			iret = 1;
		}
		break;
	}

	return iret;
}

static bool NameLessFunc( CSoundEntry *const& name1, CSoundEntry *const& name2 )
{
	if ( Q_stricmp( name1->GetName(), name2->GetName() ) < 0 )
		return true;
	return false;
}

void CSoundBrowser::LoadAllSounds()
{
	RemoveAllSounds();

	// int c = g_pSoundEmitterSystem->GetSoundCount();
	int added = 0;

	int i;
	for ( i = g_pSoundEmitterSystem->First(); i != g_pSoundEmitterSystem->InvalidIndex(); i = g_pSoundEmitterSystem->Next( i ) )
	{
		char const *name = g_pSoundEmitterSystem->GetSoundName( i );
		CSoundEntry *se = new CSoundEntry( NULL, name );
		m_AllSounds.AddToTail( se );

		char filebase [ 512 ];
		Q_FileBase( g_pSoundEmitterSystem->GetSourceFileForSound( i ), filebase, sizeof( filebase ) );

		// Add script file symbol
		CUtlSymbol script_sym = m_ScriptTable.AddString( filebase );

		if ( m_Scripts.Find( script_sym ) == m_Scripts.InvalidIndex() )
		{
			m_Scripts.AddToTail( script_sym );
		}

		++added;

		if ( !( added % 500 ) )
		{
		//	Con_Printf( "CSoundBrowser:  loaded %i sounds\n", added );
		}
	}

	m_pFilter->Init( m_ScriptTable, m_Scripts );
}

void CSoundBrowser::RemoveAllSounds()
{
	int c = m_AllSounds.Count();
	for ( int i = 0; i < c; i++ )
	{
		CSoundEntry *se = m_AllSounds[ i ];
		delete se;
	}

	m_AllSounds.RemoveAll();
	m_Scripts.RemoveAll();
	m_CurrentSelection.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSoundBrowser::PopulateTree( bool voiceonly, char const *scriptonly )
{
	int i;

	CUtlRBTree< CSoundEntry *, int >		m_Sorted( 0, 0, NameLessFunc );

	bool textsearch = false;
	char const *texttofind = NULL;

	if ( scriptonly )
	{
		if ( !Q_stricmp( scriptonly, ENTRY_ALLSOUNDS ) )
		{
			scriptonly = NULL;
		}
		else if ( !Q_stricmp( scriptonly, ENTRY_SEARCHRESULTS ) )
		{
			scriptonly = NULL;
			textsearch = true;
			texttofind = GetSearchString();
		}
	}

	int c = m_AllSounds.Count();
	for ( i = 0; i < c; i++ )
	{
		CSoundEntry *se = m_AllSounds[ i ];
		char const *name = se->GetName();

		CSoundParametersInternal *params = se->GetSoundParameters();
		if ( !params )
			continue;

		if ( voiceonly && params->GetChannel() != CHAN_VOICE )
			continue;

		if ( scriptonly )
		{
			if ( Q_stricmp( scriptonly, se->GetScriptFile() ) )
				continue;
		}

		if ( textsearch && texttofind )
		{
			bool keep = false;
			if ( Q_stristr( name, texttofind ) )
			{
				keep = true;
			}
			else
			{
				int waveCount = se->GetWaveCount();
				for ( int wave = 0; wave < waveCount; wave++ )
				{
					CWaveFile *w = se->GetWave( wave );
					if ( !w )
						continue;

					char const *wavename = w->GetFileName();
					if ( !wavename )
					{
						Assert( 0 );
						continue;
					}

					if ( !Q_stristr( wavename, texttofind ) )
					{
						continue;
					}

					keep = true;
					break;
				}
			}


			if ( !keep )
			{
				continue;
			}
		}

		m_Sorted.Insert( se );
	}


// Repopulate tree
	m_pListView->removeAll();

	int loadcount = 0;

	m_pListView->setDrawingEnabled( false );

	for ( i = m_Sorted.FirstInorder(); i != m_Sorted.InvalidIndex(); i = m_Sorted.NextInorder( i ) )
	{
		CSoundEntry *se = m_Sorted[ i ];
		char const *name = se->GetName();
		CSoundParametersInternal *params = se->GetSoundParameters();
		if ( !params )
			continue;

		int slot = m_pListView->add( name );

		m_pListView->setUserData( slot, COL_SOUND, (void *)se );

		m_pListView->setImage( slot, COL_SOUND, se->GetIconIndex() );

		int waveCount = params->NumSoundNames();

		if ( waveCount >= 1 )
		{
			m_pListView->setLabel( slot, COL_COUNT, waveCount > 1 ? va( "%i", waveCount ) : "" );
			m_pListView->setLabel( slot, COL_WAV, g_pSoundEmitterSystem->GetWaveName( params->GetSoundNames()[ 0 ].symbol ) );
		}

		m_pListView->setLabel( slot, COL_SENTENCE, se->GetSentenceText( 0 ) );

		m_pListView->setLabel( slot, COL_CHANNEL, params->ChannelToString() );
		m_pListView->setLabel( slot, COL_VOLUME, params->VolumeToString() );
		m_pListView->setLabel( slot, COL_SOUNDLEVEL, params->SoundLevelToString() );
		m_pListView->setLabel( slot, COL_PITCH, params->PitchToString() );

		wchar_t buf[ 1024 ];
		se->GetCCText( buf, 1024 );
		m_pListView->setLabel( slot, COL_CC, buf );

		char filebase [ 512 ];
		
		int soundIndex = g_pSoundEmitterSystem->GetSoundIndex( name );

		Q_FileBase( g_pSoundEmitterSystem->GetSourceFileForSound( soundIndex ), filebase, sizeof( filebase ) );

		m_pListView->setLabel( slot, COL_SCRIPT, filebase );

		++loadcount;
	}

	m_pListView->setDrawingEnabled( true );

	// Con_Printf( "CSoundBrowser:  selected %i sounds\n", loadcount );
}

CWorkspaceManager *CSoundBrowser::GetManager()
{
	return m_pManager;
}

void CSoundBrowser::RepopulateTree()
{
	bool voiceonly = m_pOptions->IsChanVoiceOnly();

	int slot = m_pFilter->getSelectedIndex();

	if ( 0 >= slot )
	{
		PopulateTree( voiceonly, NULL );
	}
	else
	{
		PopulateTree( voiceonly, m_pFilter->getLabel( slot ) );
	}

	m_pFilter->UpdatePrefixes();
}

void CSoundBrowser::BuildSelectionList( CUtlVector< CSoundEntry * >& selected )
{
	selected.RemoveAll();

	int idx = -1;
	do 
	{
		idx = m_pListView->getNextSelectedItem( idx );
		if ( idx != -1 )
		{
			CSoundEntry *se = (CSoundEntry *)m_pListView->getUserData( idx, 0 );
			if ( se )
			{
				selected.AddToTail( se );
			}
		}
	} while ( idx != -1 );
	
}

void CSoundBrowser::ShowContextMenu( void )
{
	BuildSelectionList( m_CurrentSelection );
	if ( m_CurrentSelection.Count() <= 0 )
		return;

	POINT pt;
	GetCursorPos( &pt );
	ScreenToClient( (HWND)getHandle(), &pt );

	// New scene, edit comments
	mxPopupMenu *pop = new mxPopupMenu();

	if ( m_CurrentSelection.Count() == 1 )
	{
		pop->add ("&Play", IDC_SB_PLAY );
		pop->addSeparator();
	}

	pop->add( "Refresh sentence data", IDC_SB_GETSENTENCE );

	pop->add( "Add sound entry...", IDC_SB_ADDSOUND );
	if ( m_CurrentSelection.Count() >= 1 )
	{
		pop->add( "Remove sound(s)", IDC_SB_REMOVESOUND );
	}

	pop->addSeparator();

	pop->add( "Show in Wave Browser", IDC_SB_SHOWINWAVEBROWSER );

	pop->add( "&Properties...", IDC_SB_SOUNDPROPERTIES );

	pop->popup( this, pt.x, pt.y );
}

void CSoundBrowser::OnPlay()
{
	BuildSelectionList( m_CurrentSelection );
	if ( m_CurrentSelection.Count() == 1 )
	{
		CSoundEntry *se = m_CurrentSelection[ 0 ];
		if ( se )
		{
			se->Play();
		}
	}
}

void CSoundBrowser::JumpToItem( CSoundEntry *se )
{
	char const *script = se->GetScriptFile();
	
	bool voiceonly = m_pOptions->IsChanVoiceOnly();

	if ( !script || !script[ 0 ] )
	{
		PopulateTree( voiceonly, NULL );
	}
	else
	{
		PopulateTree( voiceonly, script );
	}

	int idx = 0;
	int c = m_pListView->getItemCount();
	for ( ; idx < c; idx++ )
	{
		CSoundEntry *item = (CSoundEntry *)m_pListView->getUserData( idx, 0 );
		if ( !Q_stricmp( item->GetName(), se->GetName() ) )
		{
			break;
		}
	}

	if ( idx < c )
	{
		m_pListView->scrollToItem( idx );
	}
}

void CSoundBrowser::OnSoundProperties()
{
	BuildSelectionList( m_CurrentSelection );
	if ( m_CurrentSelection.Count() < 1 )
	{
		Con_Printf( "No selection\n" );
		return;
	}

	CSoundParams params;
	memset( &params, 0, sizeof( params ) );

	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Sound Properties" );

	int c = m_CurrentSelection.Count();
	for ( int i = 0 ; i < c; i++ )
	{
		CSoundEntry *entry = m_CurrentSelection[ i ];
		if ( !entry )
			continue;

		params.items.AddToTail( entry );
	}

	if ( params.items.Count() > 1 )
	{
		SoundProperties_Multiple( &params );
	}
	else
	{
		SoundProperties( &params );
	}
}

void CSoundBrowser::OnShowInWaveBrowser()
{
	if ( m_pListView->getNumSelected() == 1 )
	{
		int index = m_pListView->getNextSelectedItem( -1 );
		if ( index >= 0 )
		{
			CSoundEntry *se = (CSoundEntry *)m_pListView->getUserData( index, 0 );
			if ( se )
			{
				CWaveBrowser *wb = GetWorkspaceManager()->GetWaveBrowser();
				if ( wb && se->GetWaveCount() > 0 )
				{
					CWaveFile *firstwave = se->GetWave( 0 );
					Assert( firstwave );
					if ( firstwave )
					{
						wb->JumpToItem( firstwave );
					}
				}
			}
		}
	}
}

void CSoundBrowser::OnSearch()
{
	m_pFilter->select( ENTRY_SEARCH_INDEX );
	RepopulateTree();
}

char const *CSoundBrowser::GetSearchString()
{
	return m_pOptions->GetSearchString();
}

void CSoundBrowser::OnAddSound()
{
	CSoundParams params;
	memset( &params, 0, sizeof( params ) );
	params.addsound = true;

	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "New Sound" );

	if ( !SoundProperties( &params ) )
		return;

	if ( params.items.Count() == 1 )
	{
		CSoundEntry *newItem = params.items[ 0 ];
		m_AllSounds.AddToTail( newItem );

		int slot = g_pSoundEmitterSystem->GetSoundIndex( newItem->GetName() );
		if ( g_pSoundEmitterSystem->IsValidIndex( slot ) )
		{
			CSoundParametersInternal *p = g_pSoundEmitterSystem->InternalGetParametersForSound( slot );
			if ( p )
			{
				CWaveBrowser *wb = GetWorkspaceManager()->GetWaveBrowser();
				Assert( wb );

				int waveCount = p->NumSoundNames();
				for ( int wave = 0; wave < waveCount; wave++ )
				{
					char const *wavname = g_pSoundEmitterSystem->GetWaveName( p->GetSoundNames()[ wave ].symbol );
					if ( wavname )
					{
						CWaveFile *wavefile = wb->FindEntry( wavname, true );
						if ( wavefile )
						{
							newItem->AddWave( wavefile );
						}
					}
				}
			}
		}
	}

	// Repopulate things
	GetWorkspaceManager()->RefreshBrowsers();
}

void CSoundBrowser::OnRemoveSound()
{
	BuildSelectionList( m_CurrentSelection );
	if ( m_CurrentSelection.Count() < 1 )
	{
		Con_Printf( "No selection\n" );
		return;
	}

	int c = m_CurrentSelection.Count();
	for ( int i = c - 1; i >= 0 ; i-- )
	{
		CSoundEntry *se = m_CurrentSelection[ i ];
		Assert( se );

		// FIXME: See if still referenced by a vcd?

		g_pSoundEmitterSystem->RemoveSound( se->GetName() );
		m_AllSounds.FindAndRemove( se );
		delete se;
	}

	// Repopulate things
	GetWorkspaceManager()->RefreshBrowsers();
}

void CSoundBrowser::OnGetSentence()
{
	BuildSelectionList( m_CurrentSelection );
	if ( m_CurrentSelection.Count() < 1 )
	{
		Con_Printf( "No selection\n" );
		return;
	}

	int c = m_CurrentSelection.Count();
	for ( int i = c - 1; i >= 0 ; i-- )
	{
		CSoundEntry *se = m_CurrentSelection[ i ];
		Assert( se );

		int c = se->GetWaveCount();
		for ( int i = 0; i < c; ++i )
		{
			CWaveFile *wav = se->GetWave( i );
			if ( !wav->HasLoadedSentenceInfo() )
			{
				wav->EnsureSentence();
			}
		}
	}

	// Repopulate things
	GetWorkspaceManager()->RefreshBrowsers();
}
