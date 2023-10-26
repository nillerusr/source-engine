#include "MapLayout.h"
#include "Room.h"
#include "ChunkFile.h"
#include "RoomTemplate.h"
#include "KeyValues.h"
#include "LevelTheme.h"
#include "asw_spawn_selection.h"
#include "asw_mission_chooser.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CInstanceSpawn::CInstanceSpawn() :
m_InstanceSpawningMethod( ISM_INVALID ),
m_nPlacedRoomIndex( -1 )
{
	m_InstanceFilename[0] = '\0';
}

void CInstanceSpawn::FixupValues( const char *pFindValue, const char *pReplaceValue )
{
	for ( int i = 0; i < m_AdditionalKeyValues.Count(); ++ i )
	{
		if ( Q_stristr( m_AdditionalKeyValues[i].m_Value, pFindValue ) != NULL )
		{
			char outputString[MAX_TILEGEN_IDENTIFIER_LENGTH];
			Q_StrSubst( m_AdditionalKeyValues[i].m_Value, pFindValue, pReplaceValue, outputString, MAX_TILEGEN_IDENTIFIER_LENGTH );
			Q_strncpy( m_AdditionalKeyValues[i].m_Value, outputString, MAX_TILEGEN_IDENTIFIER_LENGTH );
		}
	}
}

bool CInstanceSpawn::LoadFromKeyValues( KeyValues *pKeyValues )
{
	const char *pFilename = pKeyValues->GetString( "filename", NULL );
	if ( pFilename == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "No 'filename' specified in CInstanceSpawn block.\n" );
		return false;
	}
	Q_strncpy( m_InstanceFilename, pFilename, _countof( m_InstanceFilename ) );

	const char *pType = pKeyValues->GetString( "type", NULL );
	if ( pType == NULL )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "No 'type' specified in CInstanceSpawn block.\n" );
		return false;
	}
	else
	{
		if ( Q_stricmp( pType, "add_at_random_node" ) == 0 )
		{
			m_InstanceSpawningMethod = ISM_ADD_AT_RANDOM_NODE;
		}
		else
		{
			Log_Warning( LOG_TilegenLayoutSystem, "Invalid 'type' specified in CInstanceSpaw block.\n" );
			return false;
		}
	}

	m_nPlacedRoomIndex = pKeyValues->GetInt( "room_index", -1 );
	m_nRandomSeed = pKeyValues->GetInt( "random_seed", 0 );

	for ( KeyValues *pReplaceKV = pKeyValues->GetFirstSubKey(); pReplaceKV != NULL; pReplaceKV = pReplaceKV->GetNextKey() )
	{
		if ( Q_stricmp( pReplaceKV->GetName(), "key_value" ) == 0 )
		{
			const char *pKey = pReplaceKV->GetString( "key", "" );
			const char *pValue = pReplaceKV->GetString( "value", "" );

			int nIndex = m_AdditionalKeyValues.AddToTail();
			Q_strncpy( m_AdditionalKeyValues[nIndex].m_Key, pKey, MAX_TILEGEN_IDENTIFIER_LENGTH );
			Q_strncpy( m_AdditionalKeyValues[nIndex].m_Value, pValue, MAX_TILEGEN_IDENTIFIER_LENGTH );
		}
	}

	return true;
}

void CInstanceSpawn::SaveToKeyValues( KeyValues *pKeyValues ) const
{
	pKeyValues->SetString( "filename", m_InstanceFilename );
	switch ( m_InstanceSpawningMethod )
	{
	case ISM_ADD_AT_RANDOM_NODE:
		pKeyValues->SetString( "type", "add_at_random_node" );
		break;
	default:
		Log_Warning( LOG_TilegenLayoutSystem, "Invalid instance spawning method (%d).\n", m_InstanceSpawningMethod );
	}
	
	pKeyValues->SetInt( "room_index", m_nPlacedRoomIndex );
	pKeyValues->SetInt( "random_seed", m_nRandomSeed );

	for ( int i = 0; i < m_AdditionalKeyValues.Count(); ++ i )
	{
		KeyValues *pNewReplacement = new KeyValues( "key_value" );
		pNewReplacement->SetString( "key", m_AdditionalKeyValues[i].m_Key );
		pNewReplacement->SetString( "value", m_AdditionalKeyValues[i].m_Value );
		pKeyValues->AddSubKey( pNewReplacement );
	}
}

