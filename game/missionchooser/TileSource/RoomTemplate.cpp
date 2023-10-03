#include "LevelTheme.h"
#include "KeyValues.h"
#include "RoomTemplate.h"
#include "filesystem.h"
#include "TagList.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ExitDirection_t GetDirectionFromString( const char *pDirectionString )
{
	if ( Q_stricmp( pDirectionString, "north" ) == 0 )
	{
		return EXITDIR_NORTH;
	}
	else if ( Q_stricmp( pDirectionString, "south" ) == 0 )
	{
		return EXITDIR_SOUTH;
	}
	else if ( Q_stricmp( pDirectionString, "east" ) == 0 )
	{
		return EXITDIR_EAST;
	}
	else if ( Q_stricmp( pDirectionString, "west" ) == 0 )
	{
		return EXITDIR_WEST;
	}

	return EXITDIR_END;
}

CRoomTemplate::CRoomTemplate( CLevelTheme* pLevelTheme ) :
m_pLevelTheme( pLevelTheme ),
m_nSpawnWeight( 0 ),
m_nTilesX( 1 ),
m_nTilesY( 1 )
{
	m_FullName[0] = '\0';
	m_SubFolder[0] = '\0';
	m_TemplateName[0] = '\0';
	m_Description[0] = '\0';
	m_Soundscape[0] = '\0';
	m_nTileType = ASW_TILETYPE_UNKNOWN;
}

CRoomTemplate::~CRoomTemplate()
{
	m_Exits.PurgeAndDeleteElements();
}

