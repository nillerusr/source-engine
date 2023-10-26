#include "cbase.h"
#include "asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_hack_wire_tile.h"
#include "asw_button_area.h"
#include "asw_util_shared.h"

// memdbgon must be the last include file in a .cpp file
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_hack_wire_tile, CASW_Hack_Wire_Tile );

IMPLEMENT_SERVERCLASS_ST(CASW_Hack_Wire_Tile, DT_ASW_Hack_Wire_Tile)
	SendPropInt		(SENDINFO(m_iNumColumns) ),	
	SendPropInt		(SENDINFO(m_iNumRows) ),	
	SendPropInt		(SENDINFO(m_iNumWires) ),	
	SendPropFloat	(SENDINFO(m_fFastFinishTime), 0, SPROP_NOSCALE ),
	SendPropFloat	(SENDINFO(m_fFinishedHackTime), 0, SPROP_NOSCALE ),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWire1TileLit), SendPropBool( SENDINFO_ARRAY(m_iWire1TileLit) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWire2TileLit), SendPropBool( SENDINFO_ARRAY(m_iWire2TileLit) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWire3TileLit), SendPropBool( SENDINFO_ARRAY(m_iWire3TileLit) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWire4TileLit), SendPropBool( SENDINFO_ARRAY(m_iWire4TileLit) ) ),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWire1TileType), SendPropInt( SENDINFO_ARRAY(m_iWire1TileType), ASW_TILE_ARRAY_SIZE, SPROP_UNSIGNED ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWire2TileType), SendPropInt( SENDINFO_ARRAY(m_iWire2TileType), ASW_TILE_ARRAY_SIZE, SPROP_UNSIGNED ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWire3TileType), SendPropInt( SENDINFO_ARRAY(m_iWire3TileType), ASW_TILE_ARRAY_SIZE, SPROP_UNSIGNED ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWire4TileType), SendPropInt( SENDINFO_ARRAY(m_iWire4TileType), ASW_TILE_ARRAY_SIZE, SPROP_UNSIGNED ) ),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWire1TilePosition), SendPropInt( SENDINFO_ARRAY(m_iWire1TilePosition), ASW_TILE_ARRAY_SIZE, SPROP_UNSIGNED ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWire2TilePosition), SendPropInt( SENDINFO_ARRAY(m_iWire2TilePosition), ASW_TILE_ARRAY_SIZE, SPROP_UNSIGNED ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWire3TilePosition), SendPropInt( SENDINFO_ARRAY(m_iWire3TilePosition), ASW_TILE_ARRAY_SIZE, SPROP_UNSIGNED ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWire4TilePosition), SendPropInt( SENDINFO_ARRAY(m_iWire4TilePosition), ASW_TILE_ARRAY_SIZE, SPROP_UNSIGNED ) ),
END_SEND_TABLE()

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Hack_Wire_Tile )
	DEFINE_FIELD( m_iNumColumns, FIELD_INTEGER ),
	DEFINE_FIELD( m_iNumRows, FIELD_INTEGER ),
	DEFINE_FIELD( m_iNumWires, FIELD_INTEGER ),

	DEFINE_FIELD( m_bSetupPuzzle, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fFastFinishTime , FIELD_TIME ),
	
	DEFINE_AUTO_ARRAY( m_iWire1TileLit, FIELD_BOOLEAN ),
	DEFINE_AUTO_ARRAY( m_iWire2TileLit, FIELD_BOOLEAN ),
	DEFINE_AUTO_ARRAY( m_iWire3TileLit, FIELD_BOOLEAN ),
	DEFINE_AUTO_ARRAY( m_iWire4TileLit, FIELD_BOOLEAN ),

	DEFINE_AUTO_ARRAY( m_iWire1TileType, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iWire2TileType, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iWire3TileType, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iWire4TileType, FIELD_INTEGER ),

	DEFINE_AUTO_ARRAY( m_iWire1TilePosition, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iWire2TilePosition, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iWire3TilePosition, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iWire4TilePosition, FIELD_INTEGER ),
END_DATADESC()

extern ConVar asw_debug_tile_connections;
ConVar asw_wire_full_random("asw_wire_full_random", "0", FCVAR_CHEAT, "Fully randomize wire hacks");

CASW_Hack_Wire_Tile::CASW_Hack_Wire_Tile()
{
	m_bPlayedTimeOutSound = false;
	m_fFinishedHackTime = 0;
}


