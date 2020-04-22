//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_medic.h"

//=============================================================================
//
// Medic Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassMedic, DT_PlayerClassMedicData )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassMedic )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassMedicData_t ),

END_PREDICTION_DATA()


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassMedic::C_PlayerClassMedic( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassMedic::~C_PlayerClassMedic()
{
}