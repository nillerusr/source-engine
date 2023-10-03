#include "VMFExporter.h"
#include "KeyValues.h"

#include "TileSource/RoomTemplate.h"
#include "TileSource/Room.h"
#include "TileSource/LevelTheme.h"
#include "TileSource/MapLayout.h"
#include "TileGenDialog.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar tilegen_use_instancing( "tilegen_use_instancing", "0", FCVAR_REPLICATED );

// TODO: Read room templates into keyvalues and output them after the whole room template has been loaded
//       This way we can modify state based on the complete picture of that room, rather than just making changes on a key by key basis

VMFExporter::VMFExporter()
{
	m_pMapLayout = NULL;

	Init();
}

VMFExporter::~VMFExporter()
{
	ClearExportErrors();
}

void VMFExporter::Init()
{
	m_szLastExporterError[0] = '\0';
	m_SideTranslations.Purge();
	m_NodeTranslations.Purge();
	m_vecLastPlaneOffset = vec3_origin;
	m_bWritingLevelContainer = false;
	m_iEntityCount = 1;	// 1 already, from the worldspawn
	m_iSideCount = 0;
	m_pTemplateKeys = NULL;
	
	m_iNextNodeID = 0;
	m_pRoom = NULL;
	m_iCurrentRoom = 0;

	m_iMapExtents_XMin = 0;
	m_iMapExtents_YMin = 0;
	m_iMapExtents_XMax = 0;
	m_iMapExtents_YMax = 0;
	m_pExportKeys = NULL;
	m_vecStartRoomOrigin = vec3_origin;
	ClearExportErrors();
}

