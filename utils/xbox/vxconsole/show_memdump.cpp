//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	SHOW_MEMDUMP.CPP
//
//	Show Mem Dump Display.
//=====================================================================================//
#include "vxconsole.h"

#define ID_SHOWMEMDUMP_LISTVIEW 100

// column id, pool mode
#define ID_DUMPPOOL_POOL			0
#define ID_DUMPPOOL_SIZE			1
#define ID_DUMPPOOL_ALLOCATED		2
#define ID_DUMPPOOL_FREE			3
#define ID_DUMPPOOL_COMMITTED		4
#define ID_DUMPPOOL_COMMITTEDSIZE	5

// column id, detailed mode
#define ID_DUMPDETAIL_ALLOCATION	0
#define ID_DUMPDETAIL_CURRENTSIZE	1
#define ID_DUMPDETAIL_PEAKSIZE		2
#define ID_DUMPDETAIL_TOTALSIZE		3
#define ID_DUMPDETAIL_OVERHEAD		4
#define ID_DUMPDETAIL_PEAKOVERHEAD	5
#define ID_DUMPDETAIL_TIME			6
#define ID_DUMPDETAIL_CURRENTCOUNT	7
#define ID_DUMPDETAIL_PEAKCOUNT		8
#define ID_DUMPDETAIL_TOTALCOUNT	9
#define ID_DUMPDETAIL_LTE16			10		
#define ID_DUMPDETAIL_LTE32			11
#define ID_DUMPDETAIL_LTE128		12
#define ID_DUMPDETAIL_LTE1024		13
#define ID_DUMPDETAIL_GT1024		14

#define SHOW_BYTES			0
#define SHOW_KILOBYTES		1
#define SHOW_MEGABYTES		2

#define FORMAT_POOLS		0
#define FORMAT_DETAILS		1

struct MemoryPool_t
{
	int			pool;
	char		poolBuff[32];
	int			size;
	char		sizeBuff[32];
	int			allocated;
	char		allocatedBuff[32];
	int			free;
	char		freeBuff[32];
	int			committed;
	char		committedBuff[32];
	int			committedSize;
	char		committedSizeBuff[32];
};

struct MemoryDetail_t
{
	char		*pAllocationName;
	int			currentSize;
	char		currentSizeBuff[32];
	int			peakSize;
	char		peakSizeBuff[32];
	int			totalSize;
	char		totalSizeBuff[32];
	int			overheadSize;
	char		overheadSizeBuff[32];
	int			peakOverheadSize;
	char		peakOverheadSizeBuff[32];
	int			time;
	char		timeBuff[32];
	int			currentCount;
	char		currentCountBuff[32];
	int			peakCount;
	char		peakCountBuff[32];
	int			totalCount;
	char		totalCountBuff[32];
	int			lte16;
	char		lte16Buff[32];
	int			lte32;
	char		lte32Buff[32];
	int			lte128;
	char		lte128Buff[32];
	int			lte1024;
	char		lte1024Buff[32];
	int			gt1024;
	char		gt1024Buff[32];
};

struct memory_t
{
	int				listIndex;
	MemoryPool_t	pool;
	MemoryDetail_t	detail;
};

struct label_t
{	
	const CHAR*			name;
	int					width;
	int					subItemIndex;
	CHAR				nameBuff[64];
};

HWND		g_showMemDump_hWnd;
HWND		g_showMemDump_hWndListView;
RECT		g_showMemDump_windowRect;
int			g_showMemDump_sortColumn;
int			g_showMemDump_sortDescending;
memory_t	*g_showMemDump_pMemory;
int			g_showMemDump_numMemory;
int			g_showMemDump_showBytes;
bool		g_showMemDump_bCollapseOutput;
char		g_showMemDump_currentFilename[MAX_PATH];
int			g_showMemDump_format;

void		ShowMemDump_Parse( const char *pBuffer, int fileSize );

label_t g_showMemDump_PoolLabels[] =
{
	{"Pool",				120,	ID_DUMPPOOL_POOL},
	{"Size",				120,	ID_DUMPPOOL_SIZE},
	{"Allocated Count",		120,	ID_DUMPPOOL_ALLOCATED},
	{"Free Count",			120,	ID_DUMPPOOL_FREE},
	{"Committed Count",		120,	ID_DUMPPOOL_COMMITTED},
	{"Committed Size",		120,	ID_DUMPPOOL_COMMITTEDSIZE},
};

label_t g_showMemDump_DetailLabels[] =
{
	{"Allocation",			200,	ID_DUMPDETAIL_ALLOCATION},
	{"Current Size",		120,	ID_DUMPDETAIL_CURRENTSIZE},
	{"Peak Size",			120,	ID_DUMPDETAIL_PEAKSIZE},
	{"Total Allocations",	120,	ID_DUMPDETAIL_TOTALSIZE},
	{"Overhead Size",		120,	ID_DUMPDETAIL_OVERHEAD},
	{"Peak Overhead Size",	120,	ID_DUMPDETAIL_PEAKOVERHEAD},
	{"Time (ms)",			120,	ID_DUMPDETAIL_TIME},
	{"Current Count",		120,	ID_DUMPDETAIL_CURRENTCOUNT},
	{"Peak Count",			120,	ID_DUMPDETAIL_PEAKCOUNT},
	{"Total Count",			120,	ID_DUMPDETAIL_TOTALCOUNT},
	{"<=16 bytes",			100,	ID_DUMPDETAIL_LTE16},
	{"17-32 bytes",			100,	ID_DUMPDETAIL_LTE32},
	{"33-128 bytes",		100,	ID_DUMPDETAIL_LTE128},
	{"129-1024 bytes",		100,	ID_DUMPDETAIL_LTE1024},
	{"> 1024 bytes",		100,	ID_DUMPDETAIL_GT1024},
};