CMapLayout::CMapLayout( KeyValues *pGenerationOptions ) :
m_pGenerationOptions( pGenerationOptions )
{
	SetCurrentFilename("");

	m_iPlayerStartTileX = MAP_LAYOUT_TILES_WIDE * 0.5f;
	m_iPlayerStartTileY = MAP_LAYOUT_TILES_WIDE * 0.5f;

	// clear our pointer grid
	for (int y=0;y<MAP_LAYOUT_TILES_WIDE;y++)
	{
		for (int x=0;x<MAP_LAYOUT_TILES_WIDE;x++)
		{
			m_pRoomGrid[x][y] = NULL;
		}
	}
}

CMapLayout::~CMapLayout()
{
	// todo: clear any 'currently selected rooms', etc
	Clear();

	if ( m_pGenerationOptions != NULL )
	{
		m_pGenerationOptions->deleteThis();
	}
}

void CMapLayout::Clear()
{
	// delete all placed rooms
	int iRooms = m_PlacedRooms.Count();
	for (int i=iRooms-1;i>=0;i--)
	{
		CRoom *pRoom = m_PlacedRooms[i];
		delete pRoom;	// destructor removes it from the list
	}
	m_LogicalRooms.RemoveAll();
	m_InstanceSpawns.RemoveAll();
	m_Encounters.PurgeAndDeleteElements();

	// wipe the grid
	for (int x=0;x<MAP_LAYOUT_TILES_WIDE;x++)
	{
		for (int y=0;y<MAP_LAYOUT_TILES_WIDE;y++)
		{
			m_pRoomGrid[x][y] = NULL;
		}
	}

	m_iPlayerStartTileX = MAP_LAYOUT_TILES_WIDE * 0.5f;
	m_iPlayerStartTileY = MAP_LAYOUT_TILES_WIDE * 0.5f;
}

void CMapLayout::SetGenerationOptions( KeyValues *pNewGenerationOptions )
{
	if ( m_pGenerationOptions )
	{
		m_pGenerationOptions->deleteThis();
	}

	m_pGenerationOptions = pNewGenerationOptions;
}