void VMFExporter::ExportError( const char *pMsg, ... )
{
	char msg[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( msg, sizeof( msg ), pMsg, marker );
	va_end( marker );

	m_ExportErrors.AddToTail( TileGenCopyString( msg ) );
}

void VMFExporter::ClearExportErrors()
{
	m_ExportErrors.PurgeAndDeleteElements();
}

bool VMFExporter::ShowExportErrors()
{
	if ( m_ExportErrors.Count() <= 0 )
		return false;
	char msg[4096];
	msg[0] = 0;
	for ( int i=0; i<m_ExportErrors.Count(); i++ )
	{
		Q_snprintf( msg, sizeof( msg ), "%s\n%s", msg, m_ExportErrors[i] );
	}
	VGUIMessageBox( g_pTileGenDialog, "Export problems:", msg );
	return true;
}

bool VMFExporter::ExportVMF( CMapLayout* pLayout, const char *mapname, bool bPopupWarnings )
{
	m_bPopupWarnings = bPopupWarnings;

	Init();

	m_pMapLayout = pLayout;

	if ( pLayout->m_PlacedRooms.Count() <= 0 )
	{
		Q_snprintf( m_szLastExporterError, sizeof(m_szLastExporterError), "Failed to export: No rooms placed in the map layout!\n" );
		return false;
	}

	// see if we have a start room
	bool bHasStartRoom = false;
	for ( int i = 0 ; i < pLayout->m_PlacedRooms.Count() ; i++ )
	{
		if ( pLayout->m_PlacedRooms[i]->m_pRoomTemplate->IsStartRoom() )
		{
			int half_map_size = MAP_LAYOUT_TILES_WIDE * 0.5f;		// shift back so the middle of our grid is the origin
			m_vecStartRoomOrigin.x = ( pLayout->m_PlacedRooms[i]->m_iPosX - half_map_size ) * ASW_TILE_SIZE;
			m_vecStartRoomOrigin.y = ( pLayout->m_PlacedRooms[i]->m_iPosY - half_map_size ) * ASW_TILE_SIZE;
			bHasStartRoom = true;
			break;
		}
	}
	LoadUniqueKeyList();

	m_iNextNodeID = 0;
		
	m_pExportKeys = new KeyValues( "ExportKeys" );

	m_pExportKeys->AddSubKey( GetVersionInfo() );
	m_pExportKeys->AddSubKey( GetDefaultVisGroups() );
	m_pExportKeys->AddSubKey( GetViewSettings() );
	m_pExportWorldKeys = GetDefaultWorldChunk();
	if ( !m_pExportWorldKeys )
	{
		Q_snprintf( m_szLastExporterError, sizeof(m_szLastExporterError), "Failed to save world chunk start\n");
		return false;
	}
	m_pExportKeys->AddSubKey( m_pExportWorldKeys );

	
	// save out the big cube the whole level sits in	
	if ( !AddLevelContainer() )
	{
		Q_snprintf( m_szLastExporterError, sizeof(m_szLastExporterError), "Failed to save level container\n");
		return false;
	}

	if ( tilegen_use_instancing.GetBool() )
	{
		int nLogicalRooms = m_pMapLayout->m_LogicalRooms.Count();
		int nPlacedRooms = m_pMapLayout->m_PlacedRooms.Count();

		m_pRoom = NULL;
		for ( int i = 0; i < nLogicalRooms; ++ i )
		{
			AddRoomInstance( m_pMapLayout->m_LogicalRooms[i] );
		}

		for ( int i = 0; i < nPlacedRooms; ++ i )
		{
			m_pRoom = m_pMapLayout->m_PlacedRooms[i];
			AddRoomInstance( m_pRoom->m_pRoomTemplate, i );
		}
	}
	else
	{
		// write out logical room solids
		int iLogicalRooms = m_pMapLayout->m_LogicalRooms.Count();
		m_pRoom = NULL;
		for ( int i = 0 ; i < iLogicalRooms ; i++ )
		{
			// start logical room IDs at 5000 (assumes we'll never place 5000 real rooms)
			m_iCurrentRoom = 5000 + i;
			CRoomTemplate *pRoomTemplate = m_pMapLayout->m_LogicalRooms[i];
			if ( !pRoomTemplate )
				continue;

			if ( !AddRoomTemplateSolids( pRoomTemplate ) )
				return false;
		}

		// go through each CRoom and write out its world solids
		int iRooms = m_pMapLayout->m_PlacedRooms.Count();
		for ( m_iCurrentRoom = 0 ; m_iCurrentRoom<iRooms ; m_iCurrentRoom++)
		{
			m_pRoom = m_pMapLayout->m_PlacedRooms[m_iCurrentRoom];
			if (!m_pRoom)
				continue;
			const CRoomTemplate *pRoomTemplate = m_pRoom->m_pRoomTemplate;
			if (!pRoomTemplate)
				continue;

			if ( !AddRoomTemplateSolids( pRoomTemplate ) )
				return false;
		}

		// write out logical room entities
		m_pRoom = NULL;
		for ( int i = 0 ; i < iLogicalRooms ; i++ )
		{
			// start logical room IDs at 5000 (assumes we'll never place 5000 real rooms)
			m_iCurrentRoom = 5000 + i;
			CRoomTemplate *pRoomTemplate = m_pMapLayout->m_LogicalRooms[i];
			if ( !pRoomTemplate )
				continue;

			if ( !AddRoomTemplateEntities( pRoomTemplate ) )
				return false;
		}

		// go through each CRoom and add its entities
		for ( m_iCurrentRoom = 0 ; m_iCurrentRoom<iRooms ; m_iCurrentRoom++)
		{
			m_pRoom = m_pMapLayout->m_PlacedRooms[m_iCurrentRoom];
			if (!m_pRoom)
				continue;
			const CRoomTemplate *pRoomTemplate = m_pRoom->m_pRoomTemplate;
			if (!pRoomTemplate)
				continue;

			if ( !AddRoomTemplateEntities( pRoomTemplate ) )
				return false;
		}
	}

	// add some player starts to the map in the tile the user selected
	if ( !bHasStartRoom )
	{
		m_pExportKeys->AddSubKey( GetPlayerStarts() );
	}

	m_pExportKeys->AddSubKey( GetGameRulesProxy() );
	m_pExportKeys->AddSubKey( GetDefaultCamera() );

	// save out the export keys
	char filename[512];
	Q_snprintf( filename, sizeof(filename), "maps\\%s", mapname );
	Q_SetExtension( filename, "vmf", sizeof( filename ) );
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	for ( KeyValues *pKey = m_pExportKeys->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		pKey->RecursiveSaveToFile( buf, 0 );			
	}
	if ( !g_pFullFileSystem->WriteFile( filename, "GAME", buf ) )
	{
		Msg( "Failed to SaveToFile %s\n", filename );
		return false;
	}
	
	// save the map layout there (so the game can get information about rooms during play)
	Q_snprintf( filename, sizeof( filename ), "maps\\%s", mapname );
	Q_SetExtension( filename, "layout", sizeof( filename ) );
	if ( !m_pMapLayout->SaveMapLayout( filename ) )
	{
		Q_snprintf( m_szLastExporterError, sizeof(m_szLastExporterError), "Failed to save .layout file\n");
		return false;
	}

	return true;
}

bool VMFExporter::AddRoomTemplateSolids( const CRoomTemplate *pRoomTemplate )
{
	// open its vmf file
	char roomvmfname[MAX_PATH];
	Q_snprintf(roomvmfname, sizeof(roomvmfname), "tilegen/roomtemplates/%s/%s.vmf", 
		pRoomTemplate->m_pLevelTheme->m_szName,
		pRoomTemplate->GetFullName() );
	m_pTemplateKeys = new KeyValues( "RoomTemplateVMF" );
	m_pTemplateKeys->LoadFromFile( g_pFullFileSystem, roomvmfname, "GAME" );

	// look for world key
	for ( KeyValues *pKeys = m_pTemplateKeys; pKeys; pKeys = pKeys->GetNextKey() )
	{
		if ( !Q_stricmp( pKeys->GetName(), "world" ) )		// find the world key in our room template
		{
			if ( !ProcessWorld( pKeys ) )					// fix up solid positions
			{
				Q_snprintf( m_szLastExporterError, sizeof(m_szLastExporterError), "Failed to copy world from room %s\n", pRoomTemplate->GetFullName() );
				return false;
			}
			for ( KeyValues *pSubKey = pKeys->GetFirstSubKey(); pSubKey; pSubKey = pSubKey->GetNextKey() )		// convert each solid to a func_detail entity
			{
				if ( !Q_stricmp( pSubKey->GetName(), "solid" ) )
				{
					if ( IsDisplacementBrush( pSubKey ) )
					{
						// add to world section
						m_pExportWorldKeys->AddSubKey( pSubKey->MakeCopy() );
					}
					else
					{
						// put into entity section as a func_detail
						KeyValues *pFuncDetail = new KeyValues( "entity" );
						pFuncDetail->SetInt( "id", ++m_iEntityCount );
						pFuncDetail->SetString( "classname", "func_detail" );
						pFuncDetail->AddSubKey( pSubKey->MakeCopy() );
						m_pExportKeys->AddSubKey( pFuncDetail );
					}
				}
			}			
		}
	}
	m_pTemplateKeys->deleteThis();
	m_pTemplateKeys = NULL;
	return true;
}

bool VMFExporter::AddRoomTemplateEntities( const CRoomTemplate *pRoomTemplate )
{
	m_SideTranslations.PurgeAndDeleteElements();
	m_NodeTranslations.PurgeAndDeleteElements();

	// reopen the source vmf
	char roomvmfname[MAX_PATH];
	Q_snprintf( roomvmfname, sizeof(roomvmfname), "tilegen/roomtemplates/%s/%s.vmf", 
		pRoomTemplate->m_pLevelTheme->m_szName,
		pRoomTemplate->GetFullName() );
	m_pTemplateKeys = new KeyValues( "RoomTemplateVMF" );
	m_pTemplateKeys->LoadFromFile( g_pFullFileSystem, roomvmfname, "GAME" );

	// make all node IDs unique
	MakeNodeIDsUnique();		

	// sets priority of objective entities based on the generation options
	ReorderObjectives( pRoomTemplate, m_pTemplateKeys );

	// look for entity keys
	for ( KeyValues *pKeys = m_pTemplateKeys; pKeys; pKeys = pKeys->GetNextKey() )
	{
		if ( !Q_stricmp( pKeys->GetName(), "entity" ) )
		{
			if ( !ProcessEntity( pKeys ) )
			{
				Q_snprintf( m_szLastExporterError, sizeof(m_szLastExporterError), "Failed to copy entity from room %s\n", pRoomTemplate->GetFullName());
				return false;
			}
			m_pExportKeys->AddSubKey( pKeys->MakeCopy() );
		}
	}
	
	m_pTemplateKeys->deleteThis();
	m_pTemplateKeys = NULL;
	return true;
};

bool VMFExporter::IsDisplacementBrush( KeyValues *pSolidKeys )
{
	// go through each side
	for ( KeyValues *pKeys = pSolidKeys->GetFirstSubKey(); pKeys; pKeys = pKeys->GetNextKey() )
	{
		if ( !Q_stricmp( pKeys->GetName(), "side" ) && pKeys->FindKey( "dispinfo" ) )
		{
			return true;
		}
	}
	return false;
}

bool VMFExporter::ProcessWorld( KeyValues *pWorldKeys )
{
	// look for solid keys
	KeyValues *pSolidKeys = pWorldKeys->GetFirstSubKey();
	while ( pSolidKeys )
	{
		if ( !Q_stricmp( pSolidKeys->GetName(), "solid" ) )
		{
			if ( !ProcessSolid( pSolidKeys ) )
				return false;
		}
		pSolidKeys = pSolidKeys->GetNextKey();
	}

	return true;
}

bool VMFExporter::ProcessSolid( KeyValues *pSolidKey )
{
	pSolidKey->SetInt( "id", ++m_iEntityCount );
	
	// now process each side
	KeyValues *pKeys = pSolidKey->GetFirstSubKey();
	while ( pKeys )
	{
		if ( !Q_stricmp( pKeys->GetName(), "side" ) )
		{
			if ( !ProcessSide( pKeys ) )
				return false;
		}
		else
		{
			if ( !ProcessGenericRecursive( pKeys ) )
				return false;
		}
		pKeys = pKeys->GetNextKey();
	}

	return true;
}

bool VMFExporter::ProcessSide( KeyValues *pSideKey )
{		
	for ( KeyValues *pKeys = pSideKey->GetFirstSubKey(); pKeys; pKeys = pKeys->GetNextKey() )
	{
		if ( !Q_stricmp( pKeys->GetName(), "dispinfo" ) )
		{
			if ( !ProcessGenericRecursive( pKeys ) )
				return false;
		}
		else
		{
			if ( !ProcessSideKey( pKeys ) )
				return false;
		}
	}

	return true;
}

bool VMFExporter::ProcessGenericRecursive( KeyValues *pKey )
{
	const char *szKey = pKey->GetName();
	const char *szValue = pKey->GetString();

	if ( !Q_stricmp( szKey, "startposition" ) )
	{
		Vector Origin;
		int nRead = sscanf(szValue, "[%f %f %f]", &Origin[0], &Origin[1], &Origin[2]);

		if (nRead != 3)		
			return false;

		// move the points to where our room is
		Origin += GetCurrentRoomOffset();

		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "[%f %f %f]", Origin[0], Origin[1], Origin[2]);

		pKey->SetStringValue( buffer );
	}

	if ( pKey->GetFirstSubKey() )		// if this keyvalues entry has subkeys, then process them
	{
		for ( KeyValues *pKeys = pKey->GetFirstSubKey(); pKeys; pKeys = pKeys->GetNextKey() )
		{
			if ( !ProcessGenericRecursive( pKeys ) )
				return false;
		}
	}

	return true;
}

