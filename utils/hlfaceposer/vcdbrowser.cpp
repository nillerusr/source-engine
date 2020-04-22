//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <windows.h>
#include "resource.h"
#include "vcdbrowser.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "filesystem.h"
#include "tabwindow.h"
#include "inputproperties.h"
#include "choreowidgetdrawhelper.h"
#include "UtlBuffer.h"
#include "ChoreoEvent.h"
#include "ChoreoView.h"

CVCDBrowser	*g_pVCDBrowser = NULL;

enum
{
	// Controls
	IDC_VB_LISTVIEW = 101,
	IDC_VB_FILETREE,
	// Messages
	IDC_VB_OPENVCD = 1000,
};

enum
{
	COL_VCD = 0,
};

class CVCDList : public mxListView
{
public:
	CVCDList( mxWindow *parent, int id = 0 ) 
		: mxListView( parent, 0, 0, 0, 0, id )
	{
		// Add column headers
		insertTextColumn( COL_VCD, 700, "VCD" );
	}
};

class CUtlSymbolTree : public mxTreeView
{
public:
	CUtlSymbolTree( mxWindow *parent, int id = 0 ) : mxTreeView( parent, 0, 0, 0, 0, id ),
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
class CVCDOptionsWindow : public mxWindow
{
typedef mxWindow BaseClass;
public:
	enum
	{
		IDC_OPENFILE = 1000,
		IDC_SEARCH,
		IDC_CANCELSEARCH,
	};

	CVCDOptionsWindow( CVCDBrowser *browser ) : BaseClass( browser, 0, 0, 0, 0 ), m_pBrowser( browser )
	{
		FacePoser_AddWindowStyle( this, WS_CLIPSIBLINGS | WS_CLIPCHILDREN );

		m_szSearchString[0]=0;

		m_pOpen = new mxButton( this, 0, 0, 0, 0, "Open", IDC_OPENFILE );
		
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
				
				m_pOpen->setBounds( x, 1, split, h2() - 2 );
				
			
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
				case IDC_SEARCH:
					iret = 1;
					break;
				case IDC_OPENFILE:
					{
						iret = 1;
						m_pBrowser->OnOpen();
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
	
	mxButton		*m_pOpen;
	mxLineEdit		*m_pSearch;
	mxButton		*m_pCancelSearch;
	
	CVCDBrowser		*m_pBrowser;

	char			m_szSearchString[ 256 ];
};

#pragma optimize( "", on )
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CVCDBrowser::CVCDBrowser( mxWindow *parent )
	: IFacePoserToolWindow( "VCDBrowser", "VCDs" ), mxWindow( parent, 0, 0, 0, 0 )
{
	SetAutoProcess( false );

	m_bTextSearch = false;
	m_nPrevProcessed = -1;

	m_pListView = new CVCDList( this, IDC_VB_LISTVIEW );
	m_pOptions = new CVCDOptionsWindow( this );
	m_pFileTree = new CUtlSymbolTree( this, IDC_VB_FILETREE );

	//HIMAGELIST list = CreateImageList();

	// Associate the image list with the tree-view control. 
    //m_pListView->setImageList( (void *)list ); 

	LoadAllSounds();

	PopulateTree( NULL );
}

#define CX_ICON  16
#define CY_ICON  16 

HIMAGELIST CVCDBrowser::CreateImageList()
{
	HIMAGELIST list;
	
	list = ImageList_Create( CX_ICON, CY_ICON, 
		FALSE, VCD_NUM_IMAGES, 0 );

    // Load the icon resources, and add the icons to the image list. 
    HICON hicon;
	int slot;
#if defined( DBGFLAG_ASSERT )
	int c = 0;
#endif

	hicon = LoadIcon(GetModuleHandle( 0 ), MAKEINTRESOURCE(IDI_VCD)); 
    slot = ImageList_AddIcon(list, hicon); 
	Assert( slot == c++ );
	DeleteObject( hicon );

	return list;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVCDBrowser::OnDelete()
{
	RemoveAllSounds();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *event - 
// Output : int
//-----------------------------------------------------------------------------
int CVCDBrowser::handleEvent( mxEvent *event )
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
			case IDC_VB_LISTVIEW:
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
								FileNameHandle_t vcd = (FileNameHandle_t)m_pListView->getUserData( index, 0 );
								OpenVCD( vcd );
							}
						}
					}
				}
				break;
			case IDC_VB_FILETREE:
				{
					SetActiveTool( this );

					PopulateTree( m_pFileTree->GetSelectedPath() );
				}
				break;
			case IDC_VB_OPENVCD:
				{
					OnOpen();
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

bool CVCDBrowser::CNameLessFunc::Less( const FileNameHandle_t &name1, const FileNameHandle_t &name2, void *pContext  )
{
	if ( name1 < name2 )
		return true;
	return false;
}

void CVCDBrowser::OpenVCD( const FileNameHandle_t& handle )
{
	char fn[ 512 ];
	if ( filesystem->String( handle, fn, sizeof( fn ) ) )
	{
		char pFullPath[MAX_PATH];
		const char *pFileName = filesystem->RelativePathToFullPath( fn, "GAME", pFullPath, sizeof(pFullPath) );
		if ( !pFileName )
		{
			pFileName = fn;
		}
		g_pChoreoView->LoadSceneFromFile( pFileName );
	}
}

#define SCENES_PREFIX_LEN	0
//-----------------------------------------------------------------------------
// Finds all .vcd files in a particular directory
//-----------------------------------------------------------------------------
bool CVCDBrowser::LoadVCDsFilesInDirectory( CUtlSortVector< FileNameHandle_t, CNameLessFunc >& soundlist, char const* pDirectoryName, int nDirectoryNameLen )
{
	char *pWildCard;
	pWildCard = ( char * )stackalloc( nDirectoryNameLen + 7 );
	Q_snprintf( pWildCard, nDirectoryNameLen + 7, "%s/*.vcd", pDirectoryName );

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
			Q_snprintf(	pFileNameWithPath, nAllocSize, "%s/%s", &pDirectoryName[ SCENES_PREFIX_LEN ], pFileName ); 
			Q_strnlwr( pFileNameWithPath, nAllocSize );

			FileNameHandle_t vcd;
			vcd = filesystem->FindOrAddFileName( pFileNameWithPath );
			soundlist.InsertNoSort( vcd );
		}
		pFileName = filesystem->FindNext( findHandle );
	}

