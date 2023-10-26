#ifndef TILEGEN_MAPLAYOUT_H
#define TILEGEN_MAPLAYOUT_H
#ifdef _WIN32
#pragma once
#endif

#include "Utlvector.h"
#include "UtlSortvector.h"
#include "ChunkFile.h"
#include "RoomTemplate.h"
#include "tilegen_core.h"
#include "missionchooser/iasw_spawn_selection.h"

class CRoom;
class CRoomTemplate;
class CASW_Spawn_Definition;

#define MAP_LAYOUT_TILES_WIDE 120			// giving a max map size of 30720

#define ASW_TILE_SIZE 256.0f

class CExit
{
public:
	CExit() { }
	CExit(int iX, int iY, ExitDirection_t direction, const char *szExitTag, CRoom *pRoom, bool bChokeGrow )
	{
		X = iX;
		Y = iY;
		ExitDirection = direction;
		pSourceRoom = pRoom;
		m_bChokepointGrowSource = bChokeGrow;

		if ( szExitTag )
		{
			Q_strncpy( m_szExitTag, szExitTag, sizeof( m_szExitTag ) );
		}
		else
		{
			m_szExitTag[0] = 0;
		}
	}

	int X;
	int Y;
	ExitDirection_t ExitDirection;
	CRoom *pSourceRoom;
	char m_szExitTag[MAX_EXIT_TAG_LENGTH];
	bool m_bChokepointGrowSource;
};

class CRoomCandidate
{
public:
	CRoomCandidate( const CRoomTemplate *pRoomTemplate, int x, int y, const CExit *pExit )
	{
		m_pRoomTemplate = pRoomTemplate;
		m_iXPos = x;
		m_iYPos = y;
		m_pExit = pExit;
		m_flCandidateChance = 0;
	}

	// The room template which is to be placed at a particular location.
	const CRoomTemplate *m_pRoomTemplate;
	// The grid square of the lower-left corner of the room template.
	int m_iXPos, m_iYPos;

	// This exit is located in a grid square within the bounds of this room candidate, pointing inwards
	// (i.e. away from the source room).  This exit is NOT in a grid square contained within 
	// the source room.
	// NOTE: this value can be NULL.  If so, the room candidate is not attached to an existing exit.
	const CExit *m_pExit;

	float m_flCandidateChance;
};

//-----------------------------------------------------------------------------
// String replacements for values in a child/instance map, specified by
// "replaceNN" keys in the parent map's func_instance entity.
//-----------------------------------------------------------------------------
struct KeyValue_t
{
	char m_Key[MAX_TILEGEN_IDENTIFIER_LENGTH];
	char m_Value[MAX_TILEGEN_IDENTIFIER_LENGTH];
};

//-----------------------------------------------------------------------------
// One of several modes which can be used to pick an instance 
enum InstanceSpawningMethod_t
{
	ISM_INVALID = 0,
	ISM_ADD_AT_RANDOM_NODE,
	// @TODO: add support for swapping
};

//-----------------------------------------------------------------------------
// An instance to be added to a map during VMF -> BSP compilation.
// This class is copy-able for convenience.
//-----------------------------------------------------------------------------
class CInstanceSpawn
{
public:
	CInstanceSpawn();

	bool LoadFromKeyValues( KeyValues *pKeyValues );
	void SaveToKeyValues( KeyValues *pKeyValues ) const;

	void FixupValues( const char *pFindValue, const char *pReplaceValue );

	const char *GetInstanceFilename() const { return m_InstanceFilename; }

	const KeyValue_t *GetAdditionalKeyValues() const { return m_AdditionalKeyValues.Base(); }
	int GetAdditionalKeyValueCount() const { return m_AdditionalKeyValues.Count(); }

	InstanceSpawningMethod_t GetInstanceSpawningMethod() const { return m_InstanceSpawningMethod; }

	// Gets/Sets the index of the room in the layout that this instance modifies.
	// If set to -1, this instance spawn is not actually in use.
	int GetPlacedRoomIndex() const { return m_nPlacedRoomIndex; }
	void SetPlacedRoomIndex( int nIndex ) { m_nPlacedRoomIndex = nIndex; }

	int GetRandomSeed() const { return m_nRandomSeed; }
	void SetRandomSeed( int nSeed ) { m_nRandomSeed = nSeed; }

private:
	char m_InstanceFilename[MAX_PATH];
	CCopyableUtlVector< KeyValue_t > m_AdditionalKeyValues;
	InstanceSpawningMethod_t m_InstanceSpawningMethod;
	int m_nPlacedRoomIndex;
	// Used so that any randomness in placing this instance is deterministic
	int m_nRandomSeed;
};