bool VMFExporter::ProcessSideKey( KeyValues *pKey )
{
	const char *szKey = pKey->GetName();
	const char *szValue = pKey->GetString();

	if (!stricmp(szKey, "id"))
	{
		// store the side
		SideTranslation_t *pSideTranslation = new SideTranslation_t;
		pSideTranslation->m_iOriginalSide = atoi( szValue );

		// give this side a unique ID
		char buffer[32];
		Q_snprintf(buffer, sizeof(buffer), "%d", ++m_iSideCount );
		pKey->SetStringValue( buffer );

		pSideTranslation->m_iNewSide = m_iSideCount;
		m_SideTranslations.AddToTail( pSideTranslation );

		return true;
	}
	else if (!stricmp(szKey, "plane"))
	{
		Vector planepts[3];
		int nRead = sscanf(szValue, "(%f %f %f) (%f %f %f) (%f %f %f)", 
			&planepts[0][0], &planepts[0][1], &planepts[0][2],
			&planepts[1][0], &planepts[1][1], &planepts[1][2],
			&planepts[2][0], &planepts[2][1], &planepts[2][2]);

		if (nRead != 9)		
			return false;

		if (m_bWritingLevelContainer)
		{
			// we're currently writing out the big cube that the level sits in
			// need to adjust the x and y coordinates of this box to encase the whole map layout

			for (int p=0;p<3;p++)
			{
				if (planepts[p][0] < 0)
					planepts[p][0] += m_iMapExtents_XMin * ASW_TILE_SIZE;
				else
					planepts[p][0] += m_iMapExtents_XMax * ASW_TILE_SIZE;
				if (planepts[p][1] < 0)
					planepts[p][1] += m_iMapExtents_YMin * ASW_TILE_SIZE;
				else
					planepts[p][1] += m_iMapExtents_YMax * ASW_TILE_SIZE;
			}
		}
		else
		{

			// move the points to where our room is
			for (int p=0;p<3;p++)
			{
				planepts[p] += GetCurrentRoomOffset();
			}

			// store these so we know how much to shift texture alignment
			m_vecLastPlaneOffset = GetCurrentRoomOffset();
		}

		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "(%f %f %f) (%f %f %f) (%f %f %f)",
			planepts[0][0], planepts[0][1], planepts[0][2],
			planepts[1][0], planepts[1][1], planepts[1][2],
			planepts[2][0], planepts[2][1], planepts[2][2]);

		pKey->SetStringValue( buffer );
		return true;
	}
	else if ( !stricmp(szKey, "uaxis") || !stricmp(szKey, "vaxis") )		// fix texture alignment (similar to moving a brush with texture lock turned on in hammer)
	{
		// NOTE: This assumes the plane was read in previously and set m_vecLastPlaneOffset
		Vector axis;
		float offset, scale;
		offset = 0;
		scale = 1.0f;
		if ( sscanf( szValue, "[%f %f %f %f] %f", &axis.x, &axis.y, &axis.z, &offset, &scale ) == 5 )
		{
			offset -= m_vecLastPlaneOffset.Dot( axis ) / scale;

			char buffer[256];
			Q_snprintf( buffer, sizeof(buffer), "[%f %f %f %f] %f",
				axis.x, axis.y, axis.z, offset, scale );

			pKey->SetStringValue( buffer );
			return true;
		}
		else
		{
			Msg( "Error loading in uvaxis\n" );
		}
	}

	return true;
}

