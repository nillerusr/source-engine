//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include <windows.h>
#include "resource.h"
#include "wavefile.h"
#include "wavebrowser.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "ifaceposersound.h"
#include "snd_wave_source.h"
#include "filesystem.h"
#include "tabwindow.h"
#include "inputproperties.h"
#include "choreowidgetdrawhelper.h"
#include "ifileloader.h"
#include "tier2/riff.h"
#include "UtlBuffer.h"
#include "ChoreoEvent.h"

CWaveBrowser	*g_pWaveBrowser = NULL;

//-----------------------------------------------------------------------------
// Purpose: Implements the RIFF i/o interface on stdio
//-----------------------------------------------------------------------------
class StdIOReadBinary : public IFileReadBinary
{
public:
	int open( const char *pFileName )
	{
		return (int)filesystem->Open( pFileName, "rb" );
	}

	int read( void *pOutput, int size, int file )
	{
		if ( !file )
			return 0;

		return filesystem->Read( pOutput, size, (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		if ( !file )
			return;

		filesystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		if ( !file )
			return 0;

		return filesystem->Tell( (FileHandle_t)file );
	}

	unsigned int size( int file )
	{
		if ( !file )
			return 0;

		return filesystem->Size( (FileHandle_t)file );
	}

	void close( int file )
	{
		if ( !file )
			return;

		filesystem->Close( (FileHandle_t)file );
	}
};

class StdIOWriteBinary : public IFileWriteBinary
{
public:
	int create( const char *pFileName )
	{
		return (int)filesystem->Open( pFileName, "wb" );
	}

	int write( void *pData, int size, int file )
	{
		return filesystem->Write( pData, size, (FileHandle_t)file );
	}

	void close( int file )
	{
		filesystem->Close( (FileHandle_t)file );
	}

	void seek( int file, int pos )
	{
		filesystem->Seek( (FileHandle_t)file, pos, FILESYSTEM_SEEK_HEAD );
	}

	unsigned int tell( int file )
	{
		return filesystem->Tell( (FileHandle_t)file );
	}
};

static StdIOReadBinary io_in;
static StdIOWriteBinary io_out;

#define RIFF_WAVE			MAKEID('W','A','V','E')
#define WAVE_FMT			MAKEID('f','m','t',' ')
#define WAVE_DATA			MAKEID('d','a','t','a')
#define WAVE_FACT			MAKEID('f','a','c','t')
#define WAVE_CUE			MAKEID('c','u','e',' ')

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &walk - 
//-----------------------------------------------------------------------------
static void SceneManager_ParseSentence( CSentence& sentence, IterateRIFF &walk )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	buf.EnsureCapacity( walk.ChunkSize() );
	walk.ChunkRead( buf.Base() );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, walk.ChunkSize() );

	sentence.InitFromDataChunk( buf.Base(), buf.TellPut() );
}

bool SceneManager_LoadSentenceFromWavFileUsingIO( char const *wavfile, CSentence& sentence, IFileReadBinary& io )
{
	sentence.Reset();

	InFileRIFF riff( wavfile, io );

	// UNDONE: Don't use printf to handle errors
	if ( riff.RIFFName() != RIFF_WAVE )
	{
		return false;
	}

	// set up the iterator for the whole file (root RIFF is a chunk)
	IterateRIFF walk( riff, riff.RIFFSize() );

	// This chunk must be first as it contains the wave's format
	// break out when we've parsed it
	bool found = false;
	while ( walk.ChunkAvailable() && !found )
	{
		switch( walk.ChunkName() )
		{
		case WAVE_VALVEDATA:
			{
				found = true;
				SceneManager_ParseSentence( sentence, walk );
			}
			break;
		}
		walk.ChunkNext();
	}

	return true;
}

bool SceneManager_LoadSentenceFromWavFile( char const *wavfile, CSentence& sentence )
{
	return SceneManager_LoadSentenceFromWavFileUsingIO( wavfile, sentence, io_in );
}

enum
{
	// Controls
	IDC_SB_LISTVIEW = 101,
	IDC_SB_FILETREE,

