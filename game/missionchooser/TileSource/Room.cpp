#include "Room.h"
#include "ChunkFile.h"
#include "MapLayout.h"
#include "RoomTemplate.h"
#include "LevelTheme.h"
#include "KeyValues.h"

#include <vgui/IVGui.h>
#include "vgui_controls/Controls.h"
#include <vgui_controls/Panel.h>
#include "TileGenDialog.h"
#include "layout_system\tilegen_ranges.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CRoom::CRoom()
{
	m_pRoomTemplate = NULL;
	m_iPosX = 0;
	m_iPosY = 0;
	m_iNumChildren = 0;
	m_nPlacementIndex = -1;
	m_bHasAlienEncounter = false;

	m_pPlacedRoomPanel = NULL;
	m_pMapLayout = NULL;
}

CRoom::CRoom( CMapLayout *pMapLayout, const CRoomTemplate* pRoomTemplate, int TileX, int TileY )
{
	m_pRoomTemplate = pRoomTemplate;
	m_iPosX = TileX;
	m_iPosY = TileY;
	m_iNumChildren = 0;
	m_bHasAlienEncounter = false;
	
	// Add ourself to the list of placed rooms.
	// This also sets m_nPlacementIndex.
	pMapLayout->PlaceRoom( this );

	m_pPlacedRoomPanel = NULL;
}

CRoom::~CRoom()
{
	if ( g_pTileGenDialog )
	{
		g_pTileGenDialog->m_SelectedRooms.FindAndRemove( this );
	}
	if ( m_pMapLayout )
	{
		// remove it from the list of placed rooms
		m_pMapLayout->RemoveRoom( this );
	}

	if (m_pPlacedRoomPanel)
	{
		m_pPlacedRoomPanel->MarkForDeletion();
		m_pPlacedRoomPanel = NULL;
	}
}

KeyValues *CRoom::GetKeyValuesCopy()
{
	KeyValues *pKeys = new KeyValues( "room" );
	pKeys->SetInt( "posx", m_iPosX );
	pKeys->SetInt( "posy", m_iPosY );
	pKeys->SetString( "theme", m_pRoomTemplate->m_pLevelTheme->m_szName );
	pKeys->SetString( "template", m_pRoomTemplate->GetFullName() );

	return pKeys;
}

bool CRoom::SaveRoomToFile(CChunkFile *pFile)
{
	if (!pFile || !m_pRoomTemplate || !m_pRoomTemplate->m_pLevelTheme)
		return false;

	ChunkFileResult_t eResult = pFile->BeginChunk("room");
	if (eResult != ChunkFile_Ok)
		return false;

	// write out each property of the room
	if (pFile->WriteKeyValueInt("posx", m_iPosX) != ChunkFile_Ok)
		return false;
	if (pFile->WriteKeyValueInt("posy", m_iPosY) != ChunkFile_Ok)
		return false;

	// theme and template name
	if (pFile->WriteKeyValue("theme", m_pRoomTemplate->m_pLevelTheme->m_szName) != ChunkFile_Ok)
		return false;
	if (pFile->WriteKeyValue( "template", m_pRoomTemplate->GetFullName() ) != ChunkFile_Ok)
		return false;

	if (pFile->EndChunk() != ChunkFile_Ok)
		return false;

	return true;
}

// creates a new CRoom and reads in its properties from the file
bool CRoom::LoadRoomFromKeyValues( KeyValues *pRoomKeys, CMapLayout *pMapLayout )
{	
	CRoom* pNewRoom = new CRoom();
	char szLoadingTheme[256];
	char szLoadingTemplate[256];
	szLoadingTheme[0] = '\0';
	szLoadingTemplate[0] = '\0';

	Q_snprintf( szLoadingTheme, sizeof(szLoadingTheme), "%s", pRoomKeys->GetString( "theme" ) );		
	Q_snprintf( szLoadingTemplate, sizeof(szLoadingTemplate), "%s", pRoomKeys->GetString( "template" ) );		
	pNewRoom->m_iPosX = pRoomKeys->GetInt( "posx" );
	pNewRoom->m_iPosY = pRoomKeys->GetInt( "posy" );

	// find pointer to the room template
	// first find the theme with matching name
	int iThemes = CLevelTheme::s_LevelThemes.Count();
	for (int i=0;i<iThemes;i++)
	{
		CLevelTheme* pTheme = CLevelTheme::s_LevelThemes[i];
		if (!pTheme)
			continue;
		if ( !Q_stricmp(pTheme->m_szName, szLoadingTheme) )
		{
			// now find a template within that theme with our template's name
			int iTemplates = pTheme->m_RoomTemplates.Count();
			for (int t=0;t<iTemplates;t++)
			{
				if ( !Q_stricmp( pTheme->m_RoomTemplates[t]->GetFullName(), szLoadingTemplate ) )
				{
					pNewRoom->m_pRoomTemplate = pTheme->m_RoomTemplates[t];
					break;
				}
			}
			break;
		}
	}
	if (!pNewRoom->m_pRoomTemplate)
	{
		Msg( "Failed to load room template %s in theme %s\n", szLoadingTemplate, szLoadingTheme );
		delete pNewRoom;
		return false;
	}

	// add it to the list of placed rooms
	pMapLayout->PlaceRoom( pNewRoom );

	return true;
}

