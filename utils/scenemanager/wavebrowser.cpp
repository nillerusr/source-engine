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
#include "wavefile.h"
#include "scene.h"

#include "workspacemanager.h"
#include "wavebrowser.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "iscenemanagersound.h"
#include "snd_wave_source.h"
#include "cmdlib.h"
#include "tabwindow.h"
#include "inputproperties.h"
#include "waveproperties.h"
#include "drawhelper.h"
#include "ifileloader.h"
#include "MultipleRequest.h"

#include "soundchars.h"

enum
{
	// Controls
	IDC_SB_LISTVIEW = 101,
	IDC_SB_FILETREE,

	// Messages
	IDC_SB_PLAY = 1000,
	IDC_SB_CHECKOUT,
	IDC_SB_CHECKIN,
	IDC_SB_PROPERTIES,
	IDC_SB_ENABLEVOICEDUCKING,
	IDC_SB_DISABLEVOICEDUCKING,

	IDC_SB_EXPORTSENTENCE,
	IDC_SB_IMPORTSENTENCE,

	IDC_SB_GETSENTENCE,
};

enum
{
	COL_WAV = 0,
	COL_DUCKED,
	COL_SENTENCE
};

class CWaveList : public mxListView
{
public:
	CWaveList( mxWindow *parent, int id = 0 ) 
		: mxListView( parent, 0, 0, 0, 0, id )
	{
		// SendMessage ( (HWND)getHandle(), WM_SETFONT, (WPARAM) (HFONT) GetStockObject (ANSI_FIXED_FONT), MAKELPARAM (TRUE, 0));

		//HWND wnd = (HWND)getHandle();
		//DWORD style = GetWindowLong( wnd, GWL_STYLE );
		//style |= LVS_SORTASCENDING;
		//SetWindowLong( wnd, GWL_STYLE, style );

		//SceneManager_AddWindowStyle( this, LVS_SORTASCENDING );

		// Add column headers
		insertTextColumn( COL_WAV, 300, "WAV" );
		insertTextColumn( COL_DUCKED, 50, "Ducked" );
		insertTextColumn( COL_SENTENCE, 300, "Sentence Text" );
	}
};

class CWaveFileTree : public mxTreeView
{
public:
	CWaveFileTree( mxWindow *parent, int id = 0 ) : mxTreeView( parent, 0, 0, 0, 0, id ),
		m_Paths( 0, 0, FileTreeLessFunc )
	{
	}

	void	Clear()
	{
		removeAll();
		m_Paths.RemoveAll();
	}

	void	FindOrAddSubdirectory( char const *subdir )
	{
		FileTreePath fp;
		Q_strcpy( fp.path, subdir );

		if ( m_Paths.Find( fp ) != m_Paths.InvalidIndex() )
			return;

		m_Paths.Insert( fp );
	}

	mxTreeViewItem *FindOrAddChildItem( mxTreeViewItem *parent, char const *child )
	{
		mxTreeViewItem *p = getFirstChild( parent );
		if ( !p )
		{
			return add( parent, child );
		}

		while ( p )
		{
			if ( !Q_stricmp( getLabel( p ), child ) )
				return p;

			p = getNextChild( p );
		}

		return add( parent, child );
	}

	void	_PopulateTree( int pathId, char const *path )
	{
		char sz[ 512 ];
		Q_strcpy( sz, path );
		char *p = sz;

		// Start at root
		mxTreeViewItem *cur = NULL;

		// Tokenize path
		while ( p && p[0] )
		{
			char *slash = Q_strstr( p, "/" );
			if ( !slash )
			{
				slash = Q_strstr( p, "\\" );
			}

			char *check = p;

			if ( slash )
			{
				*slash = 0;

				// see if a child of current already exists with this name
				p = slash + 1;
			}
			else
			{
				p = NULL;
			}

			Assert( check );

			cur = FindOrAddChildItem( cur, check );
		}

		setUserData( cur, (void *)pathId );
	}