bool CASW_Hack_Wire_Tile::InitHack(CASW_Player* pHackingPlayer, CASW_Marine* pHackingMarine, CBaseEntity* pHackTarget)
{
	CASW_Button_Area *pButton = dynamic_cast<CASW_Button_Area*>(pHackTarget);
	if (!pHackingPlayer || !pHackingMarine || !pButton || !pHackingMarine->GetMarineResource())
	{
		return false;
	}

	if (!m_bSetupPuzzle)
	{
		// pull the puzzle settings from the button area, applying defaults if anything is out of range
		m_iNumColumns = pButton->m_iWireColumns;
		if (m_iNumColumns < 3 || m_iNumColumns > ASW_MAX_TILE_COLUMNS)
			m_iNumColumns = 5;
		m_iNumRows = pButton->m_iWireRows;
		if (m_iNumRows <= 0 || m_iNumRows > ASW_MAX_TILE_ROWS)
			m_iNumRows = 2;
		m_iNumWires = pButton->m_iNumWires;
		if (m_iNumWires <= 0 || m_iNumWires > 4)
			m_iNumWires = 3;

		//m_iNumColumns = 8;
		//m_iNumRows = 1;
		//m_iNumWires = 4;

		BuildWirePuzzle();

		for ( int i = 0; i < ASW_NUM_WIRES; i++ )
		{
			UpdateLitTiles( i + 1 );
			m_bWasWireLit[ i ] = IsWireLit( i + 1 );
		}
		m_bSetupPuzzle = true;
	}

	return BaseClass::InitHack(pHackingPlayer, pHackingMarine, pHackTarget);
}

#define HACK_SPEED_SCALE 4.0f
//#define INCREASE_HACKING_SPEED_PER_WIRE
ConVar asw_hacking_fraction( "asw_hacking_fraction", "0.1f", FCVAR_CHEAT );

void CASW_Hack_Wire_Tile::ASWPostThink(CASW_Player *pPlayer, CASW_Marine *pMarine,  CUserCmd *ucmd, float fDeltaTime)
{	
	CBaseEntity *pEnt = m_hHackTarget;
	CASW_Button_Area *pButton = dynamic_cast<CASW_Button_Area*>( pEnt );
	if ( !pButton || !pButton->m_bIsInUse )
		return;

	int iHackLevel = pButton->m_iHackLevel;
	float flBaseIncrease = ( fDeltaTime * ( 1.0f / ( ( float ) iHackLevel ) ) ) * HACK_SPEED_SCALE;
	// boost fTime by the marine's hack skill
	//flBaseIncrease *= MarineSkills()->GetSkillBasedValueByMarine( pMarine, ASW_MARINE_SKILL_HACKING, ASW_MARINE_SUBSKILL_HACKING_SPEED_SCALE );

	float f = pButton->GetHackProgress();

#ifdef INCREASE_HACKING_SPEED_PER_WIRE
	// total time increase is based on how many wires are lit
	float flTimeIncrease = 0;
	for ( int i=0;i<=ASW_NUM_WIRES;i++ )
	{
		if ( IsWireLit( i+1 ) )
			flTimeIncrease += flBaseIncrease;
	}
	int iNumWires = m_iNumWires;
	if ( iNumWires <= 0 )
		iNumWires = 1;
	flTimeIncrease *= ( 1.0f / iNumWires );

	pButton->SetHackProgress(f + flTimeIncrease, pMarine);
#else
	int iWiresLit = 0;
	for ( int i = 0; i < ASW_NUM_WIRES; i++ )
	{
		if ( IsWireLit( i + 1 ) )
		{
			iWiresLit++;
			m_bWasWireLit[ i ] = true;
		}
		else
		{
			m_bWasWireLit[ i ] = false;
		}
	}

	float flGoal = (float) iWiresLit / (float) m_iNumWires.Get();
	float move = ( flGoal - f ) * asw_hacking_fraction.GetFloat() * fDeltaTime;
	move = MAX( move, flBaseIncrease );
	f = MIN( f + move, flGoal );

	pButton->SetHackProgress( f, pMarine);
#endif
		
	// play a sound if the player has done the hack too slowly?
	if ( !m_bPlayedTimeOutSound && f < 1.0f )
	{
		if ( m_fFastFinishTime.Get() > 0 && gpGlobals->curtime > m_fFastFinishTime.Get() )
		{
			pButton->EmitSound( "ASWComputer.TimeOut" );
			m_bPlayedTimeOutSound = true;
		}
	}
}

void CASW_Hack_Wire_Tile::BuildWirePuzzle()
{
	for (int i=1;i<=m_iNumWires;i++)
	{
		BuildWire(i);
	}
}

