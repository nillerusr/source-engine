//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_class_pyro.h"

//=============================================================================
//
// Medic Data Table
//
BEGIN_RECV_TABLE_NOBASE( C_PlayerClassPyro, DT_PlayerClassPyroData )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PlayerClassPyro )

	DEFINE_PRED_TYPEDESCRIPTION( m_ClassData, PlayerClassPyroData_t ),

END_PREDICTION_DATA()


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassPyro::C_PlayerClassPyro( C_BaseTFPlayer *pPlayer ) :
	C_PlayerClass( pPlayer )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_PlayerClassPyro::~C_PlayerClassPyro()
{
}


PlayerClassPyroData_t* C_PlayerClassPyro::GetClassData()
{
	return &m_ClassData; 
}