	char const *GetSelectedPath( void )
	{
		mxTreeViewItem *tvi = getSelectedItem();
		int id = (int)getUserData( tvi );

		if ( id < 0 || id >= m_Paths.Count() )
		{
			Assert( 0 );
			return "";
		}
		return m_Paths[ id ].path;
	}

	void	PopulateTree()
	{
		int i;
		for  ( i = m_Paths.FirstInorder(); i != m_Paths.InvalidIndex(); i = m_Paths.NextInorder( i ) )
		{
			_PopulateTree( i, m_Paths[ i ].path );
		}

		mxTreeViewItem *p = getFirstChild( NULL );
		setOpen( p, true );
	}

	struct FileTreePath
	{
		char	path[ MAX_PATH ];
	};

	static bool FileTreeLessFunc( const FileTreePath &lhs, const FileTreePath &rhs )
	{
		return Q_stricmp( lhs.path, rhs.path ) < 0;
	}

	CUtlRBTree< FileTreePath, int >	m_Paths;
};

class CWaveOptionsWindow : public mxWindow
{
typedef mxWindow BaseClass;
public:
	enum
	{
		IDC_PLAY_SOUND = 1000,
		IDC_STOP_SOUNDS,
		IDC_SEARCH,
	};

	CWaveOptionsWindow( CWaveBrowser *browser ) : BaseClass( browser, 0, 0, 0, 0 ), m_pBrowser( browser )
	{
		SceneManager_AddWindowStyle( this, WS_CLIPSIBLINGS | WS_CLIPCHILDREN );

		m_szSearchString[0]=0;

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
	
	mxButton		*m_pStopSounds;
	mxButton		*m_pPlay;
	mxButton		*m_pSearch;
	mxLabel			*m_pSearchString;
	
	CWaveBrowser	*m_pBrowser;

	char			m_szSearchString[ 256 ];
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CWaveBrowser::CWaveBrowser( mxWindow *parent, CWorkspaceManager *manager, int id ) :
	BaseClass( parent, 0, 0, 0, 0, "Wave Browser", id )
{
	m_pManager = manager;
	
	SceneManager_MakeToolWindow( this, false );

	m_pListView = new CWaveList( this, IDC_SB_LISTVIEW );
	m_pOptions = new CWaveOptionsWindow( this );
	m_pFileTree = new CWaveFileTree( this, IDC_SB_FILETREE );

	HIMAGELIST list = GetWorkspaceManager()->CreateImageList();

	// Associate the image list with the tree-view control. 
    m_pListView->setImageList( (void *)list ); 

	LoadAllSounds();

	PopulateTree( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaveBrowser::OnDelete()
{
	RemoveAllSounds();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : int
//-----------------------------------------------------------------------------
int CWaveBrowser::handleEvent( mxEvent *event )
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
								CWaveFile *wav = (CWaveFile *)m_pListView->getUserData( index, 0 );
								if ( wav )
								{
									wav->Play();
								}
							}
						}
					}
				}
				break;
			case IDC_SB_FILETREE:
				{
					PopulateTree( m_pFileTree->GetSelectedPath() );
				}
				break;
			case IDC_SB_PLAY:
				{
					OnPlay();
				}
				break;
			case  IDC_SB_GETSENTENCE:
				{
					OnGetSentence();
				}
				break;
			case IDC_SB_PROPERTIES:
				{
					OnWaveProperties();
				}
				break;
			case IDC_SB_CHECKOUT:
				{
					OnCheckout();
				}
				break;
			case IDC_SB_CHECKIN:
				{
					OnCheckin();
				}
				break;
			case IDC_SB_ENABLEVOICEDUCKING:
				{
					OnEnableVoiceDucking();
				}
				break;
			case IDC_SB_DISABLEVOICEDUCKING:
				{
					OnDisableVoiceDucking();
				}
				break;
			case IDC_SB_IMPORTSENTENCE:
				{
					OnImportSentence();
				}
				break;
			case IDC_SB_EXPORTSENTENCE:
				{
					OnExportSentence();
				}
				break;
			}
		}
		break;
	case mxEvent::Size:
		{
			int optionsh = 20;

			m_pOptions->setBounds( 0, 0, w2(), optionsh );

			int filetreewidth = 175;

			m_pFileTree->setBounds( 0, optionsh, filetreewidth, h2() - optionsh );
			m_pListView->setBounds( filetreewidth, optionsh, w2() - filetreewidth, h2() - optionsh );

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

static bool NameLessFunc( CWaveFile *const& name1, CWaveFile *const& name2 )
{
	if ( Q_stricmp( name1->GetName(), name2->GetName() ) < 0 )
		return true;
	return false;
}

#define SOUND_PREFIX_LEN	6
//-----------------------------------------------------------------------------
// Finds all .wav files in a particular directory
//-----------------------------------------------------------------------------
bool CWaveBrowser::LoadWaveFilesInDirectory( CUtlDict< CWaveFile *, int >& soundlist, char const* pDirectoryName, int nDirectoryNameLen )
{
	Assert( Q_strnicmp( pDirectoryName, "sound", 5 ) == 0 );

	char *pWildCard;
	pWildCard = ( char * )stackalloc( nDirectoryNameLen + 7 );
	Q_snprintf( pWildCard, nDirectoryNameLen + 7, "%s/*.wav", pDirectoryName );

	if ( !filesystem )
	{
		return false;
	}

	FileFindHandle_t findHandle;
	const char *pFileName = filesystem->FindFirst( pWildCard, &findHandle );
	while( pFileName )
	{
		if( !filesystem->FindIsDirectory( findHandle ) )
		{
			// Strip off the 'sound/' part of the name.
			char *pFileNameWithPath;
			int nAllocSize = nDirectoryNameLen + Q_strlen(pFileName) + 2;
			pFileNameWithPath = (char *)stackalloc( nAllocSize );
			Q_snprintf(	pFileNameWithPath, nAllocSize, "%s/%s", &pDirectoryName[SOUND_PREFIX_LEN], pFileName ); 
			Q_strnlwr( pFileNameWithPath, nAllocSize );

			CWaveFile *wav = new CWaveFile( NULL, NULL, pFileNameWithPath );
			soundlist.Insert( pFileNameWithPath, wav );

			/*
			if ( !(soundlist.Count() % 500 ) )
			{
				Con_Printf( "CWaveBrowser:  loaded %i sounds\n", soundlist.Count() );
			}
			*/
		}
		pFileName = filesystem->FindNext( findHandle );
	}

	m_pFileTree->FindOrAddSubdirectory( &pDirectoryName[ SOUND_PREFIX_LEN ] );

	filesystem->FindClose( findHandle );
	return true;
}

bool CWaveBrowser::InitDirectoryRecursive( CUtlDict< CWaveFile *, int >& soundlist, char const* pDirectoryName )
{
	// Compute directory name length
	int nDirectoryNameLen = Q_strlen( pDirectoryName );

	if (!LoadWaveFilesInDirectory( soundlist, pDirectoryName, nDirectoryNameLen ) )
		return false;

	char *pWildCard = ( char * )stackalloc( nDirectoryNameLen + 4 );
	strcpy(pWildCard, pDirectoryName);
	strcat(pWildCard, "/*.");
	int nPathStrLen = nDirectoryNameLen + 1;

	FileFindHandle_t findHandle;
	const char *pFileName = filesystem->FindFirst( pWildCard, &findHandle );
	while( pFileName )
	{
		if ((pFileName[0] != '.') || (pFileName[1] != '.' && pFileName[1] != 0))
		{
			if( filesystem->FindIsDirectory( findHandle ) )
			{
				int fileNameStrLen = Q_strlen( pFileName );
				char *pFileNameWithPath = ( char * )stackalloc( nPathStrLen + fileNameStrLen + 1 );
				memcpy( pFileNameWithPath, pWildCard, nPathStrLen );
				pFileNameWithPath[nPathStrLen] = '\0';
				strcat( pFileNameWithPath, pFileName );

				if (!InitDirectoryRecursive( soundlist, pFileNameWithPath ))
					return false;
			}
		}
		pFileName = filesystem->FindNext( findHandle );
	}

	return true;
}

void CWaveBrowser::LoadAllSounds()
{
	RemoveAllSounds();

	InitDirectoryRecursive( m_AllSounds, "sound" );
	// InitDirectoryRecursive( m_AllSounds, "sound/npc" );

	int c = m_AllSounds.Count();
	CUtlVector< CWaveFile * > list;
	for ( int i = 0; i < c; i++ )
	{
		CWaveFile *wav = m_AllSounds[ i ];
		list.AddToTail( wav );
	}


	fileloader->AddWaveFilesToThread( list );


	m_pFileTree->PopulateTree();
}

void CWaveBrowser::RemoveAllSounds()
{
	int c = m_AllSounds.Count();
	for ( int i = 0; i < c; i++ )
	{
		CWaveFile *wav = m_AllSounds[ i ];
		delete wav;
	}

	m_AllSounds.RemoveAll();
	m_Scripts.RemoveAll();
	m_CurrentSelection.RemoveAll();

	m_pFileTree->Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWaveBrowser::PopulateTree( char const *subdirectory, bool textsearch /*= false*/ )
{
	int i;

	CUtlRBTree< CWaveFile *, int >		m_Sorted( 0, 0, NameLessFunc );
	
	bool check_load_sentence_data = false;

	char const *texttofind = NULL;

	if ( textsearch )
	{
		subdirectory = NULL;
		texttofind = GetSearchString();
	}

	int len = 0;
	if ( subdirectory )
	{
		len = Q_strlen( subdirectory );
		check_load_sentence_data = Q_strstr( subdirectory, "/" ) ? true : false;
	}

	int c = m_AllSounds.Count();
	for ( i = 0; i < c; i++ )
	{
		CWaveFile *wav = m_AllSounds[ i ];
		char const *name = wav->GetName();

		if ( subdirectory )
		{
			if ( Q_strnicmp( subdirectory, wav->GetName(), len ) )
				continue;
		}

		if ( textsearch && texttofind )
		{
			if ( !Q_stristr( name, texttofind ) )
				continue;
		}

		m_Sorted.Insert( wav );
	}

	char prevSelectedName[ 512 ];
	prevSelectedName[ 0 ] = 0;
	if ( m_pListView->getNumSelected() == 1 )
	{
		int selectedItem = m_pListView->getNextSelectedItem( 0 );
		if ( selectedItem >= 0 )
		{
			// Grab wave name of previously selected item
			Q_strcpy( prevSelectedName, m_pListView->getLabel( selectedItem, 0 ) );
		}
	}

// Repopulate tree
	m_pListView->removeAll();

	int loadcount = 0;

	m_pListView->setDrawingEnabled( false );

	int selectedSlot = -1;

	for ( i = m_Sorted.FirstInorder(); i != m_Sorted.InvalidIndex(); i = m_Sorted.NextInorder( i ) )
	{
		CWaveFile *wav = m_Sorted[ i ];
		char const *name = wav->GetName();

		int slot = m_pListView->add( name );

		if ( !Q_stricmp( prevSelectedName, name ) )
		{
			selectedSlot = slot;
		}

		m_pListView->setImage( slot, COL_WAV, wav->GetIconIndex() );
		m_pListView->setUserData( slot, COL_WAV, (void *)wav );

		if ( wav->HasLoadedSentenceInfo() )
		{
			m_pListView->setLabel( slot, COL_DUCKED, wav->GetVoiceDuck() ? "yes" : "no" );
			m_pListView->setLabel( slot, COL_SENTENCE, wav->GetSentenceText() );
		}
		else
		{
			m_pListView->setLabel( slot, COL_SENTENCE, "(loading...)" );
		}

		++loadcount;
	}

	m_pListView->setDrawingEnabled( true );

	if ( selectedSlot != -1 )
	{
		m_pListView->setSelected( selectedSlot, true );
		m_pListView->scrollToItem( selectedSlot );
	}

	// Con_Printf( "CWaveBrowser:  selected %i sounds\n", loadcount );
}

CWorkspaceManager *CWaveBrowser::GetManager()
{
	return m_pManager;
}

void CWaveBrowser::RepopulateTree()
{
	PopulateTree( m_pFileTree->GetSelectedPath() );
}

void CWaveBrowser::BuildSelectionList( CUtlVector< CWaveFile * >& selected )
{
	selected.RemoveAll();

	int idx = -1;
	do 
	{
		idx = m_pListView->getNextSelectedItem( idx );
		if ( idx != -1 )
		{
			CWaveFile *wav = (CWaveFile *)m_pListView->getUserData( idx, 0 );
			if ( wav )
			{
				selected.AddToTail( wav );
			}
		}
	} while ( idx != -1 );
	
}

void CWaveBrowser::ShowContextMenu( void )
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
		pop->add( "Properties...", IDC_SB_PROPERTIES );
		pop->addSeparator();
	}
	else
	{
		pop->add( "Enable Voice Ducking", IDC_SB_ENABLEVOICEDUCKING ); 
		pop->add( "Disable Voice Ducking", IDC_SB_DISABLEVOICEDUCKING ); 
		pop->addSeparator();
	}

	pop->add( "Refresh sentence data", IDC_SB_GETSENTENCE );
	pop->add( "Import Sentence Data", IDC_SB_IMPORTSENTENCE );
	pop->add( "Export Sentence Data", IDC_SB_EXPORTSENTENCE );

	pop->addSeparator();

	pop->add( "Check out", IDC_SB_CHECKOUT );
	pop->add( "Check in", IDC_SB_CHECKIN );

	pop->popup( this, pt.x, pt.y );
}