//-----------------------------------------------------------------------------
//	ShowMemDump_FormatSize
// 
//-----------------------------------------------------------------------------
char *ShowMemDump_FormatSize( int size, char *pBuff, bool bUnits )
{
	switch ( g_showMemDump_showBytes )
	{
		case SHOW_BYTES:
			sprintf( pBuff, "%d", size );
			break;

		case SHOW_KILOBYTES:
			sprintf( pBuff, "%.2f", (float)size/1024.0f );
			if ( bUnits )
				strcat( pBuff, " K");
			break;

		case SHOW_MEGABYTES:
			sprintf( pBuff, "%.2f", (float)size/(1024.0f*1024.0f) );
			if ( bUnits )
				strcat( pBuff, " MB");
			break;
	}

	return pBuff;
}


//-----------------------------------------------------------------------------
//	ShowMemDump_SaveConfig
// 
//-----------------------------------------------------------------------------
void ShowMemDump_SaveConfig()
{
	char	buff[256];

	Sys_SetRegistryInteger( "showMemDumpSortColumn", g_showMemDump_sortColumn );
	Sys_SetRegistryInteger( "showMemDumpSortDescending", g_showMemDump_sortDescending );
	Sys_SetRegistryInteger( "showMemDumpShowBytes", g_showMemDump_showBytes );

	WINDOWPLACEMENT wp;
	memset( &wp, 0, sizeof( wp ) );
	wp.length = sizeof( WINDOWPLACEMENT );
	GetWindowPlacement( g_showMemDump_hWnd, &wp );
	g_showMemDump_windowRect = wp.rcNormalPosition;

	sprintf( buff, "%d %d %d %d", g_showMemDump_windowRect.left, g_showMemDump_windowRect.top, g_showMemDump_windowRect.right, g_showMemDump_windowRect.bottom );
	Sys_SetRegistryString( "showMemDumpWindowRect", buff );
}

//-----------------------------------------------------------------------------
//	ShowMemDump_LoadConfig	
// 
//-----------------------------------------------------------------------------
void ShowMemDump_LoadConfig()
{
	int		numArgs;
	char	buff[256];

	Sys_GetRegistryInteger( "showMemDumpSortColumn", 0, g_showMemDump_sortColumn );
	Sys_GetRegistryInteger( "showMemDumpSortDescending", false, g_showMemDump_sortDescending );
	Sys_GetRegistryInteger( "showMemDumpShowBytes", SHOW_KILOBYTES, g_showMemDump_showBytes );

	Sys_GetRegistryString( "showMemDumpWindowRect", buff, "", sizeof( buff ) );
	numArgs = sscanf( buff, "%d %d %d %d", &g_showMemDump_windowRect.left, &g_showMemDump_windowRect.top, &g_showMemDump_windowRect.right, &g_showMemDump_windowRect.bottom );
	if ( numArgs != 4 || g_showMemDump_windowRect.left < 0 || g_showMemDump_windowRect.top < 0 || g_showMemDump_windowRect.right < 0 || g_showMemDump_windowRect.bottom < 0 )
		memset( &g_showMemDump_windowRect, 0, sizeof( g_showMemDump_windowRect ) );
}

//-----------------------------------------------------------------------------
//	ShowMemDump_Clear
// 
//-----------------------------------------------------------------------------
void ShowMemDump_Clear()
{
	// delete all the list view entries
	if ( g_showMemDump_hWnd )
	{
		ListView_DeleteAllItems( g_showMemDump_hWndListView );
	}

	if ( !g_showMemDump_pMemory )
		return;

	for ( int i=0; i<g_showMemDump_numMemory; i++ )
	{
		delete [] g_showMemDump_pMemory[i].detail.pAllocationName;
	}

	Sys_Free( g_showMemDump_pMemory );

	g_showMemDump_pMemory   = NULL;
	g_showMemDump_numMemory = 0;
}

//-----------------------------------------------------------------------------
//	ShowMemDump_Export
// 
//-----------------------------------------------------------------------------
void ShowMemDump_Export()
{
	OPENFILENAME		ofn;
	char				logFilename[MAX_PATH]; 
	int					retval;
	FILE*				fp;
	int					i;
	char				buff[64];

	if ( g_showMemDump_format != FORMAT_DETAILS )
	{
		return;
	}

	memset( &ofn, 0, sizeof( ofn ) );
	ofn.lStructSize     = sizeof( ofn );
	ofn.hwndOwner       = g_showMemDump_hWnd;
	ofn.lpstrFile       = logFilename;
	ofn.lpstrFile[0]    = '\0';
	ofn.nMaxFile        = sizeof( logFilename );
	ofn.lpstrFilter     = "Excel CSV\0*.CSV\0All Files\0*.*\0";
	ofn.nFilterIndex    = 1;
	ofn.lpstrFileTitle  = NULL;
	ofn.nMaxFileTitle   = 0;
	ofn.lpstrInitialDir = "c:\\";
	ofn.Flags           = OFN_PATHMUSTEXIST;

	// display the Open dialog box. 
	retval = GetOpenFileName( &ofn ); 
	if ( !retval )
		return;

	Sys_AddExtension( ".csv", logFilename, sizeof( logFilename ) );

	fp = fopen( logFilename, "wt+" );
	if ( !fp )
		return;

	// labels
	fprintf( fp, "Allocation Type" );
	fprintf( fp, ",Current Size" );
	fprintf( fp, ",Peak Size" );
	fprintf( fp, ",Total Allocations" );
	fprintf( fp, ",Overhead Size" );
	fprintf( fp, ",Peak Overhead Size" );
	fprintf( fp, ",Time(ms)" );
	fprintf( fp, ",Current Count" );
	fprintf( fp, ",Peak Count" );
	fprintf( fp, ",Total Count" );
	fprintf( fp, ",<=16 Byte Allocations" );
	fprintf( fp, ",17-32 Byte Allocations" );
	fprintf( fp, ",33-128 Byte Allocations" );
	fprintf( fp, ",129-1024 Byte Allocations" );
	fprintf( fp, ",>1024 Byte Allocations" );
	fprintf( fp, "\n" );

	// dump to the log
	for ( i=0; i<g_showMemDump_numMemory; i++ )
	{
		fprintf( fp, "\"%s\"", g_showMemDump_pMemory[i].detail.pAllocationName );
		fprintf( fp, ",%s", ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.currentSize, buff, false ) );
		fprintf( fp, ",%s", ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.peakSize, buff, false ) );
		fprintf( fp, ",%s", ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.totalSize, buff, false ) );
		fprintf( fp, ",%s", ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.overheadSize, buff, false ) );
		fprintf( fp, ",%s", ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.peakOverheadSize, buff, false ) );
		fprintf( fp, ",%d", g_showMemDump_pMemory[i].detail.time );
		fprintf( fp, ",%d", g_showMemDump_pMemory[i].detail.currentCount );
		fprintf( fp, ",%d", g_showMemDump_pMemory[i].detail.peakCount );
		fprintf( fp, ",%d", g_showMemDump_pMemory[i].detail.totalCount  );
		fprintf( fp, ",%d", g_showMemDump_pMemory[i].detail.lte16 );
		fprintf( fp, ",%d", g_showMemDump_pMemory[i].detail.lte32 );
		fprintf( fp, ",%d", g_showMemDump_pMemory[i].detail.lte128 );
		fprintf( fp, ",%d", g_showMemDump_pMemory[i].detail.lte1024 );
		fprintf( fp, ",%d", g_showMemDump_pMemory[i].detail.gt1024 );
		fprintf( fp, "\n" );
	}

	fclose( fp );
}

