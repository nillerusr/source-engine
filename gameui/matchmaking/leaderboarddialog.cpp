//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays a leaderboard
//
//=============================================================================//

#include "leaderboarddialog.h"
#include "vgui_controls/Label.h"
#include "vgui/ILocalize.h"
#include "hl2orange.spa.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NUM_ROWS_PER_QUERY	100

CLeaderboardDialog *g_pLeaderboardDialog;

//----------------------------------------------------------
// CLeaderboardDialog
//----------------------------------------------------------
CLeaderboardDialog::CLeaderboardDialog( vgui::Panel *pParent ) : BaseClass( pParent, "LeaderboardDialog" )
{
	g_pLeaderboardDialog = this;
	m_iBaseRank = 0;
	m_iActiveRank = 0;
	m_iMaxRank = 0;
	m_cColumns = 0;
	m_iRangeBase = 0;
#if defined( _X360 )
	m_pStats = NULL;
#endif

	m_pProgressBg		= new vgui::Panel( this, "ProgressBg" );
	m_pProgressBar		= new vgui::Panel( this, "ProgressBar" );
	m_pProgressPercent	= new vgui::Label( this, "ProgressPercent", "" );
	m_pNumbering		= new vgui::Label( this, "Numbering", "" ); 
	m_pUpArrow			= new vgui::Label( this, "UpArrow", "" );
	m_pDownArrow		= new vgui::Label( this, "DownArrow", "" );
	m_pBestMoments		= new vgui::Label( this, "BestMoments", "" );
} 

CLeaderboardDialog::~CLeaderboardDialog()
{
	CleanupStats();

	delete m_pProgressBg;
	delete m_pProgressBar;
	delete m_pProgressPercent;
	delete m_pNumbering;
	delete m_pUpArrow;
	delete m_pDownArrow;
}

//----------------------------------------------------------
// Clean up the stats array
//----------------------------------------------------------
void CLeaderboardDialog::CleanupStats()
{
#if defined( _X360 )
	if ( m_pStats )
	{
		delete [] m_pStats;
		m_pStats = NULL;
	}
#endif
}

//----------------------------------------------------------
// Position the dialogs elements
//----------------------------------------------------------
void CLeaderboardDialog::PerformLayout( void )
{
	BaseClass::PerformLayout();

	if ( m_cColumns )
	{
		int x, y, wide, tall;
		m_pProgressBg->GetBounds( x, y, wide, tall );

		int columnWide = wide / m_cColumns;
		int lockedColumns = m_Menu.GetFirstUnlockedColumnIndex();
		int visibleColumns = m_Menu.GetVisibleColumnCount() - lockedColumns;
		int iColumn = m_Menu.GetActiveColumnIndex() - lockedColumns;

		if ( iColumn < 0 )
		{
			iColumn = 0;
		}
		else if ( iColumn < m_iRangeBase )
		{
			m_iRangeBase = iColumn;
		}
		else if ( iColumn >= m_iRangeBase + visibleColumns )
		{
			m_iRangeBase = iColumn - visibleColumns + 1;
		}

		m_pProgressBg->SetBounds( x, y, columnWide * m_cColumns, tall );
		m_pProgressBar->SetBounds( x + columnWide * m_iRangeBase, y, columnWide * visibleColumns, tall );
	}
	else
	{
		m_pProgressBg->SetVisible( false );
		m_pProgressBar->SetVisible( false );
	}

	int menux, menuy;
	m_Menu.GetPos( menux, menuy );

	// Do a perform layout on the menu so we get the correct height now
	m_Menu.InvalidateLayout( true, false );

	m_pNumbering->SizeToContents();

	wchar_t wszNumbering[64];
	wchar_t *wzNumberingFmt = g_pVGuiLocalize->Find( "#GameUI_Achievement_Menu_Range" );
	wchar_t wzActiveItem[8];
	wchar_t wzTotal[8];

	int iActive = m_iBaseRank + m_Menu.GetActiveItemIndex();
	if ( iActive < 0 )
	{
		iActive = 0;
	}
	V_snwprintf( wzActiveItem, ARRAYSIZE( wzActiveItem ), L"%d", iActive );
	V_snwprintf( wzTotal, ARRAYSIZE( wzTotal ), L"%d", m_iMaxRank );
	g_pVGuiLocalize->ConstructString( wszNumbering, sizeof( wszNumbering ), wzNumberingFmt, 2, wzActiveItem, wzTotal );
	m_pNumbering->SetText( wszNumbering );
	m_pNumbering->SetWide( GetWide() );

	MoveToCenterOfScreen();
}