void CWaveBrowser::OnPlay()
{
	BuildSelectionList( m_CurrentSelection );
	if ( m_CurrentSelection.Count() == 1 )
	{
		CWaveFile *wav = m_CurrentSelection[ 0 ];
		if ( wav )
		{
			wav->Play();
		}
	}
}

void CWaveBrowser::OnCheckout()
{
	BuildSelectionList( m_CurrentSelection );
	int c = m_CurrentSelection.Count();
	for ( int i = 0; i < c; i++ )
	{
		CWaveFile *wav = m_CurrentSelection[ i ];
		Assert( wav );

		wav->Checkout( false );
	}

	GetWorkspaceManager()->RefreshBrowsers();
}

void CWaveBrowser::OnCheckin()
{
	BuildSelectionList( m_CurrentSelection );
	int c = m_CurrentSelection.Count();
	for ( int i = 0; i < c; i++ )
	{
		CWaveFile *wav = m_CurrentSelection[ i ];
		Assert( wav );

		wav->Checkin( false );
	}

	GetWorkspaceManager()->RefreshBrowsers();
}

void SplitFileName( char const *in, char *path, int maxpath, char *filename, int maxfilename );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *se - 
//-----------------------------------------------------------------------------
void CWaveBrowser::JumpToItem( CWaveFile *wav )
{

	char path[ 256 ];
	char filename[ 256 ];

	SplitFileName( wav->GetFileName(), path, sizeof( path ), filename, sizeof( filename ) );

	char *usepath = path + Q_strlen( "/sound/" );
	PopulateTree( usepath );

	int idx = 0;
	int c = m_pListView->getItemCount();
	for ( ; idx < c; idx++ )
	{
		CWaveFile *item = (CWaveFile *)m_pListView->getUserData( idx, 0 );
		if ( !Q_stricmp( item->GetFileName(), wav->GetFileName() ) )
		{
			break;
		}
	}

	if ( idx < c )
	{
		m_pListView->scrollToItem( idx );
	}
}