//-----------------------------------------------------------------------------
//	ShowMemDump_Summary
// 
//-----------------------------------------------------------------------------
void ShowMemDump_Summary()
{
	char	buff[1024];
	int		i;

	if ( g_showMemDump_format != FORMAT_DETAILS )
	{
		return;
	}

	int currentSize      = 0;
	int peakSize         = 0;
	int totalSize        = 0;
	int overheadSize     = 0;
	int peakOverheadSize = 0;
	int time             = 0;
	int currentCount     = 0;
	int peakCount        = 0;
	int totalCount       = 0;
	int lte16            = 0;
	int lte32            = 0;
	int lte128           = 0;
	int lte1024          = 0;
	int gt1024           = 0;

	// tally the totals
	for (i=0; i<g_showMemDump_numMemory; i++)
	{
		if ( !strnicmp( g_showMemDump_pMemory[i].detail.pAllocationName, "Totals,", 7 ) )
			continue;

		currentSize      += g_showMemDump_pMemory[i].detail.currentSize;
		peakSize         += g_showMemDump_pMemory[i].detail.peakSize;
		totalSize        += g_showMemDump_pMemory[i].detail.totalSize;
		overheadSize     += g_showMemDump_pMemory[i].detail.overheadSize;
		peakOverheadSize += g_showMemDump_pMemory[i].detail.peakOverheadSize;
		time             += g_showMemDump_pMemory[i].detail.time;
		currentCount     += g_showMemDump_pMemory[i].detail.currentCount;
		peakCount        += g_showMemDump_pMemory[i].detail.peakCount;
		totalCount       += g_showMemDump_pMemory[i].detail.totalCount;
		lte16            += g_showMemDump_pMemory[i].detail.lte16;
		lte32            += g_showMemDump_pMemory[i].detail.lte32;
		lte128           += g_showMemDump_pMemory[i].detail.lte128;
		lte1024          += g_showMemDump_pMemory[i].detail.lte1024;
		gt1024           += g_showMemDump_pMemory[i].detail.gt1024;
	}

	sprintf( 
		buff,
		"Entries:\t\t\t%d\n"
		"Current Size:\t\t%.2f MB\n"
		"Peak Size:\t\t%.2f MB\n"
		"Total Size:\t\t%.2f MB\n"
		"Overhead Size:\t\t%d\n"
		"Peak Overhead Size:\t%d\n"
		"Time:\t\t\t%d\n"
		"Current Count:\t\t%d\n"
		"Peak Count:\t\t%d\n"
		"Total Count:\t\t%d\n"
		"<= 16:\t\t\t%d\n"
		"17-32:\t\t\t%d\n"
		"33-128:\t\t\t%d\n"
		"129-1024:\t\t%d\n"
		"> 1024:\t\t\t%d\n",
		g_showMemDump_numMemory, 
		(float)currentSize/( 1024.0F*1024.0F ),
		(float)peakSize/( 1024.0F*1024.0F ),
		(float)totalSize/( 1024.0F*1024.0F ),
		overheadSize,
		peakOverheadSize,
		time,
		currentCount,
		peakCount,
		totalCount,
		lte16,
		lte32,
		lte128,
		lte1024,
		gt1024 );

	MessageBox( g_showMemDump_hWnd, buff, "Memory Dump Summary", MB_OK|MB_APPLMODAL );
}

//-----------------------------------------------------------------------------
//	ShowMemDump_SetTitle
// 
//-----------------------------------------------------------------------------
void ShowMemDump_SetTitle()
{
	if ( g_showMemDump_hWnd )
	{
		SetWindowText( g_showMemDump_hWnd, "Memory Dump" );
	}
}

