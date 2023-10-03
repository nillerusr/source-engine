#include "cbase.h"
#ifdef CLIENT_DLL
	#include "c_asw_marine_resource.h"
	#include "asw_marine_profile.h"
	#include "c_asw_player.h"
	#include "c_asw_marine.h"
	#include "c_asw_hack_wire_tile.h"	
	#include "c_asw_button_area.h"
	#define CASW_Marine C_ASW_Marine
	#define CASW_Marine_Resource C_ASW_Marine_Resource
	#define CASW_Player C_ASW_Player
	#define CASW_Hack_Wire_Tile C_ASW_Hack_Wire_Tile
	#define CASW_Button_Area C_ASW_Button_Area
#else
	#include "asw_marine_resource.h"
	#include "asw_marine_profile.h"
	#include "asw_player.h"
	#include "asw_marine.h"
	#include "asw_hack_wire_tile.h"	
	#include "asw_button_area.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_debug_tile_connections("asw_debug_tile_connections", "0", FCVAR_REPLICATED, "Print debug info for tile connections");

int CASW_Hack_Wire_Tile::GetTileRotation(int iWire, int x, int y)
{
	if (x < 0 || x>=m_iNumColumns || y<0 || y >= m_iNumRows)
	{
		Msg("Tile coords out of bounds! %d,%d\n", x, y);
		return -1;
	}
	return GetTileRotation(iWire, y * m_iNumColumns + x);
}

int CASW_Hack_Wire_Tile::GetTileRotation(int iWire, int iTileIndex)
{
	if (iTileIndex <0 || iTileIndex >= m_iNumColumns*m_iNumRows || iWire < 1 || iWire > 4)
	{
		Msg("Tile index out of bounds! %d\n", iTileIndex);
		return -1;
	}
#ifdef CLIENT_DLL
	// check our temporary predicted rotations
	if (m_iTempPredictedTilePositionTime[iWire-1][iTileIndex] != 0)
	{
		if (((gpGlobals->curtime -  m_iTempPredictedTilePositionTime[iWire-1][iTileIndex]) < 2.0f)
				&& m_iTempPredictedTilePosition[iWire-1][iTileIndex] != -1)
		{
			return m_iTempPredictedTilePosition[iWire-1][iTileIndex];
		}
		else
		{
			m_iTempPredictedTilePositionTime[iWire-1][iTileIndex] = 0;
			m_iTempPredictedTilePosition[iWire-1][iTileIndex] = -1;
		}
	}
#endif

	if (iWire == 1)
		return m_iWire1TilePosition[iTileIndex];
	else if (iWire == 2)
		return m_iWire2TilePosition[iTileIndex];
	else if (iWire == 3)
		return m_iWire3TilePosition[iTileIndex];
	else if (iWire == 4)
		return m_iWire4TilePosition[iTileIndex];
	return -1;
}

int CASW_Hack_Wire_Tile::GetTileType(int iWire, int x, int y)
{
	if (x < 0 || x>=m_iNumColumns || y<0 || y >= m_iNumRows)
	{
		Msg("Tile coords out of bounds! %d,%d\n", x, y);
		return -1;
	}
	return GetTileType(iWire, y * m_iNumColumns + x);
}

int CASW_Hack_Wire_Tile::GetTileType(int iWire, int iTileIndex)
{
	if (iTileIndex <0 || iTileIndex >= m_iNumColumns*m_iNumRows)
	{
		Msg("Tile index out of bounds! %d\n", iTileIndex);
		return -1;
	}
	if (iWire == 1)
		return m_iWire1TileType[iTileIndex];
	else if (iWire == 2)
		return m_iWire2TileType[iTileIndex];
	else if (iWire == 3)
		return m_iWire3TileType[iTileIndex];
	else if (iWire == 4)
		return m_iWire4TileType[iTileIndex];
	return -1;
}

bool CASW_Hack_Wire_Tile::GetTileLit(int iWire, int x, int y)
{
	if (x < 0 || x>=m_iNumColumns || y<0 || y >= m_iNumRows)
	{
		Msg("Tile coords out of bounds! %d,%d\n", x, y);
		return false;
	}
	return GetTileLit(iWire, y * m_iNumColumns + x);
}

