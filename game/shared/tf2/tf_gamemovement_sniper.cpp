//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_gamemovement_sniper.h"
#include "tf_movedata.h"

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementSniper::CTFGameMovementSniper()
{
	m_pSniperData = NULL;

	m_vStandMins = SNIPERCLASS_HULL_STAND_MIN;
	m_vStandMaxs = SNIPERCLASS_HULL_STAND_MAX;
	m_vStandViewOffset = SNIPERCLASS_VIEWOFFSET_STAND;

	m_vDuckMins = SNIPERCLASS_HULL_DUCK_MIN;
	m_vDuckMaxs = SNIPERCLASS_HULL_DUCK_MAX;
	m_vDuckViewOffset = SNIPERCLASS_VIEWOFFSET_DUCK;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementSniper::ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData )
{
	// Get the class specific data from the TFMoveData structure
	Assert( PlayerClassSniperData_t::PLAYERCLASS_ID == pTFMoveData->m_nClassID );
	m_pSniperData = &pTFMoveData->SniperData();

	// to test pass it through!!
	BaseClass::ProcessMovement( (CBasePlayer *)pPlayer, static_cast<CMoveData*>( pTFMoveData ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementSniper::GetPlayerMins( bool bDucked ) const
{
	return bDucked ? m_vDuckMins : m_vStandMins; 
}
	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementSniper::GetPlayerMaxs( bool bDucked ) const
{
	return bDucked ? m_vDuckMaxs : m_vStandMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementSniper::GetPlayerViewOffset( bool bDucked ) const
{
	return bDucked ? m_vDuckViewOffset : m_vStandViewOffset;
}
