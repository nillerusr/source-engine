//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_infiltrator.h"

//=============================================================================
//
// Infiltrator Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassInfiltrator, DT_PlayerClassInfiltratorData )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassInfiltrator )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassInfiltratorData_t ),

END_PREDICTION_DATA()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassInfiltrator::C_PlayerClassInfiltrator( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassInfiltrator::~C_PlayerClassInfiltrator()
{
}