bool CASW_Hack_Wire_Tile::GetTileLit(int iWire, int iTileIndex)
{
	if (iTileIndex <0 || iTileIndex >= m_iNumColumns*m_iNumRows || iWire < 1 || iWire > 4)
	{
		Msg("Tile index out of bounds! %d\n", iTileIndex);
		return false;
	}

#ifdef CLIENT_DLL
	// check our temporary predicted rotations
	if (m_iTempPredictedTileLitTime[iWire-1][iTileIndex] != 0)
	{
		if (((gpGlobals->curtime -  m_iTempPredictedTileLitTime[iWire-1][iTileIndex]) < 2.0f)
				&& m_iTempPredictedTileLit[iWire-1][iTileIndex] != -1)
		{
			return (m_iTempPredictedTileLit[iWire-1][iTileIndex] == 1);
		}
		else
		{
			m_iTempPredictedTileLitTime[iWire-1][iTileIndex] = 0;
			m_iTempPredictedTileLit[iWire-1][iTileIndex] = -1;
		}
	}
#endif

	if (iWire == 1)
		return m_iWire1TileLit[iTileIndex];
	else if (iWire == 2)
		return m_iWire2TileLit[iTileIndex];
	else if (iWire == 3)
		return m_iWire3TileLit[iTileIndex];
	else if (iWire == 4)
		return m_iWire4TileLit[iTileIndex];
	return false;
}

bool CASW_Hack_Wire_Tile::AllWiresLit()
{
	for (int i=1;i<=m_iNumWires;i++)
	{
		if (!IsWireLit(i))
			return false;
	}
	return true;
}

bool CASW_Hack_Wire_Tile::IsWireLit(int iWire)
{
	if (iWire < 1 || iWire > m_iNumWires)
		return false;

	// simply check if the bottom right tile is lit and connected to the exit
	return GetTileLit(iWire, m_iNumColumns-1, m_iNumRows-1) && EndTileConnected(iWire);
}

// connection points for each tile type:

// north, east, south, west
bool g_TileConnections[3][4] = { 
	{ false, true, false, true },		// ASW_WIRE_TILE_HORIZ   --
	{ false, false, true, true },		// ASW_WIRE_TILE_LEFT    -.  (¬)
	{ false, true, true, false }		// ASW_WIRE_TILE_RIGHT   ,-  (r)
};

// check if the top left tile is connected on the left
bool CASW_Hack_Wire_Tile::StartTileConnected(int iWire)
{
	int iTileType1 = GetTileType(iWire, 0, 0);
	int iTileRotation1 = GetTileRotation(iWire, 0, 0);
	
	int i = 3 - iTileRotation1;
	if (i < 0)
		i += 4;	
	//Msg("start tile type %d rot %d modified %d\n", iTileType1, iTileRotation1, i);

	return g_TileConnections[iTileType1][i];
}

bool CASW_Hack_Wire_Tile::EndTileConnected(int iWire)
{
	int iTileType1 = GetTileType(iWire, m_iNumColumns-1, m_iNumRows-1);
	int iTileRotation1 = GetTileRotation(iWire, m_iNumColumns-1, m_iNumRows-1);
	
	int i = 1 - iTileRotation1;
	if (i < 0)
		i += 4;	
	//Msg("end tile type %d rot %d modified %d\n", iTileType1, iTileRotation1, i);

	return g_TileConnections[iTileType1][i];
}

