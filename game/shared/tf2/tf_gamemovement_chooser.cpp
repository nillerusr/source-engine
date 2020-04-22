//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_gamemovement_chooser.h"
#include "tf_movedata.h"

static CTFGameMovementChooser g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement* )&g_GameMovement;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementChooser::CTFGameMovementChooser()
{
	// Allocate memory for a movement type for each class (0 = undecided)
	m_Movements.SetSize( TFCLASS_CLASS_COUNT );

	// NOTE: the order here matches the enum order in tf_shareddefs.h
	m_Movements[TFCLASS_RECON] = &m_ReconMovement;
	m_Movements[TFCLASS_COMMANDO] = &m_CommandoMovement;
	m_Movements[TFCLASS_MEDIC] = &m_MedicMovement;
	m_Movements[TFCLASS_DEFENDER] = &m_DefenderMovement;
	m_Movements[TFCLASS_SNIPER] = &m_SniperMovement;
	m_Movements[TFCLASS_SUPPORT] = &m_SupportMovement;
	m_Movements[TFCLASS_ESCORT] = &m_EscortMovement;
	m_Movements[TFCLASS_SAPPER] = &m_SapperMovement;
	m_Movements[TFCLASS_INFILTRATOR] = &m_InfiltratorMovement;
	m_Movements[TFCLASS_PYRO] = &m_PyroMovement;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementChooser::ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMoveData )
{
	// Convert CMoveData to CTFMoveData
	CTFMoveData *pTFMoveData = static_cast<CTFMoveData*>( pMoveData );
	
	// Cache the current class id
	m_nClassID = pTFMoveData->m_nClassID;

	// Player class movement. (If possible)
	if ( m_nClassID != TFCLASS_UNDECIDED )
	{
		m_Movements[m_nClassID]->ProcessClassMovement( (CBaseTFPlayer *)pPlayer, pTFMoveData );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementChooser::GetPlayerMins( bool ducked ) const
{
	// Player class mins.
	return m_Movements[m_nClassID]->GetPlayerMins( ducked );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementChooser::GetPlayerMaxs( bool ducked ) const
{
	return m_Movements[m_nClassID]->GetPlayerMins( ducked );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementChooser::GetPlayerViewOffset( bool ducked ) const
{
	return m_Movements[m_nClassID]->GetPlayerViewOffset( ducked );
}