bool VMFExporter::ProcessEntity( KeyValues *pEntityKeys )
{		
	for ( KeyValues *pKeys = pEntityKeys->GetFirstSubKey(); pKeys; )
	{
		if ( pKeys->GetFirstSubKey() )
		{
			if ( !Q_stricmp( pKeys->GetName(), "solid" ) )
			{
				if ( !ProcessSolid( pKeys ) )
					return false;
			}
			else if ( !Q_stricmp( pKeys->GetName(), "connections" ) )
			{
				if ( !ProcessConnections( pKeys ) )
					return false;
			}
			else if ( !Q_stricmp( pKeys->GetName(), "editor" ) )	// remove editor keys
			{
				KeyValues *pRemoveKey = pKeys;
				pKeys = pKeys->GetNextKey();
				pEntityKeys->RemoveSubKey( pRemoveKey );
				pRemoveKey->deleteThis();
				continue;
			}
		}
		else
		{
			if ( !ProcessEntityKey( pKeys ) )
				return false;
		}
		pKeys = pKeys->GetNextKey();
	}

	return true;
}

bool VMFExporter::ProcessEntityKey( KeyValues *pKey )
{
	const char *szKey = pKey->GetName();
	const char *szValue = pKey->GetString();

	if (!stricmp(szKey, "id"))
	{
		// give this entity a unique ID
		char buffer[32];
		Q_snprintf(buffer, sizeof(buffer), "%d", ++m_iEntityCount );
		pKey->SetStringValue( buffer );

		return true;
	}
	else if (!stricmp(szKey, "origin"))
	{
		Vector Origin;
		int nRead = sscanf(szValue, "%f %f %f", &Origin[0], &Origin[1], &Origin[2]);

		if (nRead != 3)		
			return false;

		// move the points to where our room is
		Origin += GetCurrentRoomOffset();

		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "%f %f %f", Origin[0], Origin[1], Origin[2]);
		pKey->SetStringValue( buffer );

		return true;
	}
	// check for overlay values
	if (!stricmp(szKey, "sides"))
	{
		// just deal with one side for now
		// TODO: handle multiple sides in the value and handle sides in the world block
		int iSide = atoi(szValue);
		for (int i=0;i<m_SideTranslations.Count();i++)
		{
			SideTranslation_t *pSideTranslation = m_SideTranslations[i];
			if ( pSideTranslation->m_iOriginalSide == iSide )
			{
				char buffer[32];
				Q_snprintf(buffer, sizeof(buffer), "%d", pSideTranslation->m_iNewSide );
				pKey->SetStringValue( buffer );
				return true;
			}
		}
	}
	else if (!stricmp(szKey, "BasisOrigin"))
	{
		Vector Origin;
		int nRead = sscanf(szValue, "%f %f %f", &Origin[0], &Origin[1], &Origin[2]);

		if (nRead != 3)		
			return false;

		// move the points to where our room is
		Origin += GetCurrentRoomOffset();

		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "%f %f %f", Origin[0], Origin[1], Origin[2]);

		pKey->SetStringValue( buffer );
		return true;
	}

	// check for unique string keys (targetname, etc.)
	int nCount = m_UniqueKeys.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pUnique = m_UniqueKeys[i];
		if ( !stricmp(szKey, pUnique) && szValue && szValue[0] != '@' )		// don't make names unique if they start with special character
		{
			// prepend room id to make this unique
			char buffer[256];
			Q_snprintf( buffer, sizeof(buffer), "Room%d_%s", m_iCurrentRoom, szValue );

			pKey->SetStringValue( buffer );
			return true;
		}
	}

	// remap node IDs
	nCount = m_NodeIDKeys.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pNodeKey = m_NodeIDKeys[i];
		if (!stricmp(szKey, pNodeKey))
		{
			int iNodeID = atoi(szValue);
			for (int i=0;i<m_NodeTranslations.Count();i++)
			{
				NodeTranslation_t *pTranslation = m_NodeTranslations[i];
				if ( pTranslation->m_iOriginalNodeID == iNodeID )
				{

					char buffer[32];
					Q_snprintf(buffer, sizeof(buffer), "%d", pTranslation->m_iNewNodeID );
					pKey->SetStringValue( buffer );
					return true;
				}
			}
		}
	}

	return true;
}