//----------------------------------------------------------
// 
//----------------------------------------------------------
void CLeaderboardDialog::ApplySettings( KeyValues *pResourceData )
{
	BaseClass::ApplySettings( pResourceData );

	m_KeyRepeat.SetKeyRepeatTime( KEY_XBUTTON_DOWN, 0.05f );
	m_KeyRepeat.SetKeyRepeatTime( KEY_XSTICK1_DOWN, 0.05f );
	m_KeyRepeat.SetKeyRepeatTime( KEY_XBUTTON_UP, 0.05f );
	m_KeyRepeat.SetKeyRepeatTime( KEY_XSTICK1_UP, 0.05f );
}

//----------------------------------------------------------
// 
//----------------------------------------------------------
void CLeaderboardDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pProgressBg->SetBgColor( Color( 200, 184, 151, 255 ) );
	m_pProgressBar->SetBgColor( Color( 179, 82, 22, 255 ) );
	m_pNumbering->SetFgColor( pScheme->GetColor( "MatchmakingMenuItemDescriptionColor", Color( 64, 64, 64, 255 ) ) );
	m_pBestMoments->SetFgColor( pScheme->GetColor( "MatchmakingMenuItemDescriptionColor", Color( 64, 64, 64, 255 ) ) );
}

//----------------------------------------------------------
// 
//----------------------------------------------------------
void CLeaderboardDialog::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "CenterOnPlayer" ) )
	{
		if ( GetPlayerStats( -1 ) == 0 )
		{
			// Player isn't on the board, just start at rank 1
			GetPlayerStats( 1 );
		}
	}
	else if ( !Q_stricmp( pCommand, "Friends" ) )
	{
		GetPlayerStats( -1, true );
	}

	BaseClass::OnCommand( pCommand );
}

//----------------------------------------------------------
// 
//----------------------------------------------------------
void CLeaderboardDialog::AddLeaderboardEntry( const char **ppEntries, int ct )
{
	m_Menu.AddSectionedItem( ppEntries, ct );
}