void CRoomTemplate::LoadFromKeyValues( const char *pRoomName, KeyValues *pKeyValues )
{
	m_nTilesX = pKeyValues->GetInt( "TilesX", 1 );
	m_nTilesY = pKeyValues->GetInt( "TilesY", 1 );
	SetSpawnWeight( pKeyValues->GetInt( "SpawnWeight", MIN_SPAWN_WEIGHT ) );

	SetFullName( pRoomName );

	Q_strncpy( m_Description, pKeyValues->GetString( "RoomTemplateDescription", "" ), m_nMaxDescriptionLength );
	Q_strncpy( m_Soundscape, pKeyValues->GetString( "Soundscape", "" ), m_nMaxSoundscapeLength );

	SetTileType( pKeyValues->GetInt( "TileType", ASW_TILETYPE_UNKNOWN ) );

	m_Tags.RemoveAll();

	// search through all the exit subsections
	KeyValues *pkvSubSection = pKeyValues->GetFirstSubKey();
	bool bClearedExits = false;	
	while ( pkvSubSection )
	{
		// mission details
		if ( Q_stricmp(pkvSubSection->GetName(), "EXIT")==0 )
		{
			if ( !bClearedExits )
			{
				// if we haven't cleared previous exits yet then do so now
				m_Exits.PurgeAndDeleteElements();
				bClearedExits = true;
			}
			CRoomTemplateExit *pExit = new CRoomTemplateExit();
			pExit->m_iXPos = pkvSubSection->GetInt("XPos");
			pExit->m_iYPos = pkvSubSection->GetInt("YPos");
			pExit->m_ExitDirection = (ExitDirection_t) pkvSubSection->GetInt("ExitDirection");
			pExit->m_iZChange = pkvSubSection->GetInt("ZChange");
			Q_strncpy( pExit->m_szExitTag, pkvSubSection->GetString( "ExitTag" ), sizeof( pExit->m_szExitTag ) );
			pExit->m_bChokepointGrowSource = !!pkvSubSection->GetInt("ChokeGrow", 0);

			// discard exits outside the room bounds
			if ( pExit->m_iXPos < 0 || pExit->m_iYPos < 0 || pExit->m_iXPos >= m_nTilesX || pExit->m_iYPos >= m_nTilesY )
			{
				delete pExit;
			}
			else
			{
				m_Exits.AddToTail(pExit);
			}
		}
		else if ( Q_stricmp(pkvSubSection->GetName(), "Tags")==0 && TagList() )
		{
			for ( KeyValues *sub = pkvSubSection->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
			{
				if ( !Q_stricmp( sub->GetName(), "tag" ) )
				{
					AddTag( sub->GetString() );
				}
			}
		}
		pkvSubSection = pkvSubSection->GetNextKey();
	}
}

bool CRoomTemplate::SaveRoomTemplate()
{
	if (!m_pLevelTheme)
		return false;

	char szThemeDirName[MAX_PATH];
	Q_snprintf(szThemeDirName, sizeof(szThemeDirName), "tilegen/roomtemplates/%s", m_pLevelTheme->m_szName);
	g_pFullFileSystem->CreateDirHierarchy( szThemeDirName, "GAME" );

	char szFullFileName[MAX_PATH];	
	Q_snprintf( szFullFileName, sizeof(szFullFileName), "tilegen/roomtemplates/%s/%s.roomtemplate", m_pLevelTheme->m_szName, m_FullName );
	KeyValues *pRoomTemplateKeyValues = new KeyValues( m_FullName );
	pRoomTemplateKeyValues->SetInt( "TilesX", m_nTilesX );
	pRoomTemplateKeyValues->SetInt( "TilesY", m_nTilesY );
	pRoomTemplateKeyValues->SetInt( "SpawnWeight", m_nSpawnWeight );
	pRoomTemplateKeyValues->SetString( "RoomTemplateDescription", m_Description );
	pRoomTemplateKeyValues->SetString( "Soundscape", m_Soundscape );
	pRoomTemplateKeyValues->SetInt( "TileType", m_nTileType );

	// exits
	int iExits = m_Exits.Count();
	for (int i=0;i<iExits;i++)
	{
		KeyValues *pkvSubSection = new KeyValues("EXIT");
		pkvSubSection->SetInt("XPos", m_Exits[i]->m_iXPos);
		pkvSubSection->SetInt("YPos", m_Exits[i]->m_iYPos);
		pkvSubSection->SetInt("ExitDirection", (int) m_Exits[i]->m_ExitDirection);
		pkvSubSection->SetInt("ZChange", m_Exits[i]->m_iZChange);
		pkvSubSection->SetString("ExitTag", m_Exits[i]->m_szExitTag);
		pkvSubSection->SetBool("ChokeGrow", m_Exits[i]->m_bChokepointGrowSource);

		pRoomTemplateKeyValues->AddSubKey(pkvSubSection);
	}

	// tags
	KeyValues *pkvSubSection = new KeyValues("Tags");
	for ( int i=0;i<GetNumTags();i++ )
	{
		pkvSubSection->AddSubKey( new KeyValues( "tag", NULL, GetTag( i ) ) );
	}
	pRoomTemplateKeyValues->AddSubKey(pkvSubSection);

	if (!pRoomTemplateKeyValues->SaveToFile(g_pFullFileSystem, szFullFileName, "GAME"))
	{
		Msg("Error: Failed to save room template %s\n", szFullFileName);
		return false;
	}
	return true;
}

void CRoomTemplate::SetFullName( const char *pFullName )
{
	Q_strncpy( m_FullName, pFullName, MAX_PATH );
	Q_StripExtension( m_FullName, m_FullName, MAX_PATH );
	Q_FixSlashes( m_FullName );
	Q_ExtractFilePath( m_FullName, m_SubFolder, MAX_PATH );
	Q_AppendSlash( m_SubFolder, MAX_PATH );
	Q_strncpy( m_TemplateName, m_FullName + Q_strlen( m_SubFolder ), MAX_PATH );
}

void CRoomTemplate::SetDescription( const char *pDescription )
{
	Q_strncpy( m_Description, pDescription, m_nMaxDescriptionLength );
}

void CRoomTemplate::SetSoundscape( const char *pSoundscape )
{
	Q_strncpy( m_Soundscape, pSoundscape, m_nMaxSoundscapeLength );
}

void CRoomTemplateExit::GetExitOffset( ExitDirection_t Direction, int *pX, int *pY )
{
	switch ( Direction )
	{
		case EXITDIR_NORTH: *pX = 0; *pY = -1; break;
		case EXITDIR_EAST: *pX = 1; *pY = 0; break;
		case EXITDIR_SOUTH: *pX = 0; *pY = 1; break;
		case EXITDIR_WEST: *pX = -1; *pY = 0; break;
		default: break;
	}
}

ExitDirection_t CRoomTemplateExit::GetOppositeDirection( ExitDirection_t Direction )
{
	switch (Direction)
	{
		case EXITDIR_NORTH: return EXITDIR_SOUTH; break;
		case EXITDIR_EAST: return EXITDIR_WEST; break;
		case EXITDIR_SOUTH: return EXITDIR_NORTH; break;
		case EXITDIR_WEST: return EXITDIR_EAST; break;
		default: break;
	}
	return EXITDIR_NORTH;
}

bool CRoomTemplate::IsStartRoom() const
{
	return HasTag( "Start" );
}

bool CRoomTemplate::IsEscapeRoom() const
{
	return HasTag( "Escape" );
}

bool CRoomTemplate::IsBorderRoom() const
{
	return HasTag( "Border" );
}

bool CRoomTemplate::ShouldOnlyPlaceByRequest() const
{
	return HasTag( "Special" ) || HasTag( "Start" ) || HasTag( "Escape" );
}

bool CRoomTemplate::HasTag( const char* szTag ) const
{
	for ( int i=0;i<m_Tags.Count();i++ )
	{
		if ( !Q_stricmp( m_Tags[i], szTag ) )
		{
			return true;
		}
	}
	return false;
}

bool CRoomTemplate::AddTag( const char *szTag )
{
	if ( !TagList() )
		return false;

	// check it's a valid tag
	const char *pszFoundTag = TagList()->FindTag( szTag );
	if ( !pszFoundTag )
		return false;

	// check we don't have it already
	if ( HasTag( pszFoundTag ) )
		return false;

	m_Tags.AddToTail( pszFoundTag );
	return true;
}

bool CRoomTemplate::RemoveTag( const char *szTag )
{
	if ( !TagList() )
		return false;

	for ( int i=0;i<m_Tags.Count();i++ )
	{
		if ( !Q_stricmp( m_Tags[i], szTag ) )
		{
			m_Tags.Remove( i );
			return true;
		}
	}
	return false;
}