// =================================
// IASW_Room_Details interface
// =================================

// tags
bool CRoom::HasTag( const char *szTag )
{
	if ( !m_pRoomTemplate )
		return false;

	return m_pRoomTemplate->HasTag( szTag );
}

int CRoom::GetNumTags()
{
	if ( !m_pRoomTemplate )
		return 0;

	return m_pRoomTemplate->GetNumTags();
}

const char* CRoom::GetTag( int i )
{
	if ( !m_pRoomTemplate )
		return "";

	return m_pRoomTemplate->GetTag( i );
}

int CRoom::GetSpawnWeight()
{
	if ( !m_pRoomTemplate )
		return -1;

	return m_pRoomTemplate->GetSpawnWeight();
}

bool CRoom::GetThumbnailName( char* szOut, int iBufferSize )
{
	if ( !m_pRoomTemplate || !m_pRoomTemplate->m_pLevelTheme )
		return false;

	Q_snprintf( szOut, iBufferSize, "tilegen/roomtemplates/%s/%s.tga", m_pRoomTemplate->m_pLevelTheme->m_szName, m_pRoomTemplate->GetFullName() );
	return true;
}

bool CRoom::GetFullRoomName( char* szOut, int iBufferSize )
{
	if ( !m_pRoomTemplate || !m_pRoomTemplate->m_pLevelTheme )
		return false;

	Q_snprintf( szOut, iBufferSize, "%s\\%s", m_pRoomTemplate->m_pLevelTheme->m_szName, m_pRoomTemplate->GetFullName() );
	return true;
}

void CRoom::GetSoundscape( char* szOut, int iBufferSize )
{
	Q_snprintf( szOut, iBufferSize, "%s", m_pRoomTemplate ? m_pRoomTemplate->GetSoundscape() : "" );
}

void CRoom::GetTheme( char* szOut, int iBufferSize )
{
	Q_snprintf( szOut, iBufferSize, "%s", ( m_pRoomTemplate && m_pRoomTemplate->m_pLevelTheme ) ? m_pRoomTemplate->m_pLevelTheme->m_szName : "" );
}

const Vector& CRoom::GetAmbientLight()
{
	/*
	if ( m_bHasAlienEncounter )
	{
		static Vector s_vecAlienEncounterAmbient = vec3_origin; //Vector( 0.05f, 0.05f, 0.05f );
		return s_vecAlienEncounterAmbient;
	}
	*/
	return ( m_pRoomTemplate && m_pRoomTemplate->m_pLevelTheme ) ? m_pRoomTemplate->m_pLevelTheme->m_vecAmbientLight : vec3_origin;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CRoom::GetTileType()
{
	if ( !m_pRoomTemplate )
		return -1;

	return m_pRoomTemplate->GetTileType();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CRoom::GetTileTypeName( int nType )
{
	// This is the default, I didn't find anything value for GetTileType
	if ( nType == -1 )
		return NULL;

	Assert( nType >= ASW_TILETYPE_UNKNOWN && nType < ASW_TILETYPE_COUNT );
	return g_szASWTileTypeStrings[nType];
}


void CRoom::GetWorldBounds( Vector *vecWorldMins, Vector *vecWorldMaxs )
{
	if ( !m_pRoomTemplate )
	{
		*vecWorldMins = vec3_origin;
		*vecWorldMaxs = vec3_origin;
		return;
	}

	int half_map_size = MAP_LAYOUT_TILES_WIDE * 0.5f;		// shift back so the middle of our grid is the origin
	int xOffset = ( m_iPosX - half_map_size ) * ASW_TILE_SIZE;
	int yOffset = ( m_iPosY - half_map_size ) * ASW_TILE_SIZE;

	vecWorldMins->x = xOffset;
	vecWorldMins->y = yOffset;
	vecWorldMins->z = 0;

	vecWorldMaxs->x = xOffset + m_pRoomTemplate->GetTilesX() * ASW_TILE_SIZE;
	vecWorldMaxs->y = yOffset + m_pRoomTemplate->GetTilesY() * ASW_TILE_SIZE;
	vecWorldMaxs->z = 0;
}

const Vector& CRoom::WorldSpaceCenter()
{
	Vector mins = vec3_origin;
	Vector maxs = vec3_origin;
	GetWorldBounds( &mins, &maxs );

	Vector &vecResult = AllocTempVector();
	vecResult = ( mins + maxs ) / 2.0f;
	return vecResult;
}

int CRoom::GetNumExits()
{
	if ( !m_pRoomTemplate )
		return 0;

	return m_pRoomTemplate->m_Exits.Count();	
}

IASW_Room_Details *CRoom::GetAdjacentRoom( int nExit )
{
	int nExitX, nExitY;
	if ( GetExitPosition( m_pRoomTemplate, m_iPosX, m_iPosY, nExit, &nExitX, &nExitY ) )
	{
		return m_pMapLayout->GetRoom( nExitX, nExitY );
	}

	return NULL;
}