//----------------------------------------------------------
// Get some portion of the leaderboard. This should ideally live
// in the client, since it's very mod-specific
//----------------------------------------------------------
bool CLeaderboardDialog::GetPlayerStats( int rank, bool bFriends )
{
#if defined _X360
	HANDLE handle;

	// Retrieve the necessary buffer size
	DWORD cbResults = 0;

	bool bRanked = false;
	const char *pName = GetName();
	if ( !Q_stricmp( pName, "LeaderboardDialog_Ranked" ) )
	{
		bRanked = true;
	}

	XUSER_STATS_SPEC spec;
	if ( !bRanked )
	{
		spec.dwViewId        = STATS_VIEW_PLAYER_MAX_UNRANKED;
		spec.dwNumColumnIds  = 15;
		spec.rgwColumnIds[0] = STATS_COLUMN_PLAYER_MAX_UNRANKED_POINTS_SCORED;
		spec.rgwColumnIds[1] = STATS_COLUMN_PLAYER_MAX_UNRANKED_KILLS;
		spec.rgwColumnIds[2] = STATS_COLUMN_PLAYER_MAX_UNRANKED_POINTS_CAPPED;
		spec.rgwColumnIds[3] = STATS_COLUMN_PLAYER_MAX_UNRANKED_POINT_DEFENSES;
 		spec.rgwColumnIds[4] = STATS_COLUMN_PLAYER_MAX_UNRANKED_DOMINATIONS;
 		spec.rgwColumnIds[5] = STATS_COLUMN_PLAYER_MAX_UNRANKED_REVENGE;
		spec.rgwColumnIds[6] = STATS_COLUMN_PLAYER_MAX_UNRANKED_BUILDINGS_DESTROYED;
		spec.rgwColumnIds[7] = STATS_COLUMN_PLAYER_MAX_UNRANKED_HEADSHOTS;
		spec.rgwColumnIds[8] = STATS_COLUMN_PLAYER_MAX_UNRANKED_HEALTH_POINTS_HEALED;
		spec.rgwColumnIds[9] = STATS_COLUMN_PLAYER_MAX_UNRANKED_INVULNS;
		spec.rgwColumnIds[10] = STATS_COLUMN_PLAYER_MAX_UNRANKED_KILL_ASSISTS;
		spec.rgwColumnIds[11] = STATS_COLUMN_PLAYER_MAX_UNRANKED_BACKSTABS;
		spec.rgwColumnIds[12] = STATS_COLUMN_PLAYER_MAX_UNRANKED_HEALTH_POINTS_LEACHED;
		spec.rgwColumnIds[13] = STATS_COLUMN_PLAYER_MAX_UNRANKED_SENTRY_KILLS;
		spec.rgwColumnIds[14] = STATS_COLUMN_PLAYER_MAX_UNRANKED_TELEPORTS;
		m_cColumns = 15;
	}
	else
	{
		spec.dwViewId          = STATS_VIEW_PLAYER_MAX_RANKED;
		spec.dwNumColumnIds    = 1;
		spec.rgwColumnIds[ 0 ] = STATS_COLUMN_PLAYER_MAX_RANKED_POINTS_SCORED;

		// set to zero to hide the progress bar
		m_cColumns = 0;
	}

	DWORD ret;
	XUID xuid = 0u;
	XUID xuidFriends[NUM_ROWS_PER_QUERY];
	int xuidCount = 1;

	if ( !bFriends )
	{
		if ( rank == -1 )
		{
			// Center on the player's xuid
			XUserGetXUID( XBX_GetPrimaryUserId(), &xuid );

			ret = XUserCreateStatsEnumeratorByXuid( 
				0,
				xuid,
				NUM_ROWS_PER_QUERY,
				1,
				&spec,
				&cbResults,
				&handle );
		}
		else
		{
			// Start at the requested rank
			ret = XUserCreateStatsEnumeratorByRank( 
				0,
				rank,
				NUM_ROWS_PER_QUERY,
				1,
				&spec,
				&cbResults,
				&handle );
		}

		if( ret != ERROR_SUCCESS )
		{
			Warning( "Error getting stats\n" );
			return false;
		}

		// Allocate the buffer
		CleanupStats();
		m_pStats = ( XUSER_STATS_READ_RESULTS* ) new char[cbResults];

		DWORD cpReturned;
		ret = XEnumerate( handle, m_pStats, cbResults, &cpReturned, NULL );
	}
	else
	{
		// Get Friends leaderboard
		int id = XBX_GetPrimaryUserId();
		ret = XFriendsCreateEnumerator( id, 0, 5, &cbResults, &handle );

		if ( ret != ERROR_SUCCESS )
		{
			Warning( "Error getting friends list\n" );
			return false;
		}

		// Allocate the buffer
		XONLINE_FRIEND *pFriends = ( XONLINE_FRIEND* ) new char[cbResults];

		DWORD cpReturned;
		ret = XEnumerate( handle, pFriends, cbResults, &cpReturned, NULL );
		if( ret != ERROR_SUCCESS )
		{
			delete pFriends;
			return false;
		}

		for ( uint i = 0; i < cpReturned; ++i )
		{
			xuidFriends[i] = pFriends[i].xuid;
		}

		// Allocate the buffer
		CleanupStats();
		m_pStats = ( XUSER_STATS_READ_RESULTS* ) new char[cbResults];

		ret = XUserReadStats( 0, xuidCount, xuidFriends, 1, &spec, &cbResults, m_pStats, NULL );
	}

	if( ret == ERROR_SUCCESS )
	{
		const char *pEntries[32];
		char pRowBuffer[MAX_PATH];
		char pBuffers[32][MAX_PATH];

		m_Menu.ClearItems();
		m_iMaxRank = m_pStats->pViews[0].dwTotalViewRows;

		// Did this search return any rows?
		if ( m_pStats->pViews[0].dwNumRows == 0 )
			return false;

		for ( uint i = 0; i < m_pStats->pViews[0].dwNumRows; ++i )
		{
			XUSER_STATS_ROW &row = m_pStats->pViews[0].pRows[i];

			// Save off the first rank in this set of entries
			if ( i == 0 && m_iBaseRank == 0 )
			{
				m_iBaseRank = row.dwRank;
			}

			pEntries[0] = itoa( row.dwRank, pRowBuffer, 10 );
			pEntries[1] = row.szGamertag;
			for ( uint j = 0; j < row.dwNumColumns; ++j )
			{
				XUSER_STATS_COLUMN &col = m_pStats->pViews[0].pRows[i].pColumns[j];
				pEntries[j+2] = itoa( col.Value.nData, pBuffers[j], 10 );
			}

			AddLeaderboardEntry( pEntries, row.dwNumColumns + 2 );

			if ( rank == -1 && row.xuid == xuid )
			{
				m_Menu.SetFocus( i );
				m_iActiveRank = row.dwRank;
			}
		}
	}
	else
	{
		Warning( "Error getting leaderboard stats\n" );
		return false;
	}

	CloseHandle( handle );

	return true;
#endif

	return false;
}

