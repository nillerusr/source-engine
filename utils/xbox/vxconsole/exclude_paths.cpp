//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	EXCLUDE_PATHS.CPP
//
//	DVD Exclude paths.
//=====================================================================================//
#include "vxconsole.h"

#define UM_CHECKSTATECHANGE		(WM_USER + 100)
#define UM_FIRSTTIMEPOPULATE	(WM_USER + 101)

#define MAX_TREE_DEPTH		32	
#define ROOT_NAME			"Xbox:\\"
#define EXCLUDEPATHS_FILE	"xbox_exclude_paths.txt"

// known shipping 360 gamedirs
struct GameName_t
{
	const char *pName;
	bool		bIsModPath;
};

struct GamePath_t
{
	CUtlString	pathName;
	bool		bIsModPath;
};

GameName_t g_GameNames[] = 
{
	{ "bin",		false },
	{ "platform",	false },
	{ "tf",			true },
	{ "portal",		true },
	{ "hl2",		true },
	{ "episodic",	true },
	{ "ep2",		true }
};

CUtlVector< CUtlString >	g_ExcludePaths;
BOOL						g_bLinkGameDirs;

inline bool PathLessThan( GamePath_t const &lhs, GamePath_t const &rhs )
{
	if ( lhs.bIsModPath != rhs.bIsModPath )
	{
		// sort mod paths to the bottom
		return ( lhs.bIsModPath == false );
	}

	return ( stricmp( lhs.pathName.String(), rhs.pathName.String() ) < 0 ); 
}
CUtlRBTree< GamePath_t > g_PathTable( 0, 0, PathLessThan );

//-----------------------------------------------------------------------------
// Add string to TreeView at specified level.  Assumes an inorder insert.
// A node (at specified depth) must be inserted before it's children.
//-----------------------------------------------------------------------------
static HTREEITEM AddItemToTree( HWND hWndTree, LPSTR lpszItem, int nLevel, bool bIsModPath )
{ 
	TVITEM tvi = { 0 }; 
	TVINSERTSTRUCT tvins = { 0 }; 
	static HTREEITEM s_hPrevItems[MAX_TREE_DEPTH];

	if ( nLevel < 0 || nLevel >= MAX_TREE_DEPTH )
	{
		Assert( 0 );
		return NULL;
	}

	HTREEITEM hParent;
	HTREEITEM hPrev; 
	if ( !nLevel )
	{
		// one root item only, reset
		for ( int i = 0; i < ARRAYSIZE( s_hPrevItems ); i++ )
		{
			s_hPrevItems[i] = NULL;
		}
		hParent = TVI_ROOT;
		hPrev = (HTREEITEM)TVI_FIRST;
	}
	else
	{
		hParent = s_hPrevItems[nLevel-1];
		hPrev = s_hPrevItems[nLevel];
		if ( !hParent )
		{
			// parent should have been setup
			Assert( 0 );
			return NULL;
		}
		if ( !hPrev )
		{
			hPrev = TVI_FIRST;
		}
	}

	tvi.mask = TVIF_TEXT | TVIF_PARAM; 
	tvi.pszText = lpszItem; 
	tvi.cchTextMax = sizeof(tvi.pszText)/sizeof(tvi.pszText[0]); 
	tvi.lParam = (LPARAM)MAKELONG( nLevel, bIsModPath );

	tvins.item = tvi; 
	tvins.hParent = hParent;
	tvins.hInsertAfter = hPrev; 

	// Add the item to the tree-view control. 
	hPrev = TreeView_InsertItem( hWndTree, &tvins ); 
	s_hPrevItems[nLevel] = hPrev;

	if ( nLevel > 0 )
	{
		// set a back link to its parent
		tvi.mask = TVIF_HANDLE;
		tvi.hItem = TreeView_GetParent( hWndTree, hPrev ); 
		TreeView_SetItem( hWndTree, &tvi ); 
	} 

	return hPrev; 
} 