// are tiles connected?
bool CASW_Hack_Wire_Tile::TilesConnected(int iWire, int x1, int y1, int x2, int y2)
{
	// check the tiles are within bounds
	if (x1 < 0 || x1 >= m_iNumColumns)
		return false;
	if (y1 < 0 || y1 >= m_iNumRows)
		return false;
	if (x2 < 0 || x2 >= m_iNumColumns)
		return false;
	if (y2 < 0 || y2 >= m_iNumRows)
		return false;
	// get the relative offset
	int ox = x2 - x1;
	int oy = y2 - y1;
	if (asw_debug_tile_connections.GetBool())
		if (iWire == 1) Msg(" tile offset is %d, %d\n", ox, oy);
	if (ox < -1 || ox > 1 || oy < -1 || oy > 1)	// if the tiles aren't next to one another, there's no connection
	{
		return false;
	}

	int iTileType1 = GetTileType(iWire, x1, y1);
	int iTileType2 = GetTileType(iWire, x2, y2);

	int iTileRotation1 = GetTileRotation(iWire, x1, y1);
	int iTileRotation2 = GetTileRotation(iWire, x2, y2);

	// get the connection points for each tile type, rotated appropriately
	bool connect1[4];
	bool connect2[4];
	for (int i=0;i<4;i++)
	{
		int iIndex = i + iTileRotation1;
		if (iIndex >= 4)
			iIndex -= 4;
		connect1[iIndex] = g_TileConnections[iTileType1][i];

		iIndex = i + iTileRotation2;
		if (iIndex >= 4)
			iIndex -= 4;
		connect2[iIndex] = g_TileConnections[iTileType2][i];
	}
	if (asw_debug_tile_connections.GetBool())
	{
		if (iWire == 1) Msg(" tile1 type %d rot %d connections %d, %d, %d, %d\n", iTileType1, iTileRotation1, connect1[0], connect1[1], connect1[2], connect1[3]);
		if (iWire == 1) Msg(" tile2 type %d rot %d connections %d, %d, %d, %d\n", iTileType2, iTileRotation2, connect2[0], connect2[1], connect2[2], connect2[3]);
	}

	// check the appropriate connections based on our relative offset
	if (ox == 0 && oy == 1)  // tile 2 below tile 1
	{
		return (connect1[2] && connect2[0]);
	}
	else if (ox == 0 && oy == -1)  // tile 2 above tile 1
	{
		return (connect1[0] && connect2[2]);
	}
	else if (ox == 1 && oy == 0)  // tile 2 to the right of tile 1
	{
		return (connect1[1] && connect2[3]);
	}
	else if (ox == -1 && oy == 0)  // tile 2 to the left of tile 1
	{
		return (connect1[3] && connect2[1]);
	}
	return false;
}

float CASW_Hack_Wire_Tile::GetWireCharge()
{
	CASW_Button_Area *pButton = dynamic_cast<CASW_Button_Area*>(GetHackTarget());
	if (!pButton)
		return 0;

	return pButton->GetHackProgress();
}

void CASW_Hack_Wire_Tile::SetTileRotation(int iWire, int x, int y, int iRotation)
{
	if (x < 0 || x>=m_iNumColumns || y<0 || y >= m_iNumRows)
	{
		Msg("Tile coords out of bounds! %d,%d\n", x, y);
		return;
	}
	SetTileRotation(iWire, y * m_iNumColumns + x, iRotation);
}

void CASW_Hack_Wire_Tile::SetTileRotation(int iWire, int iTileIndex, int iRotation)
{
	//Msg("s=%d:SetTileRotation wire=%d, tile=%d, rot=%d\n", IsServer(), iWire, iTileIndex, iRotation);
	if (iTileIndex <0 || iTileIndex >= m_iNumColumns*m_iNumRows || iWire < 1 || iWire > 4)
	{
		Msg("Tile index out of bounds! %d\n", iTileIndex);
		return;
	}
#ifdef CLIENT_DLL	// clientside we just store our desired position (which gets drawn, and discarded after a few seconds)
	m_iTempPredictedTilePosition[iWire-1][iTileIndex] = iRotation;
	m_iTempPredictedTilePositionTime[iWire-1][iTileIndex] = gpGlobals->curtime;
	UpdateLitTiles(iWire);
	return;
#endif
	if (iWire == 1)
		m_iWire1TilePosition.Set(iTileIndex, iRotation);
	else if (iWire == 2)
		m_iWire2TilePosition.Set(iTileIndex, iRotation);
	else if (iWire == 3)
		m_iWire3TilePosition.Set(iTileIndex, iRotation);
	else if (iWire == 4)
		m_iWire4TilePosition.Set(iTileIndex, iRotation);
}