//----------------------------------------------------------
// Determine if a new set of stats needs to be downloaded
// Return true if the update has been handled, false otherwise
//----------------------------------------------------------
void CLeaderboardDialog::UpdateLeaderboard( int iNewRank )
{
	// Clamp the input
	if ( iNewRank < 1 )
	{
		iNewRank = 1;
	}
	else if ( iNewRank > m_iMaxRank )
	{
		iNewRank = m_iMaxRank;
	}

	// No action necessary?
	if ( iNewRank == m_iActiveRank )
		return;

	int nInterval = iNewRank - m_iActiveRank;
	int iNewActiveItemIndex = m_Menu.GetActiveItemIndex() + nInterval;

	// Set these "new" values to the current values - they will be conditionally updated.
	int iNewBaseRank = m_iBaseRank;
	int iNewBaseItemIndex = m_Menu.GetBaseRowIndex();
	int nVisibleItems = m_Menu.GetVisibleItemCount();
	int nHiddenItems = NUM_ROWS_PER_QUERY - nVisibleItems;

	// Are we outside the visible range of the menu?
	if ( iNewActiveItemIndex < iNewBaseItemIndex )
	{
		// Do we need to grab another set of columns?
		if ( iNewRank < m_iBaseRank )
		{
			iNewBaseRank = iNewRank - nHiddenItems;
			if ( iNewBaseRank < 1 )
			{
				iNewBaseRank = 1;
			}

			if ( !GetPlayerStats( iNewBaseRank ) )
			{
				// Failed to load player stats, don't change the current index
				return;
			}

			m_iBaseRank = iNewBaseRank;
		}

		int nBaseToActiveInterval = iNewRank - m_iBaseRank;

		// Since we shifted the menu down, both base and active item are at the first visible menu item
		iNewActiveItemIndex = nBaseToActiveInterval;
		iNewBaseItemIndex = nBaseToActiveInterval;
	}
 	else if ( iNewActiveItemIndex >= m_Menu.GetBaseRowIndex() + nVisibleItems )
 	{
		int nHiddenItems = NUM_ROWS_PER_QUERY - nVisibleItems;
		int iTopRank = iNewRank + nHiddenItems;
		if ( iTopRank > m_iMaxRank )
		{
			iTopRank = m_iMaxRank;
		}


		// Do we need to grab another set of columns?
		if ( iNewRank >= m_iBaseRank + NUM_ROWS_PER_QUERY )
		{
			iNewBaseRank = iTopRank - NUM_ROWS_PER_QUERY + 1;
			if ( !GetPlayerStats( iNewBaseRank ) )
			{
				// Failed to load player stats, don't change the current index
				return;
			}
			m_iBaseRank = iNewBaseRank;
		}

		int nBaseToActiveInterval = iNewRank - m_iBaseRank;

		iNewActiveItemIndex = nBaseToActiveInterval;
		iNewBaseItemIndex = iNewActiveItemIndex - nVisibleItems + 1;
 	}

	// Set all the new variables - must set base index before active index.
	m_iActiveRank = iNewRank;
	m_Menu.SetBaseRowIndex( iNewBaseItemIndex );
	m_Menu.SetFocus( iNewActiveItemIndex );

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLeaderboardDialog::HandleKeyRepeated( vgui::KeyCode code )
{
	OnKeyCodePressed( code );
}

//-----------------------------------------------------------------
// Purpose: Send key presses to the dialog's menu
//-----------------------------------------------------------------
void CLeaderboardDialog::OnKeyCodePressed( vgui::KeyCode code )
{

	switch( code )
	{
	case KEY_XBUTTON_A:
	case STEAMCONTROLLER_A:
#ifdef _X360
		{
			int idx = m_Menu.GetActiveItemIndex();
			if ( m_pStats && idx < (int)m_pStats->pViews[0].dwNumRows )
			{
				XUSER_STATS_ROW &row = m_pStats->pViews[0].pRows[idx];
				XShowGamerCardUI( XBX_GetPrimaryUserId(), row.xuid );		
			}
		}
#endif
		break;

	case KEY_XBUTTON_Y:
	case STEAMCONTROLLER_Y:
		break;

	case KEY_XSTICK1_DOWN:
	case KEY_XBUTTON_DOWN:
	case STEAMCONTROLLER_DPAD_DOWN:
		m_KeyRepeat.KeyDown( code );
		UpdateLeaderboard( m_iActiveRank + 1 );
		break;

	case KEY_XSTICK1_UP:
	case KEY_XBUTTON_UP:
	case STEAMCONTROLLER_DPAD_UP:
		m_KeyRepeat.KeyDown( code );
		UpdateLeaderboard( m_iActiveRank - 1 );
		break;

	case KEY_XBUTTON_LEFT_SHOULDER:
		UpdateLeaderboard( 1 );
		break;

	case KEY_XBUTTON_RIGHT_SHOULDER:
		OnCommand( "CenterOnPlayer" );
		break;

		// Disabled until friends enumeration works
// 	case KEY_XBUTTON_RIGHT_SHOULDER:
// 		OnCommand( "Friends" );
// 		break;

	default:
		m_KeyRepeat.KeyDown( code );
		BaseClass::OnKeyCodePressed( code );
		break;
	}

	// Invalidate layout when scrolling through columns
	switch( code )
	{
	case KEY_XSTICK1_LEFT:
	case KEY_XBUTTON_LEFT:
	case STEAMCONTROLLER_DPAD_LEFT:
	case KEY_XSTICK1_RIGHT:
	case KEY_XBUTTON_RIGHT:
	case STEAMCONTROLLER_DPAD_RIGHT:
		InvalidateLayout();
		break;
	}
}


CON_COMMAND( mm_add_item, "Add a stats item" )
{
	if ( args.ArgC() > 1 )
	{
		int ct = atoi( args[1] );
		const char *pEntries[32];
		for ( int i = 0; i < ct; ++i )
		{
			pEntries[i] = args[i+2];
		}
		g_pLeaderboardDialog->AddLeaderboardEntry( pEntries, ct );
	}
}