bool VMFExporter::ProcessConnections( KeyValues *pConnections )
{	
	for ( KeyValues *pKeys = pConnections->GetFirstSubKey(); pKeys; pKeys = pKeys->GetNextKey() )
	{
		if ( !ProcessConnectionsKey( pKeys ) )
			return false;		
	}

	return true;
}

bool VMFExporter::ProcessConnectionsKey( KeyValues *pKey )
{
	const char *szValue = pKey->GetString();

	char buffer[256];
	// prepend room id to targetname
	if ( szValue && szValue[0] != '@' )		// don't make names unique if they start with special character
	{
		Q_snprintf( buffer, sizeof( buffer ), "Room%d_%s", m_iCurrentRoom, szValue );		
	}
	else
	{
		Q_snprintf( buffer, sizeof( buffer ), "%s", szValue );
	}

	pKey->SetStringValue( buffer );
	return true;
}

// sets priority of objective entities based on the order the rooms were listed in the mission/objective txt
void VMFExporter::ReorderObjectives( const CRoomTemplate *pTemplate, KeyValues *pTemplateKeys )
{	
	KeyValues *pKeys = m_pTemplateKeys;
	while ( pKeys )
	{
		if ( !Q_stricmp( pKeys->GetName(), "entity" ) && !Q_strnicmp( pKeys->GetString( "classname" ), "asw_objective", 13 ) )
		{
			if ( pKeys->GetFloat( "Priority" ) != 0 )	// if level designer has already set priority, then don't override it
			{
				pKeys = pKeys->GetNextKey();
				continue;
			}

			int iPriority = 100;
			// We no longer have a requested rooms array, so this code is not valid.  Need to replace it with something else to ensure priorities are set correctly.
// 			for ( int i = 0; i < m_pMapLayout->m_pRequestedRooms.Count(); i++ )
// 			{
// 				if ( m_pMapLayout->m_pRequestedRooms[i]->m_pRoomTemplate == pTemplate )
// 				{
// 					iPriority = m_pMapLayout->m_pRequestedRooms[i]->m_iMissionTextOrder;
// 				}
// 			}
			pKeys->SetFloat( "Priority", iPriority );
		}
		pKeys = pKeys->GetNextKey();
	}
}