//-----------------------------------------------------------------------------
// An NPC spawn encounter in the mission
//-----------------------------------------------------------------------------
class CASW_Encounter : public IASW_Encounter
{
public:
	virtual const Vector&			GetEncounterPosition() { return m_vecPosition; }
	virtual int						GetNumSpawnDefs() { return m_SpawnDefs.Count(); }
	virtual IASWSpawnDefinition*	GetSpawnDef( int i ) { return ( i >= 0 && i < m_SpawnDefs.Count() ) ? (IASWSpawnDefinition*) m_SpawnDefs[ i ] : NULL; }
	virtual float					GetEncounterRadius() { return m_flEncounterRadius; }

	virtual void					SetEncounterPosition( const Vector &vecSrc ) { m_vecPosition = vecSrc; }
	virtual void					AddSpawnDef( CASW_Spawn_Definition *pSpawnDef );
	virtual void					SetEncounterRadius( float flRadius ) { m_flEncounterRadius = flRadius; }

	void LoadFromKeyValues( KeyValues *pKeys );
	void SaveToKeyValues( KeyValues *pKeys );

private:
	CUtlVector<CASW_Spawn_Definition*> m_SpawnDefs;
	Vector m_vecPosition;
	float m_flEncounterRadius;
};

class CMapLayout
{
public:
	CMapLayout( KeyValues *pGenerationOptions = NULL );
	virtual ~CMapLayout();

	// add room to the placed list and the room grid
	void PlaceRoom( CRoom *pRoom );
	void RemoveRoom( CRoom *pRoom );
	const CRoom *GetLastPlacedRoom() const { Assert( m_PlacedRooms.Count() > 0 ); return m_PlacedRooms[m_PlacedRooms.Count() - 1]; }
	void Clear();		// wipes all rooms

	KeyValues *GetGenerationOptions() { return m_pGenerationOptions; }
	void SetGenerationOptions( KeyValues *pNewGenerationOptions );

	void AddLogicalRoom( CRoomTemplate *pRoomTemplate );

	// Describes instances to be spawned during map creation
	CUtlVector< CInstanceSpawn > m_InstanceSpawns; // Instances to be spawned during VMF -> BSP process

	// all the placed rooms
	CUtlVector<CRoom*> m_PlacedRooms;
	CUtlVector<CRoomTemplate*> m_LogicalRooms;			// rooms that are being placed down just for entity logic
	bool SaveMapLayout(const char *filename);
	bool LoadMapLayout(const char *filename);

	bool LoadLogicalRoom( KeyValues *pRoomKeys );

	// holds pointer to the CRoom at that location on the grid	
	CRoom* m_pRoomGrid[MAP_LAYOUT_TILES_WIDE][MAP_LAYOUT_TILES_WIDE];

	// returns the min/max coords of the placed rooms
	void GetExtents(int &iTileX_Min, int &iTileX_Max, int &iTileY_Min, int &iTileY_Max);

	// returns true if the specified template fits in the map layout at that point (i.e. doesn't overlap any other rooms and has exits matching)
	CRoom *GetRoom( int nX, int nY ) const { Assert( nX >= 0 && nX < MAP_LAYOUT_TILES_WIDE && nY >= 0 && nY < MAP_LAYOUT_TILES_WIDE ); return m_pRoomGrid[nX][nY]; }
	CRoom *GetRoom( const Vector &vecPos );
	bool TemplateFits( const CRoomTemplate *pTemplate, int x, int y, bool bAllowNoExits = true ) const;
	bool RoomsOverlap( int x, int y, int w, int h, int x2, int y2, int w2, int h2 ) const;
	bool CheckExits( const CRoomTemplate *pTemplate, int x, int y, CUtlVector<CRoomTemplateExit*> *pMatchingExits = NULL ) const;
	bool CheckExitsOnSquares( const CRoomTemplate *pTemplate1, int offset_x, int offset_y, ExitDirection_t Direction, int x2, int y2, bool bRequireConnection = false, CUtlVector<CRoomTemplateExit*> *pMatchingExits = NULL ) const;

	// coords of the player starts (todo: support multiple tile player starts? deal better with players putting them in geometry, etc?)
	int m_iPlayerStartTileX;
	int m_iPlayerStartTileY;

	// last filename this layout was saved/loaded as
	void SetCurrentFilename(const char *szFilename);
	const char* GetCurrentFilename() { return m_szFilename; }
	char m_szFilename[MAX_PATH];

	bool SaveMiscMapProperties(CChunkFile *pFile);
	static ChunkFileResult_t LoadMiscMapProperties(CChunkFile *pFile, CMapLayout *pMapLayout);
	static ChunkFileResult_t LoadMiscMapKeyCallback(const char *szKey, const char *szValue, CMapLayout *pMapLayout);

	// fixed NPC spawns
	void MarkEncounterRooms();					// flags all CRooms that have an alien encounter on them
	CUtlVector<CASW_Encounter*> m_Encounters;

private:
	KeyValues* m_pGenerationOptions;		// keyvalues for the mission we used to generate this layout
};

#endif TILEGEN_MAPLAYOUT_H