void CASW_Hack_Wire_Tile::UpdateLitTiles(int iWire)
{
	if (iWire < 1 || iWire > m_iNumWires)
		return;

	// make all tiles unlit
	int iNumTiles = m_iNumColumns * m_iNumRows;
	for (int i=0;i<iNumTiles;i++)
	{
		SetTileLit(iWire, i, false);	
	}

	// check if the top left tile is connected on the left side, it not, abort all tiles being lit
	if (!StartTileConnected(iWire))
		return;
	
	// assuming top left tile is lit
	int lit_x = 0;
	int lit_y = 0;
	int last_lit_x = 0;
	int last_lit_y = 0;
	int test_x = 0;
	int test_y = 0;
	SetTileLit(iWire, lit_x, lit_y, true);
	
	if (asw_debug_tile_connections.GetBool())
		if (iWire == 1) Msg("Set top left to lit, starting check\n");

	// check tiles around our lit tile for connections
	bool bFoundLit = true;
	int k = 0;
	while (bFoundLit && k < 256)
	{
		bFoundLit = false;
		test_x = lit_x;
		test_y = lit_y - 1;
		if (asw_debug_tile_connections.GetBool())
			if (iWire == 1) Msg("At %d, %d Checking %d, %d (1 up)\n", lit_x, lit_y, test_x, test_y);
		if (!(test_x == last_lit_x && test_y == last_lit_y) && TilesConnected(iWire, lit_x, lit_y, test_x, test_y)
			 && !GetTileLit(iWire, test_x, test_y))
		{
			bFoundLit = true;
			last_lit_x = lit_x;
			last_lit_y = lit_y;
			lit_x = test_x;
			lit_y = test_y;
			SetTileLit(iWire, lit_x, lit_y, true);
		}
		else 
		{			
			test_y = lit_y + 1;
			if (asw_debug_tile_connections.GetBool())
				if (iWire == 1) Msg("At %d, %d Checking %d, %d (1 down)\n", lit_x, lit_y, test_x, test_y);
			if (!(test_x == last_lit_x && test_y == last_lit_y) &&	TilesConnected(iWire, lit_x, lit_y, test_x, test_y)
				 && !GetTileLit(iWire, test_x, test_y))
			{
				bFoundLit = true;
				last_lit_x = lit_x;
				last_lit_y = lit_y;
				lit_x = test_x;
				lit_y = test_y;
				SetTileLit(iWire, lit_x, lit_y, true);
			}
			else
			{
				test_x = lit_x + 1;
				test_y = lit_y;
				if (asw_debug_tile_connections.GetBool())
					if (iWire == 1) Msg("At %d, %d Checking %d, %d (1 right)\n", lit_x, lit_y, test_x, test_y);
				if (!(test_x == last_lit_x && test_y == last_lit_y) &&	TilesConnected(iWire, lit_x, lit_y, test_x, test_y)
					 && !GetTileLit(iWire, test_x, test_y))
				{
					bFoundLit = true;
					last_lit_x = lit_x;
					last_lit_y = lit_y;
					lit_x = test_x;
					lit_y = test_y;
					SetTileLit(iWire, lit_x, lit_y, true);
				}
				else				
				{
					test_x = lit_x - 1;
					if (asw_debug_tile_connections.GetBool())
						if (iWire == 1) Msg("At %d, %d Checking %d, %d (1 left)\n", lit_x, lit_y, test_x, test_y);
					if (!(test_x == last_lit_x && test_y == last_lit_y) &&	TilesConnected(iWire, lit_x, lit_y, test_x, test_y)
						 && !GetTileLit(iWire, test_x, test_y))
					{
						bFoundLit = true;
						last_lit_x = lit_x;
						last_lit_y = lit_y;
						lit_x = test_x;
						lit_y = test_y;
						SetTileLit(iWire, lit_x, lit_y, true);
					}
				}
			}	
		}
		k++;
	}
}

void CASW_Hack_Wire_Tile::SetTileLit(int iWire, int x, int y, bool bLit)
{
	if (x < 0 || x>=m_iNumColumns || y<0 || y >= m_iNumRows)
	{
		Msg("Tile coords out of bounds! %d,%d\n", x, y);
		return;
	}
	SetTileLit(iWire, y * m_iNumColumns + x, bLit);
}

void CASW_Hack_Wire_Tile::SetTileLit(int iWire, int iTileIndex, bool bLit)
{
	if (iTileIndex <0 || iTileIndex >= m_iNumColumns*m_iNumRows || iWire < 1 || iWire > 4)
	{
		Msg("Tile index out of bounds! %d\n", iTileIndex);
		return;
	}
#ifdef CLIENT_DLL	// clientside we just store our desired position (which gets drawn, and discarded after a few seconds)	
	//Msg("Setting predicted tile lit w=%d tile=%d lit=%d\n", iWire, iTileIndex, bLit);
	m_iTempPredictedTileLit[iWire-1][iTileIndex] = bLit ? 1 : 0;
	m_iTempPredictedTileLitTime[iWire-1][iTileIndex] = gpGlobals->curtime;
	return;
#endif
	if (iWire == 1)
		m_iWire1TileLit.Set(iTileIndex, bLit);
	else if (iWire == 2)
		m_iWire2TileLit.Set(iTileIndex, bLit);
	else if (iWire == 3)
		m_iWire3TileLit.Set(iTileIndex, bLit);
	else if (iWire == 4)
		m_iWire4TileLit.Set(iTileIndex, bLit);
}