int VMFExporter::MakeNodeIDsUnique()
{
	int iNodes = 0;
	KeyValues *pKeys = m_pTemplateKeys;
	while ( pKeys )
	{
		if ( !Q_stricmp( pKeys->GetName(), "entity" ) )		// go through all entities in this room
		{
			KeyValues *pFieldKey = pKeys->GetFirstSubKey();
			while ( pFieldKey )								// go through all properties of this entity
			{
				if ( !pFieldKey->GetFirstSubKey() )			// leaf
				{
					if ( !stricmp(pFieldKey->GetName(), "nodeid" ) )
					{
						NodeTranslation_t *pNodeTranslation = new NodeTranslation_t;		// store a translation so we can fix up info links
						pNodeTranslation->m_iOriginalNodeID = atoi( pFieldKey->GetString() );
						pNodeTranslation->m_iNewNodeID = m_iNextNodeID;
						m_NodeTranslations.AddToTail( pNodeTranslation );
						char buffer[16];
						Q_snprintf( buffer, sizeof( buffer ), "%d", m_iNextNodeID);
						pFieldKey->SetStringValue( buffer );
						m_iNextNodeID++;
						iNodes++;
					}
				}		
				pFieldKey = pFieldKey->GetNextKey();
			}
		}
		pKeys = pKeys->GetNextKey();
	}
	return iNodes;
}

const Vector& VMFExporter::GetCurrentRoomOffset()
{
	static Vector s_vecCurrentRoomOffset = vec3_origin;	

	if ( m_pRoom )
	{
		int half_map_size = MAP_LAYOUT_TILES_WIDE * 0.5f;
		s_vecCurrentRoomOffset.x = ( m_pRoom->m_iPosX - half_map_size ) * ASW_TILE_SIZE;
		s_vecCurrentRoomOffset.y = ( m_pRoom->m_iPosY - half_map_size ) * ASW_TILE_SIZE;
		s_vecCurrentRoomOffset.z = 0;
	}
	else
	{
		// place logical rooms at the origin and up above the camera
		s_vecCurrentRoomOffset.x = 0;
		s_vecCurrentRoomOffset.y = 0;
		s_vecCurrentRoomOffset.z = 1024;
	}
	return s_vecCurrentRoomOffset;
}

