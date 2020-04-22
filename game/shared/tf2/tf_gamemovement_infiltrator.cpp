//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_gamemovement_infiltrator.h"
#include "tf_movedata.h"

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementInfiltrator::CTFGameMovementInfiltrator()
{
	m_pInfiltratorData = NULL;

	m_vStandMins = INFILTRATORCLASS_HULL_STAND_MIN;
	m_vStandMaxs = INFILTRATORCLASS_HULL_STAND_MAX;
	m_vStandViewOffset = INFILTRATORCLASS_VIEWOFFSET_STAND;

	m_vDuckMins = INFILTRATORCLASS_HULL_DUCK_MIN;
	m_vDuckMaxs = INFILTRATORCLASS_HULL_DUCK_MAX;
	m_vDuckViewOffset = INFILTRATORCLASS_VIEWOFFSET_DUCK;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementInfiltrator::ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData )
{
	// Get the class specific data from the TFMoveData structure
	Assert( PlayerClassInfiltratorData_t::PLAYERCLASS_ID == pTFMoveData->m_nClassID );
	m_pInfiltratorData = &pTFMoveData->InfiltratorData();

	// to test pass it through!!
	BaseClass::ProcessMovement( (CBasePlayer *)pPlayer, static_cast<CMoveData*>( pTFMoveData ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementInfiltrator::GetPlayerMins( bool bDucked ) const
{
	return bDucked ? m_vDuckMins : m_vStandMins; 
}
	
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementInfiltrator::GetPlayerMaxs( bool bDucked ) const
{
	return bDucked ? m_vDuckMaxs : m_vStandMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementInfiltrator::GetPlayerViewOffset( bool bDucked ) const
{
	return bDucked ? m_vDuckViewOffset : m_vStandViewOffset;
}
