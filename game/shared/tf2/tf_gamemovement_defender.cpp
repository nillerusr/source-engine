//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_gamemovement_defender.h"
#include "tf_movedata.h"

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementDefender::CTFGameMovementDefender()
{
	m_pDefenderData = NULL;

	m_vStandMins = DEFENDERCLASS_HULL_STAND_MIN;
	m_vStandMaxs = DEFENDERCLASS_HULL_STAND_MAX;
	m_vStandViewOffset = DEFENDERCLASS_VIEWOFFSET_STAND;

	m_vDuckMins = DEFENDERCLASS_HULL_DUCK_MIN;
	m_vDuckMaxs = DEFENDERCLASS_HULL_DUCK_MAX;
	m_vDuckViewOffset = DEFENDERCLASS_VIEWOFFSET_DUCK;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementDefender::ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData )
{
	// Get the class specific data from the TFMoveData structure
	Assert( PlayerClassDefenderData_t::PLAYERCLASS_ID == pTFMoveData->m_nClassID );
	m_pDefenderData = &pTFMoveData->DefenderData();

	// to test pass it through!!
	BaseClass::ProcessMovement( (CBasePlayer *)pPlayer, static_cast<CMoveData*>( pTFMoveData ) );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementDefender::GetPlayerMins( bool bDucked ) const
{
	return bDucked ? m_vDuckMins : m_vStandMins; 
}

	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementDefender::GetPlayerMaxs( bool bDucked ) const
{
	return bDucked ? m_vDuckMaxs : m_vStandMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementDefender::GetPlayerViewOffset( bool bDucked ) const
{
	return bDucked ? m_vDuckViewOffset : m_vStandViewOffset;
}