#define UNIQUE_KEYS_FILE "tilegen/unique_keys.txt"
void VMFExporter::LoadUniqueKeyList()
{
	m_UniqueKeys.RemoveAll();
	m_NodeIDKeys.RemoveAll();

	// Open the manifest file, and read the particles specified inside it
	KeyValues *manifest = new KeyValues( UNIQUE_KEYS_FILE );
	if ( manifest->LoadFromFile( g_pFullFileSystem, UNIQUE_KEYS_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "key" ) )
			{
				m_UniqueKeys.AddToTail( sub->GetString() );
				continue;
			}
			if ( !Q_stricmp( sub->GetName(), "NodeID" ) )
			{
				m_NodeIDKeys.AddToTail( sub->GetString() );
				continue;
			}

			Warning( "VMFExporter::LoadUniqueKeyList:  Manifest '%s' with bogus file type '%s', expecting 'key' or 'NodeID'\n", UNIQUE_KEYS_FILE, sub->GetName() );
		}
	}
	else
	{
		Warning( "VMFExporter: Unable to load manifest file '%s'\n", UNIQUE_KEYS_FILE );
	}

	manifest->deleteThis();
}

void VMFExporter::AddRoomInstance( const CRoomTemplate *pRoomTemplate, int nPlacedRoomIndex )
{
	KeyValues *pFuncInstance = new KeyValues( "entity" );
	pFuncInstance->SetInt( "id", ++ m_iEntityCount );
	pFuncInstance->SetString( "classname", "func_instance" );
	pFuncInstance->SetString( "angles", "0 0 0" );

	// Used to identify rooms for later fixup (e.g. adding/swapping instances and models)
	if ( nPlacedRoomIndex != -1 )
	{
		pFuncInstance->SetInt( "PlacedRoomIndex", nPlacedRoomIndex );
	}
	
	char vmfName[MAX_PATH];
	Q_snprintf( vmfName, sizeof( vmfName ), "tilegen/roomtemplates/%s/%s.vmf", pRoomTemplate->m_pLevelTheme->m_szName, pRoomTemplate->GetFullName() );
	// Convert backslashes to forward slashes to please the VMF parser
	int nStrLen = Q_strlen( vmfName );
	for ( int i = 0; i < nStrLen; ++ i )
	{
		if ( vmfName[i] == '\\' ) vmfName[i] = '/';
	}
	
	pFuncInstance->SetString( "file", vmfName );

	Vector vOrigin = GetCurrentRoomOffset();
	char buf[128];
	Q_snprintf( buf, 128, "%f %f %f", vOrigin.x, vOrigin.y, vOrigin.z );
	pFuncInstance->SetString( "origin", buf );
	m_pExportKeys->AddSubKey( pFuncInstance );
}

//-----------------------------------------------------------------------------
// Purpose: Saves the version information chunk.
// Input  : *pFile - 
// Output : Returns ChunkFile_Ok on success, an error code on failure.
//-----------------------------------------------------------------------------
KeyValues* VMFExporter::GetVersionInfo()
{
	KeyValues *pKeys = new KeyValues( "versioninfo" );
	pKeys->SetInt( "editorversion", 400 );
	pKeys->SetInt( "editorbuild", 3900 );
	pKeys->SetInt( "mapversion", 1 );
	pKeys->SetInt( "formatversion", 100 );
	pKeys->SetBool( "prefab", false );
	return pKeys;
}

// just save out an empty visgroups section
KeyValues* VMFExporter::GetDefaultVisGroups()
{
	KeyValues *pKeys = new KeyValues( "visgroups" );
	return pKeys;
}

KeyValues* VMFExporter::GetViewSettings()
{
	KeyValues *pKeys = new KeyValues( "viewsettings" );
	pKeys->SetBool( "bSnapToGrid", true );
	pKeys->SetBool( "bShowGrid", true );
	pKeys->SetBool( "bShowLogicalGrid", false );
	pKeys->SetInt( "nGridSpacing", ASW_TILE_SIZE );
	pKeys->SetBool( "bShow3DGrid", false );
	return pKeys;
}

KeyValues* VMFExporter::GetDefaultCamera()
{
	KeyValues *pKeys = new KeyValues( "cameras" );
	pKeys->SetInt( "activecamera", 0 );

	KeyValues *pSubKeys = new KeyValues( "camera" );
	pSubKeys->SetString( "position", "[135.491 -60.314 364.02]" );
	pSubKeys->SetString( "look", "[141.195 98.7571 151.25]" );
	pKeys->AddSubKey( pSubKeys );
	
	return pKeys;
}

KeyValues* VMFExporter::GetPlayerStarts()
{
	KeyValues *pKeys = new KeyValues( "entity" );
	pKeys->SetInt( "id", m_iEntityCount++ );
	pKeys->SetString( "classname", "info_player_start" );
	pKeys->SetString( "angles", "0 0 0" );

	// put a single player start in the centre of the player start tile selected
	float player_start_x = 128.0f;
	float player_start_y = 128.0f;
	int half_map_size = MAP_LAYOUT_TILES_WIDE * 0.5f;
	player_start_x += (m_pMapLayout->m_iPlayerStartTileX - half_map_size) * ASW_TILE_SIZE;
	player_start_y += (m_pMapLayout->m_iPlayerStartTileY - half_map_size) * ASW_TILE_SIZE;
	
	char buffer[128];
	Q_snprintf(buffer, sizeof(buffer), "%f %f 1.0", player_start_x, player_start_y);
	pKeys->SetString( "origin", buffer );

	return pKeys;
}