//-----------------------------------------------------------------------------
//	ShowMemDump_CompareFunc
// 
//-----------------------------------------------------------------------------
int CALLBACK ShowMemDump_CompareFunc( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort )
{
	memory_t*	pMemoryA = (memory_t*)lParam1;
	memory_t*	pMemoryB = (memory_t*)lParam2;

	int sort = 0;

	if ( g_showMemDump_format == FORMAT_POOLS )
	{
		switch ( g_showMemDump_sortColumn )
		{
			case ID_DUMPPOOL_POOL:
				sort = pMemoryA->pool.pool - pMemoryB->pool.pool;
				break;

			case ID_DUMPPOOL_SIZE:
				sort = pMemoryA->pool.size - pMemoryB->pool.size;
				break;

			case ID_DUMPPOOL_ALLOCATED:
				sort = pMemoryA->pool.allocated - pMemoryB->pool.allocated;
				break;

			case ID_DUMPPOOL_FREE:
				sort = pMemoryA->pool.free - pMemoryB->pool.free;
				break;

			case ID_DUMPPOOL_COMMITTED:
				sort = pMemoryA->pool.committed - pMemoryB->pool.committed;
				break;

			case ID_DUMPPOOL_COMMITTEDSIZE:
				sort = pMemoryA->pool.committedSize - pMemoryB->pool.committedSize;
				break;
		}
	}
	else
	{
		switch ( g_showMemDump_sortColumn )
		{
			case ID_DUMPDETAIL_ALLOCATION:
				sort = stricmp( pMemoryA->detail.pAllocationName, pMemoryB->detail.pAllocationName );
				break;

			case ID_DUMPDETAIL_CURRENTSIZE:
				sort = pMemoryA->detail.currentSize - pMemoryB->detail.currentSize;
				break;

			case ID_DUMPDETAIL_PEAKSIZE:
				sort = pMemoryA->detail.peakSize - pMemoryB->detail.peakSize;
				break;

			case ID_DUMPDETAIL_TOTALSIZE:
				sort = pMemoryA->detail.totalSize - pMemoryB->detail.totalSize;
				break;

			case ID_DUMPDETAIL_OVERHEAD:
				sort = pMemoryA->detail.overheadSize - pMemoryB->detail.overheadSize;
				break;

			case ID_DUMPDETAIL_PEAKOVERHEAD:
				sort = pMemoryA->detail.peakOverheadSize - pMemoryB->detail.peakOverheadSize;
				break;

			case ID_DUMPDETAIL_TIME:
				sort = pMemoryA->detail.time - pMemoryB->detail.time;
				break;

			case ID_DUMPDETAIL_CURRENTCOUNT:
				sort = pMemoryA->detail.currentCount - pMemoryB->detail.currentCount;
				break;

			case ID_DUMPDETAIL_PEAKCOUNT:
				sort = pMemoryA->detail.peakCount - pMemoryB->detail.peakCount;
				break;

			case ID_DUMPDETAIL_TOTALCOUNT:
				sort = pMemoryA->detail.totalCount - pMemoryB->detail.totalCount;
				break;

			case ID_DUMPDETAIL_LTE16:
				sort = pMemoryA->detail.lte16 - pMemoryB->detail.lte16;
				break;

			case ID_DUMPDETAIL_LTE32:
				sort = pMemoryA->detail.lte32 - pMemoryB->detail.lte32;
				break;

			case ID_DUMPDETAIL_LTE128:
				sort = pMemoryA->detail.lte128 - pMemoryB->detail.lte128;
				break;

			case ID_DUMPDETAIL_LTE1024:
				sort = pMemoryA->detail.lte1024 - pMemoryB->detail.lte1024;
				break;

			case ID_DUMPDETAIL_GT1024:
				sort = pMemoryA->detail.gt1024 - pMemoryB->detail.gt1024;
				break;
		}
	}

	// flip the sort order
	if ( g_showMemDump_sortDescending )
	{
		sort *= -1;
	}

	return ( sort );
}

//-----------------------------------------------------------------------------
//	ShowMemDump_SortItems
// 
//-----------------------------------------------------------------------------
void ShowMemDump_SortItems()
{
	LVITEM		lvitem;
	memory_t	*pMemory;
	int			i;

	if ( !g_showMemDump_hWnd )
	{
		// only sort if window is visible
		return;
	}

	ListView_SortItems( g_showMemDump_hWndListView, ShowMemDump_CompareFunc, 0 );

	memset( &lvitem, 0, sizeof( lvitem ) );
	lvitem.mask = LVIF_PARAM;

	// get each item and reset its list index
	int itemCount = ListView_GetItemCount( g_showMemDump_hWndListView );
	for ( i=0; i<itemCount; i++ )
	{
		lvitem.iItem = i;
		ListView_GetItem( g_showMemDump_hWndListView, &lvitem );

		pMemory = (memory_t*)lvitem.lParam;
		pMemory->listIndex = i;
	}

	int count;
	label_t* pLabels;
	if ( g_showMemDump_format == FORMAT_POOLS )
	{
		count = sizeof( g_showMemDump_PoolLabels )/sizeof( g_showMemDump_PoolLabels[0] );
		pLabels = g_showMemDump_PoolLabels;
	}
	else
	{
		count = sizeof( g_showMemDump_DetailLabels )/sizeof( g_showMemDump_DetailLabels[0] );
		pLabels = g_showMemDump_DetailLabels;
	}

	// update list view columns with sort key
	for ( i = 0; i < count; i++ )
	{
		char		symbol;
		LVCOLUMN	lvc; 

		if ( i == g_showMemDump_sortColumn )
			symbol = g_showMemDump_sortDescending ? '<' : '>';
		else
			symbol = ' ';
		sprintf( pLabels[i].nameBuff, "%s %c", pLabels[i].name, symbol );

		memset( &lvc, 0, sizeof( lvc ) );
		lvc.mask    = LVCF_TEXT; 
		lvc.pszText = (LPSTR)pLabels[i].nameBuff;

		ListView_SetColumn( g_showMemDump_hWndListView, i, &lvc );
	}
}

//-----------------------------------------------------------------------------
//	ShowMemDump_FormatItems
// 
//-----------------------------------------------------------------------------
void ShowMemDump_FormatItems()
{
	if ( g_showMemDump_format == FORMAT_POOLS )
	{
		for ( int i = 0; i < g_showMemDump_numMemory; i++ )
		{
			sprintf( g_showMemDump_pMemory[i].pool.sizeBuff, "%d", g_showMemDump_pMemory[i].pool.size );
			ShowMemDump_FormatSize( g_showMemDump_pMemory[i].pool.committedSize, g_showMemDump_pMemory[i].pool.committedSizeBuff, true );
		}
	}
	else
	{
		for ( int i = 0; i < g_showMemDump_numMemory; i++ )
		{
			ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.currentSize, g_showMemDump_pMemory[i].detail.currentSizeBuff, true );
			ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.peakSize, g_showMemDump_pMemory[i].detail.peakSizeBuff, true );
			ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.totalSize, g_showMemDump_pMemory[i].detail.totalSizeBuff, true );
			ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.overheadSize, g_showMemDump_pMemory[i].detail.overheadSizeBuff, true );
			ShowMemDump_FormatSize( g_showMemDump_pMemory[i].detail.peakOverheadSize, g_showMemDump_pMemory[i].detail.peakOverheadSizeBuff, true );
		}
	}

	ShowMemDump_SortItems();
}