// uses the current num columns/rows to fill out the array with tiles that link the top left to the bottom right
void CASW_Hack_Wire_Tile::BuildWire(int iWire)
{
	if (m_iNumRows == 1)
	{
		// all tiles have to be straight
		for (int i=0;i<m_iNumColumns;i++)
		{
			SetTileType(iWire, i, 0, ASW_WIRE_TILE_HORIZ);
			SetTileRotation(iWire, i, 0, 0);
		}
	}
	else if (m_iNumRows == 2)
	{
		int cur_x = 0;
		int cur_y = 0;
		int last_cur_x = -1;
		int last_cur_y = 0;
		while (cur_x < m_iNumColumns - 1)
		{
			bool bGoStraight = random->RandomFloat() < 0.4f;
			last_cur_x = cur_x;
			last_cur_y = cur_y;
			if (bGoStraight)
			{
				SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_HORIZ);
				SetTileRotation(iWire, cur_x, cur_y, 0);
				// set our partner as a random piece
				SetTileType(iWire, cur_x, 1 - cur_y, random->RandomInt(0, 2));
				SetTileRotation(iWire, cur_x, 1 - cur_y, 0);
				cur_x++;
			}
			else
			{
				// otherwise, both should be corner pieces
				if (cur_y == 0)	// we're at the top, so it has to be an r/l combo
				{
					//Msg("Generating puzzle, column %d, setting top to TILE RIGHT (r=%d), bottom to TILE LEFT (r=%d)\n", cur_x, 1, 2);
					SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_RIGHT);
					SetTileRotation(iWire, cur_x, cur_y, 1);
					SetTileType(iWire, cur_x, 1 - cur_y, ASW_WIRE_TILE_LEFT);
					SetTileRotation(iWire, cur_x, 1 - cur_y, 2);
					cur_x++;
					cur_y = 1 - cur_y;
				}
				else // otherwise it's an l r combo
				{
					//Msg("Generating puzzle, column %d, setting top to TILE LEFT (r=%d), bottom to TILE RIGHT (r=%d)\n", cur_x, 1, 0);
					SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_LEFT);
					SetTileRotation(iWire, cur_x, cur_y, 1);
					SetTileType(iWire, cur_x, 1 - cur_y, ASW_WIRE_TILE_RIGHT);
					SetTileRotation(iWire, cur_x, 1 - cur_y, 0);
					cur_x++;
					cur_y = 1 - cur_y;
				}
			}						
		}
		// in the last column, we either straight or do two corner pieces, so we can get to the bottom
		if (cur_y == 1)
		{
			// already at the bottom, so go straight
			SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_HORIZ);
			SetTileRotation(iWire, cur_x, cur_y, 0);
			SetTileType(iWire, cur_x, 1 - cur_y, random->RandomInt(0, 2));	// random piece
			SetTileRotation(iWire, cur_x, 1 - cur_y, 0);
		}
		else
		{
			SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_RIGHT);
			SetTileRotation(iWire, cur_x, cur_y, 1);
			SetTileType(iWire, cur_x, 1 - cur_y, ASW_WIRE_TILE_LEFT);
			SetTileRotation(iWire, cur_x, 1- cur_y, 2);
		}
	}
	else if (m_iNumRows == 3)
	{
		// first fill them with random junk, so any unset tiles we can leave
		for (int x=0;x<m_iNumColumns;x++)
		{
			for (int y=0;y<m_iNumRows;y++)
			{
				SetTileType(iWire, x, y, random->RandomInt(0, 2));				
				SetTileRotation(iWire, x, y, random->RandomInt(0, 3));
				SetTileLit(iWire, x, y, false);
			}
		}

		int cur_x = 0;
		int cur_y = 0;
		int last_cur_x = -1;
		int last_cur_y = 0;
		if (asw_debug_tile_connections.GetBool() && iWire == 1)
			Msg("Starting 3 row generation:\n");
		while (cur_x < m_iNumColumns - 1)
		{
			int tempx = cur_x;
			int tempy = cur_y;
			if (asw_debug_tile_connections.GetBool() && iWire == 1)
				Msg(" cur = %d,%d last = %d, %d\n", cur_x, cur_y, last_cur_x, last_cur_y);
			// either go forward or up/down
			bool bGoForward = random->RandomFloat() < 0.33f;
			if (last_cur_x == cur_x && cur_y != 1)	// have to go forward if we're on the same row as last time and are at an edge
				bGoForward = true;
			if (bGoForward)
			{
				if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("  wants to go forward\n");
				// want to go forward
				if (last_cur_x != cur_x)		// if we came from the left, we can just throw in a horiz one
				{
					if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("   since we came from the left, putting in a horiz rot 0\n");
					SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_HORIZ);
					SetTileRotation(iWire, cur_x, cur_y, 0);
					cur_x++;
				}
				else	// otherwise, need to figure out where we came from
				{
					if (last_cur_y < cur_y)	// if we came from above, need to do a left turn
					{
						if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("   since we came from the above, putting in a left turn rot 2\n");
						SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_LEFT);
						SetTileRotation(iWire, cur_x, cur_y, 2);
						cur_x++;
					}
					else
					{
						if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("   since we came from the below, putting in a right turn rot 0\n");
						SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_RIGHT);
						SetTileRotation(iWire, cur_x, cur_y, 0);
						cur_x++;
					}
				}	
			}
			else	// want to stay on this column and go up/down
			{
				if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("  wants to stay on the column\n");
				bool bGoUp = random->RandomFloat() < 0.5f;
				if (cur_y <= 0 || last_cur_y <= 0)
					bGoUp = false;
				if (cur_y >= 2 || last_cur_y >= 2)
					bGoUp = true;
				if (last_cur_x != cur_x)	// if we came from the left, put in the appropriate corner piece
				{
					if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("   we came from the left, ");
					if (bGoUp)
					{
						if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("and want to go up, so putting in a left rot 1\n");
						SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_LEFT);
						SetTileRotation(iWire, cur_x, cur_y, 1);
						cur_y--;
					}
					else
					{
						if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("and want to go down, so putting in a right rot 1\n");
						SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_RIGHT);
						SetTileRotation(iWire, cur_x, cur_y, 1);
						cur_y++;
					}
				}
				else	// if we're going straight up/down, put in the horiz
				{
					if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("   we were already on this column, ");
					if (bGoUp)
					{
						if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("and want to go up, so putting in a horiz and moving up rot 2\n");
						SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_HORIZ);
						SetTileRotation(iWire, cur_x, cur_y, 1);
						cur_y--;
					}
					else
					{
						if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("and want to go down, so putting in a horiz and moving down rot 2\n");
						SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_HORIZ);
						SetTileRotation(iWire, cur_x, cur_y, 1);
						cur_y++;
					}
				}
			}
			last_cur_x = tempx;
			last_cur_y = tempy;
		}
		if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg("At the last column.\n");
		// in the last column, need to get to the bottom
		if (cur_y == 2)
		{
			if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg(" at the bottom already, so let's go straight rot 0\n");
			// already at the bottom, so go straight
			SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_HORIZ);
			SetTileRotation(iWire, cur_x, cur_y, 0);
		}
		else if (cur_y == 1)	// came into the last row at the middle
		{
			if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg(" at the middle so turnign right, then left\n");
			SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_RIGHT);
			SetTileRotation(iWire, cur_x, cur_y, 1);
			SetTileType(iWire, cur_x, 2, ASW_WIRE_TILE_LEFT);
			SetTileRotation(iWire, cur_x, 2, 2);
		}
		else if (cur_y == 0)	// came into the last row at the top
		{
			if (asw_debug_tile_connections.GetBool() && iWire == 1) Msg(" at the top so turnign right, straight, then left\n");
			SetTileType(iWire, cur_x, cur_y, ASW_WIRE_TILE_RIGHT);
			SetTileRotation(iWire, cur_x, cur_y, 1);
			SetTileType(iWire, cur_x, 1, ASW_WIRE_TILE_HORIZ);
			SetTileRotation(iWire, cur_x, 1, 2);
			SetTileType(iWire, cur_x, 2, ASW_WIRE_TILE_LEFT);
			SetTileRotation(iWire, cur_x, 2, 2);
		}
	}
	else
	{
		// just fill it with random junk as a test
		for (int x=0;x<m_iNumColumns;x++)
		{
			for (int y=0;y<m_iNumRows;y++)
			{
				SetTileType(iWire, x, y, random->RandomInt(0, 2));
				SetTileRotation(iWire, x, y, random->RandomInt(0, 3));
				SetTileLit(iWire, x, y, false);
			}
		}
	}
	JumbleWire(iWire);	
}

