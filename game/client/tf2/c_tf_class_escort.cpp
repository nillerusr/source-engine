//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_escort.h"

//=============================================================================
//
// Escort Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassEscort, DT_PlayerClassEscortData )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassEscort )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassEscortData_t ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassEscort::C_PlayerClassEscort( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassEscort::~C_PlayerClassEscort()
{
}