//-----------------------------------------------------------------------------
//	ShowMemDump_AddViewItem
// 
//-----------------------------------------------------------------------------
void ShowMemDump_AddViewItem( memory_t* pMemory )
{
	LVITEM	lvi;

	if ( !g_showMemDump_hWnd )
	{
		// only valid if log window is visible
		return;
	}

	// update the text callback buffers
	if ( g_showMemDump_format == FORMAT_POOLS )
	{
		sprintf( pMemory->pool.sizeBuff, "%d", pMemory->pool.size );
		sprintf( pMemory->pool.poolBuff, "%d", pMemory->pool.pool );
		sprintf( pMemory->pool.allocatedBuff, "%d", pMemory->pool.allocated );
		sprintf( pMemory->pool.freeBuff, "%d", pMemory->pool.free );
		sprintf( pMemory->pool.committedBuff, "%d", pMemory->pool.committed );
		ShowMemDump_FormatSize( pMemory->pool.committedSize, pMemory->pool.committedSizeBuff, true );
	}
	else
	{
		ShowMemDump_FormatSize( pMemory->detail.currentSize, pMemory->detail.currentSizeBuff, true );
		ShowMemDump_FormatSize( pMemory->detail.peakSize, pMemory->detail.peakSizeBuff, true );
		ShowMemDump_FormatSize( pMemory->detail.totalSize, pMemory->detail.totalSizeBuff, true );
		ShowMemDump_FormatSize( pMemory->detail.overheadSize, pMemory->detail.overheadSizeBuff, true );
		ShowMemDump_FormatSize( pMemory->detail.peakOverheadSize, pMemory->detail.peakOverheadSizeBuff, true );
		sprintf( pMemory->detail.timeBuff, "%d", pMemory->detail.time );
		sprintf( pMemory->detail.currentCountBuff, "%d", pMemory->detail.currentCount );
		sprintf( pMemory->detail.peakCountBuff, "%d", pMemory->detail.peakCount );
		sprintf( pMemory->detail.totalCountBuff, "%d", pMemory->detail.totalCount );
		sprintf( pMemory->detail.lte16Buff, "%d", pMemory->detail.lte16 );
		sprintf( pMemory->detail.lte32Buff, "%d", pMemory->detail.lte32 );
		sprintf( pMemory->detail.lte128Buff, "%d", pMemory->detail.lte128 );
		sprintf( pMemory->detail.lte1024Buff, "%d", pMemory->detail.lte1024 );
		sprintf( pMemory->detail.gt1024Buff, "%d", pMemory->detail.gt1024 );
	}

	int itemCount = ListView_GetItemCount( g_showMemDump_hWndListView );

	// setup and insert at end of list
	memset( &lvi, 0, sizeof( lvi ) );
	lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
	lvi.iItem = itemCount;
	lvi.iSubItem = 0;
	lvi.state = 0; 
	lvi.stateMask = 0;
	lvi.pszText = LPSTR_TEXTCALLBACK;  
	lvi.lParam = (LPARAM)pMemory;
								
	// insert and set the real index 
	pMemory->listIndex = ListView_InsertItem( g_showMemDump_hWndListView, &lvi );
}

//-----------------------------------------------------------------------------
//	ShowMemDump_Refresh
// 
//-----------------------------------------------------------------------------
void ShowMemDump_Refresh()
{
	// send the command to application which replies with list data
	if ( g_connectedToApp )
	{
		ProcessCommand( "mem_dump" );
	}
}

//-----------------------------------------------------------------------------
//	ShowMemDump_RefreshView
// 
//-----------------------------------------------------------------------------
void ShowMemDump_RefreshView()
{
	if ( strlen( g_showMemDump_currentFilename ) == 0 )
	{
		// first time
		ShowMemDump_Refresh();
		return;
	}

	// get current file
	int	fileSize;
	char *pBuffer;
	bool bSuccess = LoadTargetFile( g_showMemDump_currentFilename, &fileSize, (void **)&pBuffer );
	if ( !bSuccess )
	{
		// stale, try again
		ShowMemDump_Refresh();
		return;
	}

	// parse and update view
	ShowMemDump_Parse( pBuffer, fileSize );

	Sys_Free( pBuffer );
}

//-----------------------------------------------------------------------------
//	ShowMemDump_PopulateList
// 
//-----------------------------------------------------------------------------
void ShowMemDump_PopulateList()
{
	// delete previous labels
	while ( 1 )
	{
		if ( !ListView_DeleteColumn( g_showMemDump_hWndListView, 0 ) )
		{
			break;
		}
	}

	int count;
	label_t* pLabels;
	if ( g_showMemDump_format == FORMAT_POOLS )
	{
		count = sizeof( g_showMemDump_PoolLabels )/sizeof( g_showMemDump_PoolLabels[0] );
		pLabels = g_showMemDump_PoolLabels;
	}
	else
	{
		count = sizeof( g_showMemDump_DetailLabels )/sizeof( g_showMemDump_DetailLabels[0] );
		pLabels = g_showMemDump_DetailLabels;
	}

	// init list view columns
	for ( int i = 0; i < count; i++ )
	{
		LVCOLUMN lvc; 
		memset( &lvc, 0, sizeof( lvc ) );

		lvc.mask = LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM; 
		lvc.iSubItem = 0;
		lvc.cx = pLabels[i].width;
		lvc.fmt = LVCFMT_LEFT;
		lvc.pszText = ( LPSTR )pLabels[i].name;

		ListView_InsertColumn( g_showMemDump_hWndListView, i, &lvc );
	}

	// populate list view
	for ( int i = 0; i < g_showMemDump_numMemory; i++ )
	{
		ShowMemDump_AddViewItem( &g_showMemDump_pMemory[i] );
	}

	ShowMemDump_SortItems();
}