KeyValues* VMFExporter::GetGameRulesProxy()
{
	KeyValues *pKeys = new KeyValues( "entity" );
	pKeys->SetInt( "id", m_iEntityCount++ );
	pKeys->SetString( "classname", "asw_gamerules" );
	pKeys->SetString( "targetname", "@asw_gamerules" );
	
	char buffer[128];
	Q_snprintf(buffer, sizeof(buffer), "%f %f %f", m_vecStartRoomOrigin.x, m_vecStartRoomOrigin.y, m_vecStartRoomOrigin.z);
	pKeys->SetString( "origin", buffer );
	
	KeyValues *pGenerationOptions = m_pMapLayout->GetGenerationOptions();
	int nDifficultyModifier = 0;
	if ( pGenerationOptions != NULL )
	{
		nDifficultyModifier = pGenerationOptions->GetInt( "Difficulty", 5 ) - 5;
	}
	
	pKeys->SetInt( "difficultymodifier", nDifficultyModifier );

	return pKeys;
}

KeyValues* VMFExporter::GetDefaultWorldChunk()
{
	KeyValues *pKeys = new KeyValues( "world" );
	pKeys->SetInt( "id", 1 );
	pKeys->SetInt( "mapversion", 1 );
	pKeys->SetString( "classname", "worldspawn" );
	pKeys->SetString( "skyname", "blacksky" );
	pKeys->SetInt( "maxpropscreenwidth", -1 );
	pKeys->SetString( "detailvbsp", "detail.vbsp" );
	pKeys->SetString( "detailmaterial", "detail/detailsprites" );
	pKeys->SetInt( "speedruntime", 180 );
	return pKeys;
}

bool VMFExporter::AddLevelContainer()
{
	KeyValues *pLevelContainerKeys = new KeyValues( "LevelContainer" );
	if ( !pLevelContainerKeys->LoadFromFile( g_pFullFileSystem, "tilegen/roomtemplates/levelcontainer.vmf.no_func_detail", "GAME" ) )
		return false;

	m_bWritingLevelContainer = true;

	// set the extents of the map
	m_pMapLayout->GetExtents(m_iMapExtents_XMin, m_iMapExtents_XMax, m_iMapExtents_YMin, m_iMapExtents_YMax);
	Msg( "Layout extents: Topleft: %f %f - Lower right: %f %f\n", m_iMapExtents_XMin, m_iMapExtents_YMin, m_iMapExtents_XMax, m_iMapExtents_YMax );
	// adjust to be relative to the centre of the map
	int half_map_size = MAP_LAYOUT_TILES_WIDE * 0.5f;
	m_iMapExtents_XMin -= half_map_size;
	m_iMapExtents_XMax -= half_map_size;
	m_iMapExtents_YMin -= half_map_size;
	m_iMapExtents_YMax -= half_map_size;
	// pad them by 2 blocks, so the player doesn't move his camera into the wall when at an edge block
	m_iMapExtents_XMin -= 2;
	m_iMapExtents_XMax += 2;
	m_iMapExtents_YMin -= 2;
	m_iMapExtents_YMax += 2;

	Msg( "   Adjusted to: Topleft: %d %d - Lower right: %d %d\n", m_iMapExtents_XMin, m_iMapExtents_YMin, m_iMapExtents_XMax, m_iMapExtents_YMax );

	// find world keys
	KeyValues *pWorldKeys = NULL;
	for ( KeyValues *pKeys = pLevelContainerKeys; pKeys; pKeys = pKeys->GetNextKey() )		
	{
		const char *szName = pKeys->GetName();
		if ( !Q_stricmp( szName, "world" ) )
		{
			pWorldKeys = pKeys;
			break;
		}
	}
	if ( !pWorldKeys || !ProcessWorld( pWorldKeys ) )
	{
		Q_snprintf( m_szLastExporterError, sizeof(m_szLastExporterError), "Failed to copy level container\n" );
		return false;
	}

	m_bWritingLevelContainer = false;

	for ( KeyValues *pKeys = pWorldKeys->GetFirstSubKey(); pKeys; pKeys = pKeys->GetNextKey() )
	{
		const char *szName = pKeys->GetName();
		if ( !Q_stricmp( szName, "solid" ) )
		{
			m_pExportWorldKeys->AddSubKey( pKeys->MakeCopy() );
		}
	}
	pLevelContainerKeys->deleteThis();

	m_pTemplateKeys->deleteThis();
	m_pTemplateKeys = NULL;

	return true;
}
