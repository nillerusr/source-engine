//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_defender.h"

//=============================================================================
//
// Defender Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassDefender, DT_PlayerClassDefenderData )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassDefender )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassDefenderData_t ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassDefender::C_PlayerClassDefender( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassDefender::~C_PlayerClassDefender()
{
}
