//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_sniper.h"

//=============================================================================
//
// Sniper Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassSniper, DT_PlayerClassSniperData )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassSniper )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassSniperData_t ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassSniper::C_PlayerClassSniper( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassSniper::~C_PlayerClassSniper()
{
}