bool CMapLayout::SaveMapLayout( const char *filename )
{
	KeyValues *pLayoutKeys = new KeyValues( "Layout" );

	if ( m_pGenerationOptions )
	{
		pLayoutKeys->AddSubKey( m_pGenerationOptions->MakeCopy() );
	}

	KeyValues *pMiscKeys = new KeyValues( "mapmisc" );
	pMiscKeys->SetInt( "PlayerStartX", m_iPlayerStartTileX );
	pMiscKeys->SetInt( "PlayerStartY", m_iPlayerStartTileY );
	pLayoutKeys->AddSubKey( pMiscKeys );
	
	// save out each room
	int iRooms = m_PlacedRooms.Count();
	for (int i=0;i<iRooms;i++)
	{
		CRoom *pRoom = m_PlacedRooms[i];
		if (!pRoom)
			continue;

		pLayoutKeys->AddSubKey( pRoom->GetKeyValuesCopy() );
	}
	// save out logical rooms
	iRooms = m_LogicalRooms.Count();
	for (int i=0;i<iRooms;i++)
	{
		KeyValues *pRoomKeys = new KeyValues( "logical_room" );
		if ( m_LogicalRooms[i]->m_pLevelTheme )
		{
			pRoomKeys->SetString( "theme", m_LogicalRooms[i]->m_pLevelTheme->m_szName );
		}
		pRoomKeys->SetString( "template", m_LogicalRooms[i]->GetFullName() );
		pLayoutKeys->AddSubKey( pRoomKeys );
	}

	for ( int i = 0; i < m_InstanceSpawns.Count(); ++ i )
	{
		KeyValues *pNewInstanceSpawn = new KeyValues( "instance_spawn" );
		m_InstanceSpawns[i].SaveToKeyValues( pNewInstanceSpawn );
		pLayoutKeys->AddSubKey( pNewInstanceSpawn );
	}
	for ( int i = 0; i < m_Encounters.Count(); ++ i )
	{
		KeyValues *pEncounterKeys = new KeyValues( "npc_encounter" );
		m_Encounters[i]->SaveToKeyValues( pEncounterKeys );
		pLayoutKeys->AddSubKey( pEncounterKeys );
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	for ( KeyValues *pKey = pLayoutKeys->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		pKey->RecursiveSaveToFile( buf, 0 );			
	}
	pLayoutKeys->deleteThis();
	if ( !g_pFullFileSystem->WriteFile( filename, "GAME", buf ) )
	{
		Log_Warning( LOG_TilegenLayoutSystem, "Failed to SaveToFile %s\n", filename );
		return false;
	}

	return true;
}

bool CMapLayout::LoadLogicalRoom( KeyValues *pRoomKeys )
{
	char szLoadingTheme[256];
	char szLoadingTemplate[256];
	szLoadingTheme[0] = '\0';
	szLoadingTemplate[0] = '\0';

	Q_snprintf( szLoadingTheme, sizeof(szLoadingTheme), "%s", pRoomKeys->GetString( "theme" ) );		
	Q_snprintf( szLoadingTemplate, sizeof(szLoadingTemplate), "%s", pRoomKeys->GetString( "template" ) );

	// find pointer to the room template
	// first find the theme with matching name
	int iThemes = CLevelTheme::s_LevelThemes.Count();
	for (int i=0;i<iThemes;i++)
	{
		CLevelTheme* pTheme = CLevelTheme::s_LevelThemes[i];
		if (!pTheme)
			continue;
		if ( !Q_stricmp( pTheme->m_szName, szLoadingTheme ) )
		{
			// now find a template within that theme with our template's name
			int iTemplates = pTheme->m_RoomTemplates.Count();
			for (int t=0;t<iTemplates;t++)
			{
				if ( !Q_stricmp( pTheme->m_RoomTemplates[t]->GetFullName(), szLoadingTemplate ) )
				{
					m_LogicalRooms.AddToTail( pTheme->m_RoomTemplates[t] );
					return true;
				}
			}
			break;
		}
	}
	return false;
}

bool CMapLayout::LoadMapLayout(const char *filename)
{
	// open our output layout file
	KeyValues *pLayoutKeys = new KeyValues( "LayoutKeys" );
	if ( !pLayoutKeys->LoadFromFile( g_pFullFileSystem, filename, "GAME" ) )
	{
		//Msg( "Failed to load map layout %s\n", filename );
		pLayoutKeys->deleteThis();
		return false;
	}

	while ( pLayoutKeys )
	{
		if ( !Q_stricmp( pLayoutKeys->GetName(), "room" ) )
		{
			if ( !CRoom::LoadRoomFromKeyValues( pLayoutKeys, this ) )
				return false;
		}
		else if ( !Q_stricmp( pLayoutKeys->GetName(), "logical_room" ) )
		{
			LoadLogicalRoom( pLayoutKeys );
		}
		else if ( !Q_stricmp( pLayoutKeys->GetName(), "mapmisc" ) )
		{
			m_iPlayerStartTileX = pLayoutKeys->GetInt( "PlayerStartX" );
			m_iPlayerStartTileY = pLayoutKeys->GetInt( "PlayerStartY" );
		}
		else if ( Q_stricmp( pLayoutKeys->GetName(), "mission_settings" ) == 0 )
		{
			m_pGenerationOptions = pLayoutKeys->MakeCopy();
		}
		else if ( Q_stricmp( pLayoutKeys->GetName(), "instance_spawn" ) == 0 )
		{
			m_InstanceSpawns[m_InstanceSpawns.AddToTail()].LoadFromKeyValues( pLayoutKeys );
		}
		else if ( Q_stricmp( pLayoutKeys->GetName(), "npc_encounter" ) == 0 )
		{
			CASW_Encounter *pEncounter = new CASW_Encounter();
			pEncounter->LoadFromKeyValues( pLayoutKeys );
			m_Encounters.AddToTail( pEncounter );
		}
		pLayoutKeys = pLayoutKeys->GetNextKey();
	}

	MarkEncounterRooms();

	SetCurrentFilename(filename);

	pLayoutKeys->deleteThis();

	return true;
}

void CMapLayout::MarkEncounterRooms()
{
	// mark all rooms with alien encounters
	for ( int i = 0; i < m_Encounters.Count(); i++ )
	{
		CRoom *pRoom = GetRoom( m_Encounters[i]->GetEncounterPosition() );
		if ( pRoom )
		{
			pRoom->SetHasAlienEncounter( true );
		}
	}
}

void CMapLayout::SetCurrentFilename(const char *szFilename)
{
	Q_snprintf(m_szFilename, sizeof(m_szFilename), "%s", szFilename);
}

CRoom* CMapLayout::GetRoom( const Vector &vecPos )
{
	int nX = ( vecPos.x / ASW_TILE_SIZE ) + MAP_LAYOUT_TILES_WIDE * 0.5f;
	int nY = ( vecPos.y / ASW_TILE_SIZE ) + MAP_LAYOUT_TILES_WIDE * 0.5f;
	if ( nX >= 0 && nX < MAP_LAYOUT_TILES_WIDE && nY >= 0 && nY < MAP_LAYOUT_TILES_WIDE )
	{
		return GetRoom( nX, nY );
	}
	return NULL;
}

void CMapLayout::GetExtents(int &iTileX_Min, int &iTileX_Max, int &iTileY_Min, int &iTileY_Max)
{
	iTileX_Min = MAP_LAYOUT_TILES_WIDE-1;
	iTileY_Min = MAP_LAYOUT_TILES_WIDE-1;
	iTileX_Max = 0;
	iTileY_Max = 0;

	for (int i=0;i<m_PlacedRooms.Count();i++)
	{
		CRoom* pRoom = m_PlacedRooms[i];
		if (!pRoom)
			continue;

		if (pRoom->m_iPosX < iTileX_Min)
			iTileX_Min = pRoom->m_iPosX;
		if (pRoom->m_iPosY < iTileY_Min)
			iTileY_Min = pRoom->m_iPosY;
		if (pRoom->m_iPosX + pRoom->m_pRoomTemplate->GetTilesX() > iTileX_Max)
			iTileX_Max = pRoom->m_iPosX + pRoom->m_pRoomTemplate->GetTilesX();
		if (pRoom->m_iPosY + pRoom->m_pRoomTemplate->GetTilesY() > iTileY_Max)
			iTileY_Max = pRoom->m_iPosY + pRoom->m_pRoomTemplate->GetTilesY();
	}

	// don't return out of bounds
	if ( iTileX_Min < 0 )
		iTileX_Min = 0;
	if ( iTileY_Min < 0 )
		iTileY_Min = 0;
	if ( iTileX_Max >= MAP_LAYOUT_TILES_WIDE )
		iTileX_Max = MAP_LAYOUT_TILES_WIDE - 1;
	if ( iTileY_Max >= MAP_LAYOUT_TILES_WIDE )
		iTileY_Max = MAP_LAYOUT_TILES_WIDE - 1;
}

bool CMapLayout::SaveMiscMapProperties(CChunkFile *pFile)
{	
	if (!pFile)
		return false;

	ChunkFileResult_t eResult = pFile->BeginChunk("mapmisc");
	if (eResult != ChunkFile_Ok)
		return false;

	// write out the player start location
	if (pFile->WriteKeyValueInt("PlayerStartX", m_iPlayerStartTileX) != ChunkFile_Ok)
		return false;
	if (pFile->WriteKeyValueInt("PlayerStartY", m_iPlayerStartTileY) != ChunkFile_Ok)
		return false;

	if (pFile->EndChunk() != ChunkFile_Ok)
		return false;

	return true;
}

bool CMapLayout::RoomsOverlap( int x, int y, int w, int h,
								int x2, int y2, int w2, int h2 ) const
{
	if ( y + h <= y2 )
		return false;
	if ( y2 + h2 <= y )
		return false;

	if ( x + w <= x2 )
		return false;
	if ( x2 + w2 <= x )
		return false;

	return true;
}

bool CMapLayout::TemplateFits( const CRoomTemplate *pTemplate, int x, int y, bool bAllowNoExits ) const
{
	// check it's not out of bounds
	if ( x < 0 || y < 0 || x + pTemplate->GetTilesX() > MAP_LAYOUT_TILES_WIDE || y + pTemplate->GetTilesY() > MAP_LAYOUT_TILES_WIDE )
		return false;

	// check for overlapping any existing rooms
	for (int j = 0 ; j < pTemplate->GetTilesX() ; j++)
	{
		for (int k = 0 ; k < pTemplate->GetTilesY() ; k++)
		{
			if (m_pRoomGrid[x+j][y+k])
				return false;
		}
	}

	// if a template has no exits, assume it's a solid block used for capping and therefore fits anywhere
	if ( bAllowNoExits && pTemplate->m_Exits.Count() == 0 )
		return true;

	// check for matching exits
	if ( !CheckExits( pTemplate, x, y ) )
		return false;

	return true;
}

bool CMapLayout::CheckExits( const CRoomTemplate *pTemplate, int x, int y, CUtlVector< CRoomTemplateExit * > *pMatchingExits ) const
{
	int iRoomWide = pTemplate->GetTilesX();
	int iRoomTall = pTemplate->GetTilesY();

	// check exits of surrounding rooms match ours..
	for (int k = 0; k < iRoomWide; k++)
	{
		// check bottom
		if ( y > 0 && m_pRoomGrid[x + k][y - 1] != NULL )
		{
			if ( !CheckExitsOnSquares( pTemplate, k, iRoomTall - 1, EXITDIR_SOUTH, x + k, y - 1, false, pMatchingExits ) )
				return false;
		}

		// check top
		if ( ( y + iRoomTall ) < MAP_LAYOUT_TILES_WIDE && m_pRoomGrid[x + k][y + iRoomTall] != NULL )
		{
			if ( !CheckExitsOnSquares( pTemplate, k, 0, EXITDIR_NORTH, x + k, y + iRoomTall, false, pMatchingExits ) )
				return false;
		}
	}

	for (int j = 0; j < iRoomTall; j++)
	{
		// check left
		if (x > 0 && m_pRoomGrid[x - 1][y + j] != NULL)
		{
			if ( !CheckExitsOnSquares( pTemplate, 0, (iRoomTall - 1) - j, EXITDIR_WEST, x - 1, y + j, false, pMatchingExits ) )
				return false;
		}
		// check right
		if ( ( x + iRoomWide ) < MAP_LAYOUT_TILES_WIDE && m_pRoomGrid[x + iRoomWide][y + j] != NULL )
		{
			if ( !CheckExitsOnSquares( pTemplate, iRoomWide - 1, (iRoomTall - 1) - j, EXITDIR_EAST, x + iRoomWide, y + j, false, pMatchingExits ) )
				return false;
		}
	}
	return true;
}

// checks if these two squares match properly
bool CMapLayout::CheckExitsOnSquares( const CRoomTemplate *pTemplate1, int offset_x, int offset_y, ExitDirection_t Direction, int x2, int y2, bool bRequireConnection, CUtlVector< CRoomTemplateExit * > *pMatchingExits ) const
{
	CRoom *pRoom2 = m_pRoomGrid[x2][y2];
	Assert( pRoom2 );
	const CRoomTemplate *pTemplate2 = pRoom2->m_pRoomTemplate;
	Assert( pTemplate2 );

	// check if the roomtemplate to be placed has an exit facing this way
	CRoomTemplateExit *pTemplate1_Exit = NULL;
	for ( int i=0; i < pTemplate1->m_Exits.Count(); i++ )
	{
		if ( pTemplate1->m_Exits[i]->m_bChokepointGrowSource )		// never connect into a chokepoint grow source
			continue;
		if ( pTemplate1->m_Exits[i]->m_iXPos == offset_x && 
			 pTemplate1->m_Exits[i]->m_iYPos == offset_y && 
			 pTemplate1->m_Exits[i]->m_ExitDirection == Direction )
		{
			pTemplate1_Exit = pTemplate1->m_Exits[i];
			break;
		}
	}

	// check if the already existing room has an exit facing the opposite way
	CRoomTemplateExit *pTemplate2_Exit = NULL;
	ExitDirection_t OppositeDirection = CRoomTemplateExit::GetOppositeDirection( Direction );
	int offset_x2 = x2 - pRoom2->m_iPosX;
	int offset_y2 = (pTemplate2->GetTilesY()-1) - (y2 - pRoom2->m_iPosY);
	for ( int i=0; i < pTemplate2->m_Exits.Count(); i++ )
	{
		if ( pTemplate2->m_Exits[i]->m_iXPos == offset_x2 && 
			pTemplate2->m_Exits[i]->m_iYPos == offset_y2 && 
			pTemplate2->m_Exits[i]->m_ExitDirection == OppositeDirection )
		{
			pTemplate2_Exit = pTemplate2->m_Exits[i];
			break;
		}
	}

	// matching exits
	if ( pTemplate1_Exit && pTemplate2_Exit 
		&& !Q_stricmp( pTemplate1_Exit->m_szExitTag, pTemplate2_Exit->m_szExitTag ) )			// Make sure exit types are equal here
	{
		if ( pMatchingExits )
			pMatchingExits->AddToTail( pTemplate1_Exit );
		return true;
	}

	// matching walls
	if ( !pTemplate1_Exit && !pTemplate2_Exit && !bRequireConnection )
		return true;

	return false;
}

void CMapLayout::PlaceRoom( CRoom *pRoom )
{
	Assert( pRoom );
	pRoom->m_nPlacementIndex = m_PlacedRooms.AddToTail( pRoom );
	pRoom->m_pMapLayout = this;

	if ( !pRoom->m_pRoomTemplate )
		return;

	// fill in our pointer grid to easily access the croom later
	for (int x = pRoom->m_iPosX ; x < pRoom->m_iPosX + pRoom->m_pRoomTemplate->GetTilesX() ; x++)
	{
		for (int y = pRoom->m_iPosY ; y < pRoom->m_iPosY + pRoom->m_pRoomTemplate->GetTilesY() ; y++)
		{
			m_pRoomGrid[x][y] = pRoom;
		}
	}
}

void CMapLayout::RemoveRoom( CRoom *pRoom )
{
	m_PlacedRooms.FindAndRemove( pRoom );
	pRoom->m_pMapLayout = NULL;

	if ( !pRoom->m_pRoomTemplate )
		return;

	// clear the pointer grid at the location of this room
	int iTileX = pRoom->m_iPosX;
	int iTileY = pRoom->m_iPosY;
	for ( int x = iTileX ; x < iTileX + pRoom->m_pRoomTemplate->GetTilesX() ; x++ )
	{
		for ( int y = iTileY ; y < iTileY + pRoom->m_pRoomTemplate->GetTilesY() ; y++ )
		{
			m_pRoomGrid[x][y] = NULL;
		}
	}
}

void CMapLayout::AddLogicalRoom( CRoomTemplate *pRoomTemplate )
{
	if ( !pRoomTemplate )
		return;

	m_LogicalRooms.AddToTail( pRoomTemplate );
}

//-----------------------------------------------------------------------------
// An NPC spawn encounter in the mission
//-----------------------------------------------------------------------------	

void CASW_Encounter::AddSpawnDef( CASW_Spawn_Definition *pSpawnDef )
{
	m_SpawnDefs.AddToTail( pSpawnDef );
}

void CASW_Encounter::LoadFromKeyValues( KeyValues *pKeys )
{
	m_SpawnDefs.Purge();

	m_vecPosition.x = pKeys->GetFloat( "PosX" );
	m_vecPosition.y = pKeys->GetFloat( "PosY" );
	m_vecPosition.z = 0;
	m_flEncounterRadius = pKeys->GetFloat( "Radius" );
	for ( KeyValues *pKey = pKeys->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		if ( !Q_stricmp( pKey->GetName(), "SpawnDef" ) )
		{
			CASW_Spawn_Definition* pSpawnDef = SpawnSelection()->GetSpawnDefByID( pKey->GetInt() );
			if ( pSpawnDef )
			{
				m_SpawnDefs.AddToTail( pSpawnDef );
			}
			else
			{
				Msg( "Failed to load spawn def %d for encounter.\n", pKey->GetInt() );
			}
		}
	}
}

void CASW_Encounter::SaveToKeyValues( KeyValues *pKeys )
{
	pKeys->SetFloat( "PosX", m_vecPosition.x );
	pKeys->SetFloat( "PosY", m_vecPosition.y );
	//pKeys->SetFloat( "PosZ", m_vecPosition.z );
	pKeys->SetFloat( "Radius", m_flEncounterRadius );
	for ( int i = 0; i < m_SpawnDefs.Count(); i++ )
	{
		KeyValues *pSpawnKeys = new KeyValues( "SpawnDef" );
		pSpawnKeys->SetInt( NULL, m_SpawnDefs[i]->GetID() );
		pKeys->AddSubKey( pSpawnKeys );
	}
}