CWaveFile	*CWaveBrowser::FindEntry( char const *wavname, bool jump /*= false*/ )
{
	int idx = m_AllSounds.Find( PSkipSoundChars( wavname ) );
	if ( idx != m_AllSounds.InvalidIndex() )
	{
		CWaveFile *wav = m_AllSounds[ idx ];
#if defined( _DEBUG )
		char const *name = wav->GetName();
		NOTE_UNUSED( name );
#endif

		if ( jump )
		{
			JumpToItem( wav );
		}

		return wav;
	}

	return NULL;
}

void CWaveBrowser::OnWaveProperties()
{
	BuildSelectionList( m_CurrentSelection );
	if ( m_CurrentSelection.Count() != 1 )
	{
		Con_Printf( "Can only apply properties to one item at a time (FOR NOW)\n" );
		return;
	}

	CWaveFile *entry = m_CurrentSelection[ 0 ];
	if ( !entry )
		return;

	CWaveParams params;
	memset( &params, 0, sizeof( params ) );

	Q_snprintf( params.m_szDialogTitle, sizeof( params.m_szDialogTitle ), "Wave Properties" );

	params.items.AddToTail( entry );

	if ( !WaveProperties( &params ) )
		return;
}


int	 CWaveBrowser::GetSoundCount() const
{
	return m_AllSounds.Count();
}

