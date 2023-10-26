#ifndef _DEFINED_ASW_HACK_WIRE_TILE
#define _DEFINED_ASW_HACK_WIRE_TILE

#include "asw_hack.h"

#define ASW_MAX_TILE_COLUMNS 8
#define ASW_MAX_TILE_ROWS 3
#define ASW_TILE_ARRAY_SIZE (ASW_MAX_TILE_COLUMNS*ASW_MAX_TILE_ROWS)
#define ASW_NUM_WIRES 4				// if you change this, change m_iWireXTileType network vars too

class CASW_Hack_Wire_Tile : public CASW_Hack
{
public:
	CASW_Hack_Wire_Tile();

	DECLARE_CLASS( CASW_Hack_Wire_Tile, CASW_Hack );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void ASWPostThink(CASW_Player *pPlayer, CASW_Marine *pMarine,  CUserCmd *ucmd, float fDeltaTime);
	virtual void SelectHackOption(int i);
	virtual bool InitHack(CASW_Player* pHackingPlayer, CASW_Marine* pHackingMarine, CBaseEntity* pHackTarget);
	void BuildWirePuzzle();	
	void BuildWire(int iWire);
	void JumbleWire(int iWire);

	virtual void OnHackComplete();

	CNetworkVar(int, m_iNumColumns);
	CNetworkVar(int, m_iNumRows);
	CNetworkVar(int, m_iNumWires);
	
	CNetworkArray( int,  m_iWire1TileType, ASW_TILE_ARRAY_SIZE );
	CNetworkArray( int,  m_iWire1TilePosition, ASW_TILE_ARRAY_SIZE );
	CNetworkArray( bool, m_iWire1TileLit, ASW_TILE_ARRAY_SIZE );	

	CNetworkArray( int,  m_iWire2TileType, ASW_TILE_ARRAY_SIZE );
	CNetworkArray( int,  m_iWire2TilePosition, ASW_TILE_ARRAY_SIZE );
	CNetworkArray( bool, m_iWire2TileLit, ASW_TILE_ARRAY_SIZE );	

	CNetworkArray( int,  m_iWire3TileType, ASW_TILE_ARRAY_SIZE );
	CNetworkArray( int,  m_iWire3TilePosition, ASW_TILE_ARRAY_SIZE );
	CNetworkArray( bool, m_iWire3TileLit, ASW_TILE_ARRAY_SIZE );	

	CNetworkArray( int,  m_iWire4TileType, ASW_TILE_ARRAY_SIZE );
	CNetworkArray( int,  m_iWire4TilePosition, ASW_TILE_ARRAY_SIZE );
	CNetworkArray( bool, m_iWire4TileLit, ASW_TILE_ARRAY_SIZE );	

	void SetTileRotation(int iWire, int x, int y, int iRotation);
	void SetTileRotation(int iWire, int iTileIndex, int iRotation);
	void SetTileType(int iWire, int x, int y, int iType);
	void SetTileType(int iWire, int iTileIndex, int iType);
	void SetTileLit(int iWire, int x, int y, bool bLit);
	void SetTileLit(int iWire, int iTileIndex, bool bLit);
	void UpdateLitTiles(int iWire);

	// shared functions
	bool AllWiresLit();
	bool IsWireLit(int iWire);
	bool StartTileConnected(int iWire);
	bool EndTileConnected(int iWire);
	bool TilesConnected(int iWire, int x1, int y1, int x2, int y2);
	int GetTileRotation(int iWire, int x, int y);
	int GetTileRotation(int iWire, int iTileIndex);
	int GetTileType(int iWire, int x, int y);
	int GetTileType(int iWire, int iTileIndex);
	bool GetTileLit(int iWire, int x, int y);
	bool GetTileLit(int iWire, int iTileIndex);
	float GetWireCharge();

	bool m_bSetupPuzzle;
	CNetworkVar(float, m_fFastFinishTime);
	bool m_bPlayedTimeOutSound;

	CNetworkVar(float, m_fFinishedHackTime);

	bool m_bWasWireLit[ ASW_NUM_WIRES ];
};

enum
{
	ASW_WIRE_TILE_HORIZ = 0,
	ASW_WIRE_TILE_LEFT,
	ASW_WIRE_TILE_RIGHT,
};

#endif /* _DEFINED_ASW_HACK_WIRE_TILE */