void CASW_Hack_Wire_Tile::JumbleWire(int iWire)
{	
	if (asw_wire_full_random.GetBool())
	{
		for (int x=0;x<m_iNumColumns;x++)
		{
			for (int y=0;y<m_iNumRows;y++)
			{				
				SetTileRotation(iWire, x, y, random->RandomInt(0, 3));
				SetTileLit(iWire, x, y, false);
			}
		}
		return;
	}

	// first randomize any non-lit tiles
	UpdateLitTiles(iWire);
	for (int x=0;x<m_iNumColumns;x++)
	{
		for (int y=0;y<m_iNumRows;y++)
		{
			if (!GetTileLit(iWire, x, y))
			{
				SetTileRotation(iWire, x, y, random->RandomInt(0, 3));
				SetTileLit(iWire, x, y, false);
			}
		}
	}
	// now work from the end back, making mistakes on the lit wire
	int iMistakes = 0;
	int k = 0;
	while (iMistakes <= 0 && k <= 10)
	{
		k++;
		for (int x=m_iNumColumns-1;x>=0;x--)
		{
			for (int y=0;y<m_iNumRows;y++)
			{
				//if (x != m_iNumColumns-1)
					//continue;
				// decide if this should be a mistake column or not
				if (random->RandomFloat() < 0.5f)
				{
					if (GetTileLit(iWire, x, y))
					{
						int k=0;
						while (GetTileLit(iWire, x, y) && k < 10)
						{
							SetTileRotation(iWire, x, y, random->RandomInt(0, 3));
							SetTileLit(iWire, x, y, false);
							UpdateLitTiles(iWire);
							k++;
						}
						iMistakes++;
						if (iMistakes >= 5)	// don't make more than 5 mistakes per wire (cutting out early and working from the back biases mistakes to the end, but that's cool)
							return;

						y = m_iNumRows;	// don't change any more in this column
					}
				}
			}
		}
	}
}

