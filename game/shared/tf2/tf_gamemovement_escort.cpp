//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_gamemovement_escort.h"
#include "tf_movedata.h"

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementEscort::CTFGameMovementEscort()
{
	m_pEscortData = NULL;

	m_vStandMins = ESCORTCLASS_HULL_STAND_MIN;
	m_vStandMaxs = ESCORTCLASS_HULL_STAND_MAX;
	m_vStandViewOffset = ESCORTCLASS_VIEWOFFSET_STAND;

	m_vDuckMins = ESCORTCLASS_HULL_DUCK_MIN;
	m_vDuckMaxs = ESCORTCLASS_HULL_DUCK_MAX;
	m_vDuckViewOffset = ESCORTCLASS_VIEWOFFSET_DUCK;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementEscort::ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData )
{
	// Get the class specific data from the TFMoveData structure
	Assert( PlayerClassEscortData_t::PLAYERCLASS_ID == pTFMoveData->m_nClassID );
	m_pEscortData = &pTFMoveData->EscortData();

	// to test pass it through!!
	BaseClass::ProcessMovement( (CBasePlayer *)pPlayer, static_cast<CMoveData*>( pTFMoveData ) );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementEscort::GetPlayerMins( bool bDucked ) const
{
	return bDucked ? m_vDuckMins : m_vStandMins; 
}

	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementEscort::GetPlayerMaxs( bool bDucked ) const
{
	return bDucked ? m_vDuckMaxs : m_vStandMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementEscort::GetPlayerViewOffset( bool bDucked ) const
{
	return bDucked ? m_vDuckViewOffset : m_vStandViewOffset;
}