	// Messages
	IDC_SB_PLAY = 1000,
};

enum
{
	COL_WAV = 0,
	COL_DUCKED,
	COL_PHONEMES,
	COL_SENTENCE
};

class CWaveList : public mxListView
{
public:
	CWaveList( mxWindow *parent, int id = 0 ) 
		: mxListView( parent, 0, 0, 0, 0, id )
	{
		// Add column headers
		insertTextColumn( COL_WAV, 300, "WAV" );
		insertTextColumn( COL_DUCKED, 50, "Ducked" );
		insertTextColumn( COL_PHONEMES, 120, "Words [ Phonemes ]" );
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
		unsigned int id = (unsigned int)getUserData( tvi );

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
#pragma optimize( "", off )
class CWaveOptionsWindow : public mxWindow
{
typedef mxWindow BaseClass;
public:
	enum
	{
		IDC_PLAYSOUND = 1000,
		IDC_STOP_SOUNDS,
		IDC_SEARCH,
		IDC_CANCELSEARCH,
	};

	CWaveOptionsWindow( CWaveBrowser *browser ) : BaseClass( browser, 0, 0, 0, 0 ), m_pBrowser( browser )
	{
		FacePoser_AddWindowStyle( this, WS_CLIPSIBLINGS | WS_CLIPCHILDREN );

		m_szSearchString[0]=0;

		m_pPlay = new mxButton( this, 0, 0, 0, 0, "Play", IDC_PLAYSOUND );
		
		m_pStopSounds = new mxButton( this, 0, 0, 0, 0, "Stop Sounds", IDC_STOP_SOUNDS );
		
		m_pSearch = new mxLineEdit( this, 0, 0, 0, 0, "", IDC_SEARCH );

		m_pCancelSearch = new mxButton( this, 0, 0, 0, 0, "Cancel", IDC_CANCELSEARCH );		
	}
	
	bool PaintBackground( void )
	{
		redraw();
		return false;
	}
	
	
	virtual void redraw()
	{
		CChoreoWidgetDrawHelper drawHelper( this, GetSysColor( COLOR_BTNFACE ) );
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

				m_pCancelSearch->setBounds( x, 1, split, h2() - 2 );
				
				x += split + 10;
				
				m_pSearch->setBounds( x, 0, split * 3, h2() - 1 );
				
				x += split * 3 + 10;
			}
			break;
		case mxEvent::KeyDown:
			switch ( event->action )
			{
			default:
				break;
			case IDC_SEARCH:
				{
					if ( event->event == mxEvent::KeyDown )
					{
						OnSearch();
					}
					iret = 1;
				};
				break;
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
				case IDC_SEARCH:
					iret = 1;
					break;
				case IDC_PLAYSOUND:
					{
						iret = 1;
						m_pBrowser->OnPlay();
					}
					break;
				case IDC_CANCELSEARCH:
					{
						iret = 1;
						OnCancelSearch();
					}
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
		m_pSearch->getText( m_szSearchString, sizeof( m_szSearchString ) );

		m_pBrowser->OnSearch();
	}

	void OnCancelSearch()
	{
		m_szSearchString[ 0 ] = 0;
		m_pSearch->clear();

		m_pBrowser->OnCancelSearch();
	}

private:
	
	mxButton		*m_pStopSounds;
	mxButton		*m_pPlay;
	mxLineEdit		*m_pSearch;
	mxButton		*m_pCancelSearch;
	
	CWaveBrowser	*m_pBrowser;

	char			m_szSearchString[ 256 ];
};

#pragma optimize( "", on )
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CWaveBrowser::CWaveBrowser( mxWindow *parent )
	: IFacePoserToolWindow( "WaveBrowser", "Waves" ), mxWindow( parent, 0, 0, 0, 0 )
{
	SetAutoProcess( false );

	m_bTextSearch = false;
	m_nPrevProcessed = -1;

	m_pListView = new CWaveList( this, IDC_SB_LISTVIEW );
	m_pOptions = new CWaveOptionsWindow( this );
	m_pFileTree = new CWaveFileTree( this, IDC_SB_FILETREE );

	//HIMAGELIST list = CreateImageList();

	// Associate the image list with the tree-view control. 
    //m_pListView->setImageList( (void *)list ); 

	LoadAllSounds();

	PopulateTree( NULL );
}

#define CX_ICON  16
#define CY_ICON  16 

HIMAGELIST CWaveBrowser::CreateImageList()
{
	HIMAGELIST list;
	
	list = ImageList_Create( CX_ICON, CY_ICON, 
		FALSE, NUM_IMAGES, 0 );

    // Load the icon resources, and add the icons to the image list. 
    HICON hicon;
	int slot;
#if defined( DBGFLAG_ASSERT )
	int c = 0;
#endif

	/*
	hicon = LoadIcon( GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_WORKSPACE)); 
	slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon( GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_WORKSPACE_CHECKEDOUT)); 
	slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_PROJECT)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_PROJECT_CHECKEDOUT)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_SCENE)); 
	slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );
	*/

//	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_SCENE_CHECKEDOUT)); 
//	slot = ImageList_AddIcon(list, hicon); 
//	Assert( slot == c++ );
//	DeleteObject( hicon );

	/*
	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_VCD)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_VCD_CHECKEDOUT )); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );
	*/

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_WAV)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	/*
	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_WAV_CHECKEDOUT)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_SPEAK)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_SPEAK_CHECKEDOUT)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );
	*/

	return list;
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

	if ( HandleToolEvent( event ) )
	{
		return iret;
	}

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
					SetActiveTool( this );

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
					SetActiveTool( this );

					PopulateTree( m_pFileTree->GetSelectedPath() );
				}
				break;
			case IDC_SB_PLAY:
				{
					OnPlay();
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

			CWaveFile *wav = new CWaveFile( pFileNameWithPath );
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

	Con_Printf( "Building list of all .wavs in sound/ folder\n" );

	InitDirectoryRecursive( m_AllSounds, "sound" );

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
void CWaveBrowser::PopulateTree( char const *subdirectory )
{
	int i;

	CUtlRBTree< CWaveFile *, int >		m_Sorted( 0, 0, NameLessFunc );
	
	bool check_load_sentence_data = false;

	char const *texttofind = NULL;

	if ( m_bTextSearch )
	{
		subdirectory = NULL;
		texttofind = GetSearchString();
	}

	int len = 0;
	if ( subdirectory )
	{
		len = Q_strlen( subdirectory );
		check_load_sentence_data = ( Q_strstr( subdirectory, "/" ) || subdirectory[0] ) ? true : false;
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

		if ( m_bTextSearch && texttofind )
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

	CUtlVector< CWaveFile * > list;


	for ( i = m_Sorted.FirstInorder(); i != m_Sorted.InvalidIndex(); i = m_Sorted.NextInorder( i ) )
	{
		CWaveFile *wav = m_Sorted[ i ];
		char const *name = wav->GetName();

		int slot = m_pListView->add( name );

		if ( !Q_stricmp( prevSelectedName, name ) )
		{
			selectedSlot = slot;
		}

		if ( ( check_load_sentence_data || m_bTextSearch ) && 
			!wav->HasLoadedSentenceInfo() && !wav->IsAsyncLoading() )
		{
			wav->SetAsyncLoading( true );
			list.AddToTail( wav );
		}

		// m_pListView->setImage( slot, COL_WAV, wav->GetIconIndex() );
		m_pListView->setUserData( slot, COL_WAV, (void *)wav );

		if ( wav->HasLoadedSentenceInfo() )
		{
			m_pListView->setLabel( slot, COL_DUCKED, wav->GetVoiceDuck() ? "yes" : "no" );
			m_pListView->setLabel( slot, COL_PHONEMES, wav->GetPhonemeCount() || wav->GetWordCount() ? va( "%i [ %i ]", wav->GetWordCount(), wav->GetPhonemeCount() ) : "" );
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

	if ( list.Count() > 0 )
	{
		fileloader->AddWaveFilesToThread( list );
	}

	// Con_Printf( "CWaveBrowser:  selected %i sounds\n", loadcount );
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
	SetActiveTool( this );

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
//		pop->addSeparator();
	}

//	pop->add( "Import Sentence Data", IDC_SB_IMPORTSENTENCE );
//	pop->add( "Export Sentence Data", IDC_SB_EXPORTSENTENCE );

	pop->popup( this, pt.x, pt.y );
}

void CWaveBrowser::OnPlay()
{
	SetActiveTool( this );

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

static void SplitFileName( char const *in, char *path, int maxpath, char *filename, int maxfilename )
{
   char drive[_MAX_DRIVE];
   char dir[_MAX_DIR];
   char fname[_MAX_FNAME];
   char ext[_MAX_EXT];

   _splitpath( in, drive, dir, fname, ext );

   if ( dir[0] )
   {
		Q_snprintf( path, maxpath, "\\%s", dir );
   }
   else
   {
	   path[0] = 0;
   }
   Q_snprintf( filename, maxfilename, "%s%s", fname, ext );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *se - 
//-----------------------------------------------------------------------------
void CWaveBrowser::JumpToItem( CWaveFile *wav )
{

	SetActiveTool( this );

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
	int idx = m_AllSounds.Find( wavname );
	if ( idx != m_AllSounds.InvalidIndex() )
	{
		CWaveFile *wav = m_AllSounds[ idx ];
		if ( jump )
		{
			JumpToItem( wav );
		}

		return wav;
	}

	return NULL;
}

int	 CWaveBrowser::GetSoundCount() const
{
	return m_AllSounds.Count();
}

CWaveFile *CWaveBrowser::GetSound( int index )
{
	if ( index < 0 || index >= (int)m_AllSounds.Count() )
		return NULL;

	return m_AllSounds[ index ];
}


void CWaveBrowser::OnSearch()
{
	SetActiveTool( this );

	m_bTextSearch = true;

	PopulateTree( GetSearchString());
}

void CWaveBrowser::OnCancelSearch()
{
	SetActiveTool( this );

	m_bTextSearch = false;

	PopulateTree( m_pFileTree->GetSelectedPath() ); 
}

char const *CWaveBrowser::GetSearchString()
{
	return m_pOptions->GetSearchString();
}

void CWaveBrowser::Think( float dt )
{
	int pending = fileloader->GetPendingLoadCount();
	if ( pending != m_nPrevProcessed )
	{
		m_nPrevProcessed = pending;

		// Put into suffix of window title
		if ( pending == 0 )
		{
			SetSuffix( "" );
		}
		else
		{
			SetSuffix( va( " - %i", pending ) );
		}
	}

	int c = fileloader->ProcessCompleted();
	if ( c > 0 )
	{
		RepopulateTree();
	}
}

void CWaveBrowser::SetEvent( CChoreoEvent *event )
{
	if ( event->GetType() != CChoreoEvent::SPEAK )
		return;

	SetCurrent( FacePoser_TranslateSoundName( event->GetParameters() ) );
}

void CWaveBrowser::SetCurrent( char const *filename )
{
// Get sound name and look up .wav from it
	char const *p = filename;
	if ( p && 
		( !Q_strnicmp( p, "sound/", 6 ) || !Q_strnicmp( p, "sound\\", 6 ) ) )
	{
		p += 6;
	}

	char fn[ 512 ];
	Q_strncpy( fn, p, sizeof( fn ) );
	Q_FixSlashes( fn );

	int i;
	int c = m_pListView->getItemCount();

	for ( i = 0; i < c; ++i )
	{
		CWaveFile *wav = reinterpret_cast< CWaveFile * >( m_pListView->getUserData( i, COL_WAV ) );
		if ( !wav )
			continue;

		char fixed[ 512 ];
		Q_strncpy( fixed, wav->GetName(), sizeof( fixed ) );
		Q_FixSlashes( fixed );

		if ( !Q_stricmp( fixed, fn ) )
		{
			m_pListView->scrollToItem( i );
			m_pListView->setSelected( i, true );
		}
		else
		{
			m_pListView->setSelected( i, false );
		}
	}
}