void CASW_Hack_Wire_Tile::SetTileType(int iWire, int x, int y, int iType)
{
	if (x < 0 || x>=m_iNumColumns || y<0 || y >= m_iNumRows)
	{
		Msg("Tile coords out of bounds! %d,%d\n", x, y);
		return;
	}
	SetTileType(iWire, y * m_iNumColumns + x, iType);
}

void CASW_Hack_Wire_Tile::SetTileType(int iWire, int iTileIndex, int iType)
{
	if (iTileIndex <0 || iTileIndex >= m_iNumColumns*m_iNumRows)
	{
		Msg("Tile index out of bounds! %d\n", iTileIndex);
		return;
	}
	if (iWire == 1)
		m_iWire1TileType.Set(iTileIndex, iType);
	else if (iWire == 2)
		m_iWire2TileType.Set(iTileIndex, iType);
	else if (iWire == 3)
		m_iWire3TileType.Set(iTileIndex, iType);
	else if (iWire == 4)
		m_iWire4TileType.Set(iTileIndex, iType);
}

void CASW_Hack_Wire_Tile::SelectHackOption(int i)
{
	if (i < 0)
		return;
	int iTiles = m_iNumColumns * m_iNumRows;
	int iWire = -1;
	if (i < iTiles)
		iWire = 1;
	else if (i < iTiles * 2)
		iWire = 2;
	else if (i < iTiles * 3)
		iWire = 3;
	else if (i < iTiles * 4)
		iWire = 4;
	if (iWire != -1)
	{
		i -= (iWire-1)*iTiles;
		int iRot = GetTileRotation(iWire, i);
		iRot++;
		if (iRot >= 4)
			iRot = 0;
		SetTileRotation(iWire, i, iRot);

		UpdateLitTiles(1);
		UpdateLitTiles(2);
		UpdateLitTiles(3);
		UpdateLitTiles(4);

		CASW_Button_Area *pButton = dynamic_cast<CASW_Button_Area*>(GetHackTarget());
		if (pButton)
		{
			if (pButton->m_fStartedHackTime == 0)
			{
				pButton->m_fStartedHackTime = gpGlobals->curtime;
				
				float fSpeedScale = 1.0f;
				CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(m_hHackingMarine.Get());
				if (pMarine)
					fSpeedScale *= MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HACKING, ASW_MARINE_SUBSKILL_HACKING_SPEED_SCALE);
				float fTimeTaken = UTIL_ASW_CalcFastDoorHackTime(pButton->m_iWireRows, pButton->m_iWireColumns, pButton->m_iNumWires, pButton->m_iHackLevel, fSpeedScale);
				// notify client of when it should finish, to be a fast hack
				m_fFastFinishTime = gpGlobals->curtime + fTimeTaken;
			}
		}
		
	}
}

void CASW_Hack_Wire_Tile::OnHackComplete()
{
	BaseClass::OnHackComplete();

	m_fFinishedHackTime = gpGlobals->curtime;
}