CWaveFile *CWaveBrowser::GetSound( int index )
{
	if ( index < 0 || index >= m_AllSounds.Count() )
		return NULL;

	return m_AllSounds[ index ];
}


void CWaveBrowser::OnSearch()
{
	PopulateTree( GetSearchString(), true );
}

char const *CWaveBrowser::GetSearchString()
{
	return m_pOptions->GetSearchString();
}


void CWaveBrowser::OnEnableVoiceDucking()
{
	BuildSelectionList( m_CurrentSelection );
	int count = m_CurrentSelection.Count();
	if ( count < 1 )
		return;

	MultipleRequestChangeContext();

	for ( int i = 0; i < count; i++ )
	{
		CWaveFile *item = m_CurrentSelection[ i ];
		if ( !item->GetVoiceDuck() )
		{
			item->SetVoiceDuck( true );
		}
	}

	GetWorkspaceManager()->RefreshBrowsers();
}

void CWaveBrowser::OnDisableVoiceDucking()
{
	BuildSelectionList( m_CurrentSelection );
	int count = m_CurrentSelection.Count();
	if ( count < 1 )
		return;

	MultipleRequestChangeContext();

	for ( int i = 0; i < count; i++ )
	{
		CWaveFile *item = m_CurrentSelection[ i ];
		if ( item->GetVoiceDuck() )
		{
			item->SetVoiceDuck( false );
		}
	}

	GetWorkspaceManager()->RefreshBrowsers();
}

