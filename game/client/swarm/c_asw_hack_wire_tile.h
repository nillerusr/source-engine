#ifndef _DEFINED_C_ASW_HACK_WIRE_TILE
#define _DEFINED_C_ASW_HACK_WIRE_TILE

#include "c_asw_hack.h"
#include <vgui_controls/PHandle.h>

#define ASW_MAX_TILE_COLUMNS 8
#define ASW_MAX_TILE_ROWS 3
#define ASW_TILE_ARRAY_SIZE (ASW_MAX_TILE_COLUMNS*ASW_MAX_TILE_ROWS)

class CASW_VGUI_Frame;

class C_ASW_Hack_Wire_Tile : public C_ASW_Hack
{
public:
	C_ASW_Hack_Wire_Tile();
	virtual ~C_ASW_Hack_Wire_Tile();

	DECLARE_CLASS( C_ASW_Hack_Wire_Tile, C_ASW_Hack );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	bool ShouldPredict();
	void ClickedNode(int iNodeNum, bool bRightSide);

	void SetTileRotation(int iWire, int x, int y, int iRotation);
	void SetTileRotation(int iWire, int iTileIndex, int iRotation);

	void UpdateLitTiles(int iWire);
	void SetTileLit(int iWire, int x, int y, bool bLit);
	void SetTileLit(int iWire, int iTileIndex, bool bLit);

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

	void InitLockedTiles();
	void SetTileLocked(int iWire, int iTileIndex, int iLocked);
	int GetTileLocked(int iWire, int iTileIndex);
	int m_iTileLocked[4][ASW_TILE_ARRAY_SIZE];
	void CycleRows();
	float m_fNextLockCycleTime;
	
	void OnDataChanged( DataUpdateType_t updateType );
	void ClientThink();
	virtual void FrameDeleted(vgui::Panel *pPanel);

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

	vgui::DHANDLE<CASW_VGUI_Frame> m_hFrame;

	float m_fFastFinishTime;
	float m_fStartedHackTime;
	float m_fFinishedHackTime;

	int m_iTempPredictedTilePosition[4][ASW_TILE_ARRAY_SIZE];
	int m_iTempPredictedTileLit[4][ASW_TILE_ARRAY_SIZE];
	float m_iTempPredictedTilePositionTime[4][ASW_TILE_ARRAY_SIZE];
	float m_iTempPredictedTileLitTime[4][ASW_TILE_ARRAY_SIZE];

private:
	bool m_bLaunchedHackPanel;

	C_ASW_Hack_Wire_Tile( const C_ASW_Hack_Wire_Tile & ); // not defined, not accessible
};

enum
{
	ASW_WIRE_TILE_HORIZ = 0,
	ASW_WIRE_TILE_LEFT,
	ASW_WIRE_TILE_RIGHT,
};

#endif /* _DEFINED_C_ASW_HACK_WIRE_TILE */