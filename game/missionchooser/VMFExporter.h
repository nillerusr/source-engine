#ifndef TILEGEN_VMFEXPORTER_H
#define TILEGEN_VMFEXPORTER_H
#ifdef _WIN32
#pragma once
#endif

#include "ChunkFile.h"
#include "utlvector.h"
#include "utlstring.h"

class CRoom;
class CMapLayout;
class CRoomTemplate;

// this class uses the placed rooms and room templates to build up a vmf file of the put together map

class VMFExporter
{
public:
	VMFExporter();
	virtual ~VMFExporter();

	void Init();
	bool ExportVMF( CMapLayout* pLayout, const char *mapname, bool bPopupWarnings=false );

	KeyValues* GetVersionInfo();
	KeyValues* GetDefaultVisGroups();
	KeyValues* GetViewSettings();
	KeyValues* GetDefaultCamera();
	KeyValues* GetPlayerStarts();
	KeyValues* GetGameRulesProxy();
	KeyValues* GetDefaultWorldChunk();
	bool AddLevelContainer();
	
	//-----------------------------------------------------------------------------
	// Functionality for old manual instancing (tilegen_use_instancing = 0)
	//-----------------------------------------------------------------------------
	bool AddRoomTemplateSolids( const CRoomTemplate *pRoomTemplate );
	bool AddRoomTemplateEntities( const CRoomTemplate *pRoomTemplate );
	bool IsDisplacementBrush( KeyValues *pSolidKeys );
	// these functions go through the keys and alter any needed values (shifting origin, bumping IDs, etc.)
	bool ProcessWorld( KeyValues *pWorldKey );
	bool ProcessSolid( KeyValues *pSolidKey );
	bool ProcessSide( KeyValues *pSideKey );
	bool ProcessSideKey( KeyValues *pKey );
	bool ProcessGenericRecursive( KeyValues *pParentKey );
	bool ProcessEntity( KeyValues *pSideKey );
	bool ProcessEntityKey( KeyValues *pEntityKey );
	bool ProcessConnections( KeyValues *pSideKey );
	bool ProcessConnectionsKey( KeyValues *pEntityKey );
	void ReorderObjectives( const CRoomTemplate *pTemplate, KeyValues *pTemplateKeys );
	int MakeNodeIDsUnique();
	const Vector& GetCurrentRoomOffset();
	void LoadUniqueKeyList();

	//-----------------------------------------------------------------------------
	// Functionality for new instancing support (tilegen_use_instancing = 1)
	//-----------------------------------------------------------------------------
	void AddRoomInstance( const CRoomTemplate *pRoomTemplate, int nPlacedRoomIndex = -1 );


	CRoom* m_pRoom;	// the current CRoom we're writing out
	int m_iCurrentRoom;	// index of the current room we're writing out
	int m_iEntityCount;
	int m_iSideCount;
	int m_iNextNodeID;	// ID to give the next AI node we export

	KeyValues *m_pTemplateKeys;			// keys for the template we're currently reading in
	KeyValues *m_pExportKeys;			// the keys we're going to export as a vmf file
	KeyValues *m_pExportWorldKeys;		// world section of m_pExportKeys

	CUtlVector<CUtlString> m_UniqueKeys, m_NodeIDKeys;

	// the level extents, in tiles, relative to the centre of the map
	int m_iMapExtents_XMin, m_iMapExtents_YMin;
	int m_iMapExtents_XMax, m_iMapExtents_YMax;
	bool m_bWritingLevelContainer;		// set to true when we're reading and writing out the cube that the level sits in

	char m_szLastExporterError[256];

	struct SideTranslation_t
	{
		int m_iOriginalSide;
		int m_iNewSide;
	};
	CUtlVector<SideTranslation_t*> m_SideTranslations;

	struct NodeTranslation_t
	{
		int m_iOriginalNodeID;
		int m_iNewNodeID;
	};
	CUtlVector<NodeTranslation_t*> m_NodeTranslations;

	Vector m_vecLastPlaneOffset;
	Vector m_vecStartRoomOrigin;

	CMapLayout* m_pMapLayout;		// the layout we're currently exporting

	void ExportError( const char *pMsg, ... );
	void ClearExportErrors();
	bool ShowExportErrors();
	CUtlVector<const char*> m_ExportErrors;
	bool m_bPopupWarnings;
};

#endif // TILEGEN_VMFEXPORTER_H