//-----------------------------------------------------------------------------
// Purpose: Get list of files from current path that match pattern
//-----------------------------------------------------------------------------
static int GetFileList( const char* pDirPath, const char* pPattern, CUtlVector< CUtlString > &fileList )
{
	char	sourcePath[MAX_PATH];
	char	fullPath[MAX_PATH];
	bool	bFindDirs;

	fileList.Purge();

	strcpy( sourcePath, pDirPath );
	int len = (int)strlen( sourcePath );
	if ( !len )
	{
		strcpy( sourcePath, ".\\" );
	}
	else if ( sourcePath[len-1] != '\\' )
	{
		sourcePath[len]   = '\\';
		sourcePath[len+1] = '\0';
	}

	strcpy( fullPath, sourcePath );
	if ( pPattern[0] == '\\' && pPattern[1] == '\0' )
	{
		// find directories only
		bFindDirs = true;
		strcat( fullPath, "*" );
	}
	else
	{
		// find files, use provided pattern
		bFindDirs = false;
		strcat( fullPath, pPattern );
	}

	struct _finddata_t findData;
	intptr_t h = _findfirst( fullPath, &findData );
	if ( h == -1 )
	{
		return 0;
	}

	do
	{
		// dos attribute complexities i.e. _A_NORMAL is 0
		if ( bFindDirs )
		{
			// skip non dirs
			if ( !( findData.attrib & _A_SUBDIR ) )
				continue;
		}
		else
		{
			// skip dirs
			if ( findData.attrib & _A_SUBDIR )
				continue;
		}

		if ( !stricmp( findData.name, "." ) )
			continue;

		if ( !stricmp( findData.name, ".." ) )
			continue;

		char fileName[MAX_PATH];
		strcpy( fileName, sourcePath );
		strcat( fileName, findData.name );

		int j = fileList.AddToTail();
		fileList[j].Set( fileName );
	}
	while ( !_findnext( h, &findData ) );

	_findclose( h );

	return fileList.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Recursively determine directory tree
//-----------------------------------------------------------------------------
static void RecurseFileTree_r( const char *pBasePath, const char *pDirPath, int depth, CUtlVector< CUtlString > &dirList, bool bIsModPath )
{
	if ( depth >= 2 )
	{
		// too much unecessary detail
		return;
	}

	// ignore path roots, only interested in subdirs
	const char *pSubName = pDirPath + strlen( pBasePath );
	if ( pSubName[0] )
	{
		GamePath_t gamePath;
		gamePath.pathName = pSubName;
		gamePath.bIsModPath = bIsModPath;
		
		int iIndex = g_PathTable.Find( gamePath );
		if ( iIndex == g_PathTable.InvalidIndex() )
		{
			g_PathTable.Insert( gamePath );
		}
	}

	// recurse from source directory, get directories only
	CUtlVector< CUtlString > fileList;
	int dirCount = GetFileList( pDirPath, "\\", fileList );
	if ( !dirCount )
	{
		// add directory name to search tree
		int j = dirList.AddToTail();
		dirList[j].Set( pDirPath );
		return;
	}

	for ( int i=0; i<dirCount; i++ )
	{
		// form new path name, recurse into
		RecurseFileTree_r( pBasePath, fileList[i].String(), depth+1, dirList, bIsModPath );
	}

	int j = dirList.AddToTail();
	dirList[j].Set( pDirPath );
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_SetCheckState_r
//
//	Propogate the check state to all children
//-----------------------------------------------------------------------------
void ExcludePathsDlg_SetCheckState_r( HWND hWndTree, HTREEITEM hTree, int depth, int checkState )
{
	if ( !hTree )
	{
		return;
	}

	TreeView_SetCheckState( hWndTree, hTree, ( checkState == 1 ) );

	TVITEM tvi = { 0 };
	tvi.mask = TVIF_HANDLE | TVIF_CHILDREN;
	tvi.hItem = hTree;
	if ( TreeView_GetItem( hWndTree, &tvi ) )
	{
		if ( tvi.cChildren )
		{
			HTREEITEM hChild = TreeView_GetChild( hWndTree, hTree );
			if ( hChild )
			{
				ExcludePathsDlg_SetCheckState_r( hWndTree, hChild, depth+1, checkState );
			}
		}
	}
	else
	{
		return;
	}

	if ( !depth )
	{
		// only iterate siblings of the parent's child
		return;
	}

	HTREEITEM hSibling = hTree;
	while ( 1 )
	{
		hSibling = TreeView_GetNextSibling( hWndTree, hSibling );
		if ( !hSibling )
		{
			return;
		}

		TreeView_SetCheckState( hWndTree, hSibling, ( checkState == 1 ) );

		tvi.hItem = hSibling;
		if ( TreeView_GetItem( hWndTree, &tvi ) )
		{
			if ( tvi.cChildren )
			{
				HTREEITEM hChild = TreeView_GetChild( hWndTree, hSibling );
				if ( hChild )
				{
					ExcludePathsDlg_SetCheckState_r( hWndTree, hChild, depth+1, checkState );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_SetCheckStateLinked_r
//
//	Propogate the check state to all "mod path" children that match the specified name.
//	A NULL name matches all.
//-----------------------------------------------------------------------------
void ExcludePathsDlg_SetCheckStateLinked_r( HWND hWndTree, HTREEITEM hTree, int depth, int checkState, const char *pName )
{
	if ( !hTree )
	{
		return;
	}

	char szNodeName[MAX_PATH];
	TVITEM tvi = { 0 };
	tvi.mask = TVIF_HANDLE | TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM;
	tvi.hItem = hTree;
	tvi.pszText = szNodeName;
	tvi.cchTextMax = sizeof( szNodeName );
	if ( TreeView_GetItem( hWndTree, &tvi ) )
	{
		bool bIsModPath = HIWORD( tvi.lParam ) != 0;
		if ( bIsModPath && ( !pName || !V_stricmp( szNodeName, pName ) ) )
		{
			TreeView_SetCheckState( hWndTree, hTree, ( checkState == 1 ) );
		}

		if ( tvi.cChildren )
		{
			HTREEITEM hChild = TreeView_GetChild( hWndTree, hTree );
			if ( hChild )
			{
				ExcludePathsDlg_SetCheckStateLinked_r( hWndTree, hChild, depth+1, checkState, pName );
			}
		}
	}
	else
	{
		return;
	}

	if ( !depth )
	{
		// only iterate siblings of the parent's child
		return;
	}

	HTREEITEM hSibling = hTree;
	while ( 1 )
	{
		hSibling = TreeView_GetNextSibling( hWndTree, hSibling );
		if ( !hSibling )
		{
			return;
		}

		tvi.hItem = hSibling;
		if ( TreeView_GetItem( hWndTree, &tvi ) )
		{
			bool bIsModPath = HIWORD( tvi.lParam ) != 0;
			if ( bIsModPath && ( !pName || !V_stricmp( szNodeName, pName ) ) )
			{
				TreeView_SetCheckState( hWndTree, hSibling, ( checkState == 1 ) );
			}

			if ( tvi.cChildren )
			{
				HTREEITEM hChild = TreeView_GetChild( hWndTree, hSibling );
				if ( hChild )
				{
					ExcludePathsDlg_SetCheckStateLinked_r( hWndTree, hChild, depth+1, checkState, pName );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_GetCheckState_r
//
//	Caller invokes at node. Returns with state set.
//	State 0: unchecked, 1: checked, -1:unknown.
//	Returns true if All children match, false otherwise.
//-----------------------------------------------------------------------------
bool ExcludePathsDlg_GetCheckState_r( HWND hWndTree, HTREEITEM hTree, int depth, int *pCheckState )
{
	int checkState;

	checkState = TreeView_GetCheckState( hWndTree, hTree );
	if ( *pCheckState == -1 )
	{
		*pCheckState = checkState;
	}
	else if ( *pCheckState != checkState )
	{
		// disparate state, no need to recurse
		return false;
	}

	TVITEM tvi = { 0 };
	tvi.mask = TVIF_HANDLE | TVIF_CHILDREN;
	tvi.hItem = hTree;
	if ( TreeView_GetItem( hWndTree, &tvi ) )
	{
		if ( tvi.cChildren )
		{
			HTREEITEM hChild = TreeView_GetChild( hWndTree, hTree );
			if ( hChild )
			{
				if ( !ExcludePathsDlg_GetCheckState_r( hWndTree, hChild, depth+1, pCheckState ) )
				{
					return false;
				}
			}
		}
	}
	else
	{
		return false;
	}

	if ( !depth )
	{
		// only iterate siblings of the parent's child
		return true;
	}

	HTREEITEM hSibling = hTree;
	while ( 1 )
	{
		hSibling = TreeView_GetNextSibling( hWndTree, hSibling );
		if ( !hSibling )
		{
			break;
		}

		checkState = TreeView_GetCheckState( hWndTree, hSibling );
		if ( *pCheckState != checkState )
		{
			// disparate state, no need to recurse
			return false;
		}

		tvi.hItem = hSibling;
		if ( TreeView_GetItem( hWndTree, &tvi ) )
		{
			if ( tvi.cChildren )
			{
				HTREEITEM hChild = TreeView_GetChild( hWndTree, hSibling );
				if ( hChild )
				{
					if ( !ExcludePathsDlg_GetCheckState_r( hWndTree, hChild, depth+1, pCheckState ) )
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_Expand_r
//
//	MaxDepth >= 0, Expand/Collapse up to specified depth.
//-----------------------------------------------------------------------------
void ExcludePathsDlg_Expand_r( HWND hWndTree, HTREEITEM hTree, int depth, int maxDepth, bool bExpand )
{
	if ( !hTree )
	{
		return;
	}

	if ( maxDepth >= 0 && depth >= maxDepth )
	{
		return;
	}

	int flags;
	if ( bExpand )
		flags = TVE_EXPAND;
	else
		flags = TVE_COLLAPSE;

	TVITEM tvi = { 0 };
	tvi.mask = TVIF_HANDLE | TVIF_CHILDREN;
	tvi.hItem = hTree;
	if ( TreeView_GetItem( hWndTree, &tvi ) )
	{
		if ( tvi.cChildren )
		{
			TreeView_Expand( hWndTree, hTree, flags );
			HTREEITEM hChild = TreeView_GetChild( hWndTree, hTree );
			if ( hChild )
			{
				ExcludePathsDlg_Expand_r( hWndTree, hChild, depth+1, maxDepth, bExpand );
			}
		}
	}
	else
	{
		return;
	}

	if ( !depth )
	{
		// only iterate siblings of the parent's child
		return;
	}

	HTREEITEM hSibling = hTree;
	while ( 1 )
	{
		hSibling = TreeView_GetNextSibling( hWndTree, hSibling );
		if ( !hSibling )
		{
			return;
		}
		
		tvi.hItem = hSibling;
		if ( TreeView_GetItem( hWndTree, &tvi ) )
		{
			if ( tvi.cChildren )
			{
				TreeView_Expand( hWndTree, hSibling, flags );
				HTREEITEM hChild = TreeView_GetChild( hWndTree, hSibling );
				if ( hChild )
				{
					ExcludePathsDlg_Expand_r( hWndTree, hChild, depth+1, maxDepth, bExpand );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_ExpandToShowState_r
//
//-----------------------------------------------------------------------------
void ExcludePathsDlg_ExpandToShowState_r( HWND hWndTree, HTREEITEM hTree, int depth )
{
	if ( !hTree )
	{
		return;
	}

	TVITEM tvi = { 0 };
	tvi.mask = TVIF_HANDLE | TVIF_CHILDREN;
	tvi.hItem = hTree;
	if ( TreeView_GetItem( hWndTree, &tvi ) )
	{
		int checkState = -1;
		bool bStateIsSame = ExcludePathsDlg_GetCheckState_r( hWndTree, hTree, 0, &checkState );

		if ( tvi.cChildren && !bStateIsSame )
		{
			TreeView_Expand( hWndTree, hTree, TVE_EXPAND );
			HTREEITEM hChild = TreeView_GetChild( hWndTree, hTree );
			if ( hChild )
			{
				ExcludePathsDlg_ExpandToShowState_r( hWndTree, hChild, depth+1 );
			}
		}
	}
	else
	{
		return;
	}

	if ( !depth )
	{
		// only iterate siblings of the parent's child
		return;
	}

	HTREEITEM hSibling = hTree;
	while ( 1 )
	{
		hSibling = TreeView_GetNextSibling( hWndTree, hSibling );
		if ( !hSibling )
		{
			return;
		}

		tvi.hItem = hSibling;
		if ( TreeView_GetItem( hWndTree, &tvi ) )
		{
			int checkState = -1;
			bool bStateIsSame = ExcludePathsDlg_GetCheckState_r( hWndTree, hSibling, 0, &checkState );

			if ( tvi.cChildren && !bStateIsSame )
			{
				TreeView_Expand( hWndTree, hSibling, TVE_EXPAND );
				HTREEITEM hChild = TreeView_GetChild( hWndTree, hSibling );
				if ( hChild )
				{
					ExcludePathsDlg_ExpandToShowState_r( hWndTree, hChild, depth+1 );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_Find_r
// 
//-----------------------------------------------------------------------------
HTREEITEM ExcludePathsDlg_Find_r( HWND hWndTree, HTREEITEM hTree, int depth, const char *pBasePath, const char *pFindPath )
{
	TVITEM tvi = { 0 };
	char szName[MAX_PATH];
	tvi.mask = TVIF_HANDLE | TVIF_CHILDREN | TVIF_TEXT;
	tvi.hItem = hTree;
	tvi.pszText = szName;
	tvi.cchTextMax = sizeof( szName );
	if ( !TreeView_GetItem( hWndTree, &tvi ) )
	{
		return NULL;
	}

	char szPath[MAX_PATH];
	V_ComposeFileName( pBasePath, szName, szPath, sizeof( szPath ) );
	if ( !stricmp( szPath, pFindPath ) )
	{
		return hTree;
	}
	
	if ( tvi.cChildren )
	{
		HTREEITEM hChild = TreeView_GetChild( hWndTree, hTree );
		if ( hChild )
		{
			HTREEITEM hFindTree = ExcludePathsDlg_Find_r( hWndTree, hChild, depth+1, szPath, pFindPath );
			if ( hFindTree )
			{
				return hFindTree;
			}
		}
	}

	if ( !depth )
	{
		// only iterate siblings of the parent's child
		return NULL;
	}

	// iterate siblings
	HTREEITEM hSibling = hTree;
	while ( 1 )
	{
		hSibling = TreeView_GetNextSibling( hWndTree, hSibling );
		if ( !hSibling )
		{
			break;
		}

		tvi.hItem = hSibling;
		if ( !TreeView_GetItem( hWndTree, &tvi ) )
		{
			break;
		}

		V_ComposeFileName( pBasePath, szName, szPath, sizeof( szPath ) );
		if ( !stricmp( szPath, pFindPath ) )
		{
			return hSibling;
		}

		if ( tvi.cChildren )
		{
			HTREEITEM hChild = TreeView_GetChild( hWndTree, hSibling );
			if ( hChild )
			{
				HTREEITEM hFindTree = ExcludePathsDlg_Find_r( hWndTree, hChild, depth+1, szPath, pFindPath );
				if ( hFindTree )
				{
					return hFindTree;
				}
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_BuildExcludeList_r
//
//	An exclude path represents the path head, and thus does not need to iterate its children
//	if they are deemed to the same exclusion state.
//-----------------------------------------------------------------------------
void ExcludePathsDlg_BuildExcludeList_r( HWND hWndTree, HTREEITEM hTree, int depth, const char *pPath )
{
	int checkState = -1;
	bool bStateIsSame = ExcludePathsDlg_GetCheckState_r( hWndTree, hTree, 0, &checkState );
	if ( checkState == -1 )
	{
		return;
	}

	TVITEM tvi = { 0 };
	char szName[MAX_PATH];
	tvi.mask = TVIF_HANDLE | TVIF_CHILDREN | TVIF_TEXT;
	tvi.hItem = hTree;
	tvi.pszText = szName;
	tvi.cchTextMax = sizeof( szName );
	if ( !TreeView_GetItem( hWndTree, &tvi ) )
	{
		return;
	}

	char szPath[MAX_PATH];
	V_ComposeFileName( pPath, szName, szPath, sizeof( szPath ) );

	if ( checkState == 1 && ( bStateIsSame || !tvi.cChildren ) )
	{
		// add to exclude list
		g_ExcludePaths.AddToTail( szPath );
	}

	if ( !bStateIsSame && tvi.cChildren )
	{
		// mixed states, must recurse to resolve
		HTREEITEM hChild = TreeView_GetChild( hWndTree, hTree );
		if ( hChild )
		{
			ExcludePathsDlg_BuildExcludeList_r( hWndTree, hChild, depth+1, szPath );
		}
	}

	if ( !depth )
	{
		// only iterate siblings of the parent's child
		return;
	}

	// iterate siblings
	HTREEITEM hSibling = hTree;
	while ( 1 )
	{
		hSibling = TreeView_GetNextSibling( hWndTree, hSibling );
		if ( !hSibling )
		{
			break;
		}

		checkState = -1;
		bStateIsSame = ExcludePathsDlg_GetCheckState_r( hWndTree, hSibling, 0, &checkState );
		if ( checkState == -1 )
		{
			break;
		}

		tvi.hItem = hSibling;
		if ( !TreeView_GetItem( hWndTree, &tvi ) )
		{
			break;
		}

		V_ComposeFileName( pPath, szName, szPath, sizeof( szPath ) );

		if ( checkState == 1 && ( bStateIsSame || !tvi.cChildren ) )
		{
			// add to exclude list
			g_ExcludePaths.AddToTail( szPath );
		}

		if ( !bStateIsSame && tvi.cChildren )
		{
			HTREEITEM hChild = TreeView_GetChild( hWndTree, hSibling );
			if ( hChild )
			{
				ExcludePathsDlg_BuildExcludeList_r( hWndTree, hChild, depth+1, szPath );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_SaveChanges
//
//-----------------------------------------------------------------------------
void ExcludePathsDlg_SaveChanges( HWND hWnd )
{
	g_ExcludePaths.Purge();
	HWND hWndTree = GetDlgItem( hWnd, IDC_PATHS_TREE );
	ExcludePathsDlg_BuildExcludeList_r( hWndTree, TreeView_GetRoot( hWndTree ), 0, "" );

	char szPath[MAX_PATH];
	V_ComposeFileName( g_localPath, EXCLUDEPATHS_FILE, szPath, sizeof( szPath ) );

	if ( !g_ExcludePaths.Count() )
	{
		// no exclude paths
		unlink( szPath );
	}
	else
	{
		FILE *fp = fopen( szPath, "wt" );
		if ( !fp )
		{
			Sys_MessageBox( "Error", "Could not open '%s' for writing\n", szPath );
			return;
		}

		fprintf( fp, "// Auto-Generated by VXConsole - DO NOT MODIFY!\n" );
		for ( int i = 0; i < g_ExcludePaths.Count(); i++ )
		{
			// strip expected root path
			const char *pPath = g_ExcludePaths[i].String();
			pPath += strlen( ROOT_NAME );
			if ( !pPath[0] )
			{
				// special code for root
				fprintf( fp, "*\n" );
				break;
			}
			fprintf( fp, "\"\\%s\"\n", pPath );
		}
		fclose( fp );
	}
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_Populate
//
//	Generate a path table.
//-----------------------------------------------------------------------------
void ExcludePathsDlg_Populate( HWND hWnd, bool bRefresh )
{
	struct GamePath_t 
	{
		CUtlString	pathName;
		bool		bIsModPath;
	};

	CUtlVector< GamePath_t > gamePaths;		

	// can skip the time consuming directory scan, unless forced
	if ( bRefresh || !g_PathTable.Count() )
	{
		g_PathTable.Purge();

		for ( int i = 0; i < ARRAYSIZE( g_GameNames ); i++ )
		{
			char szTargetPath[MAX_PATH];
			V_strncpy( szTargetPath, g_localPath, sizeof( szTargetPath ) );
			V_AppendSlash( szTargetPath, sizeof( szTargetPath ) );
			V_strncat( szTargetPath, g_GameNames[i].pName, sizeof( szTargetPath ) );

			GamePath_t gamePath;
			gamePath.pathName = szTargetPath;
			gamePath.bIsModPath = g_GameNames[i].bIsModPath;
			gamePaths.AddToTail( gamePath );
		}

		// iterate all game paths
		for ( int i = 0; i < gamePaths.Count(); i++ )
		{
			CUtlVector< CUtlString > dirList;
			RecurseFileTree_r( g_localPath, gamePaths[i].pathName.String(), 0, dirList, gamePaths[i].bIsModPath );
		}
	}

	// reset the tree
	HWND hWndTree = GetDlgItem( hWnd, IDC_PATHS_TREE );
	TreeView_DeleteAllItems( hWndTree );

	// reset and add root
	// only the root is at depth 0
	AddItemToTree( hWndTree, ROOT_NAME, 0, false );

	// build the tree view
	for ( int iIndex = g_PathTable.FirstInorder(); iIndex != g_PathTable.InvalidIndex(); iIndex = g_PathTable.NextInorder( iIndex ) )
	{
		// due to sorting, number of slashes is depth
		const char *pString = g_PathTable[iIndex].pathName.String();
		int depth = 0;
		for ( int j = 0; j < (int)strlen( pString ); j++ )
		{
			if ( pString[j] == '\\' )
			{
				depth++;
			}
		}
		if ( !depth )
		{
			depth = 1;
		}

		char szPath[MAX_PATH];
		V_FileBase( pString, szPath, sizeof( szPath ) );
		AddItemToTree( hWndTree, szPath, depth, g_PathTable[iIndex].bIsModPath );
	}

	HTREEITEM hTreeRoot = TreeView_GetRoot( hWndTree );
	for ( int i = 0; i < g_ExcludePaths.Count(); i++ )
	{
		HTREEITEM hTree = ExcludePathsDlg_Find_r( hWndTree, hTreeRoot, 0, "", g_ExcludePaths[i].String() );
		if ( hTree )
		{
			ExcludePathsDlg_SetCheckState_r( hWndTree, hTree, 0, true );
		}
	}

	// expand the root node to show state
	ExcludePathsDlg_ExpandToShowState_r( hWndTree, hTreeRoot, 0 );
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_Setup
//
//-----------------------------------------------------------------------------
void ExcludePathsDlg_Setup( HWND hWnd )
{
	TreeView_SetBkColor( GetDlgItem( hWnd, IDC_PATHS_TREE ), g_backgroundColor );

	CheckDlgButton( hWnd, IDC_PATHS_LINKGAMEDIRS, g_bLinkGameDirs ? BST_CHECKED : BST_UNCHECKED );

	// read the exisiting exclude paths
	g_ExcludePaths.Purge();
	char szFilename[MAX_PATH];
	V_ComposeFileName( g_localPath, EXCLUDEPATHS_FILE, szFilename, sizeof( szFilename ) );
	if ( Sys_Exists( szFilename ) )
	{
		Sys_LoadScriptFile( szFilename );
		while ( 1 ) 
		{
			char *pToken = Sys_GetToken( true );
			if ( !pToken || !pToken[0] )
			{
				break;
			}
			Sys_StripQuotesFromToken( pToken );
			if ( !stricmp( pToken, "*" ) )
			{
				pToken = "";
			}
			else if ( pToken[0] == '\\' )
			{
				pToken++;
			}
		
			char szPath[MAX_PATH];
			V_ComposeFileName( ROOT_NAME, pToken, szPath, sizeof( szPath ) );
			V_FixSlashes( szPath );

			g_ExcludePaths.AddToTail( szPath );
		}
	}
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_Proc
//
//-----------------------------------------------------------------------------
BOOL CALLBACK ExcludePathsDlg_Proc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam ) 
{
	HWND hWndTree; 
	LPNMHDR lpnmh;
	HTREEITEM hTree;
	HTREEITEM hTreeRoot;
	int checkState;
	static bool s_bAllowPopulate = 0;

	switch ( message ) 
	{ 
	case WM_INITDIALOG:
		ExcludePathsDlg_Setup( hWnd );
		s_bAllowPopulate = true;
		return TRUE;

	case WM_NOTIFY: 
		lpnmh = (LPNMHDR)lParam;
		if ( ( lpnmh->code  == NM_CLICK ) && ( lpnmh->idFrom == IDC_PATHS_TREE ) )
		{
			TVHITTESTINFO ht = {0};
			DWORD dwpos = GetMessagePos();
			ht.pt.x = GET_X_LPARAM( dwpos );
			ht.pt.y = GET_Y_LPARAM( dwpos );
			MapWindowPoints( HWND_DESKTOP, lpnmh->hwndFrom, &ht.pt, 1 );
			TreeView_HitTest( lpnmh->hwndFrom, &ht );
			if ( ht.flags & TVHT_ONITEMSTATEICON )
			{
				PostMessage( hWnd, UM_CHECKSTATECHANGE, 0, (LPARAM)ht.hItem );
			}
		}
		break; 

	case UM_CHECKSTATECHANGE:
		hWndTree = GetDlgItem( hWnd, IDC_PATHS_TREE );
		hTreeRoot = TreeView_GetRoot( hWndTree );
		hTree = (HTREEITEM)lParam;
		checkState = TreeView_GetCheckState( hWndTree, hTree );
		if ( checkState != -1 )
		{
			TVITEM tvi = { 0 };
			char szName[MAX_PATH];
			tvi.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_TEXT;
			tvi.hItem = hTree;
			tvi.pszText = szName;
			tvi.cchTextMax = sizeof( szName );
			if ( !TreeView_GetItem( hWndTree, &tvi ) )
			{
				break;
			}
			int nDepth = LOWORD( tvi.lParam );
			bool bIsModPath = HIWORD( tvi.lParam ) != 0;

			if ( g_bLinkGameDirs && bIsModPath )
			{
				// a mod path root is at depth 1
				// a child of a mod path (depth > 1), will match all other children in mod paths
				// a mod path (depth = 1) will match all other modpaths
				ExcludePathsDlg_SetCheckStateLinked_r( hWndTree, hTreeRoot, 0, checkState, nDepth == 1 ? NULL : szName );
			}
			else
			{
				ExcludePathsDlg_SetCheckState_r( hWndTree, hTree, 0, checkState );
			}
		}
		break;

	case WM_PAINT:
		if ( s_bAllowPopulate )
		{
			// unfortunate, but tree view needs to paint first before its state can be set
			// related to checkbox style which doesn't manifest until after WM_INITDIALOG
			// stall the initial population until after the first paint
			s_bAllowPopulate = false;
			PostMessage( hWnd, UM_FIRSTTIMEPOPULATE, 0, 0 );
		}
		break;

	case UM_FIRSTTIMEPOPULATE:
		ExcludePathsDlg_Populate( hWnd, false );
		break;

	case WM_COMMAND: 
		switch ( LOWORD( wParam ) ) 
		{
		case IDC_OK: 
			ExcludePathsDlg_SaveChanges( hWnd );
			EndDialog( hWnd, wParam );
			return TRUE; 

		case IDC_PATHS_RESCAN:
			ExcludePathsDlg_Populate( hWnd, true );
			return TRUE;

		case IDC_PATHS_EXPAND:
			// expand from root
			hWndTree = GetDlgItem( hWnd, IDC_PATHS_TREE );
			ExcludePathsDlg_Expand_r( hWndTree, TreeView_GetRoot( hWndTree ), 0, -1, true );
			return TRUE;

		case IDC_PATHS_COLLAPSE:
			// collapse from root
			hWndTree = GetDlgItem( hWnd, IDC_PATHS_TREE );
			ExcludePathsDlg_Expand_r( hWndTree, TreeView_GetRoot( hWndTree ), 0, -1, false );
			return TRUE;

		case IDC_PATHS_LINKGAMEDIRS:
			g_bLinkGameDirs = IsDlgButtonChecked( hWnd, IDC_PATHS_LINKGAMEDIRS );
			return TRUE;

		case IDCANCEL:
		case IDC_CANCEL: 
			EndDialog( hWnd, wParam );
			return TRUE; 
		}
		break; 
	}
	return FALSE; 
} 

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_LoadConfig	
// 
//-----------------------------------------------------------------------------
void ExcludePathsDlg_LoadConfig()
{
	// get our config
	Sys_GetRegistryInteger( "ExcludePaths_LinkGameDirs", true, g_bLinkGameDirs );
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_SaveConfig
// 
//-----------------------------------------------------------------------------
void ExcludePathsDlg_SaveConfig()
{
	Sys_SetRegistryInteger( "ExcludePaths_LinkGameDirs", g_bLinkGameDirs );
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_Init
// 
//-----------------------------------------------------------------------------
bool ExcludePathsDlg_Init()
{
	return true;
}

//-----------------------------------------------------------------------------
//	ExcludePathsDlg_Open	
// 
//-----------------------------------------------------------------------------
void ExcludePathsDlg_Open()
{
	ExcludePathsDlg_LoadConfig();

	int result = DialogBox( g_hInstance, MAKEINTRESOURCE( IDD_EXCLUDEPATHS ), g_hDlgMain, ( DLGPROC )ExcludePathsDlg_Proc );
	if ( LOWORD( result ) != IDC_OK )
		return;

	ExcludePathsDlg_SaveConfig();
}