	m_pFileTree->FindOrAddSubdirectory( &pDirectoryName[ SCENES_PREFIX_LEN ] );

	filesystem->FindClose( findHandle );
	return true;
}

bool CVCDBrowser::InitDirectoryRecursive( CUtlSortVector< FileNameHandle_t, CNameLessFunc >& soundlist, char const* pDirectoryName )
{
	// Compute directory name length
	int nDirectoryNameLen = Q_strlen( pDirectoryName );

	if (!LoadVCDsFilesInDirectory( soundlist, pDirectoryName, nDirectoryNameLen ) )
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

void CVCDBrowser::LoadAllSounds()
{
	RemoveAllSounds();

	Con_Printf( "Building list of all .vcds in sound/ folder\n" );

	InitDirectoryRecursive( m_AllVCDs, "scenes" );
	m_AllVCDs.RedoSort();

	m_pFileTree->PopulateTree();
}

void CVCDBrowser::RemoveAllSounds()
{
	m_AllVCDs.Purge();
	m_Scripts.RemoveAll();
	m_CurrentSelection.RemoveAll();

	m_pFileTree->Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVCDBrowser::PopulateTree( char const *subdirectory )
{
	char subdir[ 512 ];
	subdir[ 0 ] = 0;

	int i;

	CUtlSortVector< FileNameHandle_t, CNameLessFunc >	sorted( 0, 0 );
	
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
		Q_strncpy( subdir, subdirectory, sizeof( subdir ) );
		Q_strlower( subdir );
		Q_FixSlashes( subdir );
	}

	int c = m_AllVCDs.Count();
	for ( i = 0; i < c; i++ )
	{
		const FileNameHandle_t &vcd = m_AllVCDs[ i ];
		char name[ 512 ];
		if ( !filesystem->String( vcd, name, sizeof( name ) ) )
			continue;

		if ( subdirectory )
		{
			if ( Q_strnicmp( subdir, name, len ) )
				continue;
		}

		if ( m_bTextSearch && texttofind )
		{
			if ( !Q_stristr( name, texttofind ) )
				continue;
		}

		sorted.InsertNoSort( vcd );
	}

	sorted.RedoSort();

	char prevSelectedName[ 512 ];
	prevSelectedName[ 0 ] = 0;
	if ( m_pListView->getNumSelected() == 1 )
	{
		int selectedItem = m_pListView->getNextSelectedItem( 0 );
		if ( selectedItem >= 0 )
		{
			// Grab name of previously selected item
			Q_strcpy( prevSelectedName, m_pListView->getLabel( selectedItem, 0 ) );
		}
	}

// Repopulate tree
	m_pListView->removeAll();

	int loadcount = 0;

	m_pListView->setDrawingEnabled( false );

	int selectedSlot = -1;

	for ( i = 0; i < sorted.Count(); ++i )
	{
		const FileNameHandle_t &vcd = sorted[ i ];
		char name[ 512 ];
		if ( !filesystem->String( vcd, name, sizeof( name ) ) )
			continue;

		int slot = m_pListView->add( name );
		m_pListView->setUserData( slot, COL_VCD, (void *)vcd );

		if ( !Q_stricmp( prevSelectedName, name ) )
		{
			selectedSlot = slot;
		}
		++loadcount;
	}

	m_pListView->setDrawingEnabled( true );

	if ( selectedSlot != -1 )
	{
		m_pListView->setSelected( selectedSlot, true );
		m_pListView->scrollToItem( selectedSlot );
	}
}

void CVCDBrowser::RepopulateTree()
{
	PopulateTree( m_pFileTree->GetSelectedPath() );
}

void CVCDBrowser::BuildSelectionList( CUtlVector< FileNameHandle_t >& selected )
{
	selected.RemoveAll();

	int idx = -1;
	do 
	{
		idx = m_pListView->getNextSelectedItem( idx );
		if ( idx != -1 )
		{
			FileNameHandle_t vcd = (FileNameHandle_t)m_pListView->getUserData( idx, 0 );
			selected.AddToTail( vcd );
		}
	} while ( idx != -1 );
	
}

void CVCDBrowser::ShowContextMenu( void )
{
	SetActiveTool( this );

	BuildSelectionList( m_CurrentSelection );
	if ( m_CurrentSelection.Count() <= 0 )
		return;

	POINT pt;
	GetCursorPos( &pt );
	ScreenToClient( (HWND)getHandle(), &pt );

	mxPopupMenu *pop = new mxPopupMenu();

	if ( m_CurrentSelection.Count() == 1 && m_CurrentSelection[ 0 ] )
	{
		char sz[ 512 ];
		char name[ 512 ];
		if ( filesystem->String( m_CurrentSelection[ 0 ], name, sizeof( name ) ) )
		{
			Q_snprintf( sz, sizeof( sz ), "&Open '%s'", name );
			pop->add ( sz, IDC_VB_OPENVCD );
		}
	}

	pop->popup( this, pt.x, pt.y );
}

void CVCDBrowser::OnOpen()
{
	SetActiveTool( this );

	BuildSelectionList( m_CurrentSelection );
	if ( m_CurrentSelection.Count() == 1 )
	{
		FileNameHandle_t& vcd = m_CurrentSelection[ 0 ];
		OpenVCD( vcd );
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
void CVCDBrowser::JumpToItem( const FileNameHandle_t& vcd )
{
	SetActiveTool( this );

	char path[ 256 ];
	char filename[ 256 ];

	char vcdfile[ 512 ];
	if ( !filesystem->String( vcd, vcdfile, sizeof( vcdfile ) ) )
		return;

	SplitFileName( vcdfile, path, sizeof( path ), filename, sizeof( filename ) );

	char *usepath = path + Q_strlen( "/scenes/" );
	PopulateTree( usepath );

	int idx = 0;
	int c = m_pListView->getItemCount();
	for ( ; idx < c; idx++ )
	{
		FileNameHandle_t item = (FileNameHandle_t)m_pListView->getUserData( idx, 0 );
		if ( item == vcd )
		{
			break;
		}
	}

	if ( idx < c )
	{
		m_pListView->scrollToItem( idx );
	}
}

int	 CVCDBrowser::GetVCDCount() const
{
	return m_AllVCDs.Count();
}

FileNameHandle_t CVCDBrowser::GetVCD( int index )
{
	if ( index < 0 || index >= (int)m_AllVCDs.Count() )
		return NULL;

	return m_AllVCDs[ index ];
}


void CVCDBrowser::OnSearch()
{
	if ( !GetSearchString()[ 0 ] )
	{
		OnCancelSearch();
		return;
	}
	
	SetActiveTool( this );
	m_bTextSearch = true;
	PopulateTree( GetSearchString());
}

void CVCDBrowser::OnCancelSearch()
{
	SetActiveTool( this );

	m_bTextSearch = false;

	PopulateTree( m_pFileTree->GetSelectedPath() ); 
}

char const *CVCDBrowser::GetSearchString()
{
	return m_pOptions->GetSearchString();
}

void CVCDBrowser::SetCurrent( char const *filename )
{
// Get sound name and look up .vcd from it
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
		FileNameHandle_t vcd = (FileNameHandle_t)( m_pListView->getUserData( i, COL_VCD ) );

		char fixed[ 512 ];
		if ( !filesystem->String( vcd, fixed, sizeof( fixed ) ) )
			continue;

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
