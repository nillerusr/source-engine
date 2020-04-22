//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_gamemovement_sapper.h"
#include "tf_movedata.h"

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementSapper::CTFGameMovementSapper()
{
	m_pSapperData = NULL;

	m_vStandMins = SAPPERCLASS_HULL_STAND_MIN;
	m_vStandMaxs = SAPPERCLASS_HULL_STAND_MAX;
	m_vStandViewOffset = SAPPERCLASS_VIEWOFFSET_STAND;

	m_vDuckMins = SAPPERCLASS_HULL_DUCK_MIN;
	m_vDuckMaxs = SAPPERCLASS_HULL_DUCK_MAX;
	m_vDuckViewOffset = SAPPERCLASS_VIEWOFFSET_DUCK;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementSapper::ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData )
{
	// Get the class specific data from the TFMoveData structure
	Assert( PlayerClassSapperData_t::PLAYERCLASS_ID == pTFMoveData->m_nClassID );
	m_pSapperData = &pTFMoveData->SapperData();

	// to test pass it through!!
	BaseClass::ProcessMovement( (CBasePlayer *)pPlayer, static_cast<CMoveData*>( pTFMoveData ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementSapper::GetPlayerMins( bool bDucked ) const
{
	return bDucked ? m_vDuckMins : m_vStandMins; 
}
	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementSapper::GetPlayerMaxs( bool bDucked ) const
{
	return bDucked ? m_vDuckMaxs : m_vStandMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementSapper::GetPlayerViewOffset( bool bDucked ) const
{
	return bDucked ? m_vDuckViewOffset : m_vStandViewOffset;
}
