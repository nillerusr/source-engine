//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Auto Repair
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_gamemovement_medic.h"
#include "tf_movedata.h"

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFGameMovementMedic::CTFGameMovementMedic()
{
	m_pMedicData = NULL;

	m_vStandMins = MEDICCLASS_HULL_STAND_MIN;
	m_vStandMaxs = MEDICCLASS_HULL_STAND_MAX;
	m_vStandViewOffset = MEDICCLASS_VIEWOFFSET_STAND;

	m_vDuckMins = MEDICCLASS_HULL_DUCK_MIN;
	m_vDuckMaxs = MEDICCLASS_HULL_DUCK_MAX;
	m_vDuckViewOffset = MEDICCLASS_VIEWOFFSET_DUCK;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFGameMovementMedic::ProcessClassMovement( CBaseTFPlayer *pPlayer, CTFMoveData *pTFMoveData )
{
	// Get the class specific data from the TFMoveData structure
	Assert( PlayerClassMedicData_t::PLAYERCLASS_ID == pTFMoveData->m_nClassID );
	m_pMedicData = &pTFMoveData->MedicData();

	// to test pass it through!!
	BaseClass::ProcessMovement( (CBasePlayer *)pPlayer, static_cast<CMoveData*>( pTFMoveData ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementMedic::GetPlayerMins( bool bDucked ) const
{
	return bDucked ? m_vDuckMins : m_vStandMins; 
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementMedic::GetPlayerMaxs( bool bDucked ) const
{
	return bDucked ? m_vDuckMaxs : m_vStandMaxs;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const Vector &CTFGameMovementMedic::GetPlayerViewOffset( bool bDucked ) const
{
	return bDucked ? m_vDuckViewOffset : m_vStandViewOffset;
}
