//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_gamemovement_support.h"
#include "tf_movedata.h"

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementSupport::CTFGameMovementSupport()
{
	m_pSupportData = NULL;

	m_vStandMins = SUPPORTCLASS_HULL_STAND_MIN;
	m_vStandMaxs = SUPPORTCLASS_HULL_STAND_MAX;
	m_vStandViewOffset = SUPPORTCLASS_VIEWOFFSET_STAND;

	m_vDuckMins = SUPPORTCLASS_HULL_DUCK_MIN;
	m_vDuckMaxs = SUPPORTCLASS_HULL_DUCK_MAX;
	m_vDuckViewOffset = SUPPORTCLASS_VIEWOFFSET_DUCK;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementSupport::ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData )
{
	// Get the class specific data from the TFMoveData structure
	Assert( PlayerClassSupportData_t::PLAYERCLASS_ID == pTFMoveData->m_nClassID );
	m_pSupportData = &pTFMoveData->SupportData();

	// to test pass it through!!
	BaseClass::ProcessMovement( (CBasePlayer *)pPlayer, static_cast<CMoveData*>( pTFMoveData ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementSupport::GetPlayerMins( bool bDucked ) const
{
	return bDucked ? m_vDuckMins : m_vStandMins; 
}
	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementSupport::GetPlayerMaxs( bool bDucked ) const
{
	return bDucked ? m_vDuckMaxs : m_vStandMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementSupport::GetPlayerViewOffset( bool bDucked ) const
{
	return bDucked ? m_vDuckViewOffset : m_vStandViewOffset;
}