//-----------------------------------------------------------------------------
//	ShowMemDump_SizeWindow
// 
//-----------------------------------------------------------------------------
void ShowMemDump_SizeWindow( HWND hwnd, int cx, int cy )
{
    if ( cx==0 || cy==0 )
    {
        RECT rcClient;
        GetClientRect( hwnd, &rcClient );
        cx = rcClient.right;
        cy = rcClient.bottom;
    }

	// position the ListView
	SetWindowPos( g_showMemDump_hWndListView, NULL, 0, 0, cx, cy, SWP_NOZORDER );
}

//-----------------------------------------------------------------------------
//	ShowMemDump_WndProc
// 
//-----------------------------------------------------------------------------
LRESULT CALLBACK ShowMemDump_WndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    WORD		wID = LOWORD( wParam );
	memory_t*	pMemory;

	switch ( message )
	{
		case WM_CREATE:
			return 0L;

		case WM_DESTROY:
			ShowMemDump_SaveConfig();

			g_showMemDump_hWnd = NULL;
			return 0L;

		case WM_INITMENU:
            CheckMenuItem( (HMENU)wParam, IDM_OPTIONS_BYTES, MF_BYCOMMAND | ( g_showMemDump_showBytes  == SHOW_BYTES ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( (HMENU)wParam, IDM_OPTIONS_KILOBYTES, MF_BYCOMMAND | ( g_showMemDump_showBytes  == SHOW_KILOBYTES ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( (HMENU)wParam, IDM_OPTIONS_MEGABYTES, MF_BYCOMMAND | ( g_showMemDump_showBytes  == SHOW_MEGABYTES ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( (HMENU)wParam, IDM_OPTIONS_COLLAPSEOUTPUT, MF_BYCOMMAND | ( g_showMemDump_bCollapseOutput ? MF_CHECKED : MF_UNCHECKED ) );
			return 0L;

		case WM_SIZE:
			ShowMemDump_SizeWindow( hwnd, LOWORD( lParam ), HIWORD( lParam ) );
			return 0L;

		case WM_NOTIFY:
			switch ( ( ( LPNMHDR )lParam )->code )
			{
				case LVN_COLUMNCLICK:
					NMLISTVIEW*	pnmlv;
					pnmlv = ( NMLISTVIEW* )lParam; 
					if ( g_showMemDump_sortColumn == pnmlv->iSubItem )
					{
						// user has clicked on same column - flip the sort
						g_showMemDump_sortDescending ^= 1;
					}
					else
					{
						// sort by new column
						g_showMemDump_sortColumn = pnmlv->iSubItem;
					}
					ShowMemDump_SortItems();
					return 0L;

				case LVN_GETDISPINFO:
					NMLVDISPINFO* plvdi;
					plvdi = (NMLVDISPINFO*)lParam;
					pMemory = (memory_t*)plvdi->item.lParam;
	
					if ( g_showMemDump_format == FORMAT_POOLS )
					{
						switch ( plvdi->item.iSubItem )
						{
							case ID_DUMPPOOL_POOL:
								plvdi->item.pszText = pMemory->pool.poolBuff;
								return 0L;

							case ID_DUMPPOOL_SIZE:
								plvdi->item.pszText = pMemory->pool.sizeBuff;
								return 0L;

							case ID_DUMPPOOL_ALLOCATED:
								plvdi->item.pszText = pMemory->pool.allocatedBuff;
								return 0L;

							case ID_DUMPPOOL_FREE:
								plvdi->item.pszText = pMemory->pool.freeBuff;
								return 0L;

							case ID_DUMPPOOL_COMMITTED:
								plvdi->item.pszText = pMemory->pool.committedBuff;
								return 0L;

							case ID_DUMPPOOL_COMMITTEDSIZE:
								plvdi->item.pszText = pMemory->pool.committedSizeBuff;
								return 0L;
						}
					}
					else
					{
						switch ( plvdi->item.iSubItem )
						{
							case ID_DUMPDETAIL_ALLOCATION:
								plvdi->item.pszText = pMemory->detail.pAllocationName;
								return 0L;
		  					
							case ID_DUMPDETAIL_CURRENTSIZE:
								plvdi->item.pszText = pMemory->detail.currentSizeBuff;
								return 0L;

							case ID_DUMPDETAIL_PEAKSIZE:
								plvdi->item.pszText = pMemory->detail.peakSizeBuff;
								return 0L;

							case ID_DUMPDETAIL_TOTALSIZE:
								plvdi->item.pszText = pMemory->detail.totalSizeBuff;
								return 0L;

							case ID_DUMPDETAIL_OVERHEAD:
								plvdi->item.pszText = pMemory->detail.overheadSizeBuff;
								return 0L;

							case ID_DUMPDETAIL_PEAKOVERHEAD:
								plvdi->item.pszText = pMemory->detail.peakOverheadSizeBuff;
								return 0L;

							case ID_DUMPDETAIL_TIME:
								plvdi->item.pszText = pMemory->detail.timeBuff;
								return 0L;

							case ID_DUMPDETAIL_CURRENTCOUNT:
								plvdi->item.pszText = pMemory->detail.currentCountBuff;
								return 0L;

							case ID_DUMPDETAIL_PEAKCOUNT:
								plvdi->item.pszText = pMemory->detail.peakCountBuff;
								return 0L;

							case ID_DUMPDETAIL_TOTALCOUNT:
								plvdi->item.pszText = pMemory->detail.totalCountBuff;
								return 0L;

							case ID_DUMPDETAIL_LTE16:
								plvdi->item.pszText = pMemory->detail.lte16Buff;
								return 0L;

							case ID_DUMPDETAIL_LTE32:
								plvdi->item.pszText = pMemory->detail.lte32Buff;
								return 0L;

							case ID_DUMPDETAIL_LTE128:
								plvdi->item.pszText = pMemory->detail.lte128Buff;
								return 0L;

							case ID_DUMPDETAIL_LTE1024:
								plvdi->item.pszText = pMemory->detail.lte1024Buff;
								return 0L;

							case ID_DUMPDETAIL_GT1024:
								plvdi->item.pszText = pMemory->detail.gt1024Buff;
								return 0L;

							default:
								break;
						}
					}
					break;
			}
			break;

        case WM_COMMAND:
            switch ( wID )
            {
				case IDM_OPTIONS_REFRESH:
					ShowMemDump_Refresh();
					return 0L;

				case IDM_OPTIONS_EXPORT:
					ShowMemDump_Export();
					return 0L;

				case IDM_OPTIONS_SUMMARY:
					ShowMemDump_Summary();
					return 0L;

				case IDM_OPTIONS_BYTES:
					g_showMemDump_showBytes = SHOW_BYTES;
					ShowMemDump_FormatItems();
					return 0L;

				case IDM_OPTIONS_KILOBYTES:
					g_showMemDump_showBytes = SHOW_KILOBYTES;
					ShowMemDump_FormatItems();
					return 0L;

				case IDM_OPTIONS_MEGABYTES:
					g_showMemDump_showBytes = SHOW_MEGABYTES;
					ShowMemDump_FormatItems();
					return 0L;

				case IDM_OPTIONS_COLLAPSEOUTPUT:
					g_showMemDump_bCollapseOutput = !g_showMemDump_bCollapseOutput;
					ShowMemDump_RefreshView();
					return 0L;
			}
			break;
	}	

	return ( DefWindowProc( hwnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
//	ShowMemDump_Init
// 
//-----------------------------------------------------------------------------
bool ShowMemDump_Init()
{
	// set up our window class
	WNDCLASS wndclass;

	memset( &wndclass, 0, sizeof( wndclass ) );
	wndclass.style         = 0;
	wndclass.lpfnWndProc   = ShowMemDump_WndProc;
	wndclass.cbClsExtra    = 0;
	wndclass.cbWndExtra    = 0;
	wndclass.hInstance     = g_hInstance;
	wndclass.hIcon         = g_hIcons[ICON_APPLICATION];
	wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wndclass.hbrBackground = g_hBackgroundBrush;
	wndclass.lpszMenuName  = MAKEINTRESOURCE( MENU_SHOWMEMORYDUMP );
	wndclass.lpszClassName = "SHOWMEMDUMPCLASS";
	if ( !RegisterClass( &wndclass ) )
		return false;

	ShowMemDump_LoadConfig();

	return true;
}

//-----------------------------------------------------------------------------
//	ShowMemDump_Open	
// 
//-----------------------------------------------------------------------------
void ShowMemDump_Open()
{
	RECT			clientRect;
	HWND			hWnd;

	if ( g_showMemDump_hWnd )
	{
		// only one instance
		if ( IsIconic( g_showMemDump_hWnd ) )
			ShowWindow( g_showMemDump_hWnd, SW_RESTORE );
		SetForegroundWindow( g_showMemDump_hWnd );
		return;
	}

	hWnd = CreateWindowEx( 
				WS_EX_CLIENTEDGE,
				"SHOWMEMDUMPCLASS",
				"",
				WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
				0,
				0,
				700,
				400,
				g_hDlgMain,
				NULL,
				g_hInstance,
				NULL );
	g_showMemDump_hWnd = hWnd;

	GetClientRect( g_showMemDump_hWnd, &clientRect ); 
	hWnd = CreateWindow( 
				WC_LISTVIEW, 
				"", 
				WS_VISIBLE|WS_CHILD|LVS_REPORT, 
				0, 
				0, 
				clientRect.right-clientRect.left,
				clientRect.bottom-clientRect.top, 
				g_showMemDump_hWnd,
				( HMENU )ID_SHOWMEMDUMP_LISTVIEW,
				g_hInstance,
				NULL ); 
	g_showMemDump_hWndListView = hWnd;

	ListView_SetBkColor( g_showMemDump_hWndListView, g_backgroundColor );
	ListView_SetTextBkColor( g_showMemDump_hWndListView, g_backgroundColor );

	DWORD style = LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyleEx( g_showMemDump_hWndListView, style, style );

	ShowMemDump_PopulateList();

	ShowMemDump_SetTitle();

	if ( g_showMemDump_windowRect.right && g_showMemDump_windowRect.bottom )
		MoveWindow( g_showMemDump_hWnd, g_showMemDump_windowRect.left, g_showMemDump_windowRect.top, g_showMemDump_windowRect.right-g_showMemDump_windowRect.left, g_showMemDump_windowRect.bottom-g_showMemDump_windowRect.top, FALSE );
	ShowWindow( g_showMemDump_hWnd, SHOW_OPENWINDOW );

	// get data from application
	ShowMemDump_Refresh();
}

//-----------------------------------------------------------------------------
//	MatchAllocationName
//
//-----------------------------------------------------------------------------
void MatchAllocationName( memory_t *&pMemory )
{
	memory_t *curEntry = g_showMemDump_pMemory;
	for ( int i=0; i<g_showMemDump_numMemory; ++i )
	{
		if ( !strcmp( curEntry->detail.pAllocationName, pMemory->detail.pAllocationName ) )
		{
			pMemory = curEntry;
			return;
		}
		++curEntry;
	}
}

//-----------------------------------------------------------------------------
//	ShowMemDump_Parse
// 
//-----------------------------------------------------------------------------
void ShowMemDump_Parse( const char *pBuffer, int fileSize )
{
	char		*ptr;
	char		*pToken;
	char		*pLineStart;
	int			numLines;
	int			size;
	memory_t	*pMemory;

	// remove old entries
	ShowMemDump_Clear();
	
	if ( !pBuffer || !fileSize )
	{
		// no valid data
		return;
	}

	Sys_SetScriptData( pBuffer, fileSize );

	// skip first line, column headers
	Sys_SkipRestOfLine();

	Sys_SaveParser();

	// count lines
	numLines = 0;
	while ( 1 )
	{
		pToken = Sys_GetToken( true );
		if ( !pToken || !pToken[0] )
			break;
		numLines++;
		Sys_SkipRestOfLine();
	}

	// each line represents one complete entry
	g_showMemDump_pMemory = (memory_t *)Sys_Alloc( numLines * sizeof( memory_t ) );

	Sys_RestoreParser();

	pMemory = g_showMemDump_pMemory;

	int format = 0;
	if ( !V_strnicmp( g_sys_scriptptr, "pool ", 5 ) )
	{
		// parse as pool stats
		format = FORMAT_POOLS;

		while ( 1 )
		{
			// find start of relevant data
			bool bFound = false;
			while ( 1 )
			{
				pToken = Sys_GetToken( true );
				if ( !pToken || !pToken[0] )
					break;
				if ( !V_stricmp( pToken, "size:" ) )
				{
					bFound = true;
					break;
				}
			}
			if ( !bFound )
			{
				break;
			}

			memset( pMemory, 0, sizeof( memory_t ) );

			pMemory->pool.pool = g_showMemDump_numMemory;

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->pool.size = atoi( pToken );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			if ( !V_stricmp( pToken, "allocated:" ) )
			{
				pToken = Sys_GetToken( false );
				if ( !pToken || !pToken[0] )
					break;
				pMemory->pool.allocated = atoi( pToken );
			}
			else
			{
				break;
			}

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			if ( !V_stricmp( pToken, "free:" ) )
			{
				pToken = Sys_GetToken( false );
				if ( !pToken || !pToken[0] )
					break;
				pMemory->pool.free = atoi( pToken );
			}
			else
			{
				break;
			}

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			if ( !V_stricmp( pToken, "committed:" ) )
			{
				pToken = Sys_GetToken( false );
				if ( !pToken || !pToken[0] )
					break;
				pMemory->pool.committed = atoi( pToken );
			}
			else
			{
				break;
			}

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			if ( !V_stricmp( pToken, "committedsize:" ) )
			{
				pToken = Sys_GetToken( false );
				if ( !pToken || !pToken[0] )
					break;
				pMemory->pool.committedSize = atoi( pToken );
			}
			else
			{
				break;
			}

			pMemory++;
			g_showMemDump_numMemory++;
			if ( g_showMemDump_numMemory >= numLines )
				break;
		}
	}
	else
	{
		// parse as detail stats
		format = FORMAT_DETAILS;

		while ( 1 )
		{
			pLineStart = g_sys_scriptptr;
		
			// can't tokenize, find start of parsable data
			ptr = V_stristr( g_sys_scriptptr, ", line " );
			if ( !ptr )
				break;
			g_sys_scriptptr = ptr + strlen( ", line " );

			// advance past the expected line number
			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;

			if ( !g_showMemDump_bCollapseOutput )
			{
				size = g_sys_scriptptr-pLineStart;
			}
			else
			{
				size = ptr-pLineStart;
			}

			memset( pMemory, 0, sizeof(memory_t) );
			memory_t *pCurRecordStart = pMemory;

			pMemory->detail.pAllocationName = new char[size+1];
			memcpy( pMemory->detail.pAllocationName, pLineStart, size );
			pMemory->detail.pAllocationName[size] = '\0';
			Sys_NormalizePath( pMemory->detail.pAllocationName, false );

			if ( g_showMemDump_bCollapseOutput )
			{
				MatchAllocationName( pMemory );
			}

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.currentSize += (int)(1024.0f*atof( pToken ));

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.peakSize += (int)(1024.0f*atof( pToken ));

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.totalSize += (int)(1024.0f*atof( pToken ));

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.overheadSize += (int)(1024.0f*atof( pToken ));

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.peakOverheadSize += (int)(1024.0f*atof( pToken ));

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.time += atoi( pToken );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.currentCount += atoi( pToken );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.peakCount += atoi( pToken );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.totalCount += atoi( pToken );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.lte16 += atoi( pToken );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.lte32 += atoi( pToken );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.lte128 += atoi( pToken );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.lte1024 += atoi( pToken );

			pToken = Sys_GetToken( false );
			if ( !pToken || !pToken[0] )
				break;
			pMemory->detail.gt1024 += atoi( pToken );

			Sys_SkipRestOfLine();

			if( pMemory == pCurRecordStart )
			{
				pMemory++;
				g_showMemDump_numMemory++;
			}
			else
			{
				pMemory = pCurRecordStart;
			}

			if ( g_showMemDump_numMemory >= numLines )
				break;
		}
	}

	if ( g_showMemDump_format != format )
	{
		// format change will cause list view change
		g_showMemDump_format = format;
		g_showMemDump_sortColumn = 0;
		g_showMemDump_sortDescending = 0;
	}

	ShowMemDump_PopulateList();
}

//-----------------------------------------------------------------------------
// rc_MemDump
//
// Sent from application with memory dump file
//-----------------------------------------------------------------------------
int rc_MemDump( char *pCommand )
{
	char*	pToken;
	int		retVal;
	int		errCode = -1;
	int		fileSize;
	char	*pBuffer;

	// get name of file
	pToken = GetToken( &pCommand );
	if ( !pToken[0] )
		goto cleanUp;

	// get file
	strcpy( g_showMemDump_currentFilename, pToken );
	retVal = LoadTargetFile( g_showMemDump_currentFilename, &fileSize, (void **)&pBuffer );

	DebugCommand( "0x%8.8x = MemDump( %s )\n", retVal, g_showMemDump_currentFilename );

	// parse and update view
	ShowMemDump_Parse( pBuffer, fileSize );

	Sys_Free( pBuffer );

	// success
	errCode = 0;

cleanUp:	
	return ( errCode );
}