void CWaveBrowser::OnImportSentence()
{
	BuildSelectionList( m_CurrentSelection );
	int count = m_CurrentSelection.Count();
	if ( count < 1 )
		return;

	MultipleRequestChangeContext();

	for ( int i = 0; i < count; i++ )
	{
		CWaveFile *item = m_CurrentSelection[ i ];

		char relative[ 512 ];
		item->GetPhonemeExportFile( relative, sizeof( relative ) );
		if ( filesystem->FileExists( relative ) )
		{
			item->ImportValveDataChunk( relative );
		}
	}

	GetWorkspaceManager()->RefreshBrowsers();
}

void CWaveBrowser::OnExportSentence()
{
	BuildSelectionList( m_CurrentSelection );
	int count = m_CurrentSelection.Count();
	if ( count < 1 )
		return;

	for ( int i = 0; i < count; i++ )
	{
		CWaveFile *item = m_CurrentSelection[ i ];

		char relative[ 512 ];
		item->GetPhonemeExportFile( relative, sizeof( relative ) );
		if ( filesystem->FileExists( relative ) )
		{
			filesystem->RemoveFile( relative );
		}

		item->ExportValveDataChunk( relative );
	}
}


void CWaveBrowser::OnGetSentence()
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
		CWaveFile *wav = m_CurrentSelection[ i ];
		if ( !wav->HasLoadedSentenceInfo() )
		{
			wav->EnsureSentence();
		}
	}

	// Repopulate things
	GetWorkspaceManager()->RefreshBrowsers();
}