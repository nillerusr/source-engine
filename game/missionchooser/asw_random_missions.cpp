#include "asw_random_missions.h"
#include "MapLayout.h"
#include "LevelTheme.h"
#include "TileGenDialog.h"
#include "filesystem.h"
#include "Room.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_Random_Missions::CASW_Random_Missions() :
m_pCurrentMapLayout( NULL ),
m_bDirtyLayoutForMinimap( false )
{
}

CASW_Random_Missions::~CASW_Random_Missions()
{
	delete m_pCurrentMapLayout;
}

vgui::Panel* CASW_Random_Missions::CreateTileGenFrame( vgui::Panel *parent )
{
	if ( g_pTileGenDialog == NULL )
	{
		// add our main window
		new CTileGenDialog( parent, "TileGenDialog" );
	}

	return g_pTileGenDialog;
}

void CASW_Random_Missions::LevelInitPostEntity( const char *pszMapName )
{
	delete m_pCurrentMapLayout;
	
	// try to load the layout for this map
	char filename[128];
	
	Q_snprintf( filename, sizeof( filename ), "maps/%s", pszMapName );
	Q_SetExtension( filename, "layout", sizeof( filename ) );
	
	char fullpath[MAX_PATH];
	g_pFullFileSystem->RelativePathToFullPath( filename, "GAME", fullpath, sizeof( fullpath ) );

	if ( g_pFullFileSystem->FileExists( filename ) )
	{
		CLevelTheme::LoadLevelThemes();
	
		m_pCurrentMapLayout = new CMapLayout();
		if ( !m_pCurrentMapLayout->LoadMapLayout( filename ) )
		{
			delete m_pCurrentMapLayout;
			m_pCurrentMapLayout = NULL;
		}
	}
	else
	{
		m_pCurrentMapLayout = NULL;
	}

	m_bDirtyLayoutForMinimap = true;
}

bool CASW_Random_Missions::ValidMapLayout()
{
	return m_pCurrentMapLayout != NULL;
}

KeyValues* CASW_Random_Missions::GetGenerationOptions()
{
	if ( !m_pCurrentMapLayout || !m_pCurrentMapLayout->GetGenerationOptions() )
		return NULL;

	return m_pCurrentMapLayout->GetGenerationOptions();
}

IASW_Room_Details* CASW_Random_Missions::GetRoomDetails( const Vector &vecPos )
{
	if ( !m_pCurrentMapLayout )
		return NULL;

	// convert vecPos to a tile
	int half_map_size = MAP_LAYOUT_TILES_WIDE * 0.5f;		// shift back so the middle of our grid is the origin
	int iTileX = vecPos.x / ASW_TILE_SIZE + half_map_size;
	int iTileY = vecPos.y / ASW_TILE_SIZE + half_map_size;
	
	if ( iTileX < 0 || iTileY < 0 || iTileX >= MAP_LAYOUT_TILES_WIDE || iTileY >= MAP_LAYOUT_TILES_WIDE )
		return NULL;

	return (IASW_Room_Details*) m_pCurrentMapLayout->m_pRoomGrid[ iTileX ][ iTileY ];
}

IASW_Room_Details* CASW_Random_Missions::GetStartRoomDetails()
{
	if ( m_pCurrentMapLayout == NULL )
		return NULL;

	int iRooms = m_pCurrentMapLayout->m_PlacedRooms.Count();
	for ( int i = 0; i < iRooms; i++ )
	{
		CRoom *pRoom = m_pCurrentMapLayout->m_PlacedRooms[ i ];
		if ( !pRoom || !pRoom->m_pRoomTemplate->IsStartRoom() )
			continue;

		return (IASW_Room_Details*) pRoom;
	}
	return NULL;
}

IASW_Room_Details* CASW_Random_Missions::GetRoomDetails( int iRoomIndex )
{
	if ( iRoomIndex < 0 || iRoomIndex >= m_pCurrentMapLayout->m_PlacedRooms.Count() )
		return NULL;

	return (IASW_Room_Details*) m_pCurrentMapLayout->m_PlacedRooms[ iRoomIndex ];
}

int CASW_Random_Missions::GetNumRooms()
{
	return m_pCurrentMapLayout->m_PlacedRooms.Count();
}

int	CASW_Random_Missions::GetNumEncounters()
{
	return m_pCurrentMapLayout->m_Encounters.Count();
}

IASW_Encounter* CASW_Random_Missions::GetEncounter( int i )
{
	if ( i < 0 || i >= m_pCurrentMapLayout->m_Encounters.Count() )
		return NULL;

	return (IASW_Encounter*) m_pCurrentMapLayout->m_Encounters[ i ];
}

void CASW_Random_Missions::GetMapBounds( Vector *vecWorldMins, Vector *vecWorldMaxs )
{
	if ( !m_pCurrentMapLayout )
		return;
	
	// find world bounds
	vecWorldMins->x = 65535;
	vecWorldMins->y = 65535;
	vecWorldMins->z = 65535;
	vecWorldMaxs->x = -65535;
	vecWorldMaxs->y = -65535;
	vecWorldMaxs->z = -65535;

	Vector vecRoomMins = vec3_origin, vecRoomMaxs = vec3_origin;
	int iNumRooms = GetNumRooms();
	for ( int i = 0 ; i < iNumRooms ; i++ )
	{
		IASW_Room_Details* pRoom = GetRoomDetails( i );
		if ( !pRoom )
			continue;

		pRoom->GetWorldBounds( &vecRoomMins, &vecRoomMaxs );
		vecWorldMins->x = MIN( vecWorldMins->x, vecRoomMins.x );
		vecWorldMins->y = MIN( vecWorldMins->y, vecRoomMins.y );
		vecWorldMins->z = MIN( vecWorldMins->z, vecRoomMins.z );
		vecWorldMaxs->x = MAX( vecWorldMaxs->x, vecRoomMaxs.x );
		vecWorldMaxs->y = MAX( vecWorldMaxs->y, vecRoomMaxs.y );
		vecWorldMaxs->z = MAX( vecWorldMaxs->z, vecRoomMaxs.z );
	}
}

bool CASW_Random_Missions::CheckAndCleanDirtyLayout( void )
{
	bool bDirty = m_bDirtyLayoutForMinimap;
	m_bDirtyLayoutForMinimap = false;

	return bDirty;
}