#include "cbase.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_hack_wire_tile.h"
#include "vgui/asw_vgui_hack_wire_tile.h"
#include "asw_vgui_frame.h"
#include "c_asw_button_area.h"
#include <vgui/vgui.h>
#include <vgui_controls/Controls.h>
#include "vgui_controls/frame.h"
#include "iclientmode.h"
#include <vgui/IScheme.h>
#include "asw_input.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Hack_Wire_Tile, DT_ASW_Hack_Wire_Tile, CASW_Hack_Wire_Tile)
	RecvPropInt		(RECVINFO(m_iNumColumns) ),	
	RecvPropInt		(RECVINFO(m_iNumRows) ),	
	RecvPropInt		(RECVINFO(m_iNumWires) ),
	RecvPropFloat		( RECVINFO( m_fFastFinishTime ) ),
	RecvPropFloat	(RECVINFO(m_fFinishedHackTime) ),

	RecvPropArray3		( RECVINFO_ARRAY(m_iWire1TileLit), RecvPropBool( RECVINFO(m_iWire1TileLit[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWire2TileLit), RecvPropBool( RECVINFO(m_iWire2TileLit[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWire3TileLit), RecvPropBool( RECVINFO(m_iWire3TileLit[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWire4TileLit), RecvPropBool( RECVINFO(m_iWire4TileLit[0])) ),

	RecvPropArray3		( RECVINFO_ARRAY(m_iWire1TileType), RecvPropInt( RECVINFO(m_iWire1TileType[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWire2TileType), RecvPropInt( RECVINFO(m_iWire2TileType[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWire3TileType), RecvPropInt( RECVINFO(m_iWire3TileType[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWire4TileType), RecvPropInt( RECVINFO(m_iWire4TileType[0])) ),

	RecvPropArray3		( RECVINFO_ARRAY(m_iWire1TilePosition), RecvPropInt( RECVINFO(m_iWire1TilePosition[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWire2TilePosition), RecvPropInt( RECVINFO(m_iWire2TilePosition[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWire3TilePosition), RecvPropInt( RECVINFO(m_iWire3TilePosition[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWire4TilePosition), RecvPropInt( RECVINFO(m_iWire4TilePosition[0])) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Hack_Wire_Tile )	
/*
	DEFINE_PRED_ARRAY( m_iWire1TilePosition, FIELD_INTEGER,  ASW_TILE_ARRAY_SIZE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_ARRAY( m_iWire2TilePosition, FIELD_INTEGER,  ASW_TILE_ARRAY_SIZE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_ARRAY( m_iWire3TilePosition, FIELD_INTEGER,  ASW_TILE_ARRAY_SIZE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_ARRAY( m_iWire4TilePosition, FIELD_INTEGER,  ASW_TILE_ARRAY_SIZE, FTYPEDESC_INSENDTABLE ),
	*/
END_PREDICTION_DATA()

ConVar asw_hack_cycle_time("asw_hack_cycle_time", "0.5f", FCVAR_CHEAT);

C_ASW_Hack_Wire_Tile::C_ASW_Hack_Wire_Tile()
{
	m_bLaunchedHackPanel = false;
	// clear our puzzle data so we know when the server stuff arrives
	m_iNumColumns = 0;
	m_iNumRows = 0;
	m_iNumWires = 0;
	m_hFrame = NULL;
	m_fFinishedHackTime = 0;
	m_fNextLockCycleTime = 0;
	SetPredictionEligible( true );

	for (int w=0;w<4;w++)
	{
		for (int i=0;i<ASW_TILE_ARRAY_SIZE;i++)
		{
			m_iTempPredictedTilePosition[w][i] = -1;
			m_iTempPredictedTilePositionTime[w][i] = 0;
			m_iTempPredictedTileLit[w][i] = -1;
			m_iTempPredictedTileLitTime[w][i] = 0;
			SetTileLocked(w+1, i, 0);
		}
	}
}

C_ASW_Hack_Wire_Tile::~C_ASW_Hack_Wire_Tile()
{
	if (m_hFrame.Get())
	{
		if (m_hFrame->m_pNotifyHackOnClose == this)
			m_hFrame->m_pNotifyHackOnClose = NULL;
	}

	ASWInput()->SetCameraFixed( false );
}

// predicted if the marine hacking us is inhabited locally
bool C_ASW_Hack_Wire_Tile::ShouldPredict()
{
	C_ASW_Marine_Resource * RESTRICT pResource = GetHackerMarineResource();
	return ( pResource && pResource->IsInhabited() && pResource->IsLocal() );
}

void C_ASW_Hack_Wire_Tile::FrameDeleted(vgui::Panel *pPanel)
{
	if (pPanel == m_hFrame.Get())
	{
		m_hFrame = NULL;
	}

	ASWInput()->SetCameraFixed( false );
}

void C_ASW_Hack_Wire_Tile::ClientThink()
{
	HACK_GETLOCALPLAYER_GUARD( "Need to support launching multiple hack panels on one machine (1 for each splitscreen player) for this to make sense." );
	if (m_bLaunchedHackPanel)	// if we've launched our hack window, but the hack has lost its hacking marine, then close our window down
	{		
		bool bStillUsing = true;
		if (!GetHackerMarineResource())
			bStillUsing = false;

		if (!bStillUsing)
		{
			//Msg("wire hack has lost his hacking marine\n");
			m_bLaunchedHackPanel = false;
			if (m_hFrame.Get())
			{
				m_hFrame->SetVisible(false);
				m_hFrame->MarkForDeletion();
				m_hFrame = NULL;
				ASWInput()->SetCameraFixed( false );
			}
		}
	}
	// if we haven't launched the window and data is all present, launch it
	if (!m_bLaunchedHackPanel && m_iNumWires > 0 && GetHackerMarineResource() != NULL)
	{
		vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");

		m_hFrame = new CASW_VGUI_Hack_Wire_Tile_Container( GetClientMode()->GetViewport(), "WireTileContainer", this);
		m_hFrame->SetScheme(scheme);					

		CASW_VGUI_Hack_Wire_Tile* pHackWireFrame = new CASW_VGUI_Hack_Wire_Tile( m_hFrame.Get(), "HackWireTile", this );
		pHackWireFrame->SetScheme(scheme);	
		pHackWireFrame->ASWInit();	
		pHackWireFrame->MoveToFront();
		pHackWireFrame->RequestFocus();
		pHackWireFrame->SetVisible(true);
		pHackWireFrame->SetEnabled(true);
		m_bLaunchedHackPanel = true;
	}
	// check for hiding the panel if the player has a different marine selected, or if the selected marine is remote controlling a turret
	if (m_bLaunchedHackPanel && GetHackerMarineResource() && m_hFrame.Get())
	{
		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();		
		if (!pPlayer)
		{
			m_hFrame->SetVisible(false);
		}
		else
		{
			bool bLocalPlayerControllingHacker = (GetHackerMarineResource()->IsInhabited() && GetHackerMarineResource()->GetCommanderIndex() == pPlayer->entindex());
			bool bMarineControllingTurret = (GetHackerMarineResource()->GetMarineEntity() && GetHackerMarineResource()->GetMarineEntity()->IsControllingTurret());

			if (bLocalPlayerControllingHacker && !bMarineControllingTurret)
			{
				ASWInput()->SetCameraFixed( true );

				m_hFrame->SetVisible(true);

				if (gpGlobals->curtime > m_fNextLockCycleTime)
				{
					m_fNextLockCycleTime = gpGlobals->curtime + asw_hack_cycle_time.GetFloat();
					CycleRows();
				}
			}
			else
			{
				ASWInput()->SetCameraFixed( false );

				m_hFrame->SetVisible(false);
			}
		}
	}	
}

void C_ASW_Hack_Wire_Tile::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		m_fNextLockCycleTime = gpGlobals->curtime + asw_hack_cycle_time.GetFloat();
		return;
	}
	if (m_fFastFinishTime != 0 && m_fStartedHackTime == 0)
	{		
		m_fStartedHackTime = gpGlobals->curtime;
	}
}

void C_ASW_Hack_Wire_Tile::SetTileLocked(int iWire, int iTileIndex, int iLocked)
{
	if (iWire < 1 || iWire > 4)
		return;

	if (iTileIndex < 0 || iTileIndex >= m_iNumColumns*m_iNumRows)
		return;

	m_iTileLocked[iWire-1][iTileIndex] = iLocked;
}

int C_ASW_Hack_Wire_Tile::GetTileLocked(int iWire, int iTileIndex)
{
	if (iWire < 1 || iWire > 4)
		return 0;

	if (iTileIndex < 0 || iTileIndex >= m_iNumColumns*m_iNumRows)
		return 0;

	return m_iTileLocked[iWire-1][iTileIndex];
}

void C_ASW_Hack_Wire_Tile::InitLockedTiles()
{
	// try creating a 'window' for each row
	int iWindowSize = m_iNumColumns / 2.0f;
	for (int wire=1;wire<=m_iNumWires;wire++)
	{
		for (int y=0;y<m_iNumRows;y++)
		{
			// clear the row
			for (int k=0;k<m_iNumColumns;k++)
			{
				SetTileLocked(wire, y * m_iNumColumns + k, 0);
			}
			// pick a random position for the window
			int iPos = random->RandomFloat() * m_iNumColumns;
			if (iPos < 0)
				iPos = m_iNumColumns -1;
			if (iPos >= m_iNumColumns)
				iPos = 0;
			for (int k=0;k<iWindowSize;k++)
			{
				int tilex = iPos+k;
				if (tilex >= m_iNumColumns)
					tilex -= m_iNumColumns;
				SetTileLocked(wire, y * m_iNumColumns + tilex, 1);
			}
		}
	}
}

void C_ASW_Hack_Wire_Tile::CycleRows()
{
	//CLocalPlayerFilter filter;
	//C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.ColumnTick" );

	// copy current state shifted one into a new array
	int iNewTileLocked[4][ASW_TILE_ARRAY_SIZE];
	for (int wire=1;wire<=m_iNumWires;wire++)
	{
		for (int y=0;y<m_iNumRows;y++)
		{
			int dir = -1;
			if ((y % 2) == 0)
				dir = 1;
			for (int column=0;column<m_iNumColumns;column++)
			{								
				int destx = column+dir; //(column + dir) % m_iNumColumns;
				if (destx >= m_iNumColumns)
					destx = 0;
				if (destx < 0)
					destx = m_iNumColumns -1;
				int cur = GetTileLocked(wire, y * m_iNumColumns + column);
				int destindex = y * m_iNumColumns + destx;

				if (destindex >=0 && destindex < (m_iNumColumns*m_iNumRows) && wire>=1 && wire<=4)
					iNewTileLocked[wire-1][destindex] = cur;
				//else
					//Msg("error out of range: wire=%d destindex=%d\n", wire, destindex);
			}
		}
	}
	// copy the new array back over us
	for (int wire=0;wire<4;wire++)
	{
		for (int i=0;i<ASW_TILE_ARRAY_SIZE;i++)
		{
			m_iTileLocked[wire][i] = iNewTileLocked[wire